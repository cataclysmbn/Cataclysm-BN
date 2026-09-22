#include "avatar.h"
#include "avatar_action.h"
#include "avatar_functions.h"
#include "calendar.h"
#include "cata_utility.h"
#include "catch/catch.hpp"
#include "coordinates.h"
#include "flag.h"
#include "inventory.h"
#include "item.h"
#include "item_contents.h"
#include "itype.h"
#include "map/map.h"
#include "player.h"
#include "player_activity.h"
#include "player_helpers.h"
#include "point.h"
#include "reload/reload.h"
#include "safe_reference.h"
#include "state_helpers.h"
#include "type_id.h"
#include "uistate.h"
#include "units.h"
#include "value_ptr.h"
#include "vehicle/vehicle.h"
#include "vehicle/vehicle_part.h"

#include <climits>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

static const itype_id itype_glock_19("glock_19");
static const itype_id itype_glockmag("glockmag");

TEST_CASE("reload_gun_with_integral_magazine", "[reload],[gun]") {
    clear_all_state();
    const time_point bday = calendar::start_of_cataclysm;
    avatar& dummy = get_avatar();
    // Make sure the player doesn't drop anything :P
    dummy.wear_item(item::spawn("backpack", bday));

    detached_ptr<item> det = item::spawn("40sw", bday, item::default_charges_tag{});
    item& ammo = *det;
    dummy.i_add(std::move(det));
    det = item::spawn("sw_610", bday, item::default_charges_tag{});
    item& gun = *det;
    dummy.i_add(std::move(det));
    int ammo_pos = dummy.inv_position_by_item(&ammo);

    REQUIRE(ammo_pos != INT_MIN);
    REQUIRE(gun.ammo_remaining() == 0);
    REQUIRE(gun.magazine_integral());

    bool success = gun.reload(dummy, ammo, ammo.charges);

    REQUIRE(success);
    REQUIRE(gun.ammo_remaining() == gun.ammo_capacity());
}

TEST_CASE("reload_gun_with_integral_magazine_using_speedloader", "[reload],[gun]") {
    clear_all_state();
    const time_point bday = calendar::start_of_cataclysm;
    avatar& dummy = get_avatar();
    // Make sure the player doesn't drop anything :P
    dummy.wear_item(item::spawn("backpack", bday));

    detached_ptr<item> det = item::spawn("38_special", bday, item::default_charges_tag{});
    item& ammo = *det;
    dummy.i_add(std::move(det));
    det = item::spawn("38_speedloader", bday, false);
    item& speedloader = *det;
    dummy.i_add(std::move(det));
    int loader_pos = dummy.inv_position_by_item(&speedloader);
    det = item::spawn("sw_619", bday, false);
    item& gun = *det;
    dummy.i_add(std::move(det));
    int ammo_pos = dummy.inv_position_by_item(&ammo);

    REQUIRE(ammo_pos != INT_MIN);
    REQUIRE(gun.ammo_remaining() == 0);
    REQUIRE(gun.magazine_integral());
    REQUIRE(loader_pos != INT_MIN);
    REQUIRE(speedloader.ammo_remaining() == 0);
    REQUIRE(speedloader.has_flag(flag_SPEEDLOADER));

    bool speedloader_success = speedloader.reload(dummy, ammo, ammo.charges);

    REQUIRE(speedloader_success);
    REQUIRE(speedloader.ammo_remaining() == speedloader.ammo_capacity());

    bool success = gun.reload(dummy, speedloader, speedloader.ammo_remaining());

    REQUIRE(success);
    REQUIRE(gun.ammo_remaining() == gun.ammo_capacity());
    // Speedloader is still in inventory.
    REQUIRE(dummy.inv_position_by_item(&speedloader) != INT_MIN);
}

TEST_CASE("reload_gun_with_swappable_magazine", "[reload],[gun]") {
    clear_all_state();
    const time_point bday = calendar::start_of_cataclysm;
    avatar& dummy = get_avatar();
    // Make sure the player doesn't drop anything :P
    dummy.wear_item(item::spawn("backpack", bday));

    detached_ptr<item> det = item::spawn("9mm", bday, item::default_charges_tag{});
    item& ammo = *det;
    dummy.i_add(std::move(det));
    const cata::value_ptr<islot_ammo>& ammo_type = ammo.type->ammo;
    REQUIRE(ammo_type);

    detached_ptr<item> mag = item::spawn(itype_glockmag, bday, 0);
    const cata::value_ptr<islot_magazine>& magazine_type = mag->type->magazine;
    REQUIRE(magazine_type);
    REQUIRE(magazine_type->type.count(ammo_type->type) != 0);

    det = item::spawn(itype_glock_19, bday, item::default_charges_tag{});
    item& gun = *det;
    dummy.i_add(std::move(det));
    REQUIRE(gun.ammo_types().count(ammo_type->type) != 0);

    gun.put_in(std::move(mag));

    int gun_pos = dummy.inv_position_by_type(itype_glock_19);
    REQUIRE(gun_pos != INT_MIN);
    item& glock = dummy.i_at(gun_pos);
    // We're expecting the magazine to end up in the inventory.
    REQUIRE(avatar_funcs::unload_item(dummy, glock));
    int magazine_pos = dummy.inv_position_by_type(itype_glockmag);
    REQUIRE(magazine_pos != INT_MIN);
    item& magazine = dummy.inv_find_item(magazine_pos);
    REQUIRE(magazine.ammo_remaining() == 0);

    int ammo_pos = dummy.inv_position_by_item(&ammo);
    REQUIRE(ammo_pos != INT_MIN);

    bool magazine_success = magazine.reload(dummy, ammo, ammo.charges);

    REQUIRE(magazine_success);
    REQUIRE(magazine.ammo_remaining() == magazine.ammo_capacity());

    REQUIRE(gun.ammo_remaining() == 0);
    REQUIRE(gun.magazine_integral() == false);

    bool gun_success = gun.reload(dummy, magazine, 1);

    CHECK(gun_success);
    REQUIRE(gun.ammo_remaining() == gun.ammo_capacity());
}

static void reload_a_revolver(player& dummy, item& gun, item& ammo) {
    while (gun.ammo_remaining() < gun.ammo_capacity()) {
        avatar_action::reload_weapon(false);
        REQUIRE(dummy.activity);
        process_activity(dummy);
        CHECK(gun.ammo_remaining() > 0);
        CHECK(gun.ammo_current() == ammo.type->get_id());
    }
}

TEST_CASE("automatic_reloading_action", "[reload],[gun]") {
    clear_all_state();
    const time_point bday = calendar::start_of_cataclysm;
    avatar& dummy = get_avatar();
    // Make sure the player doesn't drop anything :P
    dummy.wear_item(item::spawn("backpack", bday));

    GIVEN("an unarmed player") {
        REQUIRE(!dummy.is_armed());
        WHEN("the player triggers auto reload") {
            avatar_action::reload_weapon(false);
            THEN("No activity is generated") { CHECK(!dummy.activity); }
        }
    }

    GIVEN("a player armed with a revolver and ammo for it") {
        detached_ptr<item> det = item::spawn("40sw", bday, item::default_charges_tag{});
        item& ammo = *det;
        dummy.i_add(std::move(det));
        REQUIRE(ammo.is_ammo());

        dummy.set_primary_weapon(item::spawn("sw_610", bday, 0));
        REQUIRE(dummy.primary_weapon().ammo_remaining() == 0);
        REQUIRE(dummy.primary_weapon().can_reload_with(ammo.type->get_id()));

        WHEN("the player triggers auto reload until the revolver is full") {
            reload_a_revolver(dummy, dummy.primary_weapon(), ammo);
            WHEN("the player triggers auto reload again") {
                avatar_action::reload_weapon(false);
                THEN("no activity is generated") { CHECK(!dummy.activity); }
            }
        }
        GIVEN("the player has another gun with ammo") {
            det = item::spawn("sw_610", bday, 0);
            item& gun2 = *det;
            dummy.i_add(std::move(det));
            REQUIRE(gun2.ammo_remaining() == 0);
            REQUIRE(gun2.can_reload_with(ammo.type->get_id()));
            WHEN("the player triggers auto reload until the first revolver is full") {
                reload_a_revolver(dummy, dummy.primary_weapon(), ammo);
                WHEN("the player triggers auto reload until the second revolver is full") {
                    reload_a_revolver(dummy, gun2, ammo);
                    WHEN("the player triggers auto reload again") {
                        avatar_action::reload_weapon(false);
                        THEN("no activity is generated") { CHECK(!dummy.activity); }
                    }
                }
            }
        }
    }

    GIVEN("a player wielding an unloaded gun, carrying an unloaded magazine, and carrying ammo for "
          "the magazine") {
        detached_ptr<item> det = item::spawn("9mm", bday, 50);
        item& ammo = *det;
        dummy.i_add(std::move(det));
        const cata::value_ptr<islot_ammo>& ammo_type = ammo.type->ammo;
        REQUIRE(ammo_type);

        det = item::spawn("glockmag", bday, 0);
        item& mag = *det;
        dummy.i_add(std::move(det));
        const cata::value_ptr<islot_magazine>& magazine_type = mag.type->magazine;
        REQUIRE(magazine_type);
        REQUIRE(magazine_type->type.count(ammo_type->type) != 0);
        REQUIRE(mag.ammo_remaining() == 0);

        dummy.set_primary_weapon(item::spawn("glock_19", bday, 0));
        REQUIRE(dummy.primary_weapon().ammo_remaining() == 0);

        WHEN("the player triggers auto reload") {
            avatar_action::reload_weapon(false);
            REQUIRE(dummy.activity);
            process_activity(dummy);

            THEN("the associated magazine is reloaded") {
                CHECK(mag.ammo_remaining() > 0);
                CHECK(mag.contents.front().type == ammo.type);
            }
            WHEN("the player triggers auto reload again") {
                avatar_action::reload_weapon(false);
                REQUIRE(dummy.activity);
                process_activity(dummy);

                THEN("The magazine is loaded into the gun") {
                    CHECK(dummy.primary_weapon().ammo_remaining() > 0);
                }
                WHEN("the player triggers auto reload again") {
                    avatar_action::reload_weapon(false);
                    THEN("No activity is generated") { CHECK(!dummy.activity); }
                }
            }
        }
        GIVEN("the player also has an extended magazine") {
            det = item::spawn("glockbigmag", bday, 0);
            item& mag2 = *det;
            dummy.i_add(std::move(det));
            const cata::value_ptr<islot_magazine>& magazine_type2 = mag2.type->magazine;
            REQUIRE(magazine_type2);
            REQUIRE(magazine_type2->type.count(ammo_type->type) != 0);
            REQUIRE(mag2.ammo_remaining() == 0);

            WHEN("the player triggers auto reload") {
                avatar_action::reload_weapon(false);
                REQUIRE(dummy.activity);
                process_activity(dummy);

                THEN("the associated magazine is reloaded") {
                    CHECK(mag.ammo_remaining() > 0);
                    CHECK(mag.contents.front().type == ammo.type);
                }
                WHEN("the player triggers auto reload again") {
                    avatar_action::reload_weapon(false);
                    REQUIRE(dummy.activity);
                    process_activity(dummy);

                    THEN("The magazine is loaded into the gun") {
                        CHECK(dummy.primary_weapon().ammo_remaining() > 0);
                    }
                    WHEN("the player triggers auto reload again") {
                        avatar_action::reload_weapon(false);
                        REQUIRE(dummy.activity);
                        process_activity(dummy);

                        THEN("the second associated magazine is reloaded") {
                            CHECK(mag2.ammo_remaining() > 0);
                            CHECK(mag2.contents.front().type == ammo.type);
                        }
                        WHEN("the player triggers auto reload again") {
                            avatar_action::reload_weapon(false);
                            THEN("No activity is generated") { CHECK(!dummy.activity); }
                        }
                    }
                }
            }
        }
    }
}

TEST_CASE("preferred_reload_source_bypasses_prompt", "[reload][gun]") {
    clear_all_state();
    auto& who = get_avatar();
    auto& preferred = who.i_add(item::spawn("38_special", calendar::start_of_cataclysm, 3));
    auto alternate_owner = item::spawn("38_speedloader", calendar::start_of_cataclysm, 0);
    alternate_owner->put_in(item::spawn("38_special", calendar::start_of_cataclysm, 6));
    auto& alternate = who.i_add(std::move(alternate_owner));
    auto& gun = who.i_add(item::spawn("sw_619", calendar::start_of_cataclysm, 0));
    REQUIRE(gun.can_reload_with(preferred.typeId()));
    REQUIRE(gun.can_reload_with(alternate.typeId()));
    who.ammo_location = &preferred;

    const auto history_key = ammotype(gun.ammo_default().str());
    const auto restore_history = restore_on_out_of_scope<std::map<ammotype, itype_id>>(
        uistate.lastreload);
    const auto previous_history = itype_id("9mm");
    uistate.lastreload[history_key] = previous_history;

    avatar_action::reload(gun, true);

    REQUIRE(who.activity);
    CHECK(who.activity->id() == activity_id("ACT_RELOAD"));
    REQUIRE(who.activity->targets.size() == 2);
    CHECK(&*who.activity->targets.front() == &gun);
    CHECK(&*who.activity->targets.back() == &preferred);
    CHECK(who.activity->index == 1);

    process_activity(who);

    CHECK_FALSE(who.activity);
    CHECK(gun.ammo_remaining() == 1);
    CHECK(gun.ammo_current() == preferred.typeId());
    CHECK(preferred.charges == 2);
    CHECK(alternate.ammo_remaining() == 6);
    CHECK(uistate.lastreload[history_key] == previous_history);
}

TEST_CASE("reload_player_handoff_accounting", "[reload]") {
    clear_all_state();
    auto& who = get_avatar();
    auto& base = who.i_add(item::spawn("combination_gun", calendar::start_of_cataclysm, 0));
    auto* target = base.gunmod_find(itype_id("combination_gun_shotgun"));
    REQUIRE(target);
    auto& ammo = who.i_add(item::spawn("shot_00", calendar::start_of_cataclysm, 6));
    const auto source = safe_reference<item>(ammo);

    const auto without_parent = item_reload_option(&who, target, target, ammo);
    const auto option = item_reload_option(&who, target, &base, ammo);
    REQUIRE(base.get_reload_time() > 0);
    REQUIRE(option.moves() - without_parent.moves() == base.get_reload_time());
    const auto expected_activity_cost = without_parent.moves() + base.get_reload_time();

    avatar_action::reload(base, false, false);

    REQUIRE(who.activity);
    CHECK(who.activity->id() == activity_id("ACT_RELOAD"));
    REQUIRE(who.activity->targets.size() == 2);
    CHECK(&*who.activity->targets[0] == target);
    CHECK(&*who.activity->targets[1] == &ammo);
    CHECK(who.activity->index == 1);
    CHECK(who.activity->moves_total == expected_activity_cost);
    CHECK(who.activity->moves_left == expected_activity_cost);
    CHECK(target->ammo_remaining() == 0);
    CHECK(ammo.charges == 6);

    process_activity(who);

    CHECK_FALSE(who.activity);
    REQUIRE(source);
    CHECK(target->ammo_remaining() == 1);
    CHECK(target->ammo_current() == ammo.typeId());
    CHECK(ammo.charges == 5);
    CHECK(base.ammo_remaining() == 0);
    CHECK(target->parent_item() == &base);
}
