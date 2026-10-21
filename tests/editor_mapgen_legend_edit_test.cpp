#include "catch/catch.hpp"
#include "editor/document.h"
#include "editor/mapgen_legend_edit.h"

#include <string>

namespace {

using nlohmann::ordered_json;

const auto entry = std::string(R"([
  {
    "type": "mapgen",
    "method": "json",
    "om_terrain": "field",
    "object": {
      "rows": [ "./" ],
      "terrain": { ".": "t_floor" }
    }
  }
]
)");

auto object_of(const editor::document& doc) -> const ordered_json& {
    return doc.objects()[0].value["object"];
}

} // namespace

TEST_CASE("a legend mapping is added, changed and removed with its layer", "[editor_mapgen]") {
    auto doc = editor::document::parse("legend.json", entry).value();
    REQUIRE(editor::set_legend(doc, {.layer = "terrain", .symbol = "/", .value = "t_wall"}));
    CHECK(object_of(doc)["terrain"] == ordered_json{{".", "t_floor"}, {"/", "t_wall"}});
    REQUIRE(editor::set_legend(
        doc,
        {.layer = "terrain",
         .symbol = "/",
         .value = ordered_json::parse(R"([["t_wall", 3], "t_floor"])")}));
    CHECK(object_of(doc)["terrain"]["/"] == ordered_json::parse(R"([["t_wall", 3], "t_floor"])"));
    REQUIRE(editor::set_legend(doc, {.layer = "furniture", .symbol = "/", .value = "f_chair"}));
    CHECK(object_of(doc)["furniture"] == ordered_json{{"/", "f_chair"}});
    REQUIRE(editor::set_legend(doc, {.layer = "furniture", .symbol = "/"}));
    CHECK_FALSE(object_of(doc).contains("furniture"));
    REQUIRE(editor::set_legend(doc, {.layer = "terrain", .symbol = "/"}));
    CHECK(object_of(doc)["terrain"] == ordered_json{{".", "t_floor"}});
}

TEST_CASE("each legend edit is one undo step back to the exact bytes", "[editor_mapgen]") {
    auto doc = editor::document::parse("legend.json", entry).value();
    REQUIRE(editor::set_legend(doc, {.layer = "terrain", .symbol = "/", .value = "t_wall"}));
    REQUIRE(editor::set_legend(doc, {.layer = "terrain", .symbol = "."}));
    CHECK(doc.undo());
    CHECK(doc.undo());
    CHECK_FALSE(doc.undo());
    CHECK(doc.text() == entry);
}

TEST_CASE("removing a mapping that is not there is refused", "[editor_mapgen]") {
    auto doc = editor::document::parse("legend.json", entry).value();
    CHECK_FALSE(editor::set_legend(doc, {.layer = "terrain", .symbol = "x"}));
    CHECK_FALSE(editor::set_legend(doc, {.layer = "traps", .symbol = "x"}));
    CHECK_FALSE(doc.undo());
}
