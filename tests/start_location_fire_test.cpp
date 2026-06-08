#include "catch/catch.hpp"

#include "avatar.h"
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
// the player. On current main burn() builds a single-z `tinymap` and queries
// is_outside(); a zlevels=false map cannot model the z+1 roof level, so its
// outside_cache is filled all-true and is_outside() returns true for EVERY tile.
// burn()'s interior filter then rejects all candidates and places zero fires.
//
// This test paints a roofed (=> "inside") flammable building on the live map around
// the avatar and asserts that burn() actually places fd_fire on interior tiles.
// It FAILS on the buggy single-z-tinymap implementation and PASSES once burn()
// operates on the live multi-z map (g->m), whose outside cache is correct.

TEST_CASE( "start_location_burn_places_fire_on_interior_flammable_tiles",
           "[start_location][field][fire]" )
{
    clear_all_state();
    map &here = get_map();
    REQUIRE( here.has_zlevels() );

    const ter_str_id floor_primitive( "t_floor_primitive" ); // interior floor, FLAMMABLE_ASH
    const ter_str_id floor_roof( "t_floor" );                // used as a roof on z+1

    // Build an 11x11 flammable interior at z=0 with a roof one tile larger at z=1,
    // so the 3x3-above check marks every interior tile as "inside".
    for( int x = 54; x <= 66; ++x ) {
        for( int y = 54; y <= 66; ++y ) {
            here.ter_set( tripoint_bub_ms( x, y, 1 ), floor_roof );
            if( x >= 55 && x <= 65 && y >= 55 && y <= 65 ) {
                here.ter_set( tripoint_bub_ms( x, y, 0 ), floor_primitive );
            }
        }
    }
    here.invalidate_map_cache( 0 );
    here.build_map_cache( 0, true );

    // Place the avatar inside the building.
    const tripoint_bub_ms center( 60, 60, 0 );
    get_avatar().setpos( center );

    // Preconditions: a flammable interior tile, inside, beyond burn()'s safe radius (3).
    const tripoint_bub_ms interior_far( 64, 60, 0 );
    REQUIRE_FALSE( here.is_outside( interior_far ) );
    REQUIRE( ( here.has_flag( "FLAMMABLE", interior_far ) ||
               here.has_flag( "FLAMMABLE_ASH", interior_far ) ) );

    // bad_day passes the player's OMT to burn().
    const tripoint_abs_omt omtstart =
        project_to<coords::omt>( here.bub_to_abs( get_avatar().bub_pos() ) );

    const start_location sl;
    sl.burn( omtstart, /*count=*/3, /*rad=*/3 );

    int fires = 0;
    for( const tripoint_bub_ms &p : here.points_in_rectangle(
             tripoint_bub_ms( 55, 55, 0 ), tripoint_bub_ms( 65, 65, 0 ) ) ) {
        if( here.get_field( p, fd_fire ) != nullptr ) {
            ++fires;
        }
    }
    INFO( "fd_fire fields placed inside the building: " << fires );
    CHECK( fires > 0 );
}
