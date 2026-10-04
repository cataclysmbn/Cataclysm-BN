#include "reload.h"

#include "character.h"
#include "character_functions.h"
#include "debug.h"
#include "enums.h"
#include "flag.h"
#include "game_constants.h"
#include "item.h"
#include "itype.h"
#include "player.h"

#include <algorithm>
#include <unordered_set>

namespace {
const auto ammo_plutonium = ammotype("plutonium");
} // namespace

item_reload_option::item_reload_option(const item_reload_option&) = default;

auto item_reload_option::operator=(const item_reload_option&) -> item_reload_option& = default;

item_reload_option::item_reload_option(
    const player* who, item* target, const item* parent, item& ammo, bool store_container_as_item)
    : who(who),
      target(target),
      ammo(&ammo),
      store_container_as_item(
          store_container_as_item
          || (target->is_container() && !target->is_watertight_container() && ammo.is_container())),
      parent(parent) {
    if (this->target->is_ammo_belt()) {
        const auto& linkage = this->target->type->magazine->linkage;
        if (linkage) { max_qty = this->who->charges_of(*linkage); }
    }
    qty(max_qty);
}

auto item_reload_option::moves() const -> int {
    auto mv = ammo->obtain_cost(*who, qty())
            + who->item_reload_cost(*target, *ammo, qty(), store_container_as_item);
    if (parent != target) {
        if (parent->is_gun()) {
            mv += parent->get_reload_time();
        } else if (parent->is_tool()) {
            mv += 100;
        }
    }
    return mv;
}

auto item_reload_option::qty(int val) -> void {
    const auto stores_container_as_item = store_container_as_item;
    const auto ammo_in_ammo_container = ammo->is_ammo_container() && !stores_container_as_item;
    auto& ammo_obj = ammo_in_ammo_container ? ammo->contents.front() : *ammo;

    if (ammo_in_ammo_container && !ammo_obj.is_ammo()) {
        debugmsg("Invalid reload option: %s", ammo_obj.tname());
        return;
    }

    // Checking ammo capacity implicitly limits guns with removable magazines to capacity 0.
    // This gets rounded up to 1 later.
    auto remaining_capacity = 0;
    if (target->is_container()) {
        if (ammo_obj.count_by_charges()) {
            remaining_capacity = target->get_remaining_capacity_for_liquid(ammo_obj, true);
        } else {
            const auto free_volume = std::
                max(target->get_container_capacity() - target->contents.item_size_modifier(), 0_ml);
            remaining_capacity = free_volume >= ammo_obj.volume_for_storage() ? 1 : 0;
        }
    } else {
        remaining_capacity = target->ammo_capacity() - target->ammo_remaining();
    }
    const auto magazine_like = ammo->is_magazine() || ammo->has_flag(flag_SPEEDLOADER);
    if (magazine_like && remaining_capacity <= 0) { remaining_capacity = 1; }
    if (target->has_flag(flag_RELOAD_ONE) && !ammo->has_flag(flag_SPEEDLOADER)) {
        remaining_capacity = 1;
    }
    if (ammo_obj.type->ammo && ammo_obj.ammo_type() == ammo_plutonium) { // Why a specific case just
                                                                         // for plutonium? Answer:
                                                                         // Because "it's ooold".
                                                                         // Should be addressed
                                                                         // later...
        remaining_capacity =
            remaining_capacity / PLUTONIUM_CHARGES + (remaining_capacity % PLUTONIUM_CHARGES != 0);
    }

    const auto ammo_by_charges = ammo_obj.count_by_charges() || ammo->is_comestible();
    const auto available_ammo =
        ammo->has_flag(flag_SPEEDLOADER) ? ammo->ammo_remaining()
        : magazine_like                  ? 1
        : ammo_by_charges
            ? ammo_obj.charges
            : ammo_obj.count();
    // Constrain by available ammo, target capacity and other external factors (max_qty).
    // @ref max_qty is currently set when reloading ammo belts and limits to available linkages.
    // A requested quantity of zero selects one round, but cannot create ammo or capacity.
    qty_ = std::max(0, std::min({std::max(val, 1), available_ammo, remaining_capacity, max_qty}));
}

namespace reload {
auto discover_ammo(const Character& who, item& base, discovery_options options)
    -> discovery_result {
    auto result = discovery_result{};
    auto targets = base.gunmods();
    targets.push_back(&base);

    if (base.magazine_current()) { targets.push_back(base.magazine_current()); }

    for (const auto mod : base.gunmods()) {
        if (mod->magazine_current()) { targets.push_back(mod->magazine_current()); }
    }

    const auto ammo_search_range = who.is_mounted() ? -1 : 1;
    for (item* target : targets) {
        auto seen_sources = std::unordered_set<item*>();
        for (item* ammo : character_funcs::find_ammo_items_or_mags(
                 who, *target, options.include_empty_mags, ammo_search_range)) {
            // Don't try to unload frozen liquids.
            if (ammo->is_watertight_container() && ammo->contents_normally_made_of(LIQUID)
                && ammo->contents_made_of(SOLID)) {
                continue;
            }
            auto source_items = std::vector<item*>{};
            if (ammo->is_container() && !ammo->is_ammo_container() && !ammo->contents.empty()
                && target->is_container() && !target->is_watertight_container()) {
                source_items.push_back(ammo);
                const auto& contents = ammo->contents.all_items_top();
                source_items.insert(source_items.end(), contents.begin(), contents.end());
            } else if (ammo->is_watertight_container() && target->is_watertight_container()
                       && ammo->contents_made_of(LIQUID)) {
                source_items.push_back(ammo);
                const auto& contents = ammo->contents.all_items_top();
                source_items.insert(source_items.end(), contents.begin(), contents.end());
            } else if (ammo->is_container() && !ammo->is_ammo_container() && !ammo->contents.empty()
                       && !(ammo->is_watertight_container() && target->is_watertight_container()
                            && ammo->contents_made_of(LIQUID))) {
                source_items = ammo->contents.all_items_top();
            } else {
                source_items.push_back(ammo);
            }

            for (item* source : source_items) {
                if (!seen_sources.insert(source).second) { continue; }
                const auto store_container_as_item =
                    source->is_container() && target->is_container()
                    && target->can_contain(*source);
                const auto* reloadable_source =
                    source->is_ammo_container() && !store_container_as_item
                        ? &source->contents.front()
                        : source;
                const auto id = reloadable_source->typeId();
                if (target->is_container() && !target->is_container_full()
                    && !target->is_reloadable_with(id)) {
                    continue;
                }
                const auto can_reload_with = target->can_reload_with(id);
                if (can_reload_with) {
                    // Skip if the magazine is inside a gun/mod that can't fire its ammunition.
                    if (target->is_magazine() && target->parent_item()) {
                        const auto ammo_type = reloadable_source->ammo_type();
                        const auto& supported_ammo = target->parent_item()->ammo_types();
                        const auto gun_supports =
                            std::ranges::any_of(supported_ammo, [&](const ammotype& candidate) {
                                return candidate == ammo_type;
                            });
                        if (!gun_supports) { continue; }
                    }
                    // Speedloaders require an empty target.
                    if (options.include_potential || !source->has_flag(flag_SPEEDLOADER)
                        || target->ammo_remaining() < 1) {
                        result.ammo_match_found = true;
                    }
                }
                if ((options.include_potential && can_reload_with)
                    || who.as_player()->can_reload(*target, id)
                    || (target->is_container() && can_reload_with)
                    || target->has_flag(flag_RELOAD_AND_SHOOT)) {
                    result.options.emplace_back(
                        who.as_player(), target, &base, *source, store_container_as_item);
                }
            }
        }
    }
    return result;
}
} // namespace reload
