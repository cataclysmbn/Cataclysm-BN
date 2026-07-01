#include "calendar.h"
#include "catch/catch.hpp"
#include "morale.h"
#include "morale_types.h"
#include "npc.h"

TEST_CASE("player_morale_from_effects_consistent", "[morale][effect][slow]") {
    npc dummy;
    dummy.add_effect(efftype_id("test_high"), 1_minutes);
    const time_point start = calendar::turn_zero;
    const time_point end = start + 2_minutes;
    const time_duration tick_size = 1_turns;
    // As in game::do_turn
    for (time_point t = start; t < end; t += tick_size) {
        dummy.process_turn();
        if (to_turns<int>(t - start) % to_turns<int>(10_turns) == 0) { dummy.update_morale(); }
        if (to_turns<int>(t - start) % to_turns<int>(9_turns) == 0) {
            REQUIRE(dummy.check_and_recover_morale());
        }
    }
}
