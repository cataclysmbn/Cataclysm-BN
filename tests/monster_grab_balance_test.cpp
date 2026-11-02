#include "avatar.h"
#include "bodypart.h"
#include "calendar.h"
#include "catch/catch.hpp"
#include "creature.h"
#include "game.h"
#include "item.h"
#include "map_helpers.h"
#include "monster.h"
#include "mtype.h"
#include "options_helpers.h"
#include "player_helpers.h"
#include "point.h"
#include "state_helpers.h"
#include "type_id.h"

#include <list>
#include <memory>
#include <string>
#include <vector>

// The test cases below cover polymorphic functions related to melee hit and dodge rates
// for the Character, player, and monster classes, including:
//
// - Character::get_hit_base, monster::get_hit_base
// - Character::get_dodge_base, monster::get_dodge_base
// - player::get_dodge, monster::get_dodge

TEST_CASE("Monster losing grabbing effect", "[player][melee][grab]") {
    clear_all_state();
    avatar& dummy = g->u;
    clear_character(dummy);

    // Four nearby spots
    const auto mon_pos_abs = dummy.abs_pos() + tripoint_rel_ms::north();

    // Grabbed by a monster
    monster* zed = g->place_critter_at(mtype_id("debug_mon"), abs_to_bub(mon_pos_abs));

    REQUIRE(g->critter_at<monster>(abs_to_bub(mon_pos_abs)));

    // Get grabbed
    dummy.add_effect(efftype_id("grabbed"), 1_minutes);
    REQUIRE(dummy.has_effect(efftype_id("grabbed")));

    zed->add_effect(efftype_id("grabbing"), 1_minutes);
    REQUIRE(zed->has_effect(efftype_id("grabbing")));

    SECTION("Monster is bashed for over 10% its hp") {
        damage_type dt = DT_BASH;
        bodypart_str_id bp("torso");
        damage_instance di(dt, (zed->get_hp_max() / 5.0) + zed->get_armor_type(dt, bp));
        zed->deal_damage(nullptr, bp, di);
        REQUIRE(zed->get_hp() <= zed->get_hp_max() * 9 / 10);

        THEN("Monster is no longer grabbing") { CHECK(!zed->has_effect(efftype_id("grabbing"))); }
    }

    SECTION("Monster is burned for over 10% its hp") {
        damage_type dt = DT_HEAT;
        bodypart_str_id bp("torso");
        damage_instance di(dt, (zed->get_hp_max() / 5.0) + zed->get_armor_type(dt, bp));
        zed->deal_damage(nullptr, bp, di);
        REQUIRE(zed->get_hp() <= zed->get_hp_max() * 9 / 10);

        THEN("Monster is still grabbing") { CHECK(zed->has_effect(efftype_id("grabbing"))); }
    }

    SECTION("Monster is bashed for 0 damage") {
        damage_type dt = DT_BASH;
        damage_instance di(dt, 0);
        bodypart_str_id bp("torso");
        zed->deal_damage(nullptr, bp, di);
        REQUIRE(zed->get_hp() == zed->get_hp_max());

        THEN("Monster is still grabbing") { CHECK(zed->has_effect(efftype_id("grabbing"))); }
    }
}

TEST_CASE("Avatar drags manually grabbed monster while moving", "[player][melee][grab]") {
    clear_all_state();
    avatar& dummy = g->u;
    clear_character(dummy);

    const auto effect_grabbed = efftype_id("grabbed");
    const auto effect_grabbing = efftype_id("grabbing");
    const auto avatar_start_abs = dummy.abs_pos();
    const auto monster_start_abs = avatar_start_abs + tripoint_rel_ms::north();
    auto& zed = spawn_test_monster("debug_mon", abs_to_bub(monster_start_abs));

    dummy.add_effect(effect_grabbing, 1_days, body_part_torso);
    zed.add_effect(effect_grabbed, 1_days);

    REQUIRE(dummy.has_effect(effect_grabbing));
    REQUIRE(zed.has_effect(effect_grabbed));
    REQUIRE(g->critter_at<monster>(abs_to_bub(monster_start_abs)) == &zed);

    const auto avatar_destination_abs = avatar_start_abs + tripoint_rel_ms::east();
    REQUIRE(g->walk_move(avatar_destination_abs, false));

    CHECK(dummy.abs_pos() == avatar_destination_abs);
    CHECK(zed.abs_pos() == avatar_start_abs);
    CHECK(g->critter_at<monster>(abs_to_bub(avatar_start_abs)) == &zed);
}

TEST_CASE("Manually grabbed monster cannot walk away", "[player][melee][grab]") {
    clear_all_state();
    avatar& dummy = g->u;
    clear_character(dummy);

    const auto effect_grabbed = efftype_id("grabbed");
    const auto effect_grabbing = efftype_id("grabbing");

    const auto avatar_start_abs = dummy.abs_pos();
    const auto monster_start_abs = avatar_start_abs + tripoint_rel_ms::north();
    const auto monster_destination_abs = monster_start_abs + tripoint_rel_ms::north();
    auto& zed = spawn_test_monster("debug_mon", abs_to_bub(monster_start_abs));

    dummy.add_effect(effect_grabbing, 1_days, body_part_torso);
    zed.add_effect(effect_grabbed, 1_days);

    REQUIRE(zed.has_effect(effect_grabbed));
    CHECK_FALSE(zed.can_move_to(abs_to_bub(monster_destination_abs)));
    CHECK(zed.can_move_to(abs_to_bub(monster_start_abs)));
    CHECK_FALSE(zed.move_to(abs_to_bub(monster_destination_abs)));
    CHECK(zed.abs_pos() == monster_start_abs);
}

TEST_CASE(
    "Grabbed monsters are released when the avatar is no longer adjacent",
    "[player][melee][grab]") {
    clear_all_state();
    const auto cleanup = on_out_of_scope([] { clear_all_state(); });
    clear_map();
    auto& dummy = get_avatar();
    clear_character(dummy);

    const auto avatar_start_abs = dummy.abs_pos();
    const auto monster_start_abs = avatar_start_abs + tripoint_rel_ms::north();
    auto& zed = spawn_test_monster("debug_mon", abs_to_bub(monster_start_abs));
    dummy.add_effect(efftype_id("grabbing"), 1_days, body_part_torso);
    zed.add_effect(efftype_id("grabbed"), 1_days);
    auto remains_adjacent = false;

    SECTION("Avatar moves away") {
        g->set_player_position(avatar_start_abs + tripoint_rel_ms::south());
    }
    SECTION("Avatar moves to another z-level") {
        get_map()
            .ter_set(abs_to_bub(avatar_start_abs + tripoint_rel_ms::above()), ter_id("t_floor"));
        g->set_player_position(avatar_start_abs + tripoint_rel_ms::above());
        REQUIRE(dummy.abs_pos() == avatar_start_abs + tripoint_rel_ms::above());
    }
    SECTION("Monster is knocked away") {
        auto trajectory = std::vector<tripoint_bub_ms>{
            abs_to_bub(monster_start_abs), abs_to_bub(monster_start_abs + tripoint_rel_ms::north()),
            abs_to_bub(monster_start_abs + tripoint_rel_ms::north() + tripoint_rel_ms::north())};
        g->knockback(trajectory, 0, 0, &dummy);
        REQUIRE(zed.abs_pos()
                == monster_start_abs + tripoint_rel_ms::north() + tripoint_rel_ms::north());
    }
    SECTION("Cardinally adjacent grab persists") { remains_adjacent = true; }
    SECTION("Diagonally adjacent grab persists") {
        g->set_player_position(avatar_start_abs + tripoint_rel_ms::east());
        remains_adjacent = true;
    }

    REQUIRE(zed.has_effect(efftype_id("grabbed")));
    zed.process_turn();

    CHECK(zed.has_effect(efftype_id("grabbed")) == remains_adjacent);
}

TEST_CASE("Crowd crush does not drain breath when disabled", "[player][melee][grab]") {
    const override_option crowd_crush("CROWD_CRUSH", "false");
    clear_all_state();
    avatar& dummy = g->u;
    clear_character(dummy);

    const auto effect_grabbed = efftype_id("grabbed");
    const auto effect_grabbing = efftype_id("grabbing");
    const auto avatar_start_abs = dummy.abs_pos();

    dummy.add_effect(effect_grabbed, 1_days, body_part_torso);
    dummy.oxygen = 30;

    for (const auto& offset :
         {tripoint_rel_ms::north(), tripoint_rel_ms::south(), tripoint_rel_ms::east()}) {
        auto& zed = spawn_test_monster("debug_mon", abs_to_bub(avatar_start_abs + offset));
        zed.add_effect(effect_grabbing, 1_days);
    }

    dummy.suffer();

    CHECK(dummy.oxygen == 30);
}

TEST_CASE("Crowd crush drains breath while grabbed", "[player][melee][grab]") {
    const override_option crowd_crush("CROWD_CRUSH", "true");
    clear_all_state();
    avatar& dummy = g->u;
    clear_character(dummy);

    const auto effect_grabbed = efftype_id("grabbed");
    const auto effect_grabbing = efftype_id("grabbing");
    const auto avatar_start_abs = dummy.abs_pos();

    dummy.add_effect(effect_grabbed, 1_days, body_part_torso);
    dummy.oxygen = 30;

    for (const auto& offset :
         {tripoint_rel_ms::north(), tripoint_rel_ms::south(), tripoint_rel_ms::east()}) {
        auto& zed = spawn_test_monster("debug_mon", abs_to_bub(avatar_start_abs + offset));
        zed.add_effect(effect_grabbing, 1_days);
    }

    dummy.suffer();

    CHECK(dummy.oxygen < 30);
}
