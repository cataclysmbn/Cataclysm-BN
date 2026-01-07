#include "catch/catch.hpp"

#include "game.h"
#include "item.h"
#include "map.h"
#include "map_helpers.h"
#include "state_helpers.h"
#include "type_id.h"
#include "vehicle.h"
#include "vehicle_part.h"
#include "vpart_position.h"

TEST_CASE( "towed_vehicle_reverses_when_target_behind", "[vehicle][towing]" )
{
    clear_all_state();
    build_test_map( ter_id( "t_pavement" ) );

    map &here = get_map();

    const tripoint towed_origin( 60, 60, 0 );
    const tripoint tower_origin = towed_origin + tripoint_west * 10;

    vehicle *towed = here.add_vehicle( vproto_id( "bicycle" ), towed_origin, 0_degrees, 0, 0 );
    vehicle *tower = here.add_vehicle( vproto_id( "bicycle" ), tower_origin, 180_degrees, 0, 0 );

    REQUIRE( towed != nullptr );
    REQUIRE( tower != nullptr );

    const auto add_tow_part = [&]( vehicle & veh, const tripoint & pos ) {
        const optional_vpart_position vp = here.veh_at( pos );
        REQUIRE( vp );

        const auto vcoords = vp->mount();
        auto tow_part = vehicle_part( vpart_id( "hd_tow_cable" ), vcoords, item::spawn( "hd_tow_cable" ),
                                      &veh );
        veh.install_part( vcoords, std::move( tow_part ) );
    };

    add_tow_part( *towed, towed_origin );
    add_tow_part( *tower, tower_origin );

    REQUIRE( tower->tow_data.set_towing( tower, towed ) );

    tower->velocity = 1000;
    towed->velocity = 0;

    REQUIRE( tower->no_towing_slack() );

    tower->do_towing_move();

    CHECK( towed->velocity < 0 );
}
