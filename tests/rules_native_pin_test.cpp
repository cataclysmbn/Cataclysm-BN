#if defined(CATA_MCP)

#    include "auto_pickup.h"
#    include "avatar.h"
#    include "catch/catch.hpp"
#    include "client_command.h"
#    include "client_input.h"
#    include "client_interaction.h"
#    include "client_memory.h"
#    include "client_memory_scope.h"
#    include "color.h"
#    include "cursesdef.h"
#    include "cursesport.h"
#    include "distraction_manager.h"
#    include "game_constants.h"
#    include "input.h"
#    include "json.h"
#    include "output.h"
#    include "path_info.h"
#    include "safemode_ui.h"
#    include "state_helpers.h"

#    include <algorithm>
#    include <filesystem>
#    include <format>
#    include <fstream>
#    include <map>
#    include <memory>
#    include <sstream>
#    include <string>
#    include <vector>

/// Pins what the native rule screens, distraction manager and colour manager print, so that
/// publishing them to the engine client leaves the player's screens byte-equal: every row with its
/// text and colour runs.
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
        {"distractions:0",
         {
             R"pin(          <7>┌─────────────────────────────<9>Distractions manager<7>─────────────────────────────┐)pin",
             R"pin(          <7>│                                                                              │)pin",
             R"pin(          <7>│                                                                              │)pin",
             R"pin(          <7>├                                                                              ┤)pin",
             R"pin(          <7>│                                                                              │)pin",
             R"pin(          <7>│ <15:4>Noise                                                      <7>│ <10>Enabled         <7>│)pin",
             R"pin(          <7>│ <15>Pain                                                       <7>│ <10>Enabled         <7>│)pin",
             R"pin(          <7>│ <15>Attacked                                                   <7>│ <10>Enabled         <7>│)pin",
             R"pin(          <7>│ <15>Hostile Spotted                                            <7>│ <10>Enabled         <7>│)pin",
             R"pin(          <7>│ <15>Hostile Dangerously Close                                  <7>│ <10>Enabled         <7>│)pin",
             R"pin(          <7>│ <15>Conversation                                               <7>│ <10>Enabled         <7>│)pin",
             R"pin(          <7>│ <15>Asthma                                                     <7>│ <10>Enabled         <7>│)pin",
             R"pin(          <7>│ <15>Weather change                                             <7>│ <10>Enabled         <7>│)pin",
             R"pin(          <7>│                                                            │                 │)pin",
             R"pin(          <7>│                                                            │                 │)pin",
             R"pin(          <7>│                                                            │                 │)pin",
             R"pin(          <7>│                                                            │                 │)pin",
             R"pin(          <7>│                                                            │                 │)pin",
             R"pin(          <7>│                                                            │                 │)pin",
             R"pin(          <7>│                                                            │                 │)pin",
             R"pin(          <7>│                                                            │                 │)pin",
             R"pin(          <7>│                                                            │                 │)pin",
             R"pin(          <7>│                                                            │                 │)pin",
             R"pin(          <7>└────────────────────────────────────────────────────────────┴─────────────────┘)pin",
         }},
        {"distractions:1",
         {
             R"pin(          <7>┌─────────────────────────────<9>Distractions manager<7>─────────────────────────────┐)pin",
             R"pin(          <7>│                                                                              │)pin",
             R"pin(          <7>│                                                                              │)pin",
             R"pin(          <7>├                                                                              ┤)pin",
             R"pin(          <7>│                                                                              │)pin",
             R"pin(          <7>│ <15>Noise                                                      <7>│ <10>Enabled         <7>│)pin",
             R"pin(          <7>│ <15:4>Pain                                                       <7>│ <10>Enabled         <7>│)pin",
             R"pin(          <7>│ <15>Attacked                                                   <7>│ <10>Enabled         <7>│)pin",
             R"pin(          <7>│ <15>Hostile Spotted                                            <7>│ <10>Enabled         <7>│)pin",
             R"pin(          <7>│ <15>Hostile Dangerously Close                                  <7>│ <10>Enabled         <7>│)pin",
             R"pin(          <7>│ <15>Conversation                                               <7>│ <10>Enabled         <7>│)pin",
             R"pin(          <7>│ <15>Asthma                                                     <7>│ <10>Enabled         <7>│)pin",
             R"pin(          <7>│ <15>Weather change                                             <7>│ <10>Enabled         <7>│)pin",
             R"pin(          <7>│                                                            │                 │)pin",
             R"pin(          <7>│                                                            │                 │)pin",
             R"pin(          <7>│                                                            │                 │)pin",
             R"pin(          <7>│                                                            │                 │)pin",
             R"pin(          <7>│                                                            │                 │)pin",
             R"pin(          <7>│                                                            │                 │)pin",
             R"pin(          <7>│                                                            │                 │)pin",
             R"pin(          <7>│                                                            │                 │)pin",
             R"pin(          <7>│                                                            │                 │)pin",
             R"pin(          <7>│                                                            │                 │)pin",
             R"pin(          <7>└────────────────────────────────────────────────────────────┴─────────────────┘)pin",
         }},
        {"distractions:2",
         {
             R"pin(          <7>┌─────────────────────────────<9>Distractions manager<7>─────────────────────────────┐)pin",
             R"pin(          <7>│                                                                              │)pin",
             R"pin(          <7>│                                                                              │)pin",
             R"pin(          <7>├                                                                              ┤)pin",
             R"pin(          <7>│                                                                              │)pin",
             R"pin(          <7>│ <15>Noise                                                      <7>│ <10>Enabled         <7>│)pin",
             R"pin(          <7>│ <15:4>Pain                                                       <7>│ <1>Disabled        <7>│)pin",
             R"pin(          <7>│ <15>Attacked                                                   <7>│ <10>Enabled         <7>│)pin",
             R"pin(          <7>│ <15>Hostile Spotted                                            <7>│ <10>Enabled         <7>│)pin",
             R"pin(          <7>│ <15>Hostile Dangerously Close                                  <7>│ <10>Enabled         <7>│)pin",
             R"pin(          <7>│ <15>Conversation                                               <7>│ <10>Enabled         <7>│)pin",
             R"pin(          <7>│ <15>Asthma                                                     <7>│ <10>Enabled         <7>│)pin",
             R"pin(          <7>│ <15>Weather change                                             <7>│ <10>Enabled         <7>│)pin",
             R"pin(          <7>│                                                            │                 │)pin",
             R"pin(          <7>│                                                            │                 │)pin",
             R"pin(          <7>│                                                            │                 │)pin",
             R"pin(          <7>│                                                            │                 │)pin",
             R"pin(          <7>│                                                            │                 │)pin",
             R"pin(          <7>│                                                            │                 │)pin",
             R"pin(          <7>│                                                            │                 │)pin",
             R"pin(          <7>│                                                            │                 │)pin",
             R"pin(          <7>│                                                            │                 │)pin",
             R"pin(          <7>│                                                            │                 │)pin",
             R"pin(          <7>└────────────────────────────────────────────────────────────┴─────────────────┘)pin",
         }},
        {"colors:0",
         {
             R"pin(          <7>┌─────────────────────────────── <9>COLOR MANAGER <7>────────────────────────────────┐)pin",
             R"pin(          <7>│                                                                              │)pin",
             R"pin(          <7>│                                                                              │)pin",
             R"pin(          <7>├                                                                              ┤)pin",
             R"pin(          <7>│                                                                              │)pin",
             R"pin(          <10>^ <11>> <15>c_black           <0>default  <1>c_red             <7>│   <0:7>default  <10>c_light_green    <7>│)pin",
             R"pin(          <6:6>│   <15>c_black_cyan      <0:6>default                    <7>│   <6>default                   <7>│)pin",
             R"pin(          <6:6>│   <15>c_black_green     <0:2>default                    <7>│   <2>default                   <7>│)pin",
             R"pin(          <15>│   c_black_magenta   <0:5>default                    <7>│   <5>default                   <7>│)pin",
             R"pin(          <15>│   c_black_red       <8:1>default                    <7>│   <1>default                   <7>│)pin",
             R"pin(          <15>│   c_black_white     <0:7>default                    <7>│   default                   │)pin",
             R"pin(          <15>│   c_black_yellow    <0:3>default                    <7>│   <3>default                   <7>│)pin",
             R"pin(          <15>│   c_blue            <4>default                    <7>│   <0:4>default                   <7>│)pin",
             R"pin(          <15>│   c_blue_cyan       <4:6>default                    <7>│   <6:4>default                   <7>│)pin",
             R"pin(          <15>│   c_blue_green      <4:2>default                    <7>│   <2:4>default                   <7>│)pin",
             R"pin(          <15>│   c_blue_magenta    <4:5>default                    <7>│   <5:4>default                   <7>│)pin",
             R"pin(          <15>│   c_blue_red        <4:1>default                    <7>│   <1:4>default                   <7>│)pin",
             R"pin(          <15>│   c_blue_white      <4:7>default                    <7>│   <7:4>default                   <7>│)pin",
             R"pin(          <15>│   c_blue_yellow     <4:3>default                    <7>│   <3:4>default                   <7>│)pin",
             R"pin(          <15>│   c_brown           <3>default                    <7>│   <0:3>default                   <7>│)pin",
             R"pin(          <15>│   c_brown_cyan      <3:6>default                    <7>│   <6:3>default                   <7>│)pin",
             R"pin(          <15>│   c_brown_green     <3:2>default                    <7>│   <2:3>default                   <7>│)pin",
             R"pin(          <10>v   <15>c_brown_magenta   <3:5>default                    <7>│   <5:3>default                   <7>│)pin",
             R"pin(          <7>└────────────────────────────────────────────────┴─────────────────────────────┘)pin",
         }},
        {"colors:1",
         {
             R"pin(          <7>┌─────────────────────────────── <9>COLOR MANAGER <7>────────────────────────────────┐)pin",
             R"pin(          <7>│                                                                              │)pin",
             R"pin(          <7>│                                                                              │)pin",
             R"pin(          <7>├                                                                              ┤)pin",
             R"pin(          <7>│                                                                              │)pin",
             R"pin(          <10>^   <15>c_black           <0>default  <1>c_red             <7>│   <0:7>default  <10>c_light_green    <7>│)pin",
             R"pin(          <15>│ <11>> <15>c_black_cyan      <0:6>default                    <7>│   <6>default                   <7>│)pin",
             R"pin(          <6:6>│   <15>c_black_green     <0:2>default                    <7>│   <2>default                   <7>│)pin",
             R"pin(          <6:6>│   <15>c_black_magenta   <0:5>default                    <7>│   <5>default                   <7>│)pin",
             R"pin(          <15>│   c_black_red       <8:1>default                    <7>│   <1>default                   <7>│)pin",
             R"pin(          <15>│   c_black_white     <0:7>default                    <7>│   default                   │)pin",
             R"pin(          <15>│   c_black_yellow    <0:3>default                    <7>│   <3>default                   <7>│)pin",
             R"pin(          <15>│   c_blue            <4>default                    <7>│   <0:4>default                   <7>│)pin",
             R"pin(          <15>│   c_blue_cyan       <4:6>default                    <7>│   <6:4>default                   <7>│)pin",
             R"pin(          <15>│   c_blue_green      <4:2>default                    <7>│   <2:4>default                   <7>│)pin",
             R"pin(          <15>│   c_blue_magenta    <4:5>default                    <7>│   <5:4>default                   <7>│)pin",
             R"pin(          <15>│   c_blue_red        <4:1>default                    <7>│   <1:4>default                   <7>│)pin",
             R"pin(          <15>│   c_blue_white      <4:7>default                    <7>│   <7:4>default                   <7>│)pin",
             R"pin(          <15>│   c_blue_yellow     <4:3>default                    <7>│   <3:4>default                   <7>│)pin",
             R"pin(          <15>│   c_brown           <3>default                    <7>│   <0:3>default                   <7>│)pin",
             R"pin(          <15>│   c_brown_cyan      <3:6>default                    <7>│   <6:3>default                   <7>│)pin",
             R"pin(          <15>│   c_brown_green     <3:2>default                    <7>│   <2:3>default                   <7>│)pin",
             R"pin(          <10>v   <15>c_brown_magenta   <3:5>default                    <7>│   <5:3>default                   <7>│)pin",
             R"pin(          <7>└────────────────────────────────────────────────┴─────────────────────────────┘)pin",
         }},
        {"colors:2",
         {
             R"pin(          <7>┌─────────────────────────────── <9>COLOR MANAGER <7>────────────────────────────────┐)pin",
             R"pin(          <7>│                                                                              │)pin",
             R"pin(          <7>│                                                                              │)pin",
             R"pin(          <7>├                                                                              ┤)pin",
             R"pin(          <7>│                                                                              │)pin",
             R"pin(          <10>^   <15>c_black           <0>default  <1>c_red             <7>│   <0:7>default  <10>c_light_green    <7>│)pin",
             R"pin(          <15>│   c_black_cyan      <0:6>default                    <7>│ <11>> <6>default                   <7>│)pin",
             R"pin(          <6:6>│   <15>c_black_green     <0:2>default                    <7>│   <2>default                   <7>│)pin",
             R"pin(          <6:6>│   <15>c_black_magenta   <0:5>default                    <7>│   <5>default                   <7>│)pin",
             R"pin(          <15>│   c_black_red       <8:1>default                    <7>│   <1>default                   <7>│)pin",
             R"pin(          <15>│   c_black_white     <0:7>default                    <7>│   default                   │)pin",
             R"pin(          <15>│   c_black_yellow    <0:3>default                    <7>│   <3>default                   <7>│)pin",
             R"pin(          <15>│   c_blue            <4>default                    <7>│   <0:4>default                   <7>│)pin",
             R"pin(          <15>│   c_blue_cyan       <4:6>default                    <7>│   <6:4>default                   <7>│)pin",
             R"pin(          <15>│   c_blue_green      <4:2>default                    <7>│   <2:4>default                   <7>│)pin",
             R"pin(          <15>│   c_blue_magenta    <4:5>default                    <7>│   <5:4>default                   <7>│)pin",
             R"pin(          <15>│   c_blue_red        <4:1>default                    <7>│   <1:4>default                   <7>│)pin",
             R"pin(          <15>│   c_blue_white      <4:7>default                    <7>│   <7:4>default                   <7>│)pin",
             R"pin(          <15>│   c_blue_yellow     <4:3>default                    <7>│   <3:4>default                   <7>│)pin",
             R"pin(          <15>│   c_brown           <3>default                    <7>│   <0:3>default                   <7>│)pin",
             R"pin(          <15>│   c_brown_cyan      <3:6>default                    <7>│   <6:3>default                   <7>│)pin",
             R"pin(          <15>│   c_brown_green     <3:2>default                    <7>│   <2:3>default                   <7>│)pin",
             R"pin(          <10>v   <15>c_brown_magenta   <3:5>default                    <7>│   <5:3>default                   <7>│)pin",
             R"pin(          <7>└────────────────────────────────────────────────┴─────────────────────────────┘)pin",
         }},
        {"autopickup:0",
         {
             R"pin(          <7>┌──────────────────────────── <9>AUTO PICKUP MANAGER <7>─────────────────────────────┐)pin",
             R"pin(          <7>│                                                                              │)pin",
             R"pin(          <7>│                                                                              │)pin",
             R"pin(          <7>├                                                                              ┤)pin",
             R"pin(          <7>│                                                                              │)pin",
             R"pin(          <7>│ <15>1  <7>│<11>>> <15:4>*knife*                                   <7>│ <15>Include <7>│                 │)pin",
             R"pin(          <7>│ 2  │   <empty rule>                              │ Include │                 │)pin",
             R"pin(          <7>│ <15>3  <7>│   <15>sledge hammer                             <7>│ <15>Exclude <7>│                 │)pin",
             R"pin(          <7>│    │                                             │         │                 │)pin",
             R"pin(          <7>│    │                                             │         │                 │)pin",
             R"pin(          <7>│    │                                             │         │                 │)pin",
             R"pin(          <7>│    │                                             │         │                 │)pin",
             R"pin(          <7>│    │                                             │         │                 │)pin",
             R"pin(          <7>│    │                                             │         │                 │)pin",
             R"pin(          <7>│    │                                             │         │                 │)pin",
             R"pin(          <7>│    │                                             │         │                 │)pin",
             R"pin(          <7>│    │                                             │         │                 │)pin",
             R"pin(          <7>│    │                                             │         │                 │)pin",
             R"pin(          <7>│    │                                             │         │                 │)pin",
             R"pin(          <7>│    │                                             │         │                 │)pin",
             R"pin(          <7>│    │                                             │         │                 │)pin",
             R"pin(          <7>│    │                                             │         │                 │)pin",
             R"pin(          <7>│    │                                             │         │                 │)pin",
             R"pin(          <7>└────┴─────────────────────────────────────────────┴─────────┴─────────────────┘)pin",
         }},
        {"autopickup:1",
         {
             R"pin(          <7>┌──────────────────────────── <9>AUTO PICKUP MANAGER <7>─────────────────────────────┐)pin",
             R"pin(          <7>│                                                                              │)pin",
             R"pin(          <7>│                                                                              │)pin",
             R"pin(          <7>├                                                                              ┤)pin",
             R"pin(          <7>│                                                                              │)pin",
             R"pin(          <7>│ <15>1  <7>│   <15>*knife*                                   <7>│ <15>Include <7>│                 │)pin",
             R"pin(          <7>│ 2  │   <empty rule>                              │ Include │                 │)pin",
             R"pin(          <7>│ <15>3  <7>│   <15>sledge hammer                             <7>│ <15>Exclude <7>│                 │)pin",
             R"pin(          <7>│ <15>4  <7>│<11>>> <15:4><empty rule>                              <7>│ <15>Include <7>│                 │)pin",
             R"pin(          <7>│    │                                             │         │                 │)pin",
             R"pin(          <7>│    │          ┌─────────────────────────────────────────────┐                │)pin",
             R"pin(          <7>│    │          │<9>Pickup Rule: <7:4>_<7>_____________________________  │                │)pin",
             R"pin(          <7>│    │          └─────────────────────────────────────────────┘                │)pin",
             R"pin(          <7>│    │   ┌──────────────────────────────────────────────────────────┐          │)pin",
             R"pin(          <7>│    │   │<15>* is used as a Wildcard.  A few Examples:                 <7>│          │)pin",
             R"pin(          <7>│    │   │                                                          │          │)pin",
             R"pin(          <7>│    │   │<15>wooden arrow    matches the itemname exactly              <7>│          │)pin",
             R"pin(          <7>│    │   │<15>wooden ar*      matches items beginning with wood ar      <7>│          │)pin",
             R"pin(          <7>│    │   │<15>*rrow           matches items ending with rrow            <7>│          │)pin",
             R"pin(          <7>│    │   │<15>*avy fle*fi*arrow     multiple * are allowed              <7>│          │)pin",
             R"pin(          <7>│    │   │<15>heAVY*woOD*arrOW      case insensitive search             <7>│          │)pin",
             R"pin(          <7>│    │   │                                                          │          │)pin",
             R"pin(          <7>│    │   │<15>Pickup using:                                             <7>│          │)pin",
             R"pin(          <7>└────┴───│<15>c:food          matches item in category food             <7>│──────────┘)pin",
             R"pin(                   <7>│<15>m:kevlar        matches items made of Kevlar              <7>│)pin",
             R"pin(                   <7>│<15>M:copper        matches items made purely of copper       <7>│)pin",
             R"pin(                   <7>│<15>q:drilling      matches items with drilling qualites      <7>│)pin",
             R"pin(                   <7>│<15>k:fabrication   matches books that teach fabrication      <7>│)pin",
             R"pin(                   <7>│<15>b:c:food;*meat* matches conditions c:food and *meat*      <7>│)pin",
             R"pin(                   <7>│                                                          │)pin",
             R"pin(                   <7>└──────────────────────────────────────────────────────────┘)pin",
         }},
        {"autopickup:2",
         {
             R"pin(          <7>┌──────────────────────────── <9>AUTO PICKUP MANAGER <7>─────────────────────────────┐)pin",
             R"pin(          <7>│                                                                              │)pin",
             R"pin(          <7>│                                                                              │)pin",
             R"pin(          <7>├                                                                              ┤)pin",
             R"pin(          <7>│                                                                              │)pin",
             R"pin(          <7>│ <15>1  <7>│<11>>> <15:4>*knife*                                   <7>│ <15>Include <7>│                 │)pin",
             R"pin(          <7>│ 2  │   <empty rule>                              │ Include │                 │)pin",
             R"pin(          <7>│ <15>3  <7>│   <15>sledge hammer                             <7>│ <15>Exclude <7>│                 │)pin",
             R"pin(          <7>│    │                                             │         │                 │)pin",
             R"pin(          <7>│    │                                             │         │                 │)pin",
             R"pin(          <7>│    │                                             │         │                 │)pin",
             R"pin(          <7>│    │                                             │         │                 │)pin",
             R"pin(          <7>│    │                                             │         │                 │)pin",
             R"pin(          <7>│    │                                             │         │                 │)pin",
             R"pin(          <7>│    │                                             │         │                 │)pin",
             R"pin(          <7>│    │                                             │         │                 │)pin",
             R"pin(          <7>│    │                                             │         │                 │)pin",
             R"pin(          <7>│    │                                             │         │                 │)pin",
             R"pin(          <7>│    │                                             │         │                 │)pin",
             R"pin(          <7>│    │                                             │         │                 │)pin",
             R"pin(          <7>│    │                                             │         │                 │)pin",
             R"pin(          <7>│    │                                             │         │                 │)pin",
             R"pin(          <7>│    │                                             │         │                 │)pin",
             R"pin(          <7>└────┴─────────────────────────────────────────────┴─────────┴─────────────────┘)pin",
             R"pin(                   <7>│<15>m:kevlar        matches items made of Kevlar              <7>│)pin",
             R"pin(                   <7>│<15>M:copper        matches items made purely of copper       <7>│)pin",
             R"pin(                   <7>│<15>q:drilling      matches items with drilling qualites      <7>│)pin",
             R"pin(                   <7>│<15>k:fabrication   matches books that teach fabrication      <7>│)pin",
             R"pin(                   <7>│<15>b:c:food;*meat* matches conditions c:food and *meat*      <7>│)pin",
             R"pin(                   <7>│                                                          │)pin",
             R"pin(                   <7>└──────────────────────────────────────────────────────────┘)pin",
         }},
        {"autopickup:3",
         {
             R"pin(          <7>┌──────────────────────────── <9>AUTO PICKUP MANAGER <7>─────────────────────────────┐)pin",
             R"pin(          <7>│                                                                              │)pin",
             R"pin(          <7>│                                                                              │)pin",
             R"pin(          <7>├                                                                              ┤)pin",
             R"pin(          <7>│                                                                              │)pin",
             R"pin(          <7>│ <15>1  <7>│   <15>*knife*                                   <7>│ <15>Include <7>│                 │)pin",
             R"pin(          <7>│ 2  │<11>>> <7:4><empty rule>                              <7>│ Include │                 │)pin",
             R"pin(          <7>│ <15>3  <7>│   <15>sledge hammer                             <7>│ <15>Exclude <7>│                 │)pin",
             R"pin(          <7>│    │                                             │         │                 │)pin",
             R"pin(          <7>│    │                                             │         │                 │)pin",
             R"pin(          <7>│    │                                             │         │                 │)pin",
             R"pin(          <7>│    │                                             │         │                 │)pin",
             R"pin(          <7>│    │                                             │         │                 │)pin",
             R"pin(          <7>│    │                                             │         │                 │)pin",
             R"pin(          <7>│    │                                             │         │                 │)pin",
             R"pin(          <7>│    │                                             │         │                 │)pin",
             R"pin(          <7>│    │                                             │         │                 │)pin",
             R"pin(          <7>│    │                                             │         │                 │)pin",
             R"pin(          <7>│    │                                             │         │                 │)pin",
             R"pin(          <7>│    │                                             │         │                 │)pin",
             R"pin(          <7>│    │                                             │         │                 │)pin",
             R"pin(          <7>│    │                                             │         │                 │)pin",
             R"pin(          <7>│    │                                             │         │                 │)pin",
             R"pin(          <7>└────┴─────────────────────────────────────────────┴─────────┴─────────────────┘)pin",
             R"pin(                   <7>│<15>m:kevlar        matches items made of Kevlar              <7>│)pin",
             R"pin(                   <7>│<15>M:copper        matches items made purely of copper       <7>│)pin",
             R"pin(                   <7>│<15>q:drilling      matches items with drilling qualites      <7>│)pin",
             R"pin(                   <7>│<15>k:fabrication   matches books that teach fabrication      <7>│)pin",
             R"pin(                   <7>│<15>b:c:food;*meat* matches conditions c:food and *meat*      <7>│)pin",
             R"pin(                   <7>│                                                          │)pin",
             R"pin(                   <7>└──────────────────────────────────────────────────────────┘)pin",
         }},
        {"autopickup:4",
         {
             R"pin(          <7>┌──────────────────────────── <9>AUTO PICKUP MANAGER <7>─────────────────────────────┐)pin",
             R"pin(          <7>│                                                                              │)pin",
             R"pin(          <7>│                                                                              │)pin",
             R"pin(          <7>├                                                                              ┤)pin",
             R"pin(          <7>│                                                                              │)pin",
             R"pin(          <7>│ <15>1  <7>│   <15>*knife*                                   <7>│ <15>Include <7>│                 │)pin",
             R"pin(          <7>│ 2  │<11>>> <7><empty rule>                              │ <7:4>Include <7>│                 │)pin",
             R"pin(          <7>│ <15>3  <7>│   <15>sledge hammer                             <7>│ <15>Exclude <7>│                 │)pin",
             R"pin(          <7>│    │                                             │         │                 │)pin",
             R"pin(          <7>│    │                                             │         │                 │)pin",
             R"pin(          <7>│    │                                             │         │                 │)pin",
             R"pin(          <7>│    │                                             │         │                 │)pin",
             R"pin(          <7>│    │                                             │         │                 │)pin",
             R"pin(          <7>│    │                                             │         │                 │)pin",
             R"pin(          <7>│    │                                             │         │                 │)pin",
             R"pin(          <7>│    │                                             │         │                 │)pin",
             R"pin(          <7>│    │                                             │         │                 │)pin",
             R"pin(          <7>│    │                                             │         │                 │)pin",
             R"pin(          <7>│    │                                             │         │                 │)pin",
             R"pin(          <7>│    │                                             │         │                 │)pin",
             R"pin(          <7>│    │                                             │         │                 │)pin",
             R"pin(          <7>│    │                                             │         │                 │)pin",
             R"pin(          <7>│    │                                             │         │                 │)pin",
             R"pin(          <7>└────┴─────────────────────────────────────────────┴─────────┴─────────────────┘)pin",
             R"pin(                   <7>│<15>m:kevlar        matches items made of Kevlar              <7>│)pin",
             R"pin(                   <7>│<15>M:copper        matches items made purely of copper       <7>│)pin",
             R"pin(                   <7>│<15>q:drilling      matches items with drilling qualites      <7>│)pin",
             R"pin(                   <7>│<15>k:fabrication   matches books that teach fabrication      <7>│)pin",
             R"pin(                   <7>│<15>b:c:food;*meat* matches conditions c:food and *meat*      <7>│)pin",
             R"pin(                   <7>│                                                          │)pin",
             R"pin(                   <7>└──────────────────────────────────────────────────────────┘)pin",
         }},
        {"autopickup:5",
         {
             R"pin(          <7>┌──────────────────────────── <9>AUTO PICKUP MANAGER <7>─────────────────────────────┐)pin",
             R"pin(          <7>│                                                                              │)pin",
             R"pin(          <7>│                                                                              │)pin",
             R"pin(          <7>├                                                                              ┤)pin",
             R"pin(          <7>│                                                                              │)pin",
             R"pin(          <7>│    │                                             │         │                 │)pin",
             R"pin(          <7>│    │                                             │         │                 │)pin",
             R"pin(          <7>│    │                                             │         │                 │)pin",
             R"pin(          <7>│    │                                             │         │                 │)pin",
             R"pin(          <7>│    │                                             │         │                 │)pin",
             R"pin(          <7>│    │                                             │         │                 │)pin",
             R"pin(          <7>│    │                                             │         │                 │)pin",
             R"pin(          <7>│    │                                             │         │                 │)pin",
             R"pin(          <7>│    │                                             │         │                 │)pin",
             R"pin(          <7>│    │                                             │         │                 │)pin",
             R"pin(          <7>│    │                                             │         │                 │)pin",
             R"pin(          <7>│    │                                             │         │                 │)pin",
             R"pin(          <7>│    │                                             │         │                 │)pin",
             R"pin(          <7>│    │                                             │         │                 │)pin",
             R"pin(          <7>│    │                                             │         │                 │)pin",
             R"pin(          <7>│    │                                             │         │                 │)pin",
             R"pin(          <7>│    │                                             │         │                 │)pin",
             R"pin(          <7>│    │                                             │         │                 │)pin",
             R"pin(          <7>└────┴─────────────────────────────────────────────┴─────────┴─────────────────┘)pin",
             R"pin(                   <7>│<15>m:kevlar        matches items made of Kevlar              <7>│)pin",
             R"pin(                   <7>│<15>M:copper        matches items made purely of copper       <7>│)pin",
             R"pin(                   <7>│<15>q:drilling      matches items with drilling qualites      <7>│)pin",
             R"pin(                   <7>│<15>k:fabrication   matches books that teach fabrication      <7>│)pin",
             R"pin(                   <7>│<15>b:c:food;*meat* matches conditions c:food and *meat*      <7>│)pin",
             R"pin(                   <7>│                                                          │)pin",
             R"pin(                   <7>└──────────────────────────────────────────────────────────┘)pin",
         }},
        {"autopickup:6",
         {
             R"pin(          <7>┌──────────────────────────── <9>AUTO PICKUP MANAGER <7>─────────────────────────────┐)pin",
             R"pin(          <7>│                                                                              │)pin",
             R"pin(          <7>│                                                                              │)pin",
             R"pin(          <7>├                                                                              ┤)pin",
             R"pin(          <7>│                                                                              │)pin",
             R"pin(          <7>│ <15>1  <7>│<11>>> <15>*knife*                                   <7>│ <15:4>Include <7>│                 │)pin",
             R"pin(          <7>│ 2  │   <empty rule>                              │ Include │                 │)pin",
             R"pin(          <7>│ <15>3  <7>│   <15>sledge hammer                             <7>│ <15>Exclude <7>│                 │)pin",
             R"pin(          <7>│    │                                             │         │                 │)pin",
             R"pin(          <7>│    │                                             │         │                 │)pin",
             R"pin(          <7>│    │                                             │         │                 │)pin",
             R"pin(          <7>│    │                                             │         │                 │)pin",
             R"pin(          <7>│    │                                             │         │                 │)pin",
             R"pin(          <7>│    │                                             │         │                 │)pin",
             R"pin(          <7>│    │                                             │         │                 │)pin",
             R"pin(          <7>│    │                                             │         │                 │)pin",
             R"pin(          <7>│    │                                             │         │                 │)pin",
             R"pin(          <7>│    │                                             │         │                 │)pin",
             R"pin(          <7>│    │                                             │         │                 │)pin",
             R"pin(          <7>│    │                                             │         │                 │)pin",
             R"pin(          <7>│    │                                             │         │                 │)pin",
             R"pin(          <7>│    │                                             │         │                 │)pin",
             R"pin(          <7>│    │                                             │         │                 │)pin",
             R"pin(          <7>└────┴─────────────────────────────────────────────┴─────────┴─────────────────┘)pin",
             R"pin(                   <7>│<15>m:kevlar        matches items made of Kevlar              <7>│)pin",
             R"pin(                   <7>│<15>M:copper        matches items made purely of copper       <7>│)pin",
             R"pin(                   <7>│<15>q:drilling      matches items with drilling qualites      <7>│)pin",
             R"pin(                   <7>│<15>k:fabrication   matches books that teach fabrication      <7>│)pin",
             R"pin(                   <7>│<15>b:c:food;*meat* matches conditions c:food and *meat*      <7>│)pin",
             R"pin(                   <7>│                                                          │)pin",
             R"pin(                   <7>└──────────────────────────────────────────────────────────┘)pin",
         }},
        {"autopickup:7",
         {
             R"pin(          <7>┌──────────────────────────── <9>AUTO PICKUP MANAGER <7>─────────────────────────────┐)pin",
             R"pin(          <7>│                                                                              │)pin",
             R"pin(          <7>│                                                                              │)pin",
             R"pin(          <7>├                                                                              ┤)pin",
             R"pin(          <7>│                                                                              │)pin",
             R"pin(          <7>│ <15>1  <7>│   <15>*knife*                                   <7>│ <15>Include <7>│                 │)pin",
             R"pin(          <7>│ 2  │<11>>> <7:4><empty rule>                              <7>│ Include │                 │)pin",
             R"pin(          <7>│ <15>3  <7>│   <15>sledge hammer                             <7>│ <15>Exclude <7>│                 │)pin",
             R"pin(          <7>│    │                                             │         │                 │)pin",
             R"pin(          <7>│    │                                             │         │                 │)pin",
             R"pin(          <7>│    │                                             │         │                 │)pin",
             R"pin(          <7>│    │                                             │         │                 │)pin",
             R"pin(          <7>│    │                                             │         │                 │)pin",
             R"pin(          <7>│    │                                             │         │                 │)pin",
             R"pin(          <7>│    │                                             │         │                 │)pin",
             R"pin(          <7>│    │                                             │         │                 │)pin",
             R"pin(          <7>│    │                                             │         │                 │)pin",
             R"pin(          <7>│    │                                             │         │                 │)pin",
             R"pin(          <7>│    │                                             │         │                 │)pin",
             R"pin(          <7>│    │                                             │         │                 │)pin",
             R"pin(          <7>│    │                                             │         │                 │)pin",
             R"pin(          <7>│    │                                             │         │                 │)pin",
             R"pin(          <7>│    │                                             │         │                 │)pin",
             R"pin(          <7>└────┴─────────────────────────────────────────────┴─────────┴─────────────────┘)pin",
             R"pin(                   <7>│<15>m:kevlar        matches items made of Kevlar              <7>│)pin",
             R"pin(                   <7>│<15>M:copper        matches items made purely of copper       <7>│)pin",
             R"pin(                   <7>│<15>q:drilling      matches items with drilling qualites      <7>│)pin",
             R"pin(                   <7>│<15>k:fabrication   matches books that teach fabrication      <7>│)pin",
             R"pin(                   <7>│<15>b:c:food;*meat* matches conditions c:food and *meat*      <7>│)pin",
             R"pin(                   <7>│                                                          │)pin",
             R"pin(                   <7>└──────────────────────────────────────────────────────────┘)pin",
         }},
        {"autopickup:8",
         {
             R"pin(          <7>┌──────────────────────────── <9>AUTO PICKUP MANAGER <7>─────────────────────────────┐)pin",
             R"pin(          <7>│                                                                              │)pin",
             R"pin(          <7>│                                                                              │)pin",
             R"pin(          <7>├                                                                              ┤)pin",
             R"pin(          <7>│                                                                              │)pin",
             R"pin(          <7>│ <15>1  <7>│   <15>*knife*                                   <7>│ <15>Include <7>│                 │)pin",
             R"pin(          <7>│ 2  │   <empty rule>                              │ Include │                 │)pin",
             R"pin(          <7>│ <15>3  <7>│<11>>> <15:4>sledge hammer                             <7>│ <15>Exclude <7>│                 │)pin",
             R"pin(          <7>│    │                                             │         │                 │)pin",
             R"pin(          <7>│    │                                             │         │                 │)pin",
             R"pin(          <7>│    │                                             │         │                 │)pin",
             R"pin(          <7>│    │                                             │         │                 │)pin",
             R"pin(          <7>│    │                                             │         │                 │)pin",
             R"pin(          <7>│    │                                             │         │                 │)pin",
             R"pin(          <7>│    │                                             │         │                 │)pin",
             R"pin(          <7>│    │                                             │         │                 │)pin",
             R"pin(          <7>│    │                                             │         │                 │)pin",
             R"pin(          <7>│    │                                             │         │                 │)pin",
             R"pin(          <7>│    │                                             │         │                 │)pin",
             R"pin(          <7>│    │                                             │         │                 │)pin",
             R"pin(          <7>│    │                                             │         │                 │)pin",
             R"pin(          <7>│    │                                             │         │                 │)pin",
             R"pin(          <7>│    │                                             │         │                 │)pin",
             R"pin(          <7>└────┴─────────────────────────────────────────────┴─────────┴─────────────────┘)pin",
             R"pin(                   <7>│<15>m:kevlar        matches items made of Kevlar              <7>│)pin",
             R"pin(                   <7>│<15>M:copper        matches items made purely of copper       <7>│)pin",
             R"pin(                   <7>│<15>q:drilling      matches items with drilling qualites      <7>│)pin",
             R"pin(                   <7>│<15>k:fabrication   matches books that teach fabrication      <7>│)pin",
             R"pin(                   <7>│<15>b:c:food;*meat* matches conditions c:food and *meat*      <7>│)pin",
             R"pin(                   <7>│                                                          │)pin",
             R"pin(                   <7>└──────────────────────────────────────────────────────────┘)pin",
         }},
        {"autopickup:9",
         {
             R"pin(          <7>┌──────────────────────────── <9>AUTO PICKUP MANAGER <7>─────────────────────────────┐)pin",
             R"pin(          <7>│                                                                              │)pin",
             R"pin(          <7>│                                                                              │)pin",
             R"pin(          <7>├                                                                              ┤)pin",
             R"pin(          <7>│                                                                              │)pin",
             R"pin(          <7>│ <15>1  <7>│   <15>*knife<7>┌─────────<15:4>1 item matches: sledge hammer<7>──────────┐              │)pin",
             R"pin(          <7>│ 2  │   <empty│<15>1   <11>>> <15:4>sledge hammer                            <7>│              │)pin",
             R"pin(          <7>│ <15>3  <7>│<11>>> <15:4>sledge<7>│                                                │              │)pin",
             R"pin(          <7>│    │         │                                                │              │)pin",
             R"pin(          <7>│    │         │                                                │              │)pin",
             R"pin(          <7>│    │         │                                                │              │)pin",
             R"pin(          <7>│    │         │                                                │              │)pin",
             R"pin(          <7>│    │         │                                                │              │)pin",
             R"pin(          <7>│    │         │                                                │              │)pin",
             R"pin(          <7>│    │         │                                                │              │)pin",
             R"pin(          <7>│    │         │                                                │              │)pin",
             R"pin(          <7>│    │         │                                                │              │)pin",
             R"pin(          <7>│    │         │                                                │              │)pin",
             R"pin(          <7>│    │         │                                                │              │)pin",
             R"pin(          <7>│    │         │                                                │              │)pin",
             R"pin(          <7>│    │         │                                                │              │)pin",
             R"pin(          <7>│    │         │                                                │              │)pin",
             R"pin(          <7>│    │         └────<15:1>Won't display content or suffix matches<7>─────┘              │)pin",
             R"pin(          <7>└────┴─────────────────────────────────────────────┴─────────┴─────────────────┘)pin",
             R"pin(                   <7>│<15>m:kevlar        matches items made of Kevlar              <7>│)pin",
             R"pin(                   <7>│<15>M:copper        matches items made purely of copper       <7>│)pin",
             R"pin(                   <7>│<15>q:drilling      matches items with drilling qualites      <7>│)pin",
             R"pin(                   <7>│<15>k:fabrication   matches books that teach fabrication      <7>│)pin",
             R"pin(                   <7>│<15>b:c:food;*meat* matches conditions c:food and *meat*      <7>│)pin",
             R"pin(                   <7>│                                                          │)pin",
             R"pin(                   <7>└──────────────────────────────────────────────────────────┘)pin",
         }},
        {"autopickup:10",
         {
             R"pin(          <7>┌──────────────────────────── <9>AUTO PICKUP MANAGER <7>─────────────────────────────┐)pin",
             R"pin(          <7>│                                                                              │)pin",
             R"pin(          <7>│                                                                              │)pin",
             R"pin(          <7>├                                                                              ┤)pin",
             R"pin(          <7>│                                                                              │)pin",
             R"pin(          <7>│ <15>1  <7>│   <15>*knife<7>┌─────────<15:4>1 item matches: sledge hammer<7>──────────┐              │)pin",
             R"pin(          <7>│ 2  │   <empty│<15>1   <11>>> <15:4>sledge hammer                            <7>│              │)pin",
             R"pin(          <7>│ <15>3  <7>│<11>>> <15:4>sledge<7>│                                                │              │)pin",
             R"pin(          <7>│    │         │                                                │              │)pin",
             R"pin(          <7>│    │         │                                                │              │)pin",
             R"pin(          <7>│    │         │                                                │              │)pin",
             R"pin(          <7>│    │         │                                                │              │)pin",
             R"pin(          <7>│    │         │                                                │              │)pin",
             R"pin(          <7>│    │         │                                                │              │)pin",
             R"pin(          <7>│    │         │                                                │              │)pin",
             R"pin(          <7>│    │         │                                                │              │)pin",
             R"pin(          <7>│    │         │                                                │              │)pin",
             R"pin(          <7>│    │         │                                                │              │)pin",
             R"pin(          <7>│    │         │                                                │              │)pin",
             R"pin(          <7>│    │         │                                                │              │)pin",
             R"pin(          <7>│    │         │                                                │              │)pin",
             R"pin(          <7>│    │         │                                                │              │)pin",
             R"pin(          <7>│    │         └────<15:1>Won't display content or suffix matches<7>─────┘              │)pin",
             R"pin(          <7>└────┴─────────────────────────────────────────────┴─────────┴─────────────────┘)pin",
             R"pin(                   <7>│<15>m:kevlar        matches items made of Kevlar              <7>│)pin",
             R"pin(                   <7>│<15>M:copper        matches items made purely of copper       <7>│)pin",
             R"pin(                   <7>│<15>q:drilling      matches items with drilling qualites      <7>│)pin",
             R"pin(                   <7>│<15>k:fabrication   matches books that teach fabrication      <7>│)pin",
             R"pin(                   <7>│<15>b:c:food;*meat* matches conditions c:food and *meat*      <7>│)pin",
             R"pin(                   <7>│                                                          │)pin",
             R"pin(                   <7>└──────────────────────────────────────────────────────────┘)pin",
         }},
        {"autopickup:11",
         {
             R"pin(          <7>┌──────────────────────────── <9>AUTO PICKUP MANAGER <7>─────────────────────────────┐)pin",
             R"pin(          <7>│                                                                              │)pin",
             R"pin(          <7>│                                                                              │)pin",
             R"pin(          <7>├                                                                              ┤)pin",
             R"pin(          <7>│                                                                              │)pin",
             R"pin(          <7>│ <15>1  <7>│   <15>*knife*                                   <7>│ <15>Include <7>│                 │)pin",
             R"pin(          <7>│ 2  │   <empty rule>                              │ Include │                 │)pin",
             R"pin(          <7>│ <15>3  <7>│<11>>> <15:4>sledge hammer                             <7>│ <15>Exclude <7>│                 │)pin",
             R"pin(          <7>│    │                                             │         │                 │)pin",
             R"pin(          <7>│    │                                             │         │                 │)pin",
             R"pin(          <7>│    │                                             │         │                 │)pin",
             R"pin(          <7>│    │                                             │         │                 │)pin",
             R"pin(          <7>│    │                                             │         │                 │)pin",
             R"pin(          <7>│    │                                             │         │                 │)pin",
             R"pin(          <7>│    │                                             │         │                 │)pin",
             R"pin(          <7>│    │                                             │         │                 │)pin",
             R"pin(          <7>│    │                                             │         │                 │)pin",
             R"pin(          <7>│    │                                             │         │                 │)pin",
             R"pin(          <7>│    │                                             │         │                 │)pin",
             R"pin(          <7>│    │                                             │         │                 │)pin",
             R"pin(          <7>│    │                                             │         │                 │)pin",
             R"pin(          <7>│    │                                             │         │                 │)pin",
             R"pin(          <7>│    │                                             │         │                 │)pin",
             R"pin(          <7>└────┴─────────────────────────────────────────────┴─────────┴─────────────────┘)pin",
             R"pin(                   <7>│<15>m:kevlar        matches items made of Kevlar              <7>│)pin",
             R"pin(                   <7>│<15>M:copper        matches items made purely of copper       <7>│)pin",
             R"pin(                   <7>│<15>q:drilling      matches items with drilling qualites      <7>│)pin",
             R"pin(                   <7>│<15>k:fabrication   matches books that teach fabrication      <7>│)pin",
             R"pin(                   <7>│<15>b:c:food;*meat* matches conditions c:food and *meat*      <7>│)pin",
             R"pin(                   <7>│                                                          │)pin",
             R"pin(                   <7>└──────────────────────────────────────────────────────────┘)pin",
         }},
        {"autopickup_npc:0",
         {
             R"pin(          <7>┌─────────────────────────────<9>Pickup rules for Bob<7>─────────────────────────────┐)pin",
             R"pin(          <7>│                                                                              │)pin",
             R"pin(          <7>│                                                                              │)pin",
             R"pin(          <7>├                                                                              ┤)pin",
             R"pin(          <7>│                                                                              │)pin",
             R"pin(          <7>│ <15>1  <7>│<11>>> <15:4>*knife*                                   <7>│ <15>Include <7>│                 │)pin",
             R"pin(          <7>│ 2  │   <empty rule>                              │ Include │                 │)pin",
             R"pin(          <7>│ <15>3  <7>│   <15>sledge hammer                             <7>│ <15>Exclude <7>│                 │)pin",
             R"pin(          <7>│    │                                             │         │                 │)pin",
             R"pin(          <7>│    │                                             │         │                 │)pin",
             R"pin(          <7>│    │                                             │         │                 │)pin",
             R"pin(          <7>│    │                                             │         │                 │)pin",
             R"pin(          <7>│    │                                             │         │                 │)pin",
             R"pin(          <7>│    │                                             │         │                 │)pin",
             R"pin(          <7>│    │                                             │         │                 │)pin",
             R"pin(          <7>│    │                                             │         │                 │)pin",
             R"pin(          <7>│    │                                             │         │                 │)pin",
             R"pin(          <7>│    │                                             │         │                 │)pin",
             R"pin(          <7>│    │                                             │         │                 │)pin",
             R"pin(          <7>│    │                                             │         │                 │)pin",
             R"pin(          <7>│    │                                             │         │                 │)pin",
             R"pin(          <7>│    │                                             │         │                 │)pin",
             R"pin(          <7>│    │                                             │         │                 │)pin",
             R"pin(          <7>└────┴─────────────────────────────────────────────┴─────────┴─────────────────┘)pin",
         }},
        {"autopickup_npc:1",
         {
             R"pin(          <7>┌─────────────────────────────<9>Pickup rules for Bob<7>─────────────────────────────┐)pin",
             R"pin(          <7>│                                                                              │)pin",
             R"pin(          <7>│                                                                              │)pin",
             R"pin(          <7>├                                                                              ┤)pin",
             R"pin(          <7>│                                                                              │)pin",
             R"pin(          <7>│ <15>1  <7>│   <15>*knife*                                   <7>│ <15>Include <7>│                 │)pin",
             R"pin(          <7>│ 2  │<11>>> <7:4><empty rule>                              <7>│ Include │                 │)pin",
             R"pin(          <7>│ <15>3  <7>│   <15>sledge hammer                             <7>│ <15>Exclude <7>│                 │)pin",
             R"pin(          <7>│    │                                             │         │                 │)pin",
             R"pin(          <7>│    │                                             │         │                 │)pin",
             R"pin(          <7>│    │                                             │         │                 │)pin",
             R"pin(          <7>│    │                                             │         │                 │)pin",
             R"pin(          <7>│    │                                             │         │                 │)pin",
             R"pin(          <7>│    │                                             │         │                 │)pin",
             R"pin(          <7>│    │                                             │         │                 │)pin",
             R"pin(          <7>│    │                                             │         │                 │)pin",
             R"pin(          <7>│    │                                             │         │                 │)pin",
             R"pin(          <7>│    │                                             │         │                 │)pin",
             R"pin(          <7>│    │                                             │         │                 │)pin",
             R"pin(          <7>│    │                                             │         │                 │)pin",
             R"pin(          <7>│    │                                             │         │                 │)pin",
             R"pin(          <7>│    │                                             │         │                 │)pin",
             R"pin(          <7>│    │                                             │         │                 │)pin",
             R"pin(          <7>└────┴─────────────────────────────────────────────┴─────────┴─────────────────┘)pin",
         }},
        {"safemode:0",
         {
             R"pin(          <7>┌───────────────────────────── <9>SAFE MODE MANAGER <7>──────────────────────────────┐)pin",
             R"pin(          <7>│                                                                              │)pin",
             R"pin(          <7>│                                                                              │)pin",
             R"pin(          <7>├                                                                              ┤)pin",
             R"pin(          <7>│                                                                              │)pin",
             R"pin(          <7>│ <15>1  <7>│<11>><15:4>zombie                              <7>│ <15>Hostile <7>│ <15>10  <7>│ <15>Blacklist<7>│ <15>Hostile<7>│)pin",
             R"pin(          <7>│ <15>2  <7>│ <15>*                                   <7>│ <15>Any     <7>│ <15>--- <7>│ <15>Whitelist<7>│ <15>Hostile<7>│)pin",
             R"pin(          <7>│ 3  │ a loud*                             │ ---     │ 5   │ Whitelist│ Sound  │)pin",
             R"pin(          <7>│    │                                     │         │     │          │        │)pin",
             R"pin(          <7>│    │                                     │         │     │          │        │)pin",
             R"pin(          <7>│    │                                     │         │     │          │        │)pin",
             R"pin(          <7>│    │                                     │         │     │          │        │)pin",
             R"pin(          <7>│    │                                     │         │     │          │        │)pin",
             R"pin(          <7>│    │                                     │         │     │          │        │)pin",
             R"pin(          <7>│    │                                     │         │     │          │        │)pin",
             R"pin(          <7>│    │                                     │         │     │          │        │)pin",
             R"pin(          <7>│    │                                     │         │     │          │        │)pin",
             R"pin(          <7>│    │                                     │         │     │          │        │)pin",
             R"pin(          <7>│    │                                     │         │     │          │        │)pin",
             R"pin(          <7>│    │                                     │         │     │          │        │)pin",
             R"pin(          <7>│    │                                     │         │     │          │        │)pin",
             R"pin(          <7>│    │                                     │         │     │          │        │)pin",
             R"pin(          <7>│    │                                     │         │     │          │        │)pin",
             R"pin(          <7>└────┴─────────────────────────────────────┴─────────┴─────┴──────────┴────────┘)pin",
         }},
        {"safemode:1",
         {
             R"pin(          <7>┌───────────────────────────── <9>SAFE MODE MANAGER <7>──────────────────────────────┐)pin",
             R"pin(          <7>│                                                                              │)pin",
             R"pin(          <7>│                                                                              │)pin",
             R"pin(          <7>├                                                                              ┤)pin",
             R"pin(          <7>│                                                                              │)pin",
             R"pin(          <7>│ <15>1  <7>│<11>><15:4>zombie  <7>┌────────────<15:4>2 monsters match: zombie<7>────────────┐<15>klist<7>│ <15>Hostile<7>│)pin",
             R"pin(          <7>│ <15>2  <7>│ <15>*       <7>│<15>1   <11>>> <15:4>zombie                                   <7>│<15>elist<7>│ <15>Hostile<7>│)pin",
             R"pin(          <7>│ 3  │ a loud* │<15>2      zombie                                   <7>│elist│ Sound  │)pin",
             R"pin(          <7>│    │         │                                                │     │        │)pin",
             R"pin(          <7>│    │         │                                                │     │        │)pin",
             R"pin(          <7>│    │         │                                                │     │        │)pin",
             R"pin(          <7>│    │         │                                                │     │        │)pin",
             R"pin(          <7>│    │         │                                                │     │        │)pin",
             R"pin(          <7>│    │         │                                                │     │        │)pin",
             R"pin(          <7>│    │         │                                                │     │        │)pin",
             R"pin(          <7>│    │         │                                                │     │        │)pin",
             R"pin(          <7>│    │         │                                                │     │        │)pin",
             R"pin(          <7>│    │         │                                                │     │        │)pin",
             R"pin(          <7>│    │         │                                                │     │        │)pin",
             R"pin(          <7>│    │         │                                                │     │        │)pin",
             R"pin(          <7>│    │         │                                                │     │        │)pin",
             R"pin(          <7>│    │         │                                                │     │        │)pin",
             R"pin(          <7>│    │         └──<15:1>Lists monsters regardless of their attitude.<7>──┘     │        │)pin",
             R"pin(          <7>└────┴─────────────────────────────────────┴─────────┴─────┴──────────┴────────┘)pin",
         }},
        {"safemode:2",
         {
             R"pin(          <7>┌───────────────────────────── <9>SAFE MODE MANAGER <7>──────────────────────────────┐)pin",
             R"pin(          <7>│                                                                              │)pin",
             R"pin(          <7>│                                                                              │)pin",
             R"pin(          <7>├                                                                              ┤)pin",
             R"pin(          <7>│                                                                              │)pin",
             R"pin(          <7>│ <15>1  <7>│<11>><15:4>zombie  <7>┌────────────<15:4>2 monsters match: zombie<7>────────────┐<15>klist<7>│ <15>Hostile<7>│)pin",
             R"pin(          <7>│ <15>2  <7>│ <15>*       <7>│<15>1      zombie                                   <7>│<15>elist<7>│ <15>Hostile<7>│)pin",
             R"pin(          <7>│ 3  │ a loud* │<15>2   <11>>> <15:4>zombie                                   <7>│elist│ Sound  │)pin",
             R"pin(          <7>│    │         │                                                │     │        │)pin",
             R"pin(          <7>│    │         │                                                │     │        │)pin",
             R"pin(          <7>│    │         │                                                │     │        │)pin",
             R"pin(          <7>│    │         │                                                │     │        │)pin",
             R"pin(          <7>│    │         │                                                │     │        │)pin",
             R"pin(          <7>│    │         │                                                │     │        │)pin",
             R"pin(          <7>│    │         │                                                │     │        │)pin",
             R"pin(          <7>│    │         │                                                │     │        │)pin",
             R"pin(          <7>│    │         │                                                │     │        │)pin",
             R"pin(          <7>│    │         │                                                │     │        │)pin",
             R"pin(          <7>│    │         │                                                │     │        │)pin",
             R"pin(          <7>│    │         │                                                │     │        │)pin",
             R"pin(          <7>│    │         │                                                │     │        │)pin",
             R"pin(          <7>│    │         │                                                │     │        │)pin",
             R"pin(          <7>│    │         └──<15:1>Lists monsters regardless of their attitude.<7>──┘     │        │)pin",
             R"pin(          <7>└────┴─────────────────────────────────────┴─────────┴─────┴──────────┴────────┘)pin",
         }},
        {"safemode:3",
         {
             R"pin(          <7>┌───────────────────────────── <9>SAFE MODE MANAGER <7>──────────────────────────────┐)pin",
             R"pin(          <7>│                                                                              │)pin",
             R"pin(          <7>│                                                                              │)pin",
             R"pin(          <7>├                                                                              ┤)pin",
             R"pin(          <7>│                                                                              │)pin",
             R"pin(          <7>│ <15>1  <7>│<11>><15:4>zombie                              <7>│ <15>Hostile <7>│ <15>10  <7>│ <15>Blacklist<7>│ <15>Hostile<7>│)pin",
             R"pin(          <7>│ <15>2  <7>│ <15>*                                   <7>│ <15>Any     <7>│ <15>--- <7>│ <15>Whitelist<7>│ <15>Hostile<7>│)pin",
             R"pin(          <7>│ 3  │ a loud*                             │ ---     │ 5   │ Whitelist│ Sound  │)pin",
             R"pin(          <7>│    │                                     │         │     │          │        │)pin",
             R"pin(          <7>│    │                                     │         │     │          │        │)pin",
             R"pin(          <7>│    │                                     │         │     │          │        │)pin",
             R"pin(          <7>│    │                                     │         │     │          │        │)pin",
             R"pin(          <7>│    │                                     │         │     │          │        │)pin",
             R"pin(          <7>│    │                                     │         │     │          │        │)pin",
             R"pin(          <7>│    │                                     │         │     │          │        │)pin",
             R"pin(          <7>│    │                                     │         │     │          │        │)pin",
             R"pin(          <7>│    │                                     │         │     │          │        │)pin",
             R"pin(          <7>│    │                                     │         │     │          │        │)pin",
             R"pin(          <7>│    │                                     │         │     │          │        │)pin",
             R"pin(          <7>│    │                                     │         │     │          │        │)pin",
             R"pin(          <7>│    │                                     │         │     │          │        │)pin",
             R"pin(          <7>│    │                                     │         │     │          │        │)pin",
             R"pin(          <7>│    │                                     │         │     │          │        │)pin",
             R"pin(          <7>└────┴─────────────────────────────────────┴─────────┴─────┴──────────┴────────┘)pin",
         }},
        {"safemode:4",
         {
             R"pin(          <7>┌───────────────────────────── <9>SAFE MODE MANAGER <7>──────────────────────────────┐)pin",
             R"pin(          <7>│                                                                              │)pin",
             R"pin(          <7>│                                                                              │)pin",
             R"pin(          <7>├                                                                              ┤)pin",
             R"pin(          <7>│                                                                              │)pin",
             R"pin(          <7>│ <15>1  <7>│<11>><15>zombie                              <7>│ <15:4>Hostile <7>│ <15>10  <7>│ <15>Blacklist<7>│ <15>Hostile<7>│)pin",
             R"pin(          <7>│ <15>2  <7>│ <15>*                                   <7>│ <15>Any     <7>│ <15>--- <7>│ <15>Whitelist<7>│ <15>Hostile<7>│)pin",
             R"pin(          <7>│ 3  │ a loud*                             │ ---     │ 5   │ Whitelist│ Sound  │)pin",
             R"pin(          <7>│    │                                     │         │     │          │        │)pin",
             R"pin(          <7>│    │                                     │         │     │          │        │)pin",
             R"pin(          <7>│    │                                     │         │     │          │        │)pin",
             R"pin(          <7>│    │                                     │         │     │          │        │)pin",
             R"pin(          <7>│    │                                     │         │     │          │        │)pin",
             R"pin(          <7>│    │                                     │         │     │          │        │)pin",
             R"pin(          <7>│    │                                     │         │     │          │        │)pin",
             R"pin(          <7>│    │                                     │         │     │          │        │)pin",
             R"pin(          <7>│    │                                     │         │     │          │        │)pin",
             R"pin(          <7>│    │                                     │         │     │          │        │)pin",
             R"pin(          <7>│    │                                     │         │     │          │        │)pin",
             R"pin(          <7>│    │                                     │         │     │          │        │)pin",
             R"pin(          <7>│    │                                     │         │     │          │        │)pin",
             R"pin(          <7>│    │                                     │         │     │          │        │)pin",
             R"pin(          <7>│    │                                     │         │     │          │        │)pin",
             R"pin(          <7>└────┴─────────────────────────────────────┴─────────┴─────┴──────────┴────────┘)pin",
         }},
        {"safemode:5",
         {
             R"pin(          <7>┌───────────────────────────── <9>SAFE MODE MANAGER <7>──────────────────────────────┐)pin",
             R"pin(          <7>│                                                                              │)pin",
             R"pin(          <7>│                                                                              │)pin",
             R"pin(          <7>├                                                                              ┤)pin",
             R"pin(          <7>│                                                                              │)pin",
             R"pin(          <7>│ <15>1  <7>│ <15>zombie                              <7>│ <15>Hostile <7>│ <15>10  <7>│ <15>Blacklist<7>│ <15>Hostile<7>│)pin",
             R"pin(          <7>│ <15>2  <7>│<11>><15>*                                   <7>│ <15:4>Any     <7>│ <15>--- <7>│ <15>Whitelist<7>│ <15>Hostile<7>│)pin",
             R"pin(          <7>│ 3  │ a loud*                             │ ---     │ 5   │ Whitelist│ Sound  │)pin",
             R"pin(          <7>│    │                                     │         │     │          │        │)pin",
             R"pin(          <7>│    │                                     │         │     │          │        │)pin",
             R"pin(          <7>│    │                                     │         │     │          │        │)pin",
             R"pin(          <7>│    │                                     │         │     │          │        │)pin",
             R"pin(          <7>│    │                                     │         │     │          │        │)pin",
             R"pin(          <7>│    │                                     │         │     │          │        │)pin",
             R"pin(          <7>│    │                                     │         │     │          │        │)pin",
             R"pin(          <7>│    │                                     │         │     │          │        │)pin",
             R"pin(          <7>│    │                                     │         │     │          │        │)pin",
             R"pin(          <7>│    │                                     │         │     │          │        │)pin",
             R"pin(          <7>│    │                                     │         │     │          │        │)pin",
             R"pin(          <7>│    │                                     │         │     │          │        │)pin",
             R"pin(          <7>│    │                                     │         │     │          │        │)pin",
             R"pin(          <7>│    │                                     │         │     │          │        │)pin",
             R"pin(          <7>│    │                                     │         │     │          │        │)pin",
             R"pin(          <7>└────┴─────────────────────────────────────┴─────────┴─────┴──────────┴────────┘)pin",
         }},
        {"safemode:6",
         {
             R"pin(          <7>┌───────────────────────────── <9>SAFE MODE MANAGER <7>──────────────────────────────┐)pin",
             R"pin(          <7>│                                                                              │)pin",
             R"pin(          <7>│                                                                              │)pin",
             R"pin(          <7>├                                                                              ┤)pin",
             R"pin(          <7>│                                                                              │)pin",
             R"pin(          <7>│    │                                     │         │     │          │        │)pin",
             R"pin(          <7>│    │                                     │         │     │          │        │)pin",
             R"pin(          <7>│    │                                     │         │     │          │        │)pin",
             R"pin(          <7>│    │                                     │         │     │          │        │)pin",
             R"pin(          <7>│    │                                     │         │     │          │        │)pin",
             R"pin(          <7>│    │                                     │         │     │          │        │)pin",
             R"pin(          <7>│    │                                     │         │     │          │        │)pin",
             R"pin(          <7>│    │                                     │         │     │          │        │)pin",
             R"pin(          <7>│    │                                     │         │     │          │        │)pin",
             R"pin(          <7>│    │                                     │         │     │          │        │)pin",
             R"pin(          <7>│    │                                     │         │     │          │        │)pin",
             R"pin(          <7>│    │                                     │         │     │          │        │)pin",
             R"pin(          <7>│    │                                     │         │     │          │        │)pin",
             R"pin(          <7>│    │                                     │         │     │          │        │)pin",
             R"pin(          <7>│    │                                     │         │     │          │        │)pin",
             R"pin(          <7>│    │                                     │         │     │          │        │)pin",
             R"pin(          <7>│    │                                     │         │     │          │        │)pin",
             R"pin(          <7>│    │                                     │         │     │          │        │)pin",
             R"pin(          <7>└────┴─────────────────────────────────────┴─────────┴─────┴──────────┴────────┘)pin",
         }},
        {"safemode:7",
         {
             R"pin(          <7>┌───────────────────────────── <9>SAFE MODE MANAGER <7>──────────────────────────────┐)pin",
             R"pin(          <7>│                                                                              │)pin",
             R"pin(          <7>│                                                                              │)pin",
             R"pin(          <7>├                                                                              ┤)pin",
             R"pin(          <7>│                                                                              │)pin",
             R"pin(          <7>│ <15>1  <7>│<11>><15>zombie                              <7>│ <15:4>Hostile <7>│ <15>10  <7>│ <15>Blacklist<7>│ <15>Hostile<7>│)pin",
             R"pin(          <7>│ <15>2  <7>│ <15>*                                   <7>│ <15>Any     <7>│ <15>--- <7>│ <15>Whitelist<7>│ <15>Hostile<7>│)pin",
             R"pin(          <7>│ 3  │ a loud*                             │ ---     │ 5   │ Whitelist│ Sound  │)pin",
             R"pin(          <7>│    │                                     │         │     │          │        │)pin",
             R"pin(          <7>│    │                                     │         │     │          │        │)pin",
             R"pin(          <7>│    │                                     │         │     │          │        │)pin",
             R"pin(          <7>│    │                                     │         │     │          │        │)pin",
             R"pin(          <7>│    │                                     │         │     │          │        │)pin",
             R"pin(          <7>│    │                                     │         │     │          │        │)pin",
             R"pin(          <7>│    │                                     │         │     │          │        │)pin",
             R"pin(          <7>│    │                                     │         │     │          │        │)pin",
             R"pin(          <7>│    │                                     │         │     │          │        │)pin",
             R"pin(          <7>│    │                                     │         │     │          │        │)pin",
             R"pin(          <7>│    │                                     │         │     │          │        │)pin",
             R"pin(          <7>│    │                                     │         │     │          │        │)pin",
             R"pin(          <7>│    │                                     │         │     │          │        │)pin",
             R"pin(          <7>│    │                                     │         │     │          │        │)pin",
             R"pin(          <7>│    │                                     │         │     │          │        │)pin",
             R"pin(          <7>└────┴─────────────────────────────────────┴─────────┴─────┴──────────┴────────┘)pin",
         }},
        {"safemode:8",
         {
             R"pin(          <7>┌───────────────────────────── <9>SAFE MODE MANAGER <7>──────────────────────────────┐)pin",
             R"pin(          <7>│                                                                              │)pin",
             R"pin(          <7>│                                                                              │)pin",
             R"pin(          <7>├                                                                              ┤)pin",
             R"pin(          <7>│                                                                              │)pin",
             R"pin(          <7>│ <15>1  <7>│<11>><15>zombie                              <7>│ <15:4>Neutral <7>│ <15>10  <7>│ <15>Blacklist<7>│ <15>Hostile<7>│)pin",
             R"pin(          <7>│ <15>2  <7>│ <15>*                                   <7>│ <15>Any     <7>│ <15>--- <7>│ <15>Whitelist<7>│ <15>Hostile<7>│)pin",
             R"pin(          <7>│ 3  │ a loud*                             │ ---     │ 5   │ Whitelist│ Sound  │)pin",
             R"pin(          <7>│    │                                     │         │     │          │        │)pin",
             R"pin(          <7>│    │                                     │         │     │          │        │)pin",
             R"pin(          <7>│    │                                     │         │     │          │        │)pin",
             R"pin(          <7>│    │                                     │         │     │          │        │)pin",
             R"pin(          <7>│    │                                     │         │     │          │        │)pin",
             R"pin(          <7>│    │                                     │         │     │          │        │)pin",
             R"pin(          <7>│    │                                     │         │     │          │        │)pin",
             R"pin(          <7>│    │                                     │         │     │          │        │)pin",
             R"pin(          <7>│    │                                     │         │     │          │        │)pin",
             R"pin(          <7>│    │                                     │         │     │          │        │)pin",
             R"pin(          <7>│    │                                     │         │     │          │        │)pin",
             R"pin(          <7>│    │                                     │         │     │          │        │)pin",
             R"pin(          <7>│    │                                     │         │     │          │        │)pin",
             R"pin(          <7>│    │                                     │         │     │          │        │)pin",
             R"pin(          <7>│    │                                     │         │     │          │        │)pin",
             R"pin(          <7>└────┴─────────────────────────────────────┴─────────┴─────┴──────────┴────────┘)pin",
         }},
        {"safemode:9",
         {
             R"pin(          <7>┌───────────────────────────── <9>SAFE MODE MANAGER <7>──────────────────────────────┐)pin",
             R"pin(          <7>│                                                                              │)pin",
             R"pin(          <7>│                                                                              │)pin",
             R"pin(          <7>├                                                                              ┤)pin",
             R"pin(          <7>│                                                                              │)pin",
             R"pin(          <7>│ <15>1  <7>│<11>><15:4>zombie                              <7>│ <15>Neutral <7>│ <15>10  <7>│ <15>Blacklist<7>│ <15>Hostile<7>│)pin",
             R"pin(          <7>│ <15>2  <7>│ <15>*                                   <7>│ <15>Any     <7>│ <15>--- <7>│ <15>Whitelist<7>│ <15>Hostile<7>│)pin",
             R"pin(          <7>│ 3  │ a loud*                             │ ---     │ 5   │ Whitelist│ Sound  │)pin",
             R"pin(          <7>│    │                                     │         │     │          │        │)pin",
             R"pin(          <7>│    │                                     │         │     │          │        │)pin",
             R"pin(          <7>│    │                                     │         │     │          │        │)pin",
             R"pin(          <7>│    │                                     │         │     │          │        │)pin",
             R"pin(          <7>│    │                                     │         │     │          │        │)pin",
             R"pin(          <7>│    │                                     │         │     │          │        │)pin",
             R"pin(          <7>│    │                                     │         │     │          │        │)pin",
             R"pin(          <7>│    │                                     │         │     │          │        │)pin",
             R"pin(          <7>│    │                                     │         │     │          │        │)pin",
             R"pin(          <7>│    │                                     │         │     │          │        │)pin",
             R"pin(          <7>│    │                                     │         │     │          │        │)pin",
             R"pin(          <7>│    │                                     │         │     │          │        │)pin",
             R"pin(          <7>│    │                                     │         │     │          │        │)pin",
             R"pin(          <7>│    │                                     │         │     │          │        │)pin",
             R"pin(          <7>│    │                                     │         │     │          │        │)pin",
             R"pin(          <7>└────┴─────────────────────────────────────┴─────────┴─────┴──────────┴────────┘)pin",
         }},
        {"safemode:10",
         {
             R"pin(          <7>┌───────────────────────────── <9>SAFE MODE MANAGER <7>──────────────────────────────┐)pin",
             R"pin(          <7>│                                                                              │)pin",
             R"pin(          <7>│                                                                              │)pin",
             R"pin(          <7>├                                                                              ┤)pin",
             R"pin(          <7>│                                                                              │)pin",
             R"pin(          <7>│ <15>1  <7>│<11>><15:4>zombie                              <7>│ <15>Neutral <7>│ <15>10  <7>│ <15>Blacklist<7>│ <15>Hostile<7>│)pin",
             R"pin(          <7>│ <15>2  <7>│ <15>*                                   <7>│ <15>Any     <7>│ <15>--- <7>│ <15>Whitelist<7>│ <15>Hostile<7>│)pin",
             R"pin(          <7>│ 3  │ a loud*                             │ ---     │ 5   │ Whitelist│ Sound  │)pin",
             R"pin(          <7>│    │                                     │         │     │          │        │)pin",
             R"pin(          <7>│    │                                     │         │     │          │        │)pin",
             R"pin(          <7>│    │         ┌────────────────────────────────────────────────┐     │        │)pin",
             R"pin(          <7>│    │         │<9>Safe Mode Rule: <5>zombie<7:4>_<7>_______________________  │     │        │)pin",
             R"pin(          <7>│    │         └────────────────────────────────────────────────┘     │        │)pin",
             R"pin(          <7>│    │   ┌──────────────────────────────────────────────────────────┐ │        │)pin",
             R"pin(          <7>│    │   │<15>* is used as a Wildcard.  A few Examples:                 <7>│ │        │)pin",
             R"pin(          <7>│    │   │                                                          │ │        │)pin",
             R"pin(          <7>│    │   │<15>human          matches every NPC                          <7>│ │        │)pin",
             R"pin(          <7>│    │   │<15>zombie         matches the monster name exactly           <7>│ │        │)pin",
             R"pin(          <7>│    │   │<15>acidic zo*     matches monsters beginning with 'acidic zo'<7>│ │        │)pin",
             R"pin(          <7>│    │   │<15>*mbie          matches monsters ending with 'mbie'        <7>│ │        │)pin",
             R"pin(          <7>│    │   │<15>*cid*zo*ie     multiple * are allowed                     <7>│ │        │)pin",
             R"pin(          <7>│    │   │<15>AcI*zO*iE      case insensitive search                    <7>│ │        │)pin",
             R"pin(          <7>│    │   └──────────────────────────────────────────────────────────┘ │        │)pin",
             R"pin(          <7>└────┴─────────────────────────────────────┴─────────┴─────┴──────────┴────────┘)pin",
         }},
        {"safemode:11",
         {
             R"pin(          <7>┌───────────────────────────── <9>SAFE MODE MANAGER <7>──────────────────────────────┐)pin",
             R"pin(          <7>│                                                                              │)pin",
             R"pin(          <7>│                                                                              │)pin",
             R"pin(          <7>├                                                                              ┤)pin",
             R"pin(          <7>│                                                                              │)pin",
             R"pin(          <7>│ <15>1  <7>│<11>><15:4><empty rule>                        <7>│ <15>Neutral <7>│ <15>10  <7>│ <15>Blacklist<7>│ <15>Hostile<7>│)pin",
             R"pin(          <7>│ <15>2  <7>│ <15>*                                   <7>│ <15>Any     <7>│ <15>--- <7>│ <15>Whitelist<7>│ <15>Hostile<7>│)pin",
             R"pin(          <7>│ 3  │ a loud*                             │ ---     │ 5   │ Whitelist│ Sound  │)pin",
             R"pin(          <7>│    │                                     │         │     │          │        │)pin",
             R"pin(          <7>│    │                                     │         │     │          │        │)pin",
             R"pin(          <7>│    │                                     │         │     │          │        │)pin",
             R"pin(          <7>│    │                                     │         │     │          │        │)pin",
             R"pin(          <7>│    │                                     │         │     │          │        │)pin",
             R"pin(          <7>│    │                                     │         │     │          │        │)pin",
             R"pin(          <7>│    │                                     │         │     │          │        │)pin",
             R"pin(          <7>│    │                                     │         │     │          │        │)pin",
             R"pin(          <7>│    │                                     │         │     │          │        │)pin",
             R"pin(          <7>│    │                                     │         │     │          │        │)pin",
             R"pin(          <7>│    │                                     │         │     │          │        │)pin",
             R"pin(          <7>│    │                                     │         │     │          │        │)pin",
             R"pin(          <7>│    │                                     │         │     │          │        │)pin",
             R"pin(          <7>│    │                                     │         │     │          │        │)pin",
             R"pin(          <7>│    │                                     │         │     │          │        │)pin",
             R"pin(          <7>└────┴─────────────────────────────────────┴─────────┴─────┴──────────┴────────┘)pin",
         }},
        {"safemode:12",
         {
             R"pin(          <7>┌───────────────────────────── <9>SAFE MODE MANAGER <7>──────────────────────────────┐)pin",
             R"pin(          <7>│                                                                              │)pin",
             R"pin(          <7>│                                                                              │)pin",
             R"pin(          <7>├                                                                              ┤)pin",
             R"pin(          <7>│                                                                              │)pin",
             R"pin(          <7>│ <15>1  <7>│ <15><empty rule>                        <7>│ <15>Neutral <7>│ <15>10  <7>│ <15>Blacklist<7>│ <15>Hostile<7>│)pin",
             R"pin(          <7>│ <15>2  <7>│<11>><15:4>*                                   <7>│ <15>Any     <7>│ <15>--- <7>│ <15>Whitelist<7>│ <15>Hostile<7>│)pin",
             R"pin(          <7>│ 3  │ a loud*                             │ ---     │ 5   │ Whitelist│ Sound  │)pin",
             R"pin(          <7>│    │                                     │         │     │          │        │)pin",
             R"pin(          <7>│    │                                     │         │     │          │        │)pin",
             R"pin(          <7>│    │                                     │         │     │          │        │)pin",
             R"pin(          <7>│    │                                     │         │     │          │        │)pin",
             R"pin(          <7>│    │                                     │         │     │          │        │)pin",
             R"pin(          <7>│    │                                     │         │     │          │        │)pin",
             R"pin(          <7>│    │                                     │         │     │          │        │)pin",
             R"pin(          <7>│    │                                     │         │     │          │        │)pin",
             R"pin(          <7>│    │                                     │         │     │          │        │)pin",
             R"pin(          <7>│    │                                     │         │     │          │        │)pin",
             R"pin(          <7>│    │                                     │         │     │          │        │)pin",
             R"pin(          <7>│    │                                     │         │     │          │        │)pin",
             R"pin(          <7>│    │                                     │         │     │          │        │)pin",
             R"pin(          <7>│    │                                     │         │     │          │        │)pin",
             R"pin(          <7>│    │                                     │         │     │          │        │)pin",
             R"pin(          <7>└────┴─────────────────────────────────────┴─────────┴─────┴──────────┴────────┘)pin",
         }},
        {"safemode:13",
         {
             R"pin(          <7>┌───────────────────────────── <9>SAFE MODE MANAGER <7>──────────────────────────────┐)pin",
             R"pin(          <7>│                                                                              │)pin",
             R"pin(          <7>│                                                                              │)pin",
             R"pin(          <7>├                                                                              ┤)pin",
             R"pin(          <7>│                                                                              │)pin",
             R"pin(          <7>│ <15>1  <7>│ <15><empty rule>                        <7>│ <15>Neutral <7>│ <15>10  <7>│ <15>Blacklist<7>│ <15>Hostile<7>│)pin",
             R"pin(          <7>│ <15>2  <7>│<11>><15:4>*                                   <7>│ <15>Any     <7>│ <15>--- <7>│ <15>Whitelist<7>│ <15>Hostile<7>│)pin",
             R"pin(          <7>│ 3  │ a loud*                             │ ---     │ 5   │ Whitelist│ Sound  │)pin",
             R"pin(          <7>│    │                                     │         │     │          │        │)pin",
             R"pin(          <7>│    │                                     │         │     │          │        │)pin",
             R"pin(          <7>│    │         ┌────────────────────────────────────────────────┐     │        │)pin",
             R"pin(          <7>│    │         │<9>Safe Mode Rule: <5>*<7:4>_<7>____________________________  │     │        │)pin",
             R"pin(          <7>│    │         └────────────────────────────────────────────────┘     │        │)pin",
             R"pin(          <7>│    │   ┌──────────────────────────────────────────────────────────┐ │        │)pin",
             R"pin(          <7>│    │   │<15>* is used as a Wildcard.  A few Examples:                 <7>│ │        │)pin",
             R"pin(          <7>│    │   │                                                          │ │        │)pin",
             R"pin(          <7>│    │   │<15>human          matches every NPC                          <7>│ │        │)pin",
             R"pin(          <7>│    │   │<15>zombie         matches the monster name exactly           <7>│ │        │)pin",
             R"pin(          <7>│    │   │<15>acidic zo*     matches monsters beginning with 'acidic zo'<7>│ │        │)pin",
             R"pin(          <7>│    │   │<15>*mbie          matches monsters ending with 'mbie'        <7>│ │        │)pin",
             R"pin(          <7>│    │   │<15>*cid*zo*ie     multiple * are allowed                     <7>│ │        │)pin",
             R"pin(          <7>│    │   │<15>AcI*zO*iE      case insensitive search                    <7>│ │        │)pin",
             R"pin(          <7>│    │   └──────────────────────────────────────────────────────────┘ │        │)pin",
             R"pin(          <7>└────┴─────────────────────────────────────┴─────────┴─────┴──────────┴────────┘)pin",
         }},
        {"safemode:14",
         {
             R"pin(          <7>┌───────────────────────────── <9>SAFE MODE MANAGER <7>──────────────────────────────┐)pin",
             R"pin(          <7>│                                                                              │)pin",
             R"pin(          <7>│                                                                              │)pin",
             R"pin(          <7>├                                                                              ┤)pin",
             R"pin(          <7>│                                                                              │)pin",
             R"pin(          <7>│ <15>1  <7>│ <15><empty rule>                        <7>│ <15>Neutral <7>│ <15>10  <7>│ <15>Blacklist<7>│ <15>Hostile<7>│)pin",
             R"pin(          <7>│ <15>2  <7>│<11>><15:4><empty rule>                        <7>│ <15>Any     <7>│ <15>--- <7>│ <15>Whitelist<7>│ <15>Hostile<7>│)pin",
             R"pin(          <7>│ 3  │ a loud*                             │ ---     │ 5   │ Whitelist│ Sound  │)pin",
             R"pin(          <7>│    │                                     │         │     │          │        │)pin",
             R"pin(          <7>│    │                                     │         │     │          │        │)pin",
             R"pin(          <7>│    │                                     │         │     │          │        │)pin",
             R"pin(          <7>│    │                                     │         │     │          │        │)pin",
             R"pin(          <7>│    │                                     │         │     │          │        │)pin",
             R"pin(          <7>│    │                                     │         │     │          │        │)pin",
             R"pin(          <7>│    │                                     │         │     │          │        │)pin",
             R"pin(          <7>│    │                                     │         │     │          │        │)pin",
             R"pin(          <7>│    │                                     │         │     │          │        │)pin",
             R"pin(          <7>│    │                                     │         │     │          │        │)pin",
             R"pin(          <7>│    │                                     │         │     │          │        │)pin",
             R"pin(          <7>│    │                                     │         │     │          │        │)pin",
             R"pin(          <7>│    │                                     │         │     │          │        │)pin",
             R"pin(          <7>│    │                                     │         │     │          │        │)pin",
             R"pin(          <7>│    │                                     │         │     │          │        │)pin",
             R"pin(          <7>└────┴─────────────────────────────────────┴─────────┴─────┴──────────┴────────┘)pin",
         }},
        {"safemode:15",
         {
             R"pin(          <7>┌───────────────────────────── <9>SAFE MODE MANAGER <7>──────────────────────────────┐)pin",
             R"pin(          <7>│                                                                              │)pin",
             R"pin(          <7>│                                                                              │)pin",
             R"pin(          <7>├                                                                              ┤)pin",
             R"pin(          <7>│                                                                              │)pin",
             R"pin(          <7>│ <15>1  <7>│ <15><empty rule>                        <7>│ <15>Neutral <7>│ <15>10  <7>│ <15>Blacklist<7>│ <15>Hostile<7>│)pin",
             R"pin(          <7>│ <15>2  <7>│<11>><15:4><empty rule>                        <7>│ <15>Any     <7>│ <15>--- <7>│ <15>Whitelist<7>│ <15>Hostile<7>│)pin",
             R"pin(          <7>│ 3  │ a loud*                             │ ---     │ 5   │ Whitelist│ Sound  │)pin",
             R"pin(          <7>│    │                                     │         │     │          │        │)pin",
             R"pin(          <7>│    │                  ┌──────────────────────────────┐   │          │        │)pin",
             R"pin(          <7>│    │                  │<9>Save changes? (Case Sensitive)<7>│   │          │        │)pin",
             R"pin(          <7>│    │                  │                              │   │          │        │)pin",
             R"pin(          <7>│    │                  │                   <15>(Y)es  <15:4>(N)o<7>│   │          │        │)pin",
             R"pin(          <7>│    │                  └──────────────────────────────┘   │          │        │)pin",
             R"pin(          <7>│    │                                     │         │     │          │        │)pin",
             R"pin(          <7>│    │                                     │         │     │          │        │)pin",
             R"pin(          <7>│    │                                     │         │     │          │        │)pin",
             R"pin(          <7>│    │                                     │         │     │          │        │)pin",
             R"pin(          <7>│    │                                     │         │     │          │        │)pin",
             R"pin(          <7>│    │                                     │         │     │          │        │)pin",
             R"pin(          <7>│    │                                     │         │     │          │        │)pin",
             R"pin(          <7>│    │                                     │         │     │          │        │)pin",
             R"pin(          <7>│    │                                     │         │     │          │        │)pin",
             R"pin(          <7>└────┴─────────────────────────────────────┴─────────┴─────┴──────────┴────────┘)pin",
         }},
        {"safemode_empty:0",
         {
             R"pin(          <7>┌───────────────────────────── <9>SAFE MODE MANAGER <7>──────────────────────────────┐)pin",
             R"pin(          <7>│                                                                              │)pin",
             R"pin(          <7>│                                                                              │)pin",
             R"pin(          <7>├                                                                              ┤)pin",
             R"pin(          <7>│                                                                              │)pin",
             R"pin(          <7>│    │                                     │         │     │          │        │)pin",
             R"pin(          <7>│    │                                     │         │     │          │        │)pin",
             R"pin(          <7>│    │                                     │         │     │          │        │)pin",
             R"pin(          <7>│    │                                     │         │     │          │        │)pin",
             R"pin(          <7>│    │                                     │         │     │          │        │)pin",
             R"pin(          <7>│    │                                     │         │     │          │        │)pin",
             R"pin(          <7>│    │                                     │         │     │          │        │)pin",
             R"pin(          <7>│    │                                     │         │     │          │        │)pin",
             R"pin(          <7>│    │          <15>Safe Mode manager currently inactive.<7>│     │          │        │)pin",
             R"pin(          <7>│    │          <15>Default rules are used.  Add a rule to activate.      <7>│        │)pin",
             R"pin(          <7>│    │          <15>Press ~ to add a default ruleset to get started.      <7>│        │)pin",
             R"pin(          <7>│    │                                     │         │     │          │        │)pin",
             R"pin(          <7>│    │                                     │         │     │          │        │)pin",
             R"pin(          <7>│    │                                     │         │     │          │        │)pin",
             R"pin(          <7>│    │                                     │         │     │          │        │)pin",
             R"pin(          <7>│    │                                     │         │     │          │        │)pin",
             R"pin(          <7>│    │                                     │         │     │          │        │)pin",
             R"pin(          <7>│    │                                     │         │     │          │        │)pin",
             R"pin(          <7>└────┴─────────────────────────────────────┴─────────┴─────┴──────────┴────────┘)pin",
         }},
        {"safemode_empty:1",
         {
             R"pin(          <7>┌───────────────────────────── <9>SAFE MODE MANAGER <7>──────────────────────────────┐)pin",
             R"pin(          <7>│                                                                              │)pin",
             R"pin(          <7>│                                                                              │)pin",
             R"pin(          <7>├                                                                              ┤)pin",
             R"pin(          <7>│                                                                              │)pin",
             R"pin(          <7>│    │                                     │         │     │          │        │)pin",
             R"pin(          <7>│    │                                     │         │     │          │        │)pin",
             R"pin(          <7>│    │                                     │         │     │          │        │)pin",
             R"pin(          <7>│    │                                     │         │     │          │        │)pin",
             R"pin(          <7>│    │                                     │         │     │          │        │)pin",
             R"pin(          <7>│    │                                     │         │     │          │        │)pin",
             R"pin(          <7>│    │                                     │         │     │          │        │)pin",
             R"pin(          <7>│    │                                     │         │     │          │        │)pin",
             R"pin(          <7>│    │          <15>Safe Mode manager currently inactive.<7>│     │          │        │)pin",
             R"pin(          <7>│    │          <15>Default rules are used.  Add a rule to activate.      <7>│        │)pin",
             R"pin(          <7>│    │          <15>Press ~ to add a default ruleset to get started.      <7>│        │)pin",
             R"pin(          <7>│    │                                     │         │     │          │        │)pin",
             R"pin(          <7>│    │                                     │         │     │          │        │)pin",
             R"pin(          <7>│    │                                     │         │     │          │        │)pin",
             R"pin(          <7>│    │                                     │         │     │          │        │)pin",
             R"pin(          <7>│    │                                     │         │     │          │        │)pin",
             R"pin(          <7>│    │                                     │         │     │          │        │)pin",
             R"pin(          <7>│    │                                     │         │     │          │        │)pin",
             R"pin(          <7>└────┴─────────────────────────────────────┴─────────┴─────┴──────────┴────────┘)pin",
         }},
        {"safemode_nobody:0",
         {
             R"pin(          <7>┌───────────────────────────── <9>SAFE MODE MANAGER <7>──────────────────────────────┐)pin",
             R"pin(          <7>│                                                                              │)pin",
             R"pin(          <7>│                                                                              │)pin",
             R"pin(          <7>├                                                                              ┤)pin",
             R"pin(          <7>│                                                                              │)pin",
             R"pin(          <7>│    │                                     │         │     │          │        │)pin",
             R"pin(          <7>│    │                                     │         │     │          │        │)pin",
             R"pin(          <7>│    │                                     │         │     │          │        │)pin",
             R"pin(          <7>│    │                                     │         │     │          │        │)pin",
             R"pin(          <7>│    │                                     │         │     │          │        │)pin",
             R"pin(          <7>│    │                                     │         │     │          │        │)pin",
             R"pin(          <7>│    │                                     │         │     │          │        │)pin",
             R"pin(          <7>│    │                                     │         │     │          │        │)pin",
             R"pin(          <7>│    │          <15>Safe Mode manager currently inactive.<7>│     │          │        │)pin",
             R"pin(          <7>│    │          <15>Default rules are used.  Add a rule to activate.      <7>│        │)pin",
             R"pin(          <7>│    │          <15>Press ~ to add a default ruleset to get started.      <7>│        │)pin",
             R"pin(          <7>│    │                                     │         │     │          │        │)pin",
             R"pin(          <7>│    │                                     │         │     │          │        │)pin",
             R"pin(          <7>│    │                                     │         │     │          │        │)pin",
             R"pin(          <7>│    │                                     │         │     │          │        │)pin",
             R"pin(          <7>│    │                                     │         │     │          │        │)pin",
             R"pin(          <7>│    │                                     │         │     │          │        │)pin",
             R"pin(          <7>│    │                                     │         │     │          │        │)pin",
             R"pin(          <7>└────┴─────────────────────────────────────┴─────────┴─────┴──────────┴────────┘)pin",
         }},
        {"safemode_nobody:1",
         {
             R"pin(          <7>┌───────────────────────────── <9>SAFE MODE MANAGER <7>──────────────────────────────┐)pin",
             R"pin(          <7>│                                                                              │)pin",
             R"pin(          <7>│                                                                              │)pin",
             R"pin(          <7>├                                                                              ┤)pin",
             R"pin(          <7>│                                                                              │)pin",
             R"pin(          <7>│    │                                     │         │     │          │        │)pin",
             R"pin(          <7>│    │                                     │         │     │          │        │)pin",
             R"pin(          <7>│    │                                     │         │     │          │        │)pin",
             R"pin(          <7>│    │                                     │         │     │          │        │)pin",
             R"pin(          <7>│    │                                     │         │     │          │        │)pin",
             R"pin(          <7>│    │                                     │         │     │          │        │)pin",
             R"pin(          <7>│    │                                     │         │     │          │        │)pin",
             R"pin(          <7>│    │                                     │         │     │          │        │)pin",
             R"pin(          <7>│    │          <15>Please load a character first to use this page!       <7>│        │)pin",
             R"pin(          <7>│    │                                     │         │     │          │        │)pin",
             R"pin(          <7>│    │                                     │         │     │          │        │)pin",
             R"pin(          <7>│    │                                     │         │     │          │        │)pin",
             R"pin(          <7>│    │                                     │         │     │          │        │)pin",
             R"pin(          <7>│    │                                     │         │     │          │        │)pin",
             R"pin(          <7>│    │                                     │         │     │          │        │)pin",
             R"pin(          <7>│    │                                     │         │     │          │        │)pin",
             R"pin(          <7>│    │                                     │         │     │          │        │)pin",
             R"pin(          <7>│    │                                     │         │     │          │        │)pin",
             R"pin(          <7>└────┴─────────────────────────────────────┴─────────┴─────┴──────────┴────────┘)pin",
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

/// Every distraction enabled, whatever the profile holds; the profile's own are put back.
struct distractions_guard {
    const std::map<distraction_type, bool> saved = get_distraction_manager().distractions;

    distractions_guard() {
        for (auto i = 0; i < static_cast<int>(distraction_type::num_distraction_type); ++i) {
            get_distraction_manager().distractions[static_cast<distraction_type>(i)] = false;
        }
    }

    ~distractions_guard() { get_distraction_manager().distractions = saved; }
};

} // namespace

TEST_CASE("the native distraction manager keeps its text and colors", "[rules_native_pin]") {
    const auto guard = screen_guard{};
    const auto distractions = distractions_guard{};
    auto step = 0;
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        // The first row, the cursor on the second, then that one switched off.
        CHECK(matches(std::format("distractions:{}", step), rendered()));
        switch (step++) {
            case 0:
                return act("DOWN");
            case 1:
                return act("CONFIRM");
            default:
                return act("QUIT");
        }
    });
    get_distraction_manager().show();
    CHECK(step == 3);
}

TEST_CASE("the native colour manager keeps its text and colors", "[rules_native_pin]") {
    const auto guard = screen_guard{};
    // A manager of its own, with two custom colours, so the profile's colours stay as they are.
    const auto custom = std::filesystem::path{PATH_INFO::user_dir()} / "rules_pin_colors.json";
    {
        auto out = std::ofstream{custom};
        out << R"([{"name":"c_black","custom":"c_red","invertcustom":"c_light_green"}])";
    }
    auto colors = color_manager{};
    colors.load_default();
    colors.load_custom(custom.string());
    std::filesystem::remove(custom);
    auto step = 0;
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        // The first row, the cursor on the second, then in the other column.
        CHECK(matches(std::format("colors:{}", step), rendered()));
        switch (step++) {
            case 0:
                return act("DOWN");
            case 1:
                return act("RIGHT");
            default:
                return act("QUIT");
        }
    });
    colors.show_gui();
    CHECK(step == 3);
}

namespace {

/// The character's name for the screen, put back after it.
struct name_guard {
    const std::string saved = get_avatar().name;

    explicit name_guard(const std::string& name) { get_avatar().name = name; }

    ~name_guard() { get_avatar().name = saved; }
};

/// A file under the test profile written with `content` and removed after.
struct profile_file {
    const std::filesystem::path path;

    profile_file(const std::string& named, const std::string& content): path(named) {
        REQUIRE(named.starts_with(PATH_INFO::user_dir()));
        std::filesystem::create_directories(path.parent_path());
        auto out = std::ofstream{path};
        out << content;
    }

    ~profile_file() { std::filesystem::remove(path); }
};

constexpr auto pickup_rules =
    R"([{"rule":"*knife*","active":true,"exclude":false},)"
    R"({"rule":"","active":false,"exclude":false},)"
    R"({"rule":"sledge hammer","active":true,"exclude":true}])";
constexpr auto safemode_rules =
    R"([{"rule":"zombie","active":true,"whitelist":false,"attitude":0,"proximity":10,"category":0},)"
    R"({"rule":"*","active":true,"whitelist":true,"attitude":3,"proximity":0,"category":0},)"
    R"({"rule":"a loud*","active":false,"whitelist":true,"attitude":0,"proximity":5,"category":1}])";

/// What a client sends to a popup: leaves it for `cancel`, else picks the option so labelled.
auto answer(const std::string& token) -> input_event {
    const auto snapshot = game_client::current_interaction({.limit = 200});
    auto input = game_client::input_command{};
    if (token == "cancel") {
        input.interaction = game_client::interaction_command{
            .input_id = snapshot.input_id, .operation = game_client::interaction_operation::cancel};
    } else {
        const auto option =
            std::ranges::find(snapshot.choices, token, &game_client::interaction_choice::label);
        REQUIRE(option != snapshot.choices.end());
        input.interaction = game_client::interaction_command{
            .input_id = snapshot.input_id,
            .operation = game_client::interaction_operation::choose,
            .target_id = option->id};
    }
    const auto result =
        game_client::resolve_input_command(input, game_client::memory::screen_size());
    REQUIRE(result.has_value());
    return *result;
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

} // namespace

TEST_CASE("the native auto pickup manager keeps its text and colors", "[rules_native_pin]") {
    const auto guard = screen_guard{};
    const auto named = name_guard{"Tester"};
    const auto file = profile_file{PATH_INFO::autopickup(), pickup_rules};
    auto settings = auto_pickup::player_settings{};
    settings.load_global();
    // A rule added and its text entry left, the cursor down and in the other column, the empty
    // character page and back, then the match list of the last rule.
    const auto keys = std::vector<std::string>{
        "ADD_RULE", "!cancel", "DOWN",      "RIGHT", "NEXT_TAB", "PREV_TAB",
        "DOWN",     "DOWN",    "TEST_RULE", "DOWN",  "QUIT",     "QUIT"};
    const auto steps = press("autopickup", keys);
    settings.show();
    CHECK(*steps == keys.size());
}

TEST_CASE(
    "the native auto pickup manager of a character keeps its text and colors",
    "[rules_native_pin]") {
    const auto guard = screen_guard{};
    const auto named = name_guard{"Tester"};
    auto settings = auto_pickup::npc_settings{};
    auto in = std::istringstream{pickup_rules};
    auto jsin = JsonIn{in};
    settings.deserialize(jsin);
    const auto keys = std::vector<std::string>{"DOWN", "QUIT"};
    const auto steps = press("autopickup_npc", keys);
    settings.show("Bob");
    CHECK(*steps == keys.size());
}

TEST_CASE("the native safe mode manager keeps its text and colors", "[rules_native_pin]") {
    const auto guard = screen_guard{};
    const auto named = name_guard{"Tester"};
    auto rules = safemode{};
    auto in = std::istringstream{safemode_rules};
    auto jsin = JsonIn{in};
    rules.deserialize(jsin);
    // The match list of the first rule, the cursor moved, the empty character page and back, the
    // attitude of a rule changed, the text entry of a hostile and of a sound rule left, and the
    // changes refused.
    const auto keys = std::vector<std::string>{
        "TEST_RULE", "DOWN",    "QUIT",    "RIGHT", "DOWN",    "NEXT_TAB", "PREV_TAB", "CONFIRM",
        "LEFT",      "CONFIRM", "!cancel", "DOWN",  "CONFIRM", "!cancel",  "QUIT",     "!No"};
    const auto steps = press("safemode", keys);
    rules.show();
    CHECK(*steps == keys.size());
}

TEST_CASE(
    "the native safe mode manager without rules keeps its text and colors", "[rules_native_pin]") {
    const auto guard = screen_guard{};
    auto rules = safemode{};
    // The inactive notice on both pages, and the character page of a game that has no character.
    const auto keys = std::vector<std::string>{"NEXT_TAB", "QUIT"};
    {
        const auto named = name_guard{"Tester"};
        const auto steps = press("safemode_empty", keys);
        rules.show();
        CHECK(*steps == keys.size());
    }
    const auto named = name_guard{""};
    const auto steps = press("safemode_nobody", keys);
    rules.show();
    CHECK(*steps == keys.size());
}

#endif // CATA_MCP
