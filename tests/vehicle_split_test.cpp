#include "catch/catch.hpp"
#include "character.h"
#include "map.h"
#include "map_helpers.h"
#include "point.h"
#include "state_helpers.h"
#include "type_id.h"
#include "veh_type.h"
#include "vehicle.h"
#include "vehicle_part.h"

#include <algorithm>
#include <memory>
#include <set>
#include <vector>

TEST_CASE("vehicle_split_section") {
    clear_all_state();
    Character& player_character = get_player_character();
    player_character.in_vehicle = false;
    player_character.controlling_vehicle = false;
    auto& here = player_character.get_mapbuffer();
    for (units::angle dir = 0_degrees; dir < 360_degrees; dir += 15_degrees) {
        CHECK(!player_character.in_vehicle);
        player_character.setpos(test_origin);
        auto vehicle_origin = tripoint_abs_ms(-5, -5, 0);
        auto vehs = here.get_vehicles();
        vehicle* veh_ptr;
        for (auto& vehs_v : vehs) {
            veh_ptr = vehs_v;
            here.destroy_vehicle(veh_ptr);
        }
        REQUIRE(here.get_vehicles().empty());
        veh_ptr = here.add_vehicle(vproto_id("cross_split_test"), vehicle_origin, dir, 0, 0);
        REQUIRE(veh_ptr != nullptr);
        std::set<tripoint_abs_ms> original_points = veh_ptr->get_points(true);

        here.destroy(vehicle_origin);
        veh_ptr->part_removal_cleanup();
        REQUIRE(veh_ptr->get_parts_at(vehicle_origin, "", part_status_flag::available).empty());
        vehs = here.get_vehicles();
        std::vector<vehicle*> vehs_copy;
        for (auto& vehs_v : vehs) { vehs_copy.push_back(vehs_v); }
        // destroying the center frame results in 4 new vehicles
        CHECK(vehs.size() == 4);
        if (vehs.size() == 4) {
            std::ranges::sort(vehs_copy, [](const vehicle* lhs, const vehicle* rhs) {
                return lhs->part_count() > rhs->part_count();
            });
            // correct number of parts
            CHECK(vehs_copy[0]->part_count() == 12);
            CHECK(vehs_copy[1]->part_count() == 12);
            // 1 Extra part for auto generated door lock ( 2 + 1 )
            CHECK(vehs_copy[2]->part_count() == 2 + 1);
            CHECK(vehs_copy[3]->part_count() == 3);
            std::vector<std::set<tripoint_abs_ms>> all_points;
            for (int i = 0; i < 4; i++) {
                std::set<tripoint_abs_ms>& veh_points = vehs_copy[i]->get_points(true);
                all_points.push_back(veh_points);
            }
            for (int i = 0; i < 4; i++) {
                std::set<tripoint_abs_ms>& veh_points = all_points[i];
                // every point in the new vehicle was in the old vehicle
                for (const tripoint_abs_ms& vpos : veh_points) {
                    CHECK(original_points.find(vpos) != original_points.end());
                }
                // no point in any new vehicle is in any other new vehicle
                for (int j = i + 1; j < 4; j++) {
                    std::set<tripoint_abs_ms>& other_points = all_points[j];
                    for (const tripoint_abs_ms& vpos : veh_points) {
                        CHECK(other_points.find(vpos) == other_points.end());
                    }
                }
            }
            here.destroy_vehicle(vehs_copy[3]);
            here.destroy_vehicle(vehs_copy[2]);
            here.destroy_vehicle(vehs_copy[1]);
            here.destroy_vehicle(vehs_copy[0]);
        }
        REQUIRE(here.get_vehicles().empty());
        vehicle_origin = tripoint_abs_ms(20, 20, 0);
        veh_ptr = here.add_vehicle(vproto_id("circle_split_test"), vehicle_origin, dir, 0, 0);
        REQUIRE(veh_ptr != nullptr);
        here.destroy(vehicle_origin);
        veh_ptr->part_removal_cleanup();
        REQUIRE(veh_ptr->get_parts_at(vehicle_origin, "", part_status_flag::available).empty());
        vehs = here.get_vehicles();
        vehs_copy.clear();
        for (auto& vehs_v : vehs) { vehs_copy.push_back(vehs_v); }
        CHECK(vehs_copy.size() == 1);
        if (vehs_copy.size() == 1) {
            CHECK(vehs_copy[0]->part_count() == 38 + 3); // 3 Extra part for auto generated door
                                                         // lock
        }
        break;
    }
}

TEST_CASE("split vehicle keeps selected structure part at origin") {
    clear_all_state();
    auto& here = get_map();
    const auto vehicle_origin = tripoint_abs_ms(10, 10, 0);
    auto* veh_ptr =
        here.add_vehicle(vproto_id("none"), abs_to_bub(vehicle_origin), 0_degrees, 0, 0);
    REQUIRE(veh_ptr != nullptr);

    const auto anchor_frame =
        veh_ptr->install_part(tripoint_mnt_veh::zero(), vpart_id("frame_vertical"), true);
    REQUIRE(anchor_frame >= 0);
    const auto split_mount = tripoint_mnt_veh(5, 0, 0);
    const auto split_frame = veh_ptr->install_part(split_mount, vpart_id("frame_vertical"), true);
    REQUIRE(split_frame >= 0);
    const auto split_seat = veh_ptr->install_part(split_mount, vpart_id("seat"), true);
    REQUIRE(split_seat >= 0);
    const auto original_split_pos = veh_ptr->bub_part_location(split_frame);

    REQUIRE(veh_ptr->split_vehicles({{split_frame, split_seat}}));
    const auto vehs = here.get_vehicles();
    REQUIRE(vehs.size() == 2);
    const auto split_vehicle = std::ranges::find_if(vehs, [veh_ptr](const auto& veh) {
        return veh.v != veh_ptr;
    });
    REQUIRE(split_vehicle != vehs.end());
    const auto* new_vehicle = split_vehicle->v;

    const auto origin_parts = new_vehicle->parts_at_relative(tripoint_mnt_veh::zero(), true);
    REQUIRE(!origin_parts.empty());
    CHECK(std::ranges::any_of(origin_parts, [new_vehicle](const auto part_index) {
        return new_vehicle->part_info(part_index).location == "structure";
    }));
    CHECK(new_vehicle->bub_part_location(origin_parts.front()) == original_split_pos);
}
