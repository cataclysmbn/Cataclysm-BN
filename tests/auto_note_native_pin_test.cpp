#if defined(CATA_MCP)

#    include "auto_note.h"
#    include "catch/catch.hpp"
#    include "client_command.h"
#    include "client_input.h"
#    include "client_interaction.h"
#    include "client_memory.h"
#    include "client_memory_scope.h"
#    include "cursesdef.h"
#    include "cursesport.h"
#    include "game_constants.h"
#    include "input.h"
#    include "mapgen/map_extras.h"
#    include "options_helpers.h"
#    include "output.h"
#    include "point.h"
#    include "state_helpers.h"

#    include <algorithm>
#    include <format>
#    include <map>
#    include <memory>
#    include <string>
#    include <vector>

/// Pins what the native auto notes manager prints, so that publishing it to the engine client
/// leaves the player's screen byte-equal: every row with its text and colour runs.
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

/// The session's auto note settings, put back after the test.
struct settings_guard {
    const auto_notes::auto_note_settings saved = get_auto_notes_settings();

    settings_guard() { get_auto_notes_settings() = auto_notes::auto_note_settings{}; }

    ~settings_guard() { get_auto_notes_settings() = saved; }
};

auto act(const std::string& action) -> input_event {
    auto input = game_client::input_command{};
    input.action = action;
    const auto result =
        game_client::resolve_input_command(input, game_client::memory::screen_size());
    REQUIRE(result.has_value());
    return *result;
}

/// What a client sends to a popup: leaves it for `cancel`, else picks the option so labelled.
auto answer(const std::string& token) -> input_event {
    const auto snapshot = game_client::current_interaction({.limit = 200});
    auto input = game_client::input_command{};
    const auto option =
        std::ranges::find(snapshot.choices, token, &game_client::interaction_choice::label);
    REQUIRE(option != snapshot.choices.end());
    input.interaction = game_client::interaction_command{
        .input_id = snapshot.input_id,
        .operation = game_client::interaction_operation::choose,
        .target_id = option->id};
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
        {"autonote:0",
         {
             R"pin(          <7>┌───────────────────────────── <9>AUTO NOTES MANAGER <7>─────────────────────────────┐)pin",
             R"pin(          <7>│                                                                              │)pin",
             R"pin(          <7>├                                                                              ┤)pin",
             R"pin(          <7>│                                                                              │)pin",
             R"pin(          <7>│ <11>>> <15:4>Crater                                         <7>│   <1>o    <7>│   <2>yes           <7>│)pin",
             R"pin(          <7>│    <15>Lost Campers                                   <7>│   <9>c    <7>│   <1>no            <7>│)pin",
             R"pin(          <7>│    <15>Helicopter Crash                               <7>│   <12>X    <7>│   <2>yes           <7>│)pin",
             R"pin(          <7>│                                                   │        │                 │)pin",
             R"pin(          <7>│                                                   │        │                 │)pin",
             R"pin(          <7>│                                                   │        │                 │)pin",
             R"pin(          <7>│                                                   │        │                 │)pin",
             R"pin(          <7>│                                                   │        │                 │)pin",
             R"pin(          <7>│                                                   │        │                 │)pin",
             R"pin(          <7>│                                                   │        │                 │)pin",
             R"pin(          <7>│                                                   │        │                 │)pin",
             R"pin(          <7>│                                                   │        │                 │)pin",
             R"pin(          <7>│                                                   │        │                 │)pin",
             R"pin(          <7>│                                                   │        │                 │)pin",
             R"pin(          <7>│                                                   │        │                 │)pin",
             R"pin(          <7>│                                                   │        │                 │)pin",
             R"pin(          <7>│                                                   │        │                 │)pin",
             R"pin(          <7>│                                                   │        │                 │)pin",
             R"pin(          <7>│                                                   │        │                 │)pin",
             R"pin(          <7>└────────────────────────────────────────────────────────────┴─────────────────┘)pin",
         }},
        {"autonote:1",
         {
             R"pin(          <7>┌───────────────────────────── <9>AUTO NOTES MANAGER <7>─────────────────────────────┐)pin",
             R"pin(          <7>│                                                                              │)pin",
             R"pin(          <7>├                                                                              ┤)pin",
             R"pin(          <7>│                                                                              │)pin",
             R"pin(          <7>│    <15>Crater                                         <7>│   <1>o    <7>│   <2>yes           <7>│)pin",
             R"pin(          <7>│ <11>>> <15:4>Lost Campers                                   <7>│   <9>c    <7>│   <1>no            <7>│)pin",
             R"pin(          <7>│    <15>Helicopter Crash                               <7>│   <12>X    <7>│   <2>yes           <7>│)pin",
             R"pin(          <7>│                                                   │        │                 │)pin",
             R"pin(          <7>│                                                   │        │                 │)pin",
             R"pin(          <7>│                                                   │        │                 │)pin",
             R"pin(          <7>│                                                   │        │                 │)pin",
             R"pin(          <7>│                                                   │        │                 │)pin",
             R"pin(          <7>│                                                   │        │                 │)pin",
             R"pin(          <7>│                                                   │        │                 │)pin",
             R"pin(          <7>│                                                   │        │                 │)pin",
             R"pin(          <7>│                                                   │        │                 │)pin",
             R"pin(          <7>│                                                   │        │                 │)pin",
             R"pin(          <7>│                                                   │        │                 │)pin",
             R"pin(          <7>│                                                   │        │                 │)pin",
             R"pin(          <7>│                                                   │        │                 │)pin",
             R"pin(          <7>│                                                   │        │                 │)pin",
             R"pin(          <7>│                                                   │        │                 │)pin",
             R"pin(          <7>│                                                   │        │                 │)pin",
             R"pin(          <7>└────────────────────────────────────────────────────────────┴─────────────────┘)pin",
         }},
        {"autonote:2",
         {
             R"pin(          <7>┌───────────────────────────── <9>AUTO NOTES MANAGER <7>─────────────────────────────┐)pin",
             R"pin(          <7>│                                                                              │)pin",
             R"pin(          <7>├                                                                              ┤)pin",
             R"pin(          <7>│                                                                              │)pin",
             R"pin(          <7>│    <15>Crater                                         <7>│   <1>o    <7>│   <2>yes           <7>│)pin",
             R"pin(          <7>│ <11>>> <15:4>Lost Campers                                   <7>│   <9>c    <7>│   <2>yes           <7>│)pin",
             R"pin(          <7>│    <15>Helicopter Crash                               <7>│   <12>X    <7>│   <2>yes           <7>│)pin",
             R"pin(          <7>│                                                   │        │                 │)pin",
             R"pin(          <7>│                                                   │        │                 │)pin",
             R"pin(          <7>│                                                   │        │                 │)pin",
             R"pin(          <7>│                                                   │        │                 │)pin",
             R"pin(          <7>│                                                   │        │                 │)pin",
             R"pin(          <7>│                                                   │        │                 │)pin",
             R"pin(          <7>│                                                   │        │                 │)pin",
             R"pin(          <7>│                                                   │        │                 │)pin",
             R"pin(          <7>│                                                   │        │                 │)pin",
             R"pin(          <7>│                                                   │        │                 │)pin",
             R"pin(          <7>│                                                   │        │                 │)pin",
             R"pin(          <7>│                                                   │        │                 │)pin",
             R"pin(          <7>│                                                   │        │                 │)pin",
             R"pin(          <7>│                                                   │        │                 │)pin",
             R"pin(          <7>│                                                   │        │                 │)pin",
             R"pin(          <7>│                                                   │        │                 │)pin",
             R"pin(          <7>└────────────────────────────────────────────────────────────┴─────────────────┘)pin",
         }},
        {"autonote:3",
         {
             R"pin(          <7>┌───────────────────────────── <9>AUTO NOTES MANAGER <7>─────────────────────────────┐)pin",
             R"pin(          <7>│                                                                              │)pin",
             R"pin(          <7>├                                                                              ┤)pin",
             R"pin(          <7>│                                                                              │)pin",
             R"pin(          <7>│    <15>Crater                                         <7>│   <1>o    <7>│   <2>yes           <7>│)pin",
             R"pin(          <7>│ <11>>> <15:4>Lost Campers                                   <7>│   <9>c    <7>│   <1>no            <7>│)pin",
             R"pin(          <7>│    <15>Helicopter Crash                               <7>│   <12>X    <7>│   <2>yes           <7>│)pin",
             R"pin(          <7>│                                                   │        │                 │)pin",
             R"pin(          <7>│                                                   │        │                 │)pin",
             R"pin(          <7>│                                                   │        │                 │)pin",
             R"pin(          <7>│                                                   │        │                 │)pin",
             R"pin(          <7>│                                                   │        │                 │)pin",
             R"pin(          <7>│                                                   │        │                 │)pin",
             R"pin(          <7>│                                                   │        │                 │)pin",
             R"pin(          <7>│                                                   │        │                 │)pin",
             R"pin(          <7>│                                                   │        │                 │)pin",
             R"pin(          <7>│                                                   │        │                 │)pin",
             R"pin(          <7>│                                                   │        │                 │)pin",
             R"pin(          <7>│                                                   │        │                 │)pin",
             R"pin(          <7>│                                                   │        │                 │)pin",
             R"pin(          <7>│                                                   │        │                 │)pin",
             R"pin(          <7>│                                                   │        │                 │)pin",
             R"pin(          <7>│                                                   │        │                 │)pin",
             R"pin(          <7>└────────────────────────────────────────────────────────────┴─────────────────┘)pin",
         }},
        {"autonote:4",
         {
             R"pin(          <7>┌───────────────────────────── <9>AUTO NOTES MANAGER <7>─────────────────────────────┐)pin",
             R"pin(          <7>│                                                                              │)pin",
             R"pin(          <7>├                                                                              ┤)pin",
             R"pin(          <7>│                                                                              │)pin",
             R"pin(          <7>│    <15>Crater                                         <7>│   <1>o    <7>│   <2>yes           <7>│)pin",
             R"pin(          <7>│ <11>>> <15:4>Lost Campers                                   <7>│   <9>c    <7>│   <2>yes           <7>│)pin",
             R"pin(          <7>│    <15>Helicopter Crash                               <7>│   <12>X    <7>│   <2>yes           <7>│)pin",
             R"pin(          <7>│                                                   │        │                 │)pin",
             R"pin(          <7>│                                                   │        │                 │)pin",
             R"pin(          <7>│                                                   │        │                 │)pin",
             R"pin(          <7>│                                                   │        │                 │)pin",
             R"pin(          <7>│                                                   │        │                 │)pin",
             R"pin(          <7>│                                                   │        │                 │)pin",
             R"pin(          <7>│                                                   │        │                 │)pin",
             R"pin(          <7>│                                                   │        │                 │)pin",
             R"pin(          <7>│                                                   │        │                 │)pin",
             R"pin(          <7>│                                                   │        │                 │)pin",
             R"pin(          <7>│                                                   │        │                 │)pin",
             R"pin(          <7>│                                                   │        │                 │)pin",
             R"pin(          <7>│                                                   │        │                 │)pin",
             R"pin(          <7>│                                                   │        │                 │)pin",
             R"pin(          <7>│                                                   │        │                 │)pin",
             R"pin(          <7>│                                                   │        │                 │)pin",
             R"pin(          <7>└────────────────────────────────────────────────────────────┴─────────────────┘)pin",
         }},
        {"autonote:5",
         {
             R"pin(          <7>┌───────────────────────────── <9>AUTO NOTES MANAGER <7>─────────────────────────────┐)pin",
             R"pin(          <7>│                                                                              │)pin",
             R"pin(          <7>├                                                                              ┤)pin",
             R"pin(          <7>│                                                                              │)pin",
             R"pin(          <7>│    <15>Crater                                         <7>│   <1>o    <7>│   <2>yes           <7>│)pin",
             R"pin(          <7>│    <15>Lost Campers                                   <7>│   <9>c    <7>│   <2>yes           <7>│)pin",
             R"pin(          <7>│ <11>>> <15:4>Helicopter Crash                               <7>│   <12>X    <7>│   <2>yes           <7>│)pin",
             R"pin(          <7>│                                                   │        │                 │)pin",
             R"pin(          <7>│                                                   │        │                 │)pin",
             R"pin(          <7>│                                                   │        │                 │)pin",
             R"pin(          <7>│                                                   │        │                 │)pin",
             R"pin(          <7>│                                                   │        │                 │)pin",
             R"pin(          <7>│                                                   │        │                 │)pin",
             R"pin(          <7>│                                                   │        │                 │)pin",
             R"pin(          <7>│                                                   │        │                 │)pin",
             R"pin(          <7>│                                                   │        │                 │)pin",
             R"pin(          <7>│                                                   │        │                 │)pin",
             R"pin(          <7>│                                                   │        │                 │)pin",
             R"pin(          <7>│                                                   │        │                 │)pin",
             R"pin(          <7>│                                                   │        │                 │)pin",
             R"pin(          <7>│                                                   │        │                 │)pin",
             R"pin(          <7>│                                                   │        │                 │)pin",
             R"pin(          <7>│                                                   │        │                 │)pin",
             R"pin(          <7>└────────────────────────────────────────────────────────────┴─────────────────┘)pin",
         }},
        {"autonote:6",
         {
             R"pin(          <7>┌───────────────────────────── <9>AUTO NOTES MANAGER <7>─────────────────────────────┐)pin",
             R"pin(          <7>│                                                                              │)pin",
             R"pin(          <7>├                                                                              ┤)pin",
             R"pin(          <7>│                                                                              │)pin",
             R"pin(          <7>│ <11>>> <15:4>Crater                                         <7>│   <1>o    <7>│   <2>yes           <7>│)pin",
             R"pin(          <7>│    <15>Lost Campers                                   <7>│   <9>c    <7>│   <2>yes           <7>│)pin",
             R"pin(          <7>│    <15>Helicopter Crash                               <7>│   <12>X    <7>│   <2>yes           <7>│)pin",
             R"pin(          <7>│                                                   │        │                 │)pin",
             R"pin(          <7>│                                                   │        │                 │)pin",
             R"pin(          <7>│                                                   │        │                 │)pin",
             R"pin(          <7>│                                                   │        │                 │)pin",
             R"pin(          <7>│                                                   │        │                 │)pin",
             R"pin(          <7>│                                                   │        │                 │)pin",
             R"pin(          <7>│                                                   │        │                 │)pin",
             R"pin(          <7>│                                                   │        │                 │)pin",
             R"pin(          <7>│                                                   │        │                 │)pin",
             R"pin(          <7>│                                                   │        │                 │)pin",
             R"pin(          <7>│                                                   │        │                 │)pin",
             R"pin(          <7>│                                                   │        │                 │)pin",
             R"pin(          <7>│                                                   │        │                 │)pin",
             R"pin(          <7>│                                                   │        │                 │)pin",
             R"pin(          <7>│                                                   │        │                 │)pin",
             R"pin(          <7>│                                                   │        │                 │)pin",
             R"pin(          <7>└────────────────────────────────────────────────────────────┴─────────────────┘)pin",
         }},
        {"autonote:7",
         {
             R"pin(          <7>┌───────────────────────────── <9>AUTO NOTES MANAGER <7>─────────────────────────────┐)pin",
             R"pin(          <7>│                                                                              │)pin",
             R"pin(          <7>├                                                                              ┤)pin",
             R"pin(          <7>│                                                                              │)pin",
             R"pin(          <7>│    <15>Crater                                         <7>│   <1>o    <7>│   <2>yes           <7>│)pin",
             R"pin(          <7>│    <15>Lost Campers                                   <7>│   <9>c    <7>│   <2>yes           <7>│)pin",
             R"pin(          <7>│ <11>>> <15:4>Helicopter Crash                               <7>│   <12>X    <7>│   <2>yes           <7>│)pin",
             R"pin(          <7>│                                                   │        │                 │)pin",
             R"pin(          <7>│                                                   │        │                 │)pin",
             R"pin(          <7>│                                                   │        │                 │)pin",
             R"pin(          <7>│                                                   │        │                 │)pin",
             R"pin(          <7>│                                                   │        │                 │)pin",
             R"pin(          <7>│                                                   │        │                 │)pin",
             R"pin(          <7>│                                                   │        │                 │)pin",
             R"pin(          <7>│                                                   │        │                 │)pin",
             R"pin(          <7>│                                                   │        │                 │)pin",
             R"pin(          <7>│                                                   │        │                 │)pin",
             R"pin(          <7>│                                                   │        │                 │)pin",
             R"pin(          <7>│                                                   │        │                 │)pin",
             R"pin(          <7>│                                                   │        │                 │)pin",
             R"pin(          <7>│                                                   │        │                 │)pin",
             R"pin(          <7>│                                                   │        │                 │)pin",
             R"pin(          <7>│                                                   │        │                 │)pin",
             R"pin(          <7>└────────────────────────────────────────────────────────────┴─────────────────┘)pin",
         }},
        {"autonote:8",
         {
             R"pin(          <7>┌───────────────────────────── <9>AUTO NOTES MANAGER <7>─────────────────────────────┐)pin",
             R"pin(          <7>│                                                                              │)pin",
             R"pin(          <7>├                                                                              ┤)pin",
             R"pin(          <7>│                                                                              │)pin",
             R"pin(          <7>│    <15>Crater                                         <7>│   <1>o    <7>│   <2>yes           <7>│)pin",
             R"pin(          <7>│    <15>Lost Campers                                   <7>│   <9>c    <7>│   <2>yes           <7>│)pin",
             R"pin(          <7>│ <11>>> <15:4>Helicopter Crash                               <7>│   <12>X    <7>│   <2>yes           <7>│)pin",
             R"pin(          <7>│                                                   │        │                 │)pin",
             R"pin(          <7>│                                                   │        │                 │)pin",
             R"pin(          <7>│                       ┌──────────────────────────────┐     │                 │)pin",
             R"pin(          <7>│                       │<9>Save changes? (Case Sensitive)<7>│     │                 │)pin",
             R"pin(          <7>│                       │                              │     │                 │)pin",
             R"pin(          <7>│                       │                   <15>(Y)es  <15:4>(N)o<7>│     │                 │)pin",
             R"pin(          <7>│                       └──────────────────────────────┘     │                 │)pin",
             R"pin(          <7>│                                                   │        │                 │)pin",
             R"pin(          <7>│                                                   │        │                 │)pin",
             R"pin(          <7>│                                                   │        │                 │)pin",
             R"pin(          <7>│                                                   │        │                 │)pin",
             R"pin(          <7>│                                                   │        │                 │)pin",
             R"pin(          <7>│                                                   │        │                 │)pin",
             R"pin(          <7>│                                                   │        │                 │)pin",
             R"pin(          <7>│                                                   │        │                 │)pin",
             R"pin(          <7>│                                                   │        │                 │)pin",
             R"pin(          <7>└────────────────────────────────────────────────────────────┴─────────────────┘)pin",
         }},
        {"autonote_empty:0",
         {
             R"pin(          <7>┌───────────────────────────── <9>AUTO NOTES MANAGER <7>─────────────────────────────┐)pin",
             R"pin(          <7>│                                                                              │)pin",
             R"pin(          <7>├                                                                              ┤)pin",
             R"pin(          <7>│                                                                              │)pin",
             R"pin(          <7>│ Discover more special encounters to populate this list     │                 │)pin",
             R"pin(          <7>│                                                   │        │                 │)pin",
             R"pin(          <7>│                                                   │        │                 │)pin",
             R"pin(          <7>│                                                   │        │                 │)pin",
             R"pin(          <7>│                                                   │        │                 │)pin",
             R"pin(          <7>│                                                   │        │                 │)pin",
             R"pin(          <7>│                                                   │        │                 │)pin",
             R"pin(          <7>│                                                   │        │                 │)pin",
             R"pin(          <7>│                                                   │        │                 │)pin",
             R"pin(          <7>│                                                   │        │                 │)pin",
             R"pin(          <7>│                                                   │        │                 │)pin",
             R"pin(          <7>│                                                   │        │                 │)pin",
             R"pin(          <7>│                                                   │        │                 │)pin",
             R"pin(          <7>│                                                   │        │                 │)pin",
             R"pin(          <7>│                                                   │        │                 │)pin",
             R"pin(          <7>│                                                   │        │                 │)pin",
             R"pin(          <7>│                                                   │        │                 │)pin",
             R"pin(          <7>│                                                   │        │                 │)pin",
             R"pin(          <7>│                                                   │        │                 │)pin",
             R"pin(          <7>└────────────────────────────────────────────────────────────┴─────────────────┘)pin",
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

/// Presses `keys` one per read, recording the screen before each as `<screen>:<n>`; a key that
/// starts with `!` answers the popup on top. The count of reads is read after the screen returns.
auto press(const std::string& screen, const std::vector<std::string>& keys)
    -> std::shared_ptr<std::size_t> {
    auto step = std::make_shared<std::size_t>();
    game_client::memory::set_input_provider([=](const int /*timeout*/) {
        REQUIRE(*step < keys.size());
        CHECK(matches(std::format("{}:{}", screen, *step), rendered()));
        const auto& key = keys[(*step)++];
        return key.starts_with('!') ? answer(key.substr(1)) : act(key);
    });
    return step;
}

/// Three discovered map extras, the second with its note off.
void discover_three() {
    auto& settings = get_auto_notes_settings();
    settings.default_initialize();
    for (const auto* const extra : {"mx_crater", "mx_collegekids", "mx_helicopter"}) {
        settings.set_discovered(string_id<map_extra>{extra});
    }
    settings.set_auto_note_status(string_id<map_extra>{"mx_collegekids"}, false);
}

} // namespace

TEST_CASE("the native auto notes manager keeps its text and colors", "[auto_note_native_pin]") {
    const auto guard = screen_guard{};
    const auto settings = settings_guard{};
    const auto option = override_option{"AUTO_NOTES", "true"};
    discover_three();
    // The cursor down and its note switched, enabled and disabled by key, the cursor wrapped to the
    // last and the first row, then the changes refused.
    const auto keys = std::vector<std::string>{
        "DOWN", "CONFIRM", "DISABLE_MAPEXTRA_NOTE", "ENABLE_MAPEXTRA_NOTE", "DOWN", "DOWN", "UP",
        "QUIT", "!No"};
    auto gui = auto_notes::auto_note_manager_gui{};
    const auto steps = press("autonote", keys);
    gui.show();
    CHECK(*steps == keys.size());
}

TEST_CASE(
    "the native auto notes manager without a discovered extra keeps its text and colors",
    "[auto_note_native_pin]") {
    const auto guard = screen_guard{};
    const auto settings = settings_guard{};
    const auto option = override_option{"AUTO_NOTES", "true"};
    get_auto_notes_settings().default_initialize();
    const auto keys = std::vector<std::string>{"QUIT"};
    auto gui = auto_notes::auto_note_manager_gui{};
    const auto steps = press("autonote_empty", keys);
    gui.show();
    CHECK(*steps == keys.size());
}

#endif // CATA_MCP
