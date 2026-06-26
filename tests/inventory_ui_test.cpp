#include "catch/catch.hpp"

#include "avatar.h"
#include "inventory_ui.h"
#include "item.h"
#include "player_helpers.h"

TEST_CASE( "inventory selector restores consume selection by item type",
           "[inventory][ui][consume]" )
{
    clear_avatar();
    auto &dummy = get_avatar();

    auto bandages = item::spawn( "bandages" );
    auto heroin = item::spawn( "heroin" );

    auto selector = inventory_selector( dummy );
    selector.add_item( selector.own_inv_column, heroin.get() );
    selector.add_item( selector.own_inv_column, bandages.get() );

    REQUIRE( selector.select_item_type( bandages->typeId() ) );
    CHECK( selector.get_selected().any_item()->typeId() == bandages->typeId() );
}

TEST_CASE( "inventory selector restores saved position before same type in another column",
           "[inventory][ui][consume]" )
{
    clear_avatar();
    auto &dummy = get_avatar();

    auto inventory_bandages = item::spawn( "bandages" );
    auto map_bandages = item::spawn( "bandages" );

    auto selector = inventory_selector( dummy );
    selector.add_item( selector.own_inv_column, inventory_bandages.get() );
    selector.add_item( selector.map_column, map_bandages.get() );

    REQUIRE( selector.select( map_bandages.get() ) );
    const auto saved_position = selector.get_selection_position();
    REQUIRE( selector.select( inventory_bandages.get() ) );

    REQUIRE( selector.select_position_if_item_type( saved_position, map_bandages->typeId() ) );
    CHECK( selector.get_selected().any_item() == map_bandages.get() );
}

TEST_CASE( "inventory selector rejects saved position when the type changed",
           "[inventory][ui][consume]" )
{
    clear_avatar();
    auto &dummy = get_avatar();

    auto inventory_bandages = item::spawn( "bandages" );
    auto map_heroin = item::spawn( "heroin" );

    auto selector = inventory_selector( dummy );
    selector.add_item( selector.own_inv_column, inventory_bandages.get() );
    selector.add_item( selector.map_column, map_heroin.get() );

    REQUIRE( selector.select( map_heroin.get() ) );
    const auto stale_position = selector.get_selection_position();
    REQUIRE( selector.select( inventory_bandages.get() ) );

    CHECK_FALSE( selector.select_position_if_item_type( stale_position,
                 inventory_bandages->typeId() ) );
    CHECK( selector.get_selected().any_item() == inventory_bandages.get() );
}

TEST_CASE( "inventory selector type fallback starts in the saved column",
           "[inventory][ui][consume]" )
{
    clear_avatar();
    auto &dummy = get_avatar();

    auto inventory_bandages = item::spawn( "bandages" );
    auto map_heroin = item::spawn( "heroin" );
    auto map_bandages = item::spawn( "bandages" );

    auto selector = inventory_selector( dummy );
    selector.add_item( selector.own_inv_column, inventory_bandages.get() );
    selector.add_item( selector.map_column, map_heroin.get() );
    selector.add_item( selector.map_column, map_bandages.get() );

    REQUIRE( selector.select( map_heroin.get() ) );
    const auto stale_position = selector.get_selection_position();
    REQUIRE( selector.select( inventory_bandages.get() ) );

    REQUIRE_FALSE( selector.select_position_if_item_type( stale_position, map_bandages->typeId() ) );
    REQUIRE( selector.select_item_type( map_bandages->typeId(), stale_position.first ) );
    CHECK( selector.get_selected().any_item() == map_bandages.get() );
}

TEST_CASE( "inventory selector type fallback leaves saved column when needed",
           "[inventory][ui][consume]" )
{
    clear_avatar();
    auto &dummy = get_avatar();

    auto inventory_bandages = item::spawn( "bandages" );
    auto map_heroin = item::spawn( "heroin" );

    auto selector = inventory_selector( dummy );
    selector.add_item( selector.own_inv_column, inventory_bandages.get() );
    selector.add_item( selector.map_column, map_heroin.get() );

    REQUIRE( selector.select( map_heroin.get() ) );
    const auto stale_position = selector.get_selection_position();

    REQUIRE_FALSE( selector.select_position_if_item_type( stale_position,
                   inventory_bandages->typeId() ) );
    REQUIRE( selector.select_item_type( inventory_bandages->typeId(), stale_position.first ) );
    CHECK( selector.get_selected().any_item() == inventory_bandages.get() );
}

TEST_CASE( "inventory selector ignores invalid saved consume position",
           "[inventory][ui][consume]" )
{
    clear_avatar();
    auto &dummy = get_avatar();

    auto inventory_bandages = item::spawn( "bandages" );
    auto map_bandages = item::spawn( "bandages" );

    auto selector = inventory_selector( dummy );
    selector.add_item( selector.own_inv_column, inventory_bandages.get() );
    selector.add_item( selector.map_column, map_bandages.get() );

    REQUIRE( selector.select( inventory_bandages.get() ) );
    const auto invalid_position = std::pair<size_t, size_t>{ 99, 0 };

    CHECK_FALSE( selector.select_position_if_item_type( invalid_position, map_bandages->typeId() ) );
    CHECK( selector.get_selected().any_item() == inventory_bandages.get() );
}
