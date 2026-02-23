#include "catch/catch.hpp"

#include "shape.h"
#include "shape_impl.h"
#include "map.h"
#include "npc.h"
#include "ranged.h"
#include "projectile.h"
#include "map_helpers.h"
#include "overmapbuffer.h"
#include "game.h"
#include "state_helpers.h"

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
        bool point_is_covered = cov.contains( p ) && cov.at( p ) > 0.0;
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

TEST_CASE( "character using birdshot against another character", "[shape][ranged]" )
{
    clear_all_state();
    REQUIRE( get_map().has_zlevels() );
    get_player_character().setpos( {60, 60, -2} );

    const tripoint shooter_pos( 60, 60, 0 );
    const tripoint target_pos( 64, 64, 0 );
    standard_npc shooter( "shooter", shooter_pos );
    shared_ptr_fast<npc> target = make_shared_fast<npc>();
    target->randomize();
    target->worn.clear();
    target->spawn_at_precise( get_map().get_abs_sub().xy(), tripoint_zero );
    target->setpos( target_pos );
    overmap_buffer.insert_npc( target );
    g->load_npcs();

    detached_ptr<item> gun = item::spawn( itype_id( "m1014" ) );
    gun->ammo_set( itype_id( "shot_bird" ) );

    REQUIRE( gun->gun_range() >= rl_dist( shooter_pos, target_pos ) );
    REQUIRE( g->all_npcs().items->size() == 1 );
    REQUIRE( target->pos() == target_pos );
    REQUIRE( g->critter_at( target_pos ) == &*target );
    const int target_hp_total_before = target->get_hp();
    REQUIRE( target->get_hp() >= 100 );
    shooter.wield( std::move( gun ) );
    int shots_fired = ranged::fire_gun( shooter, target_pos, 1, shooter.primary_weapon(),
                                        nullptr );

    REQUIRE( shots_fired > 0 );
    CHECK( target->get_hp() < target_hp_total_before );
}
