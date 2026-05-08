#include "catch/catch.hpp"

#include <climits>
#include <list>
#include <memory>
#include <string>

#include "avatar.h"
#include "bionics.h"
#include "calendar.h"
#include "item.h"
#include "itype.h"
#include "pimpl.h"
#include "player.h"
#include "player_helpers.h"
#include "state_helpers.h"
#include "type_id.h"
#include "units.h"

static void clear_bionics( player &p )
{
    p.my_bionics->clear();
    p.set_power_level( 0_kJ );
    p.set_max_power_level( 0_kJ );
}

static void test_consumable_charges( player &p, std::string &itemname, bool when_none,
                                     bool when_max )
{
    item &it = *item::spawn_temporary( itemname, calendar::start_of_cataclysm, 0 );

    INFO( "\'" + it.tname() + "\' is count-by-charges" );
    CHECK( it.count_by_charges() );

    it.charges = 0;
    INFO( "consume \'" + it.tname() + "\' with " + std::to_string( it.charges ) + " charges" );
    REQUIRE( p.can_consume( it ) == when_none );

    it.charges = INT_MAX;
    INFO( "consume \'" + it.tname() + "\' with " + std::to_string( it.charges ) + " charges" );
    REQUIRE( p.can_consume( it ) == when_max );
}

static void test_consumable_ammo( player &p, std::string &itemname, bool when_empty,
                                  bool when_full )
{
    detached_ptr<item> it = item::spawn( itemname, calendar::start_of_cataclysm, 0 );

    it->ammo_unset();
    INFO( "consume \'" + it->tname() + "\' with " + std::to_string( it->ammo_remaining() ) +
          " charges" );
    REQUIRE( p.can_consume( *it ) == when_empty );

    it->ammo_set( it->ammo_default(), -1 ); // -1 -> full
    INFO( "consume \'" + it->tname() + "\' with " + std::to_string( it->ammo_remaining() ) +
          " charges" );
    REQUIRE( p.can_consume( *it ) == when_full );
}

TEST_CASE( "bionics", "[bionics] [item]" )
{
    clear_all_state();
    avatar &dummy = get_avatar();
    // one section failing shouldn't affect the rest
    clear_bionics( dummy );

    // Could be a SECTION, but prerequisite for many tests.
    INFO( "no power capacity at first" );
    CHECK( !dummy.has_max_power() );

    dummy.add_bionic( bionic_id( "bio_power_storage" ) );

    INFO( "adding Power Storage CBM only increases capacity" );
    CHECK( !dummy.has_power() );
    REQUIRE( dummy.has_max_power() );

    SECTION( "bio_batteries" ) {
        give_and_activate_bionic( dummy, bionic_id( "bio_batteries" ) );

        static const std::list<std::string> always = {
            "battery" // old-school
        };
        for( auto it : always ) {
            test_consumable_charges( dummy, it, true, true );
        }

        static const std::list<std::string> never = {
            "flashlight",  // !is_magazine()
            "laser_rifle", // NO_UNLOAD, uses ups_charges
            "UPS_off",     // NO_UNLOAD, !is_magazine()
            "battery_car"  // NO_UNLOAD, is_magazine()
        };
        for( auto it : never ) {
            test_consumable_ammo( dummy, it, false, false );
        }
    }

    // TODO: bio_cable
    // TODO: (pick from stuff with power_source)
}

static const bionic_id bio_reactor( "bio_reactor" );
static const bionic_id bio_advreactor( "bio_advreactor" );
static const itype_id itype_plut_cell( "plut_cell" );

/// Helper: set up a character with power storage and a reactor bionic loaded with fuel.
/// Returns a reference to the installed bionic.
static auto setup_reactor( player &p, const bionic_id &reactor_id,
                           int fuel_stock ) -> bionic &
{
    clear_bionics( p );
    p.add_bionic( bionic_id( "bio_power_storage" ) );
    p.set_max_power_level( 100_kJ );
    p.set_power_level( 0_kJ );
    p.add_bionic( reactor_id );
    // Always reset fuel state to avoid leaking values between test sections.
    if( fuel_stock > 0 ) {
        p.set_value( itype_plut_cell.str(), std::to_string( fuel_stock ) );
    } else {
        p.remove_value( itype_plut_cell.str() );
    }
    return p.get_bionic_state( reactor_id );
}

TEST_CASE( "microreactor_fuel_consumption", "[bionics] [reactor]" )
{
    clear_all_state();
    auto &dummy = get_avatar();

    SECTION( "bio_reactor consumes plutonium when active" ) {
        auto initial_fuel = 100;
        auto &bio = setup_reactor( dummy, bio_reactor, initial_fuel );

        // Activate with plut_cell fuel available
        REQUIRE( dummy.activate_bionic( bio ) );
        REQUIRE( bio.powered );

        // Drain the charge_timer so burn_fuel fires next process
        bio.charge_timer = 0;
        dummy.process_bionic( bio );

        auto remaining = std::stoi( dummy.get_value( itype_plut_cell.str() ) );
        CHECK( remaining < initial_fuel );
    }

    SECTION( "bio_advreactor consumes plutonium when active" ) {
        auto initial_fuel = 100;
        auto &bio = setup_reactor( dummy, bio_advreactor, initial_fuel );

        REQUIRE( dummy.activate_bionic( bio ) );
        REQUIRE( bio.powered );

        bio.charge_timer = 0;
        dummy.process_bionic( bio );

        auto remaining = std::stoi( dummy.get_value( itype_plut_cell.str() ) );
        CHECK( remaining < initial_fuel );
    }

    SECTION( "bio_reactor cannot activate without plutonium" ) {
        auto &bio = setup_reactor( dummy, bio_reactor, 0 );

        CHECK_FALSE( dummy.activate_bionic( bio ) );
        CHECK_FALSE( bio.powered );
    }

    SECTION( "bio_reactor generates passive power when inactive" ) {
        auto &bio = setup_reactor( dummy, bio_reactor, 0 );

        REQUIRE_FALSE( bio.powered );

        auto power_before = dummy.get_power_level();
        dummy.process_bionic( bio );
        auto power_after = dummy.get_power_level();

        CHECK( power_after > power_before );
    }

    SECTION( "bio_reactor active power exceeds passive power" ) {
        // When active with fuel, a reactor should generate significantly more power
        // than the passive generation from the perpetual atomic battery fuel.
        auto &bio = setup_reactor( dummy, bio_reactor, 1000 );

        // Measure passive power (bionic off)
        REQUIRE_FALSE( bio.powered );
        auto power_before_passive = dummy.get_power_level();
        dummy.process_bionic( bio );
        auto passive_gain = dummy.get_power_level() - power_before_passive;
        REQUIRE( passive_gain > 0_kJ );

        // Now activate and measure active power
        dummy.set_power_level( 0_kJ );
        REQUIRE( dummy.activate_bionic( bio ) );
        bio.charge_timer = 0;
        auto power_before_active = dummy.get_power_level();
        dummy.process_bionic( bio );
        auto active_gain = dummy.get_power_level() - power_before_active;

        CHECK( active_gain > passive_gain );
    }
}

TEST_CASE( "microreactor_installation_paths", "[bionics][reactor]" )
{
    clear_all_state();
    auto &dummy = get_avatar();

    SECTION( "bio_reactor does not require upgraded_bionic for direct install" ) {
        clear_bionics( dummy );

        // Create a bio_reactor CBM item
        auto cbm = item::spawn_temporary( "bio_reactor", calendar::turn_zero );
        const auto &bid = cbm->type->bionic->id.obj();

        // Verify bio_reactor does NOT have upgraded_bionic set
        // (this allows direct installation without requiring bio_atomic_battery)
        CHECK_FALSE( bid.upgraded_bionic );
    }

    SECTION( "bio_reactor can be installed directly" ) {
        clear_bionics( dummy );
        REQUIRE_FALSE( dummy.has_bionic( bionic_id( "bio_atomic_battery" ) ) );

        // Directly install microreactor using add_bionic
        dummy.add_bionic( bio_reactor );
        CHECK( dummy.has_bionic( bio_reactor ) );
    }
}
