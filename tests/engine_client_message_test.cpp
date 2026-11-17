#if defined(CATA_MCP)

#    include "calendar.h"
#    include "catch/catch.hpp"
#    include "engine_client_event.h"
#    include "engine_client_session.h"
#    include "messages.h"
#    include "state_helpers.h"

#    include <nlohmann/json.hpp>
#    include <string>

namespace {
/// The log lines `bn.subscribe` carries.
auto published_lines() -> nlohmann::json {
    auto session = engine_client::session{};
    REQUIRE(session.publish_boundary());
    const auto current = session.current();
    REQUIRE(current);
    const auto wire = engine_client::serialize_snapshot(*current);
    REQUIRE(wire);
    return nlohmann::json::parse(wire->header)["messages"];
}
} // namespace

TEST_CASE(
    "a message line carries the game turn it was logged or last repeated on",
    "[engine_client_message][messages]") {
    clear_all_state();
    Messages::clear_messages();
    const auto start = calendar::turn_zero + 1_hours;
    calendar::turn = start;
    Messages::add_msg("You open the door.");
    calendar::turn = start + 5_turns;
    Messages::add_msg("Bang.");
    const auto turn_of = [](const time_point& at) { return std::to_string(to_turn<int>(at)); };

    SECTION("each line names its own turn") {
        const auto lines = published_lines();
        REQUIRE(lines.size() == 2);
        CHECK(lines[0].value("turn", std::string{}) == turn_of(start));
        CHECK(lines[1].value("turn", std::string{}) == turn_of(start + 5_turns));
    }
    SECTION("a repeat moves its line to the turn of the repeat") {
        calendar::turn = start + 7_turns;
        Messages::add_msg("Bang.");
        const auto lines = published_lines();
        REQUIRE(lines.size() == 2);
        CHECK(lines[1]["count"] == 2);
        CHECK(lines[1].value("turn", std::string{}) == turn_of(start + 7_turns));
    }
}

#endif
