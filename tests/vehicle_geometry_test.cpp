#include "catch/catch.hpp"
#include "clzones.h"
#include "coordinates.h"
#include "game_constants.h"
#include "json.h"
#include "json_export.h"
#include "map/map.h"
#include "map/mapbuffer.h"
#include "map_helpers.h"
#include "state_helpers.h"
#include "type_id.h"
#include "units.h"
#include "vehicle/vehicle.h"
#include "vehicle/vehicle_part.h"
#include "vehicle/vpart_range.h"

#include <array>
#include <map>
#include <ranges>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace {

auto install_frame(vehicle& veh, const tripoint_mnt_veh& mount) -> int {
    const auto index = veh.install_part(mount, vpart_id("frame_vertical"), true);
    REQUIRE(index >= 0);
    return index;
}

} // namespace

TEST_CASE(
    "vehicle_nominal_geometry_preserves_xyz_at_every_yaw",
    "[vehicle][vehicle_multiz][vehicle_geometry]") {
    auto veh = vehicle();
    veh.abs_sm_pos = tripoint_abs_sm(5, -3, 4);
    veh.sm_ms_pos = point_sm_ms(7, 5);
    const auto mounts = std::array{
        tripoint_mnt_veh(-7, 4, -2), tripoint_mnt_veh(-2, -5, -1), tripoint_mnt_veh(0, 0, 0),
        tripoint_mnt_veh(3, -2, 1), tripoint_mnt_veh(5, 7, 3)};
    for (const auto& mount : mounts) { install_frame(veh, mount); }

    const auto world_origin = veh.abs_ms_location();
    const auto bubble_origin = veh.bub_ms_location();
    for (auto yaw = 0_degrees; yaw < 360_degrees; yaw += 15_degrees) {
        for (const auto& pivot :
             std::array{tripoint_mnt_veh(3, -2, 5), tripoint_mnt_veh(-4, 5, -6)}) {
            CAPTURE(yaw, pivot);
            const auto horizontal_pivot = tripoint_mnt_veh(pivot.xy(), 0);
            veh.set_facing_and_pivot(yaw, pivot, false);
            CHECK(veh.pivot_anchor[0] == horizontal_pivot);
            for (const auto slot : {0, 1}) {
                veh.precalc_mounts(slot, yaw, pivot);
                CHECK(veh.pivot_anchor[slot] == horizontal_pivot);
            }
            CHECK(veh.pivot_displacement() == tripoint_rel_ms::zero());

            for (const auto& vp : veh.get_all_parts()) {
                const auto mount = vp.mount();
                CAPTURE(mount);
                const auto planar = veh.coord_translate(tripoint_mnt_veh(mount.xy(), 0));
                const auto relative = tripoint_rel_ms(planar.xy(), mount.z());
                const auto world = world_origin + relative;
                const auto bubble = bubble_origin + relative;

                CHECK(veh.coord_translate(mount) == relative);
                CHECK(veh.mount_to_abs(mount) == world);
                CHECK(veh.mount_to_bubble(mount) == bubble);
                CHECK(veh.abs_to_mount(world) == mount);
                CHECK(veh.bubble_to_mount(bubble) == mount);
                CHECK(veh.abs_part_location(vp.part()) == world);
                CHECK(veh.bub_part_location(vp.part()) == bubble);
                CHECK(veh.mount_to_abs(mount).z() == world_origin.z() + mount.z());

                auto reversed = tripoint_mnt_veh::zero();
                veh.coord_translate_reverse(yaw, pivot, relative, reversed);
                CHECK(reversed == mount);

                /// Exercise pivot-z handling in the transforms, not just pivot normalization.
                veh.pivot_anchor[0] = pivot;
                CHECK(veh.coord_translate(mount) == relative);
                CHECK(veh.mount_to_abs(mount) == world);
                CHECK(veh.mount_to_bubble(mount) == bubble);
                CHECK(veh.abs_to_mount(world) == mount);
                CHECK(veh.bubble_to_mount(bubble) == mount);
                veh.pivot_anchor[0] = horizontal_pivot;
            }

            const auto pending_pivot = tripoint_mnt_veh(-2, 6, -9);
            veh.precalc_mounts(1, yaw + 15_degrees, pending_pivot);
            CHECK(veh.pivot_anchor[1] == tripoint_mnt_veh(pending_pivot.xy(), 0));
            auto displacement_xy = point_rel_ms::zero();
            veh.coord_translate(yaw, pending_pivot, horizontal_pivot, displacement_xy);
            CHECK(veh.pivot_displacement() == tripoint_rel_ms(displacement_xy, 0));
        }
    }
}

TEST_CASE("vehicle_shift_parts_rebases_only_xy", "[vehicle][vehicle_multiz][vehicle_geometry]") {
    using namespace std::views;
    namespace ranges = std::ranges;

    const auto ignored_z = GENERATE(-5, 0, 7);
    CAPTURE(ignored_z);
    auto veh = vehicle();
    veh.abs_sm_pos = tripoint_abs_sm(5, -3, 4);
    veh.sm_ms_pos = point_sm_ms(7, 5);
    const auto mounts = std::
        array{tripoint_mnt_veh(3, -2, -2), tripoint_mnt_veh(3, -2, 2), tripoint_mnt_veh(4, -2, 0)};
    for (const auto& mount : mounts) {
        install_frame(veh, mount);
        if (mount.z() != 0) { REQUIRE(veh.install_part(mount, vpart_id("cargo_bag"), true) >= 0); }
    }
    veh.set_facing_and_pivot(75_degrees, tripoint_mnt_veh(2, 4, 0));
    veh.precalc_mounts(1, 120_degrees, tripoint_mnt_veh(-3, 1, 0));

    for (const auto& mount : mounts) {
        if (mount.z() == 0) { continue; }
        const auto name = "deck " + std::to_string(mount.z());
        const auto world = veh.mount_to_abs(mount);
        veh.labels.emplace(mount, name);
        auto zone = zone_data(
            name, zone_type_id("LOOT_UNSORTED"), faction_id("your_followers"), false, true, world,
            world);
        zone.set_is_vehicle(true);
        veh.loot_zones.emplace(mount, zone);
    }

    struct part_snapshot {
        tripoint_mnt_veh mount;
        tripoint_abs_ms world;
        std::array<point_rel_ms, 2> precalc;
    };
    const auto before =
        veh.get_all_parts() | transform([&veh](const auto& vp) -> part_snapshot {
            return {.mount = vp.mount(),
                    .world = veh.abs_part_location(vp.part()),
                    .precalc = vp.part().precalc};
        })
        | ranges::to<std::vector>();
    const auto labels_before = veh.labels;
    const auto zones_before = veh.loot_zones;
    const auto pivots_before = veh.pivot_anchor;
    const auto rotations_before = veh.pivot_rotation;
    const auto displacement_before = veh.pivot_displacement();
    const auto origin_before = veh.abs_ms_location();
    const auto xy_delta = tripoint_rel_veh(3, -2, 0);

    veh.shift_parts(tripoint_rel_veh(3, -2, ignored_z));

    CHECK(veh.abs_ms_location() == origin_before);
    CHECK(veh.pivot_rotation == rotations_before);
    CHECK(veh.pivot_displacement() == displacement_before);
    for (const auto slot : {0, 1}) {
        CHECK(veh.pivot_anchor[slot] == pivots_before[slot] - xy_delta);
        CHECK(veh.pivot_anchor[slot].z() == 0);
    }
    REQUIRE(veh.part_count() == static_cast<int>(before.size()));
    for (auto index = 0; index < veh.part_count(); ++index) {
        const auto& old = before[index];
        const auto& part = veh.cpart(index);
        CAPTURE(index, old.mount);
        CHECK(part.mount == old.mount - xy_delta);
        CHECK(part.mount.z() == old.mount.z());
        CHECK(part.precalc == old.precalc);
        CHECK(veh.abs_part_location(index) == old.world);
        CHECK(veh.mount_to_abs(part.mount) == old.world);
        CHECK(veh.bub_part_location(index) == abs_to_map_local(get_map(), old.world));
    }
    REQUIRE(veh.labels.size() == labels_before.size());
    for (const auto& old : labels_before) {
        const auto found = veh.labels.find(label(old - xy_delta));
        REQUIRE(found != veh.labels.end());
        CHECK(found->text == old.text);
        CHECK(found->z() == old.z());
    }
    REQUIRE(veh.loot_zones.size() == zones_before.size());
    for (const auto& [mount, zone] : zones_before) {
        const auto shifted_mount = mount - xy_delta;
        REQUIRE(veh.loot_zones.count(shifted_mount) == 1);
        const auto& [zone_mount, shifted_zone] = *veh.loot_zones.find(shifted_mount);
        CHECK(zone_mount.z() == mount.z());
        CHECK(shifted_zone.get_name() == zone.get_name());
        CHECK(shifted_zone.get_start_point() == zone.get_start_point());
        CHECK(shifted_zone.get_end_point() == zone.get_end_point());
        CHECK(shifted_zone.get_is_vehicle());
        CHECK(veh.mount_to_abs(shifted_mount) == zone.get_start_point());
    }

    CHECK(veh.parts_at_relative(tripoint_mnt_veh::zero(), true).empty());
    CHECK_FALSE(veh.shift_if_needed());
    CHECK_FALSE(veh.shift_if_needed());
    CHECK(veh.abs_ms_location() == origin_before);
    CHECK(veh.cpart(0).mount == mounts.front() - xy_delta);
}

TEST_CASE(
    "vehicle_removal_keeps_an_upper_origin_column_without_repeated_rebasing",
    "[vehicle][vehicle_multiz][vehicle_geometry]") {
    clear_all_state();
    build_test_map(ter_id("t_pavement"));
    move_player_out_of_the_way();

    const auto upper_mount = GENERATE(tripoint_mnt_veh(0, 0, 1), tripoint_mnt_veh(2, -1, 1));
    CAPTURE(upper_mount);
    auto& here = get_map();
    auto* veh = here.add_vehicle(vproto_id("none"), tripoint_bub_ms(60, 60, 0), 30_degrees, 0, 0);
    REQUIRE(veh != nullptr);
    const auto removed = install_frame(*veh, tripoint_mnt_veh::zero());
    const auto upper = install_frame(*veh, upper_mount);
    const auto other_mount = tripoint_mnt_veh(3, -1, 0);
    const auto other = install_frame(*veh, other_mount);
    const auto survivor_mounts = std::array{upper_mount, other_mount};
    const auto world_positions =
        std::array{veh->abs_part_location(upper), veh->abs_part_location(other)};
    const auto origin_before = veh->abs_ms_location();
    const auto expected_delta = tripoint_rel_veh(upper_mount.x(), upper_mount.y(), 0);

    CHECK(veh->remove_part(removed) == (expected_delta != tripoint_rel_veh::zero()));
    CHECK(veh->cpart(removed).removed);
    CHECK(veh->cpart(upper).mount == upper_mount - expected_delta);
    CHECK(veh->cpart(other).mount == other_mount - expected_delta);
    CHECK(veh->abs_part_location(upper) == world_positions[0]);
    CHECK(veh->abs_part_location(other) == world_positions[1]);
    CHECK_FALSE(veh->shift_if_needed());

    veh->part_removal_cleanup();

    REQUIRE(veh->part_count() == 2);
    CHECK(veh->abs_ms_location() == origin_before);
    CHECK(veh->parts_at_relative(tripoint_mnt_veh::zero(), true).empty());
    CHECK_FALSE(veh->shift_if_needed());
    CHECK_FALSE(veh->shift_if_needed());
    for (auto index = 0; index < 2; ++index) {
        const auto mount = survivor_mounts[index] - expected_delta;
        const auto parts = veh->parts_at_relative(mount, true);
        REQUIRE(parts.size() == 1);
        CHECK(veh->cpart(parts.front()).mount.z() == survivor_mounts[index].z());
        CHECK(veh->cpart(parts.front()).z_terrain[0] == 0);
        CHECK(veh->abs_part_location(parts.front()) == world_positions[index]);
    }
    here.destroy_vehicle(veh);
}

TEST_CASE(
    "vehicle_export_groups_full_mounts_independently_of_placement",
    "[vehicle][vehicle_multiz][vehicle_geometry]") {
    auto veh = vehicle();
    veh.type = vproto_id("vehicle_geometry_export_test");
    veh.name = "vehicle geometry export test";
    veh.abs_sm_pos = tripoint_abs_sm(5, -3, 4);
    veh.sm_ms_pos = point_sm_ms(7, 5);
    const auto expected = std::map<tripoint_mnt_veh, std::multiset<std::string>>{
        {tripoint_mnt_veh(2, -1, -2), {"frame_vertical"}},
        {tripoint_mnt_veh(2, -1, 0), {"frame_vertical", "seat"}},
        {tripoint_mnt_veh(2, -1, 2), {"frame_vertical", "roof"}},
        {tripoint_mnt_veh(-3, 4, 1), {"frame_vertical"}}};
    const auto removed_only = install_frame(veh, tripoint_mnt_veh(7, 5, -1));
    const auto removed_shared =
        veh.install_part(tripoint_mnt_veh(2, -1, 2), vpart_id("seat"), true);
    REQUIRE(removed_shared >= 0);
    for (const auto& [mount, ids] : expected) {
        for (const auto& id : ids) { REQUIRE(veh.install_part(mount, vpart_id(id), true) >= 0); }
    }
    /// Retain removed entries, including one preceding live parts at the same mount.
    veh.part(removed_only).removed = true;
    veh.part(removed_shared).removed = true;
    veh.removed_part_count = 2;

    for (auto yaw = 0_degrees; yaw < 360_degrees; yaw += 15_degrees) {
        veh.set_facing_and_pivot(yaw, tripoint_mnt_veh(3, -2, 0));
        for (const auto terrain_z : {-2, 0, 3}) {
            CAPTURE(yaw, terrain_z);
            /// Detached terrain offsets must not leak into prototype geometry.
            for (const auto& vp : veh.get_all_parts()) {
                const auto offset = vp.mount().x() == 2 ? terrain_z : -terrain_z;
                vp.part().z_terrain = {offset, -offset};
            }

            auto output = std::ostringstream();
            auto jsout = JsonOut(output);
            json_export::vehicle(jsout, veh);
            auto input = std::istringstream(output.str());
            auto jsin = JsonIn(input);
            const auto root = jsin.get_object();
            CHECK(root.get_string("id") == veh.type.str());
            CHECK(root.get_string("type") == "vehicle");
            CHECK(root.get_string("name") == veh.name);
            auto exported = std::map<tripoint_mnt_veh, std::multiset<std::string>>{};
            const auto groups = root.get_array("parts");
            CHECK(groups.size() == expected.size());
            for (const auto entry : groups) {
                const auto group = entry.get_object();
                const auto mount =
                    tripoint_mnt_veh(group.get_int("x"), group.get_int("y"), group.get_int("z", 0));
                auto ids = std::multiset<std::string>{};
                if (group.has_array("parts")) {
                    CHECK_FALSE(group.has_member("part"));
                    for (const auto& id : group.get_string_array("parts")) { ids.insert(id); }
                } else {
                    ids.insert(group.get_string("part"));
                }
                CHECK_FALSE(ids.empty());
                CHECK(exported.emplace(mount, ids).second);
            }
            CHECK(exported == expected);
        }
    }
}

TEST_CASE(
    "vehicle_spatial_caches_reject_out_of_world_decks",
    "[vehicle][vehicle_multiz][vehicle_geometry]") {
    clear_all_state();
    auto& here = get_map();
    auto* veh = here.add_vehicle(vproto_id("none"), tripoint_bub_ms(60, 60, 0), 0_degrees);
    REQUIRE(veh != nullptr);
    const auto origin = install_frame(*veh, tripoint_mnt_veh::zero());
    for (const auto z : {-OVERMAP_DEPTH - 1, OVERMAP_HEIGHT + 1}) {
        install_frame(*veh, tripoint_mnt_veh(0, 0, z));
    }

    here.add_vehicle_to_cache(veh);
    here.get_mapbuffer().refresh_vehicle_footprint(veh);
    const auto origin_pos = veh->bub_part_location(origin);
    REQUIRE(here.veh_at(origin_pos));
    CHECK(&here.veh_at(origin_pos)->vehicle() == veh);
    for (const auto z : {-OVERMAP_DEPTH - 1, OVERMAP_HEIGHT + 1}) {
        const auto outside = tripoint_bub_ms(origin_pos.xy(), z);
        CHECK_FALSE(here.veh_at(outside));
        here.clear_vehicle_point_from_cache(veh, outside);
    }
    REQUIRE(here.veh_at(origin_pos));
    here.destroy_vehicle(veh);
    CHECK_FALSE(here.veh_at(origin_pos));
}
