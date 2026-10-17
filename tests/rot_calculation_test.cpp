#include "calendar.h"
#include "cata_utility.h"
#include "catch/catch.hpp"
#include "debug.h"
#include "flag.h"
#include "item.h"
#include "rot/rot_calculation.h"
#include "state_helpers.h"
#include "units_temperature.h"

#include <cmath>
#include <limits>
#include <type_traits>

static_assert(!std::is_default_constructible_v<rot::shelf_life>);
static_assert(!std::is_constructible_v<rot::shelf_life, time_duration>);

TEST_CASE("Food that can spoil must have a shelf life greater than zero", "[rot][calculation]") {
    CHECK_FALSE(rot::shelf_life::from_duration(0_turns));
    CHECK_FALSE(rot::shelf_life::from_duration(-1_turns));
    const auto maximum = time_duration::from_turns(std::numeric_limits<int>::max());
    const auto minimum = time_duration::from_turns(std::numeric_limits<int>::min());
    for (const auto duration : {1_turns, 1_days, maximum}) {
        const auto lifetime = rot::shelf_life::from_duration(duration);
        REQUIRE(lifetime);
        CHECK(lifetime->duration() == duration);
        CHECK(lifetime->relative_rot(0_turns) == 0.0);
        CHECK(lifetime->relative_rot(duration) == 1.0);
        CHECK(lifetime->relative_rot(-duration) == -1.0);
        for (const auto accumulated : {minimum, -1_turns, 0_turns, 1_turns, maximum}) {
            CHECK(std::isfinite(lifetime->relative_rot(accumulated)));
        }
    }
}

TEST_CASE(
    "Spoilage edits reject invalid numbers and values too large to store", "[rot][calculation]") {
    const auto lifetime = rot::shelf_life::from_duration(1_hours);
    REQUIRE(lifetime);
    CHECK(lifetime->from_relative(0.25) == 15_minutes);
    CHECK(lifetime->from_relative(-0.25) == -15_minutes);
    CHECK(lifetime->from_relative(1.5) == 90_minutes);
    for (const auto fraction :
         {std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::infinity(),
          -std::numeric_limits<double>::infinity(), 1.0e8, -1.0e8}) {
        CHECK_FALSE(lifetime->from_relative(fraction));
    }
    const auto one_turn = rot::shelf_life::from_duration(1_turns);
    REQUIRE(one_turn);
    CHECK(one_turn->from_relative(std::numeric_limits<int>::max())
          == time_duration::from_turns(std::numeric_limits<int>::max()));
    CHECK(one_turn->from_relative(std::numeric_limits<int>::min())
          == time_duration::from_turns(std::numeric_limits<int>::min()));
}

TEST_CASE(
    "Freshness estimates handle expired food and very long shelf lives", "[rot][calculation]") {
    const auto lifetime = rot::shelf_life::from_duration(1_days);
    REQUIRE(lifetime);
    CHECK(lifetime->minimum_freshness(0_turns, 18_c) == 1_days);
    CHECK(lifetime->minimum_freshness(12_hours, 18_c) == 12_hours);
    CHECK(lifetime->minimum_freshness(1_days, 18_c) == 0_turns);
    CHECK(lifetime->minimum_freshness(2_days, 18_c) == 0_turns);
    CHECK(lifetime->minimum_freshness(-1_days, 18_c) == 2_days);
    CHECK(lifetime->minimum_freshness(0_turns, -20_c) == calendar::INDEFINITELY_LONG_DURATION);
    const auto long_lived = rot::shelf_life::from_duration(10000_days);
    REQUIRE(long_lived);
    CHECK(long_lived->minimum_freshness(0_turns, 3_c) == calendar::INDEFINITELY_LONG_DURATION);
}

TEST_CASE(
    "Elapsed time, temperature, and field dressing determine how much food spoils",
    "[rot][calculation]") {
    CHECK(rot::increment({.elapsed = 1_hours, .temperature = 18_c}) == 1_hours);
    CHECK(rot::increment({.elapsed = 1_hours, .temperature = -20_c}) == 0_turns);
    CHECK(rot::increment({.elapsed = 1_hours, .temperature = 50_c}) == 21240_turns);
    CHECK(rot::increment({.elapsed = 1_hours, .temperature = 18_c, .field_dressed_corpse = true})
          == 45_minutes);
    CHECK(rot::increment({.elapsed = 1_turns, .temperature = 18_c, .field_dressed_corpse = true})
          == 0_turns);
    CHECK(rot::increment({.elapsed = 2_turns, .temperature = 18_c, .field_dressed_corpse = true})
          == 1_turns);
    CHECK(rot::increment({.elapsed = 1_turns, .temperature = 1_c, .initial_variation = -1_turns})
          == -1_turns);
    CHECK(rot::increment({.elapsed = -20_minutes, .temperature = 18_c}) == -20_minutes);
    const auto maximum = time_duration::from_turns(std::numeric_limits<int>::max());
    const auto minimum = time_duration::from_turns(std::numeric_limits<int>::min());
    CHECK(rot::increment({.elapsed = maximum, .temperature = 18_c}) == maximum);
    CHECK(rot::increment({.elapsed = maximum, .temperature = 50_c}) == maximum);
    CHECK(rot::increment({.elapsed = minimum, .temperature = 50_c}) == minimum);
    CHECK(rot::accumulate(20_minutes, -5_minutes) == 15_minutes);
    CHECK(rot::accumulate(maximum, 1_turns) == maximum);
    CHECK(rot::accumulate(minimum, -1_turns) == minimum);
}

TEST_CASE("Food only ages when it can spoil and is not being preserved", "[rot][calculation]") {
    const auto restore_time = restore_on_out_of_scope(calendar::turn);
    const auto cleanup = on_out_of_scope(clear_all_state);
    clear_all_state();
    calendar::turn = calendar::start_of_cataclysm + 91_days;
    auto food = item::spawn("test_rot_food", calendar::turn);
    food->set_relative_rot(0.25);
    SECTION("unprotected food ages normally") {
        CHECK(food->calc_rot(calendar::turn + 20_minutes, 18_c) == 20_minutes);
        CHECK(food->get_rot() == 6_hours);
    }
    SECTION("smoking or milling pauses spoilage") {
        food->set_flag(flag_id("PROCESSING"));
        CHECK(food->calc_rot(calendar::turn + 20_minutes, 18_c) == 0_turns);
        CHECK(food->get_shelf_life() == 1_days);
        CHECK(food->get_relative_rot() == Approx(0.25));
        food->set_relative_rot(0.5);
        CHECK(food->get_relative_rot() == Approx(0.25));
    }
    SECTION("food in a preserving container does not age") {
        auto owner = item::spawn("test_rot_preserving");
        owner->put_in(std::move(food));
        auto& stored = owner->contents.front();
        CHECK(stored.calc_rot(calendar::turn + 20_minutes, 18_c) == 0_turns);
        calendar::turn += 20_minutes;
        stored.update_rot({.position = tripoint_abs_ms::zero()});
        CHECK(stored.get_rot() == 6_hours);
    }
    SECTION("ingredients listed in finished food do not age") {
        auto owner = item::spawn("test_rot_food");
        owner->add_component(std::move(food));
        auto& stored = *owner->get_components().front();
        CHECK(stored.calc_rot(calendar::turn + 20_minutes, 18_c) == 0_turns);
    }
    SECTION("nonperishable food never ages or rots away") {
        auto nonperishable = item::spawn("test_rot_nonperishable");
        CHECK(nonperishable->calc_rot(calendar::turn + 20_minutes, 18_c) == 0_turns);
        CHECK(nonperishable->minimum_freshness_duration(temperature_flag::TEMP_NORMAL)
              == calendar::INDEFINITELY_LONG_DURATION);
        CHECK_FALSE(nonperishable->has_rotten_away());
        calendar::turn += 20_minutes;
        nonperishable->update_rot({.position = tripoint_abs_ms::zero()});
        CHECK(nonperishable->get_rot() == 0_turns);
    }
    SECTION("corpse decay is handled separately from food freshness") {
        auto corpse = item::make_corpse(mtype_id("mon_test_rot_corpse"), calendar::turn);
        REQUIRE(corpse->goes_bad());
        CHECK(corpse->get_shelf_life() == 1_days);
        CHECK(corpse->minimum_freshness_duration(temperature_flag::TEMP_NORMAL)
              == calendar::INDEFINITELY_LONG_DURATION);
    }
    SECTION("a steel corpse does not decay like food") {
        auto corpse = item::make_corpse(mtype_id("mon_test_rot_inorganic"), calendar::turn);
        REQUIRE_FALSE(corpse->goes_bad());
        calendar::turn += 21_days;
        corpse->update_rot({.position = tripoint_abs_ms::zero()});
        corpse = item::process_rot(std::move(corpse), tripoint_bub_ms::zero());
        REQUIRE(corpse);
        CHECK(corpse->get_rot() == 0_turns);
        CHECK_FALSE(corpse->has_rotten_away());
    }
    SECTION("an invalid spoilage value leaves the food unchanged") {
        const auto fraction =
            GENERATE(std::numeric_limits<double>::quiet_NaN(),
                     std::numeric_limits<double>::infinity(), 1.0e20);
        const auto message = capture_debugmsg_during([&]() { food->set_relative_rot(fraction); });
        CHECK(message.find("Invalid relative rot") != std::string::npos);
        CHECK(food->get_rot() == 6_hours);
    }
}
