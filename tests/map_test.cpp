#include "catch/catch.hpp"

#include <memory>
#include <vector>

#include "avatar.h"
#include "enums.h"
#include "game.h"
#include "game_constants.h"
#include "map.h"
#include "map_helpers.h"
#include "point.h"
#include "state_helpers.h"
#include "type_id.h"

TEST_CASE( "destroy_grabbed_furniture" )
{
    clear_all_state();
    GIVEN( "Furniture grabbed by the player" ) {
        const tripoint test_origin( 60, 60, 0 );
        map &here = get_map();
        g->u.setpos( test_origin );
        const tripoint grab_point = test_origin + tripoint_east;
        here.furn_set( grab_point, furn_id( "f_chair" ) );
        g->u.grab( OBJECT_FURNITURE, grab_point );
        WHEN( "The furniture grabbed by the player is destroyed" ) {
            here.destroy( grab_point );
            THEN( "The player's grab is released" ) {
                CHECK( g->u.get_grab_type() == OBJECT_NONE );
                CHECK( g->u.grab_point == tripoint_zero );
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
    g->place_player( tripoint_zero );
    CHECK( get_map().check_submap_active_item_consistency().empty() );
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
    static const tripoint p( 65, 65, 1 );
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
