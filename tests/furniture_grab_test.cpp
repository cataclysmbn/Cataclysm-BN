#include "catch/catch.hpp"

#include <array>

#include "avatar.h"
#include "avatar_action.h"
#include "calendar.h"
#include "coordinates.h"
#include "enums.h"
#include "game.h"
#include "game_constants.h"
#include "map.h"
#include "map_helpers.h"
#include "mapdata.h"
#include "state_helpers.h"
#include "type_id.h"

static auto grabbed_ramp_x() -> int
{
    return g_half_mapsize_x + SEEX / 2;
}

static auto set_ramp_for_furniture( const int transit_x, const bool use_ramp,
                                    const bool up ) -> void
{
    calendar::turn = calendar::turn_zero;
    auto &player_character = get_player_character();
    REQUIRE_FALSE( player_character.in_vehicle );

    auto &here = get_map();
    build_test_map( ter_id( "t_pavement" ) );
    if( use_ramp ) {
        const auto upper_zlevel = up ? 1 : 0;
        const auto lower_zlevel = up - 1;
        const auto highx = transit_x + ( up ? 0 : 1 );
        const auto lowx = transit_x + ( up ? 1 : 0 );
        const auto max_x = SEEX * MAPSIZE - 1;
        const auto max_y = SEEY * MAPSIZE - 1;

        for( const auto &pos : here.points_in_rectangle( tripoint_bub_ms( 0, 0, -1 ),
                tripoint_bub_ms( transit_x - 1, max_y, 1 ) ) ) {
            if( ( up && pos.z() == upper_zlevel ) || ( !up && pos.z() == lower_zlevel ) ) {
                here.ter_set( pos, ter_id( "t_pavement" ) );
            } else if( up ) {
                here.ter_set( pos, ter_id( "t_rock" ) );
            } else {
                here.ter_set( pos, ter_id( "t_open_air" ) );
            }
        }

        for( const auto &pos : here.points_in_rectangle( tripoint_bub_ms( transit_x + 2, 0, -1 ),
                tripoint_bub_ms( max_x, max_y, 1 ) ) ) {
            if( pos.z() == 0 ) {
                here.ter_set( pos, ter_id( "t_pavement" ) );
            } else if( pos.z() > 0 ) {
                here.ter_set( pos, ter_id( "t_open_air" ) );
            } else {
                here.ter_set( pos, ter_id( "t_rock" ) );
            }
        }

        for( const auto &pos : here.points_in_rectangle( tripoint_bub_ms( lowx, 0, lower_zlevel ),
                tripoint_bub_ms( lowx, max_y, lower_zlevel ) ) ) {
            here.ter_set( pos, ter_id( "t_ramp_up_low" ) );
        }
        for( const auto &pos : here.points_in_rectangle( tripoint_bub_ms( highx, 0, lower_zlevel ),
                tripoint_bub_ms( highx, max_y, lower_zlevel ) ) ) {
            here.ter_set( pos, ter_id( "t_ramp_up_high" ) );
        }
        for( const auto &pos : here.points_in_rectangle( tripoint_bub_ms( lowx, 0, upper_zlevel ),
                tripoint_bub_ms( lowx, max_y, upper_zlevel ) ) ) {
            here.ter_set( pos, ter_id( "t_ramp_down_low" ) );
        }
        for( const auto &pos : here.points_in_rectangle( tripoint_bub_ms( highx, 0, upper_zlevel ),
                tripoint_bub_ms( highx, max_y, upper_zlevel ) ) ) {
            here.ter_set( pos, ter_id( "t_ramp_down_high" ) );
        }
    }

    for( const auto z : std::array{ -1, 0, 1 } ) {
        here.invalidate_map_cache( z );
        here.build_map_cache( z, true );
    }
}

static auto setup_grabbed_furniture( const tripoint_bub_ms &player_pos,
                                     const tripoint_bub_ms &furniture_pos,
                                     const furn_id &furniture_id ) -> void
{
    auto &here = get_map();
    auto &player_character = get_avatar();
    player_character.setpos( player_pos );
    player_character.str_max = 100;
    player_character.str_cur = 100;
    player_character.set_stamina( player_character.get_stamina_max() );

    here.furn_set( furniture_pos, furniture_id );
    player_character.grab( OBJECT_FURNITURE, furniture_pos - player_pos );
    REQUIRE( player_character.get_grab_type() == OBJECT_FURNITURE );
    REQUIRE( player_character.grab_point == furniture_pos - player_pos );
}

static auto check_avatar_still_grabs_furniture( const tripoint_bub_ms &expected_pos,
        const furn_id &expected_furniture ) -> void
{
    const auto &here = get_map();
    auto &player_character = get_avatar();
    CHECK( player_character.get_grab_type() == OBJECT_FURNITURE );
    const auto grabbed_pos = tripoint_bub_ms( player_character.bub_pos() +
                             player_character.grab_point );
    CHECK( grabbed_pos == expected_pos );
    CHECK( here.furn( grabbed_pos ) == expected_furniture );
}

TEST_CASE( "grabbed_furniture_can_be_pulled_up_ramp", "[furniture][ramp][grab]" )
{
    clear_all_state();
    const auto ramp_x = grabbed_ramp_x();
    set_ramp_for_furniture( ramp_x, true, true );

    auto &here = get_map();
    auto &player_character = get_avatar();
    const auto test_furniture = furn_id( "f_chair" );
    setup_grabbed_furniture( tripoint_bub_ms( ramp_x + 1, 60, 0 ),
                             tripoint_bub_ms( ramp_x + 2, 60, 0 ), test_furniture );

    REQUIRE( avatar_action::move( player_character, here, tripoint_rel_ms::west() ) );
    CHECK( player_character.bub_pos() == tripoint_bub_ms( ramp_x, 60, 1 ) );
    CHECK( here.furn( tripoint_bub_ms( ramp_x + 1, 60, 0 ) ) == test_furniture );
    check_avatar_still_grabs_furniture( tripoint_bub_ms( ramp_x + 1, 60, 0 ), test_furniture );

    REQUIRE( avatar_action::move( player_character, here, tripoint_rel_ms::west() ) );
    CHECK( player_character.bub_pos() == tripoint_bub_ms( ramp_x - 1, 60, 1 ) );
    CHECK( here.furn( tripoint_bub_ms( ramp_x, 60, 1 ) ) == test_furniture );
    check_avatar_still_grabs_furniture( tripoint_bub_ms( ramp_x, 60, 1 ), test_furniture );
}

TEST_CASE( "grabbed_furniture_can_be_pushed_up_ramp", "[furniture][ramp][grab]" )
{
    clear_all_state();
    const auto ramp_x = grabbed_ramp_x();
    set_ramp_for_furniture( ramp_x, true, true );

    auto &here = get_map();
    auto &player_character = get_avatar();
    const auto test_furniture = furn_id( "f_chair" );
    setup_grabbed_furniture( tripoint_bub_ms( ramp_x + 3, 60, 0 ),
                             tripoint_bub_ms( ramp_x + 2, 60, 0 ), test_furniture );

    REQUIRE( avatar_action::move( player_character, here, tripoint_rel_ms::west() ) );
    CHECK( player_character.bub_pos() == tripoint_bub_ms( ramp_x + 2, 60, 0 ) );
    CHECK( here.furn( tripoint_bub_ms( ramp_x + 1, 60, 0 ) ) == test_furniture );
    check_avatar_still_grabs_furniture( tripoint_bub_ms( ramp_x + 1, 60, 0 ), test_furniture );

    REQUIRE( avatar_action::move( player_character, here, tripoint_rel_ms::west() ) );
    CHECK( player_character.bub_pos() == tripoint_bub_ms( ramp_x + 1, 60, 0 ) );
    CHECK( here.furn( tripoint_bub_ms( ramp_x, 60, 1 ) ) == test_furniture );
    check_avatar_still_grabs_furniture( tripoint_bub_ms( ramp_x, 60, 1 ), test_furniture );

    REQUIRE( avatar_action::move( player_character, here, tripoint_rel_ms::west() ) );
    CHECK( player_character.bub_pos() == tripoint_bub_ms( ramp_x, 60, 1 ) );
    CHECK( here.furn( tripoint_bub_ms( ramp_x - 1, 60, 1 ) ) == test_furniture );
    check_avatar_still_grabs_furniture( tripoint_bub_ms( ramp_x - 1, 60, 1 ), test_furniture );
}

TEST_CASE( "grabbed_furniture_can_be_pulled_down_ramp", "[furniture][ramp][grab]" )
{
    clear_all_state();
    const auto ramp_x = grabbed_ramp_x();
    set_ramp_for_furniture( ramp_x, true, false );

    auto &here = get_map();
    auto &player_character = get_avatar();
    const auto test_furniture = furn_id( "f_chair" );
    setup_grabbed_furniture( tripoint_bub_ms( ramp_x + 1, 60, 0 ),
                             tripoint_bub_ms( ramp_x + 2, 60, 0 ), test_furniture );

    REQUIRE( avatar_action::move( player_character, here, tripoint_rel_ms::west() ) );
    CHECK( player_character.bub_pos() == tripoint_bub_ms( ramp_x, 60, -1 ) );
    CHECK( here.furn( tripoint_bub_ms( ramp_x + 1, 60, 0 ) ) == test_furniture );
    check_avatar_still_grabs_furniture( tripoint_bub_ms( ramp_x + 1, 60, 0 ), test_furniture );

    REQUIRE( avatar_action::move( player_character, here, tripoint_rel_ms::west() ) );
    CHECK( player_character.bub_pos() == tripoint_bub_ms( ramp_x - 1, 60, -1 ) );
    CHECK( here.furn( tripoint_bub_ms( ramp_x, 60, -1 ) ) == test_furniture );
    check_avatar_still_grabs_furniture( tripoint_bub_ms( ramp_x, 60, -1 ), test_furniture );
}

TEST_CASE( "grabbed_furniture_can_be_pushed_down_ramp", "[furniture][ramp][grab]" )
{
    clear_all_state();
    const auto ramp_x = grabbed_ramp_x();
    set_ramp_for_furniture( ramp_x, true, false );

    auto &here = get_map();
    auto &player_character = get_avatar();
    const auto test_furniture = furn_id( "f_chair" );
    setup_grabbed_furniture( tripoint_bub_ms( ramp_x + 3, 60, 0 ),
                             tripoint_bub_ms( ramp_x + 2, 60, 0 ), test_furniture );

    REQUIRE( avatar_action::move( player_character, here, tripoint_rel_ms::west() ) );
    CHECK( player_character.bub_pos() == tripoint_bub_ms( ramp_x + 2, 60, 0 ) );
    CHECK( here.furn( tripoint_bub_ms( ramp_x + 1, 60, 0 ) ) == test_furniture );
    check_avatar_still_grabs_furniture( tripoint_bub_ms( ramp_x + 1, 60, 0 ), test_furniture );

    REQUIRE( avatar_action::move( player_character, here, tripoint_rel_ms::west() ) );
    CHECK( player_character.bub_pos() == tripoint_bub_ms( ramp_x + 1, 60, 0 ) );
    CHECK( here.furn( tripoint_bub_ms( ramp_x, 60, -1 ) ) == test_furniture );
    check_avatar_still_grabs_furniture( tripoint_bub_ms( ramp_x, 60, -1 ), test_furniture );

    REQUIRE( avatar_action::move( player_character, here, tripoint_rel_ms::west() ) );
    CHECK( player_character.bub_pos() == tripoint_bub_ms( ramp_x, 60, -1 ) );
    CHECK( here.furn( tripoint_bub_ms( ramp_x - 1, 60, -1 ) ) == test_furniture );
    check_avatar_still_grabs_furniture( tripoint_bub_ms( ramp_x - 1, 60, -1 ), test_furniture );
}

TEST_CASE( "dragging_furniture_burns_extra_stamina", "[furniture][grab][stamina]" )
{
    clear_all_state();
    set_ramp_for_furniture( 60, false, true );

    auto &here = get_map();
    auto &player_character = get_avatar();
    player_character.setpos( tripoint_bub_ms( 61, 60, 0 ) );
    player_character.set_stamina( player_character.get_stamina_max() );
    const auto walking_stamina = player_character.get_stamina();
    REQUIRE( avatar_action::move( player_character, here, tripoint_rel_ms::west() ) );
    const auto walking_burn = walking_stamina - player_character.get_stamina();

    clear_all_state();
    set_ramp_for_furniture( 60, false, true );
    auto &second_here = get_map();
    auto &second_player_character = get_avatar();
    setup_grabbed_furniture( tripoint_bub_ms( 61, 60, 0 ),
                             tripoint_bub_ms( 62, 60, 0 ), furn_id( "f_dresser" ) );
    second_player_character.str_max = 8;
    second_player_character.str_cur = 8;
    const auto furniture_stamina = second_player_character.get_stamina();
    REQUIRE( avatar_action::move( second_player_character, second_here, tripoint_rel_ms::west() ) );
    const auto furniture_burn = furniture_stamina - second_player_character.get_stamina();

    CHECK( furniture_burn > walking_burn );
}
