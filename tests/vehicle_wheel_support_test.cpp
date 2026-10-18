#include "catch/catch.hpp"
#include "coordinates.h"
#include "type_id.h"
#include "units.h"
#include "vehicle/vehicle.h"
#include "vehicle/vehicle_part.h"

#include <array>

TEST_CASE("vehicle_wheel_support_uses_xy_on_nonzero_decks", "[vehicle][vehicle_multiz][wheel]") {
    const auto wheel_z = GENERATE(-1, 1);
    const auto load_x = GENERATE(0, 20);
    CAPTURE(wheel_z, load_x);
    auto veh = vehicle();

    // Forced installs isolate wheel support; connectivity is not under test.
    for (const auto x : std::array{-1, 1}) {
        for (const auto y : std::array{-1, 1}) {
            const auto mount = tripoint_mnt_veh(x, y, wheel_z);
            for (const auto* id : std::array{"frame_vertical", "wheel_mount_medium", "wheel"}) {
                REQUIRE(veh.install_part(mount, vpart_id(id), true) >= 0);
            }
        }
    }
    REQUIRE(veh.wheelcache.size() == 4);
    for (const auto index : veh.wheelcache) { REQUIRE(veh.cpart(index).mount.z() == wheel_z); }
    const auto wheel_deck_mass = veh.total_mass();

    for (const auto y : std::array{-1, 1}) {
        REQUIRE(
            veh.install_part(
                tripoint_mnt_veh(load_x, y, wheel_z + 1), vpart_id("frame_vertical"), true)
            >= 0);
    }
    REQUIRE(veh.total_mass() > wheel_deck_mass);
    REQUIRE(veh.sufficient_wheel_config());

    // COM currently computes only XY, leaving z at zero; this does not test COM height.
    const auto center_xy = veh.local_center_of_mass().xy();
    CAPTURE(center_xy);
    if (load_x == 0) {
        REQUIRE(center_xy == point_mnt_veh::zero());
        CHECK(veh.valid_wheel_config());
    } else {
        REQUIRE(center_xy.x() > 1);
        REQUIRE(center_xy.y() == 0);
        CHECK_FALSE(veh.valid_wheel_config());
    }
}
