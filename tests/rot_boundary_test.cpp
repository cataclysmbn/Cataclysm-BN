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
#include "rot_test_state.h"
#include "vehicle/vehicle.h"
#include "vehicle/vehicle_part.h"
#include "weather/weather.h"

#include <string_view>
#include <vector>

namespace {

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

TEST_CASE("Legacy rot entry points accumulate elapsed rot", "[rot][boundary]") {
    const auto state = rot_state();
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
