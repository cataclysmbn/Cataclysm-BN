#include "catch/catch.hpp"
#include "editor/document.h"
#include "editor/mapgen_legend.h"

#include <filesystem>
#include <nlohmann/json.hpp>
#include <ranges>
#include <string>
#include <vector>

namespace {

using nlohmann::ordered_json;

auto symbol(const std::vector<editor::legend_symbol>& legend, const std::string& name)
    -> const editor::legend_symbol& {
    const auto found = std::ranges::find(legend, name, &editor::legend_symbol::symbol);
    REQUIRE(found != legend.end());
    return *found;
}

auto lines(const editor::legend_symbol& found) -> std::vector<std::vector<std::string>> {
    return found.lines | std::views::transform([](const auto& line) {
               return std::vector{line.layer, line.value, line.source};
           })
         | std::ranges::to<std::vector>();
}

/// A cabin of three rows on the cabin palette, which takes its walls from cabin_palette_common.
const auto cabin = ordered_json{
    {"type", "mapgen"},
    {"method", "json"},
    {"om_terrain", "field"},
    {"object",
     {{"fill_ter", "t_floor"},
      {"rows", {"#w#", "#R?", "###"}},
      {"palettes", {"cabin_palette"}},
      {"terrain", {{"#", "t_rock_wall"}}},
      {"items", {{"R", {{"item", "SUS_tools"}, {"chance", 30}}}}}}},
};

} // namespace

TEST_CASE(
    "a symbol lists what it places through every palette, in the loader's order",
    "[editor_mapgen]") {
    const auto legend = editor::mapgen_legend(cabin.dump());
    REQUIRE(legend);
    CHECK(
        lines(symbol(*legend, "#"))
        == std::vector<std::vector<std::string>>{
            {"terrain", R"("t_wall_log")", "cabin_palette_common"},
            {"terrain", R"("t_rock_wall")", ""}});
    const auto r = lines(symbol(*legend, "R"));
    REQUIRE(r.size() == 4);
    CHECK(r[0] == std::vector<std::string>{"terrain", R"("t_floor")", "cabin_palette_common"});
    CHECK(r[1]
          == std::vector<std::string>{
              "furniture", R"([ "f_rack", "f_utility_shelf" ])", "cabin_palette_common"});
    CHECK(r[2][0] == "items");
    CHECK(r[2][2] == "cabin_palette");
    CHECK(r[3] == std::vector<std::string>{"items", R"({"item":"SUS_tools","chance":30})", ""});
    CHECK(
        lines(symbol(*legend, "w"))
        == std::vector<std::vector<std::string>>{
            {"terrain", R"("t_window_domestic")", "cabin_palette"},
            {"nested", R"({ "chunks": [ [ "null", 95 ], [ "house_place_air_conditioner", 5 ] ] })",
             "cabin_palette"}});
}

TEST_CASE("the legend tells used symbols from mapped ones", "[editor_mapgen]") {
    const auto legend = editor::mapgen_legend(cabin.dump());
    REQUIRE(legend);
    CHECK(symbol(*legend, "#").used);
    CHECK(symbol(*legend, "?").used);
    CHECK(symbol(*legend, "?").lines.empty());
    CHECK_FALSE(symbol(*legend, "W").used);
    CHECK(std::ranges::is_sorted(*legend, {}, &editor::legend_symbol::symbol));
}

TEST_CASE("every mapgen in the data has terrain for each symbol its rows use", "[editor_mapgen]") {
    auto checked = 0;
    for (const auto& file : std::filesystem::recursive_directory_iterator("data/json/mapgen")) {
        if (file.path().extension() != ".json") { continue; }
        const auto doc = editor::document::open(file.path());
        REQUIRE(doc);
        for (const auto& object : doc->objects()) {
            const auto& value = object.value;
            if (!value.is_object() || value.value("type", "") != "mapgen"
                || value.value("method", "") != "json" || value.contains("nested_mapgen_id")
                || value.contains("update_mapgen_id") || !value.contains("object")
                || !value["object"].contains("rows") || value["object"].contains("fill_ter")
                || value["object"].contains("predecessor_mapgen")) {
                continue;
            }
            CAPTURE(file.path().string(), value.value("om_terrain", ordered_json()).dump());
            const auto legend = editor::mapgen_legend(value.dump());
            REQUIRE(legend);
            for (const auto& found : *legend | std::views::filter(&editor::legend_symbol::used)) {
                CAPTURE(found.symbol);
                CHECK(std::ranges::contains(
                    found.lines, std::string("terrain"), &editor::legend_line::layer));
            }
            ++checked;
        }
    }
    CHECK(checked > 100);
}
