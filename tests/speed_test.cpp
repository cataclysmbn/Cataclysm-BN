#include "catch/catch.hpp"

#include <ranges>

#include "action_time_scale.h"
#include "activity_handlers.h"
#include "activity_speed.h"
#include "avatar.h"
#include "calendar.h"
#include "character_effects.h"
#include "character_functions.h"
#include "npc.h"
#include "options_helpers.h"
#include "player_activity.h"
#include "player_helpers.h"
#include "map.h"
#include "map_helpers.h"
#include "state_helpers.h"

static const trait_id trait_DEBUG_WEIGHTLESSNESS( "DEBUG_WEIGHTLESSNESS" );
static const efftype_id effect_sleep( "sleep" );
static const efftype_id effect_debug_clairvoyance( "debug_clairvoyance" );
static const auto act_repair_item = activity_id( "ACT_REPAIR_ITEM" );
static const auto act_socialize = activity_id( "ACT_SOCIALIZE" );
static const auto act_try_sleep = activity_id( "ACT_TRY_SLEEP" );
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
    CHECK( guy.get_moves() == 50 );
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
    CHECK( guy.get_moves() == 50 );
}

TEST_CASE( "Activity progress scale modifies non-complex activity progress", "[speed][activity]" )
{
    clear_all_state();
    const auto global_scale = override_option( "TIME_ACTION_SCALE", "50" );
    const auto activity_scale = override_option( "ACTIVITY_PROGRESS_SCALE", "50" );

    auto &guy = *get_player_character().as_player();
    clear_character( guy, true );
    guy.set_moves( 100 );
    guy.assign_activity( act_socialize, 1000 );

    REQUIRE( guy.activity );
    REQUIRE( guy.activity->get_moves_left() == 1000 );

    guy.activity->do_turn( guy );

    CHECK( guy.activity->get_moves_left() == 950 );
    CHECK( guy.get_moves() == 0 );
}

TEST_CASE( "Activity progress scale modifies complex activity base progress", "[speed][activity]" )
{
    clear_all_state();
    const auto global_scale = override_option( "TIME_ACTION_SCALE", "50" );
    const auto activity_scale = override_option( "ACTIVITY_PROGRESS_SCALE", "50" );

    auto speed = activity_speed();
    CHECK( speed.moves_per_turn() == 50 );
    CHECK( speed.calendar_moves_per_turn() == 25 );

    speed.player_speed = 2.0f;
    CHECK( speed.moves_per_turn() == 100 );
    CHECK( speed.calendar_moves_per_turn() == 50 );
}

TEST_CASE( "Calendar wait progress consumes elapsed tick duration", "[speed][activity][wait]" )
{
    clear_all_state();

    auto &guy = *get_player_character().as_player();
    clear_character( guy, true );
    guy.set_moves( 100 );
    guy.assign_activity( act_wait, to_moves<int>( 1_minutes ) );
    const auto tick_scope = action_time_scale::scoped_calendar_turns_this_tick( 10 );

    REQUIRE( guy.activity );
    REQUIRE( guy.activity->get_moves_left() == to_moves<int>( 1_minutes ) );

    guy.activity->do_turn( guy );

    CHECK( guy.activity->get_moves_left() == to_moves<int>( 50_seconds ) );
    CHECK( guy.get_moves() == 0 );
}

TEST_CASE( "Trying to sleep progress consumes elapsed tick duration", "[speed][activity][sleep]" )
{
    clear_all_state();

    auto &guy = *get_player_character().as_player();
    clear_character( guy, true );
    guy.add_effect( effect_sleep, 1_minutes );
    guy.set_moves( 100 );
    guy.assign_activity( act_try_sleep, to_moves<int>( 1_minutes ) );
    const auto tick_scope = action_time_scale::scoped_calendar_turns_this_tick( 10 );

    REQUIRE( guy.activity );
    REQUIRE( guy.activity->get_moves_left() == to_moves<int>( 1_minutes ) );

    guy.activity->do_turn( guy );

    CHECK( guy.activity->get_moves_left() == to_moves<int>( 50_seconds ) );
    CHECK( guy.get_moves() == 0 );
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

TEST_CASE( "Activity progress conversion uses realized actor move budget", "[speed][activity]" )
{
    clear_all_state();
    const auto global_scale = override_option( "TIME_ACTION_SCALE", "33" );
    const auto player_scale = override_option( "PLAYER_ACTION_SCALE", "101" );
    const auto activity_scale = override_option( "ACTIVITY_PROGRESS_SCALE", "100" );

    const auto actor_factor = action_time_scale::player_tick_action_factor();
    const auto actor_moves = action_time_scale::scaled_moves(
                                 action_time_scale::base_moves_per_turn, actor_factor );

    REQUIRE( actor_moves == 101 );
    REQUIRE( action_time_scale::activity_progress_per_tick() == 100 );
    REQUIRE( action_time_scale::activity_progress_per_calendar_turn() == 33 );

    CHECK( action_time_scale::activity_progress_from_actor_moves( actor_moves,
            actor_factor ) == Approx( 100.0 ) );
    CHECK( action_time_scale::actor_moves_for_activity_progress( 50.0, actor_factor ) == 51 );
}

TEST_CASE( "Global action scale below 100 advances calendar by larger ticks", "[speed]" )
{
    clear_all_state();
    const auto global_scale = override_option( "TIME_ACTION_SCALE", "33" );

    auto remainder = 0;
    auto elapsed_turns = 0;
    for( const auto unused : std::views::iota( 0, 33 ) ) {
        static_cast<void>( unused );
        elapsed_turns += action_time_scale::calendar_turns_for_next_tick( remainder );
    }

    CHECK( elapsed_turns == 100 );
    CHECK( remainder == 0 );

    remainder = 1;
    const auto peeked_turns = action_time_scale::calendar_turns_for_remainder( remainder );
    CHECK( remainder == 1 );
    CHECK( action_time_scale::calendar_turns_for_next_tick( remainder ) == peeked_turns );
}

TEST_CASE( "Calendar tick reset exposes the next global action turn", "[speed]" )
{
    clear_all_state();
    const auto global_scale = override_option( "TIME_ACTION_SCALE", "20" );

    action_time_scale::set_calendar_turns_this_tick( 1 );
    action_time_scale::set_calendar_turns_this_tick_to_next_tick( 0 );

    CHECK( action_time_scale::calendar_turns_this_tick() == 5 );
    CHECK( action_time_scale::calendar_duration_this_tick() == 5_turns );

    action_time_scale::set_calendar_turns_this_tick( 1 );
}

TEST_CASE( "Calendar tick helpers expose elapsed turn boundaries", "[speed]" )
{
    clear_all_state();

    calendar::turn = calendar::turn_zero + 12_turns;
    const auto tick_scope = action_time_scale::scoped_calendar_turns_this_tick( 3 );

    CHECK( action_time_scale::calendar_duration_this_tick() == 3_turns );
    CHECK( action_time_scale::calendar_ticks_crossed_this_tick( 10_turns ) == 1 );
    CHECK( action_time_scale::once_every_this_tick( 10_turns ) );
    CHECK_FALSE( action_time_scale::once_every_this_tick( 7_turns ) );
}

TEST_CASE( "Effect decay consumes elapsed calendar turns", "[speed][effect]" )
{
    clear_all_state();

    auto &guy = *get_player_character().as_player();
    clear_character( guy, true );
    guy.add_effect( effect_debug_clairvoyance, 1_minutes );
    const auto tick_scope = action_time_scale::scoped_calendar_turns_this_tick( 10 );

    guy.process_effects();

    CHECK( to_turns<int>( guy.get_effect_dur( effect_debug_clairvoyance ) ) == 50 );
}

TEST_CASE( "Repair item progress uses activity scale", "[speed][activity][repair]" )
{
    clear_all_state();
    const auto global_scale = override_option( "TIME_ACTION_SCALE", "50" );
    const auto player_scale = override_option( "PLAYER_ACTION_SCALE", "50" );
    const auto activity_scale = override_option( "ACTIVITY_PROGRESS_SCALE", "100" );

    auto &guy = *get_player_character().as_player();
    clear_character( guy, true );
    guy.add_effect( effect_debug_clairvoyance, 1_minutes );
    REQUIRE( character_funcs::fine_detail_vision_mod( guy ) == Approx( 1.0f ) );

    guy.set_moves( 25 );
    auto repair_activity = player_activity( act_repair_item, 100 );

    activity_handlers::repair_item_do_turn( &repair_activity, &guy );

    CHECK( repair_activity.get_moves_left() == 50 );
    CHECK( guy.get_moves() == 0 );
}

TEST_CASE( "Repair item partial completion refunds scaled actor moves",
           "[speed][activity][repair]" )
{
    clear_all_state();
    const auto global_scale = override_option( "TIME_ACTION_SCALE", "50" );
    const auto player_scale = override_option( "PLAYER_ACTION_SCALE", "50" );
    const auto activity_scale = override_option( "ACTIVITY_PROGRESS_SCALE", "100" );

    auto &guy = *get_player_character().as_player();
    clear_character( guy, true );
    guy.add_effect( effect_debug_clairvoyance, 1_minutes );
    REQUIRE( character_funcs::fine_detail_vision_mod( guy ) == Approx( 1.0f ) );

    guy.set_moves( 25 );
    auto repair_activity = player_activity( act_repair_item, 25 );

    activity_handlers::repair_item_do_turn( &repair_activity, &guy );

    CHECK( repair_activity.get_moves_left() == 0 );
    CHECK( guy.get_moves() == 12 );
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
    guy.assign_activity( act_socialize, 1000 );

    REQUIRE( guy.activity );
    REQUIRE( guy.activity->get_moves_left() == 1000 );

    guy.advance_job_progress( 10 );

    CHECK( guy.activity->get_moves_left() == 750 );
}

TEST_CASE( "NPC calendar wait catch-up uses elapsed turns", "[speed][activity][npc][wait]" )
{
    clear_all_state();
    const auto global_scale = override_option( "TIME_ACTION_SCALE", "50" );
    const auto activity_scale = override_option( "ACTIVITY_PROGRESS_SCALE", "50" );

    auto guy = standard_npc( "calendar wait catch-up npc" );
    guy.assign_activity( act_wait, to_moves<int>( 10_seconds ) );

    REQUIRE( guy.activity );
    REQUIRE( guy.activity->get_moves_left() == to_moves<int>( 10_seconds ) );

    guy.advance_job_progress( 10 );

    CHECK( guy.activity->get_moves_left() == 0 );
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
