#include "catch/catch.hpp"

#include <vector>

#include "active_tile_data.h"
#include "active_tile_data_def.h"
#include "cata_utility.h"
#include "coordinate_conversions.h"
#include "distribution_grid.h"
#include "map.h"
#include "mapbuffer.h"
#include "map_helpers.h"
#include "overmap.h"
#include "overmapbuffer.h"
#include "submap.h"
#include "state_helpers.h"
#include "stringmaker.h"
#include "vehicle.h"
#include "vehicle_part.h"

static furn_str_id f_battery( "f_battery" );
static furn_str_id f_cable_connector( "f_cable_connector" );
static furn_str_id f_floor_lamp( "f_floor_lamp" );
static furn_str_id f_floor_lamp_on( "f_floor_lamp_on" );
static furn_str_id f_solar_unit( "f_solar_unit" );

static itype_id itype_battery( "battery" );

static inline void test_grid_veh( distribution_grid &grid, vehicle &veh, battery_tile &battery )
{
    CAPTURE( veh.fuel_capacity( itype_battery ) );
    CAPTURE( battery.max_stored );
    WHEN( "the vehicle is fully charged and battery is discharged" ) {
        veh.charge_battery( veh.fuel_capacity( itype_battery ), false );
        REQUIRE( veh.fuel_left( itype_battery, false ) ==
                 veh.fuel_capacity( itype_battery ) );
        REQUIRE( battery.get_resource() == 0 );
        REQUIRE( grid.get_resource() == veh.fuel_capacity( itype_battery ) );
        AND_WHEN( "the grid is discharged without energy in battery" ) {
            int deficit = grid.mod_resource( -( grid.get_resource() - 10 ) );
            CHECK( deficit == 0 );
            THEN( "the power is drained from vehicle" ) {
                CHECK( grid.get_resource() == 10 );
                CHECK( veh.fuel_left( itype_battery, false ) == 10 );
            }
        }

        AND_WHEN( "the vehicle is charged despite being full" ) {
            int excess = veh.charge_battery( 10 );
            THEN( "the grid contains the added energy" ) {
                CHECK( excess == 0 );
                CHECK( grid.get_resource() == veh.fuel_left( itype_battery, false ) + 10 );
                AND_THEN( "the added energy is in the battery" ) {
                    CHECK( battery.get_resource() == 10 );
                }
            }
        }
    }

    WHEN( "the battery is fully charged and vehicle is discharged" ) {
        int excess = battery.mod_resource( battery.max_stored );
        REQUIRE( excess == 0 );
        REQUIRE( battery.get_resource() == battery.max_stored );
        REQUIRE( veh.fuel_left( itype_battery, false ) == 0 );
        REQUIRE( grid.get_resource() == battery.get_resource() );
        AND_WHEN( "the vehicle is discharged despite being empty" ) {
            int deficit = veh.discharge_battery( 10, true );
            THEN( "the grid provides the needed power" ) {
                CHECK( deficit == 0 );
                AND_THEN( "this power comes from the battery" ) {
                    CHECK( battery.get_resource() == battery.max_stored - 10 );
                }
            }
        }

        AND_WHEN( "the grid is charged some more" ) {
            int excess = grid.mod_resource( 10 );
            THEN( "the grid contains the added energy" ) {
                CHECK( excess == 0 );
                CHECK( grid.get_resource() == battery.max_stored + 10 );
                AND_THEN( "the added energy is in the vehicle" ) {
                    CHECK( veh.fuel_left( itype_battery, false ) == 10 );
                }
            }
        }
    }
}

static void connect_grid_vehicle( map &m, vehicle &veh, vehicle_connector_tile &connector,
                                  const tripoint_abs_ms &connector_abs_pos )
{
    const point cable_part_pos;
    vehicle_part source_part( vpart_id( "jumper_cable" ), cable_part_pos,
                              item::spawn( "jumper_cable" ), &veh );
    source_part.target.first = connector_abs_pos.raw();
    source_part.target.second = connector_abs_pos.raw();
    source_part.set_flag( vehicle_part::targets_grid );
    connector.connected_vehicles.clear();
    connector.connected_vehicles.emplace_back( m.getabs( veh.global_pos3() ) );
    int part_index = veh.install_part( cable_part_pos, std::move( source_part ) );

    REQUIRE( part_index >= 0 );
}

struct grid_setup {
    distribution_grid &grid;
    vehicle &veh;
    battery_tile &battery;
};

static void clear_grid_connections( map &m )
{
    // TODO: fix point types
    auto om = overmap_buffer.get_om_global( project_to<coords::omt>( tripoint_abs_sm(
            m.get_abs_sub() ) ) );
    om.om->set_electric_grid_connections( om.local, {} );
}

static grid_setup set_up_grid( map &m )
{
    // TODO: clear_grids()
    clear_grid_connections( m );

    const tripoint vehicle_local_pos = tripoint( 10, 10, 0 );
    const tripoint connector_local_pos = tripoint( 13, 10, 0 );
    const tripoint battery_local_pos = tripoint( 14, 10, 0 );
    const tripoint_abs_ms connector_abs_pos( m.getabs( connector_local_pos ) );
    const tripoint_abs_ms battery_abs_pos( m.getabs( battery_local_pos ) );
    m.furn_set( connector_local_pos, f_cable_connector );
    m.furn_set( battery_local_pos, f_battery );
    vehicle *veh = m.add_vehicle( vproto_id( "car" ), vehicle_local_pos, 0_degrees, 0, 0, false );
    vehicle_connector_tile *grid_connector =
        active_tiles::furn_at<vehicle_connector_tile>( connector_abs_pos );
    battery_tile *battery = active_tiles::furn_at<battery_tile>( battery_abs_pos );

    CAPTURE( connector_abs_pos );
    CAPTURE( battery_abs_pos );
    REQUIRE( veh );
    REQUIRE( grid_connector );
    REQUIRE( battery );

    connect_grid_vehicle( m, *veh, *grid_connector, connector_abs_pos );

    distribution_grid &grid = get_distribution_grid_tracker().grid_at( connector_abs_pos );
    REQUIRE( !grid.empty() );
    REQUIRE( &grid == &get_distribution_grid_tracker().grid_at( battery_abs_pos ) );
    return grid_setup{grid, *veh, *battery};
}

TEST_CASE( "grid_and_vehicle_in_bubble", "[grids][vehicle]" )
{
    clear_all_state();
    put_player_underground();
    GIVEN( "vehicle and battery are on one grid" ) {
        auto setup = set_up_grid( get_map() );
        test_grid_veh( setup.grid, setup.veh, setup.battery );
    }
}

TEST_CASE( "grid_and_vehicle_outside_bubble", "[grids][vehicle]" )
{
    clear_all_state();
    put_player_underground();
    map &m = get_map();
    const tripoint old_abs_sub = m.get_abs_sub();
    // Ugly: we move the real map instead of the tinymap to reuse clear_map() results
    m.load( m.get_abs_sub() + point( m.getmapsize(), 0 ), true );
    GIVEN( "vehicle and battery are on one grid" ) {
        tinymap tm;
        tm.load( old_abs_sub, false );
        auto setup = set_up_grid( tm );
        tm.save();
        test_grid_veh( setup.grid, setup.veh, setup.battery );
    }
}

struct grid_setup_consumer {
    distribution_grid &grid;
    steady_consumer_tile &consumer;
    battery_tile &battery;
    tripoint_abs_ms consumer_pos;
};

struct grid_setup_watcher {
    distribution_grid &grid;
    charge_watcher_tile &watcher;
    battery_tile &battery;
    tripoint_abs_ms watcher_pos;
};

template<typename T, typename S>
static S set_up_grid_with_consumer( map &m, const furn_str_id &act_tile_id )
{
    // TODO: clear_grids()
    clear_grid_connections( m );

    const tripoint act_local_pos = tripoint( 13, 10, 0 );
    const tripoint battery_local_pos = tripoint( 14, 10, 0 );
    const tripoint_abs_ms act_abs_pos( m.getabs( act_local_pos ) );
    const tripoint_abs_ms battery_abs_pos( m.getabs( battery_local_pos ) );
    m.furn_set( act_local_pos, act_tile_id );
    m.furn_set( battery_local_pos, f_battery );
    T *act_tile = active_tiles::furn_at<T>( act_abs_pos );
    battery_tile *battery = active_tiles::furn_at<battery_tile>( battery_abs_pos );

    CAPTURE( act_abs_pos );
    CAPTURE( battery_abs_pos );
    REQUIRE( act_tile );
    REQUIRE( battery );

    distribution_grid &grid = get_distribution_grid_tracker().grid_at( act_abs_pos );
    REQUIRE( !grid.empty() );
    REQUIRE( &grid == &get_distribution_grid_tracker().grid_at( battery_abs_pos ) );
    return S{grid, *act_tile, *battery, act_abs_pos};
}

static void require_empty_queue( const grid_furn_transform_queue &q )
{
    static const grid_furn_transform_queue empty_queue;
    REQUIRE( q == empty_queue );
}

static void test_steady_consumer( grid_setup_consumer &setup )
{
    grid_furn_transform_queue &tf_queue = get_distribution_grid_tracker().get_transform_queue();
    auto _cleanup = on_out_of_scope( [&]() {
        tf_queue.clear();
    } );

    distribution_grid &grid = setup.grid;
    steady_consumer_tile &consumer = setup.consumer;
    battery_tile &battery = setup.battery;

    CAPTURE( battery.max_stored );
    CAPTURE( consumer.power );
    CAPTURE( consumer.consume_every );
    CAPTURE( consumer.transform.id );
    REQUIRE( consumer.consume_every > 1_seconds );

    WHEN( "the battery is fully charged" ) {
        int excess = battery.mod_resource( battery.max_stored );
        REQUIRE( excess == 0 );
        REQUIRE( battery.get_resource() == battery.max_stored );
        REQUIRE( grid.get_resource() == battery.get_resource() );

        AND_WHEN( "1 consumer tick passes" ) {
            time_point to = calendar::turn + consumer.consume_every;
            grid.update( to );
            THEN( "the battery has been drained by specified amount per tick" ) {
                REQUIRE( grid.get_resource() == battery.max_stored - consumer.power );
                require_empty_queue( tf_queue );
            }
        }

        AND_WHEN( "less than 1 consumer tick passes" ) {
            time_point to = calendar::turn + consumer.consume_every - 1_seconds;
            grid.update( to );
            THEN( "no changes" ) {
                REQUIRE( grid.get_resource() == battery.max_stored );
                require_empty_queue( tf_queue );
            }

            AND_WHEN( "3 consumer ticks pass" ) {
                to += consumer.consume_every * 3;
                grid.update( to );
                THEN( "the battery has been drained by 3x specified amount per tick" ) {
                    REQUIRE( grid.get_resource() == battery.max_stored - consumer.power * 3 );
                    require_empty_queue( tf_queue );
                }
            }
        }
    }

    WHEN( "the battery has power for 1 consumer tick" ) {
        int excess = battery.mod_resource( 3 );
        REQUIRE( excess == 0 );
        REQUIRE( battery.get_resource() == 3 );
        REQUIRE( grid.get_resource() == battery.get_resource() );

        AND_WHEN( "1 consumer tick passes" ) {
            time_point to = calendar::turn + consumer.consume_every;
            grid.update( to );
            THEN( "the battery has been fully drained" ) {
                REQUIRE( grid.get_resource() == 0 );
                require_empty_queue( tf_queue );
            }
        }

        AND_WHEN( "less than 1 consumer tick passes" ) {
            time_point to = calendar::turn + consumer.consume_every - 1_seconds;
            grid.update( to );
            THEN( "no changes" ) {
                REQUIRE( grid.get_resource() == 3 );
                require_empty_queue( tf_queue );
            }

            AND_WHEN( "3 consumer ticks pass" ) {
                to += consumer.consume_every * 3;
                grid.update( to );
                THEN( "the battery has been fully drained, and transform has been queued" ) {
                    REQUIRE( grid.get_resource() == 0 );

                    grid_furn_transform_queue single_dead_lamp;
                    single_dead_lamp.add( setup.consumer_pos, f_floor_lamp, "The lamp flickers and dies." );

                    REQUIRE( tf_queue == single_dead_lamp );
                }
            }
        }
    }
}

static void test_charge_watcher( grid_setup_watcher &setup )
{
    grid_furn_transform_queue &tf_queue = get_distribution_grid_tracker().get_transform_queue();
    auto _cleanup = on_out_of_scope( [&]() {
        tf_queue.clear();
    } );

    distribution_grid &grid = setup.grid;
    charge_watcher_tile &watcher = setup.watcher;
    battery_tile &battery = setup.battery;

    CAPTURE( battery.max_stored );
    CAPTURE( watcher.min_power );
    CAPTURE( watcher.transform.id );

    WHEN( "battery charge < watcher limit" ) {
        int excess = battery.mod_resource( 3 );
        REQUIRE( excess == 0 );
        REQUIRE( battery.get_resource() == 3 );
        REQUIRE( grid.get_resource() == battery.get_resource() );

        AND_WHEN( "1 turn passes" ) {
            time_point to = calendar::turn + 1_seconds;
            grid.update( to );

            THEN( "nothing happens" ) {
                require_empty_queue( tf_queue );
            }
        }
    }

    WHEN( "battery charge >= watcher limit" ) {
        int excess = battery.mod_resource( 5 );
        REQUIRE( excess == 0 );
        REQUIRE( battery.get_resource() == 5 );
        REQUIRE( grid.get_resource() == battery.get_resource() );

        AND_WHEN( "1 turn passes" ) {
            time_point to = calendar::turn + 1_seconds;
            grid.update( to );

            THEN( "transform has been queued" ) {
                grid_furn_transform_queue single_lit_lamp;
                single_lit_lamp.add( setup.watcher_pos, f_floor_lamp_on, "The lamp lights up." );

                REQUIRE( tf_queue == single_lit_lamp );
            }
        }
    }
}

TEST_CASE( "steady_consumer_in_bubble", "[grids]" )
{
    clear_all_state();
    calendar::turn = calendar::turn_zero;
    put_player_underground();

    GIVEN( "consumer and battery are on one grid" ) {
        grid_setup_consumer setup = set_up_grid_with_consumer<steady_consumer_tile, grid_setup_consumer>
                                    ( get_map(), f_floor_lamp_on );
        test_steady_consumer( setup );
    }
}

TEST_CASE( "charge_watcher_in_bubble", "[grids]" )
{
    clear_all_state();
    calendar::turn = calendar::turn_zero;
    put_player_underground();

    GIVEN( "watcher and battery are on one grid" ) {
        grid_setup_watcher setup = set_up_grid_with_consumer<charge_watcher_tile, grid_setup_watcher>
                                   ( get_map(), f_floor_lamp );
        test_charge_watcher( setup );
    }
}

TEST_CASE( "grid_furn_transform_queue_in_bubble", "[grids]" )
{
    clear_all_state();
    calendar::turn = calendar::turn_zero;
    put_player_underground();

    tripoint pos_local( 22, 7, 0 );
    tripoint_abs_ms pos_abs( get_map().getabs( pos_local ) );

    grid_furn_transform_queue tf_queue;
    tf_queue.add( pos_abs, f_floor_lamp_on, "" );

    CAPTURE( pos_abs );
    REQUIRE( get_map().furn( pos_local ).id() != f_floor_lamp_on );
    REQUIRE( active_tiles::furn_at<active_tile_data>( pos_abs ) == nullptr );

    tf_queue.apply( MAPBUFFER, get_distribution_grid_tracker(), get_player_character(), get_map() );

    REQUIRE( get_map().furn( pos_local ).id() == f_floor_lamp_on );
    REQUIRE( active_tiles::furn_at<steady_consumer_tile>( pos_abs ) != nullptr );
}

TEST_CASE( "grid_furn_transform_queue_outside_bubble", "[grids]" )
{
    clear_all_state();
    calendar::turn = calendar::turn_zero;
    put_player_underground();

    tripoint pos_local( 22, 7, 0 );
    tripoint_abs_ms pos_abs( get_map().getabs( pos_local ) );
    tripoint_abs_sm pos_abs_sm;
    point_sm_ms pos_in_sm;
    std::tie( pos_abs_sm, pos_in_sm ) = project_remain<coords::sm>( pos_abs );

    // Ugly: we move the real map to have submap exist in mapbuffer only
    map &m = get_map();
    m.load( m.get_abs_sub() + point( m.getmapsize(), 0 ), true );

    grid_furn_transform_queue tf_queue;
    tf_queue.add( pos_abs, f_floor_lamp_on, "" );

    submap *sm = nullptr;
    CAPTURE( pos_abs );
    CAPTURE( pos_abs_sm );

    sm = MAPBUFFER.lookup_submap( pos_abs_sm );
    REQUIRE( sm );
    REQUIRE( sm->get_furn( pos_in_sm.raw() ).id() != f_floor_lamp_on );
    REQUIRE( active_tiles::furn_at<active_tile_data>( pos_abs ) == nullptr );

    tf_queue.apply( MAPBUFFER, get_distribution_grid_tracker(), get_player_character(), get_map() );

    sm = MAPBUFFER.lookup_submap( pos_abs_sm );
    REQUIRE( sm );
    REQUIRE( sm->get_furn( pos_in_sm.raw() ).id() == f_floor_lamp_on );
    REQUIRE( active_tiles::furn_at<steady_consumer_tile>( pos_abs ) != nullptr );
}

TEST_CASE( "grid_power_stats", "[grids]" )
{
    clear_all_state();
    put_player_underground();
    clear_grid_connections( get_map() );

    GIVEN( "battery, solar panel and consumer on one grid" ) {
        const auto solar_local = tripoint( 15, 10, 0 );
        const auto consumer_local = tripoint( 13, 10, 0 );
        const auto battery_local = tripoint( 14, 10, 0 );
        const auto solar_abs =  tripoint_abs_ms( get_map().getabs( solar_local ) );
        const auto consumer_abs =  tripoint_abs_ms( get_map().getabs( consumer_local ) );

        get_map().furn_set( consumer_local, f_floor_lamp_on );
        get_map().furn_set( battery_local, f_battery );
        get_map().furn_set( solar_local, f_solar_unit );

        auto &grid = get_distribution_grid_tracker().grid_at( consumer_abs );
        auto *solar = active_tiles::furn_at<solar_tile>( solar_abs );
        auto *consumer = active_tiles::furn_at<steady_consumer_tile>( consumer_abs );
        REQUIRE( consumer );

        auto [gen, cons] = grid.get_power_stat();
        THEN( "grid reports solar generation and consumer power consumption" ) {
            CHECK( gen == solar->get_power_w() );
            CHECK( cons == consumer->power );
        }
    }
}
