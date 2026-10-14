#if defined(CATA_MCP)

#    include "avatar.h"
#    include "catch/catch.hpp"
#    include "client_screen_support.h"
#    include "computer.h"
#    include "computer_session.h"
#    include "engine_client_component.h"
#    include "game.h"
#    include "locale_fixture.h"
#    include "locale_scope.h"
#    include "map/map.h"
#    include "map_helpers.h"
#    include "options_helpers.h"
#    include "player_helpers.h"
#    include "popup.h"
#    include "state_helpers.h"

#    include <algorithm>
#    include <string>
#    include <variant>

namespace {

using namespace client_screen_test;
namespace ec = engine_client;

auto text_of(const ec::screen_view& top) -> const ec::text_body& {
    REQUIRE(top.root);
    REQUIRE(std::holds_alternative<ec::text_body>(top.root->body));
    return std::get<ec::text_body>(top.root->body);
}

auto has_text(const ec::screen_view& top, const std::string& text) -> bool {
    return text_of(top).markup.find(text) != std::string::npos;
}

auto find(const ec::screen_view& top, const std::string& id) -> const ec::affordance* {
    const auto found = std::ranges::find(top.affordances, id, &ec::affordance::id);
    return found == top.affordances.end() ? nullptr : &*found;
}

} // namespace

TEST_CASE(
    "a computer terminal publishes its text and an any-key action at an any-key prompt",
    "[client][interaction][mcp][screens][terminal]") {
    clear_all_state();
    clear_map();
    const auto guard = screen_guard{};
    auto terminal = computer("Test Terminal", 0);
    terminal.add_option("Emergency", COMPACT_EMERG_MESS, 0);
    auto stage = 0;
    open_stack([&] { computer_session(terminal).use(); },
               [&](const auto& snapshot, const auto& top) {
                   switch (stage++) {
                       case 0: {
                           CHECK(top.title == "Test Terminal");
                           CHECK(text_of(top).role == "terminal_output");
                           CHECK(has_text(top, "Logging into Test Terminal"));
                           CHECK(has_text(top, "Login successful"));
                           const auto* const any = find(top, "ANY_INPUT");
                           REQUIRE(any != nullptr);
                           CHECK(any->role == "any_key");
                           CHECK(any->label == "Press any key");
                           CHECK(ec::scope_of(*any) == ec::expect_scope::screen);
                           return run_action("ANY_INPUT");
                       }
                       case 1: {
                           // Native clears the login lines before the root menu, so none is a
                           // message.
                           CHECK(snapshot.message.find("Logging into") == std::string::npos);
                           REQUIRE(row(snapshot, "entry:0") != snapshot.choices.end());
                           return choose(snapshot, "entry:0");
                       }
                       case 2: {
                           CHECK(top.title == "Test Terminal");
                           CHECK(has_text(top, "GREETINGS CITIZEN"));
                           CHECK(has_text(top, "Press any key to continue"));
                           CHECK(find(top, "ANY_INPUT") != nullptr);
                           return run_action("ANY_INPUT");
                       }
                       case 3:
                           return cancel(snapshot);
                       default:
                           FAIL("unexpected extra screen " << top.context);
                   }
                   return input_event{};
               });
    CHECK(stage == 4);
}

TEST_CASE(
    "a computer terminal offers yes no and quit for a security query and any key after a no",
    "[client][interaction][mcp][screens][terminal]") {
    clear_all_state();
    clear_map();
    const auto guard = screen_guard{};
    auto terminal = computer("Locked Terminal", 5);
    terminal.add_option("Emergency", COMPACT_EMERG_MESS, 0);
    auto stage = 0;
    open_stack([&] { computer_session(terminal).use(); },
               [&](const auto& /*snapshot*/, const auto& top) {
                   switch (stage++) {
                       case 0: {
                           CHECK(top.title == "Locked Terminal");
                           CHECK(has_text(top, "Logging into Locked Terminal"));
                           CHECK(has_text(top, "Bypass security?"));
                           REQUIRE(find(top, "YES") != nullptr);
                           REQUIRE(find(top, "NO") != nullptr);
                           REQUIRE(find(top, "QUIT") != nullptr);
                           CHECK(find(top, "QUIT")->role == "cancel");
                           // No key stands in for an answer: the unnamed any-key action is no
                           // button.
                           CHECK(find(top, "ANY_INPUT") == nullptr);
                           return run_action("NO");
                       }
                       case 1: {
                           CHECK(has_text(top, "Shutting down"));
                           CHECK(find(top, "ANY_INPUT") != nullptr);
                           return run_action("ANY_INPUT");
                       }
                       default:
                           FAIL("unexpected extra screen " << top.context);
                   }
                   return input_event{};
               });
    CHECK(stage == 2);
}

TEST_CASE(
    "a quit answer ends the terminal session", "[client][interaction][mcp][screens][terminal]") {
    clear_all_state();
    clear_map();
    const auto guard = screen_guard{};
    auto terminal = computer("Locked Terminal", 5);
    auto stage = 0;
    open_stack([&] { computer_session(terminal).use(); },
               [&](const auto& /*snapshot*/, const auto& top) {
                   if (stage++ > 0) { FAIL("unexpected extra screen " << top.context); }
                   return run_action("QUIT");
               });
    CHECK(stage == 1);
}

TEST_CASE(
    "a yes answer run by id is taken although capitals are required",
    "[client][interaction][mcp][screens][terminal]") {
    clear_all_state();
    clear_map();
    const auto guard = screen_guard{};
    const auto capitals = override_option{"FORCE_CAPITAL_YN", "true"};
    auto terminal = computer("Locked Terminal", 5);
    terminal.add_option("Emergency", COMPACT_EMERG_MESS, 0);
    auto stage = 0;
    open_stack([&] { computer_session(terminal).use(); },
               [&](const auto& snapshot, const auto& top) {
                   if (stage++ == 0) {
                       CHECK(has_text(top, "(Case sensitive)"));
                       return run_action("YES");
                   }
                   // The query was answered yes: the hack attempt goes on, to an any-key prompt of
                   // the terminal or to its root menu, and no longer asks.
                   if (find(top, "ANY_INPUT") != nullptr) { return run_action("ANY_INPUT"); }
                   return snapshot.structured ? cancel(snapshot) : run_action("QUIT");
               });
    CHECK(stage >= 2);
}

TEST_CASE(
    "an any-key action the screen did not name is no button",
    "[client][interaction][mcp][screens][terminal][engine_client_component]") {
    clear_all_state();
    clear_map();
    const auto guard = screen_guard{};
    auto stage = 0;
    open_stack([&] { query_popup().message("%s", "Wait").allow_anykey(true).query(); },
               [&](const auto& /*snapshot*/, const auto& top) {
                   ++stage;
                   CHECK(find(top, "ANY_INPUT") == nullptr);
                   const auto* const any = find(top, "bn:any_key");
                   REQUIRE(any != nullptr);
                   CHECK(any->role == "any_key");
                   return invoke_event(top, "bn:any_key", {});
               });
    CHECK(stage == 1);
}

TEST_CASE(
    "the terminal is the same at any viewport size and in another language",
    "[client][interaction][mcp][screens][terminal][engine_client_component]") {
    clear_all_state();
    clear_map();
    const auto guard = screen_guard{};
    const auto fixture = locale_fixture::korean_catalogue{};
    auto terminal = computer("Locked Terminal", 5);
    auto stage = 0;
    open_stack([&] { computer_session(terminal).use(); },
               [&](const auto& /*snapshot*/, const auto& /*top*/) {
                   if (stage++ == 0) {
                       const auto [narrow, wide] = at_both_sizes();
                       CHECK(narrow.dump() == wide.dump());
                       const auto english = top_json();
                       const auto korean = [&] {
                           const auto scope = localization::locale_scope(locale_fixture::korean);
                           return top_json();
                       }();
                       CHECK(without_words(english) == without_words(korean));
                   }
                   return run_action("QUIT");
               });
    CHECK(stage == 1);
}

TEST_CASE(
    "the extended description names its target and shows the chosen one",
    "[client][interaction][mcp][screens][terminal]") {
    clear_all_state();
    clear_map();
    auto& you = get_avatar();
    clear_character(you, false);
    const auto guard = screen_guard{};
    auto stage = 0;
    auto first_title = std::string{};
    open_stack(
        [&] { g->extended_description(you.bub_pos() + tripoint_east); },
        [&](const auto& /*snapshot*/, const auto& top) {
            REQUIRE(top.root);
            REQUIRE(std::holds_alternative<ec::panel_body>(top.root->body));
            const auto& panel = std::get<ec::panel_body>(top.root->body);
            const auto& text = top.root->children.front();
            REQUIRE(std::holds_alternative<ec::text_body>(text.body));
            const auto& markup = std::get<ec::text_body>(text.body).markup;
            switch (stage++) {
                case 0: {
                    CHECK(top.title == "Description");
                    CHECK(markup.find("This is a") != std::string::npos);
                    for (const auto* const id : {"CREATURE", "FURNITURE", "TERRAIN", "QUIT"}) {
                        CHECK(find(top, id) != nullptr);
                    }
                    CHECK(find(top, "QUIT")->role == "cancel");
                    first_title = panel.title;
                    return run_action("FURNITURE");
                }
                case 1: {
                    CHECK(panel.title != first_title);
                    CHECK(markup.find("You do not see any furniture here.") != std::string::npos);
                    return run_action("QUIT");
                }
                default:
                    FAIL("unexpected extra screen " << top.context);
            }
            return input_event{};
        });
    CHECK(stage == 2);
}

TEST_CASE(
    "the extended description is the same at any viewport size and in another language",
    "[client][interaction][mcp][screens][terminal][engine_client_component]") {
    clear_all_state();
    clear_map();
    auto& you = get_avatar();
    clear_character(you, false);
    const auto guard = screen_guard{};
    const auto fixture = locale_fixture::korean_catalogue{};
    auto stage = 0;
    open_stack([&] { g->extended_description(you.bub_pos() + tripoint_east); },
               [&](const auto& /*snapshot*/, const auto& /*top*/) {
                   if (stage++ == 0) {
                       const auto [narrow, wide] = at_both_sizes();
                       CHECK(narrow.dump() == wide.dump());
                       const auto english = top_json();
                       const auto korean = [&] {
                           const auto scope = localization::locale_scope(locale_fixture::korean);
                           return top_json();
                       }();
                       CHECK(without_words(english) == without_words(korean));
                   }
                   return run_action("QUIT");
               });
    CHECK(stage == 1);
}

#endif // CATA_MCP
