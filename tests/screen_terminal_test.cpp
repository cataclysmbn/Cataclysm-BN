#if defined(CATA_MCP)

#    include "avatar.h"
#    include "catch/catch.hpp"
#    include "client_screen_support.h"
#    include "computer.h"
#    include "computer_session.h"
#    include "game.h"
#    include "map/map.h"
#    include "map_helpers.h"
#    include "player_helpers.h"
#    include "state_helpers.h"

#    include <string>

namespace {

using namespace client_screen_test;

auto has_text(const snapshot_t& snapshot, const std::string& text) -> bool {
    return snapshot.message.find(text) != std::string::npos;
}

} // namespace

TEST_CASE(
    "a computer terminal publishes its text and a continue row at an any-key prompt",
    "[client][interaction][mcp][screens][terminal]") {
    clear_all_state();
    clear_map();
    const auto guard = screen_guard{};
    auto terminal = computer("Test Terminal", 0);
    terminal.add_option("Emergency", COMPACT_EMERG_MESS, 0);
    auto stage = 0;
    open_screen(
        [&] { computer_session(terminal).use(); },
        [&](const auto& snapshot) {
            switch (stage++) {
                case 0: {
                    CHECK(has_text(snapshot, "Logging into Test Terminal"));
                    CHECK(has_text(snapshot, "Login successful"));
                    return choose(snapshot, "continue");
                }
                case 1: {
                    // Native clears the login lines before the root menu, so none is a message.
                    CHECK_FALSE(has_text(snapshot, "Logging into"));
                    REQUIRE(row(snapshot, "entry:0") != snapshot.choices.end());
                    return choose(snapshot, "entry:0");
                }
                case 2: {
                    CHECK(snapshot.title == "Test Terminal");
                    CHECK(has_text(snapshot, "GREETINGS CITIZEN"));
                    CHECK(has_text(snapshot, "Press any key to continue"));
                    CHECK(snapshot.allow_cancel);
                    REQUIRE(snapshot.choices.size() == 1);
                    CHECK(snapshot.choices.front().id == "continue");
                    CHECK(snapshot.choices.front().pane_id == "terminal");
                    return choose(snapshot, "continue");
                }
                case 3:
                    return cancel(snapshot);
                default:
                    FAIL("unexpected extra screen " << snapshot.context);
            }
            return input_event{};
        });
    CHECK(stage == 4);
}

TEST_CASE(
    "a computer terminal offers yes no and quit for a security query and any key after a no",
    "[client][interaction][mcp][screens][terminal]") {
    clear_all_state();
    clear_map();
    const auto guard = screen_guard{};
    auto terminal = computer("Locked Terminal", 5);
    terminal.add_option("Emergency", COMPACT_EMERG_MESS, 0);
    auto stage = 0;
    open_screen(
        [&] { computer_session(terminal).use(); },
        [&](const auto& snapshot) {
            switch (stage++) {
                case 0: {
                    CHECK(snapshot.title == "Locked Terminal");
                    CHECK(has_text(snapshot, "Logging into Locked Terminal"));
                    CHECK(has_text(snapshot, "Bypass security?"));
                    CHECK(rows_with(snapshot, "") == std::vector<std::string>{"yes", "no", "quit"});
                    CHECK(row(snapshot, "yes")->pane_id == "terminal");
                    return choose(snapshot, "no");
                }
                case 1: {
                    CHECK(has_text(snapshot, "Shutting down"));
                    REQUIRE(snapshot.choices.size() == 1);
                    return choose(snapshot, "continue");
                }
                default:
                    FAIL("unexpected extra screen " << snapshot.context);
            }
            return input_event{};
        });
    CHECK(stage == 2);
}

TEST_CASE(
    "a quit answer ends the terminal session", "[client][interaction][mcp][screens][terminal]") {
    clear_all_state();
    clear_map();
    const auto guard = screen_guard{};
    auto terminal = computer("Locked Terminal", 5);
    auto stage = 0;
    open_screen(
        [&] { computer_session(terminal).use(); },
        [&](const auto& snapshot) {
            if (stage++ > 0) { FAIL("unexpected extra screen " << snapshot.context); }
            return cancel(snapshot);
        });
    CHECK(stage == 1);
}

TEST_CASE(
    "the extended description lists its targets and shows the chosen one",
    "[client][interaction][mcp][screens][terminal]") {
    clear_all_state();
    clear_map();
    auto& you = get_avatar();
    clear_character(you, false);
    const auto guard = screen_guard{};
    auto stage = 0;
    open_screen(
        [&] { g->extended_description(you.bub_pos() + tripoint_east); },
        [&](const auto& snapshot) {
            switch (stage++) {
                case 0: {
                    CHECK(snapshot.context == "EXTENDED_DESCRIPTION");
                    CHECK(rows_with(snapshot, "describe:")
                          == std::vector<std::string>{
                              "describe:creature", "describe:furniture", "describe:terrain"});
                    CHECK(row(snapshot, "describe:terrain")->selected);
                    CHECK_FALSE(row(snapshot, "describe:creature")->selected);
                    CHECK(has_text(snapshot, "This is a"));
                    CHECK(snapshot.allow_cancel);
                    return choose(snapshot, "describe:furniture");
                }
                case 1: {
                    CHECK(row(snapshot, "describe:furniture")->selected);
                    CHECK_FALSE(row(snapshot, "describe:terrain")->selected);
                    CHECK(snapshot.message == "You do not see any furniture here.");
                    return cancel(snapshot);
                }
                default:
                    FAIL("unexpected extra screen " << snapshot.context);
            }
            return input_event{};
        });
    CHECK(stage == 2);
}

#endif // CATA_MCP
