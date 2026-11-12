#include "item_hauling.h"

auto is_haulable(const item& item) -> bool { return !item.made_of(phase_id::LIQUID); }

auto has_haulable_items(const tripoint_bub_ms& pos) -> bool {
    map& here = get_map();
    const map_stack items = here.i_at(pos);

    for (const item* const& item : items) {
        if (is_haulable(*item)) { return true; }
    }

    return false;
}
