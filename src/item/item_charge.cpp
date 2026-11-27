#include "item/item_charge.h"

#include "item.h"

#include <algorithm>

auto charge_target(item& it) -> item& {
    auto* mag = it.magazine_current();
    return mag ? *mag : it;
}

auto add_charges(item& it, int qty, const itype_id& fallback) -> int {
    auto& target = charge_target(it);
    const auto before = target.ammo_remaining();
    const auto added = std::min(qty, target.ammo_capacity() - before);
    if (added <= 0) { return 0; }
    const auto current = target.ammo_current();
    target.ammo_set(
        current.is_null() && !target.ammo_types().empty() ? fallback : current, before + added);
    return target.ammo_remaining() - before;
}
