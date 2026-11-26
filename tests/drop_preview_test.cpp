#include "avatar.h"
#include "catch/catch.hpp"
#include "drop_preview.h"
#include "inventory_ui.h"
#include "item.h"
#include "player_helpers.h"
#include "state_helpers.h"

#include <algorithm>

namespace {

class drop_preview_selector: public inventory_drop_selector {
public:
    using inventory_drop_selector::get_drop_preview;
    using inventory_drop_selector::inventory_drop_selector;
    using inventory_drop_selector::process_selected;
    using inventory_drop_selector::set_chosen_count;
    using inventory_drop_selector::update_drop_preview;
    using inventory_multiselector::get_selection_column_items;
};

} // namespace

TEST_CASE(
    "category drop preview groups overflow under its container", "[inventory][ui][drop_token]") {
    clear_all_state();
    auto& owner = get_avatar();
    clear_character(owner, false);
    REQUIRE_FALSE(owner.wear_item(item::spawn("test_backpack"), false));
    auto* bag = owner.worn.front();
    REQUIRE_FALSE(owner.wear_item(item::spawn("test_socks"), false));
    auto* cargo = &owner.i_add(item::spawn("test_amputator"));
    REQUIRE(owner.volume_carried() > owner.volume_capacity_reduced_by(bag->get_storage()));

    auto selector = drop_preview_selector(owner);
    selector.add_character_items(owner);
    const auto gear = selector.own_gear_column.get_all_entries([](const auto& entry) {
        return entry.is_item();
    });
    auto count = 0;
    selector.process_selected(count, gear);
    selector.update_drop_preview();
    const auto selected = selector.get_selection_column_items();
    REQUIRE(selected.size() == 3);
    CHECK(selected[0]->any_item() == bag);
    CHECK(selected[1]->any_item() == cargo);
    CHECK(selected[1]->automatic_drop_count == 1);
    CHECK(owner.is_worn(*selected[2]->any_item()));

    selector.process_selected(count, gear);
    selector.update_drop_preview();
    CHECK(selector.get_selection_column_items().empty());
}

TEST_CASE(
    "category drop preview groups linked items with each container",
    "[inventory][ui][drop_token]") {
    clear_all_state();
    auto& owner = get_avatar();
    clear_character(owner, false);
    REQUIRE_FALSE(owner.wear_item(item::spawn("test_backpack"), false));
    REQUIRE_FALSE(owner.wear_item(item::spawn("test_briefcase"), false));
    auto id = 0;
    for (auto* clothing : owner.worn) {
        clothing->set_var("DROP_WITH_CLOTHING_ID", ++id);
        auto object = item::spawn("test_amputator");
        object->set_var("DROP_WITH_CLOTHING_TARGET", id);
        owner.i_add(std::move(object));
    }

    auto selector = drop_preview_selector(owner);
    selector.add_character_items(owner);
    const auto gear = selector.own_gear_column.get_all_entries([](const auto& entry) {
        return entry.is_item();
    });
    auto count = 0;
    selector.process_selected(count, gear);
    selector.update_drop_preview();
    const auto selected = selector.get_selection_column_items();
    REQUIRE(selected.size() == 4);
    for (auto index = size_t{0}; index < selected.size(); index += 2) {
        const auto* clothing = selected[index]->any_item();
        const auto* linked = selected[index + 1]->any_item();
        CHECK(owner.is_worn(*clothing));
        CHECK(linked->get_var("DROP_WITH_CLOTHING_TARGET", 0)
              == clothing->get_var("DROP_WITH_CLOTHING_ID", -1));
    }
}

TEST_CASE("drop preview preserves automatic choices on confirmation", "[inventory][drop_token]") {
    namespace ranges = std::ranges;
    clear_all_state();
    auto& owner = get_avatar();
    clear_character(owner, false);
    REQUIRE_FALSE(owner.wear_item(item::spawn("test_backpack"), false));
    REQUIRE_FALSE(owner.wear_item(item::spawn("test_briefcase"), false));
    auto* bag = owner.worn.front();

    for (auto index = 0; index < 20; ++index) {
        auto object = item::spawn("test_amputator");
        object->set_favorite(index >= 10);
        owner.i_add(std::move(object));
    }
    owner.inv_restack();
    REQUIRE(owner.volume_carried() > owner.volume_capacity_reduced_by(bag->get_storage()));
    auto selected = drop_locations{};
    selected.emplace_back(*bag, 1);

    const auto preview = make_drop_preview(owner, selected);
    REQUIRE(preview.items.size() > 1);
    CHECK(ranges::none_of(preview.items, [](const auto& entry) { return entry.loc->is_favorite; }));
    CHECK(owner.volume_carried_reduced_by(preview.counts)
          <= owner.volume_capacity_reduced_by(0_ml, preview.counts));

    const auto confirmed = make_drop_preview(owner, preview.items);
    REQUIRE(confirmed.items.size() == preview.items.size());
    for (const auto& entry : preview.items) {
        CHECK(ranges::any_of(confirmed.items, [&](const auto& actual) {
            return &*actual.loc == &*entry.loc && actual.count == entry.count;
        }));
    }
    CHECK(make_drop_preview(owner, {}).items.empty());
}

TEST_CASE("drop preview keeps partial charge quantities", "[inventory][drop_token]") {
    clear_all_state();
    auto& owner = get_avatar();
    REQUIRE_FALSE(owner.wear_item(item::spawn("test_backpack"), false));
    auto rocks = item::spawn("test_rock");
    REQUIRE(rocks->count_by_charges());
    rocks->charges = 10;
    auto* target = &owner.i_add(std::move(rocks));
    auto selected = drop_locations{};
    selected.emplace_back(*target, 3);

    const auto preview = make_drop_preview(owner, selected);
    REQUIRE(preview.items.size() == 1);
    CHECK(preview.items.front().count == 3);
    CHECK(target->charges == 10);
    CHECK(preview.counts.at(target) == 3);
    const auto confirmed = make_drop_preview(owner, preview.items);
    REQUIRE(confirmed.items.size() == 1);
    CHECK(confirmed.items.front().count == 3);
}

TEST_CASE(
    "multidrop previews linked favorites and clears them when bag is unselected",
    "[inventory][ui][drop_token]") {
    namespace ranges = std::ranges;
    clear_all_state();
    auto& owner = get_avatar();
    REQUIRE_FALSE(owner.wear_item(item::spawn("test_backpack"), false));
    auto* bag = owner.worn.front();
    bag->set_var("DROP_WITH_CLOTHING_ID", 1);
    auto object = item::spawn("test_amputator");
    object->set_favorite(true);
    object->set_var("DROP_WITH_CLOTHING_TARGET", 1);
    auto* linked = &owner.i_add(std::move(object));

    auto selector = drop_preview_selector(owner);
    selector.add_character_items(owner);
    const auto gear = selector.own_gear_column.get_all_entries([](const auto& entry) {
        return entry.is_item();
    });
    const auto bag_entry = ranges::find_if(gear, [bag](const auto* entry) {
        return entry->any_item() == bag;
    });
    REQUIRE(bag_entry != gear.end());
    const auto inventory = selector.own_inv_column.get_all_entries([](const auto& entry) {
        return entry.is_item();
    });
    const auto linked_entry = ranges::find_if(inventory, [linked](const auto* entry) {
        return entry->any_item() == linked;
    });
    REQUIRE(linked_entry != inventory.end());

    selector.set_chosen_count(**bag_entry, 1);
    selector.update_drop_preview();
    CHECK((*linked_entry)->chosen_count == 0);
    CHECK((*linked_entry)->automatic_drop_count == 1);
    CHECK(selector.get_drop_preview().size() == 2);
    CHECK(selector.get_selection_column_items().size() == 2);

    selector.update_drop_preview();
    CHECK(selector.get_drop_preview().size() == 2);
    CHECK((*linked_entry)->chosen_count == 0);

    selector.set_chosen_count(**bag_entry, 0);
    selector.update_drop_preview();
    CHECK((*linked_entry)->automatic_drop_count == 0);
    CHECK(selector.get_drop_preview().empty());
    CHECK(selector.get_selection_column_items().empty());
}
