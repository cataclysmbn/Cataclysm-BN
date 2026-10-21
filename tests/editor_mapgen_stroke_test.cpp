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
