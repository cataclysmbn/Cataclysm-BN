#if defined(CATA_MCP)

#    include "avatar.h"
#    include "calendar.h"
#    include "cata_utility.h"
#    include "catch/catch.hpp"
#    include "client_input.h"
#    include "engine_client_session.h"
#    include "engine_client_world.h"
#    include "game.h"
#    include "game_constants.h"
#    include "game_session.h"
#    include "input.h"
#    include "item.h"
#    include "map/field.h"
#    include "map/map.h"
#    include "map_helpers.h"
#    include "map_memory.h"
#    include "map_perception.h"
#    include "monster.h"
#    include "state_helpers.h"
#    include "vehicle/vehicle.h"
#    include "vehicle/vpart_position.h"
#    include "weather/weather.h"

#    include <algorithm>
#    include <chrono>
#    include <iostream>
#    include <optional>
#    include <ranges>
#    include <string>

namespace {
namespace ec = engine_client;

/// A lit room in solid rock around the avatar, split by a wall with one doorway.
auto make_room() -> tripoint_bub_ms {
    clear_all_state();
    get_avatar().clear_map_memory();
    build_test_map(ter_id("t_floor"));
    calendar::turn = calendar::turn_zero + 12_hours;
    get_weather().weather_id = weather_type_id("clear");
    g->reset_light_level();
    game_session::set_running(true);
    auto& here = get_map();
    const auto start = tripoint_bub_ms(60, 60, 0);
    for (const auto y : std::views::iota(0, here.getmapsize() * SEEY)) {
        for (const auto x : std::views::iota(0, here.getmapsize() * SEEX)) {
            const auto inside = std::abs(x - start.x()) <= 8 && std::abs(y - start.y()) <= 8;
            here.ter_set(tripoint_bub_ms(x, y, 0), ter_id(inside ? "t_floor" : "t_rock"));
        }
    }
    for (const auto y : std::views::iota(-8, 9)) {
        here.ter_set(start + tripoint(3, y, 0), ter_id("t_wall"));
    }
    here.ter_set(start + tripoint(3, 5, 0), ter_id("t_door_c"));
    get_avatar().setpos(start);
    map_perception::acquire();
    return start;
}

/// Open grass under a clear sky at `hour`, the avatar in the middle of it.
auto make_open_ground(const int hour) -> tripoint_bub_ms {
    clear_all_state();
    get_avatar().clear_map_memory();
    clear_map();
    make_open_air_uniform();
    calendar::turn = calendar::turn_zero + time_duration::from_hours(hour);
    get_weather().weather_id = weather_type_id("clear");
    g->reset_light_level();
    game_session::set_running(true);
    const auto start = tripoint_bub_ms(g_half_mapsize_x, g_half_mapsize_y, 0);
    get_avatar().setpos(start);
    get_map().invalidate_map_cache(start.z());
    map_perception::acquire();
    return start;
}

/// Open air above the avatar holds what a test put in it until it is removed: `clear_map` clears
/// only the levels from -2 to 0.
auto clear_air_above(const tripoint_bub_ms& furniture_at) -> void {
    get_map().furn_set(furniture_at, furn_id("f_null"));
    clear_vehicles();
    for (const auto z : std::views::iota(1, OVERMAP_HEIGHT + 1)) {
        clear_fields(z);
        clear_items(z);
    }
}

/// A session that publishes patches, every one of which it checks against a full capture.
struct watched_session {
    watched_session() {
        context.register_action("YES");
        game_client::begin_input_boundary();
        authority.set_world_capture(&ec::world::capture_world, &ec::world::capture_patch);
        authority.set_oracle(true);
        publish();
    }
    auto publish() -> void {
        map_perception::acquire();
        REQUIRE(authority.publish_boundary());
        CHECK(authority.oracle_mismatches().empty());
    }
    auto step_to(const tripoint_bub_ms& to) -> void {
        get_avatar().setpos(to);
        publish();
    }
    input_context context{"YESNO"};
    game_client::input_context_scope input{context, "YESNO"};
    ec::session authority;
};
} // namespace

TEST_CASE(
    "a patch publication equals the full capture through a fight, items, doors, fields and light",
    "[engine_client_patch]") {
    const auto start = make_room();
    auto& here = get_map();
    auto& you = get_avatar();
    auto watched = watched_session{};

    SECTION("a wait with a monster in view, then the fight") {
        auto& zombie = spawn_test_monster("mon_zombie", start + tripoint(5, 0, 0));
        watched.publish();
        for (const auto turn : std::views::iota(0, 12)) {
            calendar::turn += 10_minutes;
            zombie.setpos(start + tripoint(5 - turn / 3, turn % 3, 0));
            watched.publish();
        }
        zombie.apply_damage(&you, bodypart_id("torso"), 9);
        watched.publish();
        zombie.die(&you);
        watched.publish();
        g->remove_zombie(zombie);
        watched.publish();
    }

    SECTION("items picked up, dropped and left on the floor") {
        here.add_item_or_charges(start + tripoint_east, item::spawn("rock"));
        here.add_item_or_charges(start, item::spawn("hammer"));
        watched.publish();
        you.i_add(item::spawn("rock"));
        you.i_add(item::spawn("rock"));
        watched.publish();
        here.i_clear(start);
        here.add_item_or_charges(start, item::spawn("screwdriver"));
        watched.publish();
        here.i_clear(start + tripoint_east);
        watched.publish();
        static_cast<void>(you.i_rem(0));
        watched.publish();
    }

    SECTION("doors, furniture and terrain changed in and out of sight") {
        const auto door = start + tripoint(3, 5, 0);
        here.ter_set(door, ter_id("t_door_o"));
        watched.publish();
        here.furn_set(start + tripoint(1, 1, 0), furn_id("f_table"));
        here.ter_set(start + tripoint(6, 0, 0), ter_id("t_dirt"));
        watched.publish();
        here.ter_set(door, ter_id("t_door_c"));
        here.furn_set(start + tripoint(1, 1, 0), furn_id("f_null"));
        watched.publish();
        watched.step_to(start + tripoint(1, 5, 0));
        here.ter_set(start + tripoint(6, 0, 0), ter_id("t_floor"));
        watched.publish();
    }

    SECTION("fields appearing, changing and burning out") {
        here.add_field(start + tripoint(1, 0, 0), field_type_id("fd_smoke"), 2);
        here.add_field(start + tripoint(5, 0, 0), field_type_id("fd_fire"), 1);
        watched.publish();
        here.set_field_intensity(start + tripoint(1, 0, 0), field_type_id("fd_smoke"), 3);
        watched.publish();
        here.remove_field(start + tripoint(5, 0, 0), field_type_id("fd_fire"));
        watched.publish();
        here.remove_field(start + tripoint(1, 0, 0), field_type_id("fd_smoke"));
        watched.publish();
    }

    SECTION("day and night") {
        for (const auto hour : {20, 23, 2, 5, 8, 12}) {
            calendar::turn = calendar::turn_zero + time_duration::from_hours(hour);
            g->reset_light_level();
            watched.publish();
        }
        watched.step_to(start + tripoint(2, 3, 0));
        calendar::turn = calendar::turn_zero + 2_hours;
        g->reset_light_level();
        watched.publish();
    }

    SECTION("walking through the doorway shows and forgets the rooms") {
        const auto door = start + tripoint(3, 5, 0);
        here.ter_set(door, ter_id("t_floor"));
        for (const auto step :
             {tripoint(1, 3, 0), tripoint(2, 5, 0), tripoint(3, 5, 0), tripoint(5, 5, 0),
              tripoint(7, 3, 0), tripoint(4, 5, 0), tripoint(0, 0, 0)}) {
            watched.step_to(start + step);
        }
        you.clear_memorized_tile(map_local_to_abs(here, start + tripoint(1, 1, 0)));
        watched.publish();
        you.clear_map_memory();
        watched.publish();
    }

    SECTION("a level above and back") {
        for (const auto y : std::views::iota(-4, 5)) {
            for (const auto x : std::views::iota(-4, 5)) {
                here.ter_set(start + tripoint(x, y, 1), ter_id("t_floor"));
            }
        }
        watched.publish();
        watched.step_to(start + tripoint(0, 0, 1));
        watched.step_to(start + tripoint(1, 1, 1));
        watched.step_to(start);
        watched.publish();
        // The level above is not part of what other cases build.
        for (const auto y : std::views::iota(-4, 5)) {
            for (const auto x : std::views::iota(-4, 5)) {
                here.ter_set(start + tripoint(x, y, 1), ter_id("t_open_air"));
            }
        }
    }

    SECTION("vehicles appearing, moving on and removed") {
        for (const auto turn : std::views::iota(0, 4)) {
            const auto at = start + tripoint(-4 + turn, 3 - turn, 0);
            auto* veh = here.add_vehicle(vproto_id("golf_cart"), at, 0_degrees, 0, 0, false);
            REQUIRE(veh != nullptr);
            watched.publish();
            here.destroy_vehicle(veh);
            watched.publish();
        }
    }

    CHECK(watched.authority.oracle_mismatches().empty());
}

TEST_CASE(
    "a patch over an unchanged scene names only the squares the avatar sees",
    "[engine_client_patch]") {
    const auto start = make_room();
    auto watched = watched_session{};
    for (const auto step :
         {tripoint(1, 0, 0), tripoint(2, 2, 0), tripoint(3, 5, 0), tripoint(5, 5, 0)}) {
        watched.step_to(start + step);
    }
    watched.publish();
    const auto& current = watched.authority.current();
    REQUIRE(current);
    REQUIRE(current->value.world.cells.size() > 100);
    const auto patch = ec::world::capture_patch(*current, true);
    CHECK_FALSE(patch.exhaustive);
    const auto seen = std::ranges::count_if(current->value.world.cells, [](const auto& entry) {
        return entry.second.known == ec::knowledge::visible;
    });
    CHECK(patch.cells.size() + patch.unknown.size() <= static_cast<std::size_t>(seen) + 8);
    CHECK(patch.cells.size() + patch.unknown.size() < current->value.world.cells.size() / 2);
}

TEST_CASE(
    "a patch publication equals the full capture walking open ground in daylight",
    "[engine_client_patch][open_ground]") {
    const auto start = make_open_ground(12);
    auto& here = get_map();
    auto watched = watched_session{};

    SECTION("steps, a long jump, and a field and an item on the ground") {
        for (const auto step : {tripoint(1, 0, 0), tripoint(2, 1, 0), tripoint(2, 2, 0)}) {
            watched.step_to(start + step);
        }
        here.add_field(start + tripoint(4, 4, 0), field_type_id("fd_smoke"), 2);
        here.add_item_or_charges(start + tripoint(-3, 2, 0), item::spawn("rock"));
        watched.publish();
        watched.step_to(start + tripoint(3, 2, 0));
        watched.step_to(start + tripoint(-20, 17, 0));
        watched.step_to(start + tripoint(-21, 17, 0));
        here.remove_field(start + tripoint(4, 4, 0), field_type_id("fd_smoke"));
        here.i_clear(start + tripoint(-3, 2, 0));
        watched.publish();
    }

    SECTION("dusk, night and dawn over open ground") {
        for (const auto hour : {17, 19, 20, 22, 4, 6, 8}) {
            calendar::turn = calendar::turn_zero + time_duration::from_hours(hour);
            g->reset_light_level();
            watched.publish();
            watched.step_to(start + tripoint(hour % 3, hour % 2, 0));
        }
    }

    SECTION("a roof above and back") {
        for (const auto y : std::views::iota(-4, 5)) {
            for (const auto x : std::views::iota(-4, 5)) {
                here.ter_set(start + tripoint(x, y, 1), ter_id("t_flat_roof"));
            }
        }
        watched.publish();
        watched.step_to(start + tripoint(1, 0, 0));
        watched.step_to(start + tripoint(0, 0, 1));
        watched.step_to(start + tripoint(1, 1, 1));
        watched.step_to(start);
        watched.step_to(start + tripoint(9, 0, 0));
        // The level above is not part of what other cases build.
        for (const auto y : std::views::iota(-4, 5)) {
            for (const auto x : std::views::iota(-4, 5)) {
                here.ter_set(start + tripoint(x, y, 1), ter_id("t_open_air"));
            }
        }
    }

    CHECK(watched.authority.oracle_mismatches().empty());
}

namespace {
/// The patches a session asked for that continued the last one and still named every cell.
auto exhaustive_continuations = 0;
auto counting_patch(const ec::snapshot& previous, const bool consecutive) -> ec::world_patch {
    auto patch = ec::world::capture_patch(previous, consecutive);
    if (consecutive && patch.exhaustive) { ++exhaustive_continuations; }
    return patch;
}
} // namespace

TEST_CASE(
    "a patch publication equals the full capture across a shift of the map",
    "[engine_client_patch][map_shift]") {
    make_open_ground(12);
    auto& here = get_map();
    auto& you = get_avatar();
    auto watched = watched_session{};
    watched.authority.set_world_capture(&ec::world::capture_world, &counting_patch);
    exhaustive_continuations = 0;
    auto shifts = 0;
    const auto walk = [&](const tripoint& step, const int count) {
        for (const auto i : std::views::iota(0, count)) {
            static_cast<void>(i);
            const auto origin = here.get_abs_sub();
            static_cast<void>(g->place_player(you.bub_pos() + step));
            shifts += here.get_abs_sub() != origin ? 1 : 0;
            watched.publish();
        }
    };
    walk(tripoint_east, 30);
    walk(tripoint_west, 30);
    walk(tripoint(1, 1, 0), 30);
    walk(tripoint(-1, -1, 0), 30);
    walk(tripoint_north, 30);
    walk(tripoint_south, 30);
    CHECK(shifts >= 2);
    CHECK(exhaustive_continuations == 0);
    CHECK(watched.authority.oracle_mismatches().empty());
}

TEST_CASE(
    "a patch over open ground names only the squares that changed",
    "[engine_client_patch][open_ground]") {
    const auto start = make_open_ground(12);
    auto watched = watched_session{};
    watched.step_to(start + tripoint(1, 0, 0));
    const auto& current = watched.authority.current();
    REQUIRE(current);
    const auto seen = std::ranges::count_if(current->value.world.cells, [](const auto& entry) {
        return entry.second.known == ec::knowledge::visible;
    });
    REQUIRE(seen > 10000);
    SECTION("a turn in place") {
        calendar::turn += 1_turns;
        map_perception::acquire();
        const auto patch = ec::world::capture_patch(*current, true);
        CHECK_FALSE(patch.exhaustive);
        CHECK(patch.cells.size() + patch.unknown.size() < 100);
    }
    SECTION("a step") {
        get_avatar().setpos(start + tripoint(2, 0, 0));
        map_perception::acquire();
        const auto patch = ec::world::capture_patch(*current, true);
        CHECK_FALSE(patch.exhaustive);
        CHECK(patch.cells.size() + patch.unknown.size() < static_cast<std::size_t>(seen) / 10);
    }
}

TEST_CASE(
    "a patch over open ground builds a cell only for squares whose inputs changed",
    "[engine_client_patch][open_ground]") {
    const auto start = make_open_ground(12);
    auto watched = watched_session{};
    watched.step_to(start + tripoint(1, 0, 0));
    const auto& current = watched.authority.current();
    REQUIRE(current);
    const auto seen = static_cast<
        std::size_t>(std::ranges::count_if(current->value.world.cells, [](const auto& entry) {
        return entry.second.known == ec::knowledge::visible;
    }));
    REQUIRE(seen > 10000);
    const auto built_by = [&] {
        const auto before = ec::world::visible_cells_built();
        static_cast<void>(ec::world::capture_patch(*current, true));
        return ec::world::visible_cells_built() - before;
    };
    SECTION("a turn in place") {
        calendar::turn += 1_turns;
        map_perception::acquire();
        CHECK(built_by() < seen / 100);
    }
    SECTION("a step") {
        get_avatar().setpos(start + tripoint(2, 0, 0));
        map_perception::acquire();
        CHECK(built_by() < seen / 20);
    }
}

TEST_CASE(
    "a patch over open ground names again the square whose input changed",
    "[engine_client_patch][open_ground]") {
    const auto start = make_open_ground(12);
    auto& here = get_map();
    auto watched = watched_session{};
    watched.step_to(start + tripoint(1, 0, 0));
    // A square far enough from the avatar to be plain ground the patch rebuilds only on a change.
    const auto at = start + tripoint(7, 5, 0);
    const auto position = ec::world::position_at(at);
    const auto cleanup = on_out_of_scope([&] {
        here.ter_set(at, ter_id("t_grass"));
        here.ter_set(at + tripoint_east, ter_id("t_grass"));
        here.furn_set(at, furn_id("f_null"));
        here.trap_set(at, trap_id("tr_null"));
        here.i_clear(at);
        clear_fields(at.z());
        clear_vehicles();
    });
    // The session hands out a copy of its snapshot, which the lookup must not outlive.
    const auto cell_at = [&] {
        const auto snapshot = watched.authority.current();
        const auto found = snapshot->value.world.cells.find(position);
        return found == snapshot->value.world.cells.end()
                 ? std::optional<ec::cell>{}
                 : std::optional(found->second);
    };
    const auto before = cell_at();
    REQUIRE(before);
    // The oracle compares every publication with a full capture, which names the changed cell.
    SECTION("its terrain") {
        here.ter_set(at, ter_id("t_dirt"));
        watched.publish();
        CHECK(cell_at() != before);
    }
    SECTION("the terrain beside it a wall connects to") {
        here.ter_set(at, ter_id("t_wall"));
        watched.publish();
        const auto alone = cell_at();
        here.ter_set(at + tripoint_east, ter_id("t_wall"));
        watched.publish();
        CHECK(cell_at() != alone);
    }
    SECTION("what lies on it") {
        here.add_item_or_charges(at, item::spawn("rock"));
        watched.publish();
        CHECK(cell_at() != before);
    }
    SECTION("the field on it") {
        here.add_field(at, field_type_id("fd_smoke"), 1);
        watched.publish();
        CHECK(cell_at() != before);
    }
    SECTION("the furniture on it") {
        here.furn_set(at, furn_id("f_table"));
        watched.publish();
        CHECK(cell_at() != before);
    }
    SECTION("the trap on it") {
        here.trap_set(at, trap_id("tr_beartrap"));
        // The avatar sees only the traps it knows.
        get_avatar().add_known_trap(at, here.tr_at(at));
        watched.publish();
        CHECK(cell_at() != before);
    }
    SECTION("the vehicle on it") {
        REQUIRE(here.add_vehicle(vproto_id("golf_cart"), at, 0_degrees, 0, 0, false) != nullptr);
        watched.publish();
        CHECK(cell_at() != before);
    }
    SECTION("its light") {
        calendar::turn = calendar::turn_zero + 2_hours;
        g->reset_light_level();
        watched.publish();
        CHECK(cell_at() != before);
    }
    SECTION("the season") {
        calendar::turn += calendar::season_length();
        watched.publish();
    }
    SECTION("what it was, once it is plain again") {
        here.add_item_or_charges(at, item::spawn("rock"));
        watched.publish();
        here.i_clear(at);
        watched.publish();
        CHECK(cell_at() == before);
    }
    CHECK(watched.authority.oracle_mismatches().empty());
}

TEST_CASE(
    "a patch names the squares whose memory changed and is exhaustive after too many changes",
    "[engine_client_patch]") {
    const auto start = make_room();
    auto& you = get_avatar();
    auto& here = get_map();
    auto watched = watched_session{};
    watched.step_to(start + tripoint(1, 0, 0));
    // Inside the rock around the room, where the avatar sees nothing.
    const auto far = start + tripoint(12, 0, 0);
    const auto abs = map_local_to_abs(here, far);
    const auto position = ec::position{.dim = "", .x = abs.x(), .y = abs.y(), .z = abs.z()};

    SECTION("one forgotten square") {
        you.memorize_terrain_tile(abs, "t_floor", 0, 0);
        watched.publish();
        REQUIRE(watched.authority.current()->value.world.cells.contains(position));
        you.clear_memorized_tile(abs);
        const auto patch = ec::world::capture_patch(*watched.authority.current(), true);
        CHECK_FALSE(patch.exhaustive);
        CHECK(patch.cells.size() + patch.unknown.size() < 5);
        CHECK(patch.unknown.contains(position));
    }

    SECTION("more writes than the log keeps") {
        for (const auto i : std::views::iota(0, (1 << 18) + 1)) {
            you.memorize_terrain_tile(
                tripoint_abs_ms(abs.x() + i, abs.y() + 1000, abs.z()), "t_floor", 0, 0);
        }
        CHECK(ec::world::capture_patch(*watched.authority.current(), true).exhaustive);
        // The log starts over: the next patch is a patch again.
        watched.publish();
        CHECK_FALSE(ec::world::capture_patch(*watched.authority.current(), true).exhaustive);
    }
}

TEST_CASE("a patch that does not continue the last one names every cell", "[engine_client_patch]") {
    const auto start = make_room();
    auto watched = watched_session{};
    watched.step_to(start + tripoint(1, 1, 0));
    const auto& current = watched.authority.current();
    REQUIRE(current);
    CHECK_FALSE(ec::world::capture_patch(*current, true).exhaustive);
    CHECK(ec::world::capture_patch(*current, false).exhaustive);
    auto other = *current;
    other.at.epoch = "another";
    CHECK(ec::world::capture_patch(other, true).exhaustive);
}

TEST_CASE("open air publishes a cell only while something is in it", "[engine_client_patch]") {
    const auto start = make_room();
    auto& here = get_map();
    const auto air = start + tripoint(1, 0, 1);
    const auto abs = map_local_to_abs(here, air);
    const auto position = ec::position{.dim = "", .x = abs.x(), .y = abs.y(), .z = abs.z()};
    map_perception::acquire();
    REQUIRE(here.has_flag(TFLAG_NO_MEMORY, air));
    CHECK_FALSE(ec::world::capture_world().cells.contains(position));
    here.add_field(air, field_type_id("fd_smoke"), 1);
    CHECK(ec::world::capture_world().cells.contains(position));
    here.remove_field(air, field_type_id("fd_smoke"));
    CHECK_FALSE(ec::world::capture_world().cells.contains(position));
    here.add_item_or_charges(air, item::spawn("rock"));
    CHECK(ec::world::capture_world().cells.contains(position));
    here.i_clear(air);
}

TEST_CASE(
    "anything placed in open air above the avatar publishes a cell", "[engine_client_patch]") {
    const auto start = make_open_ground(12);
    auto& here = get_map();
    // One square on each of three levels, each in a submap of its own.
    const auto item_at = start + tripoint(0, 0, 2);
    const auto field_at = start + tripoint(SEEX, 0, 4);
    const auto furniture_at = start + tripoint(0, SEEY, 6);
    const auto vehicle_at = start + tripoint(-SEEX, 0, 3);
    const auto cleanup = on_out_of_scope([&] { clear_air_above(furniture_at); });
    const auto published = [&](const tripoint_bub_ms& p) {
        map_perception::acquire();
        const auto abs = map_local_to_abs(here, p);
        return ec::world::capture_world().cells.contains(
            ec::position{.dim = "", .x = abs.x(), .y = abs.y(), .z = abs.z()});
    };
    for (const auto& p : {item_at, field_at, furniture_at, vehicle_at}) {
        REQUIRE(here.has_flag(TFLAG_NO_MEMORY, p));
        REQUIRE_FALSE(published(p));
    }
    here.add_item_or_charges(item_at, item::spawn("rock"));
    here.add_field(field_at, field_type_id("fd_smoke"), 1);
    here.furn_set(furniture_at, furn_id("f_table"));
    REQUIRE(
        here.add_vehicle(vproto_id("golf_cart"), vehicle_at, 0_degrees, 0, 0, false) != nullptr);
    CHECK(published(item_at));
    CHECK(published(field_at));
    CHECK(published(furniture_at));
    CHECK(published(vehicle_at));
    // The same square a patch later: the oracle compares it with a full capture.
    auto watched = watched_session{};
    here.i_clear(item_at);
    watched.publish();
    here.furn_set(furniture_at, furn_id("f_null"));
    watched.publish();
}

TEST_CASE(
    "a submap of open air is skipped only while every square in it is bare",
    "[engine_client_patch]") {
    const auto start = make_open_ground(12);
    auto& here = get_map();
    const auto item_at = start + tripoint(0, 0, 2);
    const auto field_at = start + tripoint(SEEX, 0, 4);
    const auto furniture_at = start + tripoint(0, SEEY, 6);
    const auto vehicle_at = start + tripoint(-SEEX, 0, 3);
    const auto cleanup = on_out_of_scope([&] { clear_air_above(furniture_at); });
    const auto untouched = start + tripoint(-SEEX, -SEEY, 5);
    const auto skipped = [&](const tripoint_bub_ms& p) {
        return ec::world::bare_submap(here, p, ec::world::vehicle_submaps(here));
    };
    // The check of `bare_unmemorized`, square by square.
    const auto holds_nothing = [&](const tripoint_bub_ms& p) {
        return here.ter(p)->has_flag(TFLAG_NO_MEMORY) && here.furn(p) == f_null && !here.veh_at(p)
            && !here.sees_some_items(p, get_avatar())
            && std::ranges::none_of(here.field_at(p), [](const auto& field) {
                   return field.second.is_field_alive();
               });
    };
    // Every square of every submap the capture would skip holds nothing.
    const auto every_skipped_is_bare = [&]() {
        map_perception::acquire();
        const auto vehicles = ec::world::vehicle_submaps(here);
        const auto visible = map_perception::visible_cells(here);
        const auto skips = std::ranges::count_if(visible, [&](const auto& p) {
            return ec::world::bare_submap(here, p, vehicles);
        });
        const auto wrong = std::ranges::count_if(visible, [&](const auto& p) {
            return ec::world::bare_submap(here, p, vehicles) && !holds_nothing(p);
        });
        CHECK(skips > 0);
        CHECK(wrong == 0);
    };
    CHECK(skipped(untouched));
    every_skipped_is_bare();
    here.add_item_or_charges(item_at, item::spawn("rock"));
    here.add_field(field_at, field_type_id("fd_smoke"), 1);
    here.furn_set(furniture_at, furn_id("f_table"));
    REQUIRE(
        here.add_vehicle(vproto_id("golf_cart"), vehicle_at, 0_degrees, 0, 0, false) != nullptr);
    for (const auto& p : {item_at, field_at, furniture_at, vehicle_at}) { CHECK_FALSE(skipped(p)); }
    CHECK(skipped(untouched));
    every_skipped_is_bare();
}

/// Milliseconds a publication costs with a full capture and with a patch, for an avatar who has
/// memorized `squares` squares: `cata_test-mcp "[.engine_client_patch_cost]"` prints them.
TEST_CASE("the cost of a publication by squares memorized", "[.engine_client_patch_cost]") {
    for (const auto radius : {0, 10, 25, 50}) {
        const auto start = make_room();
        auto& you = get_avatar();
        for (const auto y : std::views::iota(-radius, radius + 1)) {
            for (const auto x : std::views::iota(-radius, radius + 1)) {
                you.memorize_terrain_tile(
                    map_local_to_abs(get_map(), start + tripoint(x, y, 0)), "t_floor", 0, 0);
            }
        }
        for (const auto patched : {false, true}) {
            auto context = input_context{"YESNO"};
            auto input = game_client::input_context_scope{context, "YESNO"};
            context.register_action("YES");
            game_client::begin_input_boundary();
            auto authority = ec::session{};
            authority.set_world_capture(
                &ec::world::capture_world, patched ? &ec::world::capture_patch : nullptr);
            map_perception::acquire();
            REQUIRE(authority.publish_boundary());
            const auto turns = 100;
            auto elapsed = std::chrono::steady_clock::duration{};
            for (const auto turn : std::views::iota(0, turns)) {
                calendar::turn += 1_turns;
                map_perception::acquire();
                const auto began = std::chrono::steady_clock::now();
                REQUIRE(authority.publish_boundary());
                elapsed += std::chrono::steady_clock::now() - began;
                CHECK(turn >= 0);
            }
            std::cerr << "memorized " << (2 * radius + 1) * (2 * radius + 1) << ", "
                      << (patched ? "patch" : "full") << ": "
                      << std::chrono::duration<double, std::milli>(elapsed).count() / turns
                      << " ms/publication\n";
        }
    }
}

#endif
