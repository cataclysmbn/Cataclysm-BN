#include "../src/map/map.h"
#include "../src/map/mapdata.h"
#include "calendar.h"
#include "catch/catch.hpp"
#include "game.h"
#include "line.h"
#include "map_helpers.h"
#include "monster.h"
#include "state_helpers.h"

#include <algorithm>
#include <memory>

struct tripoint;

static auto spawn_and_clear(const tripoint_bub_ms& pos, bool set_floor) -> monster& { // *NOPAD*
    if (set_floor) { get_map().set(pos, t_floor, f_null); }
    return spawn_test_monster("mon_zombie", pos);
}

static const time_point midday = calendar::turn_zero + 12_hours;

TEST_CASE("monsters shouldn't see through floors", "[vision]") {
    clear_all_state();
    calendar::turn = midday;
    monster& upper = spawn_and_clear({5, 5, 0}, true);
    monster& adjacent = spawn_and_clear({5, 6, 0}, true);
    monster& distant = spawn_and_clear({5, 3, 0}, true);
    monster& lower = spawn_and_clear({5, 5, -1}, true);
    monster& deep = spawn_and_clear({5, 5, -2}, true);
    monster& sky = spawn_and_clear({5, 5, 1}, false);

    // First check monsters whose vision should be blocked by floors.
    // One intervening floor between monsters.
    CHECK(!upper.sees(lower));
    CHECK(!lower.sees(upper));
    // Two intervening floors between monsters.
    CHECK(!upper.sees(deep));
    CHECK(!deep.sees(upper));
    // One intervening floor and an open space between monsters.
    CHECK(!sky.sees(lower));
    CHECK(!lower.sees(sky));
    // One intervening floor between monsters, and offset one tile horizontally.
    CHECK(!adjacent.sees(lower));
    CHECK(!lower.sees(adjacent));
    // One intervening floor between monsters, and offset two tiles horizontally.
    CHECK(!distant.sees(lower));
    CHECK(!lower.sees(distant));
    // Two intervening floors between monsters, and offset one tile horizontally.
    CHECK(!adjacent.sees(deep));
    CHECK(!deep.sees(adjacent));
    // Two intervening floor between monsters, and offset two tiles horizontally.
    CHECK(!distant.sees(deep));
    CHECK(!deep.sees(distant));

    // Then cases where they should be able to see each other.
    // No floor between monsters
    CHECK(upper.sees(sky));
    CHECK(sky.sees(upper));
    // Adjacent monsters.
    CHECK(upper.sees(adjacent));
    CHECK(adjacent.sees(upper));
    // distant monsters.
    CHECK(upper.sees(distant));
    CHECK(distant.sees(upper));
    // One intervening vertical tile and one intervening horizontal tile.
    CHECK(sky.sees(adjacent));
    CHECK(adjacent.sees(sky));
    // One intervening vertical tile and two intervening horizontal tiles.
    CHECK(sky.sees(distant));
    CHECK(distant.sees(sky));
}

TEST_CASE("monsters_dont_see_through_vehicle_holes", "[vision]") {
    clear_all_state();
    calendar::turn = midday;
    move_player_out_of_the_way();
    tripoint_bub_ms origin(60, 60, 0);

    get_map().add_vehicle(vproto_id("apc"), origin, -45_degrees, 0, 0);
    get_map().build_map_cache(0);

    tripoint_bub_ms mon_origin = origin + tripoint_rel_ms(-2, 1, 0);

    monster& inside = spawn_test_monster("mon_zombie", mon_origin);

    auto second_origin = mon_origin + tripoint_north_west;

    monster& outside = spawn_test_monster("mon_zombie", second_origin);

    CHECK(!inside.sees(outside));
    CHECK(!outside.sees(inside));
}

namespace {

struct sight_line {
    tripoint_bub_ms from;
    tripoint_bub_ms to;
};

} // namespace

// The blocker cache answers a pair of squares from either end and monsters plan in parallel, so
// what it holds must not depend on which end asked first. The Bresenham line from (10, 10) to
// (12, 11) crosses (11, 10) and the line back crosses (11, 11): a wall on (11, 10) blocks one way.
TEST_CASE("terrain_los_blocker_answers_a_pair_alike_whichever_end_asks_first", "[vision]") {
    clear_all_state();
    auto& here = get_map();
    // The map caches each pair as first traced; a rebound map has traced none of them yet.
    rebind_map_dimension(here.get_bound_dimension());
    const auto layout = [&](const int y) {
        here.ter_set(tripoint_bub_ms(11, 10 + y, 0), t_wall);
        return sight_line{.from = {10, 10 + y, 0}, .to = {12, 11 + y, 0}};
    };
    const auto asked_from_far_end = layout(0);
    const auto asked_from_near_end = layout(10);
    here.build_map_cache(0);
    const auto wall = tripoint_bub_ms(11, 10, 0);
    REQUIRE(
        std::ranges::contains(line_to(asked_from_far_end.from, asked_from_far_end.to), wall)
        != std::ranges::contains(line_to(asked_from_far_end.to, asked_from_far_end.from), wall));

    g->clear_turn_los_blocker_cache();
    CHECK(g->terrain_los_blocks_sight_between(asked_from_far_end.to, asked_from_far_end.from)
          == g->terrain_los_blocks_sight_between(asked_from_near_end.from, asked_from_near_end.to));
}
