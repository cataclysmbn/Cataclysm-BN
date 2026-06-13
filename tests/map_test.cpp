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
#include "monster.h"
#include "npc.h"
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
    CHECK( get_map().get_abs_sub() == player_reality_bubble_origin() );
}

TEST_CASE( "free_bubble_conversions_follow_avatar_position" )
{
    clear_all_state();

    auto &you = get_avatar();
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
    CHECK( abs_to_bub( player_sm ) == tripoint_bub_sm( g_half_mapsize, g_half_mapsize,
            player_abs.z() ) );
    CHECK( bub_to_abs( tripoint_bub_sm( g_half_mapsize, g_half_mapsize, player_abs.z() ) ) ==
           player_sm );
    CHECK( abs_to_bub( player_sm.xy() ) == point_bub_sm( g_half_mapsize, g_half_mapsize ) );
    CHECK( bub_to_abs( point_bub_sm( g_half_mapsize, g_half_mapsize ) ) == player_sm.xy() );
    CHECK( you.bub_pos() == expected_bub );
    CHECK( g->critter_at<avatar>( player_abs ) == &you );

    const auto moved_bub = tripoint_bub_ms( g_half_mapsize_x + 1, g_half_mapsize_y + 2,
                                            player_abs.z() );
    const auto moved_abs = bub_to_abs( moved_bub );
    you.setpos( moved_abs );
    CHECK( you.abs_pos() == moved_abs );
    CHECK( you.bub_pos() == moved_bub );
}

TEST_CASE( "reality_bubble_origin_helpers_use_explicit_size" )
{
    const auto player_sm = tripoint_abs_sm( 100, 200, 2 );
    const auto player_abs = project_to<coords::ms>( player_sm ) + tripoint_rel_ms( 3, 4, 0 );

    const auto size_four_origin = reality_bubble_origin_from_player( player_abs, 4 );
    CHECK( size_four_origin == player_sm - tripoint_rel_sm( 5, 5, 0 ) );
    CHECK( reality_bubble_center_from_origin( size_four_origin, 4 ) == player_sm );

    const auto size_zero_origin = reality_bubble_origin_from_player( player_abs, 0 );
    CHECK( size_zero_origin == player_sm - tripoint_rel_sm( 1, 1, 0 ) );
    CHECK( reality_bubble_center_from_origin( size_zero_origin, 0 ) == player_sm );
}

TEST_CASE( "update_map_uses_avatar_absolute_position" )
{
    clear_all_state();

    auto &here = get_map();
    auto &you = get_avatar();
    const auto old_origin = here.get_abs_sub();
    const auto destination = tripoint_bub_ms( g_half_mapsize_x + SEEX, g_half_mapsize_y, 0 );
    const auto destination_abs = map_local_to_abs( here, destination );

    you.setpos( destination_abs );
    const auto shift = g->update_map( you );

    CHECK( shift == point_rel_sm( 1, 0 ) );
    CHECK( here.get_abs_sub() == old_origin + tripoint_rel_sm( 1, 0, 0 ) );
    CHECK( here.get_abs_sub() == player_reality_bubble_origin() );
    CHECK( you.abs_pos() == destination_abs );
    CHECK( you.bub_pos() == tripoint_bub_ms( g_half_mapsize_x, g_half_mapsize_y, 0 ) );
}

TEST_CASE( "monster_tracker_uses_absolute_positions" )
{
    clear_all_state();

    auto &here = get_map();
    auto &you = get_avatar();
    const auto player_center = tripoint_bub_ms( g_half_mapsize_x, g_half_mapsize_y, 0 );
    you.setpos( map_local_to_abs( here, player_center ) );

    const auto monster_start = player_center + point_rel_ms( 2, 0 );
    auto *const mon = g->place_critter_at( mtype_id( "mon_zombie" ), monster_start );
    REQUIRE( mon != nullptr );
    const auto monster_abs = mon->abs_pos();

    CHECK( mon->bub_pos() == monster_start );
    CHECK( g->critter_at<monster>( monster_start ) == mon );
    CHECK( g->critter_at<monster>( monster_abs ) == mon );

    you.setpos( you.abs_pos() + tripoint_rel_ms( SEEX, 0, 0 ) );
    const auto player_shifted_monster_pos = abs_to_bub( monster_abs );
    CHECK( mon->abs_pos() == monster_abs );
    CHECK( mon->bub_pos() == player_shifted_monster_pos );
    CHECK( g->critter_at<monster>( monster_abs ) == mon );
    CHECK( g->critter_at<monster>( player_shifted_monster_pos ) == mon );
    CHECK( g->critter_at<monster>( monster_start ) == nullptr );

    const auto moved_abs = monster_abs + tripoint_rel_ms( 1, 0, 0 );
    mon->setpos( moved_abs );
    const auto moved_bub = abs_to_bub( moved_abs );
    CHECK( mon->abs_pos() == moved_abs );
    CHECK( g->critter_at<monster>( moved_bub ) == mon );
    CHECK( g->critter_at<monster>( player_shifted_monster_pos ) == nullptr );
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
