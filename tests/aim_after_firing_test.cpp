#include "activity_actor_definitions.h"
#include "avatar.h"
#include "catch/catch.hpp"
#include "game.h"
#include "item.h"
#include "map_helpers.h"
#include "monster.h"
#include "options_helpers.h"
#include "player_activity.h"
#include "player_helpers.h"
#include "ranged/aim_until_ready.h"
#include "state_helpers.h"

#include <memory>

namespace {

constexpr auto shooter_pos = tripoint_bub_ms(60, 60, 0);
const auto act_aim = activity_id("ACT_AIM");

auto set_up_shooter() -> avatar& { // *NOPAD*
    clear_all_state();
    g->place_player(shooter_pos);
    return get_avatar();
}

/// Fires the wielded gun at `target` through aim_activity_actor::finish(), like a confirmed shot.
auto fire_through_aim_activity(avatar& you, const tripoint_bub_ms& target) -> void {
    auto actor = aim_activity_actor::use_wielded();
    auto& aim = *actor;
    aim.fin_trajectory = {target};
    auto act = player_activity(std::move(actor));
    you.recoil = 0;
    aim.finish(act, you);
}

} // namespace

/// Checks that aim mode stays after the shot target is gone while another hostile is in range.
/// A killed target leaves an empty tile, so the shot goes to an empty tile 5 tiles east.
/// A zombie 10 tiles north must keep the player in ACT_AIM.
TEST_CASE("aim_after_firing_keeps_aiming_after_kill", "[ranged][aiming]") {
    const auto opt = override_option("AIM_AFTER_FIRING", "true");
    auto& you = set_up_shooter();
    arm_character(you, "glock_19");
    spawn_test_monster("mon_zombie", shooter_pos + point(0, -10));
    set_time(calendar::turn_zero + 12_hours);

    fire_through_aim_activity(you, shooter_pos + point(5, 0));

    REQUIRE(you.activity);
    CHECK(you.activity->id() == act_aim);
}

/// Checks that aim mode uses the range of the current gun mode, not the base gun.
/// M4A1 range is 36 and M433 grenade range is 60. test_m203_10 holds 10 grenades,
/// so it is still loaded after the shot. A zombie 50 tiles east must keep ACT_AIM.
TEST_CASE("aim_after_firing_uses_underbarrel_range", "[ranged][aiming]") {
    const auto opt = override_option("AIM_AFTER_FIRING", "true");
    auto& you = set_up_shooter();
    arm_character(you, "m4a1", {"test_m203_10"});
    auto& gun = you.primary_weapon();
    REQUIRE(gun.gun_set_mode(gun_mode_id("TEST_M203_10_DEFAULT")));
    auto* launcher = gun.gun_current_mode().target;
    auto grenades = item::spawn(launcher->ammo_default(), calendar::turn, 10);
    auto& grenade_stack = *grenades;
    you.i_add(std::move(grenades));
    REQUIRE(launcher->reload(you, grenade_stack, 10));
    REQUIRE(gun.gun_range(&you) < 50);
    REQUIRE(gun.gun_current_mode().target->gun_range(&you) >= 50);
    spawn_test_monster("mon_zombie", shooter_pos + point(50, 0));
    set_time(calendar::turn_zero + 12_hours);

    fire_through_aim_activity(you, shooter_pos + point(-40, 0));

    REQUIRE(you.activity);
    CHECK(you.activity->id() == act_aim);
}

/// Checks that aiming ends after the shot when no hostile is left.
/// The shot goes to an empty tile. The next aim turn must stop with no ACT_AIM left.
TEST_CASE("aim_after_firing_stops_without_targets", "[ranged][aiming]") {
    const auto opt = override_option("AIM_AFTER_FIRING", "true");
    auto& you = set_up_shooter();
    arm_character(you, "glock_19");

    fire_through_aim_activity(you, shooter_pos + point(5, 0));
    if (you.activity) { you.activity->do_turn(you); }

    CHECK_FALSE(you.activity);
}

/// Checks that pressing fire with aim already at its cap does not spend the turn.
/// Recoil 5 is above threshold 3 but equals the aim cap 5, so more aiming cannot help.
/// aim_until_ready must keep all 100 moves for the shot.
TEST_CASE("aim_until_ready_keeps_moves_when_already_aimed", "[ranged][aiming]") {
    auto& you = set_up_shooter();
    arm_character(you, "glock_19");
    you.recoil = 5;
    you.set_moves(100);

    ranged::aim_until_ready(
        you,
        {.relevant = you.primary_weapon(), .threshold = 3, .sight_dispersion = 0, .min_recoil = 5});

    CHECK(you.get_moves() == 100);
}
