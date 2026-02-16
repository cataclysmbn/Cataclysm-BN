#include "catch/catch.hpp"

#include <cstdlib>
#include <memory>
#include <optional>
#include <vector>

#include "avatar.h"
#include "bodypart.h"
#include "calendar.h"
#include "flag.h"
#include "game.h"
#include "item.h"
#include "map.h"
#include "map_helpers.h"
#include "point.h"
#include "state_helpers.h"
#include "type_id.h"
#include "vehicle.h"
#include "vehicle_part.h"
#include "vehicle_selector.h"
#include "weather.h"

static const itype_id fuel_type_battery( "battery" );
static const itype_id fuel_type_plut_cell( "plut_cell" );

TEST_CASE( "vehicle power with reactor and solar panels", "[vehicle][power]" )
{
    clear_all_state();
    build_test_map( ter_id( "t_pavement" ) );
    map &here = get_map();

    SECTION( "vehicle with reactor" ) {
        const tripoint reactor_origin = tripoint( 10, 10, 0 );
        vehicle *veh_ptr = here.add_vehicle( vproto_id( "reactor_test" ), reactor_origin, 0_degrees, 0, 0 );
        REQUIRE( veh_ptr != nullptr );

        REQUIRE( !veh_ptr->reactors.empty() );
        vehicle_part &reactor = veh_ptr->part( veh_ptr->reactors.front() );
        reactor.enabled = true;

        GIVEN( "the reactor is empty" ) {
            reactor.ammo_unset();
            veh_ptr->discharge_battery( veh_ptr->fuel_left( fuel_type_battery ) );
            REQUIRE( veh_ptr->fuel_left( fuel_type_battery ) == 0 );

            WHEN( "the reactor is loaded with plutonium fuel" ) {
                reactor.ammo_set( fuel_type_plut_cell, 1 );
                REQUIRE( reactor.ammo_remaining() == 1 );

                AND_WHEN( "the reactor is used to power and charge the battery" ) {
                    veh_ptr->power_parts();
                    THEN( "the reactor should be empty, and the battery should be charged" ) {
                        CHECK( reactor.ammo_remaining() == 0 );
                        CHECK( veh_ptr->fuel_left( fuel_type_battery ) == 100 );
                    }
                }
            }
        }
    }

    SECTION( "vehicle with solar panels" ) {
        const tripoint solar_origin = tripoint( 5, 5, 0 );
        vehicle *veh_ptr = here.add_vehicle( vproto_id( "solar_panel_test" ), solar_origin, 0_degrees, 0,
                                             0 );
        REQUIRE( veh_ptr != nullptr );

        GIVEN( "it is 3 hours after sunrise, with sunny weather" ) {
            calendar::turn = calendar::turn_zero + calendar::season_length() + 1_days;
            const time_point start_time = sunrise( calendar::turn ) + 3_hours;
            veh_ptr->update_time( start_time );
            get_weather().weather_override = weather_type_id( "sunny" );

            AND_GIVEN( "the battery has no charge" ) {
                veh_ptr->discharge_battery( veh_ptr->fuel_left( fuel_type_battery ) );
                REQUIRE( veh_ptr->fuel_left( fuel_type_battery ) == 0 );

                WHEN( "30 minutes elapse" ) {
                    veh_ptr->update_time( start_time + 30_minutes );

                    THEN( "the battery should be partially charged" ) {
                        int charge = veh_ptr->fuel_left( fuel_type_battery ) / 100;
                        CHECK( 20 <= charge );
                        CHECK( charge <= 30 );

                        AND_WHEN( "another 30 minutes elapse" ) {
                            veh_ptr->update_time( start_time + 2 * 30_minutes );

                            THEN( "the battery should be further charged" ) {
                                charge = veh_ptr->fuel_left( fuel_type_battery ) / 100;
                                CHECK( 25 <= charge );
                                CHECK( charge <= 35 );
                            }
                        }
                    }
                }
            }
        }

        GIVEN( "it is 3 hours after sunset, with clear weather" ) {
            const time_point at_night = sunset( calendar::turn ) + 3_hours;
            get_weather().weather_override = weather_type_id( "clear" );
            veh_ptr->update_time( at_night );

            AND_GIVEN( "the battery has no charge" ) {
                veh_ptr->discharge_battery( veh_ptr->fuel_left( fuel_type_battery ) );
                REQUIRE( veh_ptr->fuel_left( fuel_type_battery ) == 0 );

                WHEN( "60 minutes elapse" ) {
                    veh_ptr->update_time( at_night + 2 * 30_minutes );

                    THEN( "the battery should still have no charge" ) {
                        CHECK( veh_ptr->fuel_left( fuel_type_battery ) == 0 );
                    }
                }
            }
        }
    }
}

TEST_CASE( "maximum reverse velocity", "[vehicle][power][reverse]" )
{
    clear_all_state();
    build_test_map( ter_id( "t_pavement" ) );
    map &here = get_map();

    GIVEN( "a scooter with combustion engine and charged battery" ) {
        const tripoint origin = tripoint( 10, 0, 0 );
        vehicle *veh_ptr = here.add_vehicle( vproto_id( "scooter_test" ), origin, 0_degrees, 0, 0 );
        REQUIRE( veh_ptr != nullptr );
        veh_ptr->charge_battery( 600 );
        REQUIRE( veh_ptr->fuel_left( fuel_type_battery ) == 600 );

        WHEN( "the engine is started" ) {
            veh_ptr->start_engines();

            THEN( "it can go in both forward and reverse" ) {
                int max_fwd = veh_ptr->max_velocity( false, true );
                int max_rev = veh_ptr->max_reverse_velocity( false, true );

                CHECK( max_rev < 0 );
                CHECK( max_fwd > 0 );

                AND_THEN( "its maximum reverse velocity is 1/4 of the maximum forward velocity" ) {
                    CHECK( std::abs( max_fwd / max_rev ) == 4 );
                }
            }

        }
    }

    GIVEN( "a scooter with an electric motor and charged battery" ) {
        const tripoint origin = tripoint( 15, 0, 0 );
        vehicle *veh_ptr = here.add_vehicle( vproto_id( "scooter_electric_test" ), origin, 0_degrees, 0,
                                             0 );
        REQUIRE( veh_ptr != nullptr );
        veh_ptr->charge_battery( 5000 );
        REQUIRE( veh_ptr->fuel_left( fuel_type_battery ) == 5000 );

        WHEN( "the engine is started" ) {
            veh_ptr->start_engines();

            THEN( "it can go in both forward and reverse" ) {
                int max_fwd = veh_ptr->max_velocity( false, true );
                int max_rev = veh_ptr->max_reverse_velocity( false, true );

                CHECK( max_rev < 0 );
                CHECK( max_fwd > 0 );

                AND_THEN( "its maximum reverse velocity is equal to maximum forward velocity" ) {
                    CHECK( std::abs( max_rev ) == std::abs( max_fwd ) );
                }
            }
        }
    }
}

TEST_CASE( "Vehicle charging station", "[vehicle][power]" )
{
    clear_all_state();
    build_test_map( ter_id( "t_pavement" ) );

    GIVEN( "Vehicle with a charged battery and an active recharging station on a box" ) {
        const tripoint vehicle_origin = tripoint( 10, 10, 0 );
        vehicle *veh_ptr = g->m.add_vehicle( vproto_id( "recharge_test" ), vehicle_origin, 0_degrees, 100,
                                             0 );
        REQUIRE( veh_ptr != nullptr );
        REQUIRE( veh_ptr->fuel_left( fuel_type_battery ) > 1000 );
        veh_ptr->update_time( calendar::turn_zero );

        auto cargo_part_index = veh_ptr->part_with_feature( point_zero, "CARGO", true );
        REQUIRE( cargo_part_index >= 0 );
        vehicle_part &cargo_part = veh_ptr->part( cargo_part_index );

        auto chargers = veh_ptr->get_parts_at( vehicle_origin, "RECHARGE", part_status_flag::available );
        REQUIRE( chargers.size() == 1 );
        chargers.front()->enabled = true;

        AND_GIVEN( "Rechargable, empty battery in the station" ) {
            detached_ptr<item> det = item::spawn( "light_battery_cell" );
            item &battery = *det;
            battery.ammo_unset();
            REQUIRE( battery.ammo_remaining() == 0 );
            REQUIRE( battery.has_flag( flag_RECHARGE ) );
            veh_ptr->add_item( cargo_part, std::move( det ) );
            WHEN( "An hour passes" ) {
                // Should use vehicle::update_time, but that doesn't do charging...
                for( int i = 0; i < to_turns<int>( 1_hours ); i++ ) {
                    g->m.process_items();
                }

                THEN( "The battery is fully charged" ) {
                    REQUIRE( battery.ammo_remaining() == battery.ammo_capacity() );
                }
            }
        }
        AND_GIVEN( "Tool with a rechargable, empty battery in the station" ) {
            detached_ptr<item> det = item::spawn( "light_battery_cell" );
            item &battery = *det;
            battery.ammo_unset();
            REQUIRE( battery.ammo_remaining() == 0 );
            REQUIRE( battery.has_flag( flag_RECHARGE ) );
            veh_ptr->add_item( cargo_part, std::move( det ) );

            det = item::spawn( "soldering_iron" );
            item &tool = *det;
            REQUIRE( tool.magazine_current() == nullptr );
            tool.reload( g->u, battery, 1 );
            veh_ptr->add_item( cargo_part, std::move( det ) );
            WHEN( "An hour passes" ) {
                for( int i = 0; i < to_turns<int>( 1_hours ); i++ ) {
                    g->m.process_items();
                }

                THEN( "The battery is fully charged" ) {
                    REQUIRE( tool.ammo_remaining() == tool.ammo_capacity() );
                }
            }
        }
    }

}
