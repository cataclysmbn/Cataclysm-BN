#include "catch/catch.hpp"

#include "calendar.h"
#include "game_constants.h"
#include "item.h"
#include "stored_ammo.h"
#include "type_id.h"

namespace
{

const auto itype_battery = itype_id( "battery" );
const auto itype_plut_cell = itype_id( "plut_cell" );

} // namespace

TEST_CASE( "stored ammo conversion itemizes only complete fuel cells", "[stored_ammo]" )
{
    CHECK( stored_ammo_item_charges( itype_battery, 42 ) == 42 );
    CHECK( stored_ammo_charges_for_items( itype_battery, 42 ) == 42 );

    CHECK( stored_ammo_item_charges( itype_plut_cell, PLUTONIUM_CHARGES - 1 ) == 0 );
    CHECK( stored_ammo_item_charges( itype_plut_cell, PLUTONIUM_CHARGES ) == 1 );
    CHECK( stored_ammo_item_charges( itype_plut_cell, PLUTONIUM_CHARGES * 2 + 10 ) == 2 );
    CHECK( stored_ammo_charges_for_items( itype_plut_cell, 2 ) == PLUTONIUM_CHARGES * 2 );
}

TEST_CASE( "stored ammo recovery has explicit partial-remainder policy", "[stored_ammo]" )
{
    auto tool = item::spawn( "adv_UPS_off", calendar::turn, PLUTONIUM_CHARGES + 10 );
    REQUIRE( tool->ammo_current() == itype_plut_cell );

    auto recovered = recover_stored_ammo( *tool, stored_ammo_remainder_handling::preserve );

    REQUIRE( recovered.size() == 1 );
    CHECK( recovered.front()->typeId() == itype_plut_cell );
    CHECK( recovered.front()->charges == 1 );
    CHECK( tool->ammo_remaining() == 10 );

    recovered = recover_stored_ammo( *tool, stored_ammo_remainder_handling::discard );

    CHECK( recovered.empty() );
    CHECK( tool->ammo_remaining() == 0 );
}
