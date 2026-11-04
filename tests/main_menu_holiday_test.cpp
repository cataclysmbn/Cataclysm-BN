#include "catch/catch.hpp"
#include "main_menu.h"

#include <ctime>

TEST_CASE("the title screen leaves out the holiday of the date when told to", "[main_menu]") {
    // Noon of 2026-10-25, local time.
    auto local = std::tm{};
    local.tm_year = 126;
    local.tm_mon = 9;
    local.tm_mday = 25;
    local.tm_hour = 12;
    local.tm_isdst = -1;
    const auto halloween = std::mktime(&local);
    CHECK(main_menu::holiday_at(halloween) == holiday::halloween);
    main_menu::ignore_holidays() = true;
    CHECK(main_menu::holiday_at(halloween) == holiday::none);
    main_menu::ignore_holidays() = false;
}
