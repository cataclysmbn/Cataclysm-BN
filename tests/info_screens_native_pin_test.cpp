#if defined(CATA_MCP)

#    include "achievement.h"
#    include "avatar.h"
#    include "catch/catch.hpp"
#    include "client_command.h"
#    include "client_input.h"
#    include "client_memory.h"
#    include "client_memory_scope.h"
#    include "client_snapshot.h"
#    include "cursesdef.h"
#    include "event.h"
#    include "faction.h"
#    include "game.h"
#    include "game_constants.h"
#    include "help.h"
#    include "input.h"
#    include "kill_tracker.h"
#    include "map_helpers.h"
#    include "mission.h"
#    include "morale_types.h"
#    include "npc.h"
#    include "output.h"
#    include "player_helpers.h"
#    include "scores_ui.h"
#    include "state_helpers.h"
#    include "stats_tracker.h"

#    include <format>
#    include <map>
#    include <string>
#    include <vector>

/// Pins what the native help, morale, mission, faction and score screens print, so that sharing
/// their fact gathering with the engine client keeps the player's screens byte-equal: every row
/// with its text and colour runs.
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
        clear_map();
        clear_character(get_avatar(), false);
        test_mode = false;
        TERMX = 100;
        TERMY = 60;
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

/// The key a player types.
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
        {"help:topics",
         {
             R"pin(          <7>┌──────────────────────────────────── <0:7>HELP <7>────────────────────────────────────┐)pin",
             R"pin(          <7>│ <15>Please press one of the following for help on that topic:                    <7>│)pin",
             R"pin(          <7>│ <15>Press ESC to return to the game.                                             <7>│)pin",
             R"pin(          <7>│                                                                              │)pin",
             R"pin(          <7>│ <12>a<15>: Introduction                       <12>n<15>: Items overview                      <7>│)pin",
             R"pin(          <7>│ <12>b<15>: Movement                           <12>o<15>: Combat                              <7>│)pin",
             R"pin(          <7>│ <12>c<15>: Viewing                            <12>p<15>: Martial arts styles                 <7>│)pin",
             R"pin(          <7>│ <12>d<15>: Interaction                        <12>q<15>: Survival tips                       <7>│)pin",
             R"pin(          <7>│ <12>e<15>: Hunger, thirst, and sleep          <12>r<15>: Vehicles and Driving                <7>│)pin",
             R"pin(          <7>│ <12>f<15>: Pain and stimulants                <12>1<15>: Electricity                         <7>│)pin",
             R"pin(          <7>│ <12>g<15>: Healing and medication             <12>2<15>: List of item types and data         <7>│)pin",
             R"pin(          <7>│ <12>h<15>: Addiction                          <12>3<15>: Description of map symbols          <7>│)pin",
             R"pin(          <7>│ <12>i<15>: Morale and learning                <12>4<15>: Description of gun types            <7>│)pin",
             R"pin(          <7>│ <12>j<15>: Radioactivity and mutation         <12>5<15>: FAQ (contains spoilers!)            <7>│)pin",
             R"pin(          <7>│ <12>k<15>: Bionics                            <12>6<15>: Resolved game directories           <7>│)pin",
             R"pin(          <7>│ <12>l<15>: Crafting                                                                  <7>│)pin",
             R"pin(          <7>│ <12>m<15>: Traps                                                                     <7>│)pin",
             R"pin(          <7>│                                                                              │)pin",
             R"pin(          <7>│                                                                              │)pin",
             R"pin(          <7>│                                                                              │)pin",
             R"pin(          <7>│                                                                              │)pin",
             R"pin(          <7>│                                                                              │)pin",
             R"pin(          <7>│                                                                              │)pin",
             R"pin(          <7>└──────────────────────────────────────────────────────────────────────────────┘)pin",
         }},
        {"help:topic",
         {
             R"pin(          <7>┌──────────────────────────────────── <0:7>HELP <7>────────────────────────────────────┐)pin",
             R"pin(          <7>│<15>Movement is performed using the numpad, or vikeys.                            <10>^)pin",
             R"pin(          <7>│                                                                              <6:6>│)pin",
             R"pin(          <7>│<12>y  k  u   7  8  9                                                             <6:6>│)pin",
             R"pin(          <7>│ <15>\ | /     \ | /                                                              <6:6>│)pin",
             R"pin(          <7>│  <15>\|/       \|/                                                               <6:6>│)pin",
             R"pin(          <7>│<12>h<15>--<12>.<15>--<12>l   4<15>--<12>5<15>--<12>6                                                             <6:6>│)pin",
             R"pin(          <7>│  <15>/|\       /|\                                                               <6:6>│)pin",
             R"pin(          <7>│ <15>/ | \     / | \                                                              <6:6>│)pin",
             R"pin(          <7>│<12>b  j  n   1  2  3                                                             <6:6>│)pin",
             R"pin(          <7>│                                                                              <6:6>│)pin",
             R"pin(          <7>│<15>Each step will take 100 movement points (or more, depending on the terrain);  <6:6>│)pin",
             R"pin(          <7>│<15>you will then replenish a variable amount of movement points, depending on    <6:6>│)pin",
             R"pin(          <7>│<15>many factors (press <12>@ <15>to see the exact amount).                               <6:6>│)pin",
             R"pin(          <7>│                                                                              <6:6>│)pin",
             R"pin(          <7>│<15>To attempt to hit a monster with your weapon, simply move into it.            <6:6>│)pin",
             R"pin(          <7>│                                                                              <6:6>│)pin",
             R"pin(          <7>│<15>You may find doors, ('+'); these may be opened with <12>o <15>or by walking into them,│)pin",
             R"pin(          <7>│<15>and closed with <12>c<15>.  Some doors are locked.  Locked doors, windows, and some   │)pin",
             R"pin(          <7>│<15>other obstacles can be destroyed by smashing them (<12>s<15>, then choose a direction)│)pin",
             R"pin(          <7>│<15>or using a mining tool such as a jackhammer or pickaxe.  Smashing down        │)pin",
             R"pin(          <7>│<15>obstacles is much easier with a good weapon or a strong character, mining     │)pin",
             R"pin(          <7>│<15>takes time, and both cause a lot of noise, so be careful there aren't any     <10>v)pin",
             R"pin(          <7>└──────────────────────────────────────────────────────────────────────────────┘)pin",
         }},
        {"morale",
         {
             R"pin(                              <7>┌──────────────────────────────────────┐)pin",
             R"pin(                              <7>│ <15>Morale                               <7>│)pin",
             R"pin(                              <7>├──────────────────────────────────────┤)pin",
             R"pin(                              <7>│ Source                         Value │)pin",
             R"pin(                              <7>│ <2>Total positive morale            +20 <7>│)pin",
             R"pin(                              <7>│ <2>Enjoyed apple                   100% <7>│)pin",
             R"pin(                              <7>│                                      │)pin",
             R"pin(                              <7>│ <9>Total negative morale            -10 <7>│)pin",
             R"pin(                              <7>│ <9>Disliked bread                  100% <7>│)pin",
             R"pin(                              <7>│                                      │)pin",
             R"pin(                              <7>│                                      │)pin",
             R"pin(                              <7>│                                      │)pin",
             R"pin(                              <7>│                                      │)pin",
             R"pin(                              <7>│                                      │)pin",
             R"pin(                              <7>│                                      │)pin",
             R"pin(                              <7>│                                      │)pin",
             R"pin(                              <7>│                                      │)pin",
             R"pin(                              <7>│                                      │)pin",
             R"pin(                              <7>│                                      │)pin",
             R"pin(                              <7>│                                      │)pin",
             R"pin(                              <7>├──────────────────────────────────────┤)pin",
             R"pin(                              <7>│ <2>Total morale:                    +10 <7>│)pin",
             R"pin(                              <7>│ <15>Focus trends towards:            110 <7>│)pin",
             R"pin(                              <7>└──────────────────────────────────────┘)pin",
         }},
        {"missions:active",
         {
             R"pin(            <7>┌───────────────┐ ┌──────────────────┐ ┌───────────────┐)pin",
             R"pin(           <7:4><<7>│<7:4>ACTIVE MISSIONS<7>│<7:4>><7>│COMPLETED MISSIONS│ │FAILED MISSIONS│)pin",
             R"pin(          <7>┌─┘               └─┴─────────┬────────┴─┴───────────────┴─────────────────────┐)pin",
             R"pin(          <7>│<10:4>Missing Caravan              <7>│<10>Missing Caravan                                 <7>│)pin",
             R"pin(          <7>│                             │                                                │)pin",
             R"pin(          <7>│                             │<15>Investigate the last known location of a        <7>│)pin",
             R"pin(          <7>│                             │<15>disappeared scavenger trading party, and        <7>│)pin",
             R"pin(          <7>│                             │<15>eliminate any bandits or raiders in the         <7>│)pin",
             R"pin(          <7>│                             │<15>vicinity in exchange for 30 certified notes.    <7>│)pin",
             R"pin(          <7>│                             │                                                │)pin",
             R"pin(          <7>│                             │                                                │)pin",
             R"pin(          <7>│                             │                                                │)pin",
             R"pin(          <7>│                             │                                                │)pin",
             R"pin(          <7>│                             │                                                │)pin",
             R"pin(          <7>│                             │                                                │)pin",
             R"pin(          <7>│                             │                                                │)pin",
             R"pin(          <7>│                             │                                                │)pin",
             R"pin(          <7>│                             │                                                │)pin",
             R"pin(          <7>│                             │                                                │)pin",
             R"pin(          <7>│                             │                                                │)pin",
             R"pin(          <7>│                             │                                                │)pin",
             R"pin(          <7>│                             │                                                │)pin",
             R"pin(          <7>│                             │                                                │)pin",
             R"pin(          <7>└─────────────────────────────┴────────────────────────────────────────────────┘)pin",
         }},
        {"missions:completed",
         {
             R"pin(            <7>┌───────────────┐ ┌──────────────────┐ ┌───────────────┐)pin",
             R"pin(            <7>│ACTIVE MISSIONS│<7:4><<7>│<7:4>COMPLETED MISSIONS<7>│<7:4>><7>│FAILED MISSIONS│)pin",
             R"pin(          <7>┌─┴───────────────┴─┘                  └─┴───────────────┴─────────────────────┐)pin",
             R"pin(          <7>│                             │                                                │)pin",
             R"pin(          <7>│                             │<9>You haven't completed any missions!             <7>│)pin",
             R"pin(          <7>│                             │                                                │)pin",
             R"pin(          <7>│                             │                                                │)pin",
             R"pin(          <7>│                             │                                                │)pin",
             R"pin(          <7>│                             │                                                │)pin",
             R"pin(          <7>│                             │                                                │)pin",
             R"pin(          <7>│                             │                                                │)pin",
             R"pin(          <7>│                             │                                                │)pin",
             R"pin(          <7>│                             │                                                │)pin",
             R"pin(          <7>│                             │                                                │)pin",
             R"pin(          <7>│                             │                                                │)pin",
             R"pin(          <7>│                             │                                                │)pin",
             R"pin(          <7>│                             │                                                │)pin",
             R"pin(          <7>│                             │                                                │)pin",
             R"pin(          <7>│                             │                                                │)pin",
             R"pin(          <7>│                             │                                                │)pin",
             R"pin(          <7>│                             │                                                │)pin",
             R"pin(          <7>│                             │                                                │)pin",
             R"pin(          <7>│                             │                                                │)pin",
             R"pin(          <7>└─────────────────────────────┴────────────────────────────────────────────────┘)pin",
         }},
        {"factions:followers",
         {
             R"pin(            <7>┌──────────────┐ ┌──────────────┐ ┌────┐ ┌─────────┐)pin",
             R"pin(           <7:4><<7>│<7:4>YOUR FOLLOWERS<7>│<7:4>><7>│OTHER FACTIONS│ │LORE│ │CREATURES│)pin",
             R"pin(          <7>┌─┘              └─┴────────── ───┴─┴────┴─┴─────────┴─────────────────────────┐)pin",
             R"pin(          <7>│<15:4>Tom 'Tom' Tom, Tester        <7>│Press enter to talk to this follower            │)pin",
             R"pin(          <7>│                             │Press s to swap to this follower                │)pin",
             R"pin(          <7>│                             │<10>Within interaction range                        <7>│)pin",
             R"pin(          <7>│                             │<15>Status: Following                               <7>│)pin",
             R"pin(          <7>│                             │<2>Condition: He is uninjured.                     <7>│)pin",
             R"pin(          <7>│                             │<2>Hunger: Sated                                   <7>│)pin",
             R"pin(          <7>│                             │<2>Thirst: Turgid                                  <7>│)pin",
             R"pin(          <7>│                             │<15>Fatigue: Nominal                                <7>│)pin",
             R"pin(          <7>│                             │<15>Wielding: none                                  <7>│)pin",
             R"pin(          <7>│                             │                                                │)pin",
             R"pin(          <7>│                             │<15>Best combat skill: Submachine Guns: 4           <7>│)pin",
             R"pin(          <7>│                             │<15>Best other skills: First Aid: 6                 <7>│)pin",
             R"pin(          <7>│                             │                    <15>Bartering: 4                <7>│)pin",
             R"pin(          <7>│                             │                    <15>Athletics: 3                <7>│)pin",
             R"pin(          <7>│                             │                                                │)pin",
             R"pin(          <7>│                             │                                                │)pin",
             R"pin(          <7>│                             │                                                │)pin",
             R"pin(          <7>│                             │                                                │)pin",
             R"pin(          <7>│                             │                                                │)pin",
             R"pin(          <7>│                             │                                                │)pin",
             R"pin(          <7>└─────────────────────────────┴────────────────────────────────────────────────┘)pin",
         }},
        {"factions:factions",
         {
             R"pin(            <7>┌──────────────┐ ┌──────────────┐ ┌────┐ ┌─────────┐)pin",
             R"pin(            <7>│YOUR FOLLOWERS│<7:4><<7>│<7:4>OTHER FACTIONS<7>│<7:4>><7>│LORE│ │CREATURES│)pin",
             R"pin(          <7>┌─┴──────────────┴─┘          ┬   └─┴────┴─┴─────────┴─────────────────────────┐)pin",
             R"pin(          <7>│<15:4>TEST observation faction     <7>│Attitude to you:           Beloved              │)pin",
             R"pin(          <7>│                             │Faction strength:       100                     │)pin",
             R"pin(          <7>│                             │Effective observation faction description.      │)pin",
             R"pin(          <7>│                             │                                                │)pin",
             R"pin(          <7>│                             │                                                │)pin",
             R"pin(          <7>│                             │                                                │)pin",
             R"pin(          <7>│                             │                                                │)pin",
             R"pin(          <7>│                             │                                                │)pin",
             R"pin(          <7>│                             │                                                │)pin",
             R"pin(          <7>│                             │                                                │)pin",
             R"pin(          <7>│                             │                                                │)pin",
             R"pin(          <7>│                             │                                                │)pin",
             R"pin(          <7>│                             │                                                │)pin",
             R"pin(          <7>│                             │                                                │)pin",
             R"pin(          <7>│                             │                                                │)pin",
             R"pin(          <7>│                             │                                                │)pin",
             R"pin(          <7>│                             │                                                │)pin",
             R"pin(          <7>│                             │                                                │)pin",
             R"pin(          <7>│                             │                                                │)pin",
             R"pin(          <7>│                             │                                                │)pin",
             R"pin(          <7>└─────────────────────────────┴────────────────────────────────────────────────┘)pin",
         }},
        {"factions:lore",
         {
             R"pin(            <7>┌──────────────┐ ┌──────────────┐ ┌────┐ ┌─────────┐)pin",
             R"pin(            <7>│YOUR FOLLOWERS│ │OTHER FACTIONS│<7:4><<7>│<7:4>LORE<7>│<7:4>><7>│CREATURES│)pin",
             R"pin(          <7>┌─┴──────────────┴─┴──────────┬───┴─┘    └─┴─────────┴─────────────────────────┐)pin",
             R"pin(          <7>│                             │                                                │)pin",
             R"pin(          <7>│                             │<9>You haven't learned anything about the world.   <7>│)pin",
             R"pin(          <7>│                             │                                                │)pin",
             R"pin(          <7>│                             │                                                │)pin",
             R"pin(          <7>│                             │                                                │)pin",
             R"pin(          <7>│                             │                                                │)pin",
             R"pin(          <7>│                             │                                                │)pin",
             R"pin(          <7>│                             │                                                │)pin",
             R"pin(          <7>│                             │                                                │)pin",
             R"pin(          <7>│                             │                                                │)pin",
             R"pin(          <7>│                             │                                                │)pin",
             R"pin(          <7>│                             │                                                │)pin",
             R"pin(          <7>│                             │                                                │)pin",
             R"pin(          <7>│                             │                                                │)pin",
             R"pin(          <7>│                             │                                                │)pin",
             R"pin(          <7>│                             │                                                │)pin",
             R"pin(          <7>│                             │                                                │)pin",
             R"pin(          <7>│                             │                                                │)pin",
             R"pin(          <7>│                             │                                                │)pin",
             R"pin(          <7>│                             │                                                │)pin",
             R"pin(          <7>└─────────────────────────────┴────────────────────────────────────────────────┘)pin",
         }},
        {"factions:creatures",
         {
             R"pin(            <7>┌──────────────┐ ┌──────────────┐ ┌────┐ ┌─────────┐)pin",
             R"pin(            <7>│YOUR FOLLOWERS│ │OTHER FACTIONS│ │LORE│<7:4><<7>│<7:4>CREATURES<7>│<7:4>>)pin",
             R"pin(          <7>┌─┴──────────────┴─┴──────────┬───┴─┴────┴─┘         └─────────────────────────┐)pin",
             R"pin(          <7>│<10:4>Z  <15:4>zombie                    <7>│  <10>Z  <15>zombie                                     <7>│)pin",
             R"pin(          <7>│                             │<15>Difficulty<7>: Mildly dangerous.                   │)pin",
             R"pin(          <7>│                             │<15>Origin<7>: 'Bright Nights'                         │)pin",
             R"pin(          <7>│                             │<15>Size<7>: Medium                                    │)pin",
             R"pin(          <7>│                             │<15>Species<7>: <11>a human <7>and <11>a zombie                   <7>│)pin",
             R"pin(          <7>│                             │<15>Senses<7>: <11>hearing<7>, <11>sight<7>, and <11>smell               <7>│)pin",
             R"pin(          <7>│                             │                                                │)pin",
             R"pin(          <7>│                             │A human body, swaying as it moves, an           │)pin",
             R"pin(          <7>│                             │unstoppable rage visible in its oily black      │)pin",
             R"pin(          <7>│                             │eyes.                                           │)pin",
             R"pin(          <7>│                             │                                                │)pin",
             R"pin(          <7>│                             │                                                │)pin",
             R"pin(          <7>│                             │                                                │)pin",
             R"pin(          <7>│                             │                                                │)pin",
             R"pin(          <7>│                             │                                                │)pin",
             R"pin(          <7>│                             │                                                │)pin",
             R"pin(          <7>│                             │                                                │)pin",
             R"pin(          <7>│                             │                                                │)pin",
             R"pin(          <7>│                             │                                                │)pin",
             R"pin(          <7>│                             │                                                │)pin",
             R"pin(          <7>└─────────────────────────────┴────────────────────────────────────────────────┘)pin",
         }},
        {"scores:achievements",
         {
             R"pin(            <7>┌────────────┐ ┌──────┐ ┌─────┐)pin",
             R"pin(           <7:4><<7>│<7:4>ACHIEVEMENTS<7>│<7:4>><7>│SCORES│ │KILLS│)pin",
             R"pin(          <7>┌─┘            └─┴──────┴─┴─────┴──────────────────────────────────────────────┐)pin",
             R"pin(          <10>^<11>[Combat] Decamate                                                             <7>│)pin",
             R"pin(          <6:6>│  <11>Kill at least 10 monsters (3/10)                                            <7>│)pin",
             R"pin(          <6:6>│  <2>Triggered by monster killed                                                 <7>│)pin",
             R"pin(          <6:6>│                                                                              <7>│)pin",
             R"pin(          <6:6>│<11>[Combat] One Down, Billions to Go…                                            <7>│)pin",
             R"pin(          <6:6>│  <10>Kill at least 1 zombie (3/1)                                                <7>│)pin",
             R"pin(          <6:6>│  <2>Triggered by zombie killed                                                  <7>│)pin",
             R"pin(          <6:6>│                                                                              <7>│)pin",
             R"pin(          <6:6>│<11>[Combat] Rude Awakening                                                       <7>│)pin",
             R"pin(          <6:6>│  <10>Within 1 minute of start of game (1 minute remaining)                       <7>│)pin",
             R"pin(          <6:6>│  <10>Kill at least 1 monster (3/1)                                               <7>│)pin",
             R"pin(          <6:6>│  <2>Triggered by monster killed                                                 <7>│)pin",
             R"pin(          <6:6>│                                                                              <7>│)pin",
             R"pin(          <6:6>│<11>[Skill] Ace Driver                                                            <7>│)pin",
             R"pin(          <6:6>│  <11>No turn is too sharp.                                                       <7>│)pin",
             R"pin(          <6:6>│  <11>Attain skill level of 5 in Driving (0/5)                                    <7>│)pin",
             R"pin(          <15>│  <2>Triggered by driving skill level up.                                        <7>│)pin",
             R"pin(          <15>│                                                                              <7>│)pin",
             R"pin(          <15>│<11>[Skill] Batter                                                                <7>│)pin",
             R"pin(          <15>│  <11>Every strike brings me closer to a home run.                                <7>│)pin",
             R"pin(          <15>│  <11>Attain skill level of 5 in Bashing Weapons (0/5)                            <7>│)pin",
             R"pin(          <15>│  <2>Triggered by bashing weapons skill level up.                                <7>│)pin",
             R"pin(          <15>│                                                                              <7>│)pin",
             R"pin(          <15>│<11>[Skill] Brawler                                                               <7>│)pin",
             R"pin(          <15>│  <11>Bottle in left hand, chair leg in right hand.                               <7>│)pin",
             R"pin(          <15>│  <11>Attain skill level of 5 in Melee (0/5)                                      <7>│)pin",
             R"pin(          <15>│  <2>Triggered by melee skill level up.                                          <7>│)pin",
             R"pin(          <15>│                                                                              <7>│)pin",
             R"pin(          <15>│<11>[Skill] Bull's Eye                                                            <7>│)pin",
             R"pin(          <15>│  <11>Wilhelm Tell?  Never heard of him.                                          <7>│)pin",
             R"pin(          <15>│  <11>Attain skill level of 5 in Archery (0/5)                                    <7>│)pin",
             R"pin(          <15>│  <2>Triggered by archery skill level up.                                        <7>│)pin",
             R"pin(          <15>│                                                                              <7>│)pin",
             R"pin(          <15>│<11>[Skill] Do-It-Yourselfer                                                      <7>│)pin",
             R"pin(          <15>│  <11>Take this thing, put it in that thing, and voila.                           <7>│)pin",
             R"pin(          <15>│  <11>Attain skill level of 5 in Fabrication (0/5)                                <7>│)pin",
             R"pin(          <15>│  <2>Triggered by fabrication skill level up.                                    <7>│)pin",
             R"pin(          <15>│                                                                              <7>│)pin",
             R"pin(          <15>│<11>[Skill] Double Barrel, Double Fun                                             <7>│)pin",
             R"pin(          <15>│  <11>I like to keep this handy for close encounters.                             <7>│)pin",
             R"pin(          <15>│  <11>Attain skill level of 5 in Shotguns (0/5)                                   <7>│)pin",
             R"pin(          <15>│  <2>Triggered by shotguns skill level up.                                       <7>│)pin",
             R"pin(          <15>│                                                                              <7>│)pin",
             R"pin(          <15>│<11>[Skill] Eagle Eye                                                             <7>│)pin",
             R"pin(          <15>│  <11>Only me and my target.                                                      <7>│)pin",
             R"pin(          <15>│  <11>Attain skill level of 5 in Marksmanship (0/5)                               <7>│)pin",
             R"pin(          <15>│  <2>Triggered by marksmanship skill level up.                                   <7>│)pin",
             R"pin(          <15>│                                                                              <7>│)pin",
             R"pin(          <15>│<11>[Skill] Eloquent                                                              <7>│)pin",
             R"pin(          <15>│  <11>We're friends, aren't we?                                                   <7>│)pin",
             R"pin(          <15>│  <11>Attain skill level of 5 in Speaking (0/5)                                   <7>│)pin",
             R"pin(          <15>│  <2>Triggered by speech skill level up.                                         <7>│)pin",
             R"pin(          <15>│                                                                              <7>│)pin",
             R"pin(          <10>v<11>[Skill] Elusive                                                               <7>│)pin",
             R"pin(          <7>└──────────────────────────────────────────────────────────────────────────────┘)pin",
         }},
        {"scores:scores",
         {
             R"pin(            <7>┌────────────┐ ┌──────┐ ┌─────┐)pin",
             R"pin(            <7>│ACHIEVEMENTS│<7:4><<7>│<7:4>SCORES<7>│<7:4>><7>│KILLS│)pin",
             R"pin(          <7>┌─┴────────────┴─┘      └─┴─────┴──────────────────────────────────────────────┐)pin",
             R"pin(          <7>│<15>This game has no valid scores.                                                <7>│)pin",
             R"pin(          <7>│                                                                              │)pin",
             R"pin(          <7>│<15>Note that only scores that existed when you started this game and still exist <7>│)pin",
             R"pin(          <7>│<15>now will appear here.                                                         <7>│)pin",
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
             R"pin(          <7>└──────────────────────────────────────────────────────────────────────────────┘)pin",
         }},
        {"scores:kills",
         {
             R"pin(            <7>┌────────────┐ ┌──────┐ ┌─────┐)pin",
             R"pin(            <7>│ACHIEVEMENTS│ │SCORES│<7:4><<7>│<7:4>KILLS<7>│<7:4>>)pin",
             R"pin(          <7>┌─┴────────────┴─┴──────┴─┘     └──────────────────────────────────────────────┐)pin",
             R"pin(          <7>│<15>KILL COUNT: 4                                                                 <7>│)pin",
             R"pin(          <7>│                                                                              │)pin",
             R"pin(          <7>│   <15>3 <10>Z <7>zombie                                                                 │)pin",
             R"pin(          <7>│   <15>1 <5>@ Bob                                                                    <7>│)pin",
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
             R"pin(          <7>└──────────────────────────────────────────────────────────────────────────────┘)pin",
         }},
        {"kills",
         {
             R"pin(          <7>┌──────────────────────────────────────────────────────────────────────────────┐)pin",
             R"pin(          <7>│                                                                              │)pin",
             R"pin(          <7>│                                                                              │)pin",
             R"pin(          <7>│<15>KILL COUNT: 4                                                                 <7>│)pin",
             R"pin(          <7>│                                                                              │)pin",
             R"pin(          <7>│   <15>3 <10>Z <7>zombie                                                                 │)pin",
             R"pin(          <7>│   <15>1 <5>@ Bob                                                                    <7>│)pin",
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
             R"pin(          <7>└──────────────────────────────────────────────────────────────────────────────┘)pin",
         }},
    };
    return screens;
}

/// Compares the screen on display with the recorded one.
auto pinned(const std::string& name) -> void {
    INFO(name);
    CHECK(rendered() == recorded().at(name));
}

} // namespace

TEST_CASE("the native help keeps its topic list and topic text", "[info_native_pin]") {
    const auto guard = screen_guard{};
    get_help().load();
    auto step = 0;
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        switch (step++) {
            case 0:
                pinned("help:topics");
                return typed("b");
            case 1:
                pinned("help:topic");
                return act("QUIT");
            case 2:
                pinned("help:topics");
                return act("QUIT");
            default:
                break;
        }
        FAIL("unexpected native read");
        return act("QUIT");
    });
    get_help().display_help();
    CHECK(step == 3);
}

TEST_CASE("the native morale screen keeps its text and colors", "[info_native_pin]") {
    const auto guard = screen_guard{};
    auto& you = get_avatar();
    you.add_morale(MORALE_FOOD_GOOD, 20, 40, 1_hours, 30_minutes, false, &*itype_id("apple"));
    you.add_morale(MORALE_FOOD_BAD, -10, -20, 1_hours, 30_minutes, false, &*itype_id("bread"));
    auto step = 0;
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        REQUIRE(step++ == 0);
        pinned("morale");
        return act("QUIT");
    });
    you.disp_morale();
    CHECK(step == 1);
}

TEST_CASE("the native mission list keeps its text and colors", "[info_native_pin]") {
    const auto guard = screen_guard{};
    auto& you = get_avatar();
    mission* assigned = nullptr;
    for (const mission_type& type : mission_type::get_all()) {
        if (type.goal == MGOAL_ASSASSINATE) {
            assigned = mission::reserve_new(type.id, you.getID());
            break;
        }
    }
    REQUIRE(assigned != nullptr);
    if (assigned->get_assigned_player_id() == you.getID()) {
        you.on_mission_assignment(*assigned);
    } else {
        assigned->assign(you);
    }
    auto step = 0;
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        switch (step++) {
            case 0:
                pinned("missions:active");
                return act("RIGHT");
            case 1:
                pinned("missions:completed");
                return act("QUIT");
            default:
                break;
        }
        FAIL("unexpected native read");
        return act("QUIT");
    });
    g->list_missions();
    CHECK(step == 2);
}

TEST_CASE("the native faction manager keeps its text and colors", "[info_native_pin]") {
    const auto guard = screen_guard{};
    auto& you = get_avatar();
    you.set_knows_creature_type(mtype_id("mon_zombie"));
    auto& follower = spawn_npc(you.bub_pos() + tripoint_east, "test_talker");
    follower.set_fac(faction_id("your_followers"));
    follower.set_attitude(NPCATT_FOLLOW);
    REQUIRE(follower.is_player_ally());
    auto step = 0;
    const auto names = std::vector<std::string>{"followers", "factions", "lore", "creatures"};
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        REQUIRE(step < 4);
        pinned("factions:" + names[step]);
        return act(++step < 4 ? "NEXT_TAB" : "QUIT");
    });
    g->faction_manager_ptr->display();
    CHECK(step == 4);
}

TEST_CASE("the native scores screen keeps its text and colors on every tab", "[info_native_pin]") {
    const auto guard = screen_guard{};
    auto stats = stats_tracker{};
    auto kills = kill_tracker{};
    kills.reset({{mtype_id("mon_zombie"), 3}}, {"Bob"});
    auto achievements = achievements_tracker(stats, kills, [](const achievement*) {});
    achievements.notify(cata::event::make<event_type::game_start>(character_id()));
    auto step = 0;
    const auto names = std::vector<std::string>{"achievements", "scores", "kills"};
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        REQUIRE(step < 3);
        pinned("scores:" + names[step]);
        return act(++step < 3 ? "NEXT_TAB" : "QUIT");
    });
    show_scores_ui(achievements, stats, kills);
    CHECK(step == 3);
}

TEST_CASE("the native kills screen keeps its text and colors", "[info_native_pin]") {
    const auto guard = screen_guard{};
    auto kills = kill_tracker{};
    kills.reset({{mtype_id("mon_zombie"), 3}}, {"Bob"});
    auto step = 0;
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        REQUIRE(step++ == 0);
        pinned("kills");
        return act("QUIT");
    });
    show_kills(kills);
    CHECK(step == 1);
}

#endif // CATA_MCP
