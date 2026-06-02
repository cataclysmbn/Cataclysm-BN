#include "catch/catch.hpp"

#include "coordinates.h"
#include "game.h"
#include "map.h"
#include "map_helpers.h"
#include "monster.h"
#include "state_helpers.h"
#include "type_id.h"
#include "vehicle.h"

TEST_CASE( "mps_cmps_round_trip_converges_to_zero", "[vehicle]" )
{
    constexpr auto max_iterations = 200;

    for( auto v = 1; v <= 50; ++v ) {
        auto coll_velocity = v;
        auto iterations = 0;
        while( coll_velocity > 0 && iterations < max_iterations ) {
            const auto vel_mps = cmps_to_mps( coll_velocity );
            const auto new_velocity = mps_to_cmps( vel_mps * 0.9 );
            coll_velocity = ( std::abs( new_velocity ) >= std::abs( coll_velocity ) )
                            ? 0
                            : new_velocity;
            ++iterations;
        }
        CAPTURE( v );
        CHECK( coll_velocity == 0 );
        CHECK( iterations < max_iterations );
    }
}

TEST_CASE( "vehicle_collision_with_wall_terminates", "[vehicle]" )
{
    clear_all_state();
    auto &here = get_map();
    build_test_map( ter_id( "t_pavement" ) );
    clear_vehicles();

    const auto veh_pos = tripoint_bub_ms( 60, 60, 0 );
    const auto wall_pos = tripoint_bub_ms( 60, 59, 0 );

    auto *veh_ptr = here.add_vehicle( vproto_id( "bicycle_test" ), veh_pos, 270_degrees, 0, 0 );
    REQUIRE( veh_ptr != nullptr );

    REQUIRE( here.ter_set( wall_pos, ter_id( "t_concrete_wall" ) ) );
    here.build_map_cache( 0, true );

    CAPTURE( here.ter( wall_pos ).id().str() );
    CAPTURE( here.move_cost_ter_furn( wall_pos ) );
    REQUIRE( here.impassable_ter_furn( wall_pos ) );

    veh_ptr->velocity = 222;
    const auto probe = veh_ptr->part_collision( vehicle_part_collision_options{
        .part = 0,
        .pos = wall_pos,
        .just_detect = true,
    } );
    REQUIRE( probe.type != veh_coll_nothing );

    veh_ptr->velocity = 222;
    const auto ret = veh_ptr->part_collision( vehicle_part_collision_options{
        .part = 0,
        .pos = wall_pos,
    } );

    CHECK( ret.type != veh_coll_nothing );
    CHECK( std::abs( veh_ptr->velocity ) < 222 );
}

TEST_CASE( "vehicle_collision_with_hallucination_terminates", "[vehicle]" )
{
    clear_all_state();
    auto &here = get_map();
    build_test_map( ter_id( "t_pavement" ) );
    clear_vehicles();

    const auto veh_pos = tripoint_bub_ms( 60, 60, 0 );
    const auto hallucination_pos = tripoint_bub_ms( 60, 59, 0 );

    auto *veh_ptr = here.add_vehicle( vproto_id( "bicycle_test" ), veh_pos, 270_degrees, 0, 0 );
    REQUIRE( veh_ptr != nullptr );

    auto &hallucination = spawn_test_monster( "mon_chicken", hallucination_pos );
    hallucination.hallucination = true;
    REQUIRE( g->critter_at<monster>( hallucination_pos, true ) == &hallucination );

    veh_ptr->velocity = 222;
    const auto ret = veh_ptr->part_collision( vehicle_part_collision_options{
        .part = 0,
        .pos = hallucination_pos,
    } );

    CHECK( ret.type == veh_coll_body );
    CHECK( hallucination.is_dead() );
    CHECK( veh_ptr->velocity == 222 );
}
