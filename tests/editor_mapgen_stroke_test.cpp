#include "catch/catch.hpp"
#include "editor/document.h"
#include "editor/mapgen_stroke.h"

#include <ranges>
#include <string>
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
