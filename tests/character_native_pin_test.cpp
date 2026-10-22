#if defined(CATA_MCP)

#    include "avatar.h"
#    include "bionics.h"
#    include "bionics_ui.h"
#    include "calendar.h"
#    include "catch/catch.hpp"
#    include "character_display.h"
#    include "client_command.h"
#    include "client_input.h"
#    include "client_memory.h"
#    include "client_memory_scope.h"
#    include "client_snapshot.h"
#    include "cursesdef.h"
#    include "cursesport.h"
#    include "effect.h"
#    include "game.h"
#    include "game_constants.h"
#    include "input.h"
#    include "mutation.h"
#    include "mutation_ui.h"
#    include "newcharacter.h"
#    include "options_helpers.h"
#    include "output.h"
#    include "player_activity.h"
#    include "state_helpers.h"

#    include <format>
#    include <map>
#    include <string>
#    include <vector>

/// Pins what the native character sheet, bionics list and mutation list print, so that sharing
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
    std::string old_name = get_avatar().name;
    bool old_male = get_avatar().male;
    profession_id old_prof = get_avatar().prof;

    // The state is cleared before the guard turns off test mode: clearing eats, which asks.
    screen_guard() {
        clear_all_state();
        // The header names the avatar, whose name, gender and profession the harness picks at
        // random.
        auto& you = get_avatar();
        you.name = "Tom 'Tom' Tom";
        you.male = true;
        you.prof = profession_id("mall_cop");
        newcharacter::add_default_mutation_type_traits(you);
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
        get_avatar().name = old_name;
        get_avatar().male = old_male;
        get_avatar().prof = old_prof;
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
        {"character:stats",
         {
             R"pin( <15>Tom 'Tom' Tom | Male | Mall Security                                        [<11>?<15>])pin",
             R"pin(          <7:4>STATS           <7>│  ENCUMBRANCE AND WARMTH  │          SPEED           │)pin",
             R"pin( <7:4>Strength:         <15>8 <7>( 8) │ Head     0+0       <2>(  0) <7>│ Base Move Cost:     <2>100  <7>│)pin",
             R"pin( <7>Dexterity:        <15>8 <7>( 8) │ Eyes     0+0       (  0) │ Current Speed:      <2>100  <7>│)pin",
             R"pin( <7>Intelligence:     <15>8 <7>( 8) │ Mouth    0+0       <2>(  0) <7>│                          │)pin",
             R"pin( <7>Perception:       <15>8 <7>( 8) │ Torso    0+0       <2>(  0) <7>│                          │)pin",
             R"pin( <7>Height:           175 cm │ Arms     0+0       <2>(  0) <7>│                          │)pin",
             R"pin( <7>Age:            25 years │ Hands    0+0       <2>(  0) <7>│                          │)pin",
             R"pin(                          <7>│ Legs     0+0       <2>(  0) <7>│                          │)pin",
             R"pin(                          <7>│ Feet     0+0       <2>(  0) <7>│                          │)pin",
             R"pin(<7>──────────────────────────┴──────────────────────────┴──────────────────────────┤)pin",
             R"pin( <5>Strength affects your melee damage, the amount of weight you can carry, your   <7>│)pin",
             R"pin( <5>total HP, your resistance to many diseases, and the effectiveness of actions   <7>│)pin",
             R"pin( <5>which require brute force.                                                     <7>│)pin",
             R"pin( <7>Base HP: <15>84                                                                    <7>│)pin",
             R"pin( <7>Carry weight (kg): <15>9223372036854.8                                             <7>│)pin",
             R"pin( <7>Melee damage: <15>6.0                                                              <7>│)pin",
             R"pin(<7>──────────────────────────┬──────────────────────────┬──────────────────────────┤)pin",
             R"pin(          <7>SKILLS          │          TRAITS          │         EFFECTS          │)pin",
             R"pin(       <11>Melee skills       <7>│ <11>TEST character info visi…<7>│ TEST character info visi…│)pin",
             R"pin( <12>Bashing Weapons:  0 ( 0%)<7>│ <14>Debug Carrying Capacity  <7>├──────────────────────────┘)pin",
             R"pin( <12>Cutting Weapons:  0 ( 0%)<7>├──────────────────────────┤)pin",
             R"pin( <12>Dodging:      2.0/0 ( 0%)<7>│         BIONICS          │)pin",
             R"pin( <12>Melee:            0 ( 0%)<7>│ <15>Bionic Power: <12>0 <15>/ <12>2      <7>│)pin",
             R"pin( <12>Piercing Weapons: 0 ( 0%)<7>│ <6>TEST character info CBM …<7>│)pin",
             R"pin( <12>Unarmed Combat  0/0 ( 0%)<7>├──────────────────────────┘)pin",
             R"pin(      <11>Ranged skills       <7>│)pin",
             R"pin( <12>Archery:          0 ( 0%)<7>│)pin",
             R"pin( <12>Handguns:         0 ( 0%)<7>│)pin",
             R"pin( <12>Launchers:        0 ( 0%)<7>│)pin",
             R"pin( <12>Marksmanship:     0 ( 0%)<7>│)pin",
             R"pin( <12>Rifles:           0 ( 0%)<7>│)pin",
             R"pin( <12>Shotguns:         0 ( 0%)<7>│)pin",
             R"pin( <12>Submachine Guns:  0 ( 0%)<7>│)pin",
             R"pin( <12>Throwing:         0 ( 0%)<7>│)pin",
             R"pin(     <11>Crafting skills      <7>│)pin",
             R"pin( <12>Cooking:          0 ( 0%)<7>│)pin",
             R"pin( <12>Electronics:      0 ( 0%)<7>│)pin",
             R"pin( <12>Fabrication:      0 ( 0%)<7>│)pin",
             R"pin( <12>Survival:         0 ( 0%)<7>│)pin",
             R"pin( <12>Tailoring:        0 ( 0%)<7>│)pin",
             R"pin(    <11>Interaction skills    <7>│)pin",
             R"pin( <12>Athletics:        0 ( 0%)<7>│)pin",
             R"pin( <12>Computers:        0 ( 0%)<7>│)pin",
             R"pin( <12>Driving:          0 ( 0%)<7>│)pin",
             R"pin( <12>First Aid:        0 ( 0%)<7>│)pin",
             R"pin( <12>Mechanics:        0 ( 0%)<7>│)pin",
             R"pin( <12>Trapping:         0 ( 0%)<7>│)pin",
             R"pin(      <11>Social skills       <7>│)pin",
             R"pin( <12>Bartering:        0 ( 0%)<7>│)pin",
             R"pin( <12>Speaking:         0 ( 0%)<7>│)pin",
             R"pin(<7>──────────────────────────┘)pin",
         }},
        {"character:encumbrance",
         {
             R"pin( <15>Tom 'Tom' Tom | Male | Mall Security                                        [<11>?<15>])pin",
             R"pin(          <7>STATS           │  <7:4>ENCUMBRANCE AND WARMTH  <7>│          SPEED           │)pin",
             R"pin( <7>Strength:         <15>8 <7>( 8) │ <7:4>Head     <7>0+0       <2>(  0) <7>│ Base Move Cost:     <2>100  <7>│)pin",
             R"pin( <7>Dexterity:        <15>8 <7>( 8) │ Eyes     0+0       (  0) │ Current Speed:      <2>100  <7>│)pin",
             R"pin( <7>Intelligence:     <15>8 <7>( 8) │ Mouth    0+0       <2>(  0) <7>│                          │)pin",
             R"pin( <7>Perception:       <15>8 <7>( 8) │ Torso    0+0       <2>(  0) <7>│                          │)pin",
             R"pin( <7>Height:           175 cm │ Arms     0+0       <2>(  0) <7>│                          │)pin",
             R"pin( <7>Age:            25 years │ Hands    0+0       <2>(  0) <7>│                          │)pin",
             R"pin(                          <7>│ Legs     0+0       <2>(  0) <7>│                          │)pin",
             R"pin(                          <7>│ Feet     0+0       <2>(  0) <7>│                          │)pin",
             R"pin(<7>──────────────────────────┴──────────────────────────┴──────────────────────────┤)pin",
             R"pin( <5>Head encumbrance has no effect; it simply limits how much you can put on.      <7>│)pin",
             R"pin(                                                                                <7>│)pin",
             R"pin(                                                                                <7>│)pin",
             R"pin(                                                                                <7>│)pin",
             R"pin(                                                                                <7>│)pin",
             R"pin(                                                                                <7>│)pin",
             R"pin(<7>──────────────────────────┬──────────────────────────┬──────────────────────────┤)pin",
             R"pin(          <7>SKILLS          │          TRAITS          │         EFFECTS          │)pin",
             R"pin(       <11>Melee skills       <7>│ <11>TEST character info visi…<7>│ TEST character info visi…│)pin",
             R"pin( <12>Bashing Weapons:  0 ( 0%)<7>│ <14>Debug Carrying Capacity  <7>├──────────────────────────┘)pin",
             R"pin( <12>Cutting Weapons:  0 ( 0%)<7>├──────────────────────────┤)pin",
             R"pin( <12>Dodging:      2.0/0 ( 0%)<7>│         BIONICS          │)pin",
             R"pin( <12>Melee:            0 ( 0%)<7>│ <15>Bionic Power: <12>0 <15>/ <12>2      <7>│)pin",
             R"pin( <12>Piercing Weapons: 0 ( 0%)<7>│ <6>TEST character info CBM …<7>│)pin",
             R"pin( <12>Unarmed Combat  0/0 ( 0%)<7>├──────────────────────────┘)pin",
             R"pin(      <11>Ranged skills       <7>│)pin",
             R"pin( <12>Archery:          0 ( 0%)<7>│)pin",
             R"pin( <12>Handguns:         0 ( 0%)<7>│)pin",
             R"pin( <12>Launchers:        0 ( 0%)<7>│)pin",
             R"pin( <12>Marksmanship:     0 ( 0%)<7>│)pin",
             R"pin( <12>Rifles:           0 ( 0%)<7>│)pin",
             R"pin( <12>Shotguns:         0 ( 0%)<7>│)pin",
             R"pin( <12>Submachine Guns:  0 ( 0%)<7>│)pin",
             R"pin( <12>Throwing:         0 ( 0%)<7>│)pin",
             R"pin(     <11>Crafting skills      <7>│)pin",
             R"pin( <12>Cooking:          0 ( 0%)<7>│)pin",
             R"pin( <12>Electronics:      0 ( 0%)<7>│)pin",
             R"pin( <12>Fabrication:      0 ( 0%)<7>│)pin",
             R"pin( <12>Survival:         0 ( 0%)<7>│)pin",
             R"pin( <12>Tailoring:        0 ( 0%)<7>│)pin",
             R"pin(    <11>Interaction skills    <7>│)pin",
             R"pin( <12>Athletics:        0 ( 0%)<7>│)pin",
             R"pin( <12>Computers:        0 ( 0%)<7>│)pin",
             R"pin( <12>Driving:          0 ( 0%)<7>│)pin",
             R"pin( <12>First Aid:        0 ( 0%)<7>│)pin",
             R"pin( <12>Mechanics:        0 ( 0%)<7>│)pin",
             R"pin( <12>Trapping:         0 ( 0%)<7>│)pin",
             R"pin(      <11>Social skills       <7>│)pin",
             R"pin( <12>Bartering:        0 ( 0%)<7>│)pin",
             R"pin( <12>Speaking:         0 ( 0%)<7>│)pin",
             R"pin(<7>──────────────────────────┘)pin",
         }},
        {"character:skills",
         {
             R"pin( <15>Tom 'Tom' Tom | Male | Mall Security                                        [<11>?<15>])pin",
             R"pin(          <7>STATS           │  ENCUMBRANCE AND WARMTH  │          SPEED           │)pin",
             R"pin( <7>Strength:         <15>8 <7>( 8) │ Head     0+0       <2>(  0) <7>│ Base Move Cost:     <2>100  <7>│)pin",
             R"pin( <7>Dexterity:        <15>8 <7>( 8) │ Eyes     0+0       (  0) │ Current Speed:      <2>100  <7>│)pin",
             R"pin( <7>Intelligence:     <15>8 <7>( 8) │ Mouth    0+0       <2>(  0) <7>│                          │)pin",
             R"pin( <7>Perception:       <15>8 <7>( 8) │ Torso    0+0       <2>(  0) <7>│                          │)pin",
             R"pin( <7>Height:           175 cm │ Arms     0+0       <2>(  0) <7>│                          │)pin",
             R"pin( <7>Age:            25 years │ Hands    0+0       <2>(  0) <7>│                          │)pin",
             R"pin(                          <7>│ Legs     0+0       <2>(  0) <7>│                          │)pin",
             R"pin(                          <7>│ Feet     0+0       <2>(  0) <7>│                          │)pin",
             R"pin(<7>──────────────────────────┴──────────────────────────┴──────────────────────────┤)pin",
             R"pin( <7>Your skill in fighting with blunt weaponry, from rocks and sticks to baseball  │)pin",
             R"pin( <7>bats and the butts of rifles.  Skill increases damage, and higher levels will  │)pin",
             R"pin( <7>improve the accuracy of an attack.                                             │)pin",
             R"pin(                                                                                <7>│)pin",
             R"pin(                                                                                <7>│)pin",
             R"pin(                                                                                <7>│)pin",
             R"pin(<7>──────────────────────────┬──────────────────────────┬──────────────────────────┤)pin",
             R"pin(          <7:4>SKILLS          <7>│          TRAITS          │         EFFECTS          │)pin",
             R"pin(       <11>Melee skills       <7>│ <11>TEST character info visi…<7>│ TEST character info visi…│)pin",
             R"pin( <12:4>Bashing Weapons:  0 ( 0%)<7>│ <14>Debug Carrying Capacity  <7>├──────────────────────────┘)pin",
             R"pin( <12>Cutting Weapons:  0 ( 0%)<7>├──────────────────────────┤)pin",
             R"pin( <12>Dodging:      2.0/0 ( 0%)<7>│         BIONICS          │)pin",
             R"pin( <12>Melee:            0 ( 0%)<7>│ <15>Bionic Power: <12>0 <15>/ <12>2      <7>│)pin",
             R"pin( <12>Piercing Weapons: 0 ( 0%)<7>│ <6>TEST character info CBM …<7>│)pin",
             R"pin( <12>Unarmed Combat  0/0 ( 0%)<7>├──────────────────────────┘)pin",
             R"pin(      <11>Ranged skills       <7>│)pin",
             R"pin( <12>Archery:          0 ( 0%)<7>│)pin",
             R"pin( <12>Handguns:         0 ( 0%)<7>│)pin",
             R"pin( <12>Launchers:        0 ( 0%)<7>│)pin",
             R"pin( <12>Marksmanship:     0 ( 0%)<7>│)pin",
             R"pin( <12>Rifles:           0 ( 0%)<7>│)pin",
             R"pin( <12>Shotguns:         0 ( 0%)<7>│)pin",
             R"pin( <12>Submachine Guns:  0 ( 0%)<7>│)pin",
             R"pin( <12>Throwing:         0 ( 0%)<7>│)pin",
             R"pin(     <11>Crafting skills      <7>│)pin",
             R"pin( <12>Cooking:          0 ( 0%)<7>│)pin",
             R"pin( <12>Electronics:      0 ( 0%)<7>│)pin",
             R"pin( <12>Fabrication:      0 ( 0%)<7>│)pin",
             R"pin( <12>Survival:         0 ( 0%)<7>│)pin",
             R"pin( <12>Tailoring:        0 ( 0%)<7>│)pin",
             R"pin(    <11>Interaction skills    <7>│)pin",
             R"pin( <12>Athletics:        0 ( 0%)<7>│)pin",
             R"pin( <12>Computers:        0 ( 0%)<7>│)pin",
             R"pin( <12>Driving:          0 ( 0%)<7>│)pin",
             R"pin( <12>First Aid:        0 ( 0%)<7>│)pin",
             R"pin( <12>Mechanics:        0 ( 0%)<7>│)pin",
             R"pin( <12>Trapping:         0 ( 0%)<7>│)pin",
             R"pin(      <11>Social skills       <7>│)pin",
             R"pin( <12>Bartering:        0 ( 0%)<7>│)pin",
             R"pin( <12>Speaking:         0 ( 0%)<7>│)pin",
             R"pin(<7>──────────────────────────┘)pin",
         }},
        {"character:traits",
         {
             R"pin( <15>Tom 'Tom' Tom | Male | Mall Security                                        [<11>?<15>])pin",
             R"pin(          <7>STATS           │  ENCUMBRANCE AND WARMTH  │          SPEED           │)pin",
             R"pin( <7>Strength:         <15>8 <7>( 8) │ Head     0+0       <2>(  0) <7>│ Base Move Cost:     <2>100  <7>│)pin",
             R"pin( <7>Dexterity:        <15>8 <7>( 8) │ Eyes     0+0       (  0) │ Current Speed:      <2>100  <7>│)pin",
             R"pin( <7>Intelligence:     <15>8 <7>( 8) │ Mouth    0+0       <2>(  0) <7>│                          │)pin",
             R"pin( <7>Perception:       <15>8 <7>( 8) │ Torso    0+0       <2>(  0) <7>│                          │)pin",
             R"pin( <7>Height:           175 cm │ Arms     0+0       <2>(  0) <7>│                          │)pin",
             R"pin( <7>Age:            25 years │ Hands    0+0       <2>(  0) <7>│                          │)pin",
             R"pin(                          <7>│ Legs     0+0       <2>(  0) <7>│                          │)pin",
             R"pin(                          <7>│ Feet     0+0       <2>(  0) <7>│                          │)pin",
             R"pin(<7>──────────────────────────┴──────────────────────────┴──────────────────────────┤)pin",
             R"pin( <11>TEST character info visible trait<7>: Complete native trait description. The last │)pin",
             R"pin( <7>sentence must remain available beyond the short native detail pane.            │)pin",
             R"pin(                                                                                <7>│)pin",
             R"pin(                                                                                <7>│)pin",
             R"pin(                                                                                <7>│)pin",
             R"pin(                                                                                <7>│)pin",
             R"pin(<7>──────────────────────────┬──────────────────────────┬──────────────────────────┤)pin",
             R"pin(          <7>SKILLS          │          <7:4>TRAITS          <7>│         EFFECTS          │)pin",
             R"pin(       <11>Melee skills       <7>│ <11:4>TEST character info visi…<7>│ TEST character info visi…│)pin",
             R"pin( <12>Bashing Weapons:  0 ( 0%)<7>│ <14>Debug Carrying Capacity  <7>├──────────────────────────┘)pin",
             R"pin( <12>Cutting Weapons:  0 ( 0%)<7>├──────────────────────────┤)pin",
             R"pin( <12>Dodging:      2.0/0 ( 0%)<7>│         BIONICS          │)pin",
             R"pin( <12>Melee:            0 ( 0%)<7>│ <15>Bionic Power: <12>0 <15>/ <12>2      <7>│)pin",
             R"pin( <12>Piercing Weapons: 0 ( 0%)<7>│ <6>TEST character info CBM …<7>│)pin",
             R"pin( <12>Unarmed Combat  0/0 ( 0%)<7>├──────────────────────────┘)pin",
             R"pin(      <11>Ranged skills       <7>│)pin",
             R"pin( <12>Archery:          0 ( 0%)<7>│)pin",
             R"pin( <12>Handguns:         0 ( 0%)<7>│)pin",
             R"pin( <12>Launchers:        0 ( 0%)<7>│)pin",
             R"pin( <12>Marksmanship:     0 ( 0%)<7>│)pin",
             R"pin( <12>Rifles:           0 ( 0%)<7>│)pin",
             R"pin( <12>Shotguns:         0 ( 0%)<7>│)pin",
             R"pin( <12>Submachine Guns:  0 ( 0%)<7>│)pin",
             R"pin( <12>Throwing:         0 ( 0%)<7>│)pin",
             R"pin(     <11>Crafting skills      <7>│)pin",
             R"pin( <12>Cooking:          0 ( 0%)<7>│)pin",
             R"pin( <12>Electronics:      0 ( 0%)<7>│)pin",
             R"pin( <12>Fabrication:      0 ( 0%)<7>│)pin",
             R"pin( <12>Survival:         0 ( 0%)<7>│)pin",
             R"pin( <12>Tailoring:        0 ( 0%)<7>│)pin",
             R"pin(    <11>Interaction skills    <7>│)pin",
             R"pin( <12>Athletics:        0 ( 0%)<7>│)pin",
             R"pin( <12>Computers:        0 ( 0%)<7>│)pin",
             R"pin( <12>Driving:          0 ( 0%)<7>│)pin",
             R"pin( <12>First Aid:        0 ( 0%)<7>│)pin",
             R"pin( <12>Mechanics:        0 ( 0%)<7>│)pin",
             R"pin( <12>Trapping:         0 ( 0%)<7>│)pin",
             R"pin(      <11>Social skills       <7>│)pin",
             R"pin( <12>Bartering:        0 ( 0%)<7>│)pin",
             R"pin( <12>Speaking:         0 ( 0%)<7>│)pin",
             R"pin(<7>──────────────────────────┘)pin",
         }},
        {"character:bionics",
         {
             R"pin( <15>Tom 'Tom' Tom | Male | Mall Security                                        [<11>?<15>])pin",
             R"pin(          <7>STATS           │  ENCUMBRANCE AND WARMTH  │          SPEED           │)pin",
             R"pin( <7>Strength:         <15>8 <7>( 8) │ Head     0+0       <2>(  0) <7>│ Base Move Cost:     <2>100  <7>│)pin",
             R"pin( <7>Dexterity:        <15>8 <7>( 8) │ Eyes     0+0       (  0) │ Current Speed:      <2>100  <7>│)pin",
             R"pin( <7>Intelligence:     <15>8 <7>( 8) │ Mouth    0+0       <2>(  0) <7>│                          │)pin",
             R"pin( <7>Perception:       <15>8 <7>( 8) │ Torso    0+0       <2>(  0) <7>│                          │)pin",
             R"pin( <7>Height:           175 cm │ Arms     0+0       <2>(  0) <7>│                          │)pin",
             R"pin( <7>Age:            25 years │ Hands    0+0       <2>(  0) <7>│                          │)pin",
             R"pin(                          <7>│ Legs     0+0       <2>(  0) <7>│                          │)pin",
             R"pin(                          <7>│ Feet     0+0       <2>(  0) <7>│                          │)pin",
             R"pin(<7>──────────────────────────┴──────────────────────────┴──────────────────────────┤)pin",
             R"pin( <7>Complete native bionic description. This final sentence must not be truncated. │)pin",
             R"pin(                                                                                <7>│)pin",
             R"pin( <7>You have 2 instances of this bionic installed.                                 │)pin",
             R"pin(                                                                                <7>│)pin",
             R"pin(                                                                                <7>│)pin",
             R"pin(                                                                                <7>│)pin",
             R"pin(<7>──────────────────────────┬──────────────────────────┬──────────────────────────┤)pin",
             R"pin(          <7>SKILLS          │          TRAITS          │         EFFECTS          │)pin",
             R"pin(       <11>Melee skills       <7>│ <11>TEST character info visi…<7>│ TEST character info visi…│)pin",
             R"pin( <12>Bashing Weapons:  0 ( 0%)<7>│ <14>Debug Carrying Capacity  <7>├──────────────────────────┘)pin",
             R"pin( <12>Cutting Weapons:  0 ( 0%)<7>├──────────────────────────┤)pin",
             R"pin( <12>Dodging:      2.0/0 ( 0%)<7>│         <7:4>BIONICS          <7>│)pin",
             R"pin( <12>Melee:            0 ( 0%)<7>│ <15>Bionic Power: <12>0 <15>/ <12>2      <7>│)pin",
             R"pin( <12>Piercing Weapons: 0 ( 0%)<7>│ <15:4>TEST character info CBM …<7>│)pin",
             R"pin( <12>Unarmed Combat  0/0 ( 0%)<7>├──────────────────────────┘)pin",
             R"pin(      <11>Ranged skills       <7>│)pin",
             R"pin( <12>Archery:          0 ( 0%)<7>│)pin",
             R"pin( <12>Handguns:         0 ( 0%)<7>│)pin",
             R"pin( <12>Launchers:        0 ( 0%)<7>│)pin",
             R"pin( <12>Marksmanship:     0 ( 0%)<7>│)pin",
             R"pin( <12>Rifles:           0 ( 0%)<7>│)pin",
             R"pin( <12>Shotguns:         0 ( 0%)<7>│)pin",
             R"pin( <12>Submachine Guns:  0 ( 0%)<7>│)pin",
             R"pin( <12>Throwing:         0 ( 0%)<7>│)pin",
             R"pin(     <11>Crafting skills      <7>│)pin",
             R"pin( <12>Cooking:          0 ( 0%)<7>│)pin",
             R"pin( <12>Electronics:      0 ( 0%)<7>│)pin",
             R"pin( <12>Fabrication:      0 ( 0%)<7>│)pin",
             R"pin( <12>Survival:         0 ( 0%)<7>│)pin",
             R"pin( <12>Tailoring:        0 ( 0%)<7>│)pin",
             R"pin(    <11>Interaction skills    <7>│)pin",
             R"pin( <12>Athletics:        0 ( 0%)<7>│)pin",
             R"pin( <12>Computers:        0 ( 0%)<7>│)pin",
             R"pin( <12>Driving:          0 ( 0%)<7>│)pin",
             R"pin( <12>First Aid:        0 ( 0%)<7>│)pin",
             R"pin( <12>Mechanics:        0 ( 0%)<7>│)pin",
             R"pin( <12>Trapping:         0 ( 0%)<7>│)pin",
             R"pin(      <11>Social skills       <7>│)pin",
             R"pin( <12>Bartering:        0 ( 0%)<7>│)pin",
             R"pin( <12>Speaking:         0 ( 0%)<7>│)pin",
             R"pin(<7>──────────────────────────┘)pin",
         }},
        {"character:effects",
         {
             R"pin( <15>Tom 'Tom' Tom | Male | Mall Security                                        [<11>?<15>])pin",
             R"pin(          <7>STATS           │  ENCUMBRANCE AND WARMTH  │          SPEED           │)pin",
             R"pin( <7>Strength:         <15>8 <7>( 8) │ Head     0+0       <2>(  0) <7>│ Base Move Cost:     <2>100  <7>│)pin",
             R"pin( <7>Dexterity:        <15>8 <7>( 8) │ Eyes     0+0       (  0) │ Current Speed:      <2>100  <7>│)pin",
             R"pin( <7>Intelligence:     <15>8 <7>( 8) │ Mouth    0+0       <2>(  0) <7>│                          │)pin",
             R"pin( <7>Perception:       <15>8 <7>( 8) │ Torso    0+0       <2>(  0) <7>│                          │)pin",
             R"pin( <7>Height:           175 cm │ Arms     0+0       <2>(  0) <7>│                          │)pin",
             R"pin( <7>Age:            25 years │ Hands    0+0       <2>(  0) <7>│                          │)pin",
             R"pin(                          <7>│ Legs     0+0       <2>(  0) <7>│                          │)pin",
             R"pin(                          <7>│ Feet     0+0       <2>(  0) <7>│                          │)pin",
             R"pin(<7>──────────────────────────┴──────────────────────────┴──────────────────────────┤)pin",
             R"pin( <7>Complete native effect description, including the final sentence.              │)pin",
             R"pin(                                                                                <7>│)pin",
             R"pin(                                                                                <7>│)pin",
             R"pin(                                                                                <7>│)pin",
             R"pin(                                                                                <7>│)pin",
             R"pin(                                                                                <7>│)pin",
             R"pin(<7>──────────────────────────┬──────────────────────────┬──────────────────────────┤)pin",
             R"pin(          <7>SKILLS          │          TRAITS          │         <7:4>EFFECTS          <7>│)pin",
             R"pin(       <11>Melee skills       <7>│ <11>TEST character info visi…<7>│ <7:4>TEST character info visi…<7>│)pin",
             R"pin( <12>Bashing Weapons:  0 ( 0%)<7>│ <14>Debug Carrying Capacity  <7>├──────────────────────────┘)pin",
             R"pin( <12>Cutting Weapons:  0 ( 0%)<7>├──────────────────────────┤)pin",
             R"pin( <12>Dodging:      2.0/0 ( 0%)<7>│         BIONICS          │)pin",
             R"pin( <12>Melee:            0 ( 0%)<7>│ <15>Bionic Power: <12>0 <15>/ <12>2      <7>│)pin",
             R"pin( <12>Piercing Weapons: 0 ( 0%)<7>│ <6>TEST character info CBM …<7>│)pin",
             R"pin( <12>Unarmed Combat  0/0 ( 0%)<7>├──────────────────────────┘)pin",
             R"pin(      <11>Ranged skills       <7>│)pin",
             R"pin( <12>Archery:          0 ( 0%)<7>│)pin",
             R"pin( <12>Handguns:         0 ( 0%)<7>│)pin",
             R"pin( <12>Launchers:        0 ( 0%)<7>│)pin",
             R"pin( <12>Marksmanship:     0 ( 0%)<7>│)pin",
             R"pin( <12>Rifles:           0 ( 0%)<7>│)pin",
             R"pin( <12>Shotguns:         0 ( 0%)<7>│)pin",
             R"pin( <12>Submachine Guns:  0 ( 0%)<7>│)pin",
             R"pin( <12>Throwing:         0 ( 0%)<7>│)pin",
             R"pin(     <11>Crafting skills      <7>│)pin",
             R"pin( <12>Cooking:          0 ( 0%)<7>│)pin",
             R"pin( <12>Electronics:      0 ( 0%)<7>│)pin",
             R"pin( <12>Fabrication:      0 ( 0%)<7>│)pin",
             R"pin( <12>Survival:         0 ( 0%)<7>│)pin",
             R"pin( <12>Tailoring:        0 ( 0%)<7>│)pin",
             R"pin(    <11>Interaction skills    <7>│)pin",
             R"pin( <12>Athletics:        0 ( 0%)<7>│)pin",
             R"pin( <12>Computers:        0 ( 0%)<7>│)pin",
             R"pin( <12>Driving:          0 ( 0%)<7>│)pin",
             R"pin( <12>First Aid:        0 ( 0%)<7>│)pin",
             R"pin( <12>Mechanics:        0 ( 0%)<7>│)pin",
             R"pin( <12>Trapping:         0 ( 0%)<7>│)pin",
             R"pin(      <11>Social skills       <7>│)pin",
             R"pin( <12>Bartering:        0 ( 0%)<7>│)pin",
             R"pin( <12>Speaking:         0 ( 0%)<7>│)pin",
             R"pin(<7>──────────────────────────┘)pin",
         }},
        {"bionics:active",
         {
             R"pin(     <7>┌─────────────────────────────────────── <9>BIONICS <7>────────────────┬───────────────────────┐)pin",
             R"pin(     <7>│ <2>Activating  <15>[<11>!<15>] Examine, [<11>=<15>] Reassign, [<11>TAB<15>] Switch tabs, [<11>S<15>]  <7>│<15>Bionic Power: <12>0J<15>/<12>101kJ <7>│)pin",
             R"pin(     <7>│ <15>Toggle fuel saving mode, [<11>H<15>] Toggle sprite visibility, [<11>A<15>]     <7>└───────────────────────┤)pin",
             R"pin(     <7>│ <15>Toggle auto start mode. [<11>s<15>] Sort: Power usage                                          <7>│)pin",
             R"pin(     <7>│ ┌──────────┐ ┌───────────┐                                                             │)pin",
             R"pin(     <7>│<7:4><<7>│<7:4>ACTIVE (1)<7>│<7:4>><7>│PASSIVE (2)│                                                             │)pin",
             R"pin(     <7>├─┘          └─┴───────────┴─────────────────────────────────────────────────────────────┤)pin",
             R"pin(     <7>│ <9:4>a Cranial Flashlight, 8 J act, 8 J/turn, OFF <7>>─────────────────┐          Head: 0/18   │)pin",
             R"pin(     <7>│                                                                └──<11>(1)<7>───< <11>Eyes: 1/4    <7>│)pin",
             R"pin(     <7>│                                                                           Mouth: 0/4   │)pin",
             R"pin(     <7>│                                                                           Torso: 0/80  │)pin",
             R"pin(     <7>│                                                                           L. Arm: 0/20 │)pin",
             R"pin(     <7>│                                                                           L. Hand: 0/5 │)pin",
             R"pin(     <7>│                                                                           R. Arm: 0/20 │)pin",
             R"pin(     <7>│                                                                           R. Hand: 0/5 │)pin",
             R"pin(     <7>│                                                                           L. Leg: 0/30 │)pin",
             R"pin(     <7>│                                                                           L. Foot: 0/7 │)pin",
             R"pin(     <7>│                                                                           R. Leg: 0/30 │)pin",
             R"pin(     <7>│                                                                           R. Foot: 0/7 │)pin",
             R"pin(     <7>│                                                                                        │)pin",
             R"pin(     <7>│                                                                                        │)pin",
             R"pin(     <7>│                                                                                        │)pin",
             R"pin(     <7>│                                                                                        │)pin",
             R"pin(     <7>└────────────────────────────────────────────────────────────────────────────────────────┘)pin",
         }},
        {"bionics:passive",
         {
             R"pin(     <7>┌─────────────────────────────────────── <9>BIONICS <7>────────────────┬───────────────────────┐)pin",
             R"pin(     <7>│ <2>Activating  <15>[<11>!<15>] Examine, [<11>=<15>] Reassign, [<11>TAB<15>] Switch tabs, [<11>S<15>]  <7>│<15>Bionic Power: <12>0J<15>/<12>101kJ <7>│)pin",
             R"pin(     <7>│ <15>Toggle fuel saving mode, [<11>H<15>] Toggle sprite visibility, [<11>A<15>]     <7>└───────────────────────┤)pin",
             R"pin(     <7>│ <15>Toggle auto start mode. [<11>s<15>] Sort: Power usage                                          <7>│)pin",
             R"pin(     <7>│ ┌──────────┐ ┌───────────┐                                                             │)pin",
             R"pin(     <7>│ │ACTIVE (1)│<7:4><<7>│<7:4>PASSIVE (2)<7>│<7:4>>                                                            <7>│)pin",
             R"pin(     <7>├─┴──────────┴─┘           └─────────────────────────────────────────────────────────────┤)pin",
             R"pin(     <7>│   <6:4>Power Storage                                                           <7>Head: 0/18   │)pin",
             R"pin(     <7>│   <6>TEST character info CBM                                                 <7>Eyes: 1/4    │)pin",
             R"pin(     <7>│                                                                           Mouth: 0/4   │)pin",
             R"pin(     <7>│                                                                           Torso: 0/80  │)pin",
             R"pin(     <7>│                                                                           L. Arm: 0/20 │)pin",
             R"pin(     <7>│                                                                           L. Hand: 0/5 │)pin",
             R"pin(     <7>│                                                                           R. Arm: 0/20 │)pin",
             R"pin(     <7>│                                                                           R. Hand: 0/5 │)pin",
             R"pin(     <7>│                                                                           L. Leg: 0/30 │)pin",
             R"pin(     <7>│                                                                           L. Foot: 0/7 │)pin",
             R"pin(     <7>│                                                                           R. Leg: 0/30 │)pin",
             R"pin(     <7>│                                                                           R. Foot: 0/7 │)pin",
             R"pin(     <7>│                                                                                        │)pin",
             R"pin(     <7>│                                                                                        │)pin",
             R"pin(     <7>│                                                                                        │)pin",
             R"pin(     <7>│                                                                                        │)pin",
             R"pin(     <7>└────────────────────────────────────────────────────────────────────────────────────────┘)pin",
         }},
        {"bionics:examining",
         {
             R"pin(     <7>┌─────────────────────────────────────── <9>BIONICS <7>────────────────┬───────────────────────┐)pin",
             R"pin(     <7>│ <12>Examining  <15>[<11>!<15>] Activate, [<11>=<15>] Reassign, [<11>TAB<15>] Switch tabs, [<11>S<15>]  <7>│<15>Bionic Power: <12>0J<15>/<12>101kJ <7>│)pin",
             R"pin(     <7>│ <15>Toggle fuel saving mode, [<11>H<15>] Toggle sprite visibility, [<11>A<15>]     <7>└───────────────────────┤)pin",
             R"pin(     <7>│ <15>Toggle auto start mode. [<11>s<15>] Sort: Power usage                                          <7>│)pin",
             R"pin(     <7>│ ┌──────────┐ ┌───────────┐                                                             │)pin",
             R"pin(     <7>│ │ACTIVE (1)│<7:4><<7>│<7:4>PASSIVE (2)<7>│<7:4>>                                                            <7>│)pin",
             R"pin(     <7>├─┴──────────┴─┘           └─────────────────────────────────────────────────────────────┤)pin",
             R"pin(     <7>│   <6:4>Power Storage                                                           <7>Head: 0/18   │)pin",
             R"pin(     <7>│   <6>TEST character info CBM              <15>Power Storage                                   <7>│)pin",
             R"pin(     <7>│                                        <12>A Compact Bionics Module that increases your    <7>│)pin",
             R"pin(     <7>│                                        <12>power capacity by 100 kJ.  Having at least one  <7>│)pin",
             R"pin(     <7>│                                        <12>of these is a prerequisite to using powered     <7>│)pin",
             R"pin(     <7>│                                        <12>bionics.  You will also need a power supply,    <7>│)pin",
             R"pin(     <7>│                                        <12>found in various CBMs.                          <7>│)pin",
             R"pin(     <7>│                                                                                        │)pin",
             R"pin(     <7>│                                                                                        │)pin",
             R"pin(     <7>│                                                                                        │)pin",
             R"pin(     <7>│                                                                                        │)pin",
             R"pin(     <7>│                                                                                        │)pin",
             R"pin(     <7>│                                                                                        │)pin",
             R"pin(     <7>│                                                                                        │)pin",
             R"pin(     <7>│                                                                                        │)pin",
             R"pin(     <7>│                                                                                        │)pin",
             R"pin(     <7>└────────────────────────────────────────────────────────────────────────────────────────┘)pin",
         }},
        {"mutations:passive",
         {
             R"pin(     <7>┌────────────────────────────────────── <9>MUTATIONS <7>───────────────────────────────────────┐)pin",
             R"pin(     <7>│ <2>Activating  <15>[<11>!<15>] to examine mutation, [<11>=<15>] to reassign invlet, [<11>,<15>] to toggle sprite      <7>│)pin",
             R"pin(     <7>│ <15>visibility, [<11>?<15>] to change keybindings.                                                 <7>│)pin",
             R"pin(     <7>├────────────────────────────────────────────────────────────────────────────────────────┤)pin",
             R"pin(     <7>│ <12>Passive:                           Active:                                             <7>│)pin",
             R"pin(     <7>│ <14:3>a Debug Carrying Capacity          <10>c TEST active preview trait - Active                <7>│)pin",
             R"pin(     <7>│ <6>b Skin tone: lighter                                                                   <7>│)pin",
             R"pin(     <7>│ <14>d TEST preview trait                                                                   <7>│)pin",
             R"pin(     <7>│                                                                                        │)pin",
             R"pin(     <7>│                                                                                        │)pin",
             R"pin(     <7>│                                                                                        │)pin",
             R"pin(     <7>│                                                                                        │)pin",
             R"pin(     <7>│                                                                                        │)pin",
             R"pin(     <7>│                                                                                        │)pin",
             R"pin(     <7>│                                                                                        │)pin",
             R"pin(     <7>│                                                                                        │)pin",
             R"pin(     <7>│                                                                                        │)pin",
             R"pin(     <7>│                                                                                        │)pin",
             R"pin(     <7>│                                                                                        │)pin",
             R"pin(     <7>│                                                                                        │)pin",
             R"pin(     <7>│                                                                                        │)pin",
             R"pin(     <7>│                                                                                        │)pin",
             R"pin(     <7>│                                                                                        │)pin",
             R"pin(     <7>└────────────────────────────────────────────────────────────────────────────────────────┘)pin",
         }},
        {"mutations:examining",
         {
             R"pin(     <7>┌────────────────────────────────────── <9>MUTATIONS <7>───────────────────────────────────────┐)pin",
             R"pin(     <7>│ <12>Examining  <15>[<11>!<15>] to activate mutation, [<11>=<15>] to reassign invlet, [<11>,<15>] to toggle sprite      <7>│)pin",
             R"pin(     <7>│ <15>visibility, [<11>?<15>] to change keybindings.                                                 <7>│)pin",
             R"pin(     <7>├────────────────────────────────────────────────────────────────────────────────────────┤)pin",
             R"pin(     <7>│ <12>Passive:                           Active:                                             <7>│)pin",
             R"pin(     <7>│ <14:3>a Debug Carrying Capacity          <10>c TEST active preview trait - Active                <7>│)pin",
             R"pin(     <7>│ <6>b Skin tone: lighter                                                                   <7>│)pin",
             R"pin(     <7>│ <14>d TEST preview trait                                                                   <7>│)pin",
             R"pin(     <7>│                                                                                        │)pin",
             R"pin(     <7>│                                                                                        │)pin",
             R"pin(     <7>│                                                                                        │)pin",
             R"pin(     <7>│                                                                                        │)pin",
             R"pin(     <7>│                                                                                        │)pin",
             R"pin(     <7>│                                                                                        │)pin",
             R"pin(     <7>│                                                                                        │)pin",
             R"pin(     <7>│                                                                                        │)pin",
             R"pin(     <7>│                                                                                        │)pin",
             R"pin(     <7>├────────────────────────────────────────────────────────────────────────────────────────┤)pin",
             R"pin(     <7>│                                                                                        │)pin",
             R"pin(     <7>│                                                                                        │)pin",
             R"pin(     <7>│                                                                                        │)pin",
             R"pin(     <7>│                                                                                        │)pin",
             R"pin(     <7>│                                                                                        │)pin",
             R"pin(     <7>└────────────────────────────────────────────────────────────────────────────────────────┘)pin",
         }},
        {"mutations:active",
         {
             R"pin(     <7>┌────────────────────────────────────── <9>MUTATIONS <7>───────────────────────────────────────┐)pin",
             R"pin(     <7>│ <12>Examining  <15>[<11>!<15>] to activate mutation, [<11>=<15>] to reassign invlet, [<11>,<15>] to toggle sprite      <7>│)pin",
             R"pin(     <7>│ <15>visibility, [<11>?<15>] to change keybindings.                                                 <7>│)pin",
             R"pin(     <7>├────────────────────────────────────────────────────────────────────────────────────────┤)pin",
             R"pin(     <7>│ <12>Passive:                           Active:                                             <7>│)pin",
             R"pin(     <7>│ <14>a Debug Carrying Capacity          <10:3>c TEST active preview trait - Active                <7>│)pin",
             R"pin(     <7>│ <6>b Skin tone: lighter                                                                   <7>│)pin",
             R"pin(     <7>│ <14>d TEST preview trait                                                                   <7>│)pin",
             R"pin(     <7>│                                                                                        │)pin",
             R"pin(     <7>│                                                                                        │)pin",
             R"pin(     <7>│                                                                                        │)pin",
             R"pin(     <7>│                                                                                        │)pin",
             R"pin(     <7>│                                                                                        │)pin",
             R"pin(     <7>│                                                                                        │)pin",
             R"pin(     <7>│                                                                                        │)pin",
             R"pin(     <7>│                                                                                        │)pin",
             R"pin(     <7>│                                                                                        │)pin",
             R"pin(     <7>├────────────────────────────────────────────────────────────────────────────────────────┤)pin",
             R"pin(     <7>│<12>A visible powered preview trait without gameplay modifiers.                             <7>│)pin",
             R"pin(     <7>│                                                                                        │)pin",
             R"pin(     <7>│                                                                                        │)pin",
             R"pin(     <7>│                                                                                        │)pin",
             R"pin(     <7>│                                                                                        │)pin",
             R"pin(     <7>└────────────────────────────────────────────────────────────────────────────────────────┘)pin",
         }},
    };
    return screens;
}

auto matches(const std::string& name, const std::vector<std::string>& screen) -> bool {
    return screen == recorded().at(name);
}

/// Walks the mutation list through its tabs, checking each against its pin.
auto check_native_mutation_list() -> void {
    const auto guard = screen_guard{};
    auto& you = get_avatar();
    you.set_mutation(trait_id("test_preview_trait"));
    you.set_mutation(trait_id("test_preview_active_trait"));
    auto step = 0;
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto names = std::vector<std::string>{"passive", "examining", "active"};
        CHECK(matches("mutations:" + names[step], rendered()));
        switch (step++) {
            case 0:
                return act("TOGGLE_EXAMINE");
            case 1:
                return act("NEXT_TAB");
            case 2:
                return act("QUIT");
            default:
                break;
        }
        FAIL("unexpected native read");
        return act("QUIT");
    });
    show_mutations_ui(you);
    CHECK(step == 3);
}

} // namespace

TEST_CASE(
    "the native character sheet keeps its text and colors on every tab", "[character_native_pin]") {
    const auto guard = screen_guard{};
    auto& you = get_avatar();
    you.set_mutation(trait_id("test_character_info_visible_trait"));
    you.add_effect(efftype_id("test_character_info_visible_effect"), 10_turns);
    you.add_bionic(bionic_id("test_character_info_cbm"));
    you.add_bionic(bionic_id("test_character_info_cbm"));
    auto step = 0;
    const auto names =
        std::vector<std::string>{"stats", "encumbrance", "skills", "traits", "bionics", "effects"};
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        REQUIRE(step < 6);
        CHECK(matches("character:" + names[step], rendered()));
        return act(++step < 6 ? "NEXT_TAB" : "QUIT");
    });
    character_display::disp_info(you);
    CHECK(step == 6);
}

TEST_CASE("the native bionics list keeps its text and colors", "[character_native_pin]") {
    const auto guard = screen_guard{};
    const auto slots = override_option{"CBM_SLOTS_ENABLED", "true"};
    auto& you = get_avatar();
    you.add_bionic(bionic_id("bio_flashlight"));
    you.add_bionic(bionic_id("bio_power_storage"));
    you.add_bionic(bionic_id("test_character_info_cbm"));
    auto step = 0;
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto names = std::vector<std::string>{"active", "passive", "examining"};
        CHECK(matches("bionics:" + names[step], rendered()));
        switch (step++) {
            case 0:
                return act("NEXT_TAB");
            case 1:
                return act("TOGGLE_EXAMINE");
            case 2:
                return act("QUIT");
            default:
                break;
        }
        FAIL("unexpected native read");
        return act("QUIT");
    });
    show_bionics_ui(you);
    CHECK(step == 3);
}

TEST_CASE("the native mutation list keeps its text and colors", "[character_native_pin]") {
    check_native_mutation_list();
}

TEST_CASE(
    "a wait left running draws nothing over the native mutation list", "[character_native_pin]") {
    clear_all_state();
    const auto no_autosave = override_option("AUTOSAVE", "false");
    const auto keep_bubble =
        override_option("REALITY_BUBBLE_SIZE", std::to_string(g_reality_bubble_size));
    const auto no_mobile = override_option("ACTIVITY_MOBILE_BUBBLE_SIZE", "0");
    const auto no_idle = override_option("ACTIVITY_IDLE_BUBBLE_SIZE", "0");
    const auto no_underground = override_option("UNDERGROUND_BUBBLE_SIZE", "0");
    const auto no_vehicle = override_option("VEHICLE_BUBBLE_SIZE", "0");
    const auto no_combat = override_option("COMBAT_BUBBLE_SIZE", "0");
    get_avatar().set_moves(100);
    get_avatar().assign_activity(activity_id("ACT_WAIT"), to_moves<int>(1_hours), 0);
    g->do_turn();
    REQUIRE(get_avatar().activity);
    check_native_mutation_list();
}

#endif // CATA_MCP
