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
