#include "catch/catch.hpp"

#include <algorithm>
#include <memory>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include "avatar.h"
#include "cata_utility.h"
#include "coordinates.h"
#include "damage.h"
#include "enums.h"
#include "game.h"
#include "item.h"
#include "json.h"
#include "map.h"
#include "map_helpers.h"
#include "mongroup.h"
#include "monster.h"
#include "overmapbuffer.h"
#include "state_helpers.h"
#include "type_id.h"
#include "vehicle.h"
#include "vehicle_part.h"
#include "vehicle_wait.h"
#include "vpart_position.h"
#include "veh_type.h"

namespace
{

const auto horde_spawn_test_group = mongroup_id( "GROUP_ZOMBIE" );
const auto horde_spawn_test_monster = mtype_id( "mon_zombie" );

struct horde_vehicle_spawn_options {
    bool owned = false;
    bool tracked = false;
};

struct horde_vehicle_spawn_fixture {
    std::set<tripoint_abs_ms> vehicle_points;
    mongroup *horde = nullptr;
};

auto point_has_monster( const tripoint_abs_ms &p ) -> bool
{
    const auto &here = get_map();
    return g->critter_at<monster>( here.abs_to_bub( p ) ) != nullptr;
}

auto vehicle_points_contain_monster( const std::set<tripoint_abs_ms> &vehicle_points ) -> bool
{
    return std::ranges::any_of( vehicle_points, point_has_monster );
}

auto make_horde_vehicle_spawn_fixture( const horde_vehicle_spawn_options &options )
-> horde_vehicle_spawn_fixture
{
    clear_all_state();
    ACTIVE_OVERMAP_BUFFER.clear();

    auto &here = get_map();
    auto &you = get_avatar();
    const auto target_submap = tripoint_bub_sm( here.getmapsize() / 2, here.getmapsize() / 2, 0 );
    const auto target_submap_abs = here.bub_to_abs( target_submap );
    const auto target_submap_origin = project_to<coords::ms>( target_submap );
    const auto target_submap_end = target_submap_origin + tripoint( SEEX - 1, SEEY - 1, 0 );
    const auto vehicle_origin = target_submap_origin + tripoint( SEEX / 2, SEEY / 2, 0 );

    you.setpos( vehicle_origin + tripoint( 0, 0, -2 ) );
    const auto veh = here.add_vehicle( vproto_id( "car" ), vehicle_origin, 0_degrees, 0, 0 );
    REQUIRE( veh != nullptr );

    auto group = mongroup( horde_spawn_test_group, target_submap_abs, 1, 0 );
    group.horde = true;
    group.interest = 10;
    group.monsters.emplace_back( horde_spawn_test_monster );

    ACTIVE_OVERMAP_BUFFER.get( project_remain<coords::om>( target_submap_abs ).quotient );
    auto target_groups = ACTIVE_OVERMAP_BUFFER.groups_at( target_submap_abs );
    std::ranges::for_each( target_groups, []( mongroup * const target_group ) {
        target_group->clear();
    } );
    ACTIVE_OVERMAP_BUFFER.discard_monster_map( target_submap_abs );

    const auto horde = ACTIVE_OVERMAP_BUFFER.create_horde( group );
    REQUIRE( horde != nullptr );

    if( options.owned ) {
        veh->set_owner( you );
    }
    if( options.tracked ) {
        veh->toggle_tracking();
    }

    const auto vehicle_points = veh->get_points( true );
    const auto horde_spawn_blocking_terrain = ter_id( "t_wall" );
    std::ranges::for_each( here.points_in_rectangle( target_submap_origin, target_submap_end ),
    [&]( const auto & p ) {
        if( !vehicle_points.contains( here.bub_to_abs( p ) ) ) {
            here.ter_set( p, horde_spawn_blocking_terrain );
        }
    } );
    here.invalidate_map_cache( target_submap.z() );
    here.build_map_cache( target_submap.z(), true );

    return horde_vehicle_spawn_fixture{ .vehicle_points = vehicle_points, .horde = horde };
}

auto vehicle_with_legacy_pivot_json() -> std::string
{
    return R"json(
           {
           "type": "none",
           "posx": 5,
           "posy": 6,
           "om_id": 0,
           "faceDir": 180,
           "moveDir": 180,
           "turn_dir": 180,
           "velocity": 0,
           "falling": false,
           "floating": false,
           "flying": false,
           "cruise_velocity": 0,
           "vertical_velocity": 0,
           "name": "legacy pivot test vehicle",
           "owner": "",
           "old_owner": "",
           "parts": [
           {
           "id": "frame_horizontal",
           "mount_dx": 0,
           "mount_dy": 0,
           "open": false,
           "direction": 0,
           "blood": 0,
           "proxy_part_id": "null",
           "proxy_sym": 0,
           "enabled": false,
           "flags": 0,
           "passenger_id": -1,
           "crew_id": -1,
           "items": [],
           "ammo_pref": "null",
           "part_color": [ 0, 0, 0, 0 ]
       }
           ],
           "pivot": [ -1, 0 ],
           "zones": []
       }
           )json";
}

auto vehicle_with_invalid_part_and_legacy_pivot_json() -> std::string
{
    return R"json(
           {
           "type": "none",
           "posx": 5,
           "posy": 6,
           "om_id": 0,
           "faceDir": 180,
           "moveDir": 180,
           "turn_dir": 180,
           "velocity": 0,
           "falling": false,
           "floating": false,
           "flying": false,
           "cruise_velocity": 0,
           "vertical_velocity": 0,
           "name": "legacy pivot invalid part test vehicle",
           "owner": "",
           "old_owner": "",
           "parts": [
           {
           "id": "missing_saved_vehicle_part",
           "mount_dx": 99,
           "mount_dy": 99,
           "open": false,
           "direction": 0,
           "blood": 0,
           "proxy_part_id": "null",
           "proxy_sym": 0,
           "enabled": false,
           "flags": 0,
           "passenger_id": -1,
           "crew_id": -1,
           "items": [],
           "ammo_pref": "null",
           "part_color": [ 0, 0, 0, 0 ]
       },
           {
           "id": "frame_horizontal",
           "mount_dx": 0,
           "mount_dy": 0,
           "open": false,
           "direction": 0,
           "blood": 0,
           "proxy_part_id": "null",
           "proxy_sym": 0,
           "enabled": false,
           "flags": 0,
           "passenger_id": -1,
           "crew_id": -1,
           "items": [],
           "ammo_pref": "null",
           "part_color": [ 0, 0, 0, 0 ]
       }
           ],
           "pivot": [ -1, 0 ],
           "zones": []
       }
           )json";
}

} // namespace

TEST_CASE( "vehicle deserialize accepts legacy two coordinate pivot", "[vehicle][save]" )
{
    auto json = std::istringstream( vehicle_with_legacy_pivot_json() );
    auto jsin = JsonIn( json );
    auto veh = vehicle();

    REQUIRE( jsin.read( veh, true ) );
    CHECK( veh.mount_to_abs( tripoint_mnt_veh( -1, 0, 0 ) ) == tripoint_abs_ms( 5, 6, 0 ) );
    CHECK( veh.mount_to_abs( tripoint_mnt_veh( 0, 0, 0 ) ) == tripoint_abs_ms( 4, 6, 0 ) );
}

TEST_CASE( "vehicle deserialize keeps valid saved parts after an invalid part", "[vehicle][save]" )
{
    auto json = std::istringstream( vehicle_with_invalid_part_and_legacy_pivot_json() );
    auto jsin = JsonIn( json );
    auto veh = vehicle();

    REQUIRE( jsin.read( veh, true ) );
    CHECK( veh.part_count() == 1 );
    CHECK( veh.mount_to_abs( tripoint_mnt_veh( 0, 0, 0 ) ) == tripoint_abs_ms( 4, 6, 0 ) );
}

TEST_CASE( "detaching_vehicle_unboards_passengers" )
{
    clear_all_state();
    const tripoint_bub_ms test_origin( 60, 60, 0 );
    const auto vehicle_origin = test_origin;
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
        const tripoint_bub_ms test_origin( 60, 60, 0 );
        avatar &player_character = get_avatar();
        player_character.setpos( test_origin );
        const auto vehicle_origin = test_origin + tripoint_south_east;
        vehicle *veh_ptr = here.add_vehicle( vproto_id( "bicycle" ), vehicle_origin, -90_degrees, 0, 0 );
        REQUIRE( veh_ptr != nullptr );
        tripoint_bub_ms grab_point = test_origin + tripoint_rel_ms::east();
        player_character.grab( OBJECT_VEHICLE, tripoint_rel_ms::east() );
        REQUIRE( player_character.get_grab_type() != OBJECT_NONE );
        REQUIRE( player_character.grab_point == tripoint_rel_ms::east() );
        WHEN( "The vehicle section grabbed by the player is destroyed" ) {
            here.destroy( grab_point );
            REQUIRE( veh_ptr->get_parts_at( grab_point, "", part_status_flag::available ).empty() );
            THEN( "The player's grab is released" ) {
                CHECK( player_character.get_grab_type() == OBJECT_NONE );
                CHECK( player_character.grab_point == tripoint_rel_ms::zero() );
            }
        }
    }
}

TEST_CASE( "taking_control_of_vehicle_without_engine", "[vehicle]" )
{
    clear_all_state();
    const auto origin = tripoint_bub_ms( 60, 60, 0 );
    auto &player_character = get_avatar();
    player_character.setpos( origin );

    auto *veh_ptr = get_map().add_vehicle( vproto_id( "shopping_cart" ), origin, 0_degrees, 0, 0 );
    REQUIRE( veh_ptr != nullptr );
    REQUIRE_FALSE( player_character.controlling_vehicle );
    REQUIRE_FALSE( veh_ptr->engine_on );

    veh_ptr->start_engines( true );

    CHECK( player_character.controlling_vehicle );
    CHECK_FALSE( veh_ptr->engine_on );
    CHECK( !player_character.activity );
}

TEST_CASE( "moving_flying_vehicle_can_use_wait_menu", "[vehicle][wait]" )
{
    clear_all_state();
    const auto origin = tripoint_bub_ms( 60, 60, 0 );

    auto *veh_ptr = get_map().add_vehicle( vproto_id( "plane_small" ), origin, 0_degrees, 0, 0 );
    REQUIRE( veh_ptr != nullptr );

    veh_ptr->velocity = 100;
    CHECK( vehicle_wait::is_wait_blocked_by_movement( *veh_ptr ) );
    CHECK_FALSE( vehicle_wait::should_offer_flying_wait_durations( *veh_ptr ) );

    veh_ptr->set_flying( true );
    CHECK_FALSE( vehicle_wait::is_wait_blocked_by_movement( *veh_ptr ) );
    CHECK( vehicle_wait::should_offer_flying_wait_durations( *veh_ptr ) );
}

TEST_CASE( "horde_spawns_skip_owned_vehicle_tiles", "[horde][vehicle][monster]" )
{
    const auto cleanup = on_out_of_scope( [] {
        clear_all_state();
        ACTIVE_OVERMAP_BUFFER.clear();
    } );

    SECTION( "unowned and untracked vehicle tiles remain valid horde spawn locations" ) {
        const auto fixture = make_horde_vehicle_spawn_fixture( horde_vehicle_spawn_options{} );

        get_map().spawn_monsters( true );

        CHECK( vehicle_points_contain_monster( fixture.vehicle_points ) );
        CHECK( fixture.horde->empty() );
    }

    SECTION( "tracked but unowned vehicle tiles remain valid horde spawn locations" ) {
        const auto fixture = make_horde_vehicle_spawn_fixture( horde_vehicle_spawn_options{ .tracked = true } );

        get_map().spawn_monsters( true );

        CHECK( vehicle_points_contain_monster( fixture.vehicle_points ) );
        CHECK( fixture.horde->empty() );
    }

    SECTION( "owned but untracked vehicle tiles are excluded from horde spawn locations" ) {
        const auto fixture = make_horde_vehicle_spawn_fixture( horde_vehicle_spawn_options{ .owned = true } );

        get_map().spawn_monsters( true );

        CHECK_FALSE( vehicle_points_contain_monster( fixture.vehicle_points ) );
        CHECK_FALSE( fixture.horde->empty() );
    }

    SECTION( "owned and tracked vehicle tiles are excluded from horde spawn locations" ) {
        const auto fixture = make_horde_vehicle_spawn_fixture( horde_vehicle_spawn_options{ .owned = true,
                             .tracked = true } );

        get_map().spawn_monsters( true );

        CHECK_FALSE( vehicle_points_contain_monster( fixture.vehicle_points ) );
        CHECK_FALSE( fixture.horde->empty() );
    }
}

TEST_CASE( "add_item_to_broken_vehicle_part" )
{
    clear_all_state();
    const tripoint_bub_ms test_origin( 60, 60, 0 );
    const auto vehicle_origin = test_origin;
    vehicle *veh_ptr = get_map().add_vehicle( vproto_id( "bicycle" ), vehicle_origin, 0_degrees, 0, 0 );
    REQUIRE( veh_ptr != nullptr );

    const tripoint_bub_ms pos = vehicle_origin + tripoint_rel_ms::west();
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
    const tripoint_bub_ms test_origin( 60, 60, 0 );
    g->place_player( test_origin );
    const tripoint_bub_ms vehicle_origin( SEEX, 0, 0 );
    vehicle *veh_ptr = get_map().add_vehicle( vproto_id( "bicycle" ), vehicle_origin, 0_degrees, 0, 0 );
    REQUIRE( veh_ptr != nullptr );

    //Put an item in the vehicle
    const tripoint_bub_ms cargo_pos = vehicle_origin + tripoint_rel_ms::west();
    auto cargo_parts = veh_ptr->get_parts_at( cargo_pos, "CARGO", part_status_flag::any );
    REQUIRE( !cargo_parts.empty( ) );
    vehicle_part *cargo_part = cargo_parts.front();
    REQUIRE( cargo_part != nullptr );
    REQUIRE( !veh_ptr->add_item( *cargo_part, item::spawn( "jeans" ) ) );

    //Shift the vehicle half off the map
    g->place_player( test_origin + tripoint_east * SEEX );

    //Check the vehicle is still there.
    optional_vpart_position part_pos = get_map().veh_at( tripoint_bub_ms::zero() );
    REQUIRE( part_pos );

    // TODO: vehicle is at origin so tripoint_west == bubble pos; use parts_at_relative( point(-1,0), true ) directly
    auto parts = veh_ptr->parts_at_relative( veh_ptr->bubble_to_mount( tripoint_bub_ms(
                     tripoint_west ) ), true );
    REQUIRE( !parts.empty( ) );
    for( int part : parts ) {
        //We aren't actually smashing each chosen part in turn here
        //it's picking a random one each time, hence why we smash them all
        veh_ptr->damage( part, 10000 );
    }
}

static void check_wreckage( int zlevel )
{
    const tripoint_bub_ms test_origin( 60, 60, zlevel );
    const auto vehicle_origin = test_origin;

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

static void test_coord_translate( units::angle dir, const tripoint_mnt_veh &pivot,
                                  const tripoint_mnt_veh &p,
                                  tripoint_rel_ms &q )
{
    tileray tdir( dir );
    tdir.advance( p.x() - pivot.x() );
    q.x() = tdir.dx() + tdir.ortho_dx( p.y() - pivot.y() );
    q.y() = tdir.dy() + tdir.ortho_dy( p.y() - pivot.y() );
}

TEST_CASE( "check_vehicle_rotation_against_old", "[.]" )
{
    clear_all_state();
    const tripoint_bub_ms test_origin( 60, 60, 0 );
    const auto vehicle_origin = test_origin;
    vehicle *veh_ptr = get_map().add_vehicle( vproto_id( "bicycle" ), vehicle_origin, 0_degrees, 0, 0 );
    const tripoint_mnt_veh pivot;

    for( int dir = 0; dir < 24; dir++ ) {
        for( int x = -5; x <= 5; x++ ) {
            for( int y = -5; y <= 5; y++ ) {
                tripoint_mnt_veh p = {x, y, 0};
                point_rel_ms oldRes;
                veh_ptr->coord_translate( 15_degrees * dir, pivot, p, oldRes );

                tripoint_rel_ms newRes;
                test_coord_translate( 15_degrees * dir, pivot, p, newRes );

                CHECK( oldRes.x() == newRes.x() );
                CHECK( oldRes.y() == newRes.y() );

            }
        }
    }
}

TEST_CASE( "vehicle_rotation_reverse" )
{
    clear_all_state();
    const tripoint_bub_ms test_origin( 60, 60, 0 );
    const auto vehicle_origin = test_origin;
    vehicle *veh_ptr = get_map().add_vehicle( vproto_id( "bicycle" ), vehicle_origin, 0_degrees, 0, 0 );
    const tripoint_mnt_veh pivot;

    for( int dir = 0; dir < 24; dir++ ) {
        for( int x = -5; x <= 5; x++ ) {
            for( int y = -5; y <= 5; y++ ) {
                tripoint_mnt_veh p = {x, y, 0};
                point_rel_ms result;
                veh_ptr->coord_translate( 15_degrees * dir, pivot, p, result );

                tripoint_mnt_veh reversed;
                veh_ptr->coord_translate_reverse( 15_degrees * dir, pivot, tripoint_rel_ms( result, 0 ), reversed );

                CHECK( reversed.x() == p.x() );
                CHECK( reversed.y() == p.y() );

            }
        }
    }
}

TEST_CASE( "broken_door_and_lock_can_be_removed", "[vehicle]" )
{
    clear_all_state();
    const auto origin = tripoint_bub_ms( 60, 60, 0 );
    auto *veh_ptr = get_map().add_vehicle( vproto_id( "cross_split_test" ), origin, 0_degrees, 0, 0 );
    REQUIRE( veh_ptr != nullptr );

    const auto door_mount = tripoint_mnt_veh( 1, 0, 0 );
    const auto door_idx = veh_ptr->part_with_feature( door_mount, "OPENABLE", true );
    const auto lock_idx = veh_ptr->part_with_feature( door_mount, "DOOR_LOCKING", true );
    REQUIRE( door_idx >= 0 );
    REQUIRE( lock_idx >= 0 );

    auto &door_part = veh_ptr->part( door_idx );
    auto &lock_part = veh_ptr->part( lock_idx );
    // DOORS CAN SPAWN OPEN GUYS
    if( door_part.open ) {
        door_part.open = false;
    }
    REQUIRE_FALSE( door_part.open );

    REQUIRE( veh_ptr->mod_hp( door_part, -( door_part.hp() + 1 ), DT_BASH ) );
    REQUIRE( veh_ptr->mod_hp( lock_part, -( lock_part.hp() + 1 ), DT_BASH ) );
    REQUIRE( door_part.is_broken() );
    REQUIRE( lock_part.is_broken() );

    auto door_reason = std::string{};
    auto lock_reason = std::string{};
    CHECK( veh_ptr->can_unmount( door_idx, door_reason ) );
    CHECK( veh_ptr->can_unmount( lock_idx, lock_reason ) );
}
