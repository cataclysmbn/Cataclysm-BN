#include "calendar.h"
#include "cata_utility.h"
#include "catch/catch.hpp"
#include "debug.h"
#include "enums.h"
#include "item.h"
#include "locations.h"
#include "map/map.h"
#include "map/submap.h"
#include "rot.h"
#include "rot_test_state.h"
#include "safe_reference.h"
#include "state_helpers.h"

#include <algorithm>
#include <ranges>
#include <string_view>
#include <vector>

namespace {

/// Force the next insertion to reallocate, regardless of prior tests' tile capacity.
auto fill_spare_item_capacity(const tripoint_bub_ms& pos) -> size_t {
    auto& here = get_map();
    auto local = point_sm_ms();
    auto* sm = here.get_submap_at(pos, local);
    REQUIRE(sm != nullptr);
    auto& storage = sm->get_items(local);
    auto existing = storage.clear();
    storage = location_vector<item>();
    for (auto& it : existing) { storage.push_back(std::move(it)); }
    const auto& items = storage.as_vector();
    const auto padding = items.capacity() - items.size();
    for (const auto ignored : std::views::iota(size_t{0}, padding)) {
        static_cast<void>(ignored);
        here.add_item(pos, item::spawn("test_rot_remains"));
    }
    REQUIRE(items.size() == items.capacity());
    return padding;
}

struct rot_state: rot_test_state {
    // clear_map() leaves submap temperature modifiers from other tests intact.
    rot_state() { get_map().set_temperature(tripoint_bub_ms(65, 65, 0), 0); }
};

/// Model stale or inconsistent location metadata without invalid item pointers.
/// The item itself and its tile location use the real ownership API.
class reported_location: public tile_item_location {
    item_location_type kind;

public:
    explicit reported_location(item_location_type type)
        : tile_item_location(
              bub_to_abs(tripoint_bub_ms(65, 65, 0)), get_map().get_bound_dimension()),
          kind(type) {}
    auto where() const -> item_location_type override { return kind; }
    auto is_loaded(const item* /*it*/) const -> bool override { return true; }
    auto bub_pos(const item* /*it*/) const -> tripoint_bub_ms override {
        return tripoint_bub_ms(65, 65, 0);
    }
};

} // namespace

TEST_CASE(
    "Item removal callbacks may append items while keeping the current item",
    "[item][rot][regression]") {
    const auto restore_time = restore_on_out_of_scope(calendar::turn);
    const auto cleanup = on_out_of_scope(clear_all_state);
    clear_all_state();
    calendar::turn = calendar::start_of_cataclysm + 91_days;
    auto& here = get_map();
    const auto pos = tripoint_bub_ms(60, 60, 0);
    auto first = item::spawn("test_rot_remains");
    const auto* identity = first.get();
    here.add_item(pos, std::move(first));
    auto local = point_sm_ms();
    auto* sm = here.get_submap_at(pos, local);
    REQUIRE(sm != nullptr);
    auto& items = sm->get_items(local);
    const auto padding = fill_spare_item_capacity(pos);
    auto visits = 0;
    items.remove_with([&](detached_ptr<item>&& it) {
        if (it.get() == identity) {
            ++visits;
            for (const auto ignored : std::views::iota(0, 64)) {
                static_cast<void>(ignored);
                here.add_item(pos, item::spawn("test_rot_remains"));
            }
        }
        return std::move(it);
    });
    CHECK(visits == 1);
    CHECK(items.size() == 65 + padding);
    CHECK(std::ranges::count(items, identity) == 1);
    CHECK(std::ranges::all_of(items, [&](const auto* it) {
        return it->has_position() && it->abs_pos() == bub_to_abs(pos);
    }));
}

TEST_CASE("Rot location fallbacks tolerate inconsistent metadata", "[rot][boundary]") {
    const auto state = rot_state();
    const auto kind = GENERATE(
        item_location_type::vehicle, item_location_type::container, item_location_type::invalid);
    auto storage = location_vector<item>(new reported_location(kind));
    storage.push_back(item::spawn("test_rot_food"));
    const auto* food = storage.front();
    auto temperature = temperature_flag::TEMP_FREEZER;
    const auto diagnostic = capture_debugmsg_during([&]() {
        temperature = rot::temp::for_location(get_map(), *food);
    });
    CHECK(temperature == temperature_flag::TEMP_NORMAL);
    if (kind == item_location_type::container) {
        CHECK(diagnostic.empty());
    } else {
        CHECK_FALSE(diagnostic.empty());
    }
}

TEST_CASE(
    "Item removal preserves ownership when callbacks transfer or replace items",
    "[rot][ownership]") {
    const auto state = rot_state();
    const auto operation =
        GENERATE("drop", "hold", "move", "reattach", "restore", "replace", "replace_after_drop",
                 "replace_after_hold", "replace_after_move");
    const auto count = GENERATE(1, 2);
    const auto pos = tripoint_bub_ms(65, 65, 0);
    const auto destination = pos + tripoint_east;
    auto& here = get_map();
    auto first = item::spawn("test_rot_remains");
    const auto* const identity = first.get();
    here.add_item(pos, std::move(first));
    if (count == 2) { here.add_item(pos, item::spawn("test_rot_remains")); }
    auto local = point_sm_ms();
    auto& items = here.get_submap_at(pos, local)->get_items(local);
    auto held = detached_ptr<item>();
    auto visits = 0;
    const auto diagnostic = capture_debugmsg_during([&]() {
        items.remove_with([&](detached_ptr<item>&& it) {
            ++visits;
            if (it.get() != identity) { return std::move(it); }
            const auto op = std::string_view(operation);
            if (op == "drop") {
                it = detached_ptr<item>();
            } else if (op == "hold") {
                held = std::move(it);
            } else if (op == "move") {
                here.add_item(destination, std::move(it));
            } else if (op == "reattach") {
                here.add_item(pos, std::move(it));
            } else if (op == "restore") {
                auto* subject = it.get();
                here.add_item(destination, std::move(it));
                return subject->detach();
            } else {
                if (op == "replace_after_drop") {
                    it = detached_ptr<item>();
                } else if (op == "replace_after_hold") {
                    held = std::move(it);
                } else if (op == "replace_after_move") {
                    here.add_item(destination, std::move(it));
                }
                return item::spawn("test_rot_remains");
            }
            return detached_ptr<item>();
        });
    });
    CAPTURE(operation, count);
    const auto op = std::string_view(operation);
    CHECK(visits == count);
    const auto remains = op == "reattach" || op == "restore" || op.starts_with("replace");
    CHECK(items.size() == static_cast<size_t>(count - (remains ? 0 : 1)));
    // A callback can replace an item only after it takes that item.
    CHECK(diagnostic.empty() == (op != "replace"));
    if (op.starts_with("replace_after")) { CHECK(items.front() != identity); }
    if (held) { CHECK_FALSE(held->has_position()); }
    if (op == "move") { CHECK(&here.i_at(destination).only_item() == identity); }
    for (const auto* it : items) { CHECK(it->abs_pos() == bub_to_abs(pos)); }
}

TEST_CASE("Item traversal stops if its callback destroys the location", "[rot][ownership]") {
    const auto state = rot_state();
    auto items = location_vector<item>(new reported_location(item_location_type::map));
    items.push_back(item::spawn("test_rot_remains"));
    items.push_back(item::spawn("test_rot_remains"));
    auto visits = 0;
    items.remove_with([&](detached_ptr<item>&& it) {
        ++visits;
        items.on_destroy();
        return std::move(it);
    });
    CHECK(visits == 1);
}

TEST_CASE("Food that rots away does not leak memory", "[rot][ownership]") {
    const auto restore_time = restore_on_out_of_scope(calendar::turn);
    const auto cleanup = on_out_of_scope(clear_all_state);
    clear_all_state();
    calendar::turn = calendar::start_of_cataclysm + 91_days;
    const auto pos = tripoint_bub_ms(65, 65, 0);
    auto food = item::spawn("test_rot_food", calendar::turn, 1);
    auto food_ref = cache_reference<item>(food.get());
    get_map().add_item(pos, std::move(food));
    REQUIRE(food_ref);
    food_ref->set_relative_rot(3.0);
    auto callback_was_called = false;
    food_ref->attempt_detach([&](auto&& value) {
        callback_was_called = true;
        return std::move(value);
    });
    CHECK_FALSE(callback_was_called);
    CHECK(get_map().i_at(pos).empty());
    INFO("Food removed from the map must also be destroyed");
    CHECK_FALSE(food_ref);
}
