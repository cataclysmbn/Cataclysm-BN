#if defined(CATA_MCP)

#    include "armor_layers.h"
#    include "avatar.h"
#    include "catch/catch.hpp"
#    include "client_command.h"
#    include "client_input.h"
#    include "client_memory.h"
#    include "client_memory_scope.h"
#    include "client_snapshot.h"
#    include "cursesdef.h"
#    include "examine_item_menu.h"
#    include "game.h"
#    include "game_constants.h"
#    include "input.h"
#    include "item.h"
#    include "map_helpers.h"
#    include "output.h"
#    include "player_helpers.h"
#    include "state_helpers.h"

#    include <format>
#    include <map>
#    include <string>
#    include <vector>

/// Pins what the native armor layers screen and item examine menu print, so that sharing their
/// fact gathering with the engine client keeps the player's screens byte-equal: every row with its
/// text and colour runs.
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
        get_avatar().moves = 100;
        test_mode = false;
        TERMX = 140;
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
        {"armor:all",
         {
             R"pin(        <7>┌───────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────┐)pin",
             R"pin(        <7>│ <15>Sort Armor  <11><< All >>          <15>[<11>H<15>] Hide sprite.  [<11>c<15>] Change side.  Press [<11>F1<15>] for help.  Press [<11>?<15>] to change keybindings. <7>│)pin",
             R"pin(        <7>├────────────────────────────────────────┬─────────────────────────────────────────┬────────────────────────────────────────┤)pin",
             R"pin(        <7>│(Innermost)                  Storage (L)│ <15>t-shirt                                 <7>│(Innermost)                  Encumbrance│)pin",
             R"pin(        <7>│<11>>> <10>|| <11>t-shirt (poor fit)            <7>0.00│  <2>[Properties]                           <7>│ <15>Torso:                                 <7>│)pin",
             R"pin(        <7>│   <10>|| <11>t-shirt (poor fit)            <7>0.00│    Coverage:                         50 │  <10>|| <11>t-shirt (poor fit)               <7>0+│)pin",
             R"pin(        <7>│   <10>|| <7>jeans (poor fit)              0.50│    Encumbrance:                       0 │  <10>|| <11>t-shirt (poor fit)               <7>0+│)pin",
             R"pin(        <7>│   <10>|| <7>hoodie (poor fit)             2.25│    Warmth:                            5 │  <10>|| <7>hoodie (poor fit)               12 │)pin",
             R"pin(        <7>│                                        │    Storage (L):                    0.00 │ <15>Head:                                  <7>│)pin",
             R"pin(        <7>│                                        │  <2>[Protection]                           <7>│ <15>Eyes:                                  <7>│)pin",
             R"pin(        <7>│                                        │    Bash:                              1 │ <15>Mouth:                                 <7>│)pin",
             R"pin(        <7>│                                        │    Cut:                               1 │ <15>Arms:                                  <7>│)pin",
             R"pin(        <7>│                                        │    Ballistic:                         1 │  <10>|| <11>t-shirt (poor fit)               <7>0+│)pin",
             R"pin(        <7>│                                        │    Acid:                              0 │  <10>|| <11>t-shirt (poor fit)               <7>0+│)pin",
             R"pin(        <7>│                                        │    Fire:                              0 │  <10>|| <7>hoodie (poor fit)               12 │)pin",
             R"pin(        <7>│                                        │    Environmental:                     0 │ <15>Hands:                                 <7>│)pin",
             R"pin(        <7>│                                        │                                         │ <15>Legs:                                  <7>│)pin",
             R"pin(        <7>│                                        │<12>It could be refitted.                    <7>│  <10>|| <7>jeans (poor fit)                16 │)pin",
             R"pin(        <7>│                                        │Wearing multiple items of <12>normal <7>clothing│ <15>Feet:                                  <7>│)pin",
             R"pin(        <7>│                                        │on your <9>torso and arms <7>is adding         │                                        │)pin",
             R"pin(        <7>│                                        │encumbrance there.                       │                                        │)pin",
             R"pin(        <7>│                                        │                                         │                                        │)pin",
             R"pin(        <7>│                                        │                                         │                                        │)pin",
             R"pin(        <7>│                                        │                                         │                                        │)pin",
             R"pin(        <7>│                                        │ <15>Encumbrance and Warmth                  <7>│                                        │)pin",
             R"pin(        <7>│                                        │ Head     0+0                      <2>(  0) <7>│                                        │)pin",
             R"pin(        <7>│                                        │ Eyes     0+0                      (  0) │                                        │)pin",
             R"pin(        <7>│                                        │ Mouth    0+0                      <2>(  0) <7>│                                        │)pin",
             R"pin(        <7>│                                        │ <2>Torso   <11>12<7>+<11>2                      <2>(  0) <7>│                                        │)pin",
             R"pin(        <7>│                                        │ <2>Arms    <11>12<7>+<11>2                      <2>(  0) <7>│                                        │)pin",
             R"pin(        <7>│                                        │ Hands    0+0                      <2>(  0) <7>│                                        │)pin",
             R"pin(        <7>│                                        │ Legs    <11>16<7>+<11>0                      <2>(  0) <7>│                                        │)pin",
             R"pin(        <7>│                                        │ Feet     0+0                      <2>(  0) <7>│                                        │)pin",
             R"pin(        <7>│                                        │                                         │                                        │)pin",
             R"pin(        <7>│                                        │                                         │                                        │)pin",
             R"pin(        <7>│                                        │                                         │                                        │)pin",
             R"pin(        <7>│(Outermost)                             │                                         │(Outermost)                             │)pin",
             R"pin(        <7>└────────────────────────────────────────┴─────────────────────────────────────────┴────────────────────────────────────────┘)pin",
         }},
        {"armor:empty",
         {
             R"pin(        <7>┌───────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────┐)pin",
             R"pin(        <7>│ <15>Sort Armor  <11><< All >>          <15>[<11>H<15>] Hide sprite.  [<11>c<15>] Change side.  Press [<11>F1<15>] for help.  Press [<11>?<15>] to change keybindings. <7>│)pin",
             R"pin(        <7>├────────────────────────────────────────┬─────────────────────────────────────────┬────────────────────────────────────────┤)pin",
             R"pin(        <7>│(Innermost)                  Storage (L)│ <15>Nothing to see here!                    <7>│(Innermost)                  Encumbrance│)pin",
             R"pin(        <7>│                                        │                                         │ <15>Torso:                                 <7>│)pin",
             R"pin(        <7>│                                        │                                         │ <15>Head:                                  <7>│)pin",
             R"pin(        <7>│                                        │                                         │ <15>Eyes:                                  <7>│)pin",
             R"pin(        <7>│                                        │                                         │ <15>Mouth:                                 <7>│)pin",
             R"pin(        <7>│                                        │                                         │ <15>Arms:                                  <7>│)pin",
             R"pin(        <7>│                                        │                                         │ <15>Hands:                                 <7>│)pin",
             R"pin(        <7>│                                        │                                         │ <15>Legs:                                  <7>│)pin",
             R"pin(        <7>│                                        │                                         │ <15>Feet:                                  <7>│)pin",
             R"pin(        <7>│                                        │                                         │                                        │)pin",
             R"pin(        <7>│                                        │                                         │                                        │)pin",
             R"pin(        <7>│                                        │                                         │                                        │)pin",
             R"pin(        <7>│                                        │                                         │                                        │)pin",
             R"pin(        <7>│                                        │                                         │                                        │)pin",
             R"pin(        <7>│                                        │                                         │                                        │)pin",
             R"pin(        <7>│                                        │                                         │                                        │)pin",
             R"pin(        <7>│                                        │                                         │                                        │)pin",
             R"pin(        <7>│                                        │                                         │                                        │)pin",
             R"pin(        <7>│                                        │                                         │                                        │)pin",
             R"pin(        <7>│                                        │                                         │                                        │)pin",
             R"pin(        <7>│                                        │                                         │                                        │)pin",
             R"pin(        <7>│                                        │ <15>Encumbrance and Warmth                  <7>│                                        │)pin",
             R"pin(        <7>│                                        │ Head     0+0                      <2>(  0) <7>│                                        │)pin",
             R"pin(        <7>│                                        │ Eyes     0+0                      (  0) │                                        │)pin",
             R"pin(        <7>│                                        │ Mouth    0+0                      <2>(  0) <7>│                                        │)pin",
             R"pin(        <7>│                                        │ Torso    0+0                      <2>(  0) <7>│                                        │)pin",
             R"pin(        <7>│                                        │ Arms     0+0                      <2>(  0) <7>│                                        │)pin",
             R"pin(        <7>│                                        │ Hands    0+0                      <2>(  0) <7>│                                        │)pin",
             R"pin(        <7>│                                        │ Legs     0+0                      <2>(  0) <7>│                                        │)pin",
             R"pin(        <7>│                                        │ Feet     0+0                      <2>(  0) <7>│                                        │)pin",
             R"pin(        <7>│                                        │                                         │                                        │)pin",
             R"pin(        <7>│                                        │                                         │                                        │)pin",
             R"pin(        <7>│                                        │                                         │                                        │)pin",
             R"pin(        <7>│(Outermost)                      <12><empty><7>│                                         │(Outermost)                             │)pin",
             R"pin(        <7>└────────────────────────────────────────┴─────────────────────────────────────────┴────────────────────────────────────────┘)pin",
         }},
        {"armor:grabbed",
         {
             R"pin(        <7>┌───────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────┐)pin",
             R"pin(        <7>│ <15>Sort Armor  <11><< All >>          <15>[<11>H<15>] Hide sprite.  [<11>c<15>] Change side.  Press [<11>F1<15>] for help.  Press [<11>?<15>] to change keybindings. <7>│)pin",
             R"pin(        <7>├────────────────────────────────────────┬─────────────────────────────────────────┬────────────────────────────────────────┤)pin",
             R"pin(        <7>│(Innermost)                  Storage (L)│ <15>t-shirt                                 <7>│(Innermost)                  Encumbrance│)pin",
             R"pin(        <7>│<11>>>  <10>|| <11>t-shirt (poor fit)           <7>0.00│  <2>[Properties]                           <7>│ <15>Torso:                                 <7>│)pin",
             R"pin(        <7>│   <10>|| <11>t-shirt (poor fit)            <7>0.00│    Coverage:                         50 │  <10>|| <11>t-shirt (poor fit)               <7>0+│)pin",
             R"pin(        <7>│   <10>|| <7>jeans (poor fit)              0.50│    Encumbrance:                       0 │  <10>|| <11>t-shirt (poor fit)               <7>0+│)pin",
             R"pin(        <7>│   <10>|| <7>hoodie (poor fit)             2.25│    Warmth:                            5 │  <10>|| <7>hoodie (poor fit)               12 │)pin",
             R"pin(        <7>│                                        │    Storage (L):                    0.00 │ <15>Head:                                  <7>│)pin",
             R"pin(        <7>│                                        │  <2>[Protection]                           <7>│ <15>Eyes:                                  <7>│)pin",
             R"pin(        <7>│                                        │    Bash:                              1 │ <15>Mouth:                                 <7>│)pin",
             R"pin(        <7>│                                        │    Cut:                               1 │ <15>Arms:                                  <7>│)pin",
             R"pin(        <7>│                                        │    Ballistic:                         1 │  <10>|| <11>t-shirt (poor fit)               <7>0+│)pin",
             R"pin(        <7>│                                        │    Acid:                              0 │  <10>|| <11>t-shirt (poor fit)               <7>0+│)pin",
             R"pin(        <7>│                                        │    Fire:                              0 │  <10>|| <7>hoodie (poor fit)               12 │)pin",
             R"pin(        <7>│                                        │    Environmental:                     0 │ <15>Hands:                                 <7>│)pin",
             R"pin(        <7>│                                        │                                         │ <15>Legs:                                  <7>│)pin",
             R"pin(        <7>│                                        │<12>It could be refitted.                    <7>│  <10>|| <7>jeans (poor fit)                16 │)pin",
             R"pin(        <7>│                                        │Wearing multiple items of <12>normal <7>clothing│ <15>Feet:                                  <7>│)pin",
             R"pin(        <7>│                                        │on your <9>torso and arms <7>is adding         │                                        │)pin",
             R"pin(        <7>│                                        │encumbrance there.                       │                                        │)pin",
             R"pin(        <7>│                                        │                                         │                                        │)pin",
             R"pin(        <7>│                                        │                                         │                                        │)pin",
             R"pin(        <7>│                                        │                                         │                                        │)pin",
             R"pin(        <7>│                                        │ <15>Encumbrance and Warmth                  <7>│                                        │)pin",
             R"pin(        <7>│                                        │ Head     0+0                      <2>(  0) <7>│                                        │)pin",
             R"pin(        <7>│                                        │ Eyes     0+0                      (  0) │                                        │)pin",
             R"pin(        <7>│                                        │ Mouth    0+0                      <2>(  0) <7>│                                        │)pin",
             R"pin(        <7>│                                        │ <2>Torso   <11>12<7>+<11>2                      <2>(  0) <7>│                                        │)pin",
             R"pin(        <7>│                                        │ <2>Arms    <11>12<7>+<11>2                      <2>(  0) <7>│                                        │)pin",
             R"pin(        <7>│                                        │ Hands    0+0                      <2>(  0) <7>│                                        │)pin",
             R"pin(        <7>│                                        │ Legs    <11>16<7>+<11>0                      <2>(  0) <7>│                                        │)pin",
             R"pin(        <7>│                                        │ Feet     0+0                      <2>(  0) <7>│                                        │)pin",
             R"pin(        <7>│                                        │                                         │                                        │)pin",
             R"pin(        <7>│                                        │                                         │                                        │)pin",
             R"pin(        <7>│                                        │                                         │                                        │)pin",
             R"pin(        <7>│(Outermost)                             │                                         │(Outermost)                             │)pin",
             R"pin(        <7>└────────────────────────────────────────┴─────────────────────────────────────────┴────────────────────────────────────────┘)pin",
         }},
        {"armor:moved",
         {
             R"pin(        <7>┌───────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────┐)pin",
             R"pin(        <7>│ <15>Sort Armor  <11><< All >>          <15>[<11>H<15>] Hide sprite.  [<11>c<15>] Change side.  Press [<11>F1<15>] for help.  Press [<11>?<15>] to change keybindings. <7>│)pin",
             R"pin(        <7>├────────────────────────────────────────┬─────────────────────────────────────────┬────────────────────────────────────────┤)pin",
             R"pin(        <7>│(Innermost)                  Storage (L)│ <15>t-shirt                                 <7>│(Innermost)                  Encumbrance│)pin",
             R"pin(        <7>│   <10>|| <11>t-shirt (poor fit)            <7>0.00│  <2>[Properties]                           <7>│ <15>Torso:                                 <7>│)pin",
             R"pin(        <7>│<11>>>  <10>|| <11>t-shirt (poor fit)           <7>0.00│    Coverage:                         50 │  <10>|| <11>t-shirt (poor fit)               <7>0+│)pin",
             R"pin(        <7>│   <10>|| <7>jeans (poor fit)              0.50│    Encumbrance:                       0 │  <10>|| <11>t-shirt (poor fit)               <7>0+│)pin",
             R"pin(        <7>│   <10>|| <7>hoodie (poor fit)             2.25│    Warmth:                            5 │  <10>|| <7>hoodie (poor fit)               12 │)pin",
             R"pin(        <7>│                                        │    Storage (L):                    0.00 │ <15>Head:                                  <7>│)pin",
             R"pin(        <7>│                                        │  <2>[Protection]                           <7>│ <15>Eyes:                                  <7>│)pin",
             R"pin(        <7>│                                        │    Bash:                              1 │ <15>Mouth:                                 <7>│)pin",
             R"pin(        <7>│                                        │    Cut:                               1 │ <15>Arms:                                  <7>│)pin",
             R"pin(        <7>│                                        │    Ballistic:                         1 │  <10>|| <11>t-shirt (poor fit)               <7>0+│)pin",
             R"pin(        <7>│                                        │    Acid:                              0 │  <10>|| <11>t-shirt (poor fit)               <7>0+│)pin",
             R"pin(        <7>│                                        │    Fire:                              0 │  <10>|| <7>hoodie (poor fit)               12 │)pin",
             R"pin(        <7>│                                        │    Environmental:                     0 │ <15>Hands:                                 <7>│)pin",
             R"pin(        <7>│                                        │                                         │ <15>Legs:                                  <7>│)pin",
             R"pin(        <7>│                                        │<12>It could be refitted.                    <7>│  <10>|| <7>jeans (poor fit)                16 │)pin",
             R"pin(        <7>│                                        │Wearing multiple items of <12>normal <7>clothing│ <15>Feet:                                  <7>│)pin",
             R"pin(        <7>│                                        │on your <9>torso and arms <7>is adding         │                                        │)pin",
             R"pin(        <7>│                                        │encumbrance there.                       │                                        │)pin",
             R"pin(        <7>│                                        │                                         │                                        │)pin",
             R"pin(        <7>│                                        │                                         │                                        │)pin",
             R"pin(        <7>│                                        │                                         │                                        │)pin",
             R"pin(        <7>│                                        │ <15>Encumbrance and Warmth                  <7>│                                        │)pin",
             R"pin(        <7>│                                        │ Head     0+0                      <2>(  0) <7>│                                        │)pin",
             R"pin(        <7>│                                        │ Eyes     0+0                      (  0) │                                        │)pin",
             R"pin(        <7>│                                        │ Mouth    0+0                      <2>(  0) <7>│                                        │)pin",
             R"pin(        <7>│                                        │ <2>Torso   <11>12<7>+<11>2                      <2>(  0) <7>│                                        │)pin",
             R"pin(        <7>│                                        │ <2>Arms    <11>12<7>+<11>2                      <2>(  0) <7>│                                        │)pin",
             R"pin(        <7>│                                        │ Hands    0+0                      <2>(  0) <7>│                                        │)pin",
             R"pin(        <7>│                                        │ Legs    <11>16<7>+<11>0                      <2>(  0) <7>│                                        │)pin",
             R"pin(        <7>│                                        │ Feet     0+0                      <2>(  0) <7>│                                        │)pin",
             R"pin(        <7>│                                        │                                         │                                        │)pin",
             R"pin(        <7>│                                        │                                         │                                        │)pin",
             R"pin(        <7>│                                        │                                         │                                        │)pin",
             R"pin(        <7>│(Outermost)                             │                                         │(Outermost)                             │)pin",
             R"pin(        <7>└────────────────────────────────────────┴─────────────────────────────────────────┴────────────────────────────────────────┘)pin",
         }},
        {"armor:part",
         {
             R"pin(        <7>┌───────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────┐)pin",
             R"pin(        <7>│ <15>Sort Armor  <11><< Torso >>        <15>[<11>H<15>] Hide sprite.  [<11>c<15>] Change side.  Press [<11>F1<15>] for help.  Press [<11>?<15>] to change keybindings. <7>│)pin",
             R"pin(        <7>├────────────────────────────────────────┬─────────────────────────────────────────┬────────────────────────────────────────┤)pin",
             R"pin(        <7>│(Innermost)                  Storage (L)│ <15>t-shirt                                 <7>│(Innermost)                  Encumbrance│)pin",
             R"pin(        <7>│<11>>> <10>|| <11>t-shirt (poor fit)            <7>0.00│  <2>[Properties]                           <7>│ <11>Torso:                                 <7>│)pin",
             R"pin(        <7>│   <10>|| <11>t-shirt (poor fit)            <7>0.00│    Coverage:                         90 │  <10>|| <11>t-shirt (poor fit)               <7>0+│)pin",
             R"pin(        <7>│   <10>|| <7>hoodie (poor fit)             2.25│    Encumbrance:                       0 │  <10>|| <11>t-shirt (poor fit)               <7>0+│)pin",
             R"pin(        <7>│                                        │    Warmth:                            5 │  <10>|| <7>hoodie (poor fit)               12 │)pin",
             R"pin(        <7>│                                        │    Storage (L):                    0.00 │ <15>Head:                                  <7>│)pin",
             R"pin(        <7>│                                        │  <2>[Protection]                           <7>│ <15>Eyes:                                  <7>│)pin",
             R"pin(        <7>│                                        │    Bash:                              1 │ <15>Mouth:                                 <7>│)pin",
             R"pin(        <7>│                                        │    Cut:                               1 │ <15>Arms:                                  <7>│)pin",
             R"pin(        <7>│                                        │    Ballistic:                         1 │  <10>|| <11>t-shirt (poor fit)               <7>0+│)pin",
             R"pin(        <7>│                                        │    Acid:                              0 │  <10>|| <11>t-shirt (poor fit)               <7>0+│)pin",
             R"pin(        <7>│                                        │    Fire:                              0 │  <10>|| <7>hoodie (poor fit)               12 │)pin",
             R"pin(        <7>│                                        │    Environmental:                     0 │ <15>Hands:                                 <7>│)pin",
             R"pin(        <7>│                                        │                                         │ <15>Legs:                                  <7>│)pin",
             R"pin(        <7>│                                        │<12>It could be refitted.                    <7>│  <10>|| <7>jeans (poor fit)                16 │)pin",
             R"pin(        <7>│                                        │Wearing multiple items of <12>normal <7>clothing│ <15>Feet:                                  <7>│)pin",
             R"pin(        <7>│                                        │on your <9>torso and arms <7>is adding         │                                        │)pin",
             R"pin(        <7>│                                        │encumbrance there.                       │                                        │)pin",
             R"pin(        <7>│                                        │                                         │                                        │)pin",
             R"pin(        <7>│                                        │                                         │                                        │)pin",
             R"pin(        <7>│                                        │                                         │                                        │)pin",
             R"pin(        <7>│                                        │ <15>Encumbrance and Warmth                  <7>│                                        │)pin",
             R"pin(        <7>│                                        │ Head     0+0                      <2>(  0) <7>│                                        │)pin",
             R"pin(        <7>│                                        │ Eyes     0+0                      (  0) │                                        │)pin",
             R"pin(        <7>│                                        │ Mouth    0+0                      <2>(  0) <7>│                                        │)pin",
             R"pin(        <7>│                                        │ <2>Torso   <11>12<7>+<11>2                      <2>(  0) <7>│                                        │)pin",
             R"pin(        <7>│                                        │ <2>Arms    <11>12<7>+<11>2                      <2>(  0) <7>│                                        │)pin",
             R"pin(        <7>│                                        │ Hands    0+0                      <2>(  0) <7>│                                        │)pin",
             R"pin(        <7>│<15>Total Protection:                       <7>│ Legs    <11>16<7>+<11>0                      <2>(  0) <7>│                                        │)pin",
             R"pin(        <7>│  Bash: 5                               │ Feet     0+0                      <2>(  0) <7>│                                        │)pin",
             R"pin(        <7>│  Cut: 5                                │                                         │                                        │)pin",
             R"pin(        <7>│  Stab: 4                               │                                         │                                        │)pin",
             R"pin(        <7>│  Ballistic: 5                          │                                         │                                        │)pin",
             R"pin(        <7>│(Outermost)                             │                                         │(Outermost)                             │)pin",
             R"pin(        <7>└────────────────────────────────────────┴─────────────────────────────────────────┴────────────────────────────────────────┘)pin",
         }},
        {"armor:part_back",
         {
             R"pin(        <7>┌───────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────┐)pin",
             R"pin(        <7>│ <15>Sort Armor  <11><< All >>          <15>[<11>H<15>] Hide sprite.  [<11>c<15>] Change side.  Press [<11>F1<15>] for help.  Press [<11>?<15>] to change keybindings. <7>│)pin",
             R"pin(        <7>├────────────────────────────────────────┬─────────────────────────────────────────┬────────────────────────────────────────┤)pin",
             R"pin(        <7>│(Innermost)                  Storage (L)│ <15>t-shirt                                 <7>│(Innermost)                  Encumbrance│)pin",
             R"pin(        <7>│<11>>> <10>|| <11>t-shirt (poor fit)            <7>0.00│  <2>[Properties]                           <7>│ <15>Torso:                                 <7>│)pin",
             R"pin(        <7>│   <10>|| <11>t-shirt (poor fit)            <7>0.00│    Coverage:                         50 │  <10>|| <11>t-shirt (poor fit)               <7>0+│)pin",
             R"pin(        <7>│   <10>|| <7>jeans (poor fit)              0.50│    Encumbrance:                       0 │  <10>|| <11>t-shirt (poor fit)               <7>0+│)pin",
             R"pin(        <7>│   <10>|| <7>hoodie (poor fit)             2.25│    Warmth:                            5 │  <10>|| <7>hoodie (poor fit)               12 │)pin",
             R"pin(        <7>│                                        │    Storage (L):                    0.00 │ <15>Head:                                  <7>│)pin",
             R"pin(        <7>│                                        │  <2>[Protection]                           <7>│ <15>Eyes:                                  <7>│)pin",
             R"pin(        <7>│                                        │    Bash:                              1 │ <15>Mouth:                                 <7>│)pin",
             R"pin(        <7>│                                        │    Cut:                               1 │ <15>Arms:                                  <7>│)pin",
             R"pin(        <7>│                                        │    Ballistic:                         1 │  <10>|| <11>t-shirt (poor fit)               <7>0+│)pin",
             R"pin(        <7>│                                        │    Acid:                              0 │  <10>|| <11>t-shirt (poor fit)               <7>0+│)pin",
             R"pin(        <7>│                                        │    Fire:                              0 │  <10>|| <7>hoodie (poor fit)               12 │)pin",
             R"pin(        <7>│                                        │    Environmental:                     0 │ <15>Hands:                                 <7>│)pin",
             R"pin(        <7>│                                        │                                         │ <15>Legs:                                  <7>│)pin",
             R"pin(        <7>│                                        │<12>It could be refitted.                    <7>│  <10>|| <7>jeans (poor fit)                16 │)pin",
             R"pin(        <7>│                                        │Wearing multiple items of <12>normal <7>clothing│ <15>Feet:                                  <7>│)pin",
             R"pin(        <7>│                                        │on your <9>torso and arms <7>is adding         │                                        │)pin",
             R"pin(        <7>│                                        │encumbrance there.                       │                                        │)pin",
             R"pin(        <7>│                                        │                                         │                                        │)pin",
             R"pin(        <7>│                                        │                                         │                                        │)pin",
             R"pin(        <7>│                                        │                                         │                                        │)pin",
             R"pin(        <7>│                                        │ <15>Encumbrance and Warmth                  <7>│                                        │)pin",
             R"pin(        <7>│                                        │ Head     0+0                      <2>(  0) <7>│                                        │)pin",
             R"pin(        <7>│                                        │ Eyes     0+0                      (  0) │                                        │)pin",
             R"pin(        <7>│                                        │ Mouth    0+0                      <2>(  0) <7>│                                        │)pin",
             R"pin(        <7>│                                        │ <2>Torso   <11>12<7>+<11>2                      <2>(  0) <7>│                                        │)pin",
             R"pin(        <7>│                                        │ <2>Arms    <11>12<7>+<11>2                      <2>(  0) <7>│                                        │)pin",
             R"pin(        <7>│                                        │ Hands    0+0                      <2>(  0) <7>│                                        │)pin",
             R"pin(        <7>│                                        │ Legs    <11>16<7>+<11>0                      <2>(  0) <7>│                                        │)pin",
             R"pin(        <7>│                                        │ Feet     0+0                      <2>(  0) <7>│                                        │)pin",
             R"pin(        <7>│                                        │                                         │                                        │)pin",
             R"pin(        <7>│                                        │                                         │                                        │)pin",
             R"pin(        <7>│                                        │                                         │                                        │)pin",
             R"pin(        <7>│(Outermost)                             │                                         │(Outermost)                             │)pin",
             R"pin(        <7>└────────────────────────────────────────┴─────────────────────────────────────────┴────────────────────────────────────────┘)pin",
         }},
        {"examine:first",
         {
             R"pin(                          <5>┌──────────────────────┐<7>┌──────────────────────────────────────┐)pin",
             R"pin(                          <5>│ <15:4>a Activate           <5>│<7>│ <10>|| <7>t-shirt (poor fit)                │)pin",
             R"pin(                          <5>│ <10>R <7>Read               <5>│<7>│                                      │)pin",
             R"pin(                          <5>│ <10>E <7>Eat                <5>│<7>│ <12>Origin: 'Bright Nights'              <7>│)pin",
             R"pin(                          <5>│ <10>W Wear               <5>│<7>│ <8>──────────────────────────────────── <7>│)pin",
             R"pin(                          <5>│ <10>w Wield              <5>│<7>│ Material: <12>Cotton                     <7>│)pin",
             R"pin(                          <5>│ <10>t Throw              <5>│<7>│ Volume: <11>0.500 <7>L  Weight: <11>0.11 <7>kg     │)pin",
             R"pin(                          <5>│ <10>c <9>Change side        <5>│<7>│ Category: <5>CLOTHING                   <7>│)pin",
             R"pin(                          <5>│ <10>T <9>Take off           <5>│<7>│ <8>──────────────────────────────────── <7>│)pin",
             R"pin(                          <5>│ <10>d Drop               <5>│<7>│ A short-sleeved cotton shirt.        │)pin",
             R"pin(                          <5>│ <10>u <7>Unload             <5>│<7>│ <8>──────────────────────────────────── <7>│)pin",
             R"pin(                          <5>│ <10>r <7>Reload             <5>│<7>│ Throw range: <11>24                      <7>│)pin",
             R"pin(                          <5>│ <10>p <7>Part reload        <5>│<7>│ Moves per throw: <11>66                  <7>│)pin",
             R"pin(                          <5>│ <10>m <7>Mend               <5>│<7>│ Stamina cost: <11>56                     <7>│)pin",
             R"pin(                          <5>│ <10>S <9>Salvage            <5>│<7>│ <8>──────────────────────────────────── <7>│)pin",
             R"pin(                          <5>│ <10>D <7>Disassemble        <5>│<7>│ <15>Covers<7>: The <6>torso<7>. The <6>arms<7>.         │)pin",
             R"pin(                          <5>│ <10>f Favorite           <5>│<7>│ Layer: <12>Normal<7>.                       │)pin",
             R"pin(                          <5>│ <10>h Hitchhiker's Guide <5>│<7>│ Warmth: <11>5                            <7>│)pin",
             R"pin(                          <5>│ <10>= Reassign           <5>│<7>│ <8>──────────────────────────────────── <7>│)pin",
             R"pin(                          <5>│ <10>+ Autopickup (add)   <5>│<7>│ <15>Coverage<7>:                            │)pin",
             R"pin(                          <5>└──────────────────────┘<7>│   Arms : <11>30<7>%                         │)pin",
             R"pin(                                                  <7>│   Torso: <11>90<7>%                         │)pin",
             R"pin(                                                  <7>│ <15>Encumbrance<7>: <11>0 <6>(for all parts)       <7>│)pin",
             R"pin(                                                  <7>│ <15>Protection<7>: Bash: <11>1  <7>Cut: <11>1          <7>│)pin",
             R"pin(                                                  <7>│ Ballistic: <11>1                         <7>│)pin",
             R"pin(                                                  <7>│   Acid: <11>0  <7>Fire: <11>0  <7>Environmental:   │)pin",
             R"pin(                                                  <7>│ <11>0                                    <7>│)pin",
             R"pin(                                                  <7>│ <8>──────────────────────────────────── <7>│)pin",
             R"pin(                                                  <7>│ <15>Repair <7>using bone needle, wooden     │)pin",
             R"pin(                                                  <7>│ needle, sewing kit, sewing machine,  │)pin",
             R"pin(                                                  <7>│ or tailor's kit.                     │)pin",
             R"pin(                                                  <7>│ * This item can be <2>reinforced<7>.       │)pin",
             R"pin(                                                  <7>│ <8>──────────────────────────────────── <7>│)pin",
             R"pin(                                                  <7>│ * This item <2>does not conduct         <7>│)pin",
             R"pin(                                                  <7>│ electricity.                         │)pin",
             R"pin(                                                  <7>│ * This clothing <6>can be refitted<7>.     │)pin",
             R"pin(                                                  <7>│ <8>──────────────────────────────────── <7>│)pin",
             R"pin(                                                  <7>│ Price: $<11>8.00  <7>Barter value: $<11>0.50    <7>│)pin",
             R"pin(                                                  <7>│ <8>──────────────────────────────────── <7>│)pin",
             R"pin(                                                  <7>│ You could use it to craft: rioter    │)pin",
             R"pin(                                                  <7>│ mask                                 │)pin",
             R"pin(                                                  <7>│                                      │)pin",
             R"pin(                                                  <7>│                                      │)pin",
             R"pin(                                                  <7>│                                      │)pin",
             R"pin(                                                  <7>│                                      │)pin",
             R"pin(                                                  <7>│                                      │)pin",
             R"pin(                                                  <7>│                                      │)pin",
             R"pin(                                                  <7>│                                      │)pin",
             R"pin(                                                  <7>│                                      │)pin",
             R"pin(                                                  <7>└──────────────────────────────────────┘)pin",
         }},
        {"examine:second",
         {
             R"pin(                          <5>┌──────────────────────┐<7>┌──────────────────────────────────────┐)pin",
             R"pin(                          <5>│ <10>a <9>Activate           <5>│<7>│ <10>|| <7>t-shirt (poor fit)                │)pin",
             R"pin(                          <5>│ <15:4>R Read               <5>│<7>│                                      │)pin",
             R"pin(                          <5>│ <10>E <7>Eat                <5>│<7>│ <12>Origin: 'Bright Nights'              <7>│)pin",
             R"pin(                          <5>│ <10>W Wear               <5>│<7>│ <8>──────────────────────────────────── <7>│)pin",
             R"pin(                          <5>│ <10>w Wield              <5>│<7>│ Material: <12>Cotton                     <7>│)pin",
             R"pin(                          <5>│ <10>t Throw              <5>│<7>│ Volume: <11>0.500 <7>L  Weight: <11>0.11 <7>kg     │)pin",
             R"pin(                          <5>│ <10>c <9>Change side        <5>│<7>│ Category: <5>CLOTHING                   <7>│)pin",
             R"pin(                          <5>│ <10>T <9>Take off           <5>│<7>│ <8>──────────────────────────────────── <7>│)pin",
             R"pin(                          <5>│ <10>d Drop               <5>│<7>│ A short-sleeved cotton shirt.        │)pin",
             R"pin(                          <5>│ <10>u <7>Unload             <5>│<7>│ <8>──────────────────────────────────── <7>│)pin",
             R"pin(                          <5>│ <10>r <7>Reload             <5>│<7>│ Throw range: <11>24                      <7>│)pin",
             R"pin(                          <5>│ <10>p <7>Part reload        <5>│<7>│ Moves per throw: <11>66                  <7>│)pin",
             R"pin(                          <5>│ <10>m <7>Mend               <5>│<7>│ Stamina cost: <11>56                     <7>│)pin",
             R"pin(                          <5>│ <10>S <9>Salvage            <5>│<7>│ <8>──────────────────────────────────── <7>│)pin",
             R"pin(                          <5>│ <10>D <7>Disassemble        <5>│<7>│ <15>Covers<7>: The <6>torso<7>. The <6>arms<7>.         │)pin",
             R"pin(                          <5>│ <10>f Favorite           <5>│<7>│ Layer: <12>Normal<7>.                       │)pin",
             R"pin(                          <5>│ <10>h Hitchhiker's Guide <5>│<7>│ Warmth: <11>5                            <7>│)pin",
             R"pin(                          <5>│ <10>= Reassign           <5>│<7>│ <8>──────────────────────────────────── <7>│)pin",
             R"pin(                          <5>│ <10>+ Autopickup (add)   <5>│<7>│ <15>Coverage<7>:                            │)pin",
             R"pin(                          <5>└──────────────────────┘<7>│   Arms : <11>30<7>%                         │)pin",
             R"pin(                                                  <7>│   Torso: <11>90<7>%                         │)pin",
             R"pin(                                                  <7>│ <15>Encumbrance<7>: <11>0 <6>(for all parts)       <7>│)pin",
             R"pin(                                                  <7>│ <15>Protection<7>: Bash: <11>1  <7>Cut: <11>1          <7>│)pin",
             R"pin(                                                  <7>│ Ballistic: <11>1                         <7>│)pin",
             R"pin(                                                  <7>│   Acid: <11>0  <7>Fire: <11>0  <7>Environmental:   │)pin",
             R"pin(                                                  <7>│ <11>0                                    <7>│)pin",
             R"pin(                                                  <7>│ <8>──────────────────────────────────── <7>│)pin",
             R"pin(                                                  <7>│ <15>Repair <7>using bone needle, wooden     │)pin",
             R"pin(                                                  <7>│ needle, sewing kit, sewing machine,  │)pin",
             R"pin(                                                  <7>│ or tailor's kit.                     │)pin",
             R"pin(                                                  <7>│ * This item can be <2>reinforced<7>.       │)pin",
             R"pin(                                                  <7>│ <8>──────────────────────────────────── <7>│)pin",
             R"pin(                                                  <7>│ * This item <2>does not conduct         <7>│)pin",
             R"pin(                                                  <7>│ electricity.                         │)pin",
             R"pin(                                                  <7>│ * This clothing <6>can be refitted<7>.     │)pin",
             R"pin(                                                  <7>│ <8>──────────────────────────────────── <7>│)pin",
             R"pin(                                                  <7>│ Price: $<11>8.00  <7>Barter value: $<11>0.50    <7>│)pin",
             R"pin(                                                  <7>│ <8>──────────────────────────────────── <7>│)pin",
             R"pin(                                                  <7>│ You could use it to craft: rioter    │)pin",
             R"pin(                                                  <7>│ mask                                 │)pin",
             R"pin(                                                  <7>│                                      │)pin",
             R"pin(                                                  <7>│                                      │)pin",
             R"pin(                                                  <7>│                                      │)pin",
             R"pin(                                                  <7>│                                      │)pin",
             R"pin(                                                  <7>│                                      │)pin",
             R"pin(                                                  <7>│                                      │)pin",
             R"pin(                                                  <7>│                                      │)pin",
             R"pin(                                                  <7>│                                      │)pin",
             R"pin(                                                  <7>└──────────────────────────────────────┘)pin",
         }},
    };
    return screens;
}

auto matches(const std::string& name, const std::vector<std::string>& screen) -> bool {
    return screen == recorded().at(name);
}

void dress(avatar& you) {
    you.wear_item(item::spawn("tshirt"), false);
    you.wear_item(item::spawn("tshirt"), false);
    you.wear_item(item::spawn("jeans"), false);
    you.wear_item(item::spawn("hoodie"), false);
}

} // namespace

TEST_CASE("the native armor layers screen keeps its text and colors", "[armor_native_pin]") {
    const auto guard = screen_guard{};
    auto& you = get_avatar();
    dress(you);
    auto step = 0;
    const auto names = std::vector<std::string>{"all", "part", "part_back", "grabbed", "moved"};
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        REQUIRE(step < 5);
        CHECK(matches("armor:" + names[step], rendered()));
        switch (step++) {
            case 0:
                return act("RIGHT");
            case 1:
                return act("LEFT");
            case 2:
                return act("MOVE_ARMOR");
            case 3:
                return act("DOWN");
            default:
                return act("QUIT");
        }
    });
    show_armor_layers_ui(you);
    CHECK(step == 5);
}

TEST_CASE(
    "the native armor layers screen keeps its text when nothing is worn", "[armor_native_pin]") {
    const auto guard = screen_guard{};
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        CHECK(matches("armor:empty", rendered()));
        return act("QUIT");
    });
    show_armor_layers_ui(get_avatar());
}

TEST_CASE("the native item examine menu keeps its text and colors", "[armor_native_pin]") {
    const auto guard = screen_guard{};
    auto& you = get_avatar();
    auto& shirt = you.i_add(item::spawn("tshirt"));
    auto step = 0;
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto names = std::vector<std::string>{"first", "second"};
        REQUIRE(step < 2);
        CHECK(matches("examine:" + names[step], rendered()));
        return act(step++ == 0 ? "DOWN" : "QUIT");
    });
    examine_item_menu::
        run(shirt, [] { return 50; }, [] { return 40; }, examine_item_menu::menu_pos_t::left);
    CHECK(step == 2);
}

#endif // CATA_MCP
