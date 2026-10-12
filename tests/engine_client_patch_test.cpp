#if defined(CATA_MCP)

#    include "avatar.h"
#    include "calendar.h"
#    include "catch/catch.hpp"
#    include "client_input.h"
#    include "engine_client_session.h"
#    include "engine_client_world.h"
#    include "game.h"
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

#endif
