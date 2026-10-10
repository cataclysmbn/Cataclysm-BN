#if defined(CATA_MCP) && defined(CATA_MESSAGES_NATIVE_TEST)

#    include "avatar.h"
#    include "catch/catch.hpp"
#    include "client_screen_support.h"
#    include "map_helpers.h"
#    include "messages.h"
#    include "player_helpers.h"
#    include "state_helpers.h"

using namespace client_screen_test;

TEST_CASE(
    "the message log lists the messages and filters them through the native filter",
    "[client][interaction][mcp][screens]") {
    clear_all_state();
    clear_map();
    auto& you = get_avatar();
    clear_character(you, false);
    Messages::clear_messages();
    add_msg("a needle in a haystack");
    add_msg("something else");
    const auto guard = screen_guard{};
    auto stage = 0;
    open_screen(
        [] { Messages::display_messages(); },
        [&](const auto& snapshot) {
            switch (stage++) {
                case 0: {
                    CHECK(snapshot.context == "MESSAGE_LOG");
                    const auto messages = rows_with(snapshot, "message:");
                    CHECK(messages.size() == 2);
                    const auto first = row(snapshot, messages.front());
                    CHECK((first->label == "something else"
                           || first->label == "a needle in a haystack"));
                    CHECK_FALSE(first->selectable);
                    CHECK(first->columns.size() == 2);
                    CHECK(row(snapshot, "action:FILTER") != snapshot.choices.end());
                    return choose(snapshot, "action:FILTER");
                }
                case 1: {
                    REQUIRE(snapshot.field);
                    return operate(snapshot, game_client::interaction_operation::fill,
                                   snapshot.field->id, "needle");
                }
                case 2: {
                    CHECK(snapshot.context == "MESSAGE_LOG");
                    const auto messages = rows_with(snapshot, "message:");
                    REQUIRE(messages.size() == 1);
                    CHECK(row(snapshot, messages.front())->label == "a needle in a haystack");
                    CHECK(snapshot.message == "Filter: needle");
                    return choose(snapshot, "action:RESET_FILTER");
                }
                case 3: {
                    CHECK(rows_with(snapshot, "message:").size() == 2);
                    CHECK(snapshot.allow_cancel);
                    return cancel(snapshot);
                }
                default:
                    FAIL("unexpected extra screen " << snapshot.context);
            }
            return input_event{};
        });
    CHECK(stage == 4);
}

#endif // CATA_MCP && CATA_MESSAGES_NATIVE_TEST
