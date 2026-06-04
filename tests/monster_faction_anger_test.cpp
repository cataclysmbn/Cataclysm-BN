#include "catch/catch.hpp"
#include "monster.h"
#include "mtype.h"
#include "monfaction.h"
#include "npc.h"
#include "avatar.h"
#include "game.h"
#include "map.h"
#include "item.h"
#include "bodypart.h"
#include "options.h"
#include "map_helpers.h"
#include "state_helpers.h"

TEST_CASE( "monster_faction_attitude_to_npc_faction", "[monster][faction][npc]" )
{
    clear_all_state();

    const auto pos = tripoint_bub_ms( 5, 5, 0 );
    auto &turret = spawn_test_monster( "mon_robofac_turret_light", pos );
    auto guard = npc();
    guard.set_fac( faction_id( "robofac" ) );

    CHECK( turret.attitude( &guard ) == MATT_FRIEND );
    CHECK( turret.generic_npc_attitude_to( guard.get_monster_faction() ) == Attitude::A_FRIENDLY );
}

TEST_CASE( "unauthorized_robofac_turrets_attack_player", "[monster][faction]" )
{
    clear_all_state();

    auto &you = get_avatar();
    you.remove_value( "npctalk_var_dialogue_intercom_completed_robofac_intercom_3" );
    auto &turret = spawn_test_monster( "mon_robofac_turret_light", tripoint_bub_ms( 5, 5, 0 ) );
    put_player_underground();

    CHECK( turret.attitude( &you ) == MATT_ATTACK );
}

TEST_CASE( "authorized_robofac_turrets_use_faction_attitude", "[monster][faction][npc]" )
{
    clear_all_state();

    const auto robofac_pos = tripoint_bub_ms( 5, 5, 0 );
    const auto authorized_pos = tripoint_bub_ms( 7, 5, 0 );
    auto &robofac_turret = spawn_test_monster( "mon_robofac_turret_light", robofac_pos );
    auto &authorized_turret = spawn_test_monster( "mon_robofac_turret_light", authorized_pos );
    authorized_turret.faction = mfaction_id( "robofac_authorized" );
    authorized_turret.friendly = 0;

    auto &you = get_avatar();
    put_player_underground();

    CHECK( authorized_turret.attitude( &you ) == MATT_FRIEND );
    CHECK( authorized_turret.attitude_to( robofac_turret ) == Attitude::A_FRIENDLY );
    CHECK( robofac_turret.attitude_to( authorized_turret ) == Attitude::A_FRIENDLY );

    authorized_turret.apply_damage( &you, bodypart_id( "torso" ), 27 );

    CHECK( authorized_turret.get_faction_anger( mfaction_id( "player" ) ) >= 10 );
    CHECK( authorized_turret.attitude( &you ) == MATT_ATTACK );
}

TEST_CASE( "authorized_robofac_turrets_alert_visible_security", "[monster][faction][npc]" )
{
    clear_all_state();

    const auto attacked_pos = tripoint_bub_ms( 10, 10, 0 );
    const auto east_pos = tripoint_bub_ms( 12, 10, 0 );
    const auto south_pos = tripoint_bub_ms( 10, 12, 0 );
    const auto southeast_pos = tripoint_bub_ms( 12, 12, 0 );
    auto &attacked_turret = spawn_test_monster( "mon_robofac_turret_light", attacked_pos );
    auto &east_turret = spawn_test_monster( "mon_robofac_turret_light", east_pos );
    auto &south_turret = spawn_test_monster( "mon_robofac_turret_light", south_pos );
    auto &southeast_turret = spawn_test_monster( "mon_robofac_turret_light", southeast_pos );

    for( monster *security : { &attacked_turret, &east_turret, &south_turret, &southeast_turret } ) {
        security->faction = mfaction_id( "robofac_authorized" );
        security->friendly = 0;
    }

    auto &you = get_avatar();
    put_player_underground();
    const auto player_faction = mfaction_id( "player" );

    REQUIRE( east_turret.sees( attacked_turret ) );
    REQUIRE( south_turret.sees( attacked_turret ) );
    REQUIRE( southeast_turret.sees( attacked_turret ) );
    CHECK( east_turret.attitude( &you ) == MATT_FRIEND );
    CHECK( south_turret.attitude( &you ) == MATT_FRIEND );
    CHECK( southeast_turret.attitude( &you ) == MATT_FRIEND );

    attacked_turret.on_hit( &you, bodypart_id( "torso" ), nullptr, false );

    CHECK( east_turret.get_faction_anger( player_faction ) >= 10 );
    CHECK( south_turret.get_faction_anger( player_faction ) >= 10 );
    CHECK( southeast_turret.get_faction_anger( player_faction ) >= 10 );
    CHECK( east_turret.attitude( &you ) == MATT_ATTACK );
    CHECK( south_turret.attitude( &you ) == MATT_ATTACK );
    CHECK( southeast_turret.attitude( &you ) == MATT_ATTACK );
}

TEST_CASE( "authorized_robofac_turrets_defend_hub_guards", "[monster][faction][npc]" )
{
    clear_all_state();

    const auto turret_pos = tripoint_bub_ms( 5, 5, 0 );
    const auto guard_pos = tripoint_bub_ms( 6, 5, 0 );
    auto &authorized_turret = spawn_test_monster( "mon_robofac_turret_light", turret_pos );
    authorized_turret.faction = mfaction_id( "robofac_authorized" );
    authorized_turret.friendly = 0;

    auto guard = npc();
    guard.set_fake( true );
    guard.set_fac( faction_id( "robofac_auxiliaries" ) );
    guard.set_attitude( NPCATT_NULL );
    guard.setpos( guard_pos );

    auto &you = get_avatar();
    CHECK( authorized_turret.attitude( &you ) == MATT_FRIEND );
    CHECK( guard.get_monster_faction() == mfaction_id( "robofac" ) );
    CHECK( authorized_turret.generic_npc_attitude_to( guard.get_monster_faction() ) ==
           Attitude::A_FRIENDLY );

    guard.on_attacked( you );

    auto monster_count = 0;
    for( monster &critter : g->all_monsters() ) {
        static_cast<void>( critter );
        monster_count++;
    }

    CHECK( monster_count == 1 );
    CHECK( guard.get_attitude() == NPCATT_KILL );
    CHECK( authorized_turret.get_faction_anger( mfaction_id( "player" ) ) >= 10 );
    CHECK( authorized_turret.attitude( &you ) == MATT_ATTACK );
}

TEST_CASE( "monster_faction_memory_anger", "[monster][faction][anger]" )
{
    clear_all_state();

    const tripoint_bub_ms tank_pos( 5, 5, 0 );
    const tripoint_bub_ms hazmat_pos( 7, 5, 0 );

    monster &tank = spawn_test_monster( "mon_tankbot", tank_pos );
    monster &hazmat = spawn_test_monster( "mon_eyebot", hazmat_pos );

    avatar &p = get_avatar();
    put_player_underground();
    tank.friendly = 0;
    tank.anger = 0;
    // We can't clear faction_anger directly as it is private, but a new monster should be clean.

    // Tank should be neutral to player initially (aggression 0)
    CHECK( tank.attitude( &p ) == MATT_IGNORE );

    // Tank should be neutral to hazmat bot
    CHECK( tank.attitude_to( hazmat ) == Attitude::A_NEUTRAL );

    // Player attacks tank
    // Deal enough damage to trigger anger (needs >= 10 anger to become hostile)
    // HURT trigger adds 1 + (dam / 3).
    // 30 damage -> 1 + 10 = 11 anger.
    tank.apply_damage( &p, bodypart_id( "torso" ), 30 );

    // Tank should now be angry at player faction
    CHECK( tank.get_faction_anger( mfaction_id( "player" ) ) >= 10 );

    // Tank should be hostile to player
    CHECK( tank.attitude( &p ) == MATT_ATTACK );

    // Tank should STILL be neutral to hazmat bot (anger shouldn't spill over)
    CHECK( tank.attitude_to( hazmat ) == Attitude::A_NEUTRAL );

    // Verify that global anger didn't increase
    // Because FACTION_MEMORY redirects anger to the specific faction
    CHECK( tank.anger == 0 );
}

TEST_CASE( "monster_faction_memory_zombie_attacks_tank", "[monster][faction][anger]" )
{
    clear_all_state();

    const tripoint_bub_ms tank_pos( 5, 5, 0 );
    const tripoint_bub_ms zombie_pos( 7, 5, 0 );

    monster &tank = spawn_test_monster( "mon_tankbot", tank_pos );
    monster &zombie = spawn_test_monster( "mon_zombie", zombie_pos );

    avatar &p = get_avatar();
    put_player_underground();

    // Ensure initial state
    tank.friendly = 0;
    tank.anger = 0;

    // Tank should be neutral to player initially
    CHECK( tank.attitude( &p ) == MATT_IGNORE );

    // Zombie attacks tank (deals 30 damage to trigger anger >= 10)
    // HURT trigger adds 1 + (dam / 3) = 1 + 10 = 11 anger
    tank.apply_damage( &zombie, bodypart_id( "torso" ), 30 );

    // Tank should now be angry at zombie faction
    CHECK( tank.get_faction_anger( zombie.faction ) >= 10 );

    // Tank should be hostile to zombie
    CHECK( tank.attitude_to( zombie ) == Attitude::A_HOSTILE );

    // Tank should STILL be neutral to player (anger shouldn't spill over)
    CHECK( tank.attitude( &p ) == MATT_IGNORE );

    // Verify that global anger didn't increase
    CHECK( tank.anger == 0 );
}

TEST_CASE( "generic_npc_attitude_uses_monster_faction_relations", "[monster][faction][npc]" )
{
    clear_all_state();

    const auto pos = tripoint_bub_ms( 5, 5, 0 );
    auto &critter = spawn_test_monster( "mon_zombie", pos );
    critter.friendly = 0;
    critter.anger = 0;
    critter.morale = 0;

    SECTION( "friendly faction" ) {
        critter.faction = mfaction_id( "robofac_authorized" );
        CHECK( critter.generic_npc_attitude_to( mfaction_id( "player" ) ) == Attitude::A_FRIENDLY );
    }

    SECTION( "neutral faction" ) {
        critter.faction = mfaction_id( "mech_bot" );
        CHECK( critter.generic_npc_attitude_to( mfaction_id( "player" ) ) == Attitude::A_NEUTRAL );
    }

    SECTION( "hated faction" ) {
        critter.faction = mfaction_id( "insect" );
        REQUIRE( critter.faction.obj().attitude( mfaction_id( "zombie" ) ) == MFA_HATE );
        CHECK( critter.generic_npc_attitude_to( mfaction_id( "zombie" ) ) == Attitude::A_HOSTILE );
    }
}

TEST_CASE( "monster_faction_memory_friend_attacked", "[monster][faction][anger]" )
{
    clear_all_state();

    const tripoint_bub_ms tank1_pos( 5, 5, 0 );
    const tripoint_bub_ms tank2_pos( 6, 5, 0 );
    const tripoint_bub_ms zombie_pos( 7, 5, 0 );

    monster &tank1 = spawn_test_monster( "mon_tankbot", tank1_pos );
    monster &tank2 = spawn_test_monster( "mon_tankbot", tank2_pos );
    monster &zombie = spawn_test_monster( "mon_zombie", zombie_pos );

    avatar &p = get_avatar();
    put_player_underground();

    // Ensure initial state
    tank1.friendly = 0;
    tank1.anger = 0;
    tank2.friendly = 0;
    tank2.anger = 0;

    // Both tanks should be neutral to player initially
    CHECK( tank1.attitude( &p ) == MATT_IGNORE );
    CHECK( tank2.attitude( &p ) == MATT_IGNORE );

    // Zombie attacks tank1 (deals 30 damage)
    // This should trigger FRIEND_ATTACKED for tank2
    tank1.on_hit( &zombie, bodypart_id( "torso" ), nullptr, false );

    // Tank2 should be angry at zombie faction due to FRIEND_ATTACKED trigger
    CHECK( tank2.get_faction_anger( zombie.faction ) > 0 );

    // Tank2 should NOT be angry at player faction
    CHECK( tank2.get_faction_anger( mfaction_id( "player" ) ) == 0 );

    // Tank2 should STILL be neutral to player
    CHECK( tank2.attitude( &p ) == MATT_IGNORE );

    // Tank2's global anger should remain 0
    CHECK( tank2.anger == 0 );
}
