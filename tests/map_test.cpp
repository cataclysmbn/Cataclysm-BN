#include "catch/catch.hpp"

#include <memory>
#include <vector>

#include "avatar.h"
#include "coordinates.h"
#include "enums.h"
#include "game.h"
#include "game_constants.h"
#include "map.h"
#include "map_helpers.h"
#include "state_helpers.h"
#include "type_id.h"

TEST_CASE( "destroy_grabbed_furniture" )
{
    clear_all_state();
    GIVEN( "Furniture grabbed by the player" ) {
        const tripoint_bub_ms test_origin( 60, 60, 0 );
        map &here = get_map();
        g->u.setpos( test_origin );
        const tripoint_bub_ms grab_point = test_origin + tripoint_rel_ms::east();
        here.furn_set( grab_point, furn_id( "f_chair" ) );
        g->u.grab( OBJECT_FURNITURE, tripoint_rel_ms::east() );
        WHEN( "The furniture grabbed by the player is destroyed" ) {
            here.destroy( grab_point );
            THEN( "The player's grab is released" ) {
                CHECK( g->u.get_grab_type() == OBJECT_NONE );
                CHECK( g->u.grab_point == tripoint_rel_ms::zero() );
            }
        }
    }
}

// map_bounds_checking removed: the basic inbounds() cuboid check is trivial.
// A meaningful bounds test would involve pocket dimensions and dimension_bounds,
// which require more involved setup (mapgen, dimension transitions, etc.).

// tinymap_bounds_checking removed: same reasoning as map_bounds_checking above.

TEST_CASE( "place_player_can_safely_move_multiple_submaps" )
{
    clear_all_state();
    // Regression test for the situation where game::place_player would misuse
    // map::shift if the resulting shift exceeded a single submap, leading to a
    // broken active item cache.
    g->place_player( tripoint_bub_ms::zero() );
    CHECK( get_map().check_submap_active_item_consistency().empty() );
}

TEST_CASE( "free_bubble_conversions_follow_avatar_position" )
{
    clear_all_state();

    avatar &you = get_avatar();
    const auto player_sm = tripoint_abs_sm( 100, 200, 2 );
    const auto player_offset = tripoint_rel_ms( 3, 4, 0 );
    const auto player_abs = project_to<coords::ms>( player_sm ) + player_offset;
    you.setpos( player_abs );

    const auto expected_origin = player_sm -
                                 tripoint_rel_sm( g_half_mapsize, g_half_mapsize, 0 );
    const auto expected_bub = tripoint_bub_ms( g_half_mapsize_x + player_offset.x(),
                              g_half_mapsize_y + player_offset.y(), player_abs.z() );

    CHECK( player_reality_bubble_origin() == expected_origin );
    CHECK( abs_to_bub( player_abs ) == expected_bub );
    CHECK( bub_to_abs( expected_bub ) == player_abs );
}

TEST_CASE( "update_map_uses_avatar_absolute_position" )
{
    clear_all_state();

    map &here = get_map();
    avatar &you = get_avatar();
    const auto old_origin = here.get_abs_sub();
    const auto destination = tripoint_bub_ms( g_half_mapsize_x + SEEX, g_half_mapsize_y, 0 );
    const auto destination_abs = here.bub_to_abs( destination );

    you.setpos( destination );
    const auto shift = g->update_map( you );

    CHECK( shift == point_rel_sm( 1, 0 ) );
    CHECK( here.get_abs_sub() == old_origin + tripoint_rel_sm( 1, 0, 0 ) );
    CHECK( you.abs_pos() == destination_abs );
    CHECK( you.bub_pos() == tripoint_bub_ms( g_half_mapsize_x, g_half_mapsize_y, 0 ) );
}

static std::ostream &operator<<( std::ostream &os, const ter_id &tid )
{
    os << tid.id().c_str();
    return os;
}

TEST_CASE( "bash_through_roof_can_destroy_multiple_times" )
{
    clear_all_state();
    map &here = get_map();
    REQUIRE( here.has_zlevels() );

    static const ter_str_id t_fragile_roof( "t_fragile_roof" );
    static const ter_str_id t_strong_roof( "t_strong_roof" );
    static const ter_str_id t_rock_floor_no_roof( "t_rock_floor_no_roof" );
    static const ter_str_id t_open_air( "t_open_air" );
    static const tripoint_bub_ms p( 65, 65, 1 );
    WHEN( "A wall has a matching roof above it, but the roof turns to a stronger roof on successful bash" ) {
        static const ter_str_id t_fragile_wall( "t_fragile_wall" );
        here.ter_set( p + tripoint_below, t_fragile_wall );
        here.ter_set( p, t_fragile_roof );
        AND_WHEN( "The roof is bashed with only enough strength to destroy the weaker roof type" ) {
            here.bash( p, 10, false, false, true );
            THEN( "The roof turns to the stronger type and the wall doesn't change" ) {
                CHECK( here.ter( p ) == t_strong_roof );
                CHECK( here.ter( p + tripoint_below ) == t_fragile_wall );
            }
        }

        AND_WHEN( "The roof is bashed with enough strength to destroy any roof" ) {
            here.bash( p, 1000, false, false, true );
            THEN( "Both the roof and the wall are destroyed" ) {
                CHECK( here.ter( p ) == t_open_air );
                CHECK( here.ter( p + tripoint_below ) == t_rock_floor_no_roof );
            }
        }
    }

    WHEN( "A passable floor has a matching roof above it, but both the roof and the floor turn into stronger variants on destroy" ) {
        static const ter_str_id t_fragile_floor( "t_fragile_floor" );
        here.ter_set( p + tripoint_below, t_fragile_floor );
        here.ter_set( p, t_fragile_roof );
        AND_WHEN( "The roof is bashed with only enough strength to destroy the weaker roof type" ) {
            here.bash( p, 10, false, false, true );
            THEN( "The roof turns to the stronger type and the floor doesn't change" ) {
                CHECK( here.ter( p ) == t_strong_roof );
                CHECK( here.ter( p + tripoint_below ) == t_fragile_floor );
            }
        }

        AND_WHEN( "The roof is bashed with enough strength to destroy any roof" ) {
            here.bash( p, 1000, false, false, true );
            THEN( "Both the roof and the floor are completely destroyed to default terrain" ) {
                CHECK( here.ter( p ) == t_open_air );
                CHECK( here.ter( p + tripoint_below ) == t_rock_floor_no_roof );
            }
        }
    }
}
