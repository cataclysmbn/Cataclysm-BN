#if defined(TILES)

#    include "avatar.h"
#    include "cata_utility.h"
#    include "catch/catch.hpp"
#    include "game.h"
#    include "input_pending.h"
#    include "options_helpers.h"
#    include "player_activity.h"
#    include "sdl_wrappers.h"
#    include "state_helpers.h"
#    include "type_id.h"

TEST_CASE("redraw_is_deferred_only_for_a_queued_key_when_allowed", "[input][redraw]") {
    clear_all_state();
    const auto restore_safe_mode = restore_on_out_of_scope(g->safe_mode);
    const auto restore_uquit = restore_on_out_of_scope(g->uquit);
    const auto opt = override_option("DEFER_REDRAW_ON_QUEUED_INPUT", "true");
    const auto flush = on_out_of_scope([] { SDL_FlushEvent(SDL_EVENT_KEY_DOWN); });

    SECTION("no key queued") { CHECK_FALSE(should_defer_redraw_for_queued_input()); }

    auto key = SDL_Event{};
    key.type = SDL_EVENT_KEY_DOWN;
    REQUIRE(SDL_PushEvent(&key));

    SECTION("a key is queued") { CHECK(should_defer_redraw_for_queued_input()); }

    SECTION("option disabled") {
        const auto off = override_option("DEFER_REDRAW_ON_QUEUED_INPUT", "false");
        CHECK_FALSE(should_defer_redraw_for_queued_input());
    }

    SECTION("safe mode stopped the player") {
        g->safe_mode = SAFE_MODE_STOP;
        CHECK_FALSE(should_defer_redraw_for_queued_input());
    }

    SECTION("death cam") {
        g->uquit = QUIT_WATCH;
        CHECK_FALSE(should_defer_redraw_for_queued_input());
    }

    SECTION("activity in progress") {
        get_avatar().assign_activity(activity_id("ACT_WAIT"), 1000, 0);
        REQUIRE(get_avatar().activity);
        CHECK_FALSE(should_defer_redraw_for_queued_input());
        get_avatar().cancel_activity();
    }
}

#endif
