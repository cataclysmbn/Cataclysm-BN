#include "catch/catch.hpp"
#include "editor/document.h"
#include "editor/mapgen_placement.h"

#include <string>

namespace {

using nlohmann::ordered_json;

const auto entry = std::string(R"([
  {
    "type": "mapgen",
    "method": "json",
    "om_terrain": "field",
    "object": {
      "fill_ter": "t_floor",
      "rows": [
        "........................", "........................", "........................", "........................",
        "........................", "........................", "........................", "........................",
        "........................", "........................", "........................", "........................",
        "........................", "........................", "........................", "........................",
        "........................", "........................", "........................", "........................",
        "........................", "........................", "........................", "........................"
      ],
      "terrain": { ".": "t_floor" },
      "place_items": [ { "item": "SUS_tools", "x": [ 2, 5 ], "y": 3, "chance": 30 } ],
      "place_monster": [ { "monster": "mon_zombie", "x": 10, "y": [ 1, 4 ] } ]
    }
  }
]
)");

} // namespace

TEST_CASE("every placement is listed with the squares it may land on", "[editor_mapgen]") {
    const auto doc = editor::document::parse("placed.json", entry).value();
    const auto placed = editor::placements_of(doc, 0);
    REQUIRE(placed.size() == 2);
    CHECK(placed[0].kind == "place_items");
    CHECK(placed[0].x == editor::placement_range{.min = 2, .max = 5});
    CHECK(placed[0].y == editor::placement_range{.min = 3, .max = 3});
    CHECK(placed[1].kind == "place_monster");
    CHECK(placed[1].y == editor::placement_range{.min = 1, .max = 4});
}

TEST_CASE("moving a placement rewrites only its x and y in one step", "[editor_mapgen]") {
    auto doc = editor::document::parse("placed.json", entry).value();
    REQUIRE(editor::move_placement(
        doc, {.kind = "place_items", .x = {.min = 7, .max = 7}, .y = {.min = 0, .max = 2}}));
    const auto& moved = doc.objects()[0].value["object"]["place_items"][0];
    CHECK(moved == ordered_json::parse(R"({"item":"SUS_tools","x":7,"y":[0,2],"chance":30})"));
    CHECK(doc.undo());
    CHECK(doc.text() == entry);
    CHECK_FALSE(editor::move_placement(doc, {.kind = "place_items", .x = {.min = 3, .max = 1}}));
    CHECK_FALSE(editor::move_placement(doc, {.kind = "place_items", .index = 4}));
}

TEST_CASE("a field edit the loader accepts is one step", "[editor_mapgen]") {
    auto doc = editor::document::parse("placed.json", entry).value();
    REQUIRE(editor::set_field(doc, {.pointer = "/object/place_items/0/chance", .value = 60}));
    CHECK(doc.objects()[0].value["object"]["place_items"][0]["chance"] == 60);
    REQUIRE(editor::set_field(doc, {.pointer = "/object/place_items/0/chance"}));
    CHECK_FALSE(doc.objects()[0].value["object"]["place_items"][0].contains("chance"));
    CHECK(doc.undo());
    CHECK(doc.undo());
    CHECK(doc.text() == entry);
}

TEST_CASE("a field edit the loader refuses changes nothing and says why", "[editor_mapgen]") {
    auto doc = editor::document::parse("placed.json", entry).value();
    const auto refused = editor::
        set_field(doc, {.pointer = "/object/place_monster/0/monster", .value = "mon_no_such"});
    REQUIRE_FALSE(refused);
    CHECK(refused.error().find("mon_no_such") != std::string::npos);
    const auto misspelt =
        editor::set_field(doc, {.pointer = "/object/place_itemz", .value = ordered_json::array()});
    REQUIRE_FALSE(misspelt);
    CHECK(misspelt.error().find("place_itemz") != std::string::npos);
    CHECK_FALSE(doc.undo());
    CHECK(doc.text() == entry);
}
