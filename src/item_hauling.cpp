#include "item_hauling.h"

bool is_haulable( const item &item )
{
    return !item.made_of( phase_id::LIQUID );
}

bool has_haulable_items( const tripoint_bub_ms &pos )
{
    map &here = get_map();
    const map_stack items = here.i_at( pos );

    for( const item * const &item : items ) {
        if( is_haulable( *item ) ) {
            return true;
        }
    }

    return false;
}
