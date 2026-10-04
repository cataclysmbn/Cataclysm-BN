#include "map/utils/map_utils.h"

#include "calendar.h"
#include "data_vars.h"
#include "game.h"
#include "item.h"
#include "map/map.h"
#include "map/submap.h"
#include "type_id.h"
#include "vehicle/veh_type.h"
#include "vehicle/vehicle.h"
#include "vehicle/vpart_position.h"

#include <algorithm>
#include <ranges>
#include <utility>

namespace map_funcs {

namespace {

auto stow_if_it_fits(item& container, detached_ptr<item>&& payload) -> detached_ptr<item> {
    if (!container.is_container() || !container.can_contain(*payload)) {
        return std::move(payload);
    }

    const auto free_volume = std::
        max(container.get_container_capacity() - container.contents.item_size_modifier(), 0_ml);
    if (payload->count_by_charges()) {
        const auto fit_charges =
            std::min(payload->charges, payload->charges_per_volume(free_volume));
        if (fit_charges <= 0) { return std::move(payload); }

        container.put_in(payload->split(fit_charges));
        container.on_contents_changed();
        return payload && payload->charges > 0 ? std::move(payload) : detached_ptr<item>();
    }

    if (payload->volume() > free_volume) { return std::move(payload); }

    container.put_in(std::move(payload));
    container.on_contents_changed();
    return detached_ptr<item>();
}

} // namespace

auto get_items_at(const tripoint_abs_ms& loc) -> location_subrange {
    map& here = get_map();
    const optional_vpart_position vp = here.veh_at(loc);
    if (vp) {
        vehicle& veh = vp->vehicle();
        const int index = veh.part_with_feature(vp->part_index(), VPFLAG_CARGO, false);
        if (index < 0) { return {}; }
        auto items = veh.get_items(index);
        return std::ranges::subrange(items);
    } else {
        auto items = here.i_at(abs_to_bub(loc));
        return std::ranges::subrange(items);
    }
}

auto take_down_deployed_furniture(
    mapbuffer& buffer, const tripoint_abs_ms& furniture_pos, const tripoint_abs_ms& drop_pos)
    -> void {
    const auto tile = buffer.get_abs_tile(furniture_pos);
    if (!tile) { return; }
    const auto furn_item = tile->get_furn_t().deployed_item;
    auto dropped_item = item::spawn(furn_item, calendar::turn);
    dropped_item->item_vars().merge(tile->get_furn_vars());
    if (auto* const sm = buffer.get_submap(
            tile->abs_submap_pos(),
            {
                .mode = mapbuffer_lookup_mode::resident_only,
            })) {
        sm->get_items(tile->submap_pos())
            .remove_with([&dropped_item](detached_ptr<item>&& payload) {
                return stow_if_it_fits(*dropped_item, std::move(payload));
            });
    }
    buffer.add_item_or_charges(drop_pos, std::move(dropped_item));
    buffer.set_furn(furniture_pos, f_null);
}

auto take_down_deployed_furniture(
    const tripoint_bub_ms& furniture_pos, const tripoint_bub_ms& drop_pos) -> void {
    auto& here = get_map();
    const auto furn_item = here.furn(furniture_pos).obj().deployed_item;
    auto dropped_item = item::spawn(furn_item, calendar::turn);
    dropped_item->item_vars().merge(*here.furn_vars(furniture_pos));
    auto items = here.i_at(furniture_pos);
    items.remove_top_items_with([&dropped_item](detached_ptr<item>&& payload) {
        return stow_if_it_fits(*dropped_item, std::move(payload));
    });
    here.add_item_or_charges(drop_pos, std::move(dropped_item));
    here.furn_set(furniture_pos, f_null);
}

} // namespace map_funcs
