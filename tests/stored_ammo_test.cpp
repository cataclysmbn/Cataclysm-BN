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

constexpr auto partial_plutonium_remainder = 10;

} // namespace

TEST_CASE( "stored ammo conversion rejects non-itemizable charges", "[stored_ammo]" )
{
    CHECK( stored_ammo_item_charges( itype_id::NULL_ID(), 42 ) == 0 );
    CHECK( stored_ammo_item_charges( itype_battery, 0 ) == 0 );
    CHECK( stored_ammo_item_charges( itype_battery, -1 ) == 0 );
    CHECK( stored_ammo_charges_for_items( itype_id::NULL_ID(), 42 ) == 0 );
    CHECK( stored_ammo_charges_for_items( itype_battery, 0 ) == 0 );
    CHECK( stored_ammo_charges_for_items( itype_battery, -1 ) == 0 );

    CHECK_FALSE( spawn_stored_ammo( itype_id::NULL_ID(), 42 ) );
    CHECK_FALSE( spawn_stored_ammo( itype_battery, 0 ) );
    CHECK_FALSE( spawn_stored_ammo( itype_plut_cell, PLUTONIUM_CHARGES - 1 ) );
}

TEST_CASE( "stored ammo conversion keeps one-to-one ammo charges", "[stored_ammo]" )
{
    CHECK( stored_ammo_item_charges( itype_battery, 42 ) == 42 );
    CHECK( stored_ammo_charges_for_items( itype_battery, 42 ) == 42 );

    auto battery = spawn_stored_ammo( itype_battery, 42 );

    REQUIRE( battery );
    CHECK( battery->typeId() == itype_battery );
    CHECK( battery->charges == 42 );
}

TEST_CASE( "stored ammo conversion itemizes only complete fuel cells", "[stored_ammo]" )
{
    CHECK( stored_ammo_item_charges( itype_plut_cell, PLUTONIUM_CHARGES - 1 ) == 0 );
    CHECK( stored_ammo_item_charges( itype_plut_cell, PLUTONIUM_CHARGES ) == 1 );
    CHECK( stored_ammo_item_charges( itype_plut_cell,
                                     PLUTONIUM_CHARGES * 2 + partial_plutonium_remainder ) == 2 );
    CHECK( stored_ammo_charges_for_items( itype_plut_cell, 2 ) == PLUTONIUM_CHARGES * 2 );

    auto fuel_cells = spawn_stored_ammo( itype_plut_cell,
                                         PLUTONIUM_CHARGES * 2 + partial_plutonium_remainder );

    REQUIRE( fuel_cells );
    CHECK( fuel_cells->typeId() == itype_plut_cell );
    CHECK( fuel_cells->charges == 2 );
}

TEST_CASE( "stored ammo recovery empties normal charged magazines", "[stored_ammo]" )
{
    auto battery_cell = item::spawn( "light_plus_battery_cell" );
    battery_cell->ammo_set( itype_battery, 42 );
    REQUIRE( battery_cell->ammo_remaining() == 42 );

    const auto recovered = recover_stored_ammo( *battery_cell,
                           stored_ammo_remainder_handling::preserve );

    REQUIRE( recovered.size() == 1 );
    CHECK( recovered.front()->typeId() == itype_battery );
    CHECK( recovered.front()->charges == 42 );
    CHECK( battery_cell->ammo_remaining() == 0 );
    CHECK( battery_cell->ammo_current().is_null() );
}

TEST_CASE( "stored ammo recovery handles exact complete fuel cells", "[stored_ammo]" )
{
    auto tool = item::spawn( "adv_UPS_off", calendar::turn, PLUTONIUM_CHARGES );
    REQUIRE( tool->ammo_current() == itype_plut_cell );

    const auto recovered = recover_stored_ammo( *tool, stored_ammo_remainder_handling::preserve );

    REQUIRE( recovered.size() == 1 );
    CHECK( recovered.front()->typeId() == itype_plut_cell );
    CHECK( recovered.front()->charges == 1 );
    CHECK( tool->ammo_remaining() == 0 );
}

TEST_CASE( "stored ammo recovery preserves partial fuel remainders", "[stored_ammo]" )
{
    auto partial_tool = item::spawn( "adv_UPS_off", calendar::turn, PLUTONIUM_CHARGES - 1 );
    REQUIRE( partial_tool->ammo_current() == itype_plut_cell );

    const auto partial_recovered = recover_stored_ammo( *partial_tool,
                                   stored_ammo_remainder_handling::preserve );

    CHECK( partial_recovered.empty() );
    CHECK( partial_tool->ammo_remaining() == PLUTONIUM_CHARGES - 1 );

    auto mixed_tool = item::spawn( "adv_UPS_off", calendar::turn,
                                   PLUTONIUM_CHARGES * 2 + partial_plutonium_remainder );
    REQUIRE( mixed_tool->ammo_current() == itype_plut_cell );

    const auto mixed_recovered = recover_stored_ammo( *mixed_tool,
                                 stored_ammo_remainder_handling::preserve );

    REQUIRE( mixed_recovered.size() == 1 );
    CHECK( mixed_recovered.front()->typeId() == itype_plut_cell );
    CHECK( mixed_recovered.front()->charges == 2 );
    CHECK( mixed_tool->ammo_remaining() == partial_plutonium_remainder );
}

TEST_CASE( "stored ammo recovery discards unitemizable consumed remainders", "[stored_ammo]" )
{
    auto tool = item::spawn( "adv_UPS_off", calendar::turn,
                             PLUTONIUM_CHARGES * 2 + partial_plutonium_remainder );
    REQUIRE( tool->ammo_current() == itype_plut_cell );

    const auto recovered = recover_stored_ammo( *tool, stored_ammo_remainder_handling::discard );

    REQUIRE( recovered.size() == 1 );
    CHECK( recovered.front()->typeId() == itype_plut_cell );
    CHECK( recovered.front()->charges == 2 );
    CHECK( tool->ammo_remaining() == 0 );
}
