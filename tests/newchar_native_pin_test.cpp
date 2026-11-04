#if defined(CATA_MCP)

#    include "avatar.h"
#    include "catch/catch.hpp"
#    include "client_command.h"
#    include "client_input.h"
#    include "client_memory.h"
#    include "client_memory_scope.h"
#    include "client_snapshot.h"
#    include "cursesdef.h"
#    include "cursesport.h"
#    include "game.h"
#    include "game_constants.h"
#    include "input.h"
#    include "newcharacter.h"
#    include "output.h"
#    include "rng.h"
#    include "state_helpers.h"

#    include <cstdlib>
#    include <format>
#    include <fstream>
#    include <map>
#    include <regex>
#    include <string>
#    include <vector>

/// Pins what the native character creation tabs print, so that sharing their fact gathering with
/// the engine client keeps the player's screens byte-equal: every row with its text and colour
/// runs.
namespace {

struct screen_guard {
    const game_client::memory::scoped_state memory;
    bool old_test_mode = test_mode;
    int old_termx = TERMX;
    int old_termy = TERMY;
    int old_width = FULL_SCREEN_WIDTH;
    int old_height = FULL_SCREEN_HEIGHT;

    // The state is cleared before the guard turns off test mode: clearing eats, which asks.
    screen_guard() {
        clear_all_state();
        test_mode = false;
        TERMX = 200;
        TERMY = 50;
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

/// Each non-blank row as `<fg>` or `<fg:bg>` runs and text, split where the colours change. A list
/// screen is cropped to what the list does not hold: below the header, only from column `from` on,
/// so that the pin does not follow the game data's contents.
auto rendered(const int from = 0) -> std::vector<std::string> {
    const auto shot = game_client::memory::snapshot();
    auto rows = std::vector<std::string>{};
    for (auto y = 0; y < shot.height; ++y) {
        auto line = std::string{};
        auto colors = std::string{};
        for (auto x = y >= 5 && y < shot.height - 6 ? from : 0; x < shot.width; ++x) {
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

/// The first column of a screen that the pin keeps below its header: what its list holds is the
/// game data's.
auto crop_of(const std::string& name) -> int {
    static const auto columns = std::map<std::string, int>{
        {"scenario", 50}, {"profession:first", 50}, {"profession:picked", 50},
        {"traits", 1000}, {"bionics", 1000},        {"skills", 31},
    };
    const auto found = columns.find(name);
    return found == columns.end() ? 0 : found->second;
}

/// A row on which `text` is printed over a highlight background.
auto highlights(const std::vector<std::string>& screen, const std::string& text) -> bool {
    const auto marked = std::regex("<[0-9]+:[0-9]+>" + text);
    return std::ranges::any_of(screen, [&](const std::string& row) {
        return std::regex_search(row, marked);
    });
}

/// The recorded screens, by name.
auto recorded() -> const std::map<std::string, std::vector<std::string>>& {
    static const auto screens = std::map<std::string, std::vector<std::string>>{
        {"points",
         {
             R"pin(  <7>┌──────┐ ┌────────┐ ┌──────────┐ ┌─────┐ ┌──────┐ ┌───────┐ ┌──────┐ ┌────────┐)pin",
             R"pin( <7:4><<7>│<7:4>POINTS<7>│<7:4>><7>│SCENARIO│ │PROFESSION│ │STATS│ │TRAITS│ │BIONICS│ │SKILLS│ │OVERVIEW│)pin",
             R"pin(<7>┌─┘      └─┴────────┴─┴──────────┴─┴─────┴─┴──────┴─┴───────┴─┴──────┴─┴────────┴──────────────────────────────────────────────────────────────────────────────────────────────────────────────────────┐)pin",
             R"pin(<7>│ Points left: 6+0+2=8                                                                                                                                                                                 │)pin",
             R"pin(<7>├──────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤)pin",
             R"pin(<7>│ <2:4>Multiple pools               <2>Stats, traits and skills have separate point pools.                                                                                                                     <7>│)pin",
             R"pin(<7>│ Single pool                  <2>Putting stat points into traits and skills is allowed and putting trait points into skills is allowed.                                                                  <7>│)pin",
             R"pin(<7>│ Freeform                     <2>Scenarios and professions affect skill point pool.                                                                                                                      <7>│)pin",
             R"pin(<7>│                                                                                                                                                                                                      │)pin",
             R"pin(<7>│                                                                                                                                                                                                      │)pin",
             R"pin(<7>│                                                                                                                                                                                                      │)pin",
             R"pin(<7>│                                                                                                                                                                                                      │)pin",
             R"pin(<7>│                                                                                                                                                                                                      │)pin",
             R"pin(<7>│                                                                                                                                                                                                      │)pin",
             R"pin(<7>│                                                                                                                                                                                                      │)pin",
             R"pin(<7>│                                                                                                                                                                                                      │)pin",
             R"pin(<7>│                                                                                                                                                                                                      │)pin",
             R"pin(<7>│                                                                                                                                                                                                      │)pin",
             R"pin(<7>│                                                                                                                                                                                                      │)pin",
             R"pin(<7>│                                                                                                                                                                                                      │)pin",
             R"pin(<7>│                                                                                                                                                                                                      │)pin",
             R"pin(<7>│                                                                                                                                                                                                      │)pin",
             R"pin(<7>│                                                                                                                                                                                                      │)pin",
             R"pin(<7>│                                                                                                                                                                                                      │)pin",
             R"pin(<7>│                                                                                                                                                                                                      │)pin",
             R"pin(<7>│                                                                                                                                                                                                      │)pin",
             R"pin(<7>│                                                                                                                                                                                                      │)pin",
             R"pin(<7>│                                                                                                                                                                                                      │)pin",
             R"pin(<7>│                                                                                                                                                                                                      │)pin",
             R"pin(<7>│                                                                                                                                                                                                      │)pin",
             R"pin(<7>│                                                                                                                                                                                                      │)pin",
             R"pin(<7>│                                                                                                                                                                                                      │)pin",
             R"pin(<7>│                                                                                                                                                                                                      │)pin",
             R"pin(<7>│                                                                                                                                                                                                      │)pin",
             R"pin(<7>│                                                                                                                                                                                                      │)pin",
             R"pin(<7>│                                                                                                                                                                                                      │)pin",
             R"pin(<7>│                                                                                                                                                                                                      │)pin",
             R"pin(<7>│                                                                                                                                                                                                      │)pin",
             R"pin(<7>│                                                                                                                                                                                                      │)pin",
             R"pin(<7>│                                                                                                                                                                                                      │)pin",
             R"pin(<7>│                                                                                                                                                                                                      │)pin",
             R"pin(<7>│                                                                                                                                                                                                      │)pin",
             R"pin(<7>│                                                                                                                                                                                                      │)pin",
             R"pin(<7>│                                                                                                                                                                                                      │)pin",
             R"pin(<7>│                                                                                                                                                                                                      │)pin",
             R"pin(<7>│                                                                                                                                                                                                      │)pin",
             R"pin(<7>│                                                                                                                                                                                                      │)pin",
             R"pin(<7>│                                                                                                                                                                                                      │)pin",
             R"pin(<7>│                                                                                                                                                                                                      │)pin",
             R"pin(<7>└──────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────┘)pin",
         }},
        {"scenario",
         {
             R"pin(  <7>┌──────┐ ┌────────┐ ┌──────────┐ ┌─────┐ ┌──────┐ ┌───────┐ ┌──────┐ ┌────────┐)pin",
             R"pin(  <7>│POINTS│<7:4><<7>│<7:4>SCENARIO<7>│<7:4>><7>│PROFESSION│ │STATS│ │TRAITS│ │BIONICS│ │SKILLS│ │OVERVIEW│)pin",
             R"pin(<7>┌─┴──────┴─┘        └─┴──────────┴─┴─────┴─┴──────┴─┴───────┴─┴──────┴─┴────────┴──────────────────────────────────────────────────────────────────────────────────────────────────────────────────────┐)pin",
             R"pin(<7>│ Points left: 6+0+2=8       <2>Scenario Evacuee cost 0 points                                                                                                                                            <7>│)pin",
             R"pin(<7>├──────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤)pin",
             R"pin(                                                  <15>Sort by: <7>points (Press <10>s <7>to change sorting.)                                                       │)pin",
             R"pin(                                                                                                                                                     <7>│)pin",
             R"pin(                                                  <15>Professions:                                                                                       <7>│)pin",
             R"pin(                                                  <7>All, default:                                                                                      │)pin",
             R"pin(                                                  <7>Survivor                                                                                           │)pin",
             R"pin(                                                                                                                                                     <7>│)pin",
             R"pin(                                                  <15>Scenario Location:                                                                                 <7>│)pin",
             R"pin(                                                  <7>Evac Shelter (2 locations, 6 variants)                                                             │)pin",
             R"pin(                                                                                                                                                     <7>│)pin",
             R"pin(                                                  <15>Scenario Vehicle:                                                                                  <7>│)pin",
             R"pin(                                                                                                                                                     <7>│)pin",
             R"pin(                                                                                                                                                     <7>│)pin",
             R"pin(                                                  <15>Scenario Flags:                                                                                    <7>│)pin",
             R"pin(                                                                                                                                                     <7>│)pin",
             R"pin(                                                                                                                                                     <7>│)pin",
             R"pin(                                                                                                                                                     <7>│)pin",
             R"pin(                                                                                                                                                     <7>│)pin",
             R"pin(                                                                                                                                                     <7>│)pin",
             R"pin(                                                                                                                                                     <7>│)pin",
             R"pin(                                                                                                                                                     <7>│)pin",
             R"pin(                                                                                                                                                     <7>│)pin",
             R"pin(                                                                                                                                                     <7>│)pin",
             R"pin(                                                                                                                                                     <7>│)pin",
             R"pin(                                                                                                                                                     <7>│)pin",
             R"pin(                                                                                                                                                     <7>│)pin",
             R"pin(                                                                                                                                                     <7>│)pin",
             R"pin(                                                                                                                                                     <7>│)pin",
             R"pin(                                                                                                                                                     <7>│)pin",
             R"pin(                                                                                                                                                     <7>│)pin",
             R"pin(                                                                                                                                                     <7>│)pin",
             R"pin(                                                                                                                                                     <7>│)pin",
             R"pin(                                                                                                                                                     <7>│)pin",
             R"pin(                                                                                                                                                     <7>│)pin",
             R"pin(                                                                                                                                                     <7>│)pin",
             R"pin(                                                                                                                                                     <7>│)pin",
             R"pin(                                                                                                                                                     <7>│)pin",
             R"pin(                                                                                                                                                     <7>│)pin",
             R"pin(                                                                                                                                                     <7>│)pin",
             R"pin(                                                                                                                                                     <7>│)pin",
             R"pin(<7>│ CVN Sailor                                                                                                                                                                                           │)pin",
             R"pin(<7>│<2>You have survived the initial wave of panic, and have achieved (relative) safety in one of the many government evac shelters.                                                                         <7>│)pin",
             R"pin(<7>│                                                                                                                                                                                                      │)pin",
             R"pin(<7>│                                                                                                                                                                                                      │)pin",
             R"pin(<7>│                                                                                                                                                                                                      │)pin",
             R"pin(<7>└─<no filter>──────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────┘)pin",
         }},
        {"profession:first",
         {
             R"pin(  <7>┌──────┐ ┌────────┐ ┌──────────┐ ┌─────┐ ┌──────┐ ┌───────┐ ┌──────┐ ┌────────┐)pin",
             R"pin(  <7>│POINTS│ │SCENARIO│<7:4><<7>│<7:4>PROFESSION<7>│<7:4>><7>│STATS│ │TRAITS│ │BIONICS│ │SKILLS│ │OVERVIEW│)pin",
             R"pin(<7>┌─┴──────┴─┴────────┴─┘          └─┴─────┴─┴──────┴─┴───────┴─┴──────┴─┴────────┴──────────────────────────────────────────────────────────────────────────────────────────────────────────────────────┐)pin",
             R"pin(<7>│ Points left: 6+0+2=8       <2>Profession Survivor costs 0 points                                                                                                                                        <7>│)pin",
             R"pin(<7>├──────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤)pin",
             R"pin(                                                  <15>Sort by: <7>points (Press <10>s <7>to change sorting.)                                                       │)pin",
             R"pin(                                                  <7>Press <10>@ <7>to switch to <5>Survivor <7>(<13>female<7>).                                                            │)pin",
             R"pin(                                                  <12>Traits:                                                                                            <7>│)pin",
             R"pin(                                                  <7>None                                                                                               │)pin",
             R"pin(                                                  <12>Skills:                                                                                            <7>│)pin",
             R"pin(                                                  <7>None                                                                                               │)pin",
             R"pin(                                                  <12>Items:                                                                                             <7>│)pin",
             R"pin(                                                  <6>Wielded:                                                                                           <7>│)pin",
             R"pin(                                                  <7>None                                                                                               │)pin",
             R"pin(                                                  <6>Worn:                                                                                              <7>│)pin",
             R"pin(                                                  <10>|| <7>boxer shorts                                                                                    │)pin",
             R"pin(                                                  <10>|| <7>pair of socks                                                                                   │)pin",
             R"pin(                                                  <10>|| <7>jeans                                                                                           │)pin",
             R"pin(                                                  <10>|| <7>long-sleeved shirt                                                                              │)pin",
             R"pin(                                                  <10>|| <7>pair of sneakers                                                                                │)pin",
             R"pin(                                                  <10>|| <7>messenger bag                                                                                   │)pin",
             R"pin(                                                  <10>|| <7>wrist watch                                                                                     │)pin",
             R"pin(                                                  <6>Inventory:                                                                                         <7>│)pin",
             R"pin(                                                  <7>lighter <2>(25/25)                                                                                    <7>│)pin",
             R"pin(                                                  <7>pocket knife                                                                                       │)pin",
             R"pin(                                                  <7>clean water (plastic bottle) (2/2)                                                                 │)pin",
             R"pin(                                                  <7>smartphone <10>(160/200)                                                                               <7>│)pin",
             R"pin(                                                  <12>Bionics:                                                                                           <7>│)pin",
             R"pin(                                                  <7>None                                                                                               │)pin",
             R"pin(                                                                                                                                                     <7>│)pin",
             R"pin(                                                                                                                                                     <7>│)pin",
             R"pin(                                                                                                                                                     <7>│)pin",
             R"pin(                                                                                                                                                     <7>│)pin",
             R"pin(                                                                                                                                                     <7>│)pin",
             R"pin(                                                                                                                                                     <7>│)pin",
             R"pin(                                                                                                                                                     <7>│)pin",
             R"pin(                                                                                                                                                     <7>│)pin",
             R"pin(                                                                                                                                                     <7>│)pin",
             R"pin(                                                                                                                                                     <7>│)pin",
             R"pin(                                                                                                                                                     <7>│)pin",
             R"pin(                                                                                                                                                     <7>│)pin",
             R"pin(                                                                                                                                                     <7>│)pin",
             R"pin(                                                                                                                                                     <7>│)pin",
             R"pin(                                                                                                                                                     <7>│)pin",
             R"pin(<10>v <7>Musician                                                                                                                                                                                             │)pin",
             R"pin(<7>│<2>Some would say that there's nothing particularly notable about you.  But you've survived, and that's more than most could say right now.                                                              <7>│)pin",
             R"pin(<7>│                                                                                                                                                                                                      │)pin",
             R"pin(<7>│                                                                                                                                                                                                      │)pin",
             R"pin(<7>│                                                                                                                                                                                                      │)pin",
             R"pin(<7>└─<no filter>──────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────┘)pin",
         }},
        {"profession:picked",
         {
             R"pin(  <7>┌──────┐ ┌────────┐ ┌──────────┐ ┌─────┐ ┌──────┐ ┌───────┐ ┌──────┐ ┌────────┐)pin",
             R"pin(  <7>│POINTS│ │SCENARIO│<7:4><<7>│<7:4>PROFESSION<7>│<7:4>><7>│STATS│ │TRAITS│ │BIONICS│ │SKILLS│ │OVERVIEW│)pin",
             R"pin(<7>┌─┴──────┴─┴────────┴─┘          └─┴─────┴─┴──────┴─┴───────┴─┴──────┴─┴────────┴──────────────────────────────────────────────────────────────────────────────────────────────────────────────────────┐)pin",
             R"pin(<7>│ Points left: 6+0-4=2       <2>Profession Bionic Survivalist costs 6 points                                                                                                                              <7>│)pin",
             R"pin(<7>├──────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤)pin",
             R"pin(                                                  <15>Sort by: <7>points (Press <10>s <7>to change sorting.)                                                       │)pin",
             R"pin(                                                  <7>Press <10>@ <7>to switch to <5>Bionic Survivalist <7>(<13>female<7>).                                                  │)pin",
             R"pin(                                                  <12>Traits:                                                                                            <7>│)pin",
             R"pin(                                                  <7>None                                                                                               │)pin",
             R"pin(                                                  <12>Skills:                                                                                            <7>│)pin",
             R"pin(                                                  <11>Ranged skills                                                                                      <7>│)pin",
             R"pin(                                                    <7>Marksmanship (2)                                                                                 │)pin",
             R"pin(                                                    <7>Rifles (2)                                                                                       │)pin",
             R"pin(                                                  <11>Crafting skills                                                                                    <7>│)pin",
             R"pin(                                                    <7>Cooking (3)                                                                                      │)pin",
             R"pin(                                                    <7>Fabrication (5)                                                                                  │)pin",
             R"pin(                                                    <7>Survival (6)                                                                                     │)pin",
             R"pin(                                                  <11>Interaction skills                                                                                 <7>│)pin",
             R"pin(                                                    <7>Athletics (2)                                                                                    │)pin",
             R"pin(                                                    <7>First Aid (3)                                                                                    │)pin",
             R"pin(                                                    <7>Trapping (5)                                                                                     │)pin",
             R"pin(                                                  <12>Items:                                                                                             <7>│)pin",
             R"pin(                                                  <6>Wielded:                                                                                           <7>│)pin",
             R"pin(                                                  <7>None                                                                                               │)pin",
             R"pin(                                                  <6>Worn:                                                                                              <7>│)pin",
             R"pin(                                                  <10>|| <7>boxer shorts                                                                                    │)pin",
             R"pin(                                                  <10>|| <7>pair of socks                                                                                   │)pin",
             R"pin(                                                  <10>|| <7>boonie hat                                                                                      │)pin",
             R"pin(                                                  <10>|| <7>t-shirt                                                                                         │)pin",
             R"pin(                                                  <10>|| <7>pair of fingerless leather gloves                                                               │)pin",
             R"pin(                                                  <10>|| <7>cargo pants                                                                                     │)pin",
             R"pin(                                                  <10>|| <7>pair of boots                                                                                   │)pin",
             R"pin(                                                  <10>|| <7>aluminum broadhead bolt (quiver) (19)                                                           │)pin",
             R"pin(                                                  <10>|| <7>machete (scabbard)                                                                              │)pin",
             R"pin(                                                  <10>|| <7>plastic canteen                                                                                 │)pin",
             R"pin(                                                  <10>|| <7>trenchcoat                                                                                      │)pin",
             R"pin(                                                  <10>|| <7>crossbow+1 <2>(1/1 aluminum broadhead bolt)                                                        <7>│)pin",
             R"pin(                                                  <10>|| <7>backpack                                                                                        │)pin",
             R"pin(                                                  <10>|| <7>diver's watch                                                                                   │)pin",
             R"pin(                                                  <10>|| <7>pair of binoculars                                                                              │)pin",
             R"pin(                                                  <6>Inventory:                                                                                         <7>│)pin",
             R"pin(                                                  <7>smartphone <10>(160/200)                                                                               <7>│)pin",
             R"pin(                                                  <12>Bionics:                                                                                           <7>│)pin",
             R"pin(                                                  <7>Metabolic Interchange (toggled)                                                                    │)pin",
             R"pin(<10>v <7>Bionic Sniper                                                                                     <15>Press <10>h, LEFT, 4 or NUMPAD_4 <15>or <10>l, RIGHT, 6 or NUMPAD_6                                            <7>│)pin",
             R"pin(<7>│<2>You knew the end was coming.  You augmented yourself with some basic bionics and got additional survival training.  Now the end has come, and it is time to see if your efforts have paid off.        <7>│)pin",
             R"pin(<7>│                                                                                                                                                                                                      │)pin",
             R"pin(<7>│                                                                                                                                                                                                      │)pin",
             R"pin(<7>│                                                                                                                                                                                                      │)pin",
             R"pin(<7>└─<no filter>──────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────┘)pin",
         }},
        {"stats",
         {
             R"pin(  <7>┌──────┐ ┌────────┐ ┌──────────┐ ┌─────┐ ┌──────┐ ┌───────┐ ┌──────┐ ┌────────┐)pin",
             R"pin(  <7>│POINTS│ │SCENARIO│ │PROFESSION│<7:4><<7>│<7:4>STATS<7>│<7:4>><7>│TRAITS│ │BIONICS│ │SKILLS│ │OVERVIEW│)pin",
             R"pin(<7>┌─┴──────┴─┴────────┴─┴──────────┴─┘     └─┴──────┴─┴───────┴─┴──────┴─┴────────┴──────────────────────────────────────────────────────────────────────────────────────────────────────────────────────┐)pin",
             R"pin(<7>│ Points left: 6+0-4=2                                                                                                                                                                                 │)pin",
             R"pin(<7>├──────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤)pin",
             R"pin(<7>│                                                                                                                                                                                                      │)pin",
             R"pin(<7>│ <15>Strength:      8           Base HP: 84                                                                                                                                                               <7>│)pin",
             R"pin(<7>│ Dexterity:     8           <15>Carry weight: 9223372036854.8 kg                                                                                                                                          <7>│)pin",
             R"pin(<7>│ Intelligence:  8           <10>Melee damage bonus: +6.0                                                                                                                                                  <7>│)pin",
             R"pin(<7>│ Perception:    8                                                                                                                                                                                     │)pin",
             R"pin(<7>│                            <15>Strength also makes you more resistant to many diseases and poisons, and makes actions which require brute force more effective.                                          <7>│)pin",
             R"pin(<7>│                                                                                                                                                                                                      │)pin",
             R"pin(<7>│                                                                                                                                                                                                      │)pin",
             R"pin(<7>│                                                                                                                                                                                                      │)pin",
             R"pin(<7>│                                                                                                                                                                                                      │)pin",
             R"pin(<7>│                                                                                                                                                                                                      │)pin",
             R"pin(<7>│     <10>k, 8, UP or NUMPAD_8 <7>/ <10>j, DOWN, 2 or NUMPAD_2 <7>to select a statistic.                                                                                                                             │)pin",
             R"pin(<7>│     <10>l, RIGHT, 6 or NUMPAD_6 <7>to increase the statistic.                                                                                                                                               │)pin",
             R"pin(<7>│     <10>h, LEFT, 4 or NUMPAD_4 <7>to decrease the statistic.                                                                                                                                                │)pin",
             R"pin(<7>│                                                                                                                                                                                                      │)pin",
             R"pin(<7>│     <10>? <7>lets you view and alter keybindings.                                                                                                                                                           │)pin",
             R"pin(<7>│     <10>TAB <7>takes you to the next tab.                                                                                                                                                                   │)pin",
             R"pin(<7>│     <10>BACKTAB <7>returns you to the main menu.                                                                                                                                                            │)pin",
             R"pin(<7>│                                                                                                                                                                                                      │)pin",
             R"pin(<7>│                                                                                                                                                                                                      │)pin",
             R"pin(<7>│                                                                                                                                                                                                      │)pin",
             R"pin(<7>│                                                                                                                                                                                                      │)pin",
             R"pin(<7>│                                                                                                                                                                                                      │)pin",
             R"pin(<7>│                                                                                                                                                                                                      │)pin",
             R"pin(<7>│                                                                                                                                                                                                      │)pin",
             R"pin(<7>│                                                                                                                                                                                                      │)pin",
             R"pin(<7>│                                                                                                                                                                                                      │)pin",
             R"pin(<7>│                                                                                                                                                                                                      │)pin",
             R"pin(<7>│                                                                                                                                                                                                      │)pin",
             R"pin(<7>│                                                                                                                                                                                                      │)pin",
             R"pin(<7>│                                                                                                                                                                                                      │)pin",
             R"pin(<7>│                                                                                                                                                                                                      │)pin",
             R"pin(<7>│                                                                                                                                                                                                      │)pin",
             R"pin(<7>│                                                                                                                                                                                                      │)pin",
             R"pin(<7>│                                                                                                                                                                                                      │)pin",
             R"pin(<7>│                                                                                                                                                                                                      │)pin",
             R"pin(<7>│                                                                                                                                                                                                      │)pin",
             R"pin(<7>│                                                                                                                                                                                                      │)pin",
             R"pin(<7>│                                                                                                                                                                                                      │)pin",
             R"pin(<7>│                                                                                                                                                                                                      │)pin",
             R"pin(<7>│                                                                                                                                                                                                      │)pin",
             R"pin(<7>│                                                                                                                                                                                                      │)pin",
             R"pin(<7>│                                                                                                                                                                                                      │)pin",
             R"pin(<7>│                                                                                                                                                                                                      │)pin",
             R"pin(<7>└──────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────┘)pin",
         }},
        {"traits",
         {
             R"pin(  <7>┌──────┐ ┌────────┐ ┌──────────┐ ┌─────┐ ┌──────┐ ┌───────┐ ┌──────┐ ┌────────┐)pin",
             R"pin(  <7>│POINTS│ │SCENARIO│ │PROFESSION│ │STATS│<7:4><<7>│<7:4>TRAITS<7>│<7:4>><7>│BIONICS│ │SKILLS│ │OVERVIEW│)pin",
             R"pin(<7>┌─┴──────┴─┴────────┴─┴──────────┴─┴─────┴─┘      └─┴───────┴─┴──────┴─┴────────┴──────────────────────────────────────────────────────────────────────────────────────────────────────────────────────┐)pin",
             R"pin(<7>│ Points left: 6+0-4=2  <10>0/12   <9>0/-12   <2>Accomplished Sleeper costs 1 point                                                                                                                              <7>│)pin",
             R"pin(<7>├──────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤)pin",
             R"pin(<15>│ <7>Psychopath                          <15>│ <8>Meat Intolerance                    <15>│ <7>Hair color: gray                                                                                                         │)pin",
             R"pin(<10>v <7>Quick                               <10>v <8>Mood Swings                         <10>v <7>Hair color: light brown                                                                                                  │)pin",
             R"pin(<7>│<2>You have always been able to fall asleep easily, even when sleeping in less than ideal circumstances.                                                                                                 <7>│)pin",
             R"pin(<7>│                                                                                                                                                                                                      │)pin",
             R"pin(<7>│                                                                                                                                                                                                      │)pin",
             R"pin(<7>└──────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────┘)pin",
         }},
        {"bionics",
         {
             R"pin(  <7>┌──────┐ ┌────────┐ ┌──────────┐ ┌─────┐ ┌──────┐ ┌───────┐ ┌──────┐ ┌────────┐)pin",
             R"pin(  <7>│POINTS│ │SCENARIO│ │PROFESSION│ │STATS│ │TRAITS│<7:4><<7>│<7:4>BIONICS<7>│<7:4>><7>│SKILLS│ │OVERVIEW│)pin",
             R"pin(<7>┌─┴──────┴─┴────────┴─┴──────────┴─┴─────┴─┴──────┴─┘       └─┴──────┴─┴────────┴──────────────────────────────────────────────────────────────────────────────────────────────────────────────────────┐)pin",
             R"pin(<7>│ Points left: 6+0-4=2 <10>99/12   <9>0/-12   <2>Aero-Evaporator costs 2 points                                                                                                                                  <7>│)pin",
             R"pin(<7>├──────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤)pin",
             R"pin(<7>│                                     │                                     │                                                                                                                          │)pin",
             R"pin(<7>│                                     │                                     │                                                                                                                          │)pin",
             R"pin(<7>│<2>This unit draws moisture from the surrounding air, which slowly trickles directly into your blood stream.  It may fail in very dry environments.                                                      <7>│)pin",
             R"pin(<7>│                                                                                                                                                                                                      │)pin",
             R"pin(<7>│                                                                                                                                                                                                      │)pin",
             R"pin(<7>└──────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────┘)pin",
         }},
        {"skills",
         {
             R"pin(  <7>┌──────┐ ┌────────┐ ┌──────────┐ ┌─────┐ ┌──────┐ ┌───────┐ ┌──────┐ ┌────────┐)pin",
             R"pin(  <7>│POINTS│ │SCENARIO│ │PROFESSION│ │STATS│ │TRAITS│ │BIONICS│<7:4><<7>│<7:4>SKILLS<7>│<7:4>><7>│OVERVIEW│)pin",
             R"pin(<7>┌─┴──────┴─┴────────┴─┴──────────┴─┴─────┴─┴──────┴─┴───────┴─┘      └─┴────────┴──────────────────────────────────────────────────────────────────────────────────────────────────────────────────────┐)pin",
             R"pin(<7>│ Points left: 6+0-4=2       <2>Upgrading Bashing Weapons by 2 levels costs 1 point                                                                                                                       <7>│)pin",
             R"pin(<7>├──────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤)pin",
             R"pin(<2>Your skill in fighting with blunt weaponry, from rocks and sticks to baseball bats and the butts of rifles.  Skill increases damage, and higher levels will improve     <7>│)pin",
             R"pin(<2>the accuracy of an attack.                                                                                                                                              <7>│)pin",
             R"pin(                                                                                                                                                                        <7>│)pin",
             R"pin(                                                                                                                                                                        <7>│)pin",
             R"pin(                                                                                                                                                                        <7>│)pin",
             R"pin(                                                                                                                                                                        <7>│)pin",
             R"pin(                                                                                                                                                                        <7>│)pin",
             R"pin(                                                                                                                                                                        <7>│)pin",
             R"pin(                                                                                                                                                                        <7>│)pin",
             R"pin(                                                                                                                                                                        <7>│)pin",
             R"pin(                                                                                                                                                                        <7>│)pin",
             R"pin(                                                                                                                                                                        <7>│)pin",
             R"pin(                                                                                                                                                                        <7>│)pin",
             R"pin(                                                                                                                                                                        <7>│)pin",
             R"pin(                                                                                                                                                                        <7>│)pin",
             R"pin(                                                                                                                                                                        <7>│)pin",
             R"pin(                                                                                                                                                                        <7>│)pin",
             R"pin(                                                                                                                                                                        <7>│)pin",
             R"pin(                                                                                                                                                                        <7>│)pin",
             R"pin(                                                                                                                                                                        <7>│)pin",
             R"pin(                                                                                                                                                                        <7>│)pin",
             R"pin(                                                                                                                                                                        <7>│)pin",
             R"pin(                                                                                                                                                                        <7>│)pin",
             R"pin(                                                                                                                                                                        <7>│)pin",
             R"pin(                                                                                                                                                                        <7>│)pin",
             R"pin(                                                                                                                                                                        <7>│)pin",
             R"pin(                                                                                                                                                                        <7>│)pin",
             R"pin(                                                                                                                                                                        <7>│)pin",
             R"pin(                                                                                                                                                                        <7>│)pin",
             R"pin(                                                                                                                                                                        <7>│)pin",
             R"pin(                                                                                                                                                                        <7>│)pin",
             R"pin(                                                                                                                                                                        <7>│)pin",
             R"pin(                                                                                                                                                                        <7>│)pin",
             R"pin(                                                                                                                                                                        <7>│)pin",
             R"pin(                                                                                                                                                                        <7>│)pin",
             R"pin(                                                                                                                                                                        <7>│)pin",
             R"pin(                                                                                                                                                                        <7>│)pin",
             R"pin(                                                                                                                                                                        <7>│)pin",
             R"pin(                                                                                                                                                                        <7>│)pin",
             R"pin(<7>│                                                                                                                                                                                                      │)pin",
             R"pin(<7>│                                                                                                                                                                                                      │)pin",
             R"pin(<7>│                                                                                                                                                                                                      │)pin",
             R"pin(<7>│                                                                                                                                                                                                      │)pin",
             R"pin(<7>│                                                                                                                                                                                                      │)pin",
             R"pin(<7>└──────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────┘)pin",
         }},
        {"overview",
         {
             R"pin(  <7>┌──────┐ ┌────────┐ ┌──────────┐ ┌─────┐ ┌──────┐ ┌───────┐ ┌──────┐ ┌────────┐)pin",
             R"pin(  <7>│POINTS│ │SCENARIO│ │PROFESSION│ │STATS│ │TRAITS│ │BIONICS│ │SKILLS│<7:4><<7>│<7:4>OVERVIEW<7>│<7:4>>)pin",
             R"pin(<7>┌─┴──────┴─┴────────┴─┴──────────┴─┴─────┴─┴──────┴─┴───────┴─┴──────┴─┘        └──────────────────────────────────────────────────────────────────────────────────────────────────────────────────────┐)pin",
             R"pin(<7>│ Points left: 6+0-4=2                                                                                                                                                                                 │)pin",
             R"pin(<7>├──────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤)pin",
             R"pin(<7>│ <7:4>Name: <15>Ada                                   <7>Gender: <14>Male  <7>Female              Height: <15>175 cm      <7>Starting location: <1>* Random location * (6 variants)         <15>Scenario: <7>Evacuee                      │)pin",
             R"pin(<7>│ Press <10>RETURN <7>to edit.                       Press <10>@ <7>to switch gender          Age: <15>52             <7>Press <10>/ <7>to select location.                                 <15>Profession: <7>Bionic Survivalist         │)pin",
             R"pin(<7>│ Press <10>* <7>to randomize description.                                                                                                                                                                    │)pin",
             R"pin(<7>├──────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤)pin",
             R"pin(<7>│ <15>Stats:                    Traits:                        Bionics:                       Vehicle: <9>None!                 <15>Items:                                                                        <7>│)pin",
             R"pin(<7>│  Strength:    8            <11>Eye color: hazel               <15>Aero-Evaporator               Companions: <9>None!               <11>Wielded: <9>None!                                                               <7>│)pin",
             R"pin(<7>│  Dexterity:   8            <11>Hair color: red                <15>Cranial Flashlight            Cash: Random!                   <11>Worn:                                                                        <7>│)pin",
             R"pin(<7>│  Intelligence 8            <11>Hair style: medium-length      <15>Finger Lighter                Pets: <9>None!                      <10>|| <15>boxer shorts                                                             <7>│)pin",
             R"pin(<7>│  Perception:  8            <11>Skin tone: rose                <15>Integrated Toolset            Addictions: <9>None!                <10>|| <15>pair of socks                                                            <7>│)pin",
             R"pin(<7>│                                                           <15>Internal Climate Control                                       <10>|| <15>boonie hat                                                               <7>│)pin",
             R"pin(<7>│                                                           <15>Internal Storage                                               <10>|| <15>t-shirt                                                                  <7>│)pin",
             R"pin(<7>│ <15>Skills:                                                   Metabolic Interchange                                          <10>|| <15>pair of fingerless leather gloves                                        <7>│)pin",
             R"pin(<7>│  <11>Ranged skills                                            <15>Power Storage Mk. II                                           <10>|| <15>cargo pants                                                              <7>│)pin",
             R"pin(<7>│   Marksmanship:       2                                   <15>Taste Modifier                                                 <10>|| <15>pair of boots                                                            <7>│)pin",
             R"pin(<7>│   Rifles:             2                                  <15>Spells: <9>None!                                                   <10>|| <15>aluminum broadhead bolt (quiver) (19)                                    <7>│)pin",
             R"pin(<7>│  <11>Crafting skills                                                                                                         <10>|| <15>machete (scabbard)                                                       <7>│)pin",
             R"pin(<7>│   Cooking:            3                                                                                                  <10>|| <15>plastic canteen                                                          <7>│)pin",
             R"pin(<7>│   Fabrication:        5                                                                                                  <10>|| <15>trenchcoat                                                               <7>│)pin",
             R"pin(<7>│   Survival:           6                                                                                                  <10>|| <15>crossbow+1 <2>(1/1 aluminum broadhead bolt)                                 <7>│)pin",
             R"pin(<7>│  <11>Interaction skills                                                                                                      <10>|| <15>backpack                                                                 <7>│)pin",
             R"pin(<7>│   Athletics:          2                                                                                                  <10>|| <15>diver's watch                                                            <7>│)pin",
             R"pin(<7>│   First Aid:          3                                                                                                  <10>|| <15>pair of binoculars                                                       <7>│)pin",
             R"pin(<7>│   Trapping:           5                                                                                                 <11>Inventory:                                                                   <7>│)pin",
             R"pin(<7>│                                                                                                                          <15>smartphone <10>(160/200)                                                        <7>│)pin",
             R"pin(<7>│                                                                                                                                                                                                      │)pin",
             R"pin(<7>│                                                                                                                                                                                                      │)pin",
             R"pin(<7>│                                                                                                                                                                                                      │)pin",
             R"pin(<7>│                                                                                                                                                                                                      │)pin",
             R"pin(<7>│                                                                                                                                                                                                      │)pin",
             R"pin(<7>│                                                                                                                                                                                                      │)pin",
             R"pin(<7>│                                                                                                                                                                                                      │)pin",
             R"pin(<7>│                                                                                                                                                                                                      │)pin",
             R"pin(<7>│                                                                                                                                                                                                      │)pin",
             R"pin(<7>│                                                                                                                                                                                                      │)pin",
             R"pin(<7>│                                                                                                                                                                                                      │)pin",
             R"pin(<7>│                                                                                                                                                                                                      │)pin",
             R"pin(<7>│                                                                                                                                                                                                      │)pin",
             R"pin(<7>│                                                                                                                                                                                                      │)pin",
             R"pin(<7>│                                                                                                                                                                                                      │)pin",
             R"pin(<7>│ Press <10>! <7>to save character template, <10>% <7>to re-roll or <10>^ <7>for random scenario.                                                                                                                           │)pin",
             R"pin(<7>│ Press <10>h, LEFT, 4 or NUMPAD_4 <7>or <10>l, RIGHT, 6 or NUMPAD_6 <7>to cycle through name, height, a                                                                                                             │)pin",
             R"pin(<7>│ Press <10>k, 8, UP or NUMPAD_8 <7>and <10>j, DOWN, 2 or NUMPAD_2 <7>to change height and age.                                                                                                                      │)pin",
             R"pin(<7>│ Press <10>RETURN <7>to enter popup input.                                                                                                                                                                   │)pin",
             R"pin(<7>│ Press <10>TAB <7>to finish character creation or <10>BACKTAB <7>to go back.                                                                                                                                        │)pin",
             R"pin(<7>└──────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────┘)pin",
         }},
    };
    return screens;
}

auto record(const std::string& name, const std::vector<std::string>& screen) -> void {
    // PIN_OUT names a file that receives each screen in the source form of `recorded`.
    if (const auto* const out = std::getenv("PIN_OUT")) {
        auto file = std::ofstream(out, std::ios::app);
        file << "        {\"" << name << "\",\n         {\n";
        for (const auto& row : screen) { file << "             R\"pin(" << row << ")pin\",\n"; }
        file << "         }},\n";
        return;
    }
    const auto found = recorded().find(name);
    INFO(name);
    REQUIRE(found != recorded().end());
    CHECK(screen == found->second);
}

} // namespace

TEST_CASE(
    "the native character creation keeps its text and colors on every tab",
    "[newchar_native_pin]") {
    const auto guard = screen_guard{};
    auto& you = get_avatar();
    const auto original_name = you.name;
    you.name = "Ada";
    struct restore_name {
        avatar& you;
        std::string name;
        ~restore_name() { you.name = name; }
    } const restore{you, original_name};
    const auto none = std::string{};
    // What the player does on each read, by the tab it is on; a screen is recorded when it has a
    // name.
    struct step {
        std::string name;
        std::string action;
    };
    auto script = std::vector<step>{
        {"points", "NEXT_TAB"},
        {"scenario", "NEXT_TAB"},
        {"profession:first", "DOWN"},
    };
    auto stage = std::size_t{0};
    auto picked = false;
    auto scans = 0;
    auto after_pick = std::vector<step>{
        {"profession:picked", "NEXT_TAB"},
        {"stats", "NEXT_TAB"},
        {"traits", "NEXT_TAB"},
        {"bionics", "NEXT_TAB"},
        {"skills", "NEXT_TAB"},
        {"overview", "QUIT"},
        {none, "YES"},
    };
    auto quit = false;
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto screen = rendered();
        if (stage < script.size()) {
            const auto& now = script[stage];
            record(now.name, rendered(crop_of(now.name)));
            ++stage;
            return act(now.action);
        }
        if (!picked) {
            // Walk the profession list to the one with bionics, then choose it.
            if (highlights(screen, "Bionic Survivalist")) {
                picked = true;
                return act("CONFIRM");
            }
            REQUIRE(scans++ < 400);
            return act("DOWN");
        }
        const auto index = stage - script.size();
        REQUIRE(index < after_pick.size());
        const auto& now = after_pick[index];
        if (!now.name.empty()) { record(now.name, rendered(crop_of(now.name))); }
        if (now.action == "YES") { quit = true; }
        ++stage;
        return act(now.action);
    });
    // Cosmetics and ages are random: the screens are recorded for one seed.
    rng_set_engine_seed(424242);
    CHECK_FALSE(you.create(character_type::CUSTOM));
    CHECK(quit);
}

#endif // CATA_MCP
