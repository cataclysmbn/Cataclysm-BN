#include "map/utils/map_utils.h"

#include "calendar.h"
#include "data_vars.h"
#include "game.h"
#include "item.h"
#include "map.h"
#include "mapbuffer.h"
#include "type_id.h"
#include "veh_type.h"
#include "vehicle.h"
#include "vpart_position.h"

#include <ranges>
#include <utility>

namespace map_funcs {

auto get_items_at(mapbuffer& buffer, const tripoint_abs_ms& loc) -> location_subrange {
    const optional_vpart_position vp = buffer.veh_at(loc);
    if (vp) {
        vehicle& veh = vp->vehicle();
        const int index = veh.part_with_feature(vp->part_index(), VPFLAG_CARGO, false);
        if (index < 0) { return {}; }
        auto items = veh.get_items(index);
        return std::ranges::subrange(items);
    } else if (auto items = buffer.get_items(loc)) {
        return std::ranges::subrange(*items);
    } else {
        return {};
    }
}

auto take_down_deployed_furniture(
    mapbuffer& buffer, const tripoint_abs_ms& furniture_pos, const tripoint_abs_ms& drop_pos)
    -> void {
    const auto tile = abs_tile_handle::fetch(buffer, furniture_pos);
    if (!tile) { return; }
    const auto furn_item = tile->furn_obj().deployed_item;
    const auto furniture_vars = tile->furn_vars();
    auto dropped_item = item::spawn(furn_item, calendar::turn);
    dropped_item->item_vars().merge(furniture_vars);
    buffer.add_item_or_charges(drop_pos, std::move(dropped_item));
    buffer.set_furn(furniture_pos, f_null);
}

auto take_down_deployed_furniture(
    const tripoint_bub_ms& furniture_pos, const tripoint_bub_ms& drop_pos) -> void {
    take_down_deployed_furniture(
        get_map().get_mapbuffer(), bub_to_abs(furniture_pos), bub_to_abs(drop_pos));
}

} // namespace map_funcs
