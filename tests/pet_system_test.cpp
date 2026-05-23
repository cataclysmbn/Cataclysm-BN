#include "catch/catch.hpp"

#include <cmath>
#include <string>

#include "activity_handlers.h"
#include "avatar.h"
#include "calendar.h"
#include "game.h"
#include "map_helpers.h"
#include "memory_fast.h"
#include "monster.h"
#include "mtype.h"
#include "player_activity.h"
#include "point.h"
#include "skill.h"
#include "state_helpers.h"
#include "type_id.h"

static const activity_id ACT_TRAIN_PET( "ACT_TRAIN_PET" );

static const efftype_id effect_ai_waiting( "ai_waiting" );
static const efftype_id effect_pet( "pet" );
static const efftype_id effect_well_fed( "well_fed" );

static const mtype_id mon_dog( "mon_dog" );

static const skill_id skill_survival( "survival" );

// A position well away from the player's underground test location so
// the monster's line-of-sight checks do not interfere with tests.
static const tripoint_bub_ms mon_pos( 10, 10, 0 );

// -----------------------------------------------------------------------
// make_pet / is_pet
// -----------------------------------------------------------------------

TEST_CASE( "make_pet sets friendly to -1", "[pet][monster]" )
{
    clear_all_state();
    monster &mon = spawn_test_monster( mon_dog.str(), mon_pos );

    REQUIRE( mon.friendly == 0 );
    mon.make_pet();
    CHECK( mon.friendly == -1 );
}

TEST_CASE( "make_pet adds effect_pet", "[pet][monster]" )
{
    clear_all_state();
    monster &mon = spawn_test_monster( mon_dog.str(), mon_pos );

    REQUIRE_FALSE( mon.has_effect( effect_pet ) );
    mon.make_pet();
    CHECK( mon.has_effect( effect_pet ) );
}

TEST_CASE( "is_pet returns true only when both friendly==-1 and effect_pet present",
           "[pet][monster]" )
{
    clear_all_state();
    monster &mon = spawn_test_monster( mon_dog.str(), mon_pos );

    SECTION( "unfriendly monster is not a pet" ) {
        mon.friendly = 0;
        CHECK_FALSE( mon.is_pet() );
    }

    SECTION( "make_friendly monster (friendly > 0) is not a pet" ) {
        mon.make_friendly();
        REQUIRE( mon.friendly > 0 );
        CHECK_FALSE( mon.is_pet() );
    }

    SECTION( "friendly=-1 without effect_pet is not a pet" ) {
        mon.friendly = -1;
        REQUIRE_FALSE( mon.has_effect( effect_pet ) );
        CHECK_FALSE( mon.is_pet() );
    }

    SECTION( "make_pet produces a true pet" ) {
        mon.make_pet();
        CHECK( mon.is_pet() );
    }
}


TEST_CASE( "make_pet does not reset training_level", "[pet][monster][training]" )
{
    clear_all_state();
    monster &mon = spawn_test_monster( mon_dog.str(), mon_pos );

    mon.make_pet();
    mon.training_level = 2;

    // Calling make_pet again (e.g. re-feeding) must preserve training_level
    mon.make_pet();
    CHECK( mon.training_level == 2 );
}

TEST_CASE( "repeated make_pet calls preserve is_pet status", "[pet][monster]" )
{
    clear_all_state();
    monster &mon = spawn_test_monster( mon_dog.str(), mon_pos );

    mon.make_pet();
    REQUIRE( mon.is_pet() );

    mon.make_pet();
    CHECK( mon.is_pet() );
}

// -----------------------------------------------------------------------
// effect_well_fed
// -----------------------------------------------------------------------

TEST_CASE( "adding effect_well_fed to a pet works", "[pet][monster]" )
{
    clear_all_state();
    monster &mon = spawn_test_monster( mon_dog.str(), mon_pos );
    mon.make_pet();

    REQUIRE_FALSE( mon.has_effect( effect_well_fed ) );
    mon.add_effect( effect_well_fed, 24_hours );
    CHECK( mon.has_effect( effect_well_fed ) );
}

TEST_CASE( "effect_well_fed duration is refreshed on re-add", "[pet][monster]" )
{
    clear_all_state();
    monster &mon = spawn_test_monster( mon_dog.str(), mon_pos );
    mon.make_pet();

    mon.add_effect( effect_well_fed, 1_hours );
    const time_duration first = mon.get_effect_dur( effect_well_fed );

    mon.add_effect( effect_well_fed, 24_hours );
    const time_duration second = mon.get_effect_dur( effect_well_fed );

    CHECK( second > first );
}

// -----------------------------------------------------------------------
// Training stat scaling
// -----------------------------------------------------------------------

TEST_CASE( "pet training stat scaling with training_level", "[pet][monster][training]" )
{
    clear_all_state();
    monster &mon = spawn_test_monster( mon_dog.str(), mon_pos );
    mon.make_pet();
    REQUIRE( mon.type->pet_training.has_value() );

    const int base_hp     = mon.type->hp;
    const float base_melee = mon.type->melee_skill;
    const float base_dodge = mon.type->sk_dodge;
    const float hp_mult    = mon.type->pet_training->hp;
    const float melee_mult = mon.type->pet_training->melee;
    const float dodge_mult = mon.type->pet_training->dodge;

    SECTION( "level 0: stats equal base" ) {
        mon.training_level = 0;
        CHECK( mon.get_hp_max() == base_hp );
        CHECK( mon.get_melee() == Approx( base_melee ) );
        CHECK( mon.get_dodge_base() == Approx( base_dodge ) );
    }

    SECTION( "level 1: stats scaled by multiplier^1" ) {
        mon.training_level = 1;
        CHECK( mon.get_hp_max() == static_cast<int>( base_hp * std::pow( hp_mult, 1 ) ) );
        CHECK( mon.get_melee() == Approx( base_melee * std::pow( melee_mult, 1 ) ) );
        CHECK( mon.get_dodge_base() == Approx( base_dodge * std::pow( dodge_mult, 1 ) ) );
    }

    SECTION( "level 2: stats scaled by multiplier^2" ) {
        mon.training_level = 2;
        CHECK( mon.get_hp_max() == static_cast<int>( base_hp * std::pow( hp_mult, 2 ) ) );
        CHECK( mon.get_melee() == Approx( base_melee * std::pow( melee_mult, 2 ) ) );
        CHECK( mon.get_dodge_base() == Approx( base_dodge * std::pow( dodge_mult, 2 ) ) );
    }

    SECTION( "higher level gives better stats than lower level" ) {
        mon.training_level = 1;
        const int hp1 = mon.get_hp_max();
        mon.training_level = 2;
        const int hp2 = mon.get_hp_max();
        CHECK( hp2 > hp1 );
    }
}

TEST_CASE( "training_level is capped at max_level", "[pet][monster][training]" )
{
    clear_all_state();
    monster &mon = spawn_test_monster( mon_dog.str(), mon_pos );
    mon.make_pet();
    REQUIRE( mon.type->pet_training.has_value() );

    const int max_level = mon.type->pet_training->max_level;
    mon.training_level = max_level;

    const int hp_at_max = mon.get_hp_max();
    const int base_hp   = mon.type->hp;
    CHECK( hp_at_max > base_hp );

    // Going above max_level in the raw field should not happen, but if it
    // did the formula would scale — verify it's not capped by the getter.
    // The capping is enforced in train_pet_finish, not in get_hp_max.
    mon.training_level = max_level + 1;
    CHECK( mon.get_hp_max() > hp_at_max );
}

// -----------------------------------------------------------------------
// train_pet_finish
// -----------------------------------------------------------------------

TEST_CASE( "train_pet_finish with null monster reference does not crash",
           "[pet][monster][training][activity]" )
{
    clear_all_state();
    avatar &p = get_avatar();

    player_activity act( ACT_TRAIN_PET );
    // Push an expired (default-constructed, never-assigned) weak_ptr so
    // lock() will return nullptr, exercising the null-guard we added.
    act.monsters.push_back( weak_ptr_fast<monster> {} );
    act.str_values.push_back( "test_pet" );

    // Must not crash and must set the activity to null.
    activity_handlers::train_pet_finish( &act, &p );
    CHECK( act.is_null() );
}

TEST_CASE( "train_pet_finish removes effect_well_fed", "[pet][monster][training][activity]" )
{
    clear_all_state();
    monster &mon = spawn_test_monster( mon_dog.str(), mon_pos );
    mon.make_pet();
    mon.add_effect( effect_well_fed, 24_hours );
    REQUIRE( mon.has_effect( effect_well_fed ) );

    avatar &p = get_avatar();
    put_player_underground();
    // Ensure skill meets the minimum so we don't early-return before consuming well_fed.
    REQUIRE( mon.type->pet_training.has_value() );
    p.set_skill_level( skill_survival, mon.type->pet_training->min_skill );

    player_activity act( ACT_TRAIN_PET );
    act.monsters.push_back( g->shared_from( mon ) );
    act.str_values.push_back( mon.get_name() );

    activity_handlers::train_pet_finish( &act, &p );
    CHECK_FALSE( mon.has_effect( effect_well_fed ) );
}

TEST_CASE( "train_pet_finish increments training_level on success",
           "[pet][monster][training][activity]" )
{
    clear_all_state();
    monster &mon = spawn_test_monster( mon_dog.str(), mon_pos );
    mon.make_pet();
    mon.add_effect( effect_well_fed, 24_hours );
    mon.training_level = 0;

    avatar &p = get_avatar();
    put_player_underground();
    // Skill 25 guarantees success: 4*25=100 >= rng(0,100) always.
    p.set_skill_level( skill_survival, 25 );

    player_activity act( ACT_TRAIN_PET );
    act.monsters.push_back( g->shared_from( mon ) );
    act.str_values.push_back( mon.get_name() );

    activity_handlers::train_pet_finish( &act, &p );
    CHECK( mon.training_level == 1 );
}

TEST_CASE( "train_pet_finish does not exceed max_level", "[pet][monster][training][activity]" )
{
    clear_all_state();
    monster &mon = spawn_test_monster( mon_dog.str(), mon_pos );
    mon.make_pet();
    REQUIRE( mon.type->pet_training.has_value() );

    const int max_level = mon.type->pet_training->max_level;
    mon.training_level  = max_level;
    mon.add_effect( effect_well_fed, 24_hours );

    avatar &p = get_avatar();
    put_player_underground();
    p.set_skill_level( skill_survival, 25 );

    player_activity act( ACT_TRAIN_PET );
    act.monsters.push_back( g->shared_from( mon ) );
    act.str_values.push_back( mon.get_name() );

    activity_handlers::train_pet_finish( &act, &p );
    CHECK( mon.training_level == max_level );
}

TEST_CASE( "train_pet_finish with insufficient skill does not increment training_level",
           "[pet][monster][training][activity]" )
{
    clear_all_state();
    monster &mon = spawn_test_monster( mon_dog.str(), mon_pos );
    mon.make_pet();
    mon.add_effect( effect_well_fed, 24_hours );
    mon.training_level = 0;

    avatar &p = get_avatar();
    put_player_underground();
    REQUIRE( mon.type->pet_training.has_value() );
    // Set skill below the minimum required for this pet.
    p.set_skill_level( skill_survival, mon.type->pet_training->min_skill - 1 );

    player_activity act( ACT_TRAIN_PET );
    act.monsters.push_back( g->shared_from( mon ) );
    act.str_values.push_back( mon.get_name() );

    activity_handlers::train_pet_finish( &act, &p );
    // Should return early without touching training_level or well_fed.
    CHECK( mon.training_level == 0 );
    CHECK( mon.has_effect( effect_well_fed ) );
}

TEST_CASE( "training_level is preserved after re-feeding (make_pet called again)",
           "[pet][monster][training]" )
{
    clear_all_state();
    monster &mon = spawn_test_monster( mon_dog.str(), mon_pos );
    mon.make_pet();
    mon.training_level = 2;

    // Simulate re-feeding: make_pet is called again.
    mon.make_pet();

    CHECK( mon.training_level == 2 );
    CHECK( mon.is_pet() );
}

TEST_CASE( "train_pet_finish removes effect_ai_waiting so pet is not stuck after training",
           "[pet][monster][training][activity]" )
{
    clear_all_state();
    monster &mon = spawn_test_monster( mon_dog.str(), mon_pos );
    mon.make_pet();
    mon.add_effect( effect_well_fed, 24_hours );
    // Simulate what monexamine::train_pet does when the activity starts.
    mon.add_effect( effect_ai_waiting, 60_minutes );
    REQUIRE( mon.has_effect( effect_ai_waiting ) );

    avatar &p = get_avatar();
    put_player_underground();
    REQUIRE( mon.type->pet_training.has_value() );
    p.set_skill_level( skill_survival, mon.type->pet_training->min_skill );

    player_activity act( ACT_TRAIN_PET );
    act.monsters.push_back( g->shared_from( mon ) );
    act.str_values.push_back( mon.get_name() );

    activity_handlers::train_pet_finish( &act, &p );
    CHECK_FALSE( mon.has_effect( effect_ai_waiting ) );
}
