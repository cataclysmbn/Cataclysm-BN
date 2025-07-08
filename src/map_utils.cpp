#include <ranges>
#include "map.h"
#include "map_utils.h"
#include "game.h"
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
