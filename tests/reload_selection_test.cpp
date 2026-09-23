#include "avatar.h"
#include "calendar.h"
#include "cata_utility.h"
#include "catch/catch.hpp"
#include "item.h"
#include "map/map.h"
#include "map_helpers.h"
#include "npc.h"
#include "player_helpers.h"
#include "reload/reload.h"
#include "reload/reload_selection.h"
#include "reload/reload_ui.h"
#include "state_helpers.h"
#include "uistate.h"
#include "vehicle/vehicle.h"

#include <map>
#include <utility>
#include <vector>

TEST_CASE("reload_selection_supplied_options", "[reload][reload_selection]") {
    clear_all_state();
    auto& target = *item::spawn_temporary("sw_619", calendar::start_of_cataclysm, 0);
    auto& first_ammo = *item::spawn_temporary("38_special", calendar::start_of_cataclysm, 1);

    SECTION("a player must interact even when one option is supplied") {
        auto who = avatar{};
        auto options = std::vector<item_reload_option>{
            item_reload_option(&who, &target, &target, first_ammo)};

        const auto result = reload_selection::prepare(who, std::move(options));

        CHECK(result.outcome == reload_selection::selection_outcome::interaction_required);
        CHECK_FALSE(result.selected);
        REQUIRE(result.options.size() == 1);
        CHECK(result.options.front().ammo == &first_ammo);
    }

    SECTION("an NPC receives the first supplied option without sorting or rediscovery") {
        auto& who = spawn_npc({50, 50, 0}, "test_talker");
        clear_character(who);
        who.setpos(get_avatar().bub_pos());
        auto& cheap = who.i_add(item::spawn("38_special", calendar::start_of_cataclysm, 6));
        auto expensive_owner = item::spawn("38_special", calendar::start_of_cataclysm, 1);
        auto& expensive = *expensive_owner;
        get_map().add_item_or_charges(who.bub_pos() + tripoint_east, std::move(expensive_owner));
        get_map().add_item_or_charges(
            who.bub_pos(), item::spawn("38_special", calendar::start_of_cataclysm, 6));
        const auto expensive_option = item_reload_option(&who, &target, &target, expensive);
        const auto cheap_option = item_reload_option(&who, &target, &target, cheap);
        REQUIRE(expensive_option.moves() > cheap_option.moves());
        const auto restore_history = restore_on_out_of_scope<std::map<ammotype, itype_id>>(
            uistate.lastreload);
        uistate.lastreload.clear();
        const auto history = uistate.lastreload;

        const auto result = reload_selection::
            prepare(who, std::vector<item_reload_option>{expensive_option, cheap_option});

        CHECK(result.outcome == reload_selection::selection_outcome::automatic);
        CHECK(result.selected.ammo == &expensive);
        CHECK(uistate.lastreload == history);
        CHECK(result.options.empty());
    }

    SECTION("an empty supplied list retains its distinct outcome") {
        auto who = avatar{};
        const auto result = reload_selection::prepare(who, std::vector<item_reload_option>{});

        CHECK(result.outcome == reload_selection::selection_outcome::empty_supplied);
        CHECK_FALSE(result.selected);
        CHECK(result.options.empty());
    }
}

TEST_CASE("reload_single_discovered_option", "[reload][reload_selection]") {
    clear_all_state();
    auto& who = get_avatar();
    auto& ammo = who.i_add(item::spawn("38_special", calendar::start_of_cataclysm, 6));
    auto& gun = who.i_add(item::spawn("sw_619", calendar::start_of_cataclysm, 0));
    const auto starting_charges = ammo.charges;
    const auto restore_history = restore_on_out_of_scope<std::map<ammotype, itype_id>>(
        uistate.lastreload);
    uistate.lastreload.clear();
    const auto history = uistate.lastreload;

    const auto discovery = reload::discover_ammo(who, gun);
    REQUIRE(discovery.ammo_match_found);
    REQUIRE(discovery.options.size() == 1);
    CHECK(discovery.options.front().target == &gun);
    CHECK(discovery.options.front().ammo == &ammo);

    const auto selected = reload_ui::select_ammo(who, gun);
    CHECK(selected.target == &gun);
    CHECK(selected.ammo == &ammo);
    CHECK(ammo.charges == starting_charges);
    CHECK(gun.ammo_remaining() == 0);
    CHECK_FALSE(who.activity);
    CHECK(uistate.lastreload == history);
}

TEST_CASE("reload_discovery_failure_precedence", "[reload][reload_selection]") {
    clear_all_state();
    auto& who = get_avatar();

    SECTION("a detachable-magazine gun reports its missing magazine first") {
        auto& gun = *item::spawn_temporary("glock_19", calendar::start_of_cataclysm, 0);
        const auto result = reload_selection::prepare(who, gun);
        CHECK(result.outcome == reload_selection::selection_outcome::missing_magazine);
    }

    SECTION("an integral-magazine gun without ammunition reports missing ammunition") {
        auto& gun = *item::spawn_temporary("sw_619", calendar::start_of_cataclysm, 0);
        const auto result = reload_selection::prepare(who, gun);
        CHECK(result.outcome == reload_selection::selection_outcome::missing_ammunition);
    }

    SECTION("an empty holster uses the supplied-empty outcome") {
        auto& holster = *item::spawn_temporary("holster", calendar::start_of_cataclysm, 0);
        REQUIRE(holster.is_holster());
        const auto result = reload_selection::prepare(who, holster);
        CHECK(result.outcome == reload_selection::selection_outcome::empty_supplied);
        CHECK_FALSE(result.selected);
        CHECK(result.options.empty());
    }

    SECTION("matching ammunition for a full gun reports nothing to reload") {
        auto& ammo = who.i_add(item::spawn("38_special", calendar::start_of_cataclysm, 6));
        auto& gun = who.i_add(item::spawn("sw_619", calendar::start_of_cataclysm, 0));
        gun.ammo_set(ammo.typeId(), -1);
        REQUIRE(gun.ammo_remaining() == gun.ammo_capacity());

        const auto discovery = reload::discover_ammo(who, gun);
        CHECK(discovery.ammo_match_found);
        CHECK(discovery.options.empty());

        const auto result = reload_selection::prepare(who, gun);
        CHECK(result.outcome == reload_selection::selection_outcome::nothing_to_reload);

        const auto potential = reload::discover_ammo(who, gun, {.include_potential = true});
        CHECK(potential.ammo_match_found);
        REQUIRE(potential.options.size() == 1);
        CHECK(potential.options.front().ammo == &ammo);
        CHECK(potential.options.front().target == &gun);
    }
}

TEST_CASE("reload_loaded_revolver_rejects_speedloader", "[reload][reload_selection]") {
    clear_all_state();
    auto& who = get_avatar();
    auto& gun = who.i_add(item::spawn("sw_619", calendar::start_of_cataclysm, 0));
    gun.ammo_set(itype_id("38_special"), 1);

    auto speedloader_owner = item::spawn("38_speedloader", calendar::start_of_cataclysm, 0);
    speedloader_owner->put_in(
        item::spawn("38_special", calendar::start_of_cataclysm, gun.ammo_capacity()));
    auto& speedloader = who.i_add(std::move(speedloader_owner));
    REQUIRE(gun.can_reload_with(speedloader.typeId()));
    REQUIRE_FALSE(who.can_reload(gun, speedloader.typeId()));

    const auto discovery = reload::discover_ammo(who, gun);

    CHECK_FALSE(discovery.ammo_match_found);
    CHECK(discovery.options.empty());

    const auto potential = reload::discover_ammo(who, gun, {.include_potential = true});
    CHECK(potential.ammo_match_found);
    REQUIRE(potential.options.size() == 1);
    CHECK(potential.options.front().ammo == &speedloader);
    CHECK(potential.options.front().target == &gun);
}

TEST_CASE("reload_stable_ordering", "[reload][reload_selection]") {
    clear_all_state();
    auto& who = get_avatar();
    auto& target = who.i_add(item::spawn("sw_619", calendar::start_of_cataclysm, 0));

    auto more_owner = item::spawn("38_speedloader", calendar::start_of_cataclysm, 0);
    more_owner->put_in(item::spawn("38_special", calendar::start_of_cataclysm, 6));
    auto& more = who.i_add(std::move(more_owner));

    auto tie_first_owner = item::spawn("38_speedloader", calendar::start_of_cataclysm, 0);
    tie_first_owner->put_in(item::spawn("38_special", calendar::start_of_cataclysm, 3));
    auto& tie_first = who.i_add(std::move(tie_first_owner));

    auto tie_second_owner = item::spawn("38_speedloader", calendar::start_of_cataclysm, 0);
    tie_second_owner->put_in(item::spawn("38_special", calendar::start_of_cataclysm, 3));
    auto& tie_second = who.i_add(std::move(tie_second_owner));

    auto less_owner = item::spawn("38_speedloader", calendar::start_of_cataclysm, 0);
    less_owner->put_in(item::spawn("38_special", calendar::start_of_cataclysm, 1));
    auto& less = who.i_add(std::move(less_owner));

    auto expensive_owner = item::spawn("38_speedloader", calendar::start_of_cataclysm, 0);
    expensive_owner->put_in(item::spawn("38_special", calendar::start_of_cataclysm, 6));
    auto& expensive = *expensive_owner;
    get_map().add_item_or_charges(who.bub_pos() + tripoint_east, std::move(expensive_owner));
    auto& empty = who.i_add(item::spawn("38_speedloader", calendar::start_of_cataclysm, 0));

    auto more_option = item_reload_option(&who, &target, &target, more);
    auto tie_first_option = item_reload_option(&who, &target, &target, tie_first);
    auto tie_second_option = item_reload_option(&who, &target, &target, tie_second);
    auto less_option = item_reload_option(&who, &target, &target, less);
    auto expensive_option = item_reload_option(&who, &target, &target, expensive);
    auto empty_option = item_reload_option(&who, &target, &target, empty);
    more_option.qty(1);
    tie_first_option.qty(1);
    tie_second_option.qty(1);
    less_option.qty(1);
    expensive_option.qty(1);
    empty_option.qty(1);

    REQUIRE(more_option.moves() == tie_first_option.moves());
    REQUIRE(tie_first_option.moves() == tie_second_option.moves());
    REQUIRE(tie_second_option.moves() == less_option.moves());
    REQUIRE(expensive_option.moves() > more_option.moves());

    auto options = std::vector<item_reload_option>{
        tie_first_option, expensive_option, less_option,
        empty_option,     more_option,      tie_second_option};
    reload_selection::order_ammo(options);

    REQUIRE(options.size() == 6);
    CHECK(options[0].ammo == &more);
    CHECK(options[1].ammo == &tie_first);
    CHECK(options[2].ammo == &tie_second);
    CHECK(options[3].ammo == &less);
    CHECK(options[4].ammo == &expensive);
    CHECK(options[5].ammo == &empty);
}

TEST_CASE("reload_npc_empty_selection", "[reload][reload_selection]") {
    clear_all_state();
    auto& who = spawn_npc({50, 50, 0}, "test_talker");
    clear_character(who);
    who.setpos(get_avatar().bub_pos());
    const auto restore_history = restore_on_out_of_scope<std::map<ammotype, itype_id>>(
        uistate.lastreload);
    uistate.lastreload.clear();
    const auto history = uistate.lastreload;
    auto& gun = *item::spawn_temporary("sw_619", calendar::start_of_cataclysm, 0);

    const auto discovered = reload_ui::select_ammo(who, gun);
    const auto supplied = reload_ui::select_ammo(who, gun, std::vector<item_reload_option>{});

    CHECK_FALSE(discovered);
    CHECK_FALSE(supplied);
    CHECK(uistate.lastreload == history);
    CHECK_FALSE(who.activity);
    CHECK_FALSE(get_avatar().activity);
}

TEST_CASE("reload_discovery_source_order_and_mounted_range", "[reload][reload_selection]") {
    clear_all_state();
    auto& who = get_avatar();
    auto& here = get_map();
    const auto center = tripoint_bub_ms(60, 60, 0);
    who.setpos(center);
    auto& gun = who.i_add(item::spawn("sw_619", calendar::start_of_cataclysm, 0));
    auto& carried = who.i_add(item::spawn("38_special", calendar::start_of_cataclysm, 1));
    auto ground_owner = item::spawn("38_special", calendar::start_of_cataclysm, 6);
    auto& ground = *ground_owner;
    here.add_item_or_charges(center, std::move(ground_owner));
    here.add_item_or_charges(
        center + tripoint_south * 2, item::spawn("38_special", calendar::start_of_cataclysm, 6));

    auto* cart =
        here.add_vehicle(vproto_id("shopping_cart"), center + tripoint_east, 0_degrees, 0, 0);
    REQUIRE(cart != nullptr);
    const auto cargo = cart->part_with_feature(tripoint_mnt_veh::zero(), "CARGO", true);
    REQUIRE(cargo >= 0);
    cart->get_items(cargo).clear();
    auto vehicle_owner = item::spawn("38_special", calendar::start_of_cataclysm, 4);
    auto& vehicle_ammo = *vehicle_owner;
    REQUIRE_FALSE(cart->add_item(cargo, std::move(vehicle_owner)));

    const auto unmounted = reload::discover_ammo(who, gun);
    CHECK(unmounted.ammo_match_found);
    REQUIRE(unmounted.options.size() == 3);
    CHECK(unmounted.options[0].ammo == &carried);
    CHECK(unmounted.options[1].ammo == &ground);
    CHECK(unmounted.options[2].ammo == &vehicle_ammo);
    for (const auto& option : unmounted.options) { CHECK(option.target == &gun); }

    auto& horse = spawn_test_monster("mon_horse", center + tripoint_north);
    who.mount_creature(horse);
    REQUIRE(who.is_mounted());
    const auto mounted = reload::discover_ammo(who, gun);
    CHECK(mounted.ammo_match_found);
    REQUIRE(mounted.options.size() == 1);
    CHECK(mounted.options.front().ammo == &carried);
    CHECK(mounted.options.front().target == &gun);
}

TEST_CASE("reload_discovery_empty_magazine_inclusion", "[reload][reload_selection]") {
    clear_all_state();
    auto& who = get_avatar();
    auto& gun = who.i_add(item::spawn("glock_19", calendar::start_of_cataclysm, 0));
    auto& empty = who.i_add(item::spawn("glockmag", calendar::start_of_cataclysm, 0));

    const auto excluded = reload::discover_ammo(who, gun, {.include_empty_mags = false});
    CHECK_FALSE(excluded.ammo_match_found);
    CHECK(excluded.options.empty());

    const auto included = reload::discover_ammo(who, gun, {.include_empty_mags = true});
    CHECK(included.ammo_match_found);
    REQUIRE(included.options.size() == 1);
    CHECK(included.options.front().ammo == &empty);
    CHECK(included.options.front().target == &gun);

    empty.ammo_set(itype_id("9mm"), 5);
    const auto loaded = reload::discover_ammo(who, gun, {.include_empty_mags = false});
    CHECK(loaded.ammo_match_found);
    REQUIRE(loaded.options.size() == 1);
    CHECK(loaded.options.front().ammo == &empty);
    CHECK(loaded.options.front().target == &gun);
}

TEST_CASE("reload_discovery_detachable_target_order", "[reload][reload_selection]") {
    clear_all_state();
    auto& who = get_avatar();
    auto& gun = who.i_add(item::spawn("acr", calendar::start_of_cataclysm, 0));
    auto mod_owner = item::spawn("aux_flamer", calendar::start_of_cataclysm, 0);
    REQUIRE(gun.is_gunmod_compatible(*mod_owner).success());
    auto& mod = *mod_owner;
    gun.put_in(std::move(mod_owner));
    REQUIRE(mod.can_reload_with(itype_id("aux_pressurized_tank")));
    mod.put_in(item::spawn("aux_pressurized_tank", calendar::start_of_cataclysm, 0));
    REQUIRE(gun.can_reload_with(itype_id("stanag30")));
    gun.put_in(item::spawn("stanag30", calendar::start_of_cataclysm, 0));
    auto* base_magazine = gun.magazine_current();
    auto* mod_magazine = mod.magazine_current();
    REQUIRE(base_magazine != nullptr);
    REQUIRE(mod_magazine != nullptr);

    auto& spare_magazine = who.i_add(item::spawn("stanag30", calendar::start_of_cataclysm, 0));
    auto& spare_tank = who.i_add(
        item::spawn("aux_pressurized_tank", calendar::start_of_cataclysm, 0));
    auto& rifle_ammo = who.i_add(item::spawn("223", calendar::start_of_cataclysm, 5));
    auto fuel_owner = item::spawn("jerrycan", calendar::start_of_cataclysm, 0);
    fuel_owner->put_in(item::spawn("napalm", calendar::start_of_cataclysm, 100));
    auto& fuel = who.i_add(std::move(fuel_owner));
    REQUIRE(mod_magazine->can_reload_with(itype_id("napalm")));

    const auto discovery = reload::discover_ammo(who, gun);

    CHECK(discovery.ammo_match_found);
    REQUIRE(discovery.options.size() == 4);
    CHECK(discovery.options[0].target == &mod);
    CHECK(discovery.options[0].ammo == &spare_tank);
    CHECK(discovery.options[1].target == &gun);
    CHECK(discovery.options[1].ammo == &spare_magazine);
    CHECK(discovery.options[2].target == base_magazine);
    CHECK(discovery.options[2].ammo == &rifle_ammo);
    CHECK(discovery.options[3].target == mod_magazine);
    CHECK(discovery.options[3].ammo == &fuel);
}

TEST_CASE("reload_discovery_respects_magazine_parent_ammo", "[reload][reload_selection]") {
    clear_all_state();
    auto& who = get_avatar();
    auto& gun = who.i_add(item::spawn("acr", calendar::start_of_cataclysm, 0));
    auto magazine_owner = item::spawn("stanag30", calendar::start_of_cataclysm, 0);
    auto& magazine = *magazine_owner;
    auto& incompatible = who.i_add(item::spawn("300blk", calendar::start_of_cataclysm, 5));
    REQUIRE(magazine.can_reload_with(itype_id("223")));
    REQUIRE(magazine.can_reload_with(incompatible.typeId()));
    REQUIRE_FALSE(gun.ammo_types().contains(incompatible.ammo_type()));

    const auto standalone = reload::discover_ammo(who, magazine);
    CHECK(standalone.ammo_match_found);
    REQUIRE(standalone.options.size() == 1);
    CHECK(standalone.options.front().ammo == &incompatible);
    CHECK(standalone.options.front().target == &magazine);

    REQUIRE(gun.can_reload_with(magazine.typeId()));
    gun.put_in(std::move(magazine_owner));
    REQUIRE(magazine.parent_item() == &gun);
    const auto rejected = reload::discover_ammo(who, gun);
    CHECK_FALSE(rejected.ammo_match_found);
    CHECK(rejected.options.empty());

    auto& compatible = who.i_add(item::spawn("223", calendar::start_of_cataclysm, 5));
    const auto accepted = reload::discover_ammo(who, gun);
    CHECK(accepted.ammo_match_found);
    REQUIRE(accepted.options.size() == 1);
    CHECK(accepted.options.front().target == &magazine);
    CHECK(accepted.options.front().ammo == &compatible);
}

TEST_CASE("reload_nonempty_precedes_cheaper_empty", "[reload][reload_selection]") {
    clear_all_state();
    auto& who = spawn_npc({50, 50, 0}, "test_talker");
    clear_character(who);
    who.setpos(get_avatar().bub_pos());
    auto& gun = who.i_add(item::spawn("glock_19", calendar::start_of_cataclysm, 0));
    auto& empty = who.i_add(item::spawn("glockmag", calendar::start_of_cataclysm, 0));
    auto loaded_owner = item::spawn("glockmag", calendar::start_of_cataclysm, 0);
    loaded_owner->ammo_set(itype_id("9mm"), 5);
    auto& loaded = *loaded_owner;
    get_map().add_item_or_charges(who.bub_pos() + tripoint_east, std::move(loaded_owner));
    const auto restore_history = restore_on_out_of_scope<std::map<ammotype, itype_id>>(
        uistate.lastreload);
    uistate.lastreload.clear();
    uistate.lastreload[ammotype(gun.ammo_default().str())] = itype_id("38_special");
    const auto history = uistate.lastreload;

    const auto discovery = reload::discover_ammo(who, gun);
    REQUIRE(discovery.options.size() == 2);
    REQUIRE(discovery.options[0].ammo == &empty);
    REQUIRE(discovery.options[1].ammo == &loaded);
    REQUIRE(discovery.options[0].moves() < discovery.options[1].moves());

    const auto selected = reload_ui::select_ammo(who, gun);

    CHECK(selected.target == &gun);
    CHECK(selected.ammo == &loaded);
    CHECK(uistate.lastreload == history);
    CHECK(empty.ammo_remaining() == 0);
    CHECK(loaded.ammo_remaining() == 5);
    CHECK(gun.magazine_current() == nullptr);
    CHECK_FALSE(who.activity);
}
