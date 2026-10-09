#include "avatar.h"
#include "calendar.h"
#include "cata_utility.h"
#include "catch/catch.hpp"
#include "game.h"
#include "game_observation.h"
#include "game_session.h"
#include "item.h"
#include "json.h"
#include "map/map.h"
#include "map_helpers.h"
#include "monster.h"
#include "overmap/overmap.h"
#include "overmap/overmapbuffer.h"
#include "overmap/overmapbuffer_registry.h"
#include "player_activity.h"
#include "player_helpers.h"
#include "rng.h"
#include "state_helpers.h"
#include "weather/weather.h"
#include "world.h"

#include <algorithm>
#include <array>
#include <sstream>
#include <string>
#include <utility>

namespace {

struct game_ready_guard {
    bool previous;

    explicit game_ready_guard(const bool previous_running): previous(previous_running) {}

    ~game_ready_guard() { game_session::set_running(previous); }
};

auto saved(const auto& value) -> std::string {
    auto stream = std::ostringstream{};
    auto json = JsonOut{stream};
    value.serialize(json);
    return stream.str();
}

} // namespace

TEST_CASE("game observation marks an absent game as not ready", "[client][state]") {
    const auto reset_ready = game_ready_guard{game_session::running()};
    game_session::set_running(false);
    const auto state = game_observation::capture(nullptr);

    auto input = std::istringstream{state.json};
    auto json_in = JsonIn{input};
    auto root = json_in.get_object();
    root.allow_omitted_members();
    auto session = root.get_object("session");
    session.allow_omitted_members();
    CHECK_FALSE(session.get_bool("running"));
    CHECK_FALSE(root.get_bool("game_ready"));
    CHECK(root.get_object("avatar").empty());
}

TEST_CASE(
    "game observation reports the native activity lifecycle without consuming RNG",
    "[client][state][rng]") {
    clear_all_state();
    const auto cleanup = on_out_of_scope([]() { clear_all_state(); });
    const auto reset_ready = game_ready_guard{game_session::running()};
    build_test_map(ter_id("t_floor"));
    game_session::set_running(true);
    auto& you = get_avatar();
    you.assign_activity(activity_id("ACT_WAIT"), 1000);
    const auto engine = rng_get_engine();

    const auto active_state = game_observation::capture();
    CHECK(rng_get_engine() == engine);
    auto active_input = std::istringstream{active_state.json};
    auto active_json = JsonIn{active_input};
    auto active_root = active_json.get_object();
    active_root.allow_omitted_members();
    auto active_avatar = active_root.get_object("avatar");
    active_avatar.allow_omitted_members();
    auto active = active_avatar.get_object("activity");
    active.allow_omitted_members();
    CHECK(active.get_bool("active"));
    CHECK(active.get_string("id") == "ACT_WAIT");

    you.activity->set_to_null();
    const auto inactive_state = game_observation::capture();
    CHECK(rng_get_engine() == engine);
    auto inactive_input = std::istringstream{inactive_state.json};
    auto inactive_json = JsonIn{inactive_input};
    auto inactive_root = inactive_json.get_object();
    inactive_root.allow_omitted_members();
    auto inactive_avatar = inactive_root.get_object("avatar");
    inactive_avatar.allow_omitted_members();
    auto inactive = inactive_avatar.get_object("activity");
    inactive.allow_omitted_members();
    CHECK_FALSE(inactive.get_bool("active"));
    CHECK_FALSE(inactive.has_member("id"));
}

TEST_CASE("game observation contains player inventory and visible world only", "[client][state]") {
    clear_all_state();
    const auto cleanup = on_out_of_scope([]() { clear_all_state(); });
    const auto reset_ready = game_ready_guard{game_session::running()};
    auto& you = get_avatar();
    auto& here = get_map();
    const auto restore_turn = restore_on_out_of_scope<time_point>(calendar::turn);
    const auto restore_weather = restore_on_out_of_scope<weather_type_id>(get_weather().weather_id);
    calendar::turn = calendar::turn_zero + 12_hours;
    get_weather().weather_id = weather_type_id("clear");
    g->reset_light_level();
    build_test_map(ter_id("t_floor"));
    const auto center = you.bub_pos();
    const auto visible = center + tripoint_east;
    const auto visible_container = center + tripoint_north;
    const auto barrier = center + tripoint(4, 0, 0);
    const auto hidden = center + tripoint(5, 0, 0);
    REQUIRE(here.ter_set(barrier, ter_id("t_wall")));
    here.add_item_or_charges(visible, item::spawn("rock"));
    auto ground_backpack = item::spawn("backpack");
    REQUIRE(ground_backpack);
    ground_backpack->put_in(item::spawn("rock"));
    here.add_item_or_charges(visible_container, std::move(ground_backpack));
    here.add_item_or_charges(hidden, item::spawn("hammer"));
    spawn_test_monster("mon_zombie", visible);
    auto& visible_hallucination = spawn_test_monster("mon_zombie", center + tripoint(0, 1, 0));
    visible_hallucination.hallucination = true;
    spawn_test_monster("mon_zombie", hidden);
    here.invalidate_map_cache(0);
    here.build_map_cache(0);
    here.update_visibility_cache(0);
    REQUIRE(you.sees(barrier));
    REQUIRE(you.sees(visible));
    REQUIRE(you.sees(visible_hallucination));
    REQUIRE_FALSE(you.sees(hidden));

    auto backpack = item::spawn("backpack");
    REQUIRE(backpack);
    auto nested_backpack = item::spawn("backpack");
    REQUIRE(nested_backpack);
    nested_backpack->put_in(item::spawn("rock"));
    backpack->put_in(std::move(nested_backpack));
    you.i_add(std::move(backpack));
    game_session::set_running(true);
    const auto state = game_observation::capture();

    auto input = std::istringstream{state.json};
    auto json_in = JsonIn{input};
    auto root = json_in.get_object();
    root.allow_omitted_members();
    CHECK(root.get_bool("game_ready"));
    auto coverage = root.get_object("coverage");
    coverage.allow_omitted_members();
    CHECK(coverage.get_int("visible_map_radius") == 12);

    auto avatar = root.get_object("avatar");
    avatar.allow_omitted_members();
    auto found_nested_backpack = false;
    auto outer_backpack_count = 0;
    auto outer_rock_count = 0;
    auto nested_rock_count = 0;
    for (const auto item_value : avatar.get_array("inventory")) {
        auto entry = item_value.get_object();
        entry.allow_omitted_members();
        if (entry.get_string("type_id") != "backpack") { continue; }
        for (const auto contained : entry.get_array("contents")) {
            auto nested = contained.get_object();
            nested.allow_omitted_members();
            const auto type_id = nested.get_string("type_id");
            if (type_id == "rock") {
                ++outer_rock_count;
                continue;
            }
            if (type_id != "backpack") { continue; }
            ++outer_backpack_count;
            found_nested_backpack = true;
            for (const auto deeply_nested : nested.get_array("contents")) {
                auto leaf = deeply_nested.get_object();
                leaf.allow_omitted_members();
                if (leaf.get_string("type_id") == "rock") { ++nested_rock_count; }
            }
        }
    }
    CHECK(found_nested_backpack);
    CHECK(outer_backpack_count == 1);
    CHECK(outer_rock_count == 0);
    CHECK(nested_rock_count == 1);
    CHECK(avatar.get_array("bodyparts").size() >= you.get_all_body_parts().size());

    auto creature_count = 0;
    auto found_hammer = false;
    auto found_ground_backpack = false;
    auto ground_backpack_has_contents = false;
    auto visible_floor_is_traversable = false;
    auto barrier_is_traversable = true;
    auto observed_barrier_count = 0;
    for (const auto tile_value : root.get_array("visible_map")) {
        auto tile = tile_value.get_object();
        tile.allow_omitted_members();
        auto position = tile.get_object("position");
        position.allow_omitted_members();
        const auto tile_position =
            tripoint_bub_ms{position.get_int("x"), position.get_int("y"), position.get_int("z")};
        if (tile_position == visible) {
            visible_floor_is_traversable = tile.get_bool("traversable");
        } else if (tile_position == barrier) {
            ++observed_barrier_count;
            barrier_is_traversable = tile.get_bool("traversable");
        }
        if (tile.has_member("creature")) {
            auto creature = tile.get_object("creature");
            creature.allow_omitted_members();
            if (!creature.get_bool("player")) { ++creature_count; }
        }
        for (const auto item_value : tile.get_array("items")) {
            auto item = item_value.get_object();
            item.allow_omitted_members();
            const auto type_id = item.get_string("type_id");
            found_hammer = found_hammer || type_id == "hammer";
            found_ground_backpack = found_ground_backpack || type_id == "backpack";
            ground_backpack_has_contents =
                ground_backpack_has_contents
                || (type_id == "backpack" && item.has_member("contents"));
        }
    }
    CHECK(creature_count == 2);
    CHECK(!found_hammer);
    CHECK(found_ground_backpack);
    CHECK(!ground_backpack_has_contents);
    CHECK(observed_barrier_count == 1);
    CHECK(visible_floor_is_traversable);
    CHECK_FALSE(barrier_is_traversable);
}

TEST_CASE(
    "game observation names perishable items without updating their rot", "[client][state][rng]") {
    clear_all_state();
    const auto cleanup = on_out_of_scope([]() { clear_all_state(); });
    const auto reset_ready = game_ready_guard{game_session::running()};
    const auto restore_turn = restore_on_out_of_scope<time_point>(calendar::turn);
    calendar::turn = calendar::turn_zero + 12_hours;
    build_test_map(ter_id("t_floor"));
    auto& you = get_avatar();
    auto& here = get_map();
    const auto visible = you.bub_pos() + tripoint_east;
    auto carried = item::spawn("meat_cooked");
    auto ground = item::spawn("meat_cooked");
    auto backpack = item::spawn("backpack");
    backpack->put_in(item::spawn("meat_cooked"));
    REQUIRE(carried->goes_bad());
    you.i_add(std::move(carried));
    you.i_add(std::move(backpack));
    here.add_item_or_charges(visible, std::move(ground));
    here.invalidate_map_cache(0);
    here.build_map_cache(0);
    here.update_visibility_cache(0);
    REQUIRE(you.sees(visible));
    calendar::turn += 3_hours;
    game_session::set_running(true);
    const auto items_before = [&]() {
        auto result = saved(you);
        for (const auto* thing : here.i_at(visible)) { result += saved(*thing); }
        return result;
    };
    const auto before = items_before();
    const auto engine = rng_get_engine();

    const auto state = game_observation::capture();

    CHECK(state.json.find("meat_cooked") != std::string::npos);
    CHECK(items_before() == before);
    CHECK(rng_get_engine() == engine);
}

TEST_CASE(
    "game observation does not load saved overmaps outside memory", "[client][state][overmap]") {
    clear_all_state();
    const auto reset_ready = game_ready_guard{game_session::running()};
    build_test_map(ter_id("t_floor"));
    auto& you = get_avatar();
    const auto dimension = g->get_current_dimension_id();
    auto& buffer = get_overmapbuffer(dimension);
    // Saved overmap files are shared by the test world: remove them with the loaded copies.
    const auto cleanup = on_out_of_scope([&]() {
        buffer.clear();
        g->get_active_world()->delete_dimension_data(dimension.str());
        clear_all_state();
    });
    // The observed radius must reach an overmap other than the avatar's own; moving the avatar
    // across an overmap would make the map shift load thousands of submaps.
    const auto omt = project_to<coords::omt>(you.abs_pos());
    const auto own = project_to<coords::om>(omt.xy());
    const auto reaches_other = [&](const point& corner) {
        return project_to<coords::om>(omt.xy() + corner) != own;
    };
    const auto corners = std::array{point{-8, 0}, point{8, 0}, point{0, -8}, point{0, 8}};
    const auto reached = std::ranges::find_if(corners, reaches_other);
    REQUIRE(reached != corners.end());
    const auto neighbor = project_to<coords::om>(omt.xy() + *reached);
    buffer.get(neighbor);
    buffer.save(dimension);
    buffer.clear();
    buffer.get(point_abs_om{0, 0});
    REQUIRE(g->get_active_world()->overmap_exists(neighbor));
    REQUIRE(buffer.find_loaded(neighbor) == nullptr);
    game_session::set_running(true);
    const auto engine = rng_get_engine();

    const auto state = game_observation::capture();

    CHECK(state.json.find("known_overmap") != std::string::npos);
    CHECK(buffer.find_loaded(neighbor) == nullptr);
    CHECK(rng_get_engine() == engine);
}
