#include "catch/catch.hpp"

#include "avatar.h"
#include "item.h"
#include "map.h"
#include "ranged.h"
#include "state_helpers.h"
#include "type_id.h"
#include "veh_type.h"
#include "vehicle.h"

TEST_CASE( "firing_from_a_vehicle_applies_recoil_to_the_vehicle", "[vehicle][gun]" )
{
    clear_all_state();

    auto &here = get_map();
    auto &player_character = get_avatar();
    const auto vehicle_origin = tripoint( 60, 60, 0 );

    auto *const veh = here.add_vehicle( vproto_id( "bicycle" ), vehicle_origin, 0_degrees, 0, 0 );
    REQUIRE( veh != nullptr );

    player_character.setpos( vehicle_origin );
    here.board_vehicle( vehicle_origin, &player_character );
    REQUIRE( player_character.in_vehicle );

    auto gun = item::spawn( itype_id( "m1014" ) );
    gun->ammo_set( itype_id( "shot_bird" ) );
    player_character.wield( std::move( gun ) );
    REQUIRE( player_character.primary_weapon().typeId() == itype_id( "m1014" ) );

    REQUIRE( veh->velocity == 0 );

    const auto shots_fired = ranged::fire_gun( player_character, vehicle_origin + tripoint( 5, 0, 0 ),
                             1 );

    REQUIRE( shots_fired == 1 );
    CHECK( veh->velocity != 0 );
}
