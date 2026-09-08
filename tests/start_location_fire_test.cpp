#include "avatar.h"
#include "catch/catch.hpp"
#include "coordinates.h"
#include "field_type.h"
#include "game.h"
#include "map.h"
#include "map_helpers.h"
#include "point.h"
#include "start_location.h"
#include "state_helpers.h"
#include "type_id.h"

// Regression test for #9169 ("Challenge - Really Bad Day" starts with no fire).
//
// The bad_day scenario carries FIRE_START, so game::start_game() calls
// start_location::burn(), which is meant to ignite interior FLAMMABLE tiles around
// the player. The old burn() path built a single-z detached map and queried
// is_outside(); a zlevels=false map cannot model the z+1 roof level, so its
// outside_cache is filled all-true and is_outside() returns true for EVERY tile.
// burn()'s interior filter then rejects all candidates and places zero fires.
//
// This test paints a roofed (=> "inside") flammable building on the live map around
// the avatar and asserts that burn() actually places fd_fire on interior tiles.
// It FAILS on the buggy single-z detached-map implementation and PASSES once burn()
// operates on the live multi-z map (g->m), whose outside cache is correct.

TEST_CASE(
    "start_location_burn_places_fire_on_interior_flammable_tiles",
    "[start_location][field]["
    "fire]") {
    clear_all_state();
    map& here = get_map();

    const ter_str_id floor_primitive("t_floor_primitive"); // interior floor, FLAMMABLE_ASH
    const ter_str_id floor_roof("t_floor");                // used as a roof on z+1

    // Establish the player's map frame before painting the absolute fixture.  setpos() may
    // load/shift the reality bubble, so doing this afterward would leave the cache and the
    // fixture referring to different bubble-local tiles.
    const auto center = test_origin;
    get_avatar().setpos(center);

    // Build an 11x11 flammable interior at z=0 with a roof one tile larger at z=1,
    // so the 3x3-above check marks every interior tile as "inside".
    for (int x = -6; x <= 6; ++x) {
        for (int y = -6; y <= 6; ++y) {
            const auto pos = center + tripoint_rel_ms(x, y, 0);
            here.get_mapbuffer().set_ter(pos + tripoint_rel_ms::above(), floor_roof);
            if (x >= -5 && x <= 5 && y >= -5 && y <= 5) {
                here.get_mapbuffer().set_ter(pos, floor_primitive);
            }
        }
    }
    here.set_outside_cache_dirty(0);
    here.set_outside_cache_dirty(1);
    here.invalidate_map_cache(0);
    here.build_map_cache(0, true);
    here.invalidate_map_cache(1);
    here.build_map_cache(1, true);

    // Preconditions: a flammable interior tile, inside, beyond burn()'s safe radius (3).
    const auto interior_far = center + tripoint_rel_ms(4, 0, 0);
    REQUIRE_FALSE(here.is_outside(abs_to_bub(interior_far)));
    REQUIRE((
        here.get_mapbuffer().has_flag("FLAMMABLE", interior_far)
        || here.get_mapbuffer().has_flag("FLAMMABLE_ASH", interior_far)));

    // bad_day passes the player's OMT to burn().
    const tripoint_abs_omt omtstart = project_to<coords::omt>(get_avatar().abs_pos());

    const start_location sl;
    sl.burn(omtstart, /*count=*/3, /*rad=*/3);

    int fires = 0;
    for (const auto& handle : simulated_tiles_in_rectangle(
             here.get_mapbuffer(), center + tripoint_rel_ms(-5, -5, 0),
             center + tripoint_rel_ms(5, 5, 0))) {
        if (here.get_mapbuffer().get_field_entry(handle.abs_pos(), fd_fire) != nullptr) { ++fires; }
    }
    INFO("fd_fire fields placed inside the building: " << fires);
    CHECK(fires > 0);
}
