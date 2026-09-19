#include "avatar.h"
#include "calendar.h"
#include "catch/catch.hpp"
#include "game.h"
#include "options.h"
#include "player_helpers.h"
#include "type_id.h"

// Reproduces issue #7559: predator mutations (PRED2/3/4) are supposed to
// prevent combat-skill rust, but Character::do_skill_rust only checked this
// via pred_tick = calendar::ticks_between( duration, N_hours ), which is
// only > 0 when a *single* call's duration spans a whole N-hour block
// (NPC catch-up). During real-time play, update_body() is called every
// turn with duration == 1_turns, so pred_tick was always 0 and the
// protection never engaged.
TEST_CASE("predator_prevents_combat_skill_rust_in_real_time_play", "[skill][mutation]") {
    avatar& dummy = get_avatar();
    clear_avatar();

    get_options().get_option("SKILL_RUST").setValue("vanilla");

    dummy.set_mutation(trait_id("PRED4"));

    const skill_id melee_skill("melee");
    const skill_id fabrication_skill("fabrication");

    dummy.set_skill_level(melee_skill, 8);
    dummy.set_skill_level(fabrication_skill, 8);

    // Simulate several hours of real-time play, one turn at a time,
    // exactly like the normal game loop does via update_body(), advancing
    // the game clock the same way the main loop does.
    for (int i = 0; i < to_turns<int>(6_hours); ++i) {
        calendar::turn += 1_turns;
        dummy.update_body();
    }

    UNSCOPED_INFO("melee level after 6h: " << dummy.get_skill_level(melee_skill));
    UNSCOPED_INFO("fabrication level after 6h: " << dummy.get_skill_level(fabrication_skill));

    CHECK(dummy.get_skill_level(fabrication_skill) < 8);
    CHECK(dummy.get_skill_level(melee_skill) == 8);
}
