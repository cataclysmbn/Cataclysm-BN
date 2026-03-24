#include "catch/catch.hpp"

#include <algorithm>
#include <cstddef>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "avatar.h"
#include "creature.h"
#include "explosion_queue.h"
#include "game.h"
#include "item.h"
#include "itype.h"
#include "iuse_actor.h"
#include "line.h"
#include "map.h"
#include "map_helpers.h"
#include "monster.h"
#include "point.h"
#include "state_helpers.h"
#include "string_id.h"
#include "test_statistics.h"
#include "type_id.h"
#include "veh_type.h"
#include "vehicle.h"
#include "vehicle_part.h"
#include "vpart_position.h"
#include "vpart_range.h"

enum class outcome_type {
    Kill, Casualty
};

namespace
{
void set_off_explosion( item &explosive, const tripoint &origin )
{
    explosion_handler::get_explosion_queue().clear();
    explosive.charges = 0;
    explosive.type->invoke( g->u, explosive, origin );
    explosion_handler::get_explosion_queue().execute();
}

void check_lethality( const std::string &explosive_id, const int range, float lethality,
                      float margin, outcome_type expected_outcome )
{
    const epsilon_threshold target_lethality{ lethality, margin };
    int num_survivors = 0;
    int num_subjects = 0;
    int num_wounded = 0;
    statistics<bool> victims;
    std::stringstream survivor_stats;
    int total_hp = 0;
    do {
        // Clear map
        clear_map();
        put_player_underground();
        // Spawn some monsters in a circle.
        tripoint origin( 30, 30, 0 );
        int num_subjects_this_time = 0;
        for( const tripoint &monster_position : closest_points_first( origin, range ) ) {
            if( rl_dist( monster_position, origin ) != range ) {
                continue;
            }
            num_subjects++;
            num_subjects_this_time++;
            monster &new_monster = spawn_test_monster( "mon_zombie", monster_position );
            new_monster.no_extra_death_drops = true;
        }
        item &explosive = *item::spawn_temporary( explosive_id );
        set_off_explosion( explosive, origin );
        // see how many monsters survive
        std::vector<Creature *> survivors = g->get_creatures_if( []( const Creature & critter ) {
            return critter.is_monster();
        } );
        num_survivors += survivors.size();
        for( Creature *survivor : survivors ) {
            survivor_stats << survivor->pos() << " " << survivor->get_hp() << ", ";
            bool wounded = survivor->get_hp() < survivor->get_hp_max();
            num_wounded += wounded ? 1 : 0;
            total_hp += survivor->get_hp();
            if( expected_outcome == outcome_type::Casualty && wounded ) {
                victims.add( true );
            } else {
                victims.add( false );
            }
        }
        if( !survivors.empty() ) {
            survivor_stats << '\n';
        }
        for( int i = survivors.size(); i < num_subjects_this_time; ++i ) {
            victims.add( true );
        }
    } while( victims.uncertain_about( target_lethality ) );
    CAPTURE( margin );
    INFO( explosive_id );
    INFO( "range " << range );
    INFO( num_survivors << " survivors out of " << num_subjects << " targets." );
    INFO( survivor_stats.str() );
    INFO( "Wounded survivors: " << num_wounded );
    const int average_hp = num_survivors ? total_hp / num_survivors : 0;
    INFO( "average hp of survivors: " << average_hp );
    CHECK( victims.avg() == Approx( lethality ).margin( margin ) );
}

auto get_part_hp( vehicle *veh ) -> std::vector<int>
{
    std::vector<int> part_hp;
    part_hp.reserve( veh->part_count() );
    for( const vpart_reference &vpr : veh->get_all_parts() ) {
        part_hp.push_back( vpr.part().hp() );
    }
    return part_hp;
}

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
void check_vehicle_damage( const std::string &explosive_id, const std::string &vehicle_id,
                           const int range )
{
    put_player_underground();
    tripoint origin( 30, 30, 0 );

    vehicle *target_vehicle = get_map().add_vehicle( vproto_id( vehicle_id ), origin, 0_degrees,
                              -1, 0 );
    std::vector<int> before_hp = get_part_hp( target_vehicle );

    while( get_map().veh_at( origin ) ) {
        origin.x++;
    }
    origin.x += range;

    item &explosive = *item::spawn_temporary( explosive_id );
    set_off_explosion( explosive, origin );

    std::vector<int> after_hp = get_part_hp( target_vehicle );

    // We don't expect any destroyed parts.
    REQUIRE( before_hp.size() == after_hp.size() );
    for( size_t i = 0; i < before_hp.size(); ++i ) {
        CAPTURE( i );
        INFO( target_vehicle->part( i ).name() );
        if( target_vehicle->part( i ).info().get_id() == vpart_id( "battery_car" ) ||
            target_vehicle->part( i ).info().get_id() == vpart_id( "headlight" ) ||
            target_vehicle->part( i ).info().get_id() == vpart_id( "windshield" ) ) {
            CHECK( before_hp[ i ] >= after_hp[ i ] );
        } else if( !( target_vehicle->part( i ).info().get_id() == vpart_id( "vehicle_clock" ) ) ) {
            CHECK( before_hp[ i ] == after_hp[ i ] );
        }
    }
}

} // namespace


TEST_CASE( "grenade_lethality", "[.][grenade][explosion][balance][slow]" )
{
    clear_all_state();
    check_lethality( "grenade_act", 3, 0.95, 0.06, outcome_type::Kill );
    check_lethality( "grenade_act", 6, 0.95, 0.06, outcome_type::Casualty );
}

TEST_CASE( "grenade_vs_vehicle", "[grenade][explosion][balance]" )
{
    clear_all_state();
    check_vehicle_damage( "grenade_act", "car", 5 );
}

TEST_CASE( "shrapnel behind wall", "[grenade][explosion][balance]" )
{
    clear_all_state();
    put_player_underground();
    tripoint origin( 30, 30, 0 );

    item &grenade = *item::spawn_temporary( "can_bomb_act" );
    REQUIRE( grenade.get_use( "explosion" ) != nullptr );
    const auto *actor = dynamic_cast<const explosion_iuse *>
                        ( grenade.get_use( "explosion" )->get_actor_ptr() );
    REQUIRE( actor != nullptr );
    REQUIRE( static_cast<bool>( actor->explosion.fragment ) );
    REQUIRE( actor->explosion.radius <= 0 );
    REQUIRE( actor->explosion.fragment->range > 2 );

    for( const tripoint &pt : closest_points_first( origin, 2 ) ) {
        if( square_dist( origin, pt ) > 1 ) {
            g->m.ter_set( pt, t_wall_metal );
        }
    }

    // Not on the bomb because shrapnel always hits that square
    const monster &m_in_range = spawn_test_monster( "mon_zombie", origin + point_east );
    const monster &m_behind_wall = spawn_test_monster( "mon_zombie", origin + point( 3, 0 ) );

    set_off_explosion( grenade, origin );

    CHECK( m_in_range.hp_percentage() < 100 );
    CHECK( m_behind_wall.hp_percentage() == 100 );
}

TEST_CASE( "shrapnel at huge range", "[grenade][explosion]" )
{
    clear_all_state();
    put_player_underground();
    tripoint origin;

    item &grenade = *item::spawn_temporary( "debug_shrapnel_blast" );
    REQUIRE( grenade.get_use( "explosion" ) != nullptr );
    const auto *actor = dynamic_cast<const explosion_iuse *>
                        ( grenade.get_use( "explosion" )->get_actor_ptr() );
    REQUIRE( actor != nullptr );
    REQUIRE( static_cast<bool>( actor->explosion.fragment ) );
    REQUIRE( actor->explosion.radius <= 0 );
    REQUIRE( actor->explosion.fragment->range > g_mapsize_x + g_mapsize_y );

    const monster &m = spawn_test_monster( "mon_zombie", tripoint( g_mapsize_x - 1, g_mapsize_y - 1,
                                           0 ) );

    set_off_explosion( grenade, origin );

    CHECK( m.is_dead_state() );
}

TEST_CASE( "shrapnel at max grenade range", "[grenade][explosion]" )
{
    clear_all_state();
    put_player_underground();
    tripoint origin( 60, 60, 0 );

    item &grenade = *item::spawn_temporary( "can_bomb_act" );
    REQUIRE( grenade.get_use( "explosion" ) != nullptr );
    const auto *actor = dynamic_cast<const explosion_iuse *>
                        ( grenade.get_use( "explosion" )->get_actor_ptr() );
    REQUIRE( actor != nullptr );
    REQUIRE( static_cast<bool>( actor->explosion.fragment ) );
    REQUIRE( actor->explosion.radius <= 0 );
    REQUIRE( actor->explosion.fragment->range > 1 );

    const int range = actor->explosion.fragment->range;
    for( const tripoint &pt : closest_points_first( origin, range + 1 ) ) {
        spawn_test_monster( "mon_zombie", pt );
    }

    set_off_explosion( grenade, origin );

    for( const tripoint &pt : closest_points_first( origin, range + 1 ) ) {
        const monster *m = g->critter_at<monster>( pt );
        REQUIRE( m != nullptr );
        CAPTURE( m->pos() );
        CAPTURE( rl_dist( origin, m->pos() ) );
        if( rl_dist( origin, m->pos() ) <= range ) {
            CHECK( m->hp_percentage() < 100 );
        } else {
            CHECK( m->hp_percentage() == 100 );
        }
    }
}

TEST_CASE( "rotated_vehicle_walls_block_explosions" )
{
    clear_all_state();
    put_player_underground();
    tripoint origin( 60, 60, 0 );

    item &grenade = *item::spawn_temporary( "can_bomb_act" );

    map &here = get_map();

    here.add_vehicle( vproto_id( "apc" ), origin, -45_degrees, 0, 0 );

    here.build_map_cache( 0 );

    tripoint mon_origin = origin + tripoint( -2, 1, 0 );

    monster &s = spawn_test_monster( "mon_squirrel", mon_origin );

    REQUIRE( veh_pointer_or_null( here.veh_at( mon_origin ) ) != nullptr );

    tripoint explode_at = mon_origin + tripoint_north_west;

    REQUIRE( veh_pointer_or_null( here.veh_at( explode_at ) ) == nullptr );

    set_off_explosion( grenade, explode_at );

    const monster *m = g->critter_at<monster>( mon_origin );
    REQUIRE( m != nullptr );
    CHECK( m == &s );
    CHECK( m->get_hp() == m->get_hp_max() );
}
