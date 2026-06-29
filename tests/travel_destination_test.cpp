#include "avatar.h"
#include "cata_utility.h"
#include "catch/catch.hpp"
#include "coordinates.h"
#include "game.h"
#include "state_helpers.h"
#include "travel/travel_destination.h"

TEST_CASE("travel_destination_accepts_memorized_tiles", "[travel][map_memory]") {
    clear_all_state();

    avatar& you = get_avatar();
    you.clear_map_memory();
    const auto cleanup = on_out_of_scope([&you]() { you.clear_map_memory(); });

    const auto target = tripoint_bub_ms(0, 0, you.bub_pos().z());
    REQUIRE_FALSE(you.sees(target));

    you.memorize_symbol(bub_to_abs(target), '#');

    CHECK(avatar_knows_travel_destination(you, target));
}

TEST_CASE("travel_line_overlay_is_drawn_for_known_targets", "[travel][tiles]") {
    CHECK(should_draw_travel_line_overlay(true, true));
    CHECK_FALSE(should_draw_travel_line_overlay(true, false));
    CHECK(should_draw_travel_line_overlay(false, false));
}
