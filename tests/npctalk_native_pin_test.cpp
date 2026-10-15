#if defined(CATA_MCP)

#    include "catch/catch.hpp"
#    include "client_memory.h"
#    include "client_snapshot.h"
#    include "dialogue_helpers.h"

#    include <format>
#    include <map>
#    include <string>
#    include <vector>

/// Pins what the native NPC talk window prints, so that publishing the dialogue as components keeps
/// the player's screen byte-equal: the header, the history log and every response with its letter,
/// its trial chance and its colour runs.
namespace {
using namespace dialogue_test;

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
        {"talk:start",
         {
             R"pin(<7>┌──────────────────────────────────────────────────────────────────────────────┐)pin",
             R"pin(<7>│<15>Dialogue: <10>Adapter talker                        <7>[L] Look at  [S] Size up stats│)pin",
             R"pin(<7>│                                                [Y] Yell     [O] Check opinion│)pin",
             R"pin(<7>│──────────────────────────────────────────────┬───────────────────────────────│)pin",
             R"pin(<7>│                                              │ <15>Your response:                <7>│)pin",
             R"pin(<7>│                                              │                               │)pin",
             R"pin(<7>│                                              │ <15:4>a Identical adapter response  <7>│)pin",
             R"pin(<7>│                                              │   <15:4>labels remain separate, even<7>│)pin",
             R"pin(<7>│                                              │   <15:4>when they span several lines<7>│)pin",
             R"pin(<7>│                                              │   <15:4>in the native response      <7>│)pin",
             R"pin(<7>│                                              │   <15:4>panel.                      <7>│)pin",
             R"pin(<7>│                                              │ <10>b <7>Identical adapter response  │)pin",
             R"pin(<7>│                                              │   labels remain separate, even│)pin",
             R"pin(<7>│                                              │   when they span several lines│)pin",
             R"pin(<7>│                                              │   in the native response      │)pin",
             R"pin(<7>│                                              │   panel.                      │)pin",
             R"pin(<7>│                                              │ <10>c <7>Even the same next topic can│)pin",
             R"pin(<7>│                                              │   have different effects.     │)pin",
             R"pin(<7>│                                              │ <10>d <7>Even the same next topic can│)pin",
             R"pin(<7>│                                              │   have different effects.     │)pin",
             R"pin(<7>│                                              │ <10>e <7>[LIE 100%] Successful native│)pin",
             R"pin(<7>│<10>Adapter talker<7>: "Adapter narrative, already   │   trial, cost 17 cents.       │)pin",
             R"pin(<7>│evaluated: bloody."                           │                               │)pin",
             R"pin(<7>└──────────────────────────────────────────────┴───────────────────< Page 1/2 >┘)pin",
         }},
        {"talk:down",
         {
             R"pin(<7>┌──────────────────────────────────────────────────────────────────────────────┐)pin",
             R"pin(<7>│<15>Dialogue: <10>Adapter talker                        <7>[L] Look at  [S] Size up stats│)pin",
             R"pin(<7>│                                                [Y] Yell     [O] Check opinion│)pin",
             R"pin(<7>│──────────────────────────────────────────────┬───────────────────────────────│)pin",
             R"pin(<7>│                                              │ <15>Your response:                <7>│)pin",
             R"pin(<7>│                                              │                               │)pin",
             R"pin(<7>│                                              │ <10>a <7>Identical adapter response  │)pin",
             R"pin(<7>│                                              │   labels remain separate, even│)pin",
             R"pin(<7>│                                              │   when they span several lines│)pin",
             R"pin(<7>│                                              │   in the native response      │)pin",
             R"pin(<7>│                                              │   panel.                      │)pin",
             R"pin(<7>│                                              │ <15:4>b Identical adapter response  <7>│)pin",
             R"pin(<7>│                                              │   <15:4>labels remain separate, even<7>│)pin",
             R"pin(<7>│                                              │   <15:4>when they span several lines<7>│)pin",
             R"pin(<7>│                                              │   <15:4>in the native response      <7>│)pin",
             R"pin(<7>│                                              │   <15:4>panel.                      <7>│)pin",
             R"pin(<7>│                                              │ <10>c <7>Even the same next topic can│)pin",
             R"pin(<7>│                                              │   have different effects.     │)pin",
             R"pin(<7>│                                              │ <10>d <7>Even the same next topic can│)pin",
             R"pin(<7>│                                              │   have different effects.     │)pin",
             R"pin(<7>│                                              │ <10>e <7>[LIE 100%] Successful native│)pin",
             R"pin(<7>│<10>Adapter talker<7>: "Adapter narrative, already   │   trial, cost 17 cents.       │)pin",
             R"pin(<7>│evaluated: bloody."                           │                               │)pin",
             R"pin(<7>└──────────────────────────────────────────────┴───────────────────< Page 1/2 >┘)pin",
         }},
        {"talk:down2",
         {
             R"pin(<7>┌──────────────────────────────────────────────────────────────────────────────┐)pin",
             R"pin(<7>│<15>Dialogue: <10>Adapter talker                        <7>[L] Look at  [S] Size up stats│)pin",
             R"pin(<7>│                                                [Y] Yell     [O] Check opinion│)pin",
             R"pin(<7>│──────────────────────────────────────────────┬───────────────────────────────│)pin",
             R"pin(<7>│                                              │ <15>Your response:                <7>│)pin",
             R"pin(<7>│                                              │                               │)pin",
             R"pin(<7>│                                              │ <10>a <7>Identical adapter response  │)pin",
             R"pin(<7>│                                              │   labels remain separate, even│)pin",
             R"pin(<7>│                                              │   when they span several lines│)pin",
             R"pin(<7>│                                              │   in the native response      │)pin",
             R"pin(<7>│                                              │   panel.                      │)pin",
             R"pin(<7>│                                              │ <10>b <7>Identical adapter response  │)pin",
             R"pin(<7>│                                              │   labels remain separate, even│)pin",
             R"pin(<7>│                                              │   when they span several lines│)pin",
             R"pin(<7>│                                              │   in the native response      │)pin",
             R"pin(<7>│                                              │   panel.                      │)pin",
             R"pin(<7>│                                              │ <15:4>c Even the same next topic can<7>│)pin",
             R"pin(<7>│                                              │   <15:4>have different effects.     <7>│)pin",
             R"pin(<7>│                                              │ <10>d <7>Even the same next topic can│)pin",
             R"pin(<7>│                                              │   have different effects.     │)pin",
             R"pin(<7>│                                              │ <10>e <7>[LIE 100%] Successful native│)pin",
             R"pin(<7>│<10>Adapter talker<7>: "Adapter narrative, already   │   trial, cost 17 cents.       │)pin",
             R"pin(<7>│evaluated: bloody."                           │                               │)pin",
             R"pin(<7>└──────────────────────────────────────────────┴───────────────────< Page 1/2 >┘)pin",
         }},
        {"talk:page",
         {
             R"pin(<7>┌──────────────────────────────────────────────────────────────────────────────┐)pin",
             R"pin(<7>│<15>Dialogue: <10>Adapter talker                        <7>[L] Look at  [S] Size up stats│)pin",
             R"pin(<7>│                                                [Y] Yell     [O] Check opinion│)pin",
             R"pin(<7>│──────────────────────────────────────────────┬───────────────────────────────│)pin",
             R"pin(<7>│                                              │ <15>Your response:                <7>│)pin",
             R"pin(<7>│                                              │                               │)pin",
             R"pin(<7>│                                              │ <15:4>f [LIE 0%] Denied native      <7>│)pin",
             R"pin(<7>│                                              │   <15:4>trial.                      <7>│)pin",
             R"pin(<7>│                                              │ <10>g <7>Only displayed before the   │)pin",
             R"pin(<7>│                                              │   conditional response is     │)pin",
             R"pin(<7>│                                              │   removed.                    │)pin",
             R"pin(<7>│                                              │ <10>h <1>Provoke the NPC, with native<7>│)pin",
             R"pin(<7>│                                              │   <1>confirmation.               <7>│)pin",
             R"pin(<7>│                                              │ <10>i <7>Goodbye.                    │)pin",
             R"pin(<7>│                                              │                               │)pin",
             R"pin(<7>│                                              │                               │)pin",
             R"pin(<7>│                                              │                               │)pin",
             R"pin(<7>│                                              │                               │)pin",
             R"pin(<7>│                                              │                               │)pin",
             R"pin(<7>│                                              │                               │)pin",
             R"pin(<7>│                                              │                               │)pin",
             R"pin(<7>│<10>Adapter talker<7>: "Adapter narrative, already   │                               │)pin",
             R"pin(<7>│evaluated: bloody."                           │                               │)pin",
             R"pin(<7>└──────────────────────────────────────────────┴───────────────────< Page 2/2 >┘)pin",
         }},
        {"talk:history",
         {
             R"pin(<7>┌──────────────────────────────────────────────────────────────────────────────┐)pin",
             R"pin(<7>│<15>Dialogue: <10>Adapter talker                        <7>[L] Look at  [S] Size up stats│)pin",
             R"pin(<7>│                                                [Y] Yell     [O] Check opinion│)pin",
             R"pin(<7>│──────────────────────────────────────────────┬───────────────────────────────│)pin",
             R"pin(<7>│                                              │ <15>Your response:                <7>│)pin",
             R"pin(<7>│                                              │                               │)pin",
             R"pin(<7>│                                              │ <15:4>a Back to the previous topic. <7>│)pin",
             R"pin(<7>│                                              │ <10>b <7>Goodbye.                    │)pin",
             R"pin(<7>│                                              │                               │)pin",
             R"pin(<7>│                                              │                               │)pin",
             R"pin(<7>│                                              │                               │)pin",
             R"pin(<7>│                                              │                               │)pin",
             R"pin(<7>│                                              │                               │)pin",
             R"pin(<7>│                                              │                               │)pin",
             R"pin(<7>│                                              │                               │)pin",
             R"pin(<7>│<10>Adapter talker<7>: "Adapter narrative, already   │                               │)pin",
             R"pin(<7>│evaluated: bloody."                           │                               │)pin",
             R"pin(<7>│                                              │                               │)pin",
             R"pin(<7>│<2>You<15>: Identical adapter response labels remain <7>│                               │)pin",
             R"pin(<7>│<15>separate, even when they span several lines in<7>│                               │)pin",
             R"pin(<7>│<15>the native response panel.                    <7>│                               │)pin",
             R"pin(<7>│                                              │                               │)pin",
             R"pin(<7>│<10>Adapter talker<15>: "Second native branch."       <7>│                               │)pin",
             R"pin(<7>└──────────────────────────────────────────────┴───────────────────< Page 1/1 >┘)pin",
         }},
        {"talk:colors",
         {
             R"pin(<7>┌──────────────────────────────────────────────────────────────────────────────┐)pin",
             R"pin(<7>│<15>Dialogue: <10>Adapter talker                        <7>[L] Look at  [S] Size up stats│)pin",
             R"pin(<7>│                                                [Y] Yell     [O] Check opinion│)pin",
             R"pin(<7>│──────────────────────────────────────────────┬───────────────────────────────│)pin",
             R"pin(<7>│                                              │ <15>Your response:                <7>│)pin",
             R"pin(<7>│                                              │                               │)pin",
             R"pin(<7>│                                              │ <15:4>a Plain response.             <7>│)pin",
             R"pin(<7>│                                              │ <10>b <9>*Acts helpless.             <7>│)pin",
             R"pin(<7>│                                              │ <10>c <2>&Acts out loud.             <7>│)pin",
             R"pin(<7>│                                              │ <10>d <7>[PERSUADE 28%] Persuade the │)pin",
             R"pin(<7>│                                              │   NPC.                        │)pin",
             R"pin(<7>│                                              │ <10>e <7>[INTIMIDATE 24%] Intimidate │)pin",
             R"pin(<7>│                                              │   the NPC.                    │)pin",
             R"pin(<7>│                                              │ <10>f <7>Goodbye.                    │)pin",
             R"pin(<7>│                                              │                               │)pin",
             R"pin(<7>│                                              │                               │)pin",
             R"pin(<7>│                                              │                               │)pin",
             R"pin(<7>│                                              │                               │)pin",
             R"pin(<7>│                                              │                               │)pin",
             R"pin(<7>│                                              │                               │)pin",
             R"pin(<7>│                                              │                               │)pin",
             R"pin(<7>│                                              │                               │)pin",
             R"pin(<7>│<10>Adapter talker<7>: "Pinned narrative."           │                               │)pin",
             R"pin(<7>└──────────────────────────────────────────────┴───────────────────< Page 1/1 >┘)pin",
         }},
        {"talk:empty",
         {
             R"pin(<7>┌──────────────────────────────────────────────────────────────────────────────┐)pin",
             R"pin(<7>│<15>Dialogue: <10>Adapter talker                        <7>[L] Look at  [S] Size up stats│)pin",
             R"pin(<7>│                                                [Y] Yell     [O] Check opinion│)pin",
             R"pin(<7>│──────────────────────────────────────────────┬───────────────────────────────│)pin",
             R"pin(<7>│                                              │ <15>Your response:                <7>│)pin",
             R"pin(<7>│                                              │                               │)pin",
             R"pin(<7>│                                              │                               │)pin",
             R"pin(<7>│                                              │                               │)pin",
             R"pin(<7>│                                              │                               │)pin",
             R"pin(<7>│                                              │                               │)pin",
             R"pin(<7>│                                              │                               │)pin",
             R"pin(<7>│                                              │                               │)pin",
             R"pin(<7>│                                              │                               │)pin",
             R"pin(<7>│                                              │                               │)pin",
             R"pin(<7>│                                              │                               │)pin",
             R"pin(<7>│                                              │                               │)pin",
             R"pin(<7>│                                              │                               │)pin",
             R"pin(<7>│                                              │                               │)pin",
             R"pin(<7>│                                              │                               │)pin",
             R"pin(<7>│                                              │                               │)pin",
             R"pin(<7>│                                              │                               │)pin",
             R"pin(<7>│<10>Adapter talker<7>: "No regular response is       │                               │)pin",
             R"pin(<7>│currently available."                         │                               │)pin",
             R"pin(<7>└──────────────────────────────────────────────┴───────────────────────────────┘)pin",
         }},
        {"talk:wide",
         {
             R"pin(               <7>┌────────────────────────────────────────────────────────────────────────────────────────────────────────────┐)pin",
             R"pin(               <7>│<15>Dialogue: <10>Adapter talker                                                      <7>[L] Look at  [S] Size up stats│)pin",
             R"pin(               <7>│                                                                              [Y] Yell     [O] Check opinion│)pin",
             R"pin(               <7>│────────────────────────────────────────────────────────────────┬───────────────────────────────────────────│)pin",
             R"pin(               <7>│                                                                │ <15>Your response:                            <7>│)pin",
             R"pin(               <7>│                                                                │                                           │)pin",
             R"pin(               <7>│                                                                │ <15:4>a Identical adapter response labels remain<7>│)pin",
             R"pin(               <7>│                                                                │   <15:4>separate, even when they span several   <7>│)pin",
             R"pin(               <7>│                                                                │   <15:4>lines in the native response panel.     <7>│)pin",
             R"pin(               <7>│                                                                │ <10>b <7>Identical adapter response labels remain│)pin",
             R"pin(               <7>│                                                                │   separate, even when they span several   │)pin",
             R"pin(               <7>│                                                                │   lines in the native response panel.     │)pin",
             R"pin(               <7>│                                                                │ <10>c <7>Even the same next topic can have       │)pin",
             R"pin(               <7>│                                                                │   different effects.                      │)pin",
             R"pin(               <7>│                                                                │ <10>d <7>Even the same next topic can have       │)pin",
             R"pin(               <7>│                                                                │   different effects.                      │)pin",
             R"pin(               <7>│                                                                │ <10>e <7>[LIE 100%] Successful native trial, cost│)pin",
             R"pin(               <7>│                                                                │   17 cents.                               │)pin",
             R"pin(               <7>│                                                                │ <10>f <7>[LIE 0%] Denied native trial.           │)pin",
             R"pin(               <7>│                                                                │ <10>g <7>Only displayed before the conditional   │)pin",
             R"pin(               <7>│                                                                │   response is removed.                    │)pin",
             R"pin(               <7>│                                                                │ <10>h <1>Provoke the NPC, with native            <7>│)pin",
             R"pin(               <7>│                                                                │   <1>confirmation.                           <7>│)pin",
             R"pin(               <7>│                                                                │ <10>i <7>Goodbye.                                │)pin",
             R"pin(               <7>│                                                                │                                           │)pin",
             R"pin(               <7>│                                                                │                                           │)pin",
             R"pin(               <7>│                                                                │                                           │)pin",
             R"pin(               <7>│                                                                │                                           │)pin",
             R"pin(               <7>│                                                                │                                           │)pin",
             R"pin(               <7>│                                                                │                                           │)pin",
             R"pin(               <7>│                                                                │                                           │)pin",
             R"pin(               <7>│                                                                │                                           │)pin",
             R"pin(               <7>│                                                                │                                           │)pin",
             R"pin(               <7>│                                                                │                                           │)pin",
             R"pin(               <7>│                                                                │                                           │)pin",
             R"pin(               <7>│                                                                │                                           │)pin",
             R"pin(               <7>│<10>Adapter talker<7>: "Adapter narrative, already evaluated: bloody." │                                           │)pin",
             R"pin(               <7>└────────────────────────────────────────────────────────────────┴───────────────────────────────< Page 1/1 >┘)pin",
         }},
    };
    return screens;
}

auto matches(const std::string& name, const std::vector<std::string>& screen) -> bool {
    const auto found = recorded().find(name);
    return found != recorded().end() && screen == found->second;
}

} // namespace

TEST_CASE("the native NPC talk window keeps its text and colors", "[npctalk_native_pin]") {
    auto fixture = dialogue_fixture{};
    rng_set_engine_seed(74261);
    auto step = 0;
    const auto names = std::vector<std::string>{"start", "down", "down2", "page"};
    const auto keys = std::vector<int>{KEY_DOWN, KEY_DOWN, KEY_NPAGE, 'b'};
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        REQUIRE(step < 4);
        CHECK(matches("talk:" + names[step], rendered()));
        return key(keys[step++]);
    });
    CHECK(fixture.opt().id == second_topic);
    CHECK(step == 4);
}

TEST_CASE(
    "the native NPC talk window keeps its history after an exchange", "[npctalk_native_pin]") {
    auto fixture = dialogue_fixture{};
    rng_set_engine_seed(74261);
    auto step = 0;
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        REQUIRE(step < 2);
        if (step == 1) { CHECK(matches("talk:history", rendered())); }
        step += 1;
        return key('b');
    });
    fixture.talker->talk_to_u(false, false);
    CHECK(step == 2);
}

TEST_CASE("the native NPC talk window keeps its trial chances and colors", "[npctalk_native_pin]") {
    auto fixture = dialogue_fixture{};
    rng_set_engine_seed(74261);
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        CHECK(matches("talk:colors", rendered()));
        return key('a');
    });
    CHECK(fixture.opt("TALK_TEST_NPCTALK_PIN").id == "TALK_DONE");
}

TEST_CASE("the native NPC talk window keeps its text with no response", "[npctalk_native_pin]") {
    auto fixture = dialogue_fixture{};
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        CHECK(matches("talk:empty", rendered()));
        return key('L');
    });
    CHECK(fixture.opt("TALK_TEST_CLIENT_DIALOGUE_EMPTY").id == "TALK_LOOK_AT");
}

TEST_CASE(
    "the native NPC talk window keeps its layout in a wider terminal", "[npctalk_native_pin]") {
    auto fixture = dialogue_fixture{};
    TERMX = 140;
    TERMY = 50;
    game_client::memory::resize(TERMX, TERMY);
    catacurses::stdscr = catacurses::newwin(TERMY, TERMX, point_zero);
    catacurses::newscr = catacurses::newwin(TERMY, TERMX, point_zero);
    rng_set_engine_seed(74261);
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        CHECK(matches("talk:wide", rendered()));
        return key('a');
    });
    CHECK(fixture.opt().id == first_topic);
}

#endif // CATA_MCP
