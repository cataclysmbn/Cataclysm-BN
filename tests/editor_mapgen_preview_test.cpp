#include "calendar.h"
#include "cata_utility.h"
#include "catch/catch.hpp"
#include "editor/document.h"
#include "editor/mapgen_preview.h"
#include "map/map.h"
#include "map/mapbuffer.h"
#include "mapgen/mapgen_constructor.h"
#include "mapgen/mapgen_functions.h"
#include "overmap/omdata.h"
#include "overmap/overmapbuffer.h"
#include "rng.h"
#include "trap.h"
#include "vehicle/vpart_position.h"

#include <nlohmann/json.hpp>
#include <ranges>
#include <set>
#include <string>

namespace {

/// The entry of `path` whose om_terrain is or holds `om_terrain`, as text.
auto entry_text(const std::string& path, const std::string& om_terrain) -> std::string {
    const auto doc = editor::document::open(path);
    REQUIRE(doc);
    const auto found = std::ranges::find_if(doc->objects(), [&](const auto& object) {
        return object.value.contains("om_terrain")
            && object.value["om_terrain"].dump().find('"' + om_terrain + '"') != std::string::npos;
    });
    REQUIRE(found != doc->objects().end());
    return found->value.dump();
}

const auto house = std::string("2StoryModern06_1");
const auto at = tripoint_abs_omt(40, 40, 0);

} // namespace

TEST_CASE("a preview is what the game generates from the registered mapgen", "[editor_mapgen]") {
    const auto text = entry_text("data/json/mapgen/house/2storymodern06.json", house);
    const auto preview = editor::preview_mapgen(text, {.at = at, .seed = 7});
    REQUIRE(preview);
    REQUIRE(preview->size == point_rel_ms(SEEX * 2, SEEY * 2));

    const auto dimension = get_map().get_bound_dimension();
    auto& overmap = get_overmapbuffer(dimension);
    const auto was = overmap.ter(at);
    overmap.ter_set(at, oter_str_id(house + "_north").id());
    auto buffer = mapbuffer{};
    buffer.set_dimension_id(dimension);
    auto constructor = mapgen_constructor(buffer);
    // The tests stub every mapgen out.
    const auto restore_mapgen = restore_on_out_of_scope<bool>(disable_mapgen);
    disable_mapgen = false;
    const auto registered = pick_mapgen_func(house);
    REQUIRE(registered);
    {
        const auto seeded = rng_deterministic_task_scope(7);
        const auto generated = constructor.generate(
            at, calendar::turn, {.use_selected_mapgen = true, .selected_mapgen = registered});
        REQUIRE(generated.is_generated());
    }
    overmap.ter_set(at, was);

    auto items = std::size_t{0};
    for (const auto p : constructor.points_on_zlevel()) {
        const auto& square = preview->squares[p.y() * preview->size.x() + p.x()];
        CAPTURE(p.x(), p.y());
        CHECK(square.ter == constructor.ter(p).id().str());
        CHECK(square.furn == constructor.furn(p).id().str());
        CHECK(square.trap == constructor.tr_at(p).id.str());
        CHECK(square.items == constructor.i_at(p).size());
        CHECK(square.vehicle == constructor.veh_at(p).has_value());
        items += square.items;
    }
    CHECK(items > 0);
    CHECK(overmap.ter(at) == was);
}

TEST_CASE("the same seed gives the same preview and another seed another", "[editor_mapgen]") {
    const auto text = entry_text("data/json/mapgen/house/2storymodern06.json", house);
    const auto squares = [&](const unsigned int seed) {
        const auto preview = editor::preview_mapgen(text, {.at = at, .seed = seed});
        REQUIRE(preview);
        return preview->squares | std::views::transform([](const auto& square) {
                   return square.ter + square.furn + square.trap + std::to_string(square.items);
               })
             | std::ranges::to<std::vector>();
    };
    CHECK(squares(3) == squares(3));
    CHECK(squares(3) != squares(4));
}

TEST_CASE("a preview of several overmap squares is one map", "[editor_mapgen]") {
    const auto text = entry_text("data/json/mapgen/apartment_con.json", "apartments_con_tower_NW");
    const auto preview = editor::preview_mapgen(text, {.at = at, .seed = 1});
    REQUIRE(preview);
    CHECK(preview->size == point_rel_ms(SEEX * 4, SEEY * 4));
    CHECK(std::ranges::none_of(preview->squares, [](const auto& square) {
        return square.ter.empty();
    }));
}

TEST_CASE("a nested chunk has no preview of its own", "[editor_mapgen]") {
    const auto preview = editor::preview_mapgen(
        R"({"type":"mapgen","method":"json","nested_mapgen_id":"x","object":{"mapgensize":[2,2],"rows":["..",".."],"terrain":{".":"t_floor"}}})",
        {.at = at});
    REQUIRE_FALSE(preview);
}

namespace {

/// A field of one parameterised floor, its parameter in `scope`.
auto parameterised(const std::string& scope) -> std::string {
    auto rows = nlohmann::ordered_json::array();
    for (auto row = 0; row < 24; ++row) { rows.push_back(std::string(24, '.')); }
    auto entry = nlohmann::ordered_json::parse(R"({
        "type": "mapgen", "method": "json", "om_terrain": "field",
        "object": {
          "parameters": { "floor": { "type": "ter_str_id",
            "default": { "distribution": [ [ "t_floor", 1 ], [ "t_dirt", 1 ] ] } } },
          "terrain": { ".": { "param": "floor" } }
        }
      })");
    entry["object"]["parameters"]["floor"]["scope"] = scope;
    // The loader asks a parameter of a special's scope for the value it takes outside one.
    if (scope == "overmap_special") { entry["object"]["terrain"]["."]["fallback"] = "t_floor"; }
    entry["object"]["rows"] = rows;
    return entry.dump();
}

auto floors(const editor::mapgen_preview& preview) -> std::set<std::string> {
    return preview.squares | std::views::transform(&editor::preview_square::ter)
         | std::ranges::to<std::set>();
}

} // namespace

TEST_CASE("a pinned parameter takes its value on every seed", "[editor_mapgen]") {
    for (const auto* scope : {"omt", "overmap_special"}) {
        CAPTURE(scope);
        const auto text = parameterised(scope);
        const auto loaded = editor::load_detached_mapgens(text);
        REQUIRE(loaded);
        const auto choices = editor::parameters_of(*loaded->functions.front());
        REQUIRE(choices.size() == 1);
        CHECK(choices[0].name == "floor");
        CHECK(std::ranges::is_permutation(
            choices[0].values, std::vector<std::string>{"t_floor", "t_dirt"}));
        auto seen = std::set<std::string>{};
        for (auto seed = 1U; seed <= 8; ++seed) {
            const auto free = editor::preview_mapgen(text, {.at = at, .seed = seed});
            REQUIRE(free);
            CAPTURE(free->messages);
            CHECK(free->messages.empty());
            seen.merge(floors(*free));
            const auto pinned = editor::
                preview_mapgen(text, {.at = at, .seed = seed, .pins = {{"floor", "t_dirt"}}});
            REQUIRE(pinned);
            // The field's region extras may draw over some squares.
            CHECK(floors(*pinned).contains("t_dirt"));
            CHECK_FALSE(floors(*pinned).contains("t_floor"));
        }
        CHECK(seen.contains("t_floor"));
        CHECK(seen.contains("t_dirt"));
    }
}

TEST_CASE("pinning a parameter the mapgen lacks is refused", "[editor_mapgen]") {
    const auto refused =
        editor::preview_mapgen(parameterised("omt"), {.at = at, .pins = {{"wall", "t_wall"}}});
    REQUIRE_FALSE(refused);
    CHECK(refused.error().find("wall") != std::string::npos);
}
