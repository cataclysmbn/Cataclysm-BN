#if defined(CATA_MCP)

#    include "avatar.h"
#    include "catch/catch.hpp"
#    include "client_screen_support.h"
#    include "color.h"
#    include "engine_client_component.h"
#    include "engine_client_contract.h"
#    include "help.h"
#    include "itype.h"
#    include "locale_fixture.h"
#    include "locale_scope.h"
#    include "map_helpers.h"
#    include "morale.h"
#    include "morale_types.h"
#    include "player_helpers.h"
#    include "state_helpers.h"
#    include "type_id.h"

#    include <algorithm>
#    include <string>
#    include <variant>
#    include <vector>

namespace {

using namespace client_screen_test;
namespace ec = engine_client;

auto part(const ec::screen_view& top, const std::string& id) -> const ec::component& {
    REQUIRE(top.root);
    const auto* const found = ec::find_component(*top.root, id);
    INFO(id);
    REQUIRE(found != nullptr);
    return *found;
}

template <typename Body> auto body(const ec::component& value) -> const Body& {
    REQUIRE(std::holds_alternative<Body>(value.body));
    return std::get<Body>(value.body);
}

auto rows_of(const ec::screen_view& top, const std::string& list) -> const std::vector<ec::row>& {
    return body<ec::list_body>(part(top, list)).rows;
}

auto text_of(const ec::screen_view& top, const std::string& id) -> const std::string& {
    return body<ec::text_body>(part(top, id)).markup;
}

auto find(const ec::screen_view& top, const std::string& id) -> const ec::affordance* {
    const auto found = std::ranges::find(top.affordances, id, &ec::affordance::id);
    return found == top.affordances.end() ? nullptr : &*found;
}

/// Whether the screen on top refuses an invoke of `affordance` with `args` as `invalid_arguments`.
auto refuses(const std::string& affordance, ec::invoke_args args) -> bool {
    const auto state = ec::capture_boundary(
        {.epoch = "epoch:test",
         .ready =
             {.phase = "waiting_for_input",
              .game_ready = true,
              .accepts_interaction_commands = true,
              .accepts_registered_actions = true}});
    REQUIRE(state.has_value());
    const auto resolved = ec::resolve_invoke(*state, {.affordance_id = affordance, .args = args});
    return !resolved.has_value() && resolved.error() == ec::error::invalid_arguments;
}

} // namespace

TEST_CASE(
    "the help lists its topics by the order of the help data and opens a picked one",
    "[client][interaction][mcp][screens][info_screens][help_screen]") {
    clear_all_state();
    clear_map();
    clear_character(get_avatar(), false);
    get_help().load();
    const auto guard = screen_guard{};
    auto stage = 0;
    open_stack(
        [] { get_help().display_help(); },
        [&](const auto& /*snapshot*/, const auto& top) {
            switch (stage++) {
                case 0: {
                    CHECK(top.title == " HELP ");
                    CHECK(text_of(top, "help:intro").find("press one of the following")
                          != std::string::npos);
                    const auto& topics = rows_of(top, "help:topics");
                    REQUIRE(topics.size() > 2);
                    CHECK(topics.front().id == "topic:0");
                    CHECK(topics.front().label == "Introduction");
                    CHECK(topics.front().hotkey == "a");
                    CHECK(topics.at(1).id == "topic:1");
                    CHECK(body<ec::list_body>(part(top, "help:topics")).activate == "help:topic");
                    const auto* const open = find(top, "help:topic");
                    REQUIRE(open != nullptr);
                    REQUIRE(find(top, "CONFIRM") != nullptr);
                    CHECK(open->label == find(top, "CONFIRM")->label);
                    CHECK(refuses("help:topic", {.row = "topic:999"}));
                    return invoke_event(top, "help:topic", {.row = "topic:1"});
                }
                case 1: {
                    // The topic's text is a screen of its own, over the list.
                    CHECK(top.context == "SCROLLABLE_TEXT");
                    CHECK(top.title == " HELP ");
                    CHECK(text_of(top, "text:body").find("Movement is performed")
                          != std::string::npos);
                    CHECK(find(top, "QUIT") != nullptr);
                    return run_action("QUIT");
                }
                case 2:
                    CHECK(find(top, "help:topic") != nullptr);
                    return run_action("QUIT");
                default:
                    FAIL("unexpected extra screen " << top.context);
            }
            return input_event{};
        });
    CHECK(stage == 3);
}

TEST_CASE(
    "the help is the same at any viewport size and in another language",
    "[client][interaction][mcp][screens][info_screens][help_screen][engine_client_component]") {
    clear_all_state();
    clear_map();
    clear_character(get_avatar(), false);
    get_help().load();
    const auto guard = screen_guard{};
    const auto fixture = locale_fixture::korean_catalogue{};
    auto stage = 0;
    open_stack([] { get_help().display_help(); },
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
    "the morale screen lists the points that raise and lower morale by identity with their totals",
    "[client][interaction][mcp][screens][info_screens][morale_screen]") {
    clear_all_state();
    clear_map();
    clear_character(get_avatar(), false);
    auto morale = player_morale{};
    morale.add(MORALE_FOOD_GOOD, 20, 40, 1_hours, 30_minutes, false, *itype_id("apple"));
    morale.add(MORALE_FOOD_BAD, -10, -20, 1_hours, 30_minutes, false, *itype_id("bread"));
    const auto guard = screen_guard{};
    auto stage = 0;
    open_stack([&] { morale.display(77, 5, 3); },
               [&](const auto& /*snapshot*/, const auto& top) {
                   REQUIRE(stage++ == 0);
                   CHECK(top.title == "Morale");
                   const auto& rows = rows_of(top, "morale:points");
                   REQUIRE(rows.size() == 4);
                   CHECK(rows[0].id == "morale:positive");
                   CHECK(rows[0].header);
                   CHECK(rows[0].label == "Total positive morale");
                   CHECK(rows[0].columns.at(0).value == "+20");
                   CHECK(rows[1].id == "morale:positive:morale_food_good:apple");
                   CHECK(rows[1].label == "Enjoyed apple");
                   CHECK(rows[1].columns.at(0).value == "100%");
                   CHECK(rows[1].color == string_from_color(c_green));
                   CHECK(rows[2].id == "morale:negative");
                   CHECK(rows[2].columns.at(0).value == "-10");
                   CHECK(rows[3].id == "morale:negative:morale_food_bad:bread");
                   CHECK(rows[3].label == "Disliked bread");
                   CHECK(rows[3].color == string_from_color(c_light_red));
                   const auto& stats = body<ec::panel_body>(part(top, "morale:totals")).stats;
                   REQUIRE(stats.size() == 4);
                   CHECK(stats[0].key == "level");
                   CHECK(stats[0].label == "Total morale:");
                   CHECK(stats[0].value == "+10");
                   CHECK(stats[0].tone == "good");
                   CHECK(stats[1].key == "pain");
                   CHECK(stats[1].value == "-5");
                   CHECK(stats[1].tone == "bad");
                   CHECK(stats[2].key == "fatigue_cap");
                   CHECK(stats[2].value == "3");
                   CHECK(stats[2].tone == "bad");
                   CHECK(stats[3].key == "focus");
                   CHECK(stats[3].label == "Focus trends towards:");
                   CHECK(stats[3].value == "77");
                   CHECK(stats[3].tone.empty());
                   CHECK(find(top, "QUIT") != nullptr);
                   return run_action("QUIT");
               });
    CHECK(stage == 1);
}

TEST_CASE(
    "the morale screen says nothing affects morale and leaves out the pain and fatigue lines at zero",
    "[client][interaction][mcp][screens][info_screens][morale_screen]") {
    clear_all_state();
    clear_map();
    clear_character(get_avatar(), false);
    auto morale = player_morale{};
    const auto guard = screen_guard{};
    auto stage = 0;
    open_stack([&] { morale.display(100, 0, 0); },
               [&](const auto& /*snapshot*/, const auto& top) {
                   REQUIRE(stage++ == 0);
                   CHECK(text_of(top, "morale:none") == "Nothing affects your morale");
                   CHECK_FALSE(top.root->children.empty());
                   CHECK(ec::find_component(*top.root, "morale:points") == nullptr);
                   const auto& stats = body<ec::panel_body>(part(top, "morale:totals")).stats;
                   REQUIRE(stats.size() == 2);
                   CHECK(stats[0].key == "level");
                   CHECK(stats[0].value == "-");
                   CHECK(stats[0].tone.empty());
                   CHECK(stats[1].key == "focus");
                   return run_action("QUIT");
               });
    CHECK(stage == 1);
}

TEST_CASE(
    "the morale screen is the same at any viewport size and in another language",
    "[client][interaction][mcp][screens][info_screens][morale_screen][engine_client_component]") {
    clear_all_state();
    clear_map();
    clear_character(get_avatar(), false);
    auto morale = player_morale{};
    morale.add(MORALE_FOOD_GOOD, 20, 40, 1_hours, 30_minutes, false, *itype_id("apple"));
    const auto guard = screen_guard{};
    const auto fixture = locale_fixture::korean_catalogue{};
    auto stage = 0;
    open_stack([&] { morale.display(77, 5, 3); },
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
