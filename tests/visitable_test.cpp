#include "catch/catch.hpp"

#include "calendar.h"
#include "inventory.h"
#include "item.h"

TEST_CASE( "visitable_summation" )
{
    inventory test_inv;

    detached_ptr<item> bottle_of_water = item::spawn( "bottle_plastic", calendar::turn );
    detached_ptr<item> water_in_bottle = item::spawn( "water", calendar::turn );
    water_in_bottle->charges = bottle_of_water->get_remaining_capacity_for_liquid( *water_in_bottle );
    bottle_of_water->put_in( std::move( water_in_bottle ) );
    test_inv.add_item( *bottle_of_water, false );

    test_inv.add_item( *item::spawn_temporary( "water", calendar::start_of_cataclysm,
                       item::INFINITE_CHARGES ), false );

    CHECK( test_inv.charges_of( itype_id( "water" ), item::INFINITE_CHARGES ) > 1 );
}
