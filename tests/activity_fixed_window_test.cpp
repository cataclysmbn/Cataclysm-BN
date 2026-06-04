#include "catch/catch.hpp"

#include <string>
#include <utility>

#include "activity_time_cadence.h"
#include "avatar.h"
#include "calendar.h"
#include "cata_utility.h"
#include "coordinates.h"
#include "field_type.h"
#include "game.h"
#include "game_constants.h"
#include "item.h"
#include "itype.h"
#include "map.h"
#include "map_helpers.h"
#include "monster.h"
#include "npc.h"
#include "options_helpers.h"
#include "player_activity.h"
#include "player_helpers.h"
#include "state_helpers.h"
#include "timed_event.h"
#include "units_temperature.h"
#include "vehicle.h"
#include "weather.h"

static const auto act_wait = activity_id( "ACT_WAIT" );

static auto prepare_fixed_window_wait( const time_duration &duration ) -> void
{
    clear_all_state();

    g->timed_events = timed_event_manager {};
    calendar::turn = calendar::turn_zero + 12_hours;

    auto &weather = get_weather();
    weather.weather_id = weather_type_id( "clear" );
    weather.nextweather = calendar::turn + 10_minutes;
    weather.temperature = 18_c;
    weather.clear_temp_cache();

    g->reset_light_level();
    g->m.invalidate_map_cache( g->get_levz() );
    g->m.build_map_cache( g->get_levz(), true );

    g->new_game = false;
    g->u.set_moves( 100 );
    g->u.assign_activity( act_wait, to_moves<int>( duration ), 0 );

    REQUIRE( g->u.activity );
    REQUIRE( *g->u.activity );
}

static auto expect_fixed_window_skip_blocked_by( auto setup_blocker ) -> void
{
    const auto no_autosave = override_option( "AUTOSAVE", "false" );
    const auto normal_bubble_size = override_option( "REALITY_BUBBLE_SIZE",
                                    std::to_string( g_reality_bubble_size ) );
    const auto no_mobile_bubble = override_option( "ACTIVITY_MOBILE_BUBBLE_SIZE", "0" );
    const auto no_idle_bubble = override_option( "ACTIVITY_IDLE_BUBBLE_SIZE", "0" );
    const auto no_underground_bubble = override_option( "UNDERGROUND_BUBBLE_SIZE", "0" );
    const auto no_vehicle_bubble = override_option( "VEHICLE_BUBBLE_SIZE", "0" );
    const auto no_combat_bubble = override_option( "COMBAT_BUBBLE_SIZE", "0" );
    const auto duration = activity_time_cadence::fixed_window();
    prepare_fixed_window_wait( duration );
    const auto cleanup = on_out_of_scope( []() {
        clear_all_state();
    } );
    setup_blocker();

    const auto start_turn = calendar::turn;

    CHECK_FALSE( g->do_turn() );
    CHECK( calendar::turn == start_turn + 1_turns );
    CHECK( static_cast<bool>( g->u.activity ) );
}

TEST_CASE( "fixed window activity skip completes a short wait with active creatures",
           "[activity][fixed_window]" )
{
    const auto no_autosave = override_option( "AUTOSAVE", "false" );
    const auto normal_bubble_size = override_option( "REALITY_BUBBLE_SIZE",
                                    std::to_string( g_reality_bubble_size ) );
    const auto no_mobile_bubble = override_option( "ACTIVITY_MOBILE_BUBBLE_SIZE", "0" );
    const auto no_idle_bubble = override_option( "ACTIVITY_IDLE_BUBBLE_SIZE", "0" );
    const auto no_underground_bubble = override_option( "UNDERGROUND_BUBBLE_SIZE", "0" );
    const auto no_vehicle_bubble = override_option( "VEHICLE_BUBBLE_SIZE", "0" );
    const auto no_combat_bubble = override_option( "COMBAT_BUBBLE_SIZE", "0" );
    const auto duration = 30_seconds;
    prepare_fixed_window_wait( duration );
    const auto cleanup = on_out_of_scope( []() {
        clear_all_state();
    } );

    const auto item_pos = g->u.bub_pos() + point_east;
    auto timer = item::spawn( "gasbomb_act" );
    timer->activate();
    REQUIRE( timer->has_explicit_turn_timer() );
    const auto starting_counter = timer->get_counter();
    REQUIRE( starting_counter > to_turns<int>( duration ) );
    g->m.add_item( item_pos, std::move( timer ) );
    REQUIRE( g->m.i_at( item_pos ).size() == 1 );

    spawn_test_monster( "mon_zombie", g->u.bub_pos() + tripoint_rel_ms( 30, 0, 0 ) );
    auto &talker = spawn_npc( g->u.bub_pos().xy() + point( 5, 0 ), "test_talker" );
    talker.mission = NPC_MISSION_SHOPKEEP;

    const auto start_turn = calendar::turn;

    CHECK_FALSE( g->do_turn() );

    CHECK( calendar::turn == start_turn + duration );
    CHECK_FALSE( static_cast<bool>( g->u.activity ) );

    auto &timer_after = g->m.i_at( item_pos ).only_item();
    CHECK( timer_after.get_counter() <= starting_counter - to_turns<int>( duration ) );
    CHECK( timer_after.get_counter() >= starting_counter - to_turns<int>( duration ) - 1 );
}

TEST_CASE( "fixed window activity skip hard blockers fall back to normal turns",
           "[activity][fixed_window]" )
{
    const auto no_autosave = override_option( "AUTOSAVE", "false" );

    SECTION( "player tile field" ) {
        expect_fixed_window_skip_blocked_by( [] {
            g->m.add_field( g->u.bub_pos(), fd_acid, 1 );
        } );
    }

    SECTION( "active fire in simulated submap" ) {
        expect_fixed_window_skip_blocked_by( [] {
            g->m.add_field( g->u.bub_pos() + point_east, fd_fire, 1 );
        } );
    }

    SECTION( "relevant vehicle" ) {
        expect_fixed_window_skip_blocked_by( [] {
            auto *veh = g->m.add_vehicle( vproto_id( "car" ), g->u.bub_pos() + tripoint_east,
                                          0_degrees, 0, 0 );
            REQUIRE( veh != nullptr );
            veh->engine_on = true;
        } );
    }

    SECTION( "timed event during window" ) {
        expect_fixed_window_skip_blocked_by( [] {
            g->timed_events.add( TIMED_EVENT_WANTED, calendar::turn + 30_seconds );
        } );
    }
}
