#include "stored_ammo.h"

#include <algorithm>
#include <utility>
#include <vector>

#include "calendar.h"
#include "game_constants.h"
#include "item.h"
#include "itype.h"

namespace
{

const auto itype_plut_cell = itype_id( "plut_cell" );

auto stored_ammo_remaining( const item &source ) -> int
{
    return source.is_magazine() ? source.ammo_remaining() : source.charges;
}

auto set_stored_ammo_remaining( item &source, const itype_id &ammo, int charges ) -> void
{
    const auto remaining_charges = std::max( charges, 0 );
    if( source.is_magazine() ) {
        if( remaining_charges > 0 ) {
            source.ammo_set( ammo, remaining_charges );
        } else {
            source.ammo_unset();
        }
        return;
    }

    source.charges = remaining_charges;
    if( remaining_charges == 0 ) {
        source.ammo_unset();
    }
}

} // namespace

auto stored_ammo_item_charges( const itype_id &ammo, int stored_charges ) -> int
{
    if( stored_charges <= 0 || ammo.is_null() ) {
        return 0;
    }
    return ammo == itype_plut_cell ? stored_charges / PLUTONIUM_CHARGES : stored_charges;
}

auto stored_ammo_charges_for_items( const itype_id &ammo, int item_charges ) -> int
{
    if( item_charges <= 0 || ammo.is_null() ) {
        return 0;
    }
    return ammo == itype_plut_cell ? item_charges * PLUTONIUM_CHARGES : item_charges;
}

auto spawn_stored_ammo( const itype_id &ammo, int stored_charges ) -> detached_ptr<item>
{
    const auto item_charges = stored_ammo_item_charges( ammo, stored_charges );
    if( item_charges <= 0 ) {
        return detached_ptr<item>();
    }

    auto ammodrop = item::spawn( ammo, calendar::turn );
    ammodrop->charges = item_charges;
    return ammodrop;
}

auto recover_stored_ammo( item &source,
                          stored_ammo_remainder_handling remainder_handling ) -> recovered_stored_ammo
{
    auto recovered = recovered_stored_ammo();
    const auto ammo = source.ammo_current();
    const auto starting_charges = stored_ammo_remaining( source );
    if( starting_charges <= 0 || ammo.is_null() ) {
        return recovered;
    }

    const auto recovered_item_charges = stored_ammo_item_charges( ammo, starting_charges );
    const auto consumed_charges = stored_ammo_charges_for_items( ammo, recovered_item_charges );
    if( auto ammodrop = spawn_stored_ammo( ammo, starting_charges ) ) {
        recovered.push_back( std::move( ammodrop ) );
    }

    const auto remaining_charges = remainder_handling == stored_ammo_remainder_handling::preserve ?
                                   starting_charges - consumed_charges : 0;
    set_stored_ammo_remaining( source, ammo, remaining_charges );
    return recovered;
}
