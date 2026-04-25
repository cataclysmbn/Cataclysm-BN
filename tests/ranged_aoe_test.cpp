#include "catch/catch.hpp"

#include <algorithm>
#include <optional>
#include <vector>

#include "ballistics.h"
#include "dispersion.h"
#include "shape.h"
#include "shape_impl.h"
#include "map.h"
#include "npc.h"
#include "ranged.h"
#include "projectile.h"
#include "map_helpers.h"
#include "overmapbuffer.h"
#include "game.h"
#include "itype.h"
#include "rng.h"
#include "skill.h"
#include "state_helpers.h"

static const skill_id skill_gun( "gun" );
static const skill_id skill_shotgun( "shotgun" );

static auto fire_shell_at_target( const itype_id &ammo_id,
                                  const std::vector<itype_id> &armor_ids ) -> int
{
    clear_all_state();
    rng_set_engine_seed( 0 );
    REQUIRE( get_map().has_zlevels() );
    get_player_character().setpos( {60, 60, -2} );

    const auto shooter_pos = tripoint( 60, 60, 0 );
    const auto target_pos = tripoint( 62, 60, 0 );
    auto shooter = standard_npc( "shooter", shooter_pos );
    shooter.set_skill_level( skill_gun, 10 );
    shooter.set_skill_level( skill_shotgun, 10 );

    auto target = make_shared_fast<standard_npc>( "pellet_target", target_pos );
    target->worn.clear();
    target->spawn_at_precise( get_map().get_abs_sub().xy(), tripoint_zero );
    target->setpos( target_pos );
    for( const auto &armor_id : armor_ids ) {
        target->worn.push_back( item::spawn( armor_id ) );
    }
    ACTIVE_OVERMAP_BUFFER.insert_npc( target );
    g->load_npcs();

    detached_ptr<item> gun = item::spawn( itype_id( "m1014" ) );
    gun->ammo_set( ammo_id );

    REQUIRE( gun->ammo_data() != nullptr );
    REQUIRE( gun->ammo_data()->ammo != nullptr );
    REQUIRE( gun->ammo_data()->ammo->shot.has_value() );
    REQUIRE( gun->ammo_data()->ammo->shot->count > 1 );
    REQUIRE_FALSE( ranged::get_shape_factory( *gun ).has_value() );
    REQUIRE( ranged::get_target_shape_factory( *gun ).has_value() );
    REQUIRE( gun->gun_range() >= rl_dist( shooter_pos, target_pos ) );

    const auto target_hp_total_before = target->get_hp();
    shooter.wield( std::move( gun ) );

    const auto shots_fired = ranged::fire_gun( shooter, target_pos, 3, shooter.primary_weapon(),
                             nullptr );

    REQUIRE( shots_fired == 3 );
    return target_hp_total_before - target->get_hp();
}

static void shape_coverage_vs_distance_no_obstacle( const shape_factory_impl &c,
        const tripoint &origin, const tripoint &end )
{
    std::shared_ptr<shape> s = c.create( rl_vec3d( origin ), rl_vec3d( end ) );
    projectile p;
    p.impact = damage_instance();
    p.impact.add_damage( DT_STAB, 10 );
    auto cov = ranged::expected_coverage( *s, get_map(), 200 );

    map &here = get_map();
    inclusive_cuboid<tripoint> bb = s->bounding_box();
    REQUIRE( bb.p_min != bb.p_max );
    inclusive_cuboid<tripoint> expanded_bb( bb.p_min - point( 5, 5 ), bb.p_max + point( 5, 5 ) );
    bool had_any = false;
    CHECK( s->distance_at( rl_vec3d( origin ) ) > 0.0 );
    CHECK( cov[origin] <= 0.0 );
    for( const tripoint &p : here.points_in_rectangle( expanded_bb.p_min, expanded_bb.p_max ) ) {
        double signed_distance = s->distance_at( p );
        bool distance_on_shape_is_negative = signed_distance < 0.0;
        bool point_is_covered = cov.find( p ) != cov.end() && cov.at( p ) > 0.0;
        bool in_bounding_box = bb.contains( p );
        CAPTURE( p );
        CAPTURE( signed_distance );
        CAPTURE( cov[p] );
        CHECK( distance_on_shape_is_negative == point_is_covered );
        had_any |= distance_on_shape_is_negative;
        if( point_is_covered ) {
            CHECK( in_bounding_box );
        }
    }

    CHECK( had_any );
}

TEST_CASE( "expected shape coverage mass test", "[shape]" )
{
    clear_all_state();
    cone_factory c( 15_degrees, 10.0 );
    const tripoint origin( 60, 60, 0 );
    for( const tripoint &end : points_in_radius<tripoint>( origin, 5 ) ) {
        shape_coverage_vs_distance_no_obstacle( c, origin, end );
    }

    // Hard case
    shape_coverage_vs_distance_no_obstacle( c, {65, 65, 0}, tripoint{65, 65, 0} + point( 2, 1 ) );
}

TEST_CASE( "expected shape coverage without obstacles", "[shape]" )
{
    clear_all_state();
    cone_factory c( 22.5_degrees, 10.0 );
    const tripoint origin( 60, 60, 0 );
    const tripoint offset( 5, 5, 0 );
    const tripoint end = origin + offset;
    std::shared_ptr<shape> s = c.create( rl_vec3d( origin ), rl_vec3d( end ) );
    auto cov = ranged::expected_coverage( *s, get_map(), 3 );

    for( size_t i = 1; i <= 4; i++ ) {
        CHECK( cov[origin + point( i, i )] == 1.0 );
    }

    CHECK( cov[origin + point( 2, 1 )] == 1.0 );
    CHECK( cov[origin + point( 1, 2 )] == 1.0 );
}

TEST_CASE( "expected shape coverage through windows", "[shape]" )
{
    clear_all_state();
    cone_factory c( 22.5_degrees, 10.0 );
    const tripoint origin( 60, 60, 0 );
    const tripoint offset( 5, 0, 0 );
    const tripoint end = origin + offset;
    map &here = get_map();
    for( int wall_offset = -10; wall_offset <= 10; wall_offset++ ) {
        here.ter_set( tripoint( 62, 60 + wall_offset, 0 ), t_window );
    }

    std::shared_ptr<shape> s = c.create( rl_vec3d( origin ), rl_vec3d( end ) );
    auto cov = ranged::expected_coverage( *s, here, 3 );
    CHECK( cov[origin + point_east] == 1.0 );

    CHECK( cov[origin + 2 * point_east] == Approx( 0.25 ) );
    CHECK( cov[origin + 3 * point_east] == Approx( 0.25 ) );
    CHECK( cov[origin + 4 * point_east] == Approx( 0.25 ) );
}

TEST_CASE( "character using birdshot against another character", "[ranged]" )
{
    const auto damage = fire_shell_at_target( itype_id( "shot_bird" ), {} );

    CHECK( damage > 0 );
}

TEST_CASE( "birdshot pellets are much worse against armor", "[ranged][balance]" )
{
    const auto unarmored_damage = fire_shell_at_target( itype_id( "shot_bird" ), {} );
    const auto armored_damage = fire_shell_at_target( itype_id( "shot_bird" ),
    { itype_id( "survivor_suit" ), itype_id( "depowered_helmet" ) } );

    CHECK( unarmored_damage > armored_damage );
    CHECK( unarmored_damage >= armored_damage * 2 );
}

TEST_CASE( "pellet projectile keeps last hit critter after overpenetration",
           "[ranged][projectile]" )
{
    clear_all_state();
    rng_set_engine_seed( 0 );
    REQUIRE( get_map().has_zlevels() );

    auto &shooter = get_player_character();
    const auto shooter_pos = tripoint( 60, 60, 0 );
    const auto target_pos = tripoint( 62, 60, 0 );
    shooter.set_body();
    shooter.setpos( shooter_pos );
    shooter.set_skill_level( skill_gun, 10 );
    shooter.set_skill_level( skill_shotgun, 10 );

    auto target = make_shared_fast<standard_npc>( "pellet_target", target_pos );
    target->worn.clear();
    target->spawn_at_precise( get_map().get_abs_sub().xy(), tripoint_zero );
    target->setpos( target_pos );
    ACTIVE_OVERMAP_BUFFER.insert_npc( target );
    g->load_npcs();
    CHECK( shooter.sees( *target ) );

    detached_ptr<item> gun = item::spawn( itype_id( "m1014" ) );
    gun->ammo_set( itype_id( "shot_00" ) );
    shooter.wield( std::move( gun ) );

    auto probe = projectile {};
    probe.speed = shooter.primary_weapon().gun_speed();
    probe.impact = shooter.primary_weapon().gun_damage();
    probe.range = shooter.primary_weapon().gun_range();
    for( const auto &ammo_effect : shooter.primary_weapon().ammo_effects() ) {
        probe.add_effect( ammo_effect );
    }
    const auto probe_attack = projectile_attack( probe, shooter_pos, target_pos, dispersion_sources {},
                              &shooter, &shooter.primary_weapon(), nullptr, true );

    CHECK( probe_attack.hit_critter != nullptr );
    CHECK( probe_attack.dealt_dam.total_damage() > 0 );
    CHECK( probe_attack.end_point != target_pos );
}
