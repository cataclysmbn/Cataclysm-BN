#include "catch/catch.hpp"

#include "map.h"
#include "state_helpers.h"
#include "type_id.h"
#include "vehicle.h"
#include "vehicle_control_helpers.h"

TEST_CASE( "vertical_controls_prioritize_remote_vehicle" )
{
    clear_all_state();

    auto &here = get_map();
    const auto test_origin = tripoint( 60, 60, 0 );
    auto *player_vehicle = here.add_vehicle( vproto_id( "bicycle" ), test_origin, 0_degrees, 0, 0 );
    auto *remote_vehicle = here.add_vehicle( vproto_id( "bicycle" ), test_origin + tripoint_east * 10,
                           0_degrees, 0, 0 );

    REQUIRE( player_vehicle != nullptr );
    REQUIRE( remote_vehicle != nullptr );

    CHECK( vehicle_control_helpers::get_vertical_controlled_vehicle( player_vehicle, true,
            remote_vehicle ) == remote_vehicle );
    CHECK( vehicle_control_helpers::get_vertical_controlled_vehicle( player_vehicle, true,
            nullptr ) == player_vehicle );
    CHECK( vehicle_control_helpers::get_vertical_controlled_vehicle( player_vehicle, false,
            nullptr ) == nullptr );
}
