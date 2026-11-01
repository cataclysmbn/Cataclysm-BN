#include "cata_utility.h"
#include "catch/catch.hpp"
#include "coordinates.h"
#include "json.h"
#include "type_id.h"
#include "units.h"
#include "vehicle/veh_type.h"
#include "vehicle/vehicle.h"
#include "vehicle/vehicle_group.h"
#include "vehicle/vehicle_part.h"

#include <map>
#include <set>
#include <sstream>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

extern std::unordered_map<vproto_id, vehicle_prototype> vtypes;

namespace {

auto install(vehicle& veh, tripoint_mnt_veh mount, const char* id = "frame_vertical") -> int {
    const auto index = veh.install_part(mount, vpart_id(id));
    REQUIRE(index >= 0);
    return index;
}

auto load_blueprint(const std::string& text) -> void {
    auto stream = std::istringstream(text);
    auto reader = JsonIn(stream);
    vehicle_prototype::load(reader.get_object());
}

auto parts(const vehicle& veh) -> std::map<tripoint_mnt_veh, std::multiset<vpart_id>> {
    auto result = std::map<tripoint_mnt_veh, std::multiset<vpart_id>>{};
    for (auto index = 0; index < veh.part_count(); ++index) {
        const auto& part = veh.cpart(index);
        result[part.mount].insert(part.info().get_id());
    }
    return result;
}

} // namespace

TEST_CASE("multiz_core_local_geometry", "[vehicle][vehicle_multiz][vehicle_multiz_core]") {
    auto veh = vehicle{};
    for (const auto z : {-1, 0, 1}) { install(veh, tripoint_mnt_veh(0, 0, z)); }
    veh.abs_sm_pos = tripoint_abs_sm(5, -3, 2);
    veh.set_facing_and_pivot(45_degrees, tripoint_mnt_veh(1, 2, 0));
    auto positions = std::vector<tripoint_abs_ms>{};
    for (auto index = 0; index < veh.part_count(); ++index) {
        const auto mount = veh.cpart(index).mount;
        const auto nominal = veh.mount_to_abs(mount);
        CHECK(nominal.z() == veh.abs_ms_location().z() + mount.z());
        CHECK(veh.abs_to_mount(nominal) == mount);
        veh.part(index).z_terrain = {1, 1};
        CHECK(veh.abs_part_location(index) == nominal + tripoint_rel_ms::above());
        positions.push_back(veh.abs_part_location(index));
    }
    veh.shift_parts(tripoint_rel_veh(2, -1, 9));
    for (auto index = 0; index < veh.part_count(); ++index) {
        CHECK(veh.cpart(index).mount.z() == index - 1);
        CHECK(veh.abs_part_location(index) == positions[index]);
    }
}

TEST_CASE("multiz_core_layered_blueprints", "[vehicle][vehicle_multiz][vehicle_multiz_core]") {
    const auto saved_types = restore_on_out_of_scope<decltype(vtypes)>(std::move(vtypes));
    const auto saved_groups = restore_on_out_of_scope<decltype(vgroups)>(std::move(vgroups));
    vehicle_prototype::reset();
    VehicleGroup::reset();
    load_blueprint(R"({"id":"core_layered","name":"Core layered",
        "palette":{"s":["frame_vertical","seat"],"f":["frame_vertical"],"c":["seat"]},
        "blueprint":[{"z":-1,"rows":["s"]},{"z":0,"rows":["f"]},{"z":0,"rows":["c"]}]})");
    load_blueprint(R"({"id":"core_explicit","name":"Core explicit","parts":[
        {"x":0,"y":0,"z":-1,"parts":["frame_vertical","seat"]},
        {"x":0,"y":0,"z":0,"parts":["frame_vertical","seat"]}]})");
    load_blueprint(R"({"id":"core_flat","name":"Core flat",
        "palette":{"s":["frame_vertical","seat"]},"blueprint":["s"]})");
    vehicle_prototype::finalize();
    const auto& layered = *vproto_id("core_layered")->blueprint;
    CHECK(parts(layered) == parts(*vproto_id("core_explicit")->blueprint));
    CHECK(layered.part_count() == 4);
    const auto& flat = *vproto_id("core_flat")->blueprint;
    CHECK(flat.part_count() == 2);
    CHECK(flat.cpart(0).mount == tripoint_mnt_veh::zero());
}

TEST_CASE("multiz_core_normal_construction", "[vehicle][vehicle_multiz][vehicle_multiz_core]") {
    const auto cover = vpart_id(GENERATE("roof", "solar_panel"));
    auto veh = vehicle{};
    const auto bottom = tripoint_mnt_veh::zero();
    const auto upper = bottom + tripoint_above;
    CHECK_FALSE(veh.can_mount(upper, vpart_id("seat")));
    install(veh, bottom);
    install(veh, bottom + point_west);
    const auto column = install(veh, upper);
    install(veh, upper + point_east);
    const auto seat = install(veh, upper + point_east, "seat");
    CHECK_FALSE(veh.can_unmount(column));
    CHECK(veh.can_unmount(seat));
    install(veh, bottom + point_east);
    CHECK(veh.can_unmount(column));
    CHECK_FALSE(veh.can_mount(bottom, cover));
    CHECK(veh.can_mount(upper, cover));
    install(veh, bottom + point_west, cover.c_str());
    CHECK_FALSE(veh.can_mount(upper + point_west, vpart_id("frame_vertical")));
}

TEST_CASE("multiz_core_deck_preview", "[vehicle][vehicle_multiz][vehicle_multiz_core]") {
    auto veh = vehicle{};
    const auto lower = install(veh, tripoint_mnt_veh::zero());
    const auto exposed = install(veh, tripoint_mnt_veh(1, 0, 0));
    const auto upper = install(veh, tripoint_mnt_veh(0, 0, 1));
    CHECK(veh.all_standalone_parts(1) == std::vector<int>{upper});
    CHECK(veh.all_standalone_parts(0) == std::vector<int>{lower, exposed});
    CHECK(veh.all_standalone_parts(-1).empty());
    CHECK(veh.all_standalone_parts(2).empty());
    CHECK(veh.min_mount_z() == 0);
    CHECK(veh.max_mount_z() == 1);
}
