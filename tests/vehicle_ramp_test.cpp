#include "catch/catch.hpp"

#include <cstdio>
#include <cstdlib>
#include <sstream>
#include <algorithm>
#include <cmath>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "avatar.h"
#include "itype.h"
#include "map.h"
#include "map_helpers.h"
#include "map_iterator.h"
#include "veh_type.h"
#include "vehicle.h"
#include "vehicle_part.h"
#include "vpart_range.h"
#include "bodypart.h"
#include "calendar.h"
#include "enums.h"
#include "game.h"
#include "game_constants.h"
#include "item.h"
#include "line.h"
#include "mapdata.h"
#include "monster.h"
#include "mtype.h"
#include "units.h"
#include "type_id.h"
#include "point.h"
#include "vpart_position.h"
#include "player_helpers.h"
#include "state_helpers.h"

static void set_ramp( const int transit_x, bool use_ramp, bool up )
{
    // Set to turn 0 to prevent solars from producing power
    calendar::turn = calendar::turn_zero;
    Character &player_character = get_player_character();
    // Move player somewhere safe
    REQUIRE_FALSE( player_character.in_vehicle );

    map &here = get_map();
    build_test_map( ter_id( "t_pavement" ) );
    if( use_ramp ) {
        const int upper_zlevel = up ? 1 : 0;
        const int lower_zlevel = up - 1;
        const int highx = transit_x + ( up ? 0 : 1 );
        const int lowx = transit_x + ( up ? 1 : 0 );

        // up   z1    ......  rdh  rDl
        //      z0            rUh  rul .................
        // down z0            rDl  rdh .................
        //      z-1   ......  rdl  rUh
        //                    60   61
        for( int y = 0; y < SEEY * MAPSIZE; y++ ) {
            for( int x = 0; x < transit_x; x++ ) {
                const int mid = up ? upper_zlevel : lower_zlevel;
                here.ter_set( tripoint( x, y, mid - 2 ), ter_id( "t_rock" ) );
                here.ter_set( tripoint( x, y, mid - 1 ), ter_id( "t_rock" ) );
                here.ter_set( tripoint( x, y, mid ), ter_id( "t_pavement" ) );
                here.ter_set( tripoint( x, y, mid + 1 ), ter_id( "t_open_air" ) );
                here.ter_set( tripoint( x, y, mid + 2 ), ter_id( "t_open_air" ) );
            }
            const tripoint ramp_up_low = tripoint( lowx, y, lower_zlevel );
            const tripoint ramp_up_high = tripoint( highx, y, lower_zlevel );
            const tripoint ramp_down_low = tripoint( lowx, y, upper_zlevel );
            const tripoint ramp_down_high = tripoint( highx, y, upper_zlevel );
            here.ter_set( ramp_up_low, ter_id( "t_ramp_up_low" ) );
            here.ter_set( ramp_up_high, ter_id( "t_ramp_up_high" ) );
            here.ter_set( ramp_down_low, ter_id( "t_ramp_down_low" ) );
            here.ter_set( ramp_down_high, ter_id( "t_ramp_down_high" ) );
            for( int x = transit_x + 2; x < SEEX * MAPSIZE; x++ ) {
                here.ter_set( tripoint( x, y, 1 ), ter_id( "t_open_air" ) );
                here.ter_set( tripoint( x, y, 0 ), ter_id( "t_pavement" ) );
                here.ter_set( tripoint( x, y, -1 ), ter_id( "t_rock" ) );
            }
        }
    }
    here.invalidate_map_cache( 0 );
    here.build_map_cache( 0, true );
}

// Algorithm goes as follows:
// Clear map and create a ramp
// Spawn a vehicle
// Drive it over the ramp, and confirm that the vehicle changes z-levels
static void ramp_transition_angled( const vproto_id &veh_id, const units::angle angle,
                                    const int transition_x, bool use_ramp, bool up )
{
    map &here = get_map();
    set_ramp( transition_x, use_ramp, up );

    const tripoint map_starting_point( transition_x + 4, 60, 0 );
    REQUIRE( here.ter( map_starting_point ) == ter_id( "t_pavement" ) );
    if( here.ter( map_starting_point ) != ter_id( "t_pavement" ) ) {
        return;
    }
    vehicle *veh_ptr = here.add_vehicle( veh_id, map_starting_point, angle, 1, 0 );

    REQUIRE( veh_ptr != nullptr );
    if( veh_ptr == nullptr ) {
        return;
    }

    vehicle &veh = *veh_ptr;
    veh.check_falling_or_floating();

    REQUIRE( !veh.is_in_water() );

    veh.tags.insert( "IN_CONTROL_OVERRIDE" );
    veh.engine_on = true;
    Character &player_character = get_player_character();
    player_character.setpos( map_starting_point );

    REQUIRE( player_character.pos() == map_starting_point );
    if( player_character.pos() != map_starting_point ) {
        return;
    }
    get_map().board_vehicle( map_starting_point, player_character.as_player() );
    REQUIRE( player_character.pos() == map_starting_point );
    if( player_character.pos() != map_starting_point ) {
        return;
    }
    const int transition_cycle = 3;
    veh.cruise_velocity = 0;
    veh.velocity = 0;
    here.vehmove();

    const int target_velocity = 179;
    veh.cruise_velocity = target_velocity;
    veh.velocity = target_velocity;
    CHECK( veh.safe_velocity() > 0 );
    int cycles = 0;
    const int target_z = use_ramp ? ( up ? 1 : -1 ) : 0;

    std::set<tripoint> vpts = veh.get_points();
    while( veh.engine_on && veh.safe_velocity() > 0 && cycles < 10 ) {
        CAPTURE( cycles );
        for( const tripoint &checkpt : vpts ) {
            int partnum = 0;
            vehicle *check_veh = here.veh_at_internal( checkpt, partnum );
            CHECK( check_veh == veh_ptr );
        }
        vpts.clear();
        here.vehmove();
        CHECK( veh.velocity == target_velocity );
        // If the vehicle starts skidding, the effects become random and test is RUINED
        REQUIRE( !veh.skidding );
        for( const tripoint &pos : veh.get_points() ) {
            REQUIRE( here.ter( pos ) );
        }
        for( const vpart_reference &vp : veh.get_all_parts() ) {
            if( vp.info().location != "structure" ) {
                continue;
            }
            const point &pmount = vp.mount();
            CAPTURE( pmount );
            const tripoint &ppos = vp.pos();
            CAPTURE( ppos );
            if( cycles > ( transition_cycle - pmount.x ) ) {
                CHECK( ppos.z == target_z );
            }
            if( pmount.x == 0 && pmount.y == 0 ) {
                CHECK( player_character.pos() == ppos );
            }
        }
        vpts = veh.get_points();
        cycles++;
    }

    const int expected_move = use_ramp ? ( up ? 1 : -1 ) : 0;
    CHECK( veh.global_pos3().z - map_starting_point.z == expected_move );

    const std::optional<vpart_reference> vp = here.veh_at( player_character.pos() ).part_with_feature(
                VPFLAG_BOARDABLE, true );
    REQUIRE( vp );
    if( vp ) {
        const int z_change = map_starting_point.z - player_character.pos().z;
        here.unboard_vehicle( *vp, &player_character, false );
        here.ter_set( map_starting_point, ter_id( "t_pavement" ) );
        player_character.setpos( map_starting_point );
        if( z_change ) {
            g->vertical_move( z_change, true );
        }
    }
}

static void test_ramp( std::string type, const int transition_x )
{
    CAPTURE( type );
    SECTION( "no ramp" ) {
        ramp_transition_angled( vproto_id( type ), 180_degrees, transition_x, false, false );
    }
    SECTION( "ramp up" ) {
        ramp_transition_angled( vproto_id( type ), 180_degrees, transition_x, true, true );
    }
    SECTION( "ramp down" ) {
        ramp_transition_angled( vproto_id( type ), 180_degrees, transition_x, true, false );
    }
    SECTION( "angled no ramp" ) {
        ramp_transition_angled( vproto_id( type ), 225_degrees, transition_x, false, false );
    }
    SECTION( "angled ramp down" ) {
        ramp_transition_angled( vproto_id( type ), 225_degrees, transition_x, true, false );
    }
    SECTION( "angled ramp up" ) {
        ramp_transition_angled( vproto_id( type ), 225_degrees, transition_x, true, true );
    }
}

static std::vector<std::string> ramp_vehs_to_test = {{
        "motorcycle",
    }
};

// I'd like to do this in a single loop, but that doesn't work for some reason
TEST_CASE( "vehicle_ramp_test_59", "[vehicle][ramp]" )
{
    clear_all_state();
    for( const std::string &veh : ramp_vehs_to_test ) {
        test_ramp( veh, 59 );
    }
}
TEST_CASE( "vehicle_ramp_test_60", "[vehicle][ramp]" )
{
    clear_all_state();
    for( const std::string &veh : ramp_vehs_to_test ) {
        test_ramp( veh, 60 );
    }
}
TEST_CASE( "vehicle_ramp_test_61", "[vehicle][ramp]" )
{
    clear_all_state();
    for( const std::string &veh : ramp_vehs_to_test ) {
        test_ramp( veh, 61 );
    }
}
