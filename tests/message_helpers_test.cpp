#include "catch/catch.hpp"
#include "message_helpers.h"
#include "messages.h"

#include <stdexcept>

TEST_CASE("Message capture is scoped and leaves ordinary tests unchanged", "[messages]") {
    add_msg("discarded");
    CHECK(Messages::size() == 0);
    const auto messages = capture_messages_during([]() {
        add_msg("plain");
        add_msg(m_warning, "warning");
        Messages::add_msg("direct");
        Messages::add_msg(m_bad, "direct warning");
    });
    CHECK(messages == std::vector<std::string>{"plain", "warning", "direct", "direct warning"});
    CHECK(capture_messages_during([]() {}).empty());
}

TEST_CASE("Nested message captures restore the enclosing capture", "[messages]") {
    const auto outer = capture_messages_during([]() {
        add_msg("before");
        const auto inner = capture_messages_during([]() { add_msg("inner"); });
        CHECK(inner == std::vector<std::string>{"inner"});
        add_msg("after");
    });
    CHECK(outer == std::vector<std::string>{"before", "after"});
}

TEST_CASE("Message captures restore state when a callback throws", "[messages]") {
    CHECK_THROWS_AS(
        capture_messages_during([]() {
            add_msg("discarded on throw");
            throw std::runtime_error("capture test");
        }),
        std::runtime_error);
    CHECK(capture_messages_during([]() { add_msg("next"); }) == std::vector<std::string>{"next"});
}
