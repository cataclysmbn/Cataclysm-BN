#include "avatar.h"
#include "catch/catch.hpp"
#include "damage.h"
#include "options_helpers.h"
#include "state_helpers.h"
#include "type_id.h"
#include "vehicle/veh_type.h"

#include <array>

TEST_CASE("verify_copy_from_gets_damage_reduction", "[vehicle]") {
    clear_all_state();
    // Picking halfboard_horizontal as a vpart which is likely to remain
    // defined via copy-from, and which should have non-zero damage reduction.
    const vpart_info& vp = vpart_id("halfboard_horizontal").obj();
    CHECK(vp.damage_reduction.type_resist(DT_BASH) != 0);
}

/// Checks that VEHICLE_WORK_SCALING scales vehicle install, removal, and repair times
/// proportionally (including for skill-less parts) and clamps 0% to 1 move so activities
/// finish instantly without dividing by zero. Uses override_option on both a real vpart
/// and a synthetic skill-less vpart_info.
TEST_CASE("vehicle_work_scaling_option", "[vehicle]") {
    clear_all_state();
    const auto& you = get_avatar();
    const auto& part = vpart_id("halfboard_horizontal").obj();

    struct work_times {
        int install;
        int removal;
        int repair;
    };
    const auto base = [&] {
        const auto opt = override_option("VEHICLE_WORK_SCALING", "100");
        return work_times{
            .install = part.install_time(you),
            .removal = part.removal_time(you),
            .repair = part.repair_time(you),
        };
    }();

    REQUIRE(base.install > 1);
    REQUIRE(base.removal > 1);
    REQUIRE(base.repair > 1);

    SECTION("50% halves work time") {
        const auto opt = override_option("VEHICLE_WORK_SCALING", "50");
        CHECK(part.install_time(you) == base.install / 2);
        CHECK(part.removal_time(you) == base.removal / 2);
        CHECK(part.repair_time(you) == base.repair / 2);
    }

    SECTION("200% doubles work time") {
        const auto opt = override_option("VEHICLE_WORK_SCALING", "200");
        CHECK(part.install_time(you) == base.install * 2);
        CHECK(part.removal_time(you) == base.removal * 2);
        CHECK(part.repair_time(you) == base.repair * 2);
    }

    SECTION("0% makes vehicle work instant without returning zero moves") {
        const auto opt = override_option("VEHICLE_WORK_SCALING", "0");
        CHECK(part.install_time(you) == 1);
        CHECK(part.removal_time(you) == 1);
        CHECK(part.repair_time(you) == 1);
    }

    SECTION("skill-less parts still respect VEHICLE_WORK_SCALING") {
        auto skillless = vpart_info();
        skillless.install_skills.clear();
        skillless.install_moves = 10000;

        {
            const auto opt = override_option("VEHICLE_WORK_SCALING", "50");
            CHECK(skillless.install_time(you) == 5000);
        }
        {
            const auto opt = override_option("VEHICLE_WORK_SCALING", "200");
            CHECK(skillless.install_time(you) == 20000);
        }
        {
            const auto opt = override_option("VEHICLE_WORK_SCALING", "0");
            CHECK(skillless.install_time(you) == 1);
        }

        skillless.install_moves = 0;
        const auto opt = override_option("VEHICLE_WORK_SCALING", "0");
        CHECK(skillless.install_time(you) == 0);
    }
}
