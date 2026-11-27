#include "activity_handlers.h"
#include "avatar.h"
#include "avatar_functions.h"
#include "cata_utility.h"
#include "catch/catch.hpp"
#include "game.h"
#include "item.h"
#include "itype.h"
#include "iuse.h"
#include "map/map.h"
#include "map_helpers.h"
#include "monattack.h"
#include "monster.h"
#include "npc.h"
#include "player_activity.h"
#include "player_helpers.h"
#include "state_helpers.h"

#include <algorithm>

TEST_CASE("bungee_cord_retains_player_and_npc_weapons", "[monster][iuse][bungee]") {
    clear_all_state();
    const auto cleanup = on_out_of_scope(clear_all_state);
    const auto weapon_id = itype_id(GENERATE("test_bungee_melee", "test_bungee_gun"));
    const auto cord_id = itype_id(GENERATE("bungee_cord", "test_custom_weapon_tether"));
    const auto npc_target = GENERATE(false, true);
    const auto target_pos = tripoint_bub_ms{60, 60, 0};
    auto& you = get_avatar();
    you.setpos(npc_target ? target_pos + tripoint_west * 10 : target_pos);
    auto& wielder =
        npc_target ? static_cast<player&>(spawn_npc(target_pos, "test_talker"))
                   : static_cast<player&>(you);
    wielder.str_cur = 4;
    wielder.remove_primary_weapon();
    wielder.wield(item::spawn(weapon_id));
    auto& weapon = wielder.primary_weapon();
    REQUIRE(weapon.typeId() == weapon_id);
    const auto sling_slots = weapon.get_free_mod_locations(gunmod_location("sling"));

    auto* cord = &wielder.i_add(item::spawn(cord_id));
    REQUIRE(cord != nullptr);
    REQUIRE(cord->type->get_use("GUNMOD_ATTACH") != nullptr);
    REQUIRE(weapon.is_gunmod_compatible(*cord).success());
    auto activity = player_activity(activity_id("ACT_GUNMOD_ADD"));
    activity.targets.emplace_back(&weapon);
    activity.targets.emplace_back(cord);
    activity.values = {0, 100, 0, 0};
    activity_handlers::gunmod_add_finish(&activity, &wielder);
    REQUIRE(weapon.gunmod_find(cord_id) == cord);
    CHECK(weapon.get_free_mod_locations(gunmod_location("sling")) == sling_slots);

    auto& technician = spawn_test_monster("mon_zombie_technician", target_pos + tripoint_east);
    technician.set_goal(target_pos);
    REQUIRE(technician.attack_target() == &wielder);
    const auto initial_moves = technician.moves;
    CHECK(mattack::pull_metal_weapon(&technician));
    CHECK(technician.moves < initial_moves);
    REQUIRE(wielder.primary_weapon().typeId() == weapon_id);

    SECTION("a second cord cannot be attached") {
        auto* spare = &wielder.i_add(item::spawn("bungee_cord"));
        REQUIRE(spare != nullptr);
        CHECK_FALSE(weapon.is_gunmod_compatible(*spare).success());
    }

    SECTION("removing the cord restores the pull attack") {
        const auto* remove = weapon.get_use("detach_gunmods");
        REQUIRE(remove != nullptr);
        REQUIRE(remove->can_call(wielder, weapon, false, target_pos).success());
        REQUIRE(avatar_funcs::gunmod_remove(you, weapon, *cord));
        CHECK(weapon.gunmod_find(cord_id) == nullptr);
        CHECK((
            you.has_item(*cord)
            || std::ranges::any_of(get_map().i_at(you.bub_pos()), [cord](const auto* it) {
                   return it == cord;
               })));
        CHECK_FALSE(remove->can_call(wielder, weapon, false, target_pos).success());
        if (!weapon.is_gun()) { CHECK_FALSE(weapon.has_use()); }
        CHECK(mattack::pull_metal_weapon(&technician));
        CHECK(wielder.primary_weapon().is_null());
    }

    SECTION("the modified weapon can still be unwielded and transferred") {
        auto detached = wielder.remove_primary_weapon();
        REQUIRE(detached);
        CHECK(detached->gunmod_find(cord_id) == cord);
        CHECK(wielder.primary_weapon().is_null());
    }
}

TEST_CASE("unmodified_items_do_not_gain_mod_removal_actions", "[item][bungee]") {
    const auto item_id =
        GENERATE("sea_scooter_battery", "small_storage_battery", "test_bungee_melee");
    auto object = item::spawn(item_id);
    CHECK_FALSE(object->has_use());
    CHECK(object->get_use("detach_gunmods") == nullptr);
}

TEST_CASE("shoulder_strap_prevents_zombie_technicians_from_pulling_rifle", "[monster][gunmod]") {
    clear_all_state();
    const auto cleanup = on_out_of_scope(clear_all_state);
    auto& you = get_avatar();
    const auto target_pos = tripoint_bub_ms{60, 60, 0};
    you.setpos(target_pos);
    you.remove_primary_weapon();
    you.wield(item::spawn("m4a1"));
    auto& rifle = you.primary_weapon();
    const auto sling_slots = rifle.get_free_mod_locations(gunmod_location("sling"));
    auto* strap = &you.i_add(item::spawn("shoulder_strap"));
    REQUIRE(strap != nullptr);
    REQUIRE(rifle.is_gunmod_compatible(*strap).success());

    auto activity = player_activity(activity_id("ACT_GUNMOD_ADD"));
    activity.targets.emplace_back(&rifle);
    activity.targets.emplace_back(strap);
    activity.values = {0, 100, 0, 0};
    activity_handlers::gunmod_add_finish(&activity, &you);
    REQUIRE(rifle.gunmod_find(itype_id("shoulder_strap")) == strap);
    CHECK(rifle.get_free_mod_locations(gunmod_location("sling")) == sling_slots - 1);

    const auto bungee = item::spawn("bungee_cord");
    CHECK_FALSE(rifle.is_gunmod_compatible(*bungee).success());

    auto& technician = spawn_test_monster("mon_zombie_technician", target_pos + tripoint_east);
    technician.set_goal(target_pos);
    REQUIRE(technician.attack_target() == &you);
    CHECK(mattack::pull_metal_weapon(&technician));
    CHECK(you.primary_weapon().typeId() == itype_id("m4a1"));
}
