#include "catch/catch.hpp"
#include "editor/document.h"
#include "editor/mapgen_stroke.h"

#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <nlohmann/json.hpp>
#include <ranges>
#include <string>
#include <utility>
#include <vector>

namespace {

/// A 6x4 mapgen of floor with a wall across its third row.
const auto room = std::string(R"([
  {
    "type": "mapgen",
    "method": "json",
    "om_terrain": "field",
    "object": {
      "fill_ter": "t_floor",
      "rows": [
        "......",
        "......",
        "######",
        "......"
      ],
      "terrain": { ".": "t_floor", "#": "t_wall" }
    }
  }
]
)");

auto rows_of(const editor::document& doc) -> std::vector<std::string> {
    return editor::symbols_of(doc, 0) | std::views::transform([](const auto& row) {
               return row | std::views::join | std::ranges::to<std::string>();
           })
         | std::ranges::to<std::vector>();
}

auto drawn(const editor::shape_kind kind, const point& from, const point& to)
    -> std::vector<std::string> {
    auto doc = editor::document::parse("room.json", room).value();
    REQUIRE(editor::stroke(doc, {.drawn = {.kind = kind, .from = from, .to = to}, .symbol = "x"}));
    return rows_of(doc);
}

} // namespace

TEST_CASE("each shape covers its squares and no others", "[editor_mapgen]") {
    using enum editor::shape_kind;
    CHECK(drawn(point, {1, 1}, {4, 3})
          == std::vector<std::string>{"......", ".x....", "######", "......"});
    CHECK(drawn(line, {0, 0}, {5, 0})
          == std::vector<std::string>{"xxxxxx", "......", "######", "......"});
    CHECK(drawn(rect, {1, 0}, {4, 3})
          == std::vector<std::string>{".xxxx.", ".x..x.", "#x##x#", ".xxxx."});
    CHECK(drawn(rect_filled, {4, 3}, {1, 0})
          == std::vector<std::string>{".xxxx.", ".xxxx.", "#xxxx#", ".xxxx."});
    CHECK(drawn(ellipse, {0, 0}, {5, 3})
          == std::vector<std::string>{".xxxx.", "xxxxxx", "xxxxxx", ".xxxx."});
    CHECK(drawn(fill, {2, 1}, {2, 1})
          == std::vector<std::string>{"xxxxxx", "xxxxxx", "######", "......"});
}

TEST_CASE("a shape past the rows is cut at their edge", "[editor_mapgen]") {
    using enum editor::shape_kind;
    CHECK(drawn(rect_filled, {4, 2}, {9, 9})
          == std::vector<std::string>{"......", "......", "####xx", "....xx"});
}

TEST_CASE("a stroke is one undo step that rewrites only its rows", "[editor_mapgen]") {
    auto doc = editor::document::parse("room.json", room).value();
    const auto changed = editor::stroke(
        doc,
        {.drawn = {.kind = editor::shape_kind::line, .from = {0, 3}, .to = {2, 3}}, .symbol = "#"});
    REQUIRE(changed);
    CHECK(*changed == 3);
    CHECK(doc.text().find("\"###...\"") != std::string::npos);
    CHECK(doc.undo());
    CHECK(doc.text() == room);
    CHECK_FALSE(doc.undo());
}

TEST_CASE("a stroke that changes nothing adds no step", "[editor_mapgen]") {
    auto doc = editor::document::parse("room.json", room).value();
    const auto changed = editor::
        stroke(doc, {.drawn = {.kind = editor::shape_kind::point, .from = {0, 2}}, .symbol = "#"});
    REQUIRE(changed);
    CHECK(*changed == 0);
    CHECK_FALSE(doc.undo());
}

TEST_CASE("a stroke refuses what is not one symbol", "[editor_mapgen]") {
    auto doc = editor::document::parse("room.json", room).value();
    CHECK_FALSE(editor::stroke(doc, {.symbol = "ab"}));
    CHECK_FALSE(editor::stroke(doc, {.symbol = ""}));
    CHECK(doc.text() == room);
}

TEST_CASE("a room is its outline around its inside in one step", "[editor_mapgen]") {
    auto doc = editor::document::parse("room.json", room).value();
    REQUIRE(editor::stroke(
        doc,
        {.drawn = {.kind = editor::shape_kind::room, .from = {1, 0}, .to = {4, 3}},
         .symbol = "#",
         .fill_symbol = "_"}));
    CHECK(rows_of(doc) == std::vector<std::string>{".####.", ".#__#.", "##__##", ".####."});
    CHECK(doc.undo());
    CHECK_FALSE(doc.undo());
}

TEST_CASE("a brush widens each square and mirrors copy the shape", "[editor_mapgen]") {
    auto doc = editor::document::parse("room.json", room).value();
    REQUIRE(editor::stroke(
        doc,
        {.drawn = {.kind = editor::shape_kind::point, .from = {0, 0}, .size = 2, .mirror_x = true},
         .symbol = "x"}));
    CHECK(rows_of(doc) == std::vector<std::string>{"xx..xx", "xx..xx", "######", "......"});
    auto mirrored = editor::document::parse("room.json", room).value();
    REQUIRE(editor::stroke(
        mirrored,
        {.drawn =
             {.kind = editor::shape_kind::point, .from = {1, 0}, .mirror_x = true, .mirror_y = true},
         .symbol = "x"}));
    CHECK(rows_of(mirrored) == std::vector<std::string>{".x..x.", "......", "######", ".x..x."});
}

TEST_CASE("the eyedropper reads the symbol under it", "[editor_mapgen]") {
    const auto doc = editor::document::parse("room.json", room).value();
    const auto grid = editor::symbols_of(doc, 0);
    CHECK(editor::symbol_at(grid, {3, 2}) == "#");
    CHECK(editor::symbol_at(grid, {3, 1}) == ".");
    CHECK(editor::symbol_at(grid, {6, 0}).empty());
}

TEST_CASE("a copied region pastes elsewhere as one undo step", "[editor_mapgen]") {
    auto doc = editor::document::parse("room.json", room).value();
    const auto clip = editor::copy_region(editor::symbols_of(doc, 0), {3, 2}, {1, 1});
    CHECK(clip == editor::symbol_grid{{".", ".", "."}, {"#", "#", "#"}});
    CHECK(editor::paste(doc, {.object = 0, .clip = clip, .at = {3, 2}}).value() == 6);
    CHECK(rows_of(doc) == std::vector<std::string>{"......", "......", "###...", "...###"});
    CHECK(doc.undo());
    CHECK(rows_of(doc) == std::vector<std::string>{"......", "......", "######", "......"});
    CHECK_FALSE(doc.undo());
}

TEST_CASE("a copy and a paste stop at the edge of the rows", "[editor_mapgen]") {
    auto doc = editor::document::parse("room.json", room).value();
    const auto grid = editor::symbols_of(doc, 0);
    CHECK(editor::copy_region(grid, {-2, -2}, {1, 0}) == editor::symbol_grid{{".", "."}});
    CHECK(editor::copy_region(grid, {6, 0}, {8, 3}).empty());
    const auto clip = editor::symbol_grid{{"x", "y"}, {"z", "w"}};
    CHECK(editor::paste(doc, {.object = 0, .clip = clip, .at = {5, 3}}).value() == 1);
    CHECK(rows_of(doc) == std::vector<std::string>{"......", "......", "######", ".....x"});
    CHECK(editor::paste(doc, {.object = 0, .clip = clip, .at = {6, 0}}).value() == 0);
    CHECK(editor::paste(doc, {.object = 0, .clip = {}, .at = {0, 0}}).value() == 0);
    CHECK(doc.undo());
    CHECK_FALSE(doc.undo());
}

TEST_CASE("a paste refuses a symbol that is not one column wide", "[editor_mapgen]") {
    auto doc = editor::document::parse("room.json", room).value();
    CHECK_FALSE(editor::paste(doc, {.object = 0, .clip = {{"ab"}}, .at = {0, 0}}));
    CHECK_FALSE(editor::paste(doc, {.object = 3, .clip = {{"x"}}, .at = {0, 0}}));
    CHECK_FALSE(doc.can_undo());
}

TEST_CASE("rotating a clip turns it a quarter clockwise", "[editor_mapgen]") {
    const auto clip = editor::symbol_grid{{"a", "b", "c"}, {"d", "e", "f"}};
    CHECK(editor::rotated(clip) == editor::symbol_grid{{"d", "a"}, {"e", "b"}, {"f", "c"}});
    CHECK(editor::rotated(editor::rotated(editor::rotated(editor::rotated(clip)))) == clip);
    CHECK(editor::rotated({}).empty());
}

TEST_CASE("mirroring a clip flips it left to right or top to bottom", "[editor_mapgen]") {
    const auto clip = editor::symbol_grid{{"a", "b", "c"}, {"d", "e", "f"}};
    CHECK(editor::mirrored_x(clip) == editor::symbol_grid{{"c", "b", "a"}, {"f", "e", "d"}});
    CHECK(editor::mirrored_y(clip) == editor::symbol_grid{{"d", "e", "f"}, {"a", "b", "c"}});
    CHECK(editor::mirrored_x(editor::mirrored_x(clip)) == clip);
}

TEST_CASE(
    "span_shapes.json lists the squares each span shape of the map editor covers",
    "[editor_mapgen]") {
    // Clients draw a span drag from this file; BN_WRITE_SPAN_SHAPES rewrites it from the editor.
    const auto path = std::string("docs/schema/engine-client/span_shapes.json");
    const auto kinds = std::vector<std::pair<std::string, editor::shape_kind>>{
        {"line", editor::shape_kind::line},
        {"rect", editor::shape_kind::rect},
        {"rect_filled", editor::shape_kind::rect_filled},
        {"ellipse", editor::shape_kind::ellipse},
        {"room", editor::shape_kind::room}};
    const auto spans = std::vector<std::pair<point, point>>{
        {{3, 3}, {3, 3}}, {{1, 2}, {8, 5}},  {{8, 5}, {1, 2}}, {{2, 1}, {4, 9}},
        {{0, 0}, {9, 9}}, {{9, 0}, {0, 7}},  {{4, 4}, {6, 5}}, {{2, 7}, {9, 8}},
        {{5, 1}, {5, 8}}, {{0, 9}, {10, 9}}, {{3, 2}, {7, 7}}, {{6, 9}, {1, 1}}};
    const auto grid = editor::symbol_grid(10, std::vector<std::string>(11, "."));
    const auto listed = [](const std::vector<point>& squares) {
        auto out = nlohmann::ordered_json::array();
        for (const auto& p : squares) { out.push_back({p.x, p.y}); }
        return out;
    };
    auto cases = nlohmann::ordered_json::array();
    for (const auto& [name, kind] : kinds) {
        for (const auto& [from, to] : spans) {
            for (const auto size : {1, 2, 3}) {
                for (const auto mirrors : {0, 1, 2, 3}) {
                    const auto drawn = editor::shape{
                        .kind = kind,
                        .from = from,
                        .to = to,
                        .size = size,
                        .mirror_x = (mirrors & 1) != 0,
                        .mirror_y = (mirrors & 2) != 0};
                    auto entry = nlohmann::ordered_json{
                        {"shape", name},
                        {"from", {from.x, from.y}},
                        {"to", {to.x, to.y}},
                        {"size", size},
                        {"mirror_x", drawn.mirror_x},
                        {"mirror_y", drawn.mirror_y},
                        {"squares", listed(editor::squares_of(drawn, grid))}};
                    if (kind == editor::shape_kind::room) {
                        // The inside, as the stroke fills it before the outline.
                        const auto inside_from =
                            point(std::min(from.x, to.x) + 1, std::min(from.y, to.y) + 1);
                        const auto inside_to =
                            point(std::max(from.x, to.x) - 1, std::max(from.y, to.y) - 1);
                        entry["fill"] =
                            inside_from.x <= inside_to.x && inside_from.y <= inside_to.y
                                ? listed(editor::squares_of(
                                      {.kind = editor::shape_kind::rect_filled,
                                       .from = inside_from,
                                       .to = inside_to,
                                       .size = size,
                                       .mirror_x = drawn.mirror_x,
                                       .mirror_y = drawn.mirror_y},
                                      grid))
                                : nlohmann::ordered_json::array();
                    }
                    cases.push_back(std::move(entry));
                }
            }
        }
    }
    const auto fixture = nlohmann::ordered_json{{"cols", 11}, {"rows", 10}, {"cases", cases}};
    if (std::getenv("BN_WRITE_SPAN_SHAPES") != nullptr) {
        auto out = std::ofstream(path, std::ios::binary | std::ios::trunc);
        out << fixture.dump() << '\n';
    }
    auto in = std::ifstream(path, std::ios::binary);
    REQUIRE(in.good());
    CHECK(nlohmann::ordered_json::parse(in) == fixture);
}
