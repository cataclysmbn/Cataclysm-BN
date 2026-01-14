#include "catch/catch.hpp"
#include "monster.h"
#include "mtype.h"
#include "avatar.h"
#include "game.h"
#include "map.h"
#include "item.h"
#include "bodypart.h"
#include "options.h"
#include "map_helpers.h"
#include "state_helpers.h"

TEST_CASE( "monster_faction_memory_anger", "[monster][faction][anger]" )
{
    clear_all_state();

    const tripoint tank_pos( 5, 5, 0 );
    const tripoint hazmat_pos( 7, 5, 0 );

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

    const tripoint tank_pos( 5, 5, 0 );
    const tripoint zombie_pos( 7, 5, 0 );

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

TEST_CASE( "monster_faction_memory_friend_attacked", "[monster][faction][anger]" )
{
    clear_all_state();

    const tripoint tank1_pos( 5, 5, 0 );
    const tripoint tank2_pos( 6, 5, 0 );
    const tripoint zombie_pos( 7, 5, 0 );

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
