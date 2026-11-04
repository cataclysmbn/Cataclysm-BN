#if defined(CATA_MCP) && defined(CATA_MESSAGES_NATIVE_TEST)

#    include "avatar.h"
#    include "catch/catch.hpp"
#    include "client_screen_support.h"
#    include "client_snapshot.h"
#    include "color.h"
#    include "locale_fixture.h"
#    include "locale_scope.h"
#    include "map_helpers.h"
#    include "messages.h"
#    include "player_helpers.h"
#    include "state_helpers.h"

#    include <algorithm>
#    include <format>
#    include <string>
#    include <variant>
#    include <vector>

using namespace client_screen_test;

namespace {

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

/// The recorded screen.
auto recorded() -> const std::vector<std::string>& {
    static const auto screen = std::vector<std::string>{
        R"pin(          <7>┌──────────────────────────────────────────────────────────────────────────────┐)pin",
        R"pin(          <7>│          <8>┌ <15>You hear a whack! x 2                                             <7>│)pin",
        R"pin(          <7>│          <8>│ <10>Your wound closes.                                                <7>│)pin",
        R"pin(          <7>│          <8>│ <9>You are hit!                                                      <7>│)pin",
        R"pin(          <7>│  <12>0 seconds <11>The floor creaks.                                                 <7>│)pin",
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
        R"pin(          <7>└< f, F or / to filter, r or R to reset, w/h to adjust size, c or C to copy, x o)pin",
    };
    return screen;
}

} // namespace

TEST_CASE("the native message log keeps its text and colors", "[messages_native_pin]") {
    clear_all_state();
    clear_map();
    clear_character(get_avatar(), false);
    Messages::clear_messages();
    add_msg("You hear a whack!");
    add_msg("You hear a whack!");
    add_msg(m_good, "Your wound closes.");
    add_msg(m_bad, "You are hit!");
    add_msg(m_warning, "The floor creaks.");
    const auto guard = screen_guard{};
    auto step = 0;
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        REQUIRE(step++ == 0);
        CHECK(rendered() == recorded());
        return event_of({.action = "QUIT"});
    });
    Messages::display_messages();
    CHECK(step == 1);
}


namespace {

namespace ec = engine_client;

auto log_of(const ec::screen_view& top) -> const ec::list_body& {
    REQUIRE(top.root);
    const auto* const log = ec::find_component(*top.root, "messages:log");
    REQUIRE(log != nullptr);
    REQUIRE(std::holds_alternative<ec::list_body>(log->body));
    return std::get<ec::list_body>(log->body);
}

auto filter_of(const ec::screen_view& top) -> const std::string& {
    REQUIRE(top.root);
    REQUIRE(std::holds_alternative<ec::panel_body>(top.root->body));
    return std::get<ec::panel_body>(top.root->body).filter;
}

auto labels_of(const ec::screen_view& top) -> std::vector<std::string> {
    auto labels = std::vector<std::string>{};
    for (const auto& row : log_of(top).rows) { labels.push_back(row.label); }
    return labels;
}

auto column_of(const ec::row& row, const std::string& key) -> std::string {
    const auto found = std::ranges::find(row.columns, key, &game_client::interaction_column::key);
    REQUIRE(found != row.columns.end());
    return found->value;
}

/// The log of the avatar after `texts`, each at the type paired with it.
auto fill_log(const std::vector<std::pair<game_message_type, std::string>>& texts) -> void {
    clear_all_state();
    clear_map();
    clear_character(get_avatar(), false);
    Messages::clear_messages();
    for (const auto& [type, text] : texts) { add_msg(type, "%s", text); }
}

/// The rows the log shows once `typed` is submitted to its filter.
auto rows_after_filter(const std::string& typed) -> std::vector<std::string> {
    const auto guard = screen_guard{};
    auto shown = std::vector<std::string>{};
    auto stage = 0;
    open_stack([] { Messages::display_messages(); },
               [&](const auto& /*snapshot*/, const auto& top) {
                   switch (stage++) {
                       case 0:
                           return run_action("FILTER");
                       case 1:
                           return invoke_event(top, "bn:submit:form:field", {.text = typed});
                       case 2:
                           shown = labels_of(top);
                           return run_action("QUIT");
                       default:
                           FAIL("unexpected extra screen " << top.context);
                   }
                   return input_event{};
               });
    CHECK(stage == 3);
    return shown;
}

} // namespace

TEST_CASE(
    "the message log lists its messages by identity and filters them with the native filter",
    "[client][interaction][mcp][screens][messages_screen]") {
    fill_log({{m_neutral, "a needle in a haystack"}, {m_neutral, "something else"}});
    const auto guard = screen_guard{};
    auto stage = 0;
    auto ids = std::vector<std::string>{};
    open_stack(
        [] { Messages::display_messages(); },
        [&](const auto& /*snapshot*/, const auto& top) {
            switch (stage++) {
                case 0: {
                    CHECK(top.context == "MESSAGE_LOG");
                    CHECK(top.title == "Messages");
                    const auto& log = log_of(top);
                    REQUIRE(log.rows.size() == 2);
                    CHECK(log.selection == "none");
                    for (const auto& row : log.rows) {
                        CHECK(row.id.starts_with("message:"));
                        CHECK_FALSE(row.selectable);
                        CHECK(column_of(row, "time") == "0 seconds");
                        CHECK(column_of(row, "type") == "neutral");
                        ids.push_back(row.id);
                    }
                    CHECK(ids.front() != ids.back());
                    CHECK(filter_of(top).empty());
                    const auto filter =
                        std::ranges::find(top.affordances, "FILTER", &ec::affordance::id);
                    REQUIRE(filter != top.affordances.end());
                    CHECK(filter->label == "Filter");
                    return run_action("FILTER");
                }
                case 1: {
                    CHECK(top.context != "MESSAGE_LOG");
                    return invoke_event(top, "bn:submit:form:field", {.text = "needle"});
                }
                case 2: {
                    CHECK(top.context == "MESSAGE_LOG");
                    REQUIRE(labels_of(top) == std::vector<std::string>{"a needle in a haystack"});
                    CHECK(filter_of(top) == "needle");
                    // A message keeps its id while the filter hides the others.
                    CHECK(log_of(top).rows.front().id == ids.front());
                    return run_action("RESET_FILTER");
                }
                case 3: {
                    CHECK(log_of(top).rows.size() == 2);
                    CHECK(filter_of(top).empty());
                    return run_action("QUIT");
                }
                default:
                    FAIL("unexpected extra screen " << top.context);
            }
            return input_event{};
        });
    CHECK(stage == 4);
}

TEST_CASE(
    "the message log filter takes a message type before a colon and keeps unknown prefixes as text",
    "[client][interaction][mcp][screens][messages_screen]") {
    fill_log({{m_good, "You heal"}, {m_bad, "You are hit"}, {m_bad, "The hit lands"}});
    CHECK(rows_after_filter("bad:hit") == std::vector<std::string>{"You are hit", "The hit lands"});
    CHECK(rows_after_filter("good:hit").empty());
    CHECK(rows_after_filter("bad:") == std::vector<std::string>{"You are hit", "The hit lands"});
    // `nosuch` is no message type: the text after the colon is all that is searched.
    CHECK(rows_after_filter("nosuch:you") == std::vector<std::string>{"You heal", "You are hit"});
    CHECK(rows_after_filter("YOU ARE") == std::vector<std::string>{"You are hit"});
}

TEST_CASE(
    "the message log colors a message by its type and repeats count in its label",
    "[client][interaction][mcp][screens][messages_screen]") {
    fill_log({{m_warning, "The floor creaks"}, {m_warning, "The floor creaks"}});
    const auto guard = screen_guard{};
    auto stage = 0;
    open_stack([] { Messages::display_messages(); },
               [&](const auto& /*snapshot*/, const auto& top) {
                   REQUIRE(stage++ == 0);
                   const auto& log = log_of(top);
                   REQUIRE(log.rows.size() == 1);
                   CHECK(log.rows.front().label == "The floor creaks x 2");
                   CHECK(log.rows.front().markup
                         == colorize("The floor creaks x 2", msgtype_to_color(m_warning, false)));
                   CHECK(column_of(log.rows.front(), "type") == "warning");
                   return run_action("QUIT");
               });
    CHECK(stage == 1);
}

TEST_CASE(
    "the message log's window size toggles only change the native layout",
    "[client][interaction][mcp][screens][messages_screen][engine_client_component]") {
    fill_log({{m_neutral, "You wake up"}});
    const auto guard = screen_guard{};
    auto stage = 0;
    open_stack(
        [] { Messages::display_messages(); },
        [&](const auto& /*snapshot*/, const auto& top) {
            REQUIRE(stage++ == 0);
            const auto role_of = [&](const std::string& id) {
                const auto found = std::ranges::find(top.affordances, id, &ec::affordance::id);
                INFO(id);
                REQUIRE(found != top.affordances.end());
                return found->role;
            };
            CHECK(role_of("TOGGLE_WIDE_DISPLAY") == "layout");
            CHECK(role_of("TOGGLE_FULL_HEIGHT_DISPLAY") == "layout");
            CHECK(role_of("FILTER").empty());
            return run_action("QUIT");
        });
    CHECK(stage == 1);
}

TEST_CASE(
    "the message log is the same at any viewport size and in another language",
    "[client][interaction][mcp][screens][messages_screen][engine_client_component]") {
    fill_log({{m_good, "You heal"}, {m_bad, "You are hit"}});
    const auto guard = screen_guard{};
    const auto fixture = locale_fixture::korean_catalogue{};
    auto stage = 0;
    open_stack([] { Messages::display_messages(); },
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

#endif // CATA_MCP && CATA_MESSAGES_NATIVE_TEST
