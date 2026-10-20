#include "avatar.h"
#include "calendar.h"
#include "catch/catch.hpp"
#include "player_activity.h"
#include "player_helpers.h"
#include "state_helpers.h"

#include <cstdint>
#include <limits>

TEST_CASE("time_duration_to_string", "[calendar]") {
    CHECK(to_string(10_seconds) == "10 seconds");
    CHECK(to_string(60_seconds) == "1 minute");
    CHECK(to_string(70_seconds) == "1 minute and 10 seconds");
    CHECK(to_string(60_minutes) == "1 hour");
    CHECK(to_string(70_minutes) == "1 hour and 10 minutes");
    CHECK(to_string(24_hours) == "1 day");
    CHECK(to_string(24_hours + 1_seconds) == "1 day and 1 second");
    CHECK(to_string(25_hours) == "1 day and 1 hour");
    CHECK(to_string(25_hours + 1_seconds) == "1 day and 1 hour");
    CHECK(to_string(7_days) == "1 week");
    CHECK(to_string(8_days) == "1 week and 1 day");
}

TEST_CASE("time_duration_to_string_eternal_season", "[calendar]") {
    calendar::set_eternal_season(true);
    CHECK(to_string(7_days) == "1 week");
    CHECK(to_string(8_days) == "1 week and 1 day");
    CHECK(to_string(91_days) == "13 weeks");
    CHECK(to_string(92_days) == "13 weeks and 1 day");
    CHECK(to_string(99_days) == "14 weeks and 1 day");
    CHECK(to_string(364_days) == "52 weeks");
    CHECK(to_string(365_days) == "52 weeks and 1 day");
    CHECK(to_string(465_days) == "66 weeks and 3 days");
    CHECK(to_string(3650_days) == "521 weeks and 3 days");
    calendar::set_eternal_season(false);
}

TEST_CASE("time_point_to_string", "[calendar]") {
    CHECK(to_string(calendar::turn_zero) == "Year 1, Spring, day 1 12:00:00AM");
}

TEST_CASE("to_moves_is_exact_in_range_and_saturates_instead_of_overflowing", "[calendar]") {
    CHECK(to_moves<int>(5_seconds) == 500);
    CHECK(to_moves<int>(200_days) == 200 * 24 * 60 * 60 * 100);
    CHECK(to_moves<int>(-5_seconds) == -500);
    CHECK(to_moves<int>(365_days) == std::numeric_limits<int>::max());
    CHECK(to_moves<int>(-365_days) == std::numeric_limits<int>::lowest());
    CHECK(to_moves<std::int64_t>(365_days) == std::int64_t{365} * 24 * 60 * 60 * 100);
}

TEST_CASE("to_moves_converts_to_floating_point_exactly", "[calendar]") {
    CHECK(to_moves<double>(1_seconds) == 100.0);
    CHECK(to_moves<double>(-5_seconds) == -500.0);
    CHECK(to_moves<double>(365_days) == 365.0 * 24 * 60 * 60 * 100);
    CHECK(to_moves<float>(5_seconds) == 500.0f);
}

TEST_CASE("a_365_day_wait_activity_stays_active", "[calendar][activity]") {
    clear_all_state();
    auto& you = get_avatar();
    you.assign_activity(
        std::make_unique<player_activity>(activity_id("ACT_WAIT"), to_moves<int>(365_days), 0),
        false);
    CHECK(you.activity->get_moves_left() > to_moves<int>(200_days));
    CHECK_FALSE(you.activity->complete());
    clear_all_state();
}
