#include "catch/catch.hpp"

#include "action_time_scale.h"
#include "activity_speed.h"
#include "avatar.h"
#include "character_effects.h"
#include "npc.h"
#include "options_helpers.h"
#include "player_activity.h"
#include "player_helpers.h"
#include "map.h"
#include "map_helpers.h"
#include "state_helpers.h"

static const trait_id trait_DEBUG_WEIGHTLESSNESS( "DEBUG_WEIGHTLESSNESS" );
static const auto act_wait = activity_id( "ACT_WAIT" );

static void advance_turn( Character &guy )
{
    guy.process_turn();
    calendar::turn += 1_turns;
}

static player &prepare_player()
{
    auto &guy = *get_player_character().as_player();
    clear_character( *guy.as_player(), true );
    guy.set_moves( 0 );

    advance_turn( guy );

    REQUIRE( guy.get_speed_base() == 100 );
    REQUIRE( guy.get_speed() == 100 );
    REQUIRE( guy.get_moves() == 100 );

    guy.set_moves( 0 );

    return guy;
}

TEST_CASE( "Character regains moves each turn", "[speed]" )
{
    clear_all_state();
    player &guy = prepare_player();

    advance_turn( guy );

    CHECK( guy.get_moves() == 100 );
}

TEST_CASE( "Player action scale modifies move gain", "[speed]" )
{
    clear_all_state();
    const auto global_scale = override_option( "TIME_ACTION_SCALE", "50" );
    const auto player_scale = override_option( "PLAYER_ACTION_SCALE", "50" );

    auto &guy = *get_player_character().as_player();
    clear_character( guy, true );
    guy.set_moves( 0 );

    advance_turn( guy );

    CHECK( guy.get_speed() == 100 );
    CHECK( guy.get_moves() == 25 );
}

TEST_CASE( "NPC action scale modifies move gain", "[speed][npc]" )
{
    clear_all_state();
    const auto global_scale = override_option( "TIME_ACTION_SCALE", "50" );
    const auto npc_scale = override_option( "NPC_ACTION_SCALE", "50" );

    auto guy = standard_npc( "action scale npc" );
    guy.set_moves( 0 );

    advance_turn( guy );

    CHECK( guy.get_speed() == 100 );
    CHECK( guy.get_moves() == 25 );
}

TEST_CASE( "Activity progress scale modifies non-complex activity progress", "[speed][activity]" )
{
    clear_all_state();
    const auto global_scale = override_option( "TIME_ACTION_SCALE", "50" );
    const auto activity_scale = override_option( "ACTIVITY_PROGRESS_SCALE", "50" );

    auto &guy = *get_player_character().as_player();
    clear_character( guy, true );
    guy.set_moves( 100 );
    guy.assign_activity( act_wait, 1000 );

    REQUIRE( guy.activity );
    REQUIRE( guy.activity->get_moves_left() == 1000 );

    guy.activity->do_turn( guy );

    CHECK( guy.activity->get_moves_left() == 975 );
    CHECK( guy.get_moves() == 0 );
}

TEST_CASE( "Activity progress scale modifies complex activity base progress", "[speed][activity]" )
{
    clear_all_state();
    const auto global_scale = override_option( "TIME_ACTION_SCALE", "50" );
    const auto activity_scale = override_option( "ACTIVITY_PROGRESS_SCALE", "50" );

    auto speed = activity_speed();
    CHECK( speed.moves_per_turn() == 25 );

    speed.player_speed = 2.0f;
    CHECK( speed.moves_per_turn() == 50 );
}

TEST_CASE( "Activity progress scale modifies progress time estimates", "[speed][activity]" )
{
    clear_all_state();
    const auto global_scale = override_option( "TIME_ACTION_SCALE", "50" );
    const auto activity_scale = override_option( "ACTIVITY_PROGRESS_SCALE", "50" );

    CHECK( action_time_scale::activity_turns_for_progress( 100 ) == 4 );
    CHECK( action_time_scale::turns_for_progress( 101, 25 ) == 5 );
    CHECK( action_time_scale::turns_for_progress( 0, 25 ) == 0 );
}

TEST_CASE( "Overmap horde scale modifies horde speed", "[speed][monster][horde]" )
{
    clear_all_state();
    const auto global_scale = override_option( "TIME_ACTION_SCALE", "50" );
    const auto monster_scale = override_option( "MONSTER_SPEED", "50" );
    const auto horde_scale = override_option( "OVERMAP_HORDE_SCALE", "50" );

    CHECK( action_time_scale::scaled_overmap_horde_speed( 100.0 ) == Approx( 12.5 ) );
}

TEST_CASE( "NPC activity catch-up uses activity progress scale", "[speed][activity][npc]" )
{
    clear_all_state();
    const auto global_scale = override_option( "TIME_ACTION_SCALE", "50" );
    const auto activity_scale = override_option( "ACTIVITY_PROGRESS_SCALE", "50" );

    auto guy = standard_npc( "activity catch-up npc" );
    guy.assign_activity( act_wait, 1000 );

    REQUIRE( guy.activity );
    REQUIRE( guy.activity->get_moves_left() == 1000 );

    guy.advance_job_progress( 10 );

    CHECK( guy.activity->get_moves_left() == 750 );
}

static void pain_penalty_test( player &guy, int pain, int speed_exp )
{
    int penalty = 100 - speed_exp;

    guy.set_pain( pain );
    REQUIRE( guy.get_pain() == pain );
    guy.set_painkiller( 0 );
    REQUIRE( guy.get_painkiller() == 0 );
    REQUIRE( guy.get_perceived_pain() == pain );
    REQUIRE( character_effects::get_pain_penalty( guy ).speed == penalty );

    advance_turn( guy );

    CHECK( guy.get_speed_bonus() == -penalty );
    CHECK( guy.get_speed() == speed_exp );
    CHECK( guy.get_moves() == speed_exp );
}

TEST_CASE( "Character is slowed down by pain", "[speed][pain]" )
{
    clear_all_state();
    player &guy = prepare_player();

    WHEN( "10 pain" ) {
        pain_penalty_test( guy, 10, 95 );
    }
    WHEN( "100 pain" ) {
        pain_penalty_test( guy, 100, 75 );
    }
    WHEN( "300 pain" ) {
        pain_penalty_test( guy, 300, 70 );
    }
}

static void carry_weight_test( Character &guy, int load_kg, int speed_exp )
{
    item &item_1kg = *item::spawn_temporary( "test_1kg_cube" );
    REQUIRE( item_1kg.weight() == 1_kilogram );
    REQUIRE( item_1kg.volume() == 10_ml );

    CAPTURE( load_kg, speed_exp );
    WHEN( "Character carries specified weight" ) {
        for( int i = 0; i < load_kg; i++ ) {
            guy.i_add( item::spawn( item_1kg ) );
        }
        THEN( "No effect on speed" ) {
            CHECK( guy.get_speed() == 100 );
            AND_WHEN( "Turn passes" ) {
                advance_turn( guy );
                REQUIRE( guy.weight_carried() == 1_kilogram * load_kg + 633_gram );
                REQUIRE( guy.weight_capacity() == 45_kilogram );
                THEN( "Speed matches expected value" ) {
                    CHECK( guy.get_speed() == speed_exp );
                }
            }
        }
    }
}

static auto equip_carried_weight( Character &guy ) -> units::mass
{
    auto backpack = item::spawn( "test_backpack" );
    auto inventory_item = item::spawn( "test_1kg_cube" );
    auto wielded_item = item::spawn( "test_1kg_cube" );

    const auto carried_weight = backpack->weight() + inventory_item->weight() + wielded_item->weight();

    guy.wear_item( std::move( backpack ), false );
    guy.i_add( std::move( inventory_item ) );
    guy.wield( std::move( wielded_item ) );

    return carried_weight;
}

TEST_CASE( "Character is slowed down while overburdened", "[speed]" )
{
    clear_all_state();
    player &guy = prepare_player();

    detached_ptr<item> backpack = item::spawn( "test_backpack" );
    REQUIRE( backpack->get_storage() == 15_liter );
    REQUIRE( backpack->weight() == 633_gram );

    guy.clear_mutations();
    guy.wear_item( std::move( backpack ), false );
    REQUIRE( guy.weight_capacity() == 45_kilogram );
    REQUIRE( guy.volume_capacity() == 15_liter );

    SECTION( "Carry weight under carry capacity" ) {
        // 94%, no penalty
        carry_weight_test( guy, 42, 100 );
    }
    SECTION( "Carry weight over carry capacity" ) {
        // 148% gives -12 speed (25 * 0.48)
        carry_weight_test( guy, 66, 88 );
    }
    SECTION( "Carry weight significantly over carry capacity" ) {
        // 228% gives -32 speed (25 * 1.28)
        carry_weight_test( guy, 102, 68 );
    }
}

TEST_CASE( "Debug weightlessness ignores carried item weight", "[speed][debug]" )
{
    clear_all_state();
    player &guy = *get_player_character().as_player();

    SECTION( "Normal carrying adds to character weight" ) {
        clear_character( guy, false );
        const auto base_weight = guy.get_weight();
        const auto carried_weight = equip_carried_weight( guy );

        CHECK( guy.get_weight() - base_weight == carried_weight );
    }

    SECTION( "Debug carrying capacity still adds to character weight" ) {
        clear_character( guy, true );
        const auto base_weight = guy.get_weight();
        const auto carried_weight = equip_carried_weight( guy );

        CHECK( guy.get_weight() - base_weight == carried_weight );
    }

    SECTION( "Debug weightlessness zeroes character weight" ) {
        clear_character( guy, true );
        guy.set_mutation( trait_DEBUG_WEIGHTLESSNESS );
        const auto carried_weight = equip_carried_weight( guy );
        CAPTURE( carried_weight );

        CHECK( guy.get_weight() == 0_gram );
    }
}
