#include "../src/map/map.h"
#include "../src/map/mapdata.h"
#include "../src/map/submap.h"
#include "avatar.h"
#include "catch/catch.hpp"
#include "line.h"
#include "map_helpers.h"
#include "state_helpers.h"

#include <algorithm>

namespace {

struct sight_line {
    tripoint_bub_ms from;
    tripoint_bub_ms to;
};

// The Bresenham line from (10, 10) to (12, 11) crosses (11, 10) and the line back crosses
// (11, 11): a wall on (11, 10) blocks the line one way only.
auto wall_on_one_way(const point_rel_ms& offset) -> sight_line {
    get_map().ter_set(tripoint_bub_ms(11, 10, 0) + offset, t_wall);
    return {.from = tripoint_bub_ms(10, 10, 0) + offset, .to = tripoint_bub_ms(12, 11, 0) + offset};
}

/// A map whose sight line cache holds nothing yet.
auto fresh_map() -> map& { // *NOPAD*
    clear_all_state();
    auto& here = get_map();
    rebind_map_dimension(here.get_bound_dimension());
    return here;
}

} // namespace

// Monsters plan in parallel and the cache answers a pair of squares from either end, so what it
// holds must not depend on which end asked first.
TEST_CASE("map_sees_answers_a_pair_alike_whichever_end_asks_first", "[vision]") {
    auto& here = fresh_map();
    const auto far_end_first = wall_on_one_way({0, 0});
    const auto near_end_first = wall_on_one_way({0, 10});
    here.build_map_cache(0);
    const auto wall = tripoint_bub_ms(11, 10, 0);
    REQUIRE(std::ranges::contains(line_to(far_end_first.from, far_end_first.to), wall)
            != std::ranges::contains(line_to(far_end_first.to, far_end_first.from), wall));

    static_cast<void>(here.sees(far_end_first.to, far_end_first.from, -1));

    CHECK(here.sees(far_end_first.from, far_end_first.to, -1)
          == here.sees(near_end_first.from, near_end_first.to, -1));
}

TEST_CASE("map_sees_forgets_a_line_when_transparency_changes", "[vision]") {
    auto& here = fresh_map();
    const auto line = sight_line{.from = {10, 10, 0}, .to = {14, 10, 0}};
    const auto wall = tripoint_bub_ms(12, 10, 0);
    here.ter_set(wall, t_wall);
    here.build_map_cache(0);
    REQUIRE_FALSE(here.sees(line.from, line.to, -1));

    // Only the transparency cache is told, as a regenerated overmap tile does.
    auto at = point_sm_ms{};
    here.get_submap_at(wall, at)->set_ter(at, t_floor);
    here.set_transparency_cache_dirty(wall.z());
    here.build_map_cache(0);

    CHECK(here.sees(line.from, line.to, -1));
}

TEST_CASE("map_sees_forgets_its_lines_when_the_map_shifts", "[vision]") {
    auto& here = fresh_map();
    auto& you = get_avatar();
    const auto line = sight_line{.from = {60, 60, 0}, .to = {64, 60, 0}};
    here.ter_set(tripoint_bub_ms(62, 60, 0), t_wall);
    here.build_map_cache(0);
    REQUIRE_FALSE(here.sees(line.from, line.to, -1));

    // One submap east: the squares under the old line are open ground.
    you.setpos(map_local_to_abs(here, you.bub_pos() + tripoint_rel_ms(SEEX, 0, 0)));
    REQUIRE(here.ter(tripoint_bub_ms(62, 60, 0)) != t_wall);
    here.build_map_cache(0);

    CHECK(here.sees(line.from, line.to, -1));
}
