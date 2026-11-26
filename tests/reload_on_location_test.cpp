#include "../src/map/map.h"
#include "../src/vehicle/vehicle_part.h"
#include "../src/vehicle/vehicle_selector.h"
#include "avatar.h"
#include "avatar_action.h"
#include "calendar.h"
#include "catch/catch.hpp"
#include "game.h"
#include "inventory.h"
#include "item.h"
#include "map_helpers.h"
#include "player_activity.h"
#include "player_helpers.h"
#include "state_helpers.h"
#include "type_id.h"
#include "vehicle/veh_type.h"
#include "vehicle/vehicle.h"

#include <list>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

TEST_CASE("reload_on_vehicle_cargo", "[magazine][visitable][item][item_location][reload]") {
    clear_all_state();
    const auto z = g->u.abs_pos().z();
    // Spawn an avatar...
    auto& who = get_avatar();

    // Spawn a car...
    map& here = get_map();
    const auto vehicle_center = tripoint_bub_ms(65, 65, z);
    const vproto_id car_id("car");

    vehicle* veh = here.add_vehicle(car_id, vehicle_center, 0_radians, 0, 0, false);
    REQUIRE(veh != nullptr);

    // Create cargo for vehicle...
    int part_num = veh->part_with_feature(0, VPFLAG_CARGO, true);
    REQUIRE(part_num >= 0);

    // Place a verified reloadable target in vehicle cargo...
    auto gun_owner = item::spawn("sw_619", calendar::start_of_cataclysm, 0);
    auto& gun = *gun_owner;

    auto remaining = veh->add_item(part_num, std::move(gun_owner));
    REQUIRE(!remaining); // Cargo accepted gun, I think.

    // Give the avatar a compatible source and position within reach...
    auto& ammo = who.i_add(item::spawn("38_special", calendar::start_of_cataclysm, 6));
    who.setpos(veh->bub_part_location(part_num)); // Position avatar on cargo tile...
    REQUIRE(gun.ammo_remaining() == 0);
    REQUIRE(gun.can_reload_with(ammo.typeId()));
    REQUIRE_FALSE(who.has_item(gun));

    // Invoke public reload caller without prompting..
    avatar_action::reload(gun, false, false);

    // Assert scheduled target/source reference and quantity...
    REQUIRE(who.activity);
    CHECK(who.activity->id() == activity_id("ACT_RELOAD"));

    REQUIRE(who.activity->targets.size() == 2);
    CHECK(&*who.activity->targets[0] == &gun);
    CHECK(&*who.activity->targets[1] == &ammo);
    CHECK(who.activity->index == 1);

    CHECK(who.has_item(gun));
    CHECK(gun.ammo_remaining() == 0);
    CHECK(ammo.charges == 6);

    // Complete activity...
    process_activity(who);

    // Assert target ammunition, source consumption, and target's resulting location...
    CHECK_FALSE(who.activity);
    CHECK(gun.ammo_remaining() == 1);
    CHECK(gun.ammo_current() == ammo.typeId());
    CHECK(ammo.charges == 5);
    CHECK(who.has_item(gun));
}
