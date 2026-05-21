#include "catch/catch.hpp"

#include <list>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "avatar.h"
#include "avatar_action.h"
#include "inventory.h"
#include "item.h"
#include "map.h"
#include "map_helpers.h"
#include "state_helpers.h"
#include "type_id.h"
#include "vehicle.h"
#include "vehicle_part.h"
#include "vehicle_selector.h"
#include "veh_type.h"

TEST_CASE( "reload_on_vehicle_cargo", "[magazine] [visitable] [item] [item_location]" )
{
    clear_all_state();
    const tripoint_bub_ms vehicle_center = tripoint_bub_ms( 65, 65, 0 );
    put_player_underground();

    map &here = get_map();
    const vproto_id car_id( "car" );
    const itype_id ups_id( "UPS_off" );
    vehicle *veh = here.add_vehicle( car_id, vehicle_center, 0_radians, 0, 0, false );
    REQUIRE( veh != nullptr );

    int part_num = veh->part_with_feature( 0, VPFLAG_CARGO, true );
    REQUIRE( part_num >= 0 );
    auto remaining = veh->add_item( part_num, item::spawn( ups_id ) );
    REQUIRE( !remaining );

    vehicle_cursor vc = vehicle_cursor( *veh, static_cast<std::ptrdiff_t>( part_num ) );

    item *it = nullptr;
    vc.visit_items( [&it, &ups_id]( item * e ) {
        if( e->typeId() == ups_id ) {
            it = e;
            return VisitResponse::ABORT;
        }
        return VisitResponse::NEXT;
    } );
    REQUIRE( it != nullptr );

    avatar_action::reload( *it, false, false );
}
