#include "reload_selection.h"

#include "item.h"
#include "options.h"
#include "player.h"

#include <algorithm>
#include <utility>

namespace reload_selection {
auto order_ammo(std::vector<item_reload_option>& options) -> void {
    std::ranges::
        stable_sort(options, [](const item_reload_option& lhs, const item_reload_option& rhs) {
            return lhs.ammo->ammo_remaining() > rhs.ammo->ammo_remaining();
        });
    std::ranges::
        stable_sort(options, [](const item_reload_option& lhs, const item_reload_option& rhs) {
            return lhs.moves() < rhs.moves();
        });
    std::ranges::
        stable_sort(options, [](const item_reload_option& lhs, const item_reload_option& rhs) {
            return (lhs.ammo->ammo_remaining() != 0) > (rhs.ammo->ammo_remaining() != 0);
        });
}

auto prepare(const player& who, item& base, selection_options options) -> selection_result {
    auto discovery = reload::discover_ammo(who, base, options.discovery);
    if (base.is_container() && get_option<bool>("RELOAD_CONTAINER_AMMO_COMESTIBLES_ONLY")) {
        std::erase_if(discovery.options, [](const item_reload_option& option) {
            const auto* source = option.ammo;
            const auto* reloadable_source =
                source->is_ammo_container() && !option.store_container_as_item
                    ? &source->contents.front()
                    : source;
            return !reloadable_source->is_ammo() && !reloadable_source->is_comestible();
        });
    }
    if (discovery.options.empty() && !base.is_holster()) {
        if (!base.is_magazine() && !base.magazine_integral() && !base.magazine_current()) {
            return {.outcome = selection_outcome::missing_magazine};
        }
        if (discovery.ammo_match_found) {
            return {.outcome = selection_outcome::nothing_to_reload};
        }
        return {.outcome = selection_outcome::missing_ammunition};
    }

    order_ammo(discovery.options);
    if (!options.prompt && discovery.options.size() == 1) {
        return {.outcome = selection_outcome::automatic, .selected = discovery.options.front()};
    }

    return prepare(who, std::move(discovery.options));
}

auto prepare(const player& who, std::vector<item_reload_option> options) -> selection_result {
    if (options.empty()) { return {.outcome = selection_outcome::empty_supplied}; }
    if (who.is_npc()) {
        return {.outcome = selection_outcome::automatic, .selected = options.front()};
    }
    return {.outcome = selection_outcome::interaction_required, .options = std::move(options)};
}
} // namespace reload_selection
