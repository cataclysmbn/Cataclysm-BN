#include "catch/catch.hpp"

#include <memory>
#include <vector>

#include "avatar.h"
#include "avatar_action.h"
#include "coordinates.h"
#include "enums.h"
#include "game.h"
#include "game_constants.h"
#include "map.h"
#include "map_helpers.h"
#include "options_helpers.h"
#include "state_helpers.h"
#include "type_id.h"

namespace
{

static const auto effect_in_pit = efftype_id( "in_pit" );
static const auto skill_dodge = skill_id( "dodge" );

struct adjacent_pit_move {
    tripoint_bub_ms origin;
    tripoint_bub_ms destination;
};

auto setup_adjacent_pit_move( const ter_id &origin_terrain,
                              const ter_id &destination_terrain ) -> adjacent_pit_move
{
    clear_all_state();
    auto &here = get_map();
    const auto origin = tripoint_bub_ms( 60, 60, 0 );
    const auto destination = origin + tripoint_rel_ms::east();

    g->place_player( origin );
    here.ter_set( origin, origin_terrain );
    here.ter_set( destination, destination_terrain );
    g->u.add_known_trap( origin, here.tr_at( origin ) );
    g->u.add_known_trap( destination, here.tr_at( destination ) );
    g->u.add_effect( effect_in_pit, 1_turns, bodypart_str_id::NULL_ID() );
    g->u.str_cur = 0;
    g->u.dex_cur = 0;
    g->u.set_skill_level( skill_dodge, 0 );
    g->u.moves = 1000;

    return { .origin = origin, .destination = destination };
}

auto setup_adjacent_pit_move( const ter_id &terrain ) -> adjacent_pit_move
{
    return setup_adjacent_pit_move( terrain, terrain );
}

} // namespace

TEST_CASE( "moving_between_adjacent_pit_traps" )
{
    SECTION( "regular pit movement skips warning, escape check, and repeated damage" ) {
        const auto positions = setup_adjacent_pit_move( ter_id( "t_pit" ) );
        const auto hp_before = g->u.get_hp();

        CHECK( g->get_dangerous_tile( positions.destination ).empty() );
        REQUIRE( avatar_action::move( g->u, get_map(), tripoint_rel_ms::east() ) );

        CHECK( g->u.bub_pos() == positions.destination );
        CHECK( g->u.get_hp() == hp_before );
        CHECK( g->u.has_effect( effect_in_pit ) );
    }

    SECTION( "same spiked pit movement skips only the escape check" ) {
        const auto positions = setup_adjacent_pit_move( ter_id( "t_pit_spiked" ) );
        const auto dangerous_prompt = override_option( "DANGEROUS_TERRAIN_WARNING_PROMPT", "IGNORE" );
        const auto hp_before = g->u.get_hp();

        CHECK_FALSE( g->get_dangerous_tile( positions.destination ).empty() );
        REQUIRE( avatar_action::move( g->u, get_map(), tripoint_rel_ms::east() ) );

        CHECK( g->u.bub_pos() == positions.destination );
        CHECK( g->u.get_hp() < hp_before );
    }

    SECTION( "same glass pit movement skips only the escape check" ) {
        const auto positions = setup_adjacent_pit_move( ter_id( "t_pit_glass" ) );
        const auto dangerous_prompt = override_option( "DANGEROUS_TERRAIN_WARNING_PROMPT", "IGNORE" );
        const auto hp_before = g->u.get_hp();

        CHECK_FALSE( g->get_dangerous_tile( positions.destination ).empty() );
        REQUIRE( avatar_action::move( g->u, get_map(), tripoint_rel_ms::east() ) );

        CHECK( g->u.bub_pos() == positions.destination );
        CHECK( g->u.get_hp() < hp_before );
    }

    SECTION( "glass pit to regular pit skips warning, escape check, and repeated damage" ) {
        const auto positions = setup_adjacent_pit_move( ter_id( "t_pit_glass" ), ter_id( "t_pit" ) );
        const auto hp_before = g->u.get_hp();

        CHECK( g->get_dangerous_tile( positions.destination ).empty() );
        REQUIRE( avatar_action::move( g->u, get_map(), tripoint_rel_ms::east() ) );

        CHECK( g->u.bub_pos() == positions.destination );
        CHECK( g->u.get_hp() == hp_before );
        CHECK( g->u.has_effect( effect_in_pit ) );
    }

    SECTION( "different pit trap movement remains dangerous" ) {
        const auto positions = setup_adjacent_pit_move( ter_id( "t_pit" ) );
        auto &here = get_map();
        here.ter_set( positions.destination, ter_id( "t_pit_spiked" ) );
        g->u.add_known_trap( positions.destination, here.tr_at( positions.destination ) );

        CHECK_FALSE( g->get_dangerous_tile( positions.destination ).empty() );
    }
}

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
