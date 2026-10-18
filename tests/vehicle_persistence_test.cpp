#include "catch/catch.hpp"
#include "clzones.h"
#include "coordinates.h"
#include "debug.h"
#include "game_constants.h"
#include "json.h"
#include "type_id.h"
#include "vehicle/vehicle.h"
#include "vehicle/vehicle_part.h"

#include <array>
#include <limits>
#include <sstream>
#include <string>
#include <utility>

TEST_CASE(
    "vehicle_part_oldsave_defaults_mount_z_without_flattening_terrain",
    "[vehicle][vehicle_multiz][save][vehicle_persistence]") {
    const auto part_json = std::string(R"json({
        "id": "frame_horizontal",
        "mount_dx": 2,
        "mount_dy": -1,
        "direction": 0,
        "z_offset": 1
    })json");

    SECTION("reused part resets a previous permanent mount z") {
        auto part = vehicle_part();
        part.mount = tripoint_mnt_veh(9, 8, 7);
        part.z_terrain = {-3, 4};
        auto input = std::istringstream(part_json);
        auto jsin = JsonIn(input);

        REQUIRE(jsin.read(part, true));
        CHECK(part.mount == tripoint_mnt_veh(2, -1, 0));
        CHECK(part.z_terrain == std::array{1, 1});
    }

    SECTION("saved vehicle parts retain terrain through vector insertion") {
        auto input = std::istringstream("{\"turn_dir\":0,\"parts\":[" + part_json + "]}");
        auto jsin = JsonIn(input);
        auto loaded = vehicle();
        loaded.abs_sm_pos = tripoint_abs_sm(3, 4, 2);

        REQUIRE(jsin.read(loaded, true));
        REQUIRE(loaded.part_count() == 1);
        CHECK(loaded.part(0).mount == tripoint_mnt_veh(2, -1, 0));
        CHECK(loaded.part(0).z_terrain == std::array{1, 1});
        CHECK(loaded.abs_part_location(0).z() == 3);
    }
}

TEST_CASE(
    "vehicle_part_mount_dz_rejects_invalid_numbers",
    "[vehicle][vehicle_multiz][save][vehicle_persistence]") {
    // Even integral decimals/exponents are deliberately rejected for this new field.
    const auto mount_dz = GENERATE(
        as<std::string>{}, "1.5", "-0.5", "0.0", "1.0", "-2.0", "1e0", "-2e0", "1e-2", "1e-400",
        "-1e-400", "1e309", "-1e309", "18446744073709551616", "-18446744073709551616", "2147483648",
        "-2147483649", "00", "01", "-01", "+1", "--1", "1-2", "-", "null", "true", "\"1\"", "[]",
        "{}", std::to_string(std::numeric_limits<int>::min()),
        std::to_string(std::numeric_limits<int>::max()),
        std::to_string(-(2 * (OVERMAP_DEPTH + OVERMAP_HEIGHT) + 1)),
        std::to_string(2 * (OVERMAP_DEPTH + OVERMAP_HEIGHT) + 1));
    CAPTURE(mount_dz);
    auto input = std::istringstream(
        std::string(R"({"id":"frame_horizontal","direction":0,"mount_dz":)") + mount_dz + "}");
    auto jsin = JsonIn(input);
    auto part = vehicle_part();

    CHECK_THROWS_AS(part.deserialize(jsin), JsonError);
}

TEST_CASE(
    "vehicle_part_mount_dz_accepts_bounded_integer_tokens",
    "[vehicle][vehicle_multiz][save][vehicle_persistence]") {
    const auto mount_dz = GENERATE(
        -2 * (OVERMAP_DEPTH + OVERMAP_HEIGHT), -2, 0, 3, 2 * (OVERMAP_DEPTH + OVERMAP_HEIGHT));
    const auto whitespace = GENERATE("", " \t\r\n");
    const auto following_member = GENERATE(false, true);
    CAPTURE(mount_dz, whitespace, following_member);
    auto input = std::istringstream(
        std::string(R"({"id":"frame_horizontal","direction":0,"mount_dz":)") + whitespace
        + std::to_string(mount_dz) + whitespace
        + (following_member ? ", \t\r\n\"z_offset\":-1}" : "}"));
    auto jsin = JsonIn(input);
    auto part = vehicle_part();

    REQUIRE(jsin.read(part, true));
    CHECK(part.mount.z() == mount_dz);
    CHECK(part.z_terrain == (following_member ? std::array{-1, -1} : std::array{0, 0}));
}

TEST_CASE(
    "vehicle_load_skips_invalid_mount_dz_and_keeps_other_parts",
    "[vehicle][vehicle_multiz][save][vehicle_persistence]") {
    const auto invalid_mount_dz =
        GENERATE(as<std::string>{}, "18446744073709551616", "1e-400", "2.0", "01", "null",
                 std::to_string(2 * (OVERMAP_DEPTH + OVERMAP_HEIGHT) + 1));
    CAPTURE(invalid_mount_dz);
    auto input = std::istringstream(
        R"json({"turn_dir":0,"parts":[
            {"id":"frame_horizontal","mount_dx":2,"mount_dy":-1,"mount_dz":-2,
             "direction":0,"z_offset":1},
            {"id":"frame_horizontal","mount_dz":)json"
        + invalid_mount_dz + R"json(,
             "direction":0},
            {"id":"frame_horizontal","mount_dx":2,"mount_dy":-1,"mount_dz":2,
             "direction":0,"z_offset":-1}
        ]})json");
    auto jsin = JsonIn(input);
    auto veh = vehicle();
    veh.abs_sm_pos = tripoint_abs_sm(3, 4, 2);
    auto loaded = false;
    const auto warning = capture_debugmsg_during([&]() { loaded = jsin.read(veh, true); });

    REQUIRE(loaded);
    CHECK(warning.find("Skipping invalid saved vehicle part") != std::string::npos);
    CHECK(warning.find("mount_dz") != std::string::npos);
    REQUIRE(veh.part_count() == 2);
    CHECK(veh.part(0).mount == tripoint_mnt_veh(2, -1, -2));
    CHECK(veh.part(0).z_terrain == std::array{1, 1});
    CHECK(veh.part(1).mount == tripoint_mnt_veh(2, -1, 2));
    CHECK(veh.part(1).z_terrain == std::array{-1, -1});
}

TEST_CASE(
    "vehicle_roundtrip_preserves_signed_mounts_labels_and_zone_keys",
    "[vehicle][vehicle_multiz][save][vehicle_persistence]") {
    const auto terrain_offset = GENERATE(-1, 1);
    CAPTURE(terrain_offset);
    auto original = vehicle();
    original.abs_sm_pos = tripoint_abs_sm(3, 4, 2);
    original.sm_ms_pos = point_sm_ms(5, 6);

    auto zone_input = std::istringstream(R"json({
        "name": "deck cargo",
        "type": "LOOT_UNSORTED",
        "faction": "your_followers",
        "invert": false,
        "enabled": true,
        "is_vehicle": true,
        "start": [0, 0, 0],
        "end": [0, 0, 0]
    })json");
    auto zone_jsin = JsonIn(zone_input);
    auto zone = zone_data();
    REQUIRE(zone_jsin.read(zone, true));

    for (const auto mount_dz : std::array{0, 2, -2}) {
        const auto mount = tripoint_mnt_veh(2, -1, mount_dz);
        const auto index = original.install_part(mount, vpart_id("frame_horizontal"), true);
        REQUIRE(index >= 0);
        original.part(index).z_terrain = {terrain_offset, -terrain_offset};
        original.labels.emplace(mount, std::to_string(mount_dz));
        original.loot_zones.emplace(mount, zone);
    }

    auto output = std::ostringstream();
    auto jsout = JsonOut(output);
    original.serialize(jsout);
    auto input = std::istringstream(output.str());
    auto jsin = JsonIn(input);
    auto loaded = vehicle();
    // The owning submap supplies the origin; detached loading must not recompute terrain.
    loaded.abs_sm_pos = original.abs_sm_pos;

    REQUIRE(jsin.read(loaded, true));
    REQUIRE(loaded.part_count() == original.part_count());
    CHECK(loaded.abs_ms_location() == original.abs_ms_location());
    CHECK(loaded.labels.size() == original.labels.size());
    CHECK(loaded.loot_zones.size() == original.loot_zones.size());

    for (auto index = 0; index < original.part_count(); ++index) {
        const auto& source = original.part(index);
        const auto& restored = loaded.part(index);
        CAPTURE(index, source.mount);
        CHECK(source.z_terrain == std::array{terrain_offset, -terrain_offset});
        CHECK(restored.mount == source.mount);
        // Only the active terrain offset is persisted, then loaded into both buffers.
        CHECK(restored.z_terrain == std::array{terrain_offset, terrain_offset});
        CHECK(loaded.abs_part_location(index) == original.abs_part_location(index));

        const auto restored_label = loaded.labels.find(label(source.mount));
        REQUIRE(restored_label != loaded.labels.end());
        CHECK(restored_label->text == std::to_string(source.mount.z()));
        REQUIRE(loaded.loot_zones.count(source.mount) == 1);
        const auto& restored_zone = loaded.loot_zones.find(source.mount)->second;
        CHECK(restored_zone.get_name() == zone.get_name());
        CHECK(restored_zone.get_type() == zone.get_type());
        CHECK(restored_zone.get_is_vehicle());
    }
}

TEST_CASE(
    "vehicle_part_copy_move_preserve_both_terrain_buffers",
    "[vehicle][vehicle_multiz][vehicle_persistence][lifecycle]") {
    auto owner = vehicle();
    const auto mount = tripoint_mnt_veh(2, -1, -2);
    const auto index = owner.install_part(mount, vpart_id("frame_horizontal"), true);
    REQUIRE(index >= 0);
    const auto precalc = std::array{point_rel_ms(3, 4), point_rel_ms(-5, 6)};
    const auto terrain = std::array{1, -2};
    auto& source = owner.part(index);
    source.precalc = precalc;
    source.z_terrain = terrain;

    auto copy_owner = vehicle();
    auto copied = vehicle_part(source, &copy_owner);
    CHECK(copied.mount == mount);
    CHECK(copied.precalc == precalc);
    CHECK(copied.z_terrain == terrain);

    auto moved = vehicle_part(std::move(copied));
    CHECK(moved.mount == mount);
    CHECK(moved.precalc == precalc);
    CHECK(moved.z_terrain == terrain);

    auto assigned = vehicle_part(source, &copy_owner);
    assigned.mount = tripoint_mnt_veh(-7, 8, 4);
    assigned.precalc = {point_rel_ms(-9, 8), point_rel_ms(7, -6)};
    assigned.z_terrain = {7, 8};
    assigned = std::move(moved);
    CHECK(assigned.mount == mount);
    CHECK(assigned.precalc == precalc);
    CHECK(assigned.z_terrain == terrain);
}
