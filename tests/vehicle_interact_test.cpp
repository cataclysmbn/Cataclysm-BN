#include "../src/vehicle/veh_interact.h"
#include "../src/vehicle/vehicle_part.h"
#include "../src/vehicle/vpart_position.h"
#include "avatar.h"
#include "calendar.h"
#include "catch/catch.hpp"
#include "construction.h"
#include "coordinates.h"
#include "game.h"
#include "inventory.h"
#include "item.h"
#include "map.h"
#include "map_helpers.h"
#include "player_activity.h"
#include "player_helpers.h"
#include "requirements.h"
#include "state_helpers.h"
#include "type_id.h"
#include "vehicle/veh_type.h"
#include "vehicle/veh_utils.h"
#include "vehicle/vehicle.h"
#include "vehicle/vpart_range.h"

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

static const activity_id ACT_VEHICLE("ACT_VEHICLE");
static const trait_id trait_DEBUG_HS("DEBUG_HS");

static void test_repair(std::vector<detached_ptr<item>>& tools, bool expect_craftable) {

    const tripoint_bub_ms test_origin(60, 60, 0);
    g->u.setpos(test_origin);
    g->u.wear_item(item::spawn("backpack"), false);
    for (detached_ptr<item>& gear : tools) { g->u.i_add(std::move(gear)); }

    const tripoint_bub_ms vehicle_origin = test_origin + tripoint_rel_ms::south_east();
    vehicle* veh_ptr =
        get_map().add_vehicle(vproto_id("bicycle"), vehicle_origin, -90_degrees, 0, 0);
    REQUIRE(veh_ptr != nullptr);
    // Find the frame at the origin.
    vehicle_part* origin_frame = nullptr;
    for (vehicle_part* part : veh_ptr->get_parts_at(vehicle_origin, "", part_status_flag::any)) {
        if (part->info().location == "structure") {
            origin_frame = part;
            break;
        }
    }
    REQUIRE(origin_frame != nullptr);
    REQUIRE(origin_frame->hp() == origin_frame->info().durability);
    veh_ptr->mod_hp(*origin_frame, -100);
    REQUIRE(origin_frame->hp() < origin_frame->info().durability);

    const vpart_info& vp = origin_frame->info();
    // Assertions about frame part?

    requirement_data reqs = vp.repair_requirements();
    // Bust cache on crafting_inventory()
    g->u.mod_moves(1);
    inventory crafting_inv = g->u.crafting_inventory();
    bool can_repair = vp.repair_requirements().can_make_with_inventory(
        g->u.crafting_inventory(), is_crafting_component);
    CHECK(can_repair == expect_craftable);
}

TEST_CASE("repair_vehicle_part") {
    clear_all_state();
    const time_point bday = calendar::start_of_cataclysm;
    SECTION("welder") {
        std::vector<detached_ptr<item>> tools;
        tools.push_back(item::spawn("welder", bday, 500));
        tools.push_back(item::spawn("goggles_welding"));
        tools.push_back(item::spawn("material_aluminium_ingot", bday, 10));
        test_repair(tools, true);
    }
    SECTION("UPS_modded_welder") {
        std::vector<detached_ptr<item>> tools;
        detached_ptr<item> welder = item::spawn("welder", bday, 0);
        welder->put_in(item::spawn("battery_ups"));
        tools.push_back(std::move(welder));
        tools.push_back(item::spawn("UPS_off", bday, 500));
        tools.push_back(item::spawn("goggles_welding"));
        tools.push_back(item::spawn("material_aluminium_ingot", bday, 10));
        test_repair(tools, true);
    }
    SECTION("welder_missing_goggles") {
        std::vector<detached_ptr<item>> tools;
        tools.push_back(item::spawn("welder", bday, 500));
        tools.push_back(item::spawn("material_aluminium_ingot", bday, 10));
        test_repair(tools, false);
    }
    SECTION("welder_missing_charge") {
        std::vector<detached_ptr<item>> tools;
        tools.push_back(item::spawn("welder", bday, 5));
        tools.push_back(item::spawn("goggles_welding"));
        tools.push_back(item::spawn("material_aluminium_ingot", bday, 10));
        test_repair(tools, false);
    }
    SECTION("UPS_modded_welder_missing_charges") {
        std::vector<detached_ptr<item>> tools;
        detached_ptr<item> welder = item::spawn("welder", bday, 0);
        welder->put_in(item::spawn("battery_ups"));
        tools.push_back(std::move(welder));
        tools.push_back(item::spawn("UPS_off", bday, 5));
        tools.push_back(item::spawn("goggles_welding"));
        tools.push_back(item::spawn("material_aluminium_ingot", bday, 10));
        test_repair(tools, false);
    }
}

TEST_CASE("debug_hammerspace_installs_full_vehicle_battery", "[vehicle][veh_interact]") {
    clear_all_state();

    map& here = get_map();
    avatar& you = get_avatar();
    clear_avatar();
    you.toggle_trait(trait_DEBUG_HS);
    you.set_body();

    const tripoint_bub_ms vehicle_origin(60, 60, 0);
    you.setpos(vehicle_origin + point_south);

    vehicle* veh_ptr = here.add_vehicle(vproto_id("bicycle"), vehicle_origin, 0_degrees, 0, 0);
    REQUIRE(veh_ptr != nullptr);

    const auto install_part_id = vpart_id("storage_battery");
    const auto reference_part_index = 0;
    const auto reference_part = &veh_ptr->part(reference_part_index);
    const auto reference_pos = map_local_to_abs(here, veh_ptr->bub_part_location(*reference_part));

    you.assign_activity(ACT_VEHICLE, 1, static_cast<int>('i'));
    you.activity->values =
        {reference_pos.x(), reference_pos.y(), reference_pos.z(), 0, 0, 0, reference_part_index};
    you.activity->str_values.push_back(install_part_id.str());
    for (const tripoint_abs_ms& p : veh_ptr->get_points(true)) {
        you.activity->coord_set.insert(p);
    }

    veh_interact::complete_vehicle(you);

    const auto all_parts = veh_ptr->get_all_parts();
    const auto installed_battery = std::find_if(
        all_parts.begin(), all_parts.end(), [&install_part_id](const vpart_reference& part) {
            return part.info().get_id() == install_part_id;
        });

    REQUIRE(installed_battery != all_parts.end());
    CHECK(installed_battery->part().ammo_remaining() == installed_battery->part().ammo_capacity());
}

static detached_ptr<item> spawn_filled_container(const std::string& id, const std::string& liquid) {
    detached_ptr<item> can = item::spawn(id);
    can->fill_with(item::spawn(liquid), -1);
    REQUIRE_FALSE(can->contents.empty());
    REQUIRE(can->contents.front().typeId() == itype_id(liquid));
    return can;
}

TEST_CASE("filled_containers_install_as_vehicle_tanks", "[vehicle][crafting][construction]") {
    clear_all_state();
    avatar& you = get_avatar();
    clear_avatar();
    you.setpos(tripoint_bub_ms(60, 60, 0));
    you.wear_item(item::spawn("backpack"), false);

    const auto& tank_small = vpart_id("tank_small").obj();
    const auto tank_filter = veh_utils::install_component_filter(tank_small);

    SECTION("install menu sees a gasoline jerrycan as a 10L tank") {
        you.i_add(item::spawn("wrench"));
        you.i_add(item::spawn("hand_drill"));
        get_map().add_item_or_charges(you.bub_pos(), spawn_filled_container("jerrycan", "gasoline"));
        you.mod_moves(1);
        const inventory crafting_inv = you.crafting_inventory();
        CHECK_FALSE(crafting_inv.has_components(itype_id("jerrycan"), 1, is_crafting_component));
        CHECK(crafting_inv.has_components(itype_id("jerrycan"), 1, tank_filter));
        CHECK(tank_small.install_requirements().can_make_with_inventory(crafting_inv, tank_filter));
    }

    SECTION("install menu sees a diesel steel jerrycan as a 20L tank") {
        const auto& tank_medium = vpart_id("tank_medium").obj();
        you.i_add(item::spawn("wrench"));
        you.i_add(item::spawn("hand_drill"));
        get_map().add_item_or_charges(
            you.bub_pos(), spawn_filled_container("jerrycan_big", "diesel"));
        you.mod_moves(1);
        const inventory crafting_inv = you.crafting_inventory();
        CHECK(tank_medium.install_requirements().can_make_with_inventory(
            crafting_inv, veh_utils::install_component_filter(tank_medium)));
    }

    SECTION("consuming a filled jerrycan for a tank leaves the gasoline inside") {
        get_map().add_item_or_charges(you.bub_pos(), spawn_filled_container("jerrycan", "gasoline"));
        you.mod_moves(1);
        std::vector<item_comp> comps{item_comp(itype_id("jerrycan"), 1)};
        std::vector<detached_ptr<item>> used = you.consume_items(comps, 1, tank_filter, false);
        REQUIRE(used.size() == 1);
        CHECK(used[0]->typeId() == itype_id("jerrycan"));
        REQUIRE_FALSE(used[0]->contents.empty());
        CHECK(used[0]->contents.front().typeId() == itype_id("gasoline"));
    }

    SECTION("vehicle install keeps gasoline in the new tank") {
        map& here = get_map();
        const tripoint_bub_ms vehicle_origin(60, 60, 0);
        you.setpos(vehicle_origin + point_south);
        you.i_add(item::spawn("wrench"));
        you.i_add(item::spawn("hand_drill"));
        here.add_item_or_charges(you.bub_pos(), spawn_filled_container("jerrycan", "gasoline"));
        you.mod_moves(1);

        vehicle* veh_ptr = here.add_vehicle(vproto_id("bicycle"), vehicle_origin, 0_degrees, 0, 0);
        REQUIRE(veh_ptr != nullptr);

        const auto install_part_id = vpart_id("tank_small");
        const auto reference_part_index = 0;
        const auto reference_part = &veh_ptr->part(reference_part_index);
        const auto reference_pos =
            map_local_to_abs(here, veh_ptr->bub_part_location(*reference_part));

        you.assign_activity(ACT_VEHICLE, 1, static_cast<int>('i'));
        you.activity->values = {
            reference_pos.x(), reference_pos.y(), reference_pos.z(), 0, 0, 0, reference_part_index};
        you.activity->str_values.push_back(install_part_id.str());
        for (const tripoint_abs_ms& p : veh_ptr->get_points(true)) {
            you.activity->coord_set.insert(p);
        }

        veh_interact::complete_vehicle(you);

        const auto all_parts = veh_ptr->get_all_parts();
        const auto installed_tank = std::find_if(
            all_parts.begin(), all_parts.end(), [&install_part_id](const vpart_reference& part) {
                return part.info().get_id() == install_part_id;
            });
        REQUIRE(installed_tank != all_parts.end());
        CHECK(installed_tank->part().ammo_current() == itype_id("gasoline"));
        CHECK(installed_tank->part().ammo_remaining() > 0);
    }

    SECTION("wood stove construction still requires an empty metal tank") {
        get_map().add_item_or_charges(you.bub_pos(), spawn_filled_container("metal_tank", "water"));
        you.mod_moves(1);
        const inventory crafting_inv = you.crafting_inventory();
        CHECK_FALSE(crafting_inv.has_components(itype_id("metal_tank"), 1, is_crafting_component));
        CHECK(construction_str_id("constr_woodstove").is_valid());
    }
}
