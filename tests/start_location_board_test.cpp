#include "catch/catch.hpp"

#include "coordinates.h"
#include "game.h"
#include "map.h"
#include "map_helpers.h"
#include "point.h"
#include "start_location.h"
#include "state_helpers.h"
#include "type_id.h"

// Regression test for the BOARDED start-location bug (sibling of #9169).
//
// sloc_house_boarded (offered by 7 scenarios) carries the BOARDED flag, so
// start_location::prepare_map() boards up the building via a throwaway tinymap.
// On the buggy SINGLE-z tinymap, build_outside_cache() has no z+1 roof, so
// is_outside() is true for every tile; board_up() then boards INTERNAL doors
// shut (t_door_boarded) instead of opening them (t_door_o). The fix makes the
// throwaway tinymap MULTI-z ( tinymap player_start( 2, true ) ) so is_outside()
// is correct and interior doors are opened.
//
// The reality bubble's submaps are shared with the throwaway tinymap via
// MAPBUFFER, so painting the building on get_map() and then driving the public
// prepare_map(omt) path exercises the real boarding code against our building.
//
// A window is painted as a PLUMBING DISCRIMINATOR: board_up boards windows
// UNCONDITIONALLY of is_outside (start_location.cpp:140-143), so window-not-boarded
// means board_up never ran on these submaps (infrastructure problem, not the bug),
// window+door boarded is clean RED, and window-boarded + door-open is clean GREEN.
// FAILS on the single-z tinymap (door -> t_door_boarded); PASSES after the fix.

TEST_CASE( "boarded_start_opens_interior_doors", "[start_location][boarded]" )
{
    clear_all_state();
    map &here = get_map();
    REQUIRE( here.has_zlevels() );
    // Fail clearly (not via a confusing crash) if start_locations aren't in the test DB.
    REQUIRE( start_location_id( "sloc_house_boarded" ).is_valid() );

    const ter_str_id floor( "t_floor" );          // interior floor; also used as a z+1 roof
    const ter_str_id door_closed( "t_door_c" );
    const ter_str_id door_open( "t_door_o" );
    const ter_str_id door_boarded( "t_door_boarded" );
    const ter_str_id window( "t_window" );
    const ter_str_id window_boarded( "t_window_boarded" );

    // Paint a generously sized roofed interior around map center, so the door and
    // all four of its orthogonal neighbours are interior-with-roof and fall inside
    // a single OMT (board_up only processes the player's starting OMT, so keep the
    // painted area generous to survive OMT-boundary alignment).
    for( int x = 50; x <= 70; ++x ) {
        for( int y = 50; y <= 70; ++y ) {
            here.ter_set( tripoint_bub_ms( x, y, 1 ), floor );   // roof above => "inside"
            here.ter_set( tripoint_bub_ms( x, y, 0 ), floor );   // interior floor
        }
    }

    // A CLOSED interior door fully surrounded by interior floor (no outside neighbour),
    // and a window two tiles away as the unconditional-boarding plumbing probe.
    const tripoint_bub_ms interior_door( 60, 60, 0 );
    const tripoint_bub_ms window_pos( 62, 60, 0 );
    here.ter_set( interior_door, door_closed );
    here.ter_set( window_pos, window );

    here.invalidate_map_cache( 0 );
    here.build_map_cache( 0, true );

    // Preconditions: the door is closed and reads as INSIDE (and so do its neighbours).
    REQUIRE( here.ter( interior_door ) == door_closed.id() );
    REQUIRE_FALSE( here.is_outside( interior_door ) );
    REQUIRE_FALSE( here.is_outside( interior_door + tripoint_north ) );

    // Drive boarding through the public OMT entry of the real BOARDED start location.
    const start_location &sl = start_location_id( "sloc_house_boarded" ).obj();
    const tripoint_abs_omt omtstart =
        project_to<coords::omt>( here.bub_to_abs( interior_door ) );
    sl.prepare_map( omtstart );

    // Plumbing discriminator FIRST: if the window did not board, board_up never ran
    // on these submaps -- the failure is infrastructure, not the is_outside bug.
    REQUIRE( here.ter( window_pos ) == window_boarded.id() );

    // The fix: with a correct is_outside the interior door is OPENED, not boarded shut.
    INFO( "interior door terrain after boarding: " << here.ter( interior_door ).id().str() );
    CHECK( here.ter( interior_door ) == door_open.id() );
    CHECK( here.ter( interior_door ) != door_boarded.id() );
}
