#if defined(CATA_MCP)

#    include "avatar.h"
#    include "catch/catch.hpp"
#    include "client_screen_support.h"
#    include "game.h"
#    include "map_helpers.h"
#    include "player_helpers.h"
#    include "state_helpers.h"

using namespace client_screen_test;

// A screen that is never answered with input it can act on used to keep the test at 100% CPU for
// hours; the test passes only if the helper fails it.
TEST_CASE(
    "a screen script that never ends its screen fails instead of spinning",
    "[client][mcp][screens][!shouldfail]") {
    clear_all_state();
    clear_map();
    auto& you = get_avatar();
    clear_character(you, false);
    const auto guard = screen_guard{};
    open_stack([&] { g->extended_description(you.bub_pos() + tripoint_east); },
               [](const auto& /*snapshot*/, const auto& /*top*/) { return input_event{}; });
}

#endif // CATA_MCP
