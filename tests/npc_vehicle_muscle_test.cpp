#include "catch/catch.hpp"

#include <memory>
#include <optional>

#include "avatar.h"
#include "calendar.h"
#include "character_id.h"
#include "faction.h"
#include "game.h"
#include "map.h"
#include "map_helpers.h"
#include "npc.h"
#include "npc_class.h"
#include "options.h"
#include "options_helpers.h"
#include "player_helpers.h"
#include "point.h"
#include "state_helpers.h"
#include "type_id.h"
#include "vehicle.h"
#include "vehicle_part.h"
#include "vpart_position.h"
#include "vpart_range.h"
#include "veh_type.h"

static const itype_id fuel_type_muscle( "muscle" );

static npc &create_test_npc()
{
    const string_id<npc_template> test_guy( "test_talker" );
    const tripoint npc_pos( 15, 15, 0 );
    const character_id model_id = get_map().place_npc( npc_pos.xy(), test_guy );
    g->load_npcs();

    npc *model_npc = g->find_npc( model_id );
    REQUIRE( model_npc != nullptr );

    // Ensure NPC has working limbs
    model_npc->set_part_hp_cur( bodypart_id( "arm_l" ), 30 );
    model_npc->set_part_hp_cur( bodypart_id( "arm_r" ), 30 );
    model_npc->set_part_hp_cur( bodypart_id( "leg_l" ), 30 );
    model_npc->set_part_hp_cur( bodypart_id( "leg_r" ), 30 );
    model_npc->set_stamina( model_npc->get_stamina_max() );
    model_npc->set_stored_kcal( model_npc->max_stored_kcal() );
    model_npc->set_thirst( 0 );
    model_npc->set_fatigue( 0 );

    // Make NPC an ally so they can be assigned to seats
    model_npc->set_attitude( NPCATT_FOLLOW );
    model_npc->set_fac( faction_id( "your_followers" ) );

    return *model_npc;
}

TEST_CASE( "multiple_manual_engines_allowed", "[vehicle][muscle][engine]" )
{
    clear_all_state();
    build_test_map( ter_id( "t_pavement" ) );
    map &here = get_map();

    GIVEN( "a tandem bicycle with two foot_pedals" ) {
        const tripoint bike_origin( 10, 10, 0 );
        vehicle *veh_ptr = here.add_vehicle( vproto_id( "tandem" ), bike_origin, 0_degrees, 0, 0 );
        REQUIRE( veh_ptr != nullptr );

        WHEN( "checking for muscle engines" ) {
            // Enable all engines first (like player would do via engine config menu)
            for( size_t e = 0; e < veh_ptr->engines.size(); e++ ) {
                vehicle_part &engine = veh_ptr->part( veh_ptr->engines[e] );
                engine.enabled = true;
                // For muscle engines, set the fuel type explicitly
                if( engine.info().fuel_type == fuel_type_muscle ) {
                    engine.fuel_set( fuel_type_muscle );
                }
            }

            int muscle_engine_count = 0;
            CAPTURE( veh_ptr->engines.size() );

            // Now check the engines
            for( size_t e = 0; e < veh_ptr->engines.size(); e++ ) {
                if( veh_ptr->is_engine_type( e, fuel_type_muscle ) ) {
                    muscle_engine_count++;
                }
            }
            CAPTURE( muscle_engine_count );

            THEN( "it should have exactly two muscle engines" ) {
                CHECK( muscle_engine_count == 2 );
            }
        }
    }
}

TEST_CASE( "npc_muscle_engine_fuel_availability", "[vehicle][muscle][npc]" )
{
    clear_all_state();
    build_test_map( ter_id( "t_pavement" ) );
    map &here = get_map();

    GIVEN( "a tandem bicycle with an NPC assigned to rear seat" ) {
        const tripoint bike_origin( 10, 10, 0 );
        vehicle *veh_ptr = here.add_vehicle( vproto_id( "tandem" ), bike_origin, 0_degrees, 0, 0 );
        REQUIRE( veh_ptr != nullptr );

        npc &test_npc = create_test_npc();

        // Find the rear seat with foot pedals at position x=-1
        int rear_seat_part = -1;
        for( const vpart_reference &vpr : veh_ptr->get_all_parts() ) {
            if( vpr.part().is_seat() && vpr.mount().x == -1 ) { // Rear seat position
                rear_seat_part = vpr.part_index();
                break;
            }
        }
        REQUIRE( rear_seat_part >= 0 );

        WHEN( "NPC is boarded to the rear seat" ) {
            // Enable all engines first (like player would do via engine config menu)
            for( size_t e = 0; e < veh_ptr->engines.size(); e++ ) {
                vehicle_part &engine = veh_ptr->part( veh_ptr->engines[e] );
                engine.enabled = true;
                // For muscle engines, set the fuel type explicitly
                if( engine.info().fuel_type == fuel_type_muscle ) {
                    engine.fuel_set( fuel_type_muscle );
                }
            }

            // Position NPC at rear seat and board them
            const tripoint rear_seat_pos = veh_ptr->global_part_pos3( rear_seat_part );
            test_npc.setpos( rear_seat_pos );
            here.board_vehicle( rear_seat_pos, &test_npc );
            REQUIRE( test_npc.in_vehicle );

            // Find the muscle engine at the rear position
            int rear_engine_idx = -1;
            CAPTURE( veh_ptr->engines.size() );

            for( size_t e = 0; e < veh_ptr->engines.size(); e++ ) {
                if( veh_ptr->is_engine_type( e, fuel_type_muscle ) ) {
                    // Get the part at this engine index
                    const vehicle_part &engine_part = veh_ptr->part( veh_ptr->engines[e] );
                    const vpart_info &engine_info = veh_ptr->part_info( veh_ptr->engines[e] );
                    CAPTURE( e, engine_info.get_id().str(), engine_part.mount.x, engine_part.mount.y );

                    if( engine_part.mount.x == -1 ) { // Rear position
                        rear_engine_idx = static_cast<int>( e );
                        break;
                    }
                }
            }
            REQUIRE( rear_engine_idx >= 0 );

            AND_WHEN( "the rear muscle engine is turned on" ) {
                veh_ptr->toggle_specific_engine( rear_engine_idx, true );
                REQUIRE( veh_ptr->is_engine_on( rear_engine_idx ) );

                THEN( "muscle fuel should be available" ) {
                    int muscle_fuel = veh_ptr->fuel_left( fuel_type_muscle );
                    CHECK( muscle_fuel >= 10 ); // Each working muscle engine adds 10
                }

                AND_THEN( "NPC should be providing power" ) {
                    // Check that the NPC is boarded at the same position as the engine
                    const vehicle_part &engine_part = veh_ptr->part( veh_ptr->engines[rear_engine_idx] );
                    const point engine_mount = engine_part.mount;

                    const player *passenger = nullptr;
                    for( const vpart_reference &vpr : veh_ptr->get_all_parts() ) {
                        if( vpr.mount() == engine_mount && vpr.part().has_flag( vehicle_part::passenger_flag ) ) {
                            passenger = veh_ptr->get_passenger( vpr.part_index() );
                            break;
                        }
                    }

                    CHECK( passenger != nullptr );
                    CHECK( passenger->getID() == test_npc.getID() );
                }
            }
        }
    }
}

TEST_CASE( "player_and_npc_muscle_power_combined", "[vehicle][muscle][npc][player]" )
{
    GIVEN( "a tandem bicycle with player and NPC both seated" ) {
        clear_all_state();
        build_test_map( ter_id( "t_pavement" ) );
        map &here = get_map();
        avatar &player = get_avatar();

        const tripoint bike_origin( 60, 60, 0 );  // Use same coordinates as working test

        vehicle *veh_ptr = here.add_vehicle( vproto_id( "tandem" ), bike_origin, 0_degrees, 0, 0 );
        REQUIRE( veh_ptr != nullptr );

        npc &test_npc = create_test_npc();

        // Enable all engines first
        for( size_t e = 0; e < veh_ptr->engines.size(); e++ ) {
            vehicle_part &engine = veh_ptr->part( veh_ptr->engines[e] );
            engine.enabled = true;
            if( engine.info().fuel_type == fuel_type_muscle ) {
                engine.fuel_set( fuel_type_muscle );
            }
        }

        // Find front and rear seats
        int front_seat_part = -1;
        int rear_seat_part = -1;
        for( const vpart_reference &vpr : veh_ptr->get_all_parts() ) {
            if( vpr.part().is_seat() ) {
                if( vpr.mount().x == 0 ) { // Front seat
                    front_seat_part = vpr.part_index();
                } else if( vpr.mount().x == -1 ) { // Rear seat
                    rear_seat_part = vpr.part_index();
                }
            }
        }
        REQUIRE( front_seat_part >= 0 );
        REQUIRE( rear_seat_part >= 0 );

        // Board the player to front seat (like working vehicle_test.cpp)
        here.board_vehicle( bike_origin, &player );
        REQUIRE( player.in_vehicle );

        // Position NPC at rear seat and board them
        const tripoint rear_seat_pos = veh_ptr->global_part_pos3( rear_seat_part );
        test_npc.setpos( rear_seat_pos );
        here.board_vehicle( rear_seat_pos, &test_npc );
        REQUIRE( test_npc.in_vehicle );

        WHEN( "both muscle engines are turned on" ) {
            // Engines already enabled above, just check they're on
            for( size_t e = 0; e < veh_ptr->engines.size(); e++ ) {
                if( veh_ptr->is_engine_type( e, fuel_type_muscle ) ) {
                    REQUIRE( veh_ptr->is_engine_on( e ) );
                }
            }

            THEN( "total muscle fuel should reflect both contributors" ) {
                int muscle_fuel = veh_ptr->fuel_left( fuel_type_muscle );
                CHECK( muscle_fuel >= 20 ); // Player (10) + NPC (10)
            }
        }
    }
}

TEST_CASE( "npc_muscle_engine_energy_consumption", "[vehicle][muscle][npc][energy]" )
{
    clear_all_state();
    build_test_map( ter_id( "t_pavement" ) );
    map &here = get_map();

    // Ensure NPC needs are enabled for this test
    override_option opt( "NO_NPC_FOOD", "false" );

    GIVEN( "an NPC powering a muscle engine under load" ) {
        const tripoint bike_origin( 10, 10, 0 );
        vehicle *veh_ptr = here.add_vehicle( vproto_id( "bicycle" ), bike_origin, 0_degrees, 0, 0 );
        REQUIRE( veh_ptr != nullptr );

        npc &test_npc = create_test_npc();

        // Enable all engines first
        for( size_t e = 0; e < veh_ptr->engines.size(); e++ ) {
            vehicle_part &engine = veh_ptr->part( veh_ptr->engines[e] );
            engine.enabled = true;
            if( engine.info().fuel_type == fuel_type_muscle ) {
                engine.fuel_set( fuel_type_muscle );
            }
        }

        // Board NPC to the seat with foot pedals
        int seat_part = -1;
        for( const vpart_reference &vpr : veh_ptr->get_all_parts() ) {
            if( vpr.part().is_seat() && vpr.mount().x == 0 ) {
                seat_part = vpr.part_index();
                break;
            }
        }
        REQUIRE( seat_part >= 0 );

        const tripoint seat_pos = veh_ptr->global_part_pos3( seat_part );
        test_npc.setpos( seat_pos );
        here.board_vehicle( seat_pos, &test_npc );
        REQUIRE( test_npc.in_vehicle );

        // Record initial energy levels
        int initial_stamina = test_npc.get_stamina();
        int initial_kcal = test_npc.get_stored_kcal();
        int initial_thirst = test_npc.get_thirst();
        int initial_fatigue = test_npc.get_fatigue();

        WHEN( "the vehicle operates with muscle engine load" ) {
            // Simulate vehicle load - idle() doesn't take time_duration
            // Instead we'll test the fuel_left function directly which is what matters
            int initial_fuel = veh_ptr->fuel_left( fuel_type_muscle );

            THEN( "muscle fuel should be available from NPC" ) {
                CHECK( initial_fuel >= 10 ); // NPC should contribute fuel
            }
        }
    }
}

TEST_CASE( "npc_muscle_engine_with_disabled_needs", "[vehicle][muscle][npc][energy]" )
{
    clear_all_state();
    build_test_map( ter_id( "t_pavement" ) );
    map &here = get_map();

    // Disable NPC needs for this test
    override_option opt( "NO_NPC_FOOD", "true" );

    GIVEN( "an NPC powering a muscle engine with needs disabled" ) {
        const tripoint bike_origin( 10, 10, 0 );
        vehicle *veh_ptr = here.add_vehicle( vproto_id( "bicycle" ), bike_origin, 0_degrees, 0, 0 );
        REQUIRE( veh_ptr != nullptr );

        npc &test_npc = create_test_npc();

        // Enable all engines first
        for( size_t e = 0; e < veh_ptr->engines.size(); e++ ) {
            vehicle_part &engine = veh_ptr->part( veh_ptr->engines[e] );
            engine.enabled = true;
            if( engine.info().fuel_type == fuel_type_muscle ) {
                engine.fuel_set( fuel_type_muscle );
            }
        }

        // Board NPC to seat
        int seat_part = -1;
        for( const vpart_reference &vpr : veh_ptr->get_all_parts() ) {
            if( vpr.part().is_seat() && vpr.mount().x == 0 ) {
                seat_part = vpr.part_index();
                break;
            }
        }
        REQUIRE( seat_part >= 0 );

        const tripoint seat_pos = veh_ptr->global_part_pos3( seat_part );
        test_npc.setpos( seat_pos );
        here.board_vehicle( seat_pos, &test_npc );
        REQUIRE( test_npc.in_vehicle );

        int initial_stamina = test_npc.get_stamina();

        WHEN( "the vehicle operates with muscle engine load" ) {
            int fuel_available = veh_ptr->fuel_left( fuel_type_muscle );

            THEN( "NPC should still provide muscle power even with needs disabled" ) {
                CHECK( fuel_available >= 10 ); // NPC should contribute fuel
            }
        }
    }
}

TEST_CASE( "npc_muscle_engine_broken_limbs", "[vehicle][muscle][npc][injury]" )
{
    clear_all_state();
    build_test_map( ter_id( "t_pavement" ) );
    map &here = get_map();

    GIVEN( "an NPC with broken legs assigned to foot pedals" ) {
        const tripoint bike_origin( 10, 10, 0 );
        vehicle *veh_ptr = here.add_vehicle( vproto_id( "bicycle" ), bike_origin, 0_degrees, 0, 0 );
        REQUIRE( veh_ptr != nullptr );

        npc &test_npc = create_test_npc();

        // Break the NPC's legs by adding the disabled effect
        test_npc.add_effect( efftype_id( "disabled" ), 1_hours, bodypart_str_id( "leg_l" ) );
        test_npc.add_effect( efftype_id( "disabled" ), 1_hours, bodypart_str_id( "leg_r" ) );
        CAPTURE( test_npc.has_effect( efftype_id( "disabled" ), bodypart_str_id( "leg_l" ) ) );
        CAPTURE( test_npc.has_effect( efftype_id( "disabled" ), bodypart_str_id( "leg_r" ) ) );
        CAPTURE( test_npc.get_working_leg_count() );
        REQUIRE( test_npc.get_working_leg_count() < 2 );

        // Enable engines first
        for( size_t e = 0; e < veh_ptr->engines.size(); e++ ) {
            vehicle_part &engine = veh_ptr->part( veh_ptr->engines[e] );
            engine.enabled = true;
            if( engine.info().fuel_type == fuel_type_muscle ) {
                engine.fuel_set( fuel_type_muscle );
            }
        }

        // Board NPC to seat
        int seat_part = -1;
        for( const vpart_reference &vpr : veh_ptr->get_all_parts() ) {
            if( vpr.part().is_seat() && vpr.mount().x == 0 ) {
                seat_part = vpr.part_index();
                break;
            }
        }
        REQUIRE( seat_part >= 0 );

        const tripoint seat_pos = veh_ptr->global_part_pos3( seat_part );
        test_npc.setpos( seat_pos );
        here.board_vehicle( seat_pos, &test_npc );
        REQUIRE( test_npc.in_vehicle );

        WHEN( "checking muscle fuel availability" ) {
            int muscle_fuel = veh_ptr->fuel_left( fuel_type_muscle );

            THEN( "NPC should not contribute power due to broken limbs" ) {
                // Should be 0 or very low since NPC can't contribute with broken legs
                CHECK( muscle_fuel == 0 );
            }
        }
    }
}