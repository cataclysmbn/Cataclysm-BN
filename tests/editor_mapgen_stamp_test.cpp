#include "avatar.h"
#include "cata_utility.h"
#include "catch/catch.hpp"
#include "editor/mapgen_preview.h"
#include "editor/mapgen_stamp.h"
#include "map/map.h"
#include "map/mapbuffer.h"
#include "map_perception.h"
#include "mapgen/mapgen_constructor.h"
#include "mapgen/mapgen_functions.h"
#include "overmap/omdata.h"
#include "overmap/overmapbuffer.h"
#include "rng.h"
#include "state_helpers.h"
#include "type_id.h"

#include <nlohmann/json.hpp>
#include <string>

namespace {

/// One 24x24 mapgen of `om_terrain` that is floor with a wall at `wall`.
auto entry_with_wall(const std::string& om_terrain, const point& wall) -> std::string {
    auto rows = nlohmann::ordered_json::array();
    for (auto y = 0; y < 24; ++y) {
        auto row = std::string(24, '.');
        if (y == wall.y) { row[wall.x] = '#'; }
        rows.push_back(row);
    }
    return nlohmann::ordered_json{
        {"type", "mapgen"},
        {"method", "json"},
        {"om_terrain", om_terrain},
        {"object", {{"rows", rows}, {"terrain", {{".", "t_floor"}, {"#", "t_wall"}}}}}}
        .dump();
}

/// The terrain of the square `p` of the overmap square `omt` in the live map.
auto live_ter(const tripoint_abs_omt& omt, const point& p) -> ter_id {
    return get_map().ter(abs_to_bub(project_to<coords::ms>(omt) + tripoint_rel_ms(p.x, p.y, 0)));
}

} // namespace

TEST_CASE(
    "a stamp puts the entry in the live map and taking it back restores it", "[editor_mapgen]") {
    clear_all_state();
    const auto omt = get_avatar().abs_omt_pos();
    const auto text = entry_with_wall("house_04", point(2, 3));
    const auto was = get_overmapbuffer(get_map().get_bound_dimension()).ter(omt);
    const auto wall_was = live_ter(omt, point(2, 3));
    const auto floor_was = live_ter(omt, point(5, 5));
    REQUIRE(wall_was != ter_id("t_wall"));
    REQUIRE(floor_was != ter_id("t_floor"));

    auto stamped = editor::stamp_mapgen(text, {.at = omt});
    REQUIRE(stamped);
    CHECK(live_ter(omt, point(2, 3)) == ter_id("t_wall"));
    CHECK(live_ter(omt, point(5, 5)) == ter_id("t_floor"));
    CHECK(get_overmapbuffer(get_map().get_bound_dimension()).ter(omt) != was);

    editor::swap_stamp(*stamped);
    CHECK(live_ter(omt, point(2, 3)) == wall_was);
    CHECK(live_ter(omt, point(5, 5)) == floor_was);
    CHECK(get_overmapbuffer(get_map().get_bound_dimension()).ter(omt) == was);

    editor::swap_stamp(*stamped);
    CHECK(live_ter(omt, point(2, 3)) == ter_id("t_wall"));
    editor::swap_stamp(*stamped);
}

TEST_CASE(
    "the avatar remembers a stamp's squares as it holds them, and as they were once it is taken "
    "back",
    "[editor_mapgen]") {
    clear_all_state();
    auto& you = get_avatar();
    const auto omt = you.abs_omt_pos();
    const auto square = [&](const int x, const int y) {
        return project_to<coords::ms>(omt) + tripoint_rel_ms(x, y, 0);
    };
    // A run of wall and a chair where the avatar remembers rock.
    auto rows = nlohmann::ordered_json::array();
    for (auto y = 0; y < 24; ++y) { rows.push_back(std::string(24, '.')); }
    rows[3] = "..###" + std::string(19, '.');
    rows[5] = ".....h" + std::string(18, '.');
    rows[7] = "..ccc" + std::string(19, '.');
    const auto text =
        nlohmann::ordered_json{
            {"type", "mapgen"},
            {"method", "json"},
            {"om_terrain", "house_04"},
            {"object",
             {{"rows", rows},
              {"terrain", {{".", "t_floor"}, {"#", "t_wall"}, {"h", "t_floor"}, {"c", "t_floor"}}},
              {"furniture", {{"h", "f_chair"}, {"c", "f_counter"}}}}}}
            .dump();
    for (const auto& p : {point(3, 3), point(5, 5)}) {
        you.memorize_terrain_tile(square(p.x, p.y), "t_rock", 0, 0);
    }
    const auto was = live_ter(omt, point(3, 3));
    REQUIRE(was != ter_id("t_rock"));

    auto stamped = editor::stamp_mapgen(text, {.at = omt});
    REQUIRE(stamped);
    const auto wall = you.get_terrain_tile(square(3, 3));
    CHECK(wall.tile == "t_wall");
    // The middle of the run joins the walls east and west of it, as when it is seen.
    const auto joined = map_perception::orient(0b0110);
    CHECK(wall.subtile == joined.subtile);
    CHECK(wall.rotation == joined.rotation);
    CHECK(you.get_terrain_tile(square(5, 5)).tile == "t_floor");
    CHECK(you.get_memorized_tile(square(5, 5)).tile == "f_chair");
    // So does the middle of a run of counters, whose east end is remembered after it.
    const auto counter = you.get_memorized_tile(square(3, 7));
    CHECK(counter.tile == "f_counter");
    CHECK(counter.subtile == joined.subtile);
    CHECK(counter.rotation == joined.rotation);

    editor::swap_stamp(*stamped);
    CHECK(you.get_terrain_tile(square(3, 3)).tile == was.id().str());
    CHECK(you.get_memorized_tile(square(5, 5)).tile.empty());
}

TEST_CASE(
    "a stamp refuses what it cannot put in the loaded map and changes nothing", "[editor_mapgen]") {
    clear_all_state();
    const auto omt = get_avatar().abs_omt_pos();
    const auto was = get_overmapbuffer(get_map().get_bound_dimension()).ter(omt);
    const auto far = omt + point_rel_omt(40, 0);
    const auto text = entry_with_wall("house_04", point(2, 3));

    const auto outside = editor::stamp_mapgen(text, {.at = far});
    REQUIRE_FALSE(outside);
    CHECK(outside.error().find("loaded") != std::string::npos);
    CHECK(get_overmapbuffer(get_map().get_bound_dimension()).ter(far)
          != oter_str_id("house_04_north").id());

    const auto nested = editor::stamp_mapgen(
        R"({"type":"mapgen","method":"json","nested_mapgen_id":"x","object":{"mapgensize":[1,1],"rows":["."],"terrain":{".":"t_floor"}}})",
        {.at = omt});
    REQUIRE_FALSE(nested);
    CHECK(get_overmapbuffer(get_map().get_bound_dimension()).ter(omt) == was);
}

TEST_CASE(
    "a stamp of a mapgen whose overmap terrain is undefined is generated over the terrain there",
    "[editor_mapgen]") {
    clear_all_state();
    const auto omt = get_avatar().abs_omt_pos();
    const auto was = get_overmapbuffer(get_map().get_bound_dimension()).ter(omt);
    REQUIRE_FALSE(oter_str_id("editor_undefined_hut").is_valid());

    auto stamped =
        editor::stamp_mapgen(entry_with_wall("editor_undefined_hut", point(2, 3)), {.at = omt});
    REQUIRE(stamped);
    CHECK(live_ter(omt, point(2, 3)) == ter_id("t_wall"));
    CHECK(get_overmapbuffer(get_map().get_bound_dimension()).ter(omt) == was);

    editor::swap_stamp(*stamped);
    CHECK(live_ter(omt, point(2, 3)) != ter_id("t_wall"));
}

TEST_CASE(
    "applying a mapgen makes it the only one the game generates for its terrain",
    "[editor_mapgen]") {
    clear_all_state();
    const auto restore_mapgen = restore_on_out_of_scope<bool>(disable_mapgen);
    disable_mapgen = false;
    // A terrain of its own, so that no other test finds its mapgen replaced.
    const auto id = std::string("editor_apply_test");
    // The overmap square has no map extras and the dice are fixed, so whatever terrain the earlier
    // tests left there or however they rolled, nothing but the mapgen lands on it.
    const auto at = tripoint_abs_omt(60, 60, 0);
    auto& overmap = get_overmapbuffer(get_map().get_bound_dimension());
    const auto was = overmap.ter(at);
    overmap.ter_set(at, oter_str_id("crater").id());
    const auto restore_terrain = on_out_of_scope([&] { overmap.ter_set(at, was); });
    const auto generated_wall = [&](const point& p) {
        auto buffer = mapbuffer{};
        buffer.set_dimension_id(get_map().get_bound_dimension());
        auto constructor = mapgen_constructor(buffer);
        const auto picked = pick_mapgen_func(id);
        REQUIRE(picked);
        {
            const auto seeded = rng_deterministic_task_scope(1);
            REQUIRE(
                constructor
                    .generate(at, calendar::turn,
                              {.use_selected_mapgen = true, .selected_mapgen = picked})
                    .is_generated());
        }
        return constructor.ter(point_omt_ms(p.x, p.y)) == ter_id("t_wall");
    };

    REQUIRE(editor::apply_mapgen(entry_with_wall(id, point(2, 3))));
    CHECK(generated_wall(point(2, 3)));
    CHECK_FALSE(generated_wall(point(4, 4)));

    REQUIRE(editor::apply_mapgen(entry_with_wall(id, point(4, 4))));
    CHECK_FALSE(generated_wall(point(2, 3)));
    CHECK(generated_wall(point(4, 4)));

    const auto refused = editor::apply_mapgen(
        R"({"type":"mapgen","method":"json","om_terrain":"x"})");
    REQUIRE_FALSE(refused);
    CHECK(generated_wall(point(4, 4)));
}
