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
#    include "options.h"
#    include "options_helpers.h"
#    include "output.h"
#    include "state_helpers.h"

#    include <cstdlib>
#    include <format>
#    include <fstream>
#    include <map>
#    include <string>
#    include <vector>

/// Pins what the native options and keybinding screens print, so that publishing them to the engine
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
        {"options:0",
         {
             R"pin(          <7>┌────────────────────────────────── <9>OPTIONS <7>───────────────────────────────────┐)pin",
             R"pin(          <7>│<15>If enabled, when your character dies, the player is given a prompt that gives <7>│)pin",
             R"pin(          <7>│<15>the option to reload the last saved game instead of dying. #Default: False    <7>│)pin",
             R"pin(          <7>│                                                                              │)pin",
             R"pin(          <7>│                                                                              │)pin",
             R"pin(          <7>├────┬──<15>[<10:4>General<15>]<7>─<15>[<10>Interface<15>]<7>─<15>[<10>Graphics<15>]<7>─<15>[<10>Performance<15>]<7>─<15>[<10>World Defaults<15>]<7>─<15>[<10>Debug<15>]<7>┤)pin",
             R"pin(          <10>^ <15>1  <7>│<11>>> <15>Prompt on character death                           <7>│ <9:4>False           <7>│)pin",
             R"pin(          <6:6>│ <15>2  <7>│                                                       │                 │)pin",
             R"pin(          <6:6>│ <15>3  <7>│   <15>Default character name                              <7>│                 │)pin",
             R"pin(          <6:6>│ <15>4  <7>│   <15>Default character gender                            <7>│ <10>Male            <7>│)pin",
             R"pin(          <6:6>│ <15>5  <7>│                                                       │                 │)pin",
             R"pin(          <6:6>│ <15>6  <7>│   <15>+ Merge similar comestibles                         <7>│                 │)pin",
             R"pin(          <6:6>│ <15>9  <7>│                                                       │                 │)pin",
             R"pin(          <6:6>│ <15>10 <7>│   <15>Auto pickup enabled                                 <7>│ <9>False           <7>│)pin",
             R"pin(          <6:6>│ <15>11 <7>│   Auto pickup adjacent                                │ False           │)pin",
             R"pin(          <6:6>│ <15>12 <7>│   Auto pickup weight limit                            │ 0               │)pin",
             R"pin(          <6:6>│ <15>13 <7>│   Auto pickup volume limit                            │ 0               │)pin",
             R"pin(          <6:6>│ <15>14 <7>│   Auto pickup safe mode                               │ False           │)pin",
             R"pin(          <6:6>│ <15>15 <7>│   List items within no auto pickup zones              │ True            │)pin",
             R"pin(          <6:6>│ <15>16 <7>│                                                       │                 │)pin",
             R"pin(          <6:6>│ <15>17 <7>│   <15>Additional auto features                            <7>│ <9>False           <7>│)pin",
             R"pin(          <6:6>│ <15>18 <7>│   Auto pulp or butcher                                │ Disabled        │)pin",
             R"pin(          <6:6>│ <15>19 <7>│   Auto mining                                         │ False           │)pin",
             R"pin(          <6:6>│ <15>20 <7>│   Auto foraging                                       │ Disabled        │)pin",
             R"pin(          <6:6>│ <15>21 <7>│                                                       │                 │)pin",
             R"pin(          <6:6>│ <15>22 <7>│   <15>Dangerous pickups                                   <7>│ <9>False           <7>│)pin",
             R"pin(          <7>│ <15>23 <7>│   <15>Dangerous terrain warning prompt                    <7>│ <10>Always          <7>│)pin",
             R"pin(          <7>│ <15>24 <7>│                                                       │                 │)pin",
             R"pin(          <7>│ <15>25 <7>│   <15>Safe mode                                           <7>│ <10>True            <7>│)pin",
             R"pin(          <7>│ <15>26 <7>│   <15>Safe mode proximity distance                        <7>│ <10>0               <7>│)pin",
             R"pin(          <7>│ <15>27 <7>│   <15>Safe mode when driving                              <7>│ <9>False           <7>│)pin",
             R"pin(          <7>│ <15>28 <7>│   <15>Auto reactivate safe mode                           <7>│ <9>False           <7>│)pin",
             R"pin(          <7>│ <15>29 <7>│   <15>Turns to auto reactivate safe mode                  <7>│ <10>50              <7>│)pin",
             R"pin(          <7>│ <15>30 <7>│   <15>Turns to remember ignored monsters                  <7>│ <10>200             <7>│)pin",
             R"pin(          <7>│ <15>31 <7>│   <15>Query before attacking neutral monsters             <7>│ <10>True            <7>│)pin",
             R"pin(          <7>│ <15>32 <7>│                                                       │                 │)pin",
             R"pin(          <7>│ <15>33 <7>│   <15>+ Clothing destruction popup                        <7>│                 │)pin",
             R"pin(          <7>│ <15>38 <7>│                                                       │                 │)pin",
             R"pin(          <10>v <15>39 <7>│   <15>Realtime turn progression                           <7>│ <10>0.00            <7>│)pin",
             R"pin(          <7>└────┴───────────────────────────────────────────────────────┴─────────────────┘)pin",
         }},
        {"options:5",
         {
             R"pin(          <7>┌────────────────────────────────── <9>OPTIONS <7>───────────────────────────────────┐)pin",
             R"pin(          <7>│<15>If true, will enable to pickup items one tile around to the player.  You can  <7>│)pin",
             R"pin(          <7>│<15>assign No Auto Pickup zones with the Zones Manager 'Y' key for e.g.  your     <7>│)pin",
             R"pin(          <7>│<15>homebase. #Default: False                                                     <7>│)pin",
             R"pin(          <7>│                                                                              │)pin",
             R"pin(          <7>├────┬──<15>[<10:4>General<15>]<7>─<15>[<10>Interface<15>]<7>─<15>[<10>Graphics<15>]<7>─<15>[<10>Performance<15>]<7>─<15>[<10>World Defaults<15>]<7>─<15>[<10>Debug<15>]<7>┤)pin",
             R"pin(          <10>^ <15>1  <7>│   <15>Prompt on character death                           <7>│ <9>False           <7>│)pin",
             R"pin(          <7>│ <15>2  <7>│                                                       │                 │)pin",
             R"pin(          <7>│ <15>3  <7>│   <15>Default character name                              <7>│                 │)pin",
             R"pin(          <6:6>│ <15>4  <7>│   <15>Default character gender                            <7>│ <10>Male            <7>│)pin",
             R"pin(          <6:6>│ <15>5  <7>│                                                       │                 │)pin",
             R"pin(          <6:6>│ <15>6  <7>│   <15>+ Merge similar comestibles                         <7>│                 │)pin",
             R"pin(          <6:6>│ <15>9  <7>│                                                       │                 │)pin",
             R"pin(          <6:6>│ <15>10 <7>│   <15>Auto pickup enabled                                 <7>│ <9>False           <7>│)pin",
             R"pin(          <6:6>│ <15>11 <7>│<11>>> <7>Auto pickup adjacent                                │ <7:4>False           <7>│)pin",
             R"pin(          <6:6>│ <15>12 <7>│   Auto pickup weight limit                            │ 0               │)pin",
             R"pin(          <6:6>│ <15>13 <7>│   Auto pickup volume limit                            │ 0               │)pin",
             R"pin(          <6:6>│ <15>14 <7>│   Auto pickup safe mode                               │ False           │)pin",
             R"pin(          <6:6>│ <15>15 <7>│   List items within no auto pickup zones              │ True            │)pin",
             R"pin(          <6:6>│ <15>16 <7>│                                                       │                 │)pin",
             R"pin(          <6:6>│ <15>17 <7>│   <15>Additional auto features                            <7>│ <9>False           <7>│)pin",
             R"pin(          <6:6>│ <15>18 <7>│   Auto pulp or butcher                                │ Disabled        │)pin",
             R"pin(          <6:6>│ <15>19 <7>│   Auto mining                                         │ False           │)pin",
             R"pin(          <6:6>│ <15>20 <7>│   Auto foraging                                       │ Disabled        │)pin",
             R"pin(          <6:6>│ <15>21 <7>│                                                       │                 │)pin",
             R"pin(          <6:6>│ <15>22 <7>│   <15>Dangerous pickups                                   <7>│ <9>False           <7>│)pin",
             R"pin(          <6:6>│ <15>23 <7>│   <15>Dangerous terrain warning prompt                    <7>│ <10>Always          <7>│)pin",
             R"pin(          <6:6>│ <15>24 <7>│                                                       │                 │)pin",
             R"pin(          <7>│ <15>25 <7>│   <15>Safe mode                                           <7>│ <10>True            <7>│)pin",
             R"pin(          <7>│ <15>26 <7>│   <15>Safe mode proximity distance                        <7>│ <10>0               <7>│)pin",
             R"pin(          <7>│ <15>27 <7>│   <15>Safe mode when driving                              <7>│ <9>False           <7>│)pin",
             R"pin(          <7>│ <15>28 <7>│   <15>Auto reactivate safe mode                           <7>│ <9>False           <7>│)pin",
             R"pin(          <7>│ <15>29 <7>│   <15>Turns to auto reactivate safe mode                  <7>│ <10>50              <7>│)pin",
             R"pin(          <7>│ <15>30 <7>│   <15>Turns to remember ignored monsters                  <7>│ <10>200             <7>│)pin",
             R"pin(          <7>│ <15>31 <7>│   <15>Query before attacking neutral monsters             <7>│ <10>True            <7>│)pin",
             R"pin(          <7>│ <15>32 <7>│                                                       │                 │)pin",
             R"pin(          <7>│ <15>33 <7>│   <15>+ Clothing destruction popup                        <7>│                 │)pin",
             R"pin(          <7>│ <15>38 <7>│                                                       │                 │)pin",
             R"pin(          <10>v <15>39 <7>│   <15>Realtime turn progression                           <7>│ <10>0.00            <7>│)pin",
             R"pin(          <7>└────┴───────────────────────────────────────────────────────┴─────────────────┘)pin",
         }},
        {"world:0",
         {
             R"pin(          <7>┌────────────────────────────────── <9>OPTIONS <7>───────────────────────────────────┐)pin",
             R"pin(          <7>│<15>If enabled, when your character dies, the player is given a prompt that gives <7>│)pin",
             R"pin(          <7>│<15>the option to reload the last saved game instead of dying. #Default: False    <7>│)pin",
             R"pin(          <7>│                                                                              │)pin",
             R"pin(          <7>│                                                                              │)pin",
             R"pin(          <7>├────┬──<15>[<10:4>General<15>]<7>─<15>[<10>Interface<15>]<7>─<15>[<10>Graphics<15>]<7>─<15>[<10>Performance<15>]<7>─<15>[<10>Current world<15>]<7>─<15>[<10>Debug<15>]<7>─┤)pin",
             R"pin(          <10>^ <15>1  <7>│<11>>> <15>Prompt on character death                           <7>│ <9:4>False           <7>│)pin",
             R"pin(          <6:6>│ <15>2  <7>│                                                       │                 │)pin",
             R"pin(          <6:6>│ <15>3  <7>│   <15>Default character name                              <7>│                 │)pin",
             R"pin(          <6:6>│ <15>4  <7>│   <15>Default character gender                            <7>│ <10>Male            <7>│)pin",
             R"pin(          <6:6>│ <15>5  <7>│                                                       │                 │)pin",
             R"pin(          <6:6>│ <15>6  <7>│   <15>+ Merge similar comestibles                         <7>│                 │)pin",
             R"pin(          <6:6>│ <15>9  <7>│                                                       │                 │)pin",
             R"pin(          <6:6>│ <15>10 <7>│   <15>Auto pickup enabled                                 <7>│ <9>False           <7>│)pin",
             R"pin(          <6:6>│ <15>11 <7>│   Auto pickup adjacent                                │ False           │)pin",
             R"pin(          <6:6>│ <15>12 <7>│   Auto pickup weight limit                            │ 0               │)pin",
             R"pin(          <6:6>│ <15>13 <7>│   Auto pickup volume limit                            │ 0               │)pin",
             R"pin(          <6:6>│ <15>14 <7>│   Auto pickup safe mode                               │ False           │)pin",
             R"pin(          <6:6>│ <15>15 <7>│   List items within no auto pickup zones              │ True            │)pin",
             R"pin(          <6:6>│ <15>16 <7>│                                                       │                 │)pin",
             R"pin(          <6:6>│ <15>17 <7>│   <15>Additional auto features                            <7>│ <9>False           <7>│)pin",
             R"pin(          <6:6>│ <15>18 <7>│   Auto pulp or butcher                                │ Disabled        │)pin",
             R"pin(          <6:6>│ <15>19 <7>│   Auto mining                                         │ False           │)pin",
             R"pin(          <6:6>│ <15>20 <7>│   Auto foraging                                       │ Disabled        │)pin",
             R"pin(          <6:6>│ <15>21 <7>│                                                       │                 │)pin",
             R"pin(          <6:6>│ <15>22 <7>│   <15>Dangerous pickups                                   <7>│ <9>False           <7>│)pin",
             R"pin(          <7>│ <15>23 <7>│   <15>Dangerous terrain warning prompt                    <7>│ <10>Always          <7>│)pin",
             R"pin(          <7>│ <15>24 <7>│                                                       │                 │)pin",
             R"pin(          <7>│ <15>25 <7>│   <15>Safe mode                                           <7>│ <10>True            <7>│)pin",
             R"pin(          <7>│ <15>26 <7>│   <15>Safe mode proximity distance                        <7>│ <10>0               <7>│)pin",
             R"pin(          <7>│ <15>27 <7>│   <15>Safe mode when driving                              <7>│ <9>False           <7>│)pin",
             R"pin(          <7>│ <15>28 <7>│   <15>Auto reactivate safe mode                           <7>│ <9>False           <7>│)pin",
             R"pin(          <7>│ <15>29 <7>│   <15>Turns to auto reactivate safe mode                  <7>│ <10>50              <7>│)pin",
             R"pin(          <7>│ <15>30 <7>│   <15>Turns to remember ignored monsters                  <7>│ <10>200             <7>│)pin",
             R"pin(          <7>│ <15>31 <7>│   <15>Query before attacking neutral monsters             <7>│ <10>True            <7>│)pin",
             R"pin(          <7>│ <15>32 <7>│                                                       │                 │)pin",
             R"pin(          <7>│ <15>33 <7>│   <15>+ Clothing destruction popup                        <7>│                 │)pin",
             R"pin(          <7>│ <15>38 <7>│                                                       │                 │)pin",
             R"pin(          <10>v <15>39 <7>│   <15>Realtime turn progression                           <7>│ <10>0.00            <7>│)pin",
             R"pin(          <7>└────┴───────────────────────────────────────────────────────┴─────────────────┘)pin",
         }},
        {"worldgen:0",
         {
             R"pin(            <7>┌──────────┐ ┌─────────────┐ ┌──────────────┐)pin",
             R"pin(            <7>│World Mods│<7:4><<7>│<7:4>World Options<7>│<7:4>><7>│Finalize World│)pin",
             R"pin(          <7>┌─┴──────────┴─┘             └─┴──────────────┴────────────────────────────────┐)pin",
             R"pin(          <7>│<15>Handling of game world when last character dies. #Default: Reset - Values:    <7>│)pin",
             R"pin(          <7>│<15>Reset, Delete, Query, Keep                                                    <7>│)pin",
             R"pin(          <7>│                                                                              │)pin",
             R"pin(          <7>│                                                                              │)pin",
             R"pin(          <7>├                                                                              ┤)pin",
             R"pin(          <10>^ <15>1  <7>│<11>>> <15>World end handling                                  <7>│ <10:4>Reset           <7>│)pin",
             R"pin(          <6:6>│ <15>2  <7>│                                                       │                 │)pin",
             R"pin(          <6:6>│ <15>3  <7>│   <15>Size of cities                                      <7>│ <10>8               <7>│)pin",
             R"pin(          <6:6>│ <15>4  <7>│   <15>City spacing                                        <7>│ <10>4               <7>│)pin",
             R"pin(          <6:6>│ <15>5  <7>│   <15>Overmap specials density factor                     <7>│ <10>1.00            <7>│)pin",
             R"pin(          <6:6>│ <15>6  <7>│   <15>Overmap specials spacing                            <7>│ <10>6               <7>│)pin",
             R"pin(          <6:6>│ <15>7  <7>│   <15>Vehicle damage scaling factor                       <7>│ <10>1.00            <7>│)pin",
             R"pin(          <6:6>│ <15>8  <7>│   <15>Vehicle door locks                                  <7>│ <10>True            <7>│)pin",
             R"pin(          <6:6>│ <15>9  <7>│   <15>Vehicle spawn rate scaling factor                   <7>│ <10>1.00            <7>│)pin",
             R"pin(          <6:6>│ <15>10 <7>│   <15>Vehicle gun recoil scaling factor                   <7>│ <10>1.00            <7>│)pin",
             R"pin(          <6:6>│ <15>11 <7>│   <15>Spawn rate scaling factor                           <7>│ <10>1.00            <7>│)pin",
             R"pin(          <6:6>│ <15>12 <7>│   <15>Animal spawn rate scaling factor                    <7>│ <10>1.00            <7>│)pin",
             R"pin(          <6:6>│ <15>13 <7>│   <15>Carrion spawn rate scaling factor                   <7>│ <10>1.00            <7>│)pin",
             R"pin(          <6:6>│ <15>14 <7>│   <15>NPC spawn rate scaling factor                       <7>│ <10>0.10            <7>│)pin",
             R"pin(          <6:6>│ <15>15 <7>│   <15>Monster evolution scaling factor                    <7>│ <10>2.00            <7>│)pin",
             R"pin(          <7>│ <15>16 <7>│   <15>Maximum Evolution Half Lives                        <7>│ <10>5               <7>│)pin",
             R"pin(          <7>│ <15>17 <7>│   <15>Zombies Always Evolve                               <7>│ <9>False           <7>│)pin",
             R"pin(          <7>│ <15>18 <7>│   <15>Crowd crush                                         <7>│ <9>False           <7>│)pin",
             R"pin(          <7>│ <15>19 <7>│                                                       │                 │)pin",
             R"pin(          <7>│ <15>20 <7>│   <15>Merchant restock scaling factor                     <7>│ <10>1.00            <7>│)pin",
             R"pin(          <7>│ <15>21 <7>│   <15>Cross z-level melee difficulty modifier             <7>│ <10>1.20            <7>│)pin",
             R"pin(          <7>│ <15>22 <7>│                                                       │                 │)pin",
             R"pin(          <7>│ <15>23 <7>│   <15>+ Enabled Skill Buffs                               <7>│                 │)pin",
             R"pin(          <7>│ <15>26 <7>│                                                       │                 │)pin",
             R"pin(          <7>│ <15>27 <7>│   <15>Starting Trait Cancelling                           <7>│ <9>False           <7>│)pin",
             R"pin(          <7>│ <15>28 <7>│                                                       │                 │)pin",
             R"pin(          <7>│ <15>29 <7>│   <15>Item spawn scaling factor                           <7>│ <10>1.00            <7>│)pin",
             R"pin(          <7>│ <15>30 <7>│                                                       │                 │)pin",
             R"pin(          <7>│ <15>31 <7>│   <15>+ Item category scaling factors                     <7>│                 │)pin",
             R"pin(          <7>│ <15>79 <7>│                                                       │                 │)pin",
             R"pin(          <10>v <15>80 <7>│   <15>+ Time Scaling                                      <7>│                 │)pin",
             R"pin(          <7>└────┴───────────────────────────────────────────────────────┴─────────────────┘)pin",
         }},
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

TEST_CASE("the native options screen keeps its text and colors", "[settings_native_pin]") {
    const auto guard = screen_guard{};
    auto step = 0;
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        // The first page, then the cursor on a row whose prerequisite is not met.
        if (step == 0 || step == 5) { CHECK(matches(std::format("options:{}", step), rendered())); }
        if (step++ < 5) { return act("DOWN"); }
        return act("QUIT");
    });
    get_options().show(false);
    CHECK(step == 6);
}

TEST_CASE("the native world options tab says Current world in game", "[settings_native_pin]") {
    const auto guard = screen_guard{};
    auto step = 0;
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        CHECK(matches(std::format("world:{}", step++), rendered()));
        return act("QUIT");
    });
    get_options().show(true);
    CHECK(step == 1);
}

TEST_CASE("the native world creation options keep their text and colors", "[settings_native_pin]") {
    const auto guard = screen_guard{};
    auto step = 0;
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        CHECK(matches(std::format("worldgen:{}", step++), rendered()));
        return act("QUIT");
    });
    get_options().show(false, true);
    CHECK(step == 1);
}

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
