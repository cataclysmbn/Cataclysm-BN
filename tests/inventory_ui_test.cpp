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

    REQUIRE( selector.select_item_type( map_bandages->typeId(), 1 ) );
    const auto saved_position = selector.get_selection_position();
    REQUIRE( selector.select( inventory_bandages.get() ) );

    REQUIRE( selector.restore_selection( saved_position, map_bandages->typeId() ) );
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

    REQUIRE( selector.select_item_type( map_heroin->typeId(), 1 ) );
    const auto stale_position = selector.get_selection_position();
    REQUIRE( selector.select( inventory_bandages.get() ) );

    CHECK_FALSE( selector.select_position_if_item_type( stale_position,
                 inventory_bandages->typeId() ) );
    CHECK( selector.get_selected().any_item() == inventory_bandages.get() );
}

TEST_CASE( "inventory selector restores saved row before same type fallback in the same column",
           "[inventory][ui][consume]" )
{
    clear_avatar();
    auto &dummy = get_avatar();

    auto inventory_bandages = item::spawn( "bandages" );
    auto first_map_bandages = item::spawn( "bandages" );
    auto second_map_bandages = item::spawn( "bandages" );

    auto selector = inventory_selector( dummy );
    selector.add_item( selector.own_inv_column, inventory_bandages.get() );
    selector.add_item( selector.map_column, first_map_bandages.get() );
    selector.add_item( selector.map_column, second_map_bandages.get() );

    REQUIRE( selector.select_item_type( first_map_bandages->typeId(), 1 ) );
    REQUIRE( selector.select( second_map_bandages.get() ) );
    const auto saved_position = selector.get_selection_position();
    REQUIRE( selector.select( inventory_bandages.get() ) );

    REQUIRE( selector.restore_selection( saved_position, second_map_bandages->typeId() ) );
    CHECK( selector.get_selected().any_item() == second_map_bandages.get() );
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

    REQUIRE( selector.select_item_type( map_heroin->typeId(), 1 ) );
    const auto stale_position = selector.get_selection_position();
    REQUIRE( selector.select( inventory_bandages.get() ) );

    REQUIRE( selector.restore_selection( stale_position, map_bandages->typeId() ) );
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

    REQUIRE( selector.select_item_type( map_heroin->typeId(), 1 ) );
    const auto stale_position = selector.get_selection_position();

    REQUIRE( selector.restore_selection( stale_position, inventory_bandages->typeId() ) );
    CHECK( selector.get_selected().any_item() == inventory_bandages.get() );
}

TEST_CASE( "inventory selector type fallback resumes default order after saved column",
           "[inventory][ui][consume]" )
{
    clear_avatar();
    auto &dummy = get_avatar();

    auto inventory_bandages = item::spawn( "bandages" );
    auto gear_bandages = item::spawn( "bandages" );
    auto map_heroin = item::spawn( "heroin" );

    auto selector = inventory_selector( dummy );
    selector.add_item( selector.own_inv_column, inventory_bandages.get() );
    selector.add_item( selector.own_gear_column, gear_bandages.get() );
    selector.add_item( selector.map_column, map_heroin.get() );

    REQUIRE( selector.select_item_type( map_heroin->typeId(), 1 ) );
    const auto stale_position = selector.get_selection_position();

    REQUIRE( selector.restore_selection( stale_position, inventory_bandages->typeId() ) );
    CHECK( selector.get_selected().any_item() == inventory_bandages.get() );
}

TEST_CASE( "inventory selector does not restore a stale row when saved type is gone",
           "[inventory][ui][consume]" )
{
    clear_avatar();
    auto &dummy = get_avatar();

    auto inventory_aspirin = item::spawn( "aspirin" );
    auto inventory_heroin = item::spawn( "heroin" );
    auto map_heroin = item::spawn( "heroin" );
    auto missing_bandages = item::spawn( "bandages" );

    auto selector = inventory_selector( dummy );
    selector.add_item( selector.own_inv_column, inventory_aspirin.get() );
    selector.add_item( selector.own_inv_column, inventory_heroin.get() );
    selector.add_item( selector.map_column, map_heroin.get() );

    REQUIRE( selector.select_item_type( map_heroin->typeId(), 1 ) );
    const auto stale_position = selector.get_selection_position();

    selector.clear_items();
    selector.add_item( selector.own_inv_column, inventory_aspirin.get() );
    selector.add_item( selector.own_inv_column, inventory_heroin.get() );
    selector.add_item( selector.map_column, map_heroin.get() );
    REQUIRE( selector.select_item_type( map_heroin->typeId(), 1 ) );
    const auto expected_entries = selector.own_inv_column.get_entries( []( const auto & entry ) { return entry.is_selectable(); } );
    const auto expected_default = expected_entries.front()->any_item();

    CHECK_FALSE( selector.restore_selection( stale_position, missing_bandages->typeId() ) );
    CHECK( selector.get_selected().any_item() == expected_default );
}

TEST_CASE( "inventory selector falls back from out of range saved row",
           "[inventory][ui][consume]" )
{
    clear_avatar();
    auto &dummy = get_avatar();

    auto inventory_bandages = item::spawn( "bandages" );
    auto map_bandages = item::spawn( "bandages" );

    auto selector = inventory_selector( dummy );
    selector.add_item( selector.own_inv_column, inventory_bandages.get() );
    selector.add_item( selector.map_column, map_bandages.get() );

    REQUIRE( selector.select_item_type( map_bandages->typeId(), 1 ) );
    auto invalid_position = selector.get_selection_position();
    invalid_position.second = 999;
    REQUIRE( selector.select( inventory_bandages.get() ) );

    REQUIRE( selector.restore_selection( invalid_position, map_bandages->typeId() ) );
    CHECK( selector.get_selected().any_item() == map_bandages.get() );
}

TEST_CASE( "inventory selector falls back from invalid saved consume column",
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
    const auto invalid_position = std::pair<size_t, size_t> { 99, 0 };

    REQUIRE( selector.restore_selection( invalid_position, map_bandages->typeId() ) );
    CHECK( selector.get_selected().any_item() == inventory_bandages.get() );
}
