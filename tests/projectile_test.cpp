#include "catch/catch.hpp"

#include <memory>
#include <set>
#include <vector>

#include "avatar.h"
#include "ballistics.h"
#include "damage.h"
#include "dispersion.h"
#include "game.h"
#include "item.h"
#include "map.h"
#include "map_helpers.h"
#include "monster.h"
#include "npc.h"
#include "player_helpers.h"
#include "point.h"
#include "projectile.h"
#include "state_helpers.h"
#include "type_id.h"
#include "vehicle.h"
#include "vpart_position.h"

static tripoint projectile_end_point( const std::vector<tripoint> &range, const item &gun,
                                      int proj_range )
{
    projectile test_proj;
    test_proj.speed = gun.gun_speed();
    test_proj.range = proj_range;
    test_proj.impact = gun.gun_damage();
    for( const ammo_effect_str_id &ae_id : gun.ammo_effects() ) {
        test_proj.add_effect( ae_id );
    }

    dealt_projectile_attack attack;

    attack = projectile_attack( test_proj, range[0], range[2], dispersion_sources(), &get_avatar(),
                                nullptr );

    return attack.end_point;
}

TEST_CASE( "projectiles_through_obstacles", "[projectile]" )
{
    clear_all_state();
    // Move the player out of the way of the test area
    get_avatar().setpos( { 2, 2, 0 } );

    map &here = get_map();
    // Ensure that a projectile fired from a gun can pass through a wall
    // First, set up a test area - three tiles in a row
    // One on either side clear, with a wooden wall in the middle
    std::vector<tripoint> range = { tripoint_zero, tripoint_east, tripoint( 2, 0, 0 ) };
    for( const tripoint &pt : range ) {
        REQUIRE( here.inbounds( pt ) );
        here.ter_set( pt, ter_id( "t_dirt" ) );
        here.furn_set( pt, furn_id( "f_null" ) );
        REQUIRE_FALSE( g->critter_at( pt ) );
        REQUIRE( here.is_transparent( pt ) );
    }

    // Set first obstacle in the way, a wooden wall
    here.ter_set( range[1], ter_id( "t_wall_wood" ) );

    // Create two guns to fire a projectile from, one won't penetrate obstacles
    detached_ptr<item> gun_penetrating = item::spawn( itype_id( "m1a" ) );
    gun_penetrating->ammo_set( itype_id( "308" ), 5 );
    detached_ptr<item> gun_nonpenetrating = item::spawn( itype_id( "remington_870" ) );
    gun_nonpenetrating->ammo_set( itype_id( "shot_dragon" ), 5 );

    // Check that a rifle bullet can go through a weak wall, but dragon's breath stops early
    INFO( "rifle bullet vs. wooden wall" );
    CHECK( projectile_end_point( range, *gun_penetrating, 3 ) == range[2] );
    INFO( "dragon's breath shell vs. wooden wall" );
    CHECK( projectile_end_point( range, *gun_nonpenetrating, 3 ) == range[0] );

    // Change obstacle to something tougher
    here.ter_set( range[1], ter_id( "t_rock" ) );

    // Check that the rifle bullet cannot go through a tougher wall
    INFO( "rifle bullet vs. solid rock" );
    CHECK( projectile_end_point( range, *gun_penetrating, 3 ) == range[0] );
}

TEST_CASE( "adjacent_friendly_fire_prevention", "[projectile][ballistics]" )
{
    clear_all_state();
    map &here = get_map();

    // Set up test area: shooter at (0,0), friendly NPC adjacent at (1,0), target at (5,0)
    const auto shooter_pos = tripoint_zero;
    const auto friendly_pos = tripoint_east;
    const auto target_pos = tripoint( 5, 0, 0 );

    // Clear the area
    for( int x = 0; x <= 5; ++x ) {
        auto pt = tripoint( x, 0, 0 );
        REQUIRE( here.inbounds( pt ) );
        here.ter_set( pt, ter_id( "t_dirt" ) );
        here.furn_set( pt, furn_id( "f_null" ) );
    }

    // Create friendly NPC at adjacent position
    auto &ally = spawn_npc( friendly_pos.xy(), "thug" );
    ally.set_fac( faction_id( "your_followers" ) );
    ally.set_attitude( NPCATT_FOLLOW );
    REQUIRE( g->critter_at( friendly_pos ) == &ally );
    REQUIRE( ally.is_player_ally() );

    // Set up avatar as shooter
    auto &shooter = get_avatar();
    shooter.setpos( shooter_pos );
    shooter.set_body();

    // Create a gun for the projectile
    auto gun_ptr = item::spawn( itype_id( "glock_19" ) );
    gun_ptr->ammo_set( itype_id( "9mm" ), 1 );
    item &gun = *gun_ptr;

    // Record initial HP of friendly
    const int initial_friendly_hp = ally.get_hp();

    // Create projectile from gun with extremely high dispersion to increase miss chance
    projectile test_proj;
    test_proj.speed = gun.gun_speed();
    test_proj.range = 10;
    test_proj.impact = gun.gun_damage();

    // Fire projectile toward target with high dispersion (simulating a miss toward the ally)
    dealt_projectile_attack attack = projectile_attack(
                                         test_proj, shooter_pos, target_pos,
                                         dispersion_sources( 5000 ), &shooter, &gun );

    // Verify friendly was not hit despite being adjacent to shooter in projectile path
    const int final_friendly_hp = ally.get_hp();
    CHECK( final_friendly_hp == initial_friendly_hp );

    // The projectile should not stop at the friendly's position
    CHECK( attack.end_point != friendly_pos );
}

TEST_CASE( "npc_adjacent_friendly_fire_prevention", "[projectile][ballistics]" )
{
    clear_all_state();
    map &here = get_map();

    // Set up test area: NPC shooter at (0,0), friendly NPC adjacent at (1,0), target at (5,0)
    const auto shooter_pos = tripoint_zero;
    const auto friendly_pos = tripoint_east;
    const auto target_pos = tripoint( 5, 0, 0 );

    // Clear the area
    for( int x = 0; x <= 5; ++x ) {
        auto pt = tripoint( x, 0, 0 );
        REQUIRE( here.inbounds( pt ) );
        here.ter_set( pt, ter_id( "t_dirt" ) );
        here.furn_set( pt, furn_id( "f_null" ) );
    }

    // Create NPC shooter
    auto &shooter = spawn_npc( shooter_pos.xy(), "thug" );
    shooter.set_fac( faction_id( "your_followers" ) );
    shooter.set_attitude( NPCATT_FOLLOW );
    REQUIRE( g->critter_at( shooter_pos ) == &shooter );
    REQUIRE( shooter.is_player_ally() );

    // Create friendly NPC at adjacent position
    auto &ally = spawn_npc( friendly_pos.xy(), "thug" );
    ally.set_fac( faction_id( "your_followers" ) );
    ally.set_attitude( NPCATT_FOLLOW );
    REQUIRE( g->critter_at( friendly_pos ) == &ally );
    REQUIRE( ally.is_player_ally() );

    // Verify they're friendly to each other
    REQUIRE( shooter.attitude_to( ally ) == Attitude::A_FRIENDLY );

    // Move player out of the way
    get_avatar().setpos( tripoint( 10, 10, 0 ) );

    // Create a gun for the projectile
    auto gun_ptr = item::spawn( itype_id( "glock_19" ) );
    gun_ptr->ammo_set( itype_id( "9mm" ), 1 );
    item &gun = *gun_ptr;

    // Record initial HP of friendly
    const int initial_friendly_hp = ally.get_hp();

    // Create projectile from gun
    projectile test_proj;
    test_proj.speed = gun.gun_speed();
    test_proj.range = 10;
    test_proj.impact = gun.gun_damage();

    // Fire projectile toward target with high dispersion
    dealt_projectile_attack attack = projectile_attack(
                                         test_proj, shooter_pos, target_pos,
                                         dispersion_sources( 5000 ), &shooter, &gun );

    // Verify friendly NPC was not hit despite being adjacent to shooter
    const int final_friendly_hp = ally.get_hp();
    CHECK( final_friendly_hp == initial_friendly_hp );

    // The projectile should not stop at the friendly's position
    CHECK( attack.end_point != friendly_pos );
}

TEST_CASE( "npc_protects_adjacent_player", "[projectile][ballistics]" )
{
    clear_all_state();
    map &here = get_map();

    // Set up test area: NPC shooter at (0,0), player adjacent at (1,0), target at (5,0)
    const auto shooter_pos = tripoint_zero;
    const auto player_pos = tripoint_east;
    const auto target_pos = tripoint( 5, 0, 0 );

    // Clear the area
    for( int x = 0; x <= 5; ++x ) {
        auto pt = tripoint( x, 0, 0 );
        REQUIRE( here.inbounds( pt ) );
        here.ter_set( pt, ter_id( "t_dirt" ) );
        here.furn_set( pt, furn_id( "f_null" ) );
    }

    // Create NPC shooter
    auto &shooter = spawn_npc( shooter_pos.xy(), "thug" );
    shooter.set_fac( faction_id( "your_followers" ) );
    shooter.set_attitude( NPCATT_FOLLOW );
    REQUIRE( g->critter_at( shooter_pos ) == &shooter );
    REQUIRE( shooter.is_player_ally() );

    // Set up player at adjacent position
    auto &player = get_avatar();
    player.setpos( player_pos );
    player.set_body();
    REQUIRE( g->critter_at( player_pos ) == &player );

    // Verify NPC is friendly to player
    REQUIRE( shooter.attitude_to( player ) == Attitude::A_FRIENDLY );

    // Create a gun for the projectile
    auto gun_ptr = item::spawn( itype_id( "glock_19" ) );
    gun_ptr->ammo_set( itype_id( "9mm" ), 1 );
    item &gun = *gun_ptr;

    // Record initial HP of player
    const int initial_player_hp = player.get_hp();

    // Create projectile from gun
    projectile test_proj;
    test_proj.speed = gun.gun_speed();
    test_proj.range = 10;
    test_proj.impact = gun.gun_damage();

    // Fire projectile toward target with high dispersion
    dealt_projectile_attack attack = projectile_attack(
                                         test_proj, shooter_pos, target_pos,
                                         dispersion_sources( 5000 ), &shooter, &gun );

    // Verify player was not hit despite being adjacent to NPC shooter
    const int final_player_hp = player.get_hp();
    CHECK( final_player_hp == initial_player_hp );

    // The projectile should not stop at the player's position
    CHECK( attack.end_point != player_pos );
}

TEST_CASE( "monster_adjacent_ally_fire_prevention", "[projectile][ballistics]" )
{
    clear_all_state();
    map &here = get_map();

    // Set up test area: monster shooter at (0,0), allied monster at (1,0), target at (5,0)
    const auto shooter_pos = tripoint_zero;
    const auto ally_pos = tripoint_east;
    const auto target_pos = tripoint( 5, 0, 0 );

    // Clear the area
    for( int x = 0; x <= 5; ++x ) {
        auto pt = tripoint( x, 0, 0 );
        REQUIRE( here.inbounds( pt ) );
        here.ter_set( pt, ter_id( "t_dirt" ) );
        here.furn_set( pt, furn_id( "f_null" ) );
    }

    // Move player out of the way
    get_avatar().setpos( tripoint( 10, 10, 0 ) );

    // Create two monsters from the same faction
    monster &shooter = spawn_test_monster( "mon_zombie", shooter_pos );
    monster &ally = spawn_test_monster( "mon_zombie", ally_pos );

    REQUIRE( g->critter_at( shooter_pos ) == &shooter );
    REQUIRE( g->critter_at( ally_pos ) == &ally );

    // Verify they're friendly to each other (same faction, both unfriendly to player)
    REQUIRE( shooter.attitude_to( ally ) == Attitude::A_FRIENDLY );

    // Record initial HP of ally monster
    const int initial_ally_hp = ally.get_hp();

    // Create a projectile (simulating monster ranged attack)
    projectile test_proj;
    test_proj.speed = 1000;
    test_proj.range = 10;
    test_proj.impact.add_damage( DT_BASH, 10 );

    // Fire projectile toward target with high dispersion
    dealt_projectile_attack attack = projectile_attack(
                                         test_proj, shooter_pos, target_pos,
                                         dispersion_sources( 5000 ), &shooter, nullptr );

    // Verify allied monster was not hit despite being adjacent to shooter
    const int final_ally_hp = ally.get_hp();
    CHECK( final_ally_hp == initial_ally_hp );

    // The projectile should not stop at the ally's position
    CHECK( attack.end_point != ally_pos );
}

TEST_CASE( "hostile_npc_adjacent_ally_fire_prevention", "[projectile][ballistics]" )
{
    clear_all_state();
    map &here = get_map();

    // Set up test area: hostile NPC shooter at (0,0), allied hostile NPC at (1,0), target at (5,0)
    const auto shooter_pos = tripoint_zero;
    const auto ally_pos = tripoint_east;
    const auto target_pos = tripoint( 5, 0, 0 );

    // Clear the area
    for( int x = 0; x <= 5; ++x ) {
        auto pt = tripoint( x, 0, 0 );
        REQUIRE( here.inbounds( pt ) );
        here.ter_set( pt, ter_id( "t_dirt" ) );
        here.furn_set( pt, furn_id( "f_null" ) );
    }

    // Move player out of the way
    get_avatar().setpos( tripoint( 10, 10, 0 ) );

    // Create two hostile NPCs from the same faction
    auto &shooter = spawn_npc( shooter_pos.xy(), "thug" );
    shooter.set_fac( faction_id( "hells_raiders" ) );
    shooter.set_attitude( NPCATT_KILL );

    auto &ally = spawn_npc( ally_pos.xy(), "thug" );
    ally.set_fac( faction_id( "hells_raiders" ) );
    ally.set_attitude( NPCATT_KILL );

    REQUIRE( g->critter_at( shooter_pos ) == &shooter );
    REQUIRE( g->critter_at( ally_pos ) == &ally );

    // Verify they're friendly to each other (same faction)
    REQUIRE( shooter.attitude_to( ally ) == Attitude::A_FRIENDLY );

    // Create a gun for the projectile
    auto gun_ptr = item::spawn( itype_id( "glock_19" ) );
    gun_ptr->ammo_set( itype_id( "9mm" ), 1 );
    item &gun = *gun_ptr;

    // Record initial HP of ally
    const int initial_ally_hp = ally.get_hp();

    // Create projectile from gun
    projectile test_proj;
    test_proj.speed = gun.gun_speed();
    test_proj.range = 10;
    test_proj.impact = gun.gun_damage();

    // Fire projectile toward target with high dispersion
    dealt_projectile_attack attack = projectile_attack(
                                         test_proj, shooter_pos, target_pos,
                                         dispersion_sources( 5000 ), &shooter, &gun );

    // Verify allied hostile NPC was not hit despite being adjacent to shooter
    const int final_ally_hp = ally.get_hp();
    CHECK( final_ally_hp == initial_ally_hp );

    // The projectile should not stop at the ally's position
    CHECK( attack.end_point != ally_pos );
}

TEST_CASE( "friendly_monster_iff_respects_adjacent_player", "[projectile][monster][iff]" )
{
    clear_all_state();
    map &here = get_map();

    // Set up test area: friendly monster at (0,0), player adjacent at (1,0), hostile at (2,0)
    // This tests that IFF checks still apply when player is adjacent to friendly monster
    const auto monster_pos = tripoint_zero;
    const auto player_pos = tripoint_east;
    const auto hostile_pos = tripoint( 2, 0, 0 );

    // Clear the area
    for( int x = 0; x <= 3; ++x ) {
        auto pt = tripoint( x, 0, 0 );
        REQUIRE( here.inbounds( pt ) );
        here.ter_set( pt, ter_id( "t_dirt" ) );
        here.furn_set( pt, furn_id( "f_null" ) );
    }

    // Create friendly monster
    monster &friendly_mon = spawn_test_monster( "mon_zombie", monster_pos );
    friendly_mon.friendly = 1;
    REQUIRE( g->critter_at( monster_pos ) == &friendly_mon );
    REQUIRE( friendly_mon.friendly != 0 );

    // Set up player at adjacent position
    auto &player = get_avatar();
    player.setpos( player_pos );
    player.set_body();
    REQUIRE( g->critter_at( player_pos ) == &player );

    // Verify monster is friendly to player
    REQUIRE( friendly_mon.attitude_to( player ) == Attitude::A_FRIENDLY );

    // Create hostile monster directly behind player (in line of fire)
    monster &hostile = spawn_test_monster( "mon_zombie_tough", hostile_pos );
    hostile.friendly = 0;
    REQUIRE( g->critter_at( hostile_pos ) == &hostile );

    // Attempt to find a hostile target - with player adjacent and in line of fire
    int boo_hoo = 0;
    Creature *target = friendly_mon.auto_find_hostile_target( 10, boo_hoo );

    // The key test: IFF should be active even when player is adjacent
    // Since player is directly in line of fire, IFF should prevent targeting the hostile
    // and increment boo_hoo to indicate friendly fire risk
    if( target == &hostile ) {
        // If the hostile was selected as target despite player being in line of fire,
        // boo_hoo should have been incremented (IFF warning)
        CHECK( boo_hoo > 0 );
    }
}
