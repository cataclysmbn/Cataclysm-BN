#include "catch/catch.hpp"

#include <cmath>
#include <ranges>

#include "avatar.h"
#include "gun_mode.h"
#include "item.h"
#include "map.h"
#include "options_helpers.h"
#include "ranged.h"
#include "rng.h"
#include "state_helpers.h"
#include "type_id.h"
#include "units_utility.h"
#include "veh_type.h"
#include "vehicle.h"
#include "vehicle_part.h"

TEST_CASE( "firing_from_a_vehicle_applies_recoil_to_the_vehicle", "[vehicle][gun]" )
{
    clear_all_state();
    rng_set_engine_seed( 0 );

    auto &here = get_map();
    auto &player_character = get_avatar();
    const auto vehicle_origin = tripoint( 60, 60, 0 );

    auto *const veh = here.add_vehicle( vproto_id( "bicycle" ), vehicle_origin, 0_degrees, 0, 0 );
    REQUIRE( veh != nullptr );

    player_character.setpos( vehicle_origin );
    here.board_vehicle( vehicle_origin, &player_character );
    REQUIRE( player_character.in_vehicle );

    auto gun = item::spawn( itype_id( "m1014" ) );
    gun->ammo_set( itype_id( "shot_bird" ) );
    player_character.wield( std::move( gun ) );
    REQUIRE( player_character.primary_weapon().typeId() == itype_id( "m1014" ) );

    REQUIRE( veh->velocity == 0 );

    const auto shots_fired = ranged::fire_gun( player_character, vehicle_origin + tripoint( 5, 0, 0 ),
                             1 );

    REQUIRE( shots_fired == 1 );
    CHECK( veh->velocity != 0 );
}

TEST_CASE( "vehicle gun recoil scaling factor can disable vehicle thrust", "[vehicle][gun]" )
{
    clear_all_state();
    rng_set_engine_seed( 0 );

    override_option vehicle_gun_recoil_factor( "VEHICLE_GUN_RECOIL_FACTOR", "0.0" );

    auto &here = get_map();
    auto &player_character = get_avatar();
    const auto vehicle_origin = tripoint( 60, 60, 0 );

    auto *const veh = here.add_vehicle( vproto_id( "bicycle" ), vehicle_origin, 0_degrees, 0, 0 );
    REQUIRE( veh != nullptr );

    player_character.setpos( vehicle_origin );
    here.board_vehicle( vehicle_origin, &player_character );
    REQUIRE( player_character.in_vehicle );

    auto gun = item::spawn( itype_id( "m1014" ) );
    gun->ammo_set( itype_id( "shot_bird" ) );
    player_character.wield( std::move( gun ) );
    REQUIRE( player_character.primary_weapon().typeId() == itype_id( "m1014" ) );

    REQUIRE( veh->velocity == 0 );

    const auto shots_fired = ranged::fire_gun( player_character, vehicle_origin + tripoint( 5, 0, 0 ),
                             1 );

    REQUIRE( shots_fired == 1 );
    CHECK( veh->velocity == 0 );
}

TEST_CASE( "brake hold toggles parked braking drag", "[vehicle][drag]" )
{
    clear_all_state();

    auto &here = get_map();
    auto *const bicycle = here.add_vehicle( vproto_id( "bicycle" ), tripoint( 60, 60, 0 ), 0_degrees, 0,
                                            0 );
    auto *const shopping_cart = here.add_vehicle( vproto_id( "shopping_cart" ), tripoint( 70, 60, 0 ),
                                0_degrees, 0, 0 );

    REQUIRE( bicycle != nullptr );
    REQUIRE( shopping_cart != nullptr );

    CHECK( bicycle->static_drag() < bicycle->static_drag( false ) );

    bicycle->toggle_brake_hold();
    shopping_cart->toggle_brake_hold();

    CHECK( bicycle->static_drag() == bicycle->static_drag( false ) );
    CHECK( shopping_cart->static_drag() == shopping_cart->static_drag( false ) );
}

TEST_CASE( "single birdshot can move a swivel chair one tile on office floor at 10x recoil",
           "[vehicle][gun]" )
{
    clear_all_state();
    rng_set_engine_seed( 0 );

    override_option vehicle_gun_recoil_factor( "VEHICLE_GUN_RECOIL_FACTOR", "10.0" );

    auto &here = get_map();
    auto &player_character = get_avatar();
    const auto vehicle_origin = tripoint( 60, 60, 0 );

    for( const auto x : std::views::iota( 40, 81 ) ) {
        here.ter_set( tripoint( x, vehicle_origin.y, vehicle_origin.z ), ter_id( "t_linoleum_white" ) );
        here.furn_set( tripoint( x, vehicle_origin.y, vehicle_origin.z ), furn_id( "f_null" ) );
    }

    auto *const veh = here.add_vehicle( vproto_id( "swivel_chair" ), vehicle_origin, 0_degrees, 0, 0 );
    REQUIRE( veh != nullptr );

    veh->toggle_brake_hold();

    player_character.setpos( vehicle_origin );
    here.board_vehicle( vehicle_origin, &player_character );
    REQUIRE( player_character.in_vehicle );

    auto gun = item::spawn( itype_id( "m1014" ) );
    gun->ammo_set( itype_id( "shot_bird" ) );
    player_character.wield( std::move( gun ) );
    REQUIRE( player_character.primary_weapon().typeId() == itype_id( "m1014" ) );

    const auto starting_pos = veh->global_pos3();

    const auto shots_fired = ranged::fire_gun( player_character, vehicle_origin + tripoint( 5, 0, 0 ),
                             1 );

    REQUIRE( shots_fired == 1 );
    REQUIRE( veh->velocity != 0 );

    for( const auto _ : std::views::iota( 0, 20 ) ) {
        ( void ) _;
        here.vehmove();
        if( veh->global_pos3() != starting_pos ) {
            break;
        }
    }

    CHECK( square_dist( starting_pos, veh->global_pos3() ) >= 1 );
    CHECK( player_character.pos() == veh->global_pos3() );
}

TEST_CASE( "vehicle gun recoil can launch a shopping cart with a mounted M2 Browning near 6 km/h",
           "[vehicle][gun]" )
{
    clear_all_state();
    rng_set_engine_seed( 0 );

    auto &here = get_map();
    auto &player_character = get_avatar();
    const auto vehicle_origin = tripoint( 60, 60, 0 );

    auto *const veh = here.add_vehicle( vproto_id( "shopping_cart" ), vehicle_origin, 0_degrees, 0, 0 );
    REQUIRE( veh != nullptr );

    REQUIRE( veh->install_part( point_zero, vpart_id( "turret_mount_manual_steel" ), true ) >= 0 );
    const auto turret_index = veh->install_part( point_zero, vpart_id( "mounted_browning" ), true );
    REQUIRE( turret_index >= 0 );
    REQUIRE( veh->part( turret_index ).ammo_set( itype_id( "50bmg" ) ) );

    player_character.setpos( vehicle_origin );
    here.board_vehicle( vehicle_origin, &player_character );
    REQUIRE( player_character.in_vehicle );

    auto turret = veh->turret_query( veh->part( turret_index ) );
    REQUIRE( turret );
    REQUIRE( turret.base().gun_set_mode( gun_mode_id( "AUTO" ) ) );
    REQUIRE( turret.query() == turret_data::status::ready );

    REQUIRE( veh->velocity == 0 );

    auto shots_fired = 0;
    for( const auto _ : std::views::iota( 0, 5 ) ) {
        ( void ) _;
        shots_fired += turret.fire( player_character, vehicle_origin + tripoint( 10, 0, 0 ) );
    }

    REQUIRE( shots_fired == 15 );
    CHECK( std::abs( veh->velocity ) >= 160 );
    CHECK( veh->forward_velocity() < 0.0f );
}

TEST_CASE( "perpendicular gun recoil keeps full sideways push on rigid-wheel vehicles",
           "[vehicle][gun]" )
{
    const auto vehicle_origin = tripoint( 60, 60, 0 );

    override_option vehicle_gun_recoil_factor( "VEHICLE_GUN_RECOIL_FACTOR", "1.0" );

    struct recoil_result {
        int velocity = 0;
        bool skidding = false;
        units::angle move_dir = 0_degrees;
    };

    const auto fire_recoil = [&]( const vproto_id & vehicle_type, const units::angle facing,
                                  const tripoint & target,
    const std::optional<tripoint> &shot_origin ) -> recoil_result {
        clear_all_state();
        rng_set_engine_seed( 0 );

        auto &here = get_map();
        auto &player_character = get_avatar();
        auto *const veh = here.add_vehicle( vehicle_type, vehicle_origin, facing, 0, 0 );
        REQUIRE( veh != nullptr );

        player_character.setpos( vehicle_origin );
        here.board_vehicle( vehicle_origin, &player_character );
        REQUIRE( player_character.in_vehicle );

        auto gun = item::spawn( itype_id( "m1014" ) );
        gun->ammo_set( itype_id( "shot_00" ) );
        player_character.wield( std::move( gun ) );
        REQUIRE( player_character.primary_weapon().typeId() == itype_id( "m1014" ) );

        REQUIRE( veh->velocity == 0 );

        auto shots_fired = 0;
        for( const auto _ : std::views::iota( 0, 5 ) )
        {
            ( void ) _;
            shots_fired += ranged::fire_gun( player_character, target, 1,
                                             player_character.primary_weapon(), nullptr, shot_origin );
        }

        REQUIRE( shots_fired == 5 );
        return recoil_result{
            .velocity = std::abs( veh->velocity ),
            .skidding = veh->skidding,
            .move_dir = veh->move.dir(),
        };
    };

    const auto forward_result = fire_recoil( vproto_id( "shopping_cart" ), 180_degrees,
                                vehicle_origin + tripoint( -5, 0, 0 ), std::nullopt );
    const auto offset_lateral_result = fire_recoil( vproto_id( "grocery_cart" ), -90_degrees,
                                       vehicle_origin + tripoint( -6, 0, 0 ),
                                       vehicle_origin + tripoint( -1, 0, 0 ) );

    CHECK( forward_result.skidding );
    CHECK( normalize( forward_result.move_dir ) == normalize( 0_degrees ) );
    CHECK( forward_result.velocity > 0 );
    CHECK( offset_lateral_result.skidding );
    CHECK( normalize( offset_lateral_result.move_dir ) == normalize( 0_degrees ) );
    CHECK( offset_lateral_result.velocity > 0 );
}
