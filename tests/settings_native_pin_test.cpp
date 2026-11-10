#if defined(CATA_MCP)

#    include "catch/catch.hpp"
#    include "client_command.h"
#    include "client_input.h"
#    include "client_memory.h"
#    include "client_memory_scope.h"
#    include "cursesdef.h"
#    include "cursesport.h"
#    include "game_constants.h"
#    include "input.h"
#    include "output.h"
#    include "state_helpers.h"

#    include <cstdlib>
#    include <format>
#    include <fstream>
#    include <map>
#    include <string>
#    include <vector>

/// Pins what the native keybinding screen prints, so that publishing them to the engine
/// client leaves the player's screens byte-equal: every row with its text and colour runs.
namespace {

struct screen_guard {
    const game_client::memory::scoped_state memory;
    bool old_test_mode = test_mode;
    int old_termx = TERMX;
    int old_termy = TERMY;
    int old_width = FULL_SCREEN_WIDTH;
    int old_height = FULL_SCREEN_HEIGHT;

    screen_guard() {
        clear_all_state();
        test_mode = false;
        TERMX = 100;
        TERMY = 40;
        FULL_SCREEN_WIDTH = 80;
        FULL_SCREEN_HEIGHT = 24;
        game_client::memory::resize(TERMX, TERMY);
        catacurses::stdscr = catacurses::newwin(TERMY, TERMX, point_zero);
        catacurses::newscr = catacurses::newwin(TERMY, TERMX, point_zero);
    }

    ~screen_guard() {
        game_client::memory::set_input_provider({});
        TERMX = old_termx;
        TERMY = old_termy;
        FULL_SCREEN_WIDTH = old_width;
        FULL_SCREEN_HEIGHT = old_height;
        test_mode = old_test_mode;
    }
};

auto act(const std::string& action) -> input_event {
    auto input = game_client::input_command{};
    input.action = action;
    const auto result =
        game_client::resolve_input_command(input, game_client::memory::screen_size());
    REQUIRE(result.has_value());
    return *result;
}

/// Typed text, as the filter box of the keybinding screen takes it.
auto typed(const std::string& text) -> input_event {
    auto input = game_client::input_command{};
    input.text = text;
    const auto result =
        game_client::resolve_input_command(input, game_client::memory::screen_size());
    REQUIRE(result.has_value());
    return *result;
}

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

/// The recorded screens, by name.
auto recorded() -> const std::map<std::string, std::vector<std::string>>& {
    static const auto screens = std::map<std::string, std::vector<std::string>>{
        {"keys:0",
         {
             R"pin(          <7>┌────────────────────────────────<9>Keybindings<7>─────────────────────────────────┐)pin",
             R"pin(          <7>│ <9>Unbound keys                                                               <7>│)pin",
             R"pin(          <7>│ <10>Keybinding active only on this screen                                      <7>│)pin",
             R"pin(          <7>│ Keybinding active globally                                                 │)pin",
             R"pin(          <7>│ <15>Press - to remove keybinding                                               <7>│)pin",
             R"pin(          <7>│ <15>Press + to add local keybinding                                            <7>│)pin",
             R"pin(          <7>│ <15>Press = to add global keybinding                                           <7>│)pin",
             R"pin(          <7>│                                                                            │)pin",
             R"pin(          <7>│   <7:4>_<7>_____________________________________________________________________   │)pin",
             R"pin(          <7>│                                                                            │)pin",
             R"pin(          <7>│   Pan up:                                         k, 8, UP or NUMPAD_8     │)pin",
             R"pin(          <7>│   Pan down:                                       j, DOWN, 2 or NUMPAD_2   │)pin",
             R"pin(          <7>│   Pan left:                                       h, LEFT, 4 or NUMPAD_4   │)pin",
             R"pin(          <7>│                                                                            │)pin",
             R"pin(          <7>│                                                                            │)pin",
             R"pin(          <7>│                                                                            │)pin",
             R"pin(          <7>│                                                                            │)pin",
             R"pin(          <7>│                                                                            │)pin",
             R"pin(          <7>│                                                                            │)pin",
             R"pin(          <7>│                                                                            │)pin",
             R"pin(          <7>│                                                                            │)pin",
             R"pin(          <7>│                                                                            │)pin",
             R"pin(          <7>│                                                                            │)pin",
             R"pin(          <7>│                                                                            │)pin",
             R"pin(          <7>│                                                                            │)pin",
             R"pin(          <7>│                                                                            │)pin",
             R"pin(          <7>│                                                                            │)pin",
             R"pin(          <7>│                                                                            │)pin",
             R"pin(          <7>│                                                                            │)pin",
             R"pin(          <7>│                                                                            │)pin",
             R"pin(          <7>│                                                                            │)pin",
             R"pin(          <7>│                                                                            │)pin",
             R"pin(          <7>│                                                                            │)pin",
             R"pin(          <7>│                                                                            │)pin",
             R"pin(          <7>│                                                                            │)pin",
             R"pin(          <7>│                                                                            │)pin",
             R"pin(          <7>│                                                                            │)pin",
             R"pin(          <7>└────────────────────────────────────────────────────────────────────────────┘)pin",
         }},
        {"keys:1",
         {
             R"pin(          <7>┌────────────────────────────────<9>Keybindings<7>─────────────────────────────────┐)pin",
             R"pin(          <7>│ <9>Unbound keys                                                               <7>│)pin",
             R"pin(          <7>│ <10>Keybinding active only on this screen                                      <7>│)pin",
             R"pin(          <7>│ Keybinding active globally                                                 │)pin",
             R"pin(          <7>│ <15>Press - to remove keybinding                                               <7>│)pin",
             R"pin(          <7>│ <15>Press + to add local keybinding                                            <7>│)pin",
             R"pin(          <7>│ <15>Press = to add global keybinding                                           <7>│)pin",
             R"pin(          <7>│                                                                            │)pin",
             R"pin(          <7>│   <5>u<7:4>_<7>____________________________________________________________________   │)pin",
             R"pin(          <7>│                                                                            │)pin",
             R"pin(          <7>│   Pan up:                                         k, 8, UP or NUMPAD_8     │)pin",
             R"pin(          <7>│                                                                            │)pin",
             R"pin(          <7>│                                                                            │)pin",
             R"pin(          <7>│                                                                            │)pin",
             R"pin(          <7>│                                                                            │)pin",
             R"pin(          <7>│                                                                            │)pin",
             R"pin(          <7>│                                                                            │)pin",
             R"pin(          <7>│                                                                            │)pin",
             R"pin(          <7>│                                                                            │)pin",
             R"pin(          <7>│                                                                            │)pin",
             R"pin(          <7>│                                                                            │)pin",
             R"pin(          <7>│                                                                            │)pin",
             R"pin(          <7>│                                                                            │)pin",
             R"pin(          <7>│                                                                            │)pin",
             R"pin(          <7>│                                                                            │)pin",
             R"pin(          <7>│                                                                            │)pin",
             R"pin(          <7>│                                                                            │)pin",
             R"pin(          <7>│                                                                            │)pin",
             R"pin(          <7>│                                                                            │)pin",
             R"pin(          <7>│                                                                            │)pin",
             R"pin(          <7>│                                                                            │)pin",
             R"pin(          <7>│                                                                            │)pin",
             R"pin(          <7>│                                                                            │)pin",
             R"pin(          <7>│                                                                            │)pin",
             R"pin(          <7>│                                                                            │)pin",
             R"pin(          <7>│                                                                            │)pin",
             R"pin(          <7>│                                                                            │)pin",
             R"pin(          <7>└────────────────────────────────────────────────────────────────────────────┘)pin",
         }},
        {"keys:2",
         {
             R"pin(          <7>┌────────────────────────────────<9>Keybindings<7>─────────────────────────────────┐)pin",
             R"pin(          <7>│ <9>Unbound keys                                                               <7>│)pin",
             R"pin(          <7>│ <10>Keybinding active only on this screen                                      <7>│)pin",
             R"pin(          <7>│ Keybinding active globally                                                 │)pin",
             R"pin(          <7>│ <15>Press - to remove keybinding                                               <7>│)pin",
             R"pin(          <7>│ <15>Press + to add local keybinding                                            <7>│)pin",
             R"pin(          <7>│ <15>Press = to add global keybinding                                           <7>│)pin",
             R"pin(          <7>│                                                                            │)pin",
             R"pin(          <7>│   <5>u<7:4>_<7>____________________________________________________________________   │)pin",
             R"pin(          <7>│                                                                            │)pin",
             R"pin(          <7>│ <12>a <7>Pan up:                                         k, 8, UP or NUMPAD_8     │)pin",
             R"pin(          <7>│                                                                            │)pin",
             R"pin(          <7>│                                                                            │)pin",
             R"pin(          <7>│                                                                            │)pin",
             R"pin(          <7>│                                                                            │)pin",
             R"pin(          <7>│                                                                            │)pin",
             R"pin(          <7>│                                                                            │)pin",
             R"pin(          <7>│                                                                            │)pin",
             R"pin(          <7>│                                                                            │)pin",
             R"pin(          <7>│                                                                            │)pin",
             R"pin(          <7>│                                                                            │)pin",
             R"pin(          <7>│                                                                            │)pin",
             R"pin(          <7>│                                                                            │)pin",
             R"pin(          <7>│                                                                            │)pin",
             R"pin(          <7>│                                                                            │)pin",
             R"pin(          <7>│                                                                            │)pin",
             R"pin(          <7>│                                                                            │)pin",
             R"pin(          <7>│                                                                            │)pin",
             R"pin(          <7>│                                                                            │)pin",
             R"pin(          <7>│                                                                            │)pin",
             R"pin(          <7>│                                                                            │)pin",
             R"pin(          <7>│                                                                            │)pin",
             R"pin(          <7>│                                                                            │)pin",
             R"pin(          <7>│                                                                            │)pin",
             R"pin(          <7>│                                                                            │)pin",
             R"pin(          <7>│                                                                            │)pin",
             R"pin(          <7>│                                                                            │)pin",
             R"pin(          <7>└────────────────────────────────────────────────────────────────────────────┘)pin",
         }},
    };
    return screens;
}

/// Whether the rows are the recorded screen `name`, naming the first row that differs.
auto matches(const std::string& name, const std::vector<std::string>& rows) -> bool {
    const auto& expected = recorded().at(name);
    INFO(name);
    CHECK(rows.size() == expected.size());
    for (auto i = std::size_t{}; i < std::min(rows.size(), expected.size()); ++i) {
        INFO("row " << i);
        CHECK(rows[i] == expected[i]);
    }
    return rows == expected;
}

} // namespace

TEST_CASE("the native keybinding screen keeps its text and colors", "[settings_native_pin]") {
    const auto guard = screen_guard{};
    auto ctxt = input_context{"KEYBINDING_TEST"};
    ctxt.register_action("UP");
    ctxt.register_action("DOWN");
    ctxt.register_action("LEFT");
    auto step = 0;
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        // The legend and the list, the list filtered, then the remove mode with its row hotkeys.
        if (step < 3) { CHECK(matches(std::format("keys:{}", step), rendered())); }
        switch (step++) {
            case 0:
                return typed("u");
            case 1:
                return act("REMOVE");
            default:
                return act("QUIT");
        }
    });
    static_cast<void>(ctxt.display_menu());
}

#endif // CATA_MCP
