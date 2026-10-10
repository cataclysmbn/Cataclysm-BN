#include "catch/catch.hpp"
#include "engine_client_event.h"
#include "engine_client_session.h"
#include "messages.h"

#include <algorithm>
#include <nlohmann/json.hpp>
#include <ranges>
#include <string>
#include <vector>

namespace {
namespace ec = engine_client;

auto texts(const ec::snapshot& snapshot) -> std::vector<std::string> {
    return snapshot.log | std::views::transform(&ec::message_value::text)
         | std::ranges::to<std::vector>();
}

/// The events of one kind a session has queued for its subscribers since the last call.
auto pushed_events(ec::session& session) -> std::vector<ec::public_event> {
    auto result = std::vector<ec::public_event>{};
    for (const auto& item : session.take_push()) {
        if (const auto* event = std::get_if<ec::event_push>(&item)) {
            result.push_back(event->event);
        }
    }
    return result;
}
} // namespace

TEST_CASE(
    "a stream created after the game wrote its log carries the whole log",
    "[engine_client_session]") {
    Messages::clear_messages();
    Messages::add_msg("You wake up.");
    Messages::add_msg("Your head hurts.");
    auto session = ec::session{};
    REQUIRE(session.publish_boundary());
    const auto current = session.current();
    REQUIRE(current);
    CHECK(texts(*current) == std::vector<std::string>{"You wake up.", "Your head hurts."});
    const auto wire = ec::serialize_snapshot(*current);
    REQUIRE(wire);
    const auto header = nlohmann::json::parse(wire->header);
    REQUIRE(header.contains("messages"));
    CHECK(header["messages"].size() == 2);
    CHECK(header["messages"][0]["text"] == "You wake up.");
}

TEST_CASE(
    "a loaded world's saved log reaches the stream its epoch restart creates",
    "[engine_client_session]") {
    Messages::clear_messages();
    Messages::add_msg("First game.");
    auto session = ec::session{};
    REQUIRE(session.publish_boundary());
    REQUIRE(texts(*session.current()) == std::vector<std::string>{"First game."});
    // Loading replaces the world: the log is replaced too, and the stream starts over.
    session.replace_world();
    Messages::clear_messages();
    Messages::add_msg("Saved line one.");
    Messages::add_msg("Saved line two.");
    REQUIRE_FALSE(session.current());
    REQUIRE(session.publish_boundary());
    CHECK(texts(*session.current())
          == std::vector<std::string>{"Saved line one.", "Saved line two."});
}

TEST_CASE(
    "a receiver that follows the stream's message events holds the log a fresh snapshot carries",
    "[engine_client_session]") {
    Messages::clear_messages();
    Messages::add_msg("Before.");
    auto session = ec::session{};
    REQUIRE(session.publish_boundary());
    auto receiver = *session.current();
    session.take_push();
    Messages::add_msg("After.");
    Messages::add_msg("After.");
    Messages::add_msg("Later.");
    REQUIRE(session.publish_boundary());
    REQUIRE(
        ec::apply_batch(receiver, {.epoch = session.epoch(), .events = pushed_events(session)}));
    const auto fresh = *session.current();
    CHECK(receiver.at == fresh.at);
    CHECK(texts(receiver) == texts(fresh));
    CHECK(texts(fresh) == std::vector<std::string>{"Before.", "After.", "Later."});
    CHECK(fresh.log[1].count == 2);
    CHECK(receiver.log[1].count == 2);
}

TEST_CASE(
    "lines the game wrote before the stream existed take no sequence", "[engine_client_session]") {
    Messages::clear_messages();
    Messages::add_msg("One.");
    Messages::add_msg("Two.");
    auto session = ec::session{};
    REQUIRE(session.publish_boundary());
    CHECK(session.current()->at.sequence == 0);
    CHECK(session.current()->log.size() == 2);
    CHECK(session.take_push().empty());
}
