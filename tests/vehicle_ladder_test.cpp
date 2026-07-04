#include "catch/catch.hpp"
#include "coordinates.h"
#include "game_constants.h"
#include "map.h"
#include "map_helpers.h"
#include "state_helpers.h"
#include "type_id.h"
#include "units.h"
#include "veh_type.h"
#include "vehicle.h"
#include "vehicle_part.h"

#include <ranges>

// Regression test for issue #9590: the multi-z "3-story rope ladder" (vehicle part
// ladder_3, flag LADDER, length 3) must be detectable along the whole rope it hangs,
// not just at the top tile where the part actually lives. map::has_rope_at() drives both
// climbing UP from the ground and rendering the rope below the vehicle; a refactor
// (#9543) that re-keyed map::cached_veh_rope from 2D (x,y) to 3D (x,y,z) accidentally
// limited the cache to the single top tile, so the ladder could only be climbed down and
// was invisible from below.

TEST_CASE("rope_ladder_spans_full_column_in_rope_cache", "[vehicle][ladder][zlevel]") {
    clear_all_state();
    auto& here = get_map();
    build_test_map(ter_id("t_pavement"));

    const tripoint_bub_ms vpos(60, 60, 0);
    auto* veh = here.add_vehicle(vproto_id("none"), vpos, 0_degrees, 0, 0);
    REQUIRE(veh != nullptr);
    REQUIRE(veh->install_part(tripoint_mnt_veh::zero(), vpart_id("frame_vertical"), true) >= 0);
    const auto ladder = veh->install_part(tripoint_mnt_veh::zero(), vpart_id("ladder_3"), true);
    REQUIRE(ladder >= 0);

    here.add_vehicle_to_cache(veh);
    here.build_map_cache(vpos.z(), true);

    const auto top = veh->bub_part_location(ladder);
    const auto len = veh->part(ladder).info().ladder_length();
    REQUIRE(len == 3);

    SECTION("rope is present from the part down to ladder_length below it") {
        // Top tile: needed so a boarded player can climb DOWN.
        CHECK(here.has_rope_at(top));
        // Hanging rope: needed to climb UP from the ground and to render the rope.
        for (const auto dz : std::views::iota(1, len + 1)) {
            CHECK(here.has_rope_at(tripoint_bub_ms(top.xy(), top.z() - dz)));
        }
        // One tile past the ladder's reach must NOT report rope.
        CHECK_FALSE(here.has_rope_at(tripoint_bub_ms(top.xy(), top.z() - (len + 1))));
    }

    SECTION("removing the ladder clears the whole column") {
        // remove_part() marks the part removed and returns shift_if_needed() (not a
        // success flag); part_removal_cleanup() is what invokes clear_vehicle_point_from_cache.
        veh->remove_part(ladder);
        veh->part_removal_cleanup();
        for (const auto dz : std::views::iota(0, len + 1)) {
            CHECK_FALSE(here.has_rope_at(tripoint_bub_ms(top.xy(), top.z() - dz)));
        }
    }
}
