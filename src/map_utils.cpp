#include <ranges>
#include <utility>

#include "calendar.h"
#include "data_vars.h"
#include "game.h"
#include "item.h"
#include "map.h"
#include "map_utils.h"
#include "type_id.h"
#include "veh_type.h"
#include "vehicle.h"
#include "vpart_position.h"

auto get_items_at( const tripoint &loc ) -> location_subrange
{
    map &here = get_map();
    const optional_vpart_position vp = here.veh_at( g->m.getlocal( loc ) );
    if( vp ) {
        vehicle &veh = vp->vehicle();
        const int index = veh.part_with_feature( vp->part_index(), VPFLAG_CARGO, false );
        if( index < 0 ) {
            return {};
        }
        auto items = veh.get_items( index );
        return std::ranges::subrange( items );
    } else {
        auto items = here.i_at( here.getlocal( loc ) );
        return std::ranges::subrange( items );
    }
}

auto take_down_deployed_furniture( const tripoint &furniture_pos, const tripoint &drop_pos ) -> void
{
    auto &here = get_map();
    const auto furn_item = here.furn( furniture_pos ).obj().deployed_item;
    auto dropped_item = item::spawn( furn_item, calendar::turn );
    dropped_item->item_vars().merge( *here.furn_vars( furniture_pos ) );
    here.add_item_or_charges( drop_pos, std::move( dropped_item ) );
    here.furn_set( furniture_pos, f_null );
}
