#if defined(CATA_MCP) && defined(CATA_MESSAGES_NATIVE_TEST)

#    include "avatar.h"
#    include "catch/catch.hpp"
#    include "client_screen_support.h"
#    include "client_snapshot.h"
#    include "map_helpers.h"
#    include "messages.h"
#    include "player_helpers.h"
#    include "state_helpers.h"

#    include <format>

using namespace client_screen_test;

namespace {

/// Each non-blank row as `<fg>` or `<fg:bg>` runs and text, split where the colours change.
auto rendered() -> std::vector<std::string> {
    const auto shot = game_client::memory::snapshot();
    auto rows = std::vector<std::string>{};
    for (auto y = 0; y < shot.height; ++y) {
        auto line = std::string{};
        auto colors = std::string{};
        for (auto x = 0; x < shot.width; ++x) {
            const auto& cell = shot.cells[static_cast<std::size_t>(y * shot.width + x)];
            if (cell.text == " ") {
                line += ' ';
                continue;
            }
            const auto now =
                cell.background == 0
                    ? std::format("<{}>", cell.foreground)
                    : std::format("<{}:{}>", cell.foreground, cell.background);
            if (now != colors) {
                colors = now;
                line += now;
            }
            line += cell.text;
        }
        while (!line.empty() && line.back() == ' ') { line.pop_back(); }
        if (!line.empty()) { rows.push_back(line); }
    }
    return rows;
}

/// The recorded screen.
auto recorded() -> const std::vector<std::string>& {
    static const auto screen = std::vector<std::string>{
        R"pin(          <7>┌──────────────────────────────────────────────────────────────────────────────┐)pin",
        R"pin(          <7>│          <8>┌ <15>You hear a whack! x 2                                             <7>│)pin",
        R"pin(          <7>│          <8>│ <10>Your wound closes.                                                <7>│)pin",
        R"pin(          <7>│          <8>│ <9>You are hit!                                                      <7>│)pin",
        R"pin(          <7>│  <12>0 seconds <11>The floor creaks.                                                 <7>│)pin",
        R"pin(          <7>│                                                                              │)pin",
        R"pin(          <7>│                                                                              │)pin",
        R"pin(          <7>│                                                                              │)pin",
        R"pin(          <7>│                                                                              │)pin",
        R"pin(          <7>│                                                                              │)pin",
        R"pin(          <7>│                                                                              │)pin",
        R"pin(          <7>│                                                                              │)pin",
        R"pin(          <7>│                                                                              │)pin",
        R"pin(          <7>│                                                                              │)pin",
        R"pin(          <7>│                                                                              │)pin",
        R"pin(          <7>│                                                                              │)pin",
        R"pin(          <7>│                                                                              │)pin",
        R"pin(          <7>│                                                                              │)pin",
        R"pin(          <7>│                                                                              │)pin",
        R"pin(          <7>│                                                                              │)pin",
        R"pin(          <7>│                                                                              │)pin",
        R"pin(          <7>│                                                                              │)pin",
        R"pin(          <7>│                                                                              │)pin",
        R"pin(          <7>└< f, F or / to filter, r or R to reset, w/h to adjust size, c or C to copy, x o)pin",
    };
    return screen;
}

} // namespace

TEST_CASE("the native message log keeps its text and colors", "[messages_native_pin]") {
    clear_all_state();
    clear_map();
    clear_character(get_avatar(), false);
    Messages::clear_messages();
    add_msg("You hear a whack!");
    add_msg("You hear a whack!");
    add_msg(m_good, "Your wound closes.");
    add_msg(m_bad, "You are hit!");
    add_msg(m_warning, "The floor creaks.");
    const auto guard = screen_guard{};
    auto step = 0;
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        REQUIRE(step++ == 0);
        CHECK(rendered() == recorded());
        return event_of({.action = "QUIT"});
    });
    Messages::display_messages();
    CHECK(step == 1);
}


TEST_CASE(
    "the message log lists the messages and filters them through the native filter",
    "[client][interaction][mcp][screens]") {
    clear_all_state();
    clear_map();
    auto& you = get_avatar();
    clear_character(you, false);
    Messages::clear_messages();
    add_msg("a needle in a haystack");
    add_msg("something else");
    const auto guard = screen_guard{};
    auto stage = 0;
    open_screen(
        [] { Messages::display_messages(); },
        [&](const auto& snapshot) {
            switch (stage++) {
                case 0: {
                    CHECK(snapshot.context == "MESSAGE_LOG");
                    const auto messages = rows_with(snapshot, "message:");
                    CHECK(messages.size() == 2);
                    const auto first = row(snapshot, messages.front());
                    CHECK((first->label == "something else"
                           || first->label == "a needle in a haystack"));
                    CHECK_FALSE(first->selectable);
                    CHECK(first->columns.size() == 2);
                    CHECK(row(snapshot, "action:FILTER") != snapshot.choices.end());
                    return choose(snapshot, "action:FILTER");
                }
                case 1: {
                    REQUIRE(snapshot.field);
                    return operate(snapshot, game_client::interaction_operation::fill,
                                   snapshot.field->id, "needle");
                }
                case 2: {
                    CHECK(snapshot.context == "MESSAGE_LOG");
                    const auto messages = rows_with(snapshot, "message:");
                    REQUIRE(messages.size() == 1);
                    CHECK(row(snapshot, messages.front())->label == "a needle in a haystack");
                    CHECK(snapshot.message == "Filter: needle");
                    return choose(snapshot, "action:RESET_FILTER");
                }
                case 3: {
                    CHECK(rows_with(snapshot, "message:").size() == 2);
                    CHECK(snapshot.allow_cancel);
                    return cancel(snapshot);
                }
                default:
                    FAIL("unexpected extra screen " << snapshot.context);
            }
            return input_event{};
        });
    CHECK(stage == 4);
}

#endif // CATA_MCP && CATA_MESSAGES_NATIVE_TEST
