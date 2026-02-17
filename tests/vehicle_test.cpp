#include "catch/catch.hpp"

#include <memory>
#include <optional>
#include <vector>

#include "avatar.h"
#include "damage.h"
#include "enums.h"
#include "game.h"
#include "item.h"
#include "map.h"
#include "map_helpers.h"
#include "point.h"
#include "state_helpers.h"
#include "type_id.h"
#include "vehicle.h"
#include "vehicle_part.h"
#include "vpart_position.h"
#include "veh_type.h"

TEST_CASE( "detaching_vehicle_unboards_passengers" )
{
    clear_all_state();
    const tripoint test_origin( 60, 60, 0 );
    const tripoint vehicle_origin = test_origin;
    avatar &player_character = get_avatar();
    map &here = get_map();
    vehicle *veh_ptr = here.add_vehicle( vproto_id( "bicycle" ), vehicle_origin, -90_degrees, 0, 0 );
    here.board_vehicle( test_origin, &player_character );
    REQUIRE( player_character.in_vehicle );
    here.detach_vehicle( veh_ptr );
    REQUIRE( !player_character.in_vehicle );
}

TEST_CASE( "destroy_grabbed_vehicle_section" )
{
    clear_all_state();
    GIVEN( "A vehicle grabbed by the player" ) {
        map &here = get_map();
        const tripoint test_origin( 60, 60, 0 );
        avatar &player_character = get_avatar();
        player_character.setpos( test_origin );
        const tripoint vehicle_origin = test_origin + tripoint_south_east;
        vehicle *veh_ptr = here.add_vehicle( vproto_id( "bicycle" ), vehicle_origin, -90_degrees, 0, 0 );
        REQUIRE( veh_ptr != nullptr );
        tripoint grab_point = test_origin + tripoint_east;
        player_character.grab( OBJECT_VEHICLE, grab_point );
        REQUIRE( player_character.get_grab_type() != OBJECT_NONE );
        REQUIRE( player_character.grab_point == grab_point );
        WHEN( "The vehicle section grabbed by the player is destroyed" ) {
            here.destroy( grab_point );
            REQUIRE( veh_ptr->get_parts_at( grab_point, "", part_status_flag::available ).empty() );
            THEN( "The player's grab is released" ) {
                CHECK( player_character.get_grab_type() == OBJECT_NONE );
                CHECK( player_character.grab_point == tripoint_zero );
            }
        }
    }
}

TEST_CASE( "add_item_to_broken_vehicle_part" )
{
    clear_all_state();
    const tripoint test_origin( 60, 60, 0 );
    const tripoint vehicle_origin = test_origin;
    vehicle *veh_ptr = get_map().add_vehicle( vproto_id( "bicycle" ), vehicle_origin, 0_degrees, 0, 0 );
    REQUIRE( veh_ptr != nullptr );

    const tripoint pos = vehicle_origin + tripoint_west;
    auto cargo_parts = veh_ptr->get_parts_at( pos, "CARGO", part_status_flag::any );
    REQUIRE( !cargo_parts.empty( ) );
    vehicle_part *cargo_part = cargo_parts.front();
    REQUIRE( cargo_part != nullptr );
    //Must not be broken yet
    REQUIRE( !cargo_part->is_broken() );
    //For some reason (0 - cargo_part->hp()) is just not enough to destroy a part
    REQUIRE( veh_ptr->mod_hp( *cargo_part, -( 1 + cargo_part->hp() ), DT_BASH ) );
    //Now it must be broken
    REQUIRE( cargo_part->is_broken() );
    //Now part is really broken, adding an item should fail
    detached_ptr<item> itm2 = item::spawn( "jeans" );
    itm2 = veh_ptr->add_item( *cargo_part, std::move( itm2 ) );
    CHECK( itm2 );
}

TEST_CASE( "damage_vehicle_oob" )
{
    clear_all_state();
    const tripoint test_origin( 60, 60, 0 );
    g->place_player( test_origin );
    const tripoint vehicle_origin( SEEX, 0, 0 );
    vehicle *veh_ptr = get_map().add_vehicle( vproto_id( "bicycle" ), vehicle_origin, 0_degrees, 0, 0 );
    REQUIRE( veh_ptr != nullptr );

    //Put an item in the vehicle
    const tripoint cargo_pos = vehicle_origin + tripoint_west;
    auto cargo_parts = veh_ptr->get_parts_at( cargo_pos, "CARGO", part_status_flag::any );
    REQUIRE( !cargo_parts.empty( ) );
    vehicle_part *cargo_part = cargo_parts.front();
    REQUIRE( cargo_part != nullptr );
    REQUIRE( !veh_ptr->add_item( *cargo_part, item::spawn( "jeans" ) ) );

    //Shift the vehicle half off the map
    g->place_player( test_origin + tripoint_east * SEEX );

    //Check the vehicle is still there.
    optional_vpart_position part_pos = get_map().veh_at( tripoint_zero );
    REQUIRE( part_pos );

    auto parts = veh_ptr->parts_at_relative( veh_ptr->tripoint_to_mount( tripoint_west ), true );
    REQUIRE( !parts.empty( ) );
    for( int part : parts ) {
        //We aren't actually smashing each chosen part in turn here
        //it's picking a random one each time, hence why we smash them all
        veh_ptr->damage( part, 10000 );
    }
}

static void check_wreckage( int zlevel )
{
    const tripoint test_origin( 60, 60, zlevel );
    const tripoint vehicle_origin = test_origin;

    vehicle *veh_ptr = get_map().add_vehicle( vproto_id( "bicycle" ), vehicle_origin, 0_degrees, 0, 0 );
    REQUIRE( veh_ptr != nullptr );

    vehicle *veh_ptr2 = get_map().add_vehicle( vproto_id( "car" ), vehicle_origin + tripoint_north_west,
                        0_degrees, 0, 0 );
    REQUIRE( veh_ptr2 != nullptr );

    INFO( veh_ptr2->name );
    CHECK( veh_ptr2->name == "Wreckage" );
}

TEST_CASE( "overlapping_vehicles_make_wreck" )
{
    clear_all_state();
    check_wreckage( 0 );
    check_wreckage( OVERMAP_HEIGHT );
    check_wreckage( -OVERMAP_DEPTH );
}

static void test_coord_translate( units::angle dir, const point &pivot, const point &p,
                                  tripoint &q )
{
    tileray tdir( dir );
    tdir.advance( p.x - pivot.x );
    q.x = tdir.dx() + tdir.ortho_dx( p.y - pivot.y );
    q.y = tdir.dy() + tdir.ortho_dy( p.y - pivot.y );
}

TEST_CASE( "check_vehicle_rotation_against_old", "[.]" )
{
    clear_all_state();
    const tripoint test_origin( 60, 60, 0 );
    const tripoint vehicle_origin = test_origin;
    vehicle *veh_ptr = get_map().add_vehicle( vproto_id( "bicycle" ), vehicle_origin, 0_degrees, 0, 0 );
    const point pivot;

    for( int dir = 0; dir < 24; dir++ ) {
        for( int x = -5; x <= 5; x++ ) {
            for( int y = -5; y <= 5; y++ ) {
                point p = {x, y};
                tripoint oldRes;
                veh_ptr->coord_translate( 15_degrees * dir, pivot, p, oldRes );

                tripoint newRes;
                test_coord_translate( 15_degrees * dir, pivot, p, newRes );

                CHECK( oldRes.x == newRes.x );
                CHECK( oldRes.y == newRes.y );

            }
        }
    }
}

TEST_CASE( "vehicle_rotation_reverse" )
{
    clear_all_state();
    const tripoint test_origin( 60, 60, 0 );
    const tripoint vehicle_origin = test_origin;
    vehicle *veh_ptr = get_map().add_vehicle( vproto_id( "bicycle" ), vehicle_origin, 0_degrees, 0, 0 );
    const point pivot;

    for( int dir = 0; dir < 24; dir++ ) {
        for( int x = -5; x <= 5; x++ ) {
            for( int y = -5; y <= 5; y++ ) {
                point p = {x, y};
                tripoint result;
                veh_ptr->coord_translate( 15_degrees * dir, pivot, p, result );

                point reversed;
                veh_ptr->coord_translate_reverse( 15_degrees * dir, pivot, result, reversed );

                CHECK( reversed.x == p.x );
                CHECK( reversed.y == p.y );

            }
        }
    }
}
