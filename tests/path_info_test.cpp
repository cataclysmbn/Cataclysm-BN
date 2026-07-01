#include "catch/catch.hpp"
#include "path_info.h"

TEST_CASE("test harness config directory is isolated under user directory", "[path_info]") {
    const auto expected = PATH_INFO::user_dir() + "config/";
    CHECK(PATH_INFO::config_dir() == expected);
}
