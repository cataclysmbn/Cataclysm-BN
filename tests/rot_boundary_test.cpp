#include "avatar.h"
#include "calendar.h"
#include "cata_utility.h"
#include "catch/catch.hpp"
#include "debug.h"
#include "enums.h"
#include "game.h"
#include "item.h"
#include "locations.h"
#include "map/map.h"
#include "map/mapbuffer.h"
#include "map/mapbuffer_registry.h"
#include "map/submap.h"
#include "map_helpers.h"
#include "rot.h"
#include "state_helpers.h"
#include "vehicle/vehicle.h"
#include "vehicle/vehicle_part.h"
#include "weather/weather.h"

#include <string_view>
#include <vector>

namespace {

struct rot_state {
    restore_on_out_of_scope<time_point> time{calendar::turn};
    rot_state() {
        clear_all_state();
        calendar::turn = calendar::start_of_cataclysm + 91_days;
        // clear_map() leaves submap temperature modifiers from other tests intact.
        get_map().set_temperature(tripoint_bub_ms(65, 65, 0), 0);
        get_weather().temperature = 18_c;
        get_weather().clear_temp_cache();
    }
    ~rot_state() { clear_all_state(); }
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

TEST_CASE("Vehicle parts without cargo use ambient rot temperature", "[rot][boundary]") {
    const auto state = rot_state();
    auto& here = get_map();
    auto* veh = here.add_vehicle(vproto_id("none"), tripoint_bub_ms(65, 65, 0), 0_degrees, 0, 0);
    REQUIRE(veh != nullptr);
    const auto part = veh->install_part(tripoint_mnt_veh::zero(), vpart_id("frame_vertical"), true);
    REQUIRE(part >= 0);
    here.add_vehicle_to_cache(veh);
    CHECK(
        rot::temp::for_location(here, veh->part(part).get_base()) == temperature_flag::TEMP_NORMAL);
}

TEST_CASE("Location removal falls back when a reported loaded tile is missing", "[rot][boundary]") {
    const auto state = rot_state();
    auto* location = new reported_location(item_location_type::map);
    location->move_by(tripoint_rel_ms(1000000, 1000000, 0));
    auto storage = location_vector<item>(location);
    storage.push_back(item::spawn("test_rot_food"));
    auto* food = storage.front();
    food->set_relative_rot(0.0);
    calendar::turn += 20_minutes;
    food->prepare_for_location_removal();
    CHECK(food->get_rot() == 20_minutes);
}

TEST_CASE(
    "Corrupt item types are retained and diagnosed before location removal", "[rot][boundary]") {
    const auto state = rot_state();
    const auto on_map = GENERATE(false, true);
    auto food = item::spawn("test_rot_food");
    auto* const original = food.get();
    const auto* const type = food->type;
    const auto pos = tripoint_bub_ms(65, 65, 0);
    if (on_map) { get_map().add_item(pos, std::move(food)); }
    original->type = nullptr;
    const auto restore_type = on_out_of_scope([&]() { original->type = type; });
    const auto message = capture_debugmsg_during([&]() {
        if (on_map) {
            auto& buffer = MAPBUFFER_REGISTRY.get(get_map().get_bound_dimension());
            const auto sm_pos = project_to<coords::sm>(bub_to_abs(pos));
            auto* sm = buffer.lookup_submap_in_memory(sm_pos);
            REQUIRE(sm != nullptr);
            sm->last_touched = calendar::turn - 1_turns;
            buffer.actualize_submap(sm_pos);
        } else {
            food = item::actualize_rot(std::move(food), {.position = bub_to_abs(pos)});
        }
    });
    CHECK(message.find("null") != std::string::npos);
    if (on_map) {
        CHECK(&get_map().i_at(pos).only_item() == original);
    } else {
        CHECK(food.get() == original);
    }
}

TEST_CASE("Legacy rot entry points respect game initialization state", "[rot][boundary]") {
    const auto state = rot_state();
    const auto starting = GENERATE(false, true);
    const auto restore_starting = restore_on_out_of_scope(g->new_game);
    g->new_game = starting;
    const auto entry = GENERATE(0, 1, 2);
    auto food = item::spawn("test_rot_food");
    food->set_relative_rot(0.0);
    calendar::turn += 20_minutes;
    const auto pos = tripoint_bub_ms(65, 65, 0);
    if (entry == 0) {
        food->update_rot(pos, temperature_flag::TEMP_NORMAL, get_weather());
    } else if (entry == 1) {
        food =
            item::actualize_rot(std::move(food), pos, temperature_flag::TEMP_NORMAL, get_weather());
    } else {
        food = item::process_rot(
            std::move(food), false, pos, nullptr, temperature_flag::TEMP_NORMAL, get_weather());
    }
    REQUIRE(food);
    CHECK(food->get_rot() == 20_minutes);
}

TEST_CASE(
    "Direct rot processing retains nonperishables and preserving container contents",
    "[rot][boundary]") {
    const auto state = rot_state();
    SECTION("nonprocessing contents do not require a location") {
        auto container = item::spawn("test_rot_open");
        container->put_in(item::spawn("test_rot_remains"));
        container->prepare_for_location_removal();
        REQUIRE(container->contents.num_item_stacks() == 1);
        CHECK(container->contents.front().typeId() == itype_id("test_rot_remains"));
    }
    SECTION("nonperishable food") {
        auto food = item::spawn("test_rot_nonperishable");
        calendar::turn += 20_minutes;
        food = item::process_rot(std::move(food), tripoint_bub_ms(65, 65, 0));
        REQUIRE(food);
        CHECK_FALSE(food->goes_bad());
    }
    SECTION("preserving ancestry remains visible during attempted detachment") {
        auto container = item::spawn("test_rot_preserving");
        auto corpse = item::make_corpse(mtype_id("mon_test_rot_corpse"), calendar::turn);
        corpse->set_relative_rot(0.0);
        container->put_in(std::move(corpse));
        auto& stored = container->contents.front();
        calendar::turn += 20_minutes;
        stored.attempt_detach([&](detached_ptr<item>&& it) {
            REQUIRE(it->parent_item() == container.get());
            return item::process_rot(std::move(it), tripoint_bub_ms(65, 65, 0));
        });
        CHECK(stored.get_rot() == 0_turns);
    }
}

TEST_CASE("Invalid temperature flags diagnose and fall back to ambient rot", "[rot][boundary]") {
    const auto state = rot_state();
    const auto invalid = static_cast<temperature_flag>(999);
    auto food = item::spawn("test_rot_food");
    food->set_relative_rot(0.0);
    CHECK(food->minimum_freshness_duration(invalid)
          == food->minimum_freshness_duration(temperature_flag::TEMP_NORMAL));
    calendar::turn += 20_minutes;
    const auto diagnostic = capture_debugmsg_during([&]() {
        food->update_rot(
            {.position = bub_to_abs(tripoint_bub_ms(65, 65, 0)), .temperature = invalid});
    });
    CHECK(diagnostic.find("not valid") != std::string::npos);
    CHECK(food->get_rot() == 20_minutes);
}

TEST_CASE(
    "Long shelf lives saturate freshness estimates instead of overflowing", "[rot][boundary]") {
    const auto state = rot_state();
    auto food = item::spawn("test_rot_long_lived");
    food->set_relative_rot(0.0);
    CHECK(food->minimum_freshness_duration(temperature_flag::TEMP_FRIDGE)
          == calendar::INDEFINITELY_LONG_DURATION);
}

TEST_CASE(
    "Item removal preserves ownership when callbacks transfer or replace items",
    "[rot][ownership]") {
    const auto state = rot_state();
    const auto operation =
        GENERATE("drop", "hold", "move", "reattach", "restore", "replace", "replace_after_drop");
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
                if (op == "replace_after_drop") { it = detached_ptr<item>(); }
                return item::spawn("test_rot_remains");
            }
            return detached_ptr<item>();
        });
    });
    CAPTURE(operation, count);
    const auto op = std::string_view(operation);
    CHECK(visits == count);
    const auto remains = op == "reattach" || op == "restore" || op == "replace";
    CHECK(items.size() == static_cast<size_t>(count - (remains ? 0 : 1)));
    CHECK(diagnostic.empty() == !op.starts_with("replace"));
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

TEST_CASE("Removing items from a destroyed location is rejected", "[rot][ownership]") {
    auto items = location_vector<item>();
    items.on_destroy();
    auto called = false;
    const auto diagnostic = capture_debugmsg_during([&]() {
        items.remove_with([&](detached_ptr<item>&& it) {
            called = true;
            return std::move(it);
        });
    });
    CHECK_FALSE(called);
    CHECK(diagnostic.find("destroyed") != std::string::npos);
}
