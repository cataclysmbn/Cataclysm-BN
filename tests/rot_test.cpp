#include "catch/catch.hpp"

#include <memory>

#include "calendar.h"
#include "enums.h"
#include "item.h"
#include "map.h"
#include "map_helpers.h"
#include "game.h" // Just for get_convection_temperature(), TODO: Remove
#include "point.h"
#include "units_temperature.h"
#include "weather.h"

static const furn_str_id f_atomic_freezer( "f_atomic_freezer" );

static void set_map_temperature( weather_manager &weather, units::temperature new_temperature )
{
    weather.temperature = new_temperature;
    weather.clear_temp_cache();
}

static void ensure_no_temperature_mods( tripoint location )
{
    REQUIRE( get_heat_radiation( location, false ) == 0 );
    REQUIRE( get_convection_temperature( location ) == 0 );
    REQUIRE( get_map().get_temperature( location ) == 0 );
}

TEST_CASE( "Rate of rotting" )
{
    SECTION( "Passage of time" ) {
        weather_manager weather;
        // Item rot is a time duration.
        // At 65 F (18,3 C) item rots at rate of 1h/1h
        // So the level of rot should be about same as the item age
        // In preserving containers and in freezer the item should not rot at all

        // Items created at turn zero are handled differently, so ensure we're
        // not there.
        if( calendar::turn <= calendar::start_of_cataclysm ) {
            calendar::turn = calendar::start_of_cataclysm + 1_minutes;
        }

        detached_ptr<item> normal_item = item::spawn( "meat_cooked" );
        detached_ptr<item> freeze_item = item::spawn( "offal_canned" );
        detached_ptr<item> sealed_item = item::in_its_container( item::spawn( "offal_canned" ) );

        set_map_temperature( weather, 18_c );
        ensure_no_temperature_mods( tripoint_zero );
        REQUIRE( weather.get_temperature( tripoint_zero ) == 18_c );

        normal_item = item::process( std::move( normal_item ), nullptr, tripoint_zero, false,
                                     temperature_flag::TEMP_NORMAL, weather );
        sealed_item = item::process( std::move( sealed_item ), nullptr, tripoint_zero, false,
                                     temperature_flag::TEMP_NORMAL, weather );
        freeze_item = item::process( std::move( freeze_item ), nullptr, tripoint_zero, false,
                                     temperature_flag::TEMP_NORMAL, weather );

        // Item should exist with no rot when it is brand new
        CHECK( normal_item->get_rot() == 0_turns );
        CHECK( sealed_item->get_rot() == 0_turns );
        CHECK( freeze_item->get_rot() == 0_turns );

        INFO( "Initial turn: " << to_turn<int>( calendar::turn ) );

        calendar::turn += 20_minutes;
        normal_item = item::process( std::move( normal_item ), nullptr, tripoint_zero, false,
                                     temperature_flag::TEMP_NORMAL, weather );
        sealed_item = item::process( std::move( sealed_item ), nullptr, tripoint_zero, false,
                                     temperature_flag::TEMP_NORMAL, weather );
        freeze_item = item::process( std::move( freeze_item ), nullptr, tripoint_zero, false,
                                     temperature_flag::TEMP_FREEZER, weather );

        // After 20 minutes the normal item should have 20 minutes of rot
        CHECK( to_turns<int>( normal_item->get_rot() )
               == Approx( to_turns<int>( 20_minutes ) ).epsilon( 0.01 ) );
        // Item in freezer and in preserving container should have no rot
        CHECK( sealed_item->get_rot() == 0_turns );
        CHECK( freeze_item->get_rot() == 0_turns );

        // Move time 110 minutes
        calendar::turn += 110_minutes;
        // TODO: Check >1 hour normal processing as well - can't be "simply done" because of weather globals
        sealed_item = item::process( std::move( sealed_item ), nullptr, tripoint_zero, false,
                                     temperature_flag::TEMP_NORMAL, weather );
        freeze_item = item::process( std::move( freeze_item ), nullptr, tripoint_zero, false,
                                     temperature_flag::TEMP_FREEZER, weather );
        // In freezer and in preserving container still should be no rot
        CHECK( sealed_item->get_rot() == 0_turns );
        CHECK( freeze_item->get_rot() == 0_turns );
    }
}

TEST_CASE( "Items rot away" )
{
    SECTION( "Item in reality bubble rots away" ) {
        weather_manager weather;
        // Item should rot away when it has 2x of its shelf life in rot.

        if( calendar::turn <= calendar::start_of_cataclysm ) {
            calendar::turn = calendar::start_of_cataclysm + 1_minutes;
        }

        detached_ptr<item> test_item = item::spawn( "meat_cooked" );

        // Process item once to set all of its values.
        test_item = item::process( std::move( test_item ), nullptr, tripoint_zero, false,
                                   temperature_flag::TEMP_HEATER, weather );

        // Set rot to >2 days and process again. process_rot should destroy the item.
        calendar::turn += 20_minutes;
        test_item->mod_rot( 4_days );
        test_item = item::process_rot( std::move( test_item ), false, tripoint_zero, nullptr,
                                       temperature_flag::TEMP_HEATER, weather );
        CHECK( !test_item );
    }

    SECTION( "Item on map rots away" ) {
        weather_manager weather;
        const tripoint loc;

        if( calendar::turn <= calendar::start_of_cataclysm ) {
            calendar::turn = calendar::start_of_cataclysm + 1_minutes;
        }

        detached_ptr<item> test_item = item::process( item::spawn( "meat_cooked" ), nullptr, tripoint_zero,
                                       false, temperature_flag::TEMP_HEATER, weather );
        map &m = get_map();
        m.add_item_or_charges( loc, std::move( test_item ), false );

        REQUIRE( m.i_at( loc ).size() == 1 );

        calendar::turn += 20_minutes;
        m.i_at( loc ).only_item().mod_rot( 7_days );
        m.process_items();

        CHECK( m.i_at( loc ).empty() );
    }
}

TEST_CASE( "Items don't rot away on map load if in a freezer" )
{
    tinymap m;
    weather_manager weather;
    if( calendar::turn <= calendar::start_of_cataclysm ) {
        calendar::turn = calendar::start_of_cataclysm + 1_minutes;
    }

    constexpr tripoint_abs_sm non_tested_location = tripoint_abs_sm( 0, 0, 0 );
    constexpr tripoint_abs_sm test_location = tripoint_abs_sm( 100, 100, 0 );
    m.load( test_location, false );

    const tripoint freezer_pnt = {13, 13, 0};
    const tripoint sealed_pnt = {14, 13, 0};
    const tripoint normal_pnt = {15, 13, 0};
    m.furn_set( freezer_pnt, f_atomic_freezer );
    m.furn_set( sealed_pnt, furn_str_id::NULL_ID() );
    m.furn_set( normal_pnt, furn_str_id::NULL_ID() );
    m.ter_set( freezer_pnt, t_grass );
    m.ter_set( sealed_pnt, t_grass );
    m.ter_set( normal_pnt, t_grass );


    detached_ptr<item> normal_item_d = item::spawn( "meat_cooked" );
    item &normal_item = *normal_item_d;
    detached_ptr<item> freeze_item_d = item::spawn( "offal_canned" );
    item &freeze_item = *freeze_item_d;
    detached_ptr<item> sealed_item_d = item::in_its_container( item::spawn( "offal_canned" ) );
    item &sealed_item = *sealed_item_d;

    set_map_temperature( weather, 18_c );

    m.i_clear( freezer_pnt );
    m.i_clear( sealed_pnt );
    m.i_clear( normal_pnt );

    m.add_item( freezer_pnt, std::move( freeze_item_d ) );
    m.add_item( sealed_pnt, std::move( sealed_item_d ) );
    m.add_item( normal_pnt, std::move( normal_item_d ) );

    REQUIRE( normal_item.get_rot() == 0_turns );
    REQUIRE( sealed_item.get_rot() == 0_turns );
    REQUIRE( freeze_item.get_rot() == 0_turns );

    auto freezer_stack = m.i_at( freezer_pnt );
    REQUIRE( freezer_stack.size() == 1 );
    auto sealed_stack = m.i_at( sealed_pnt );
    REQUIRE( sealed_stack.size() == 1 );
    auto normal_stack = m.i_at( normal_pnt );
    REQUIRE( normal_stack.size() == 1 );

    INFO( "Initial turn: " << to_turn<int>( calendar::turn ) );

    // Change the date outside the location, to force @ref map::actualize to proc rot
    m.load( non_tested_location, false );
    calendar::turn += 365_days;
    m.load( test_location, false );

    auto freezer_stack_after = m.i_at( freezer_pnt );
    REQUIRE( freezer_stack_after.size() == 1 );
    auto sealed_stack_after = m.i_at( sealed_pnt );
    REQUIRE( sealed_stack_after.size() == 1 );
    auto normal_stack_after = m.i_at( normal_pnt );
    REQUIRE( normal_stack_after.empty() );
}
