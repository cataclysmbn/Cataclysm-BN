#include "item_stack.h"

#include "debug.h"
#include "item.h"
#include "output.h"
#include "units.h"

#include <algorithm>
#include <utility>

auto item_stack::size() const -> size_t { return items->size(); }

auto item_stack::empty() const -> bool { return items->empty(); }

auto item_stack::clear() -> std::vector<detached_ptr<item>> { return items->clear(); }

auto item_stack::begin() -> item_stack::iterator { return items->begin(); }

auto item_stack::end() -> item_stack::iterator { return items->end(); }

auto item_stack::begin() const -> item_stack::const_iterator { return items->cbegin(); }

auto item_stack::end() const -> item_stack::const_iterator { return items->cend(); }

auto item_stack::rbegin() -> item_stack::reverse_iterator { return items->rbegin(); }

auto item_stack::rend() -> item_stack::reverse_iterator { return items->rend(); }

auto item_stack::rbegin() const -> item_stack::const_reverse_iterator { return items->crbegin(); }

auto item_stack::rend() const -> item_stack::const_reverse_iterator { return items->crend(); }

auto item_stack::only_item() -> item& {
    if (empty()) {
        debugmsg("Missing item at target location");
        return null_item_reference();
    } else if (size() > 1) {
        debugmsg("More than one item at target location: %s",
                 enumerate_as_string(begin(), end(), [](const item* const& it) {
                     return it->typeId();
                 }));
        return null_item_reference();
    }
    return **items->begin();
}

auto item_stack::stored_volume() const -> units::volume {
    units::volume ret = 0_ml;
    for (const item* const& it : *items) { ret += it->volume(); }
    return ret;
}

auto item_stack::amount_can_fit(const item& it) const -> int {
    // Without stacking charges, would we violate the count limit?
    const bool violates_count = size() >= static_cast<size_t>(count_limit());
    const item* here = it.count_by_charges() ? stacks_with(it) : nullptr;

    if (violates_count && !here) { return 0; }
    // Call max because a tile may have been overfilled to begin with (e.g. #14115)
    const int ret = std::max(0, it.charges_per_volume(free_volume()));
    return it.count_by_charges() ? std::min(ret, it.charges) : ret;
}

auto item_stack::stacks_with(const item& it) -> item* {
    for (item*& here : *items) {
        if (here->stacks_with(it)) { return here; }
    }
    return nullptr;
}

auto item_stack::stacks_with(const item& it) const -> const item* {
    for (const item* const& here : *items) {
        if (here->stacks_with(it)) { return here; }
    }
    return nullptr;
}

auto item_stack::free_volume() const -> units::volume { return max_volume() - stored_volume(); }

void item_stack::move_all_to(item_stack* destination) {
    for (detached_ptr<item>& it : items->clear()) { destination->insert(std::move(it)); }
}

void item_stack::remove_top_items_with(std::function<detached_ptr<item>(detached_ptr<item>&&)> cb) {
    items->remove_with(std::move(cb));
}
