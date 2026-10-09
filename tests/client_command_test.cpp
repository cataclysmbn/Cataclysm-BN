#include "catch/catch.hpp"
#include "client_command.h"
#include "client_input.h"
#include "input.h"

#include <algorithm>

TEST_CASE("client commands resolve against the active context", "[input][client]") {
    auto context = input_context("DEFAULTMODE");
    context.register_action("UP");
    const auto scope = game_client::input_context_scope(context, "DEFAULTMODE");
    const auto actions = game_client::available_input_actions();
    REQUIRE(actions.size() == 1);
    CHECK(actions.front().id == "UP");
    REQUIRE_FALSE(actions.front().bindings.empty());

    const auto resolved = game_client::resolve_input_command({.action = "UP"}, point(80, 24));
    REQUIRE(resolved.has_value());
    CHECK(std::ranges::any_of(actions.front().bindings, [&](const auto& binding) {
        return binding.sequence == resolved->sequence && binding.type == resolved->type;
    }));
    CHECK_FALSE(game_client::resolve_input_command({.action = "DOWN"}, point(80, 24)));
}

TEST_CASE("client commands reject stale observations before resolving input", "[input][client]") {
    const auto current = game_client::current_input_id();
    const auto accepted =
        game_client::resolve_input_command({.key = "ENTER", .input_id = current}, point(80, 24));
    REQUIRE(accepted.has_value());
    CHECK(accepted->get_first_input() == inp_mngr.get_keycode("RETURN"));

    const auto stale = game_client::
        resolve_input_command({.key = "ENTER", .input_id = current + 1}, point(80, 24));
    REQUIRE_FALSE(stale.has_value());
    CHECK(stale.error().find("Input boundary changed") != std::string::npos);
}

TEST_CASE("client commands preserve text and distinguish activity polling", "[input][client]") {
    const auto text = game_client::resolve_input_command({.text = "한글"}, point(80, 24));
    REQUIRE(text.has_value());
    CHECK(text->type == input_event_t::keyboard);
    CHECK(text->text == "한글");

    const auto timeout = game_client::resolve_input_command({.key = "TIMEOUT"}, point(80, 24));
    REQUIRE(timeout.has_value());
    CHECK(timeout->type == input_event_t::timeout);
    CHECK_FALSE(
        game_client::resolve_input_command({.action = "UP", .text = "text"}, point(80, 24)));
    CHECK_FALSE(
        game_client::resolve_input_command({.modifiers = {"CTRL"}, .text = "text"}, point(80, 24)));
}

TEST_CASE("client idle commands require the current nonblocking boundary", "[input][client]") {
    auto context = input_context("DEFAULTMODE");
    context.register_action("ANY_INPUT");
    const auto current = game_client::current_input_id();

    SECTION("current nonblocking boundary") {
        const auto scope = game_client::input_context_scope(context, "DEFAULTMODE", 0);
        const auto idle =
            game_client::resolve_input_command({.key = "IDLE", .input_id = current}, point(80, 24));
        REQUIRE(idle.has_value());
        CHECK(idle->type == input_event_t::error);

        const auto stale = game_client::
            resolve_input_command({.key = "IDLE", .input_id = current + 1}, point(80, 24));
        REQUIRE_FALSE(stale.has_value());
        CHECK(stale.error().find("Input boundary changed") != std::string::npos);
    }
    SECTION("blocking boundary") {
        const auto scope = game_client::input_context_scope(context, "DEFAULTMODE", -1);
        const auto idle = game_client::resolve_input_command({.key = "IDLE"}, point(80, 24));
        REQUIRE_FALSE(idle.has_value());
        CHECK(idle.error().find("nonblocking input boundary") != std::string::npos);
    }
    SECTION("timed boundary") {
        const auto scope = game_client::input_context_scope(context, "DEFAULTMODE", 25);
        CHECK_FALSE(game_client::resolve_input_command({.key = "IDLE"}, point(80, 24)));
    }
}

TEST_CASE("client mouse commands enforce the selected surface bounds", "[input][client]") {
    const auto click = game_client::resolve_input_command(
        {.mouse_position = point(79, 23), .mouse_button = "left"}, point(80, 24));
    REQUIRE(click.has_value());
    CHECK(click->type == input_event_t::mouse);
    CHECK(click->mouse_pos == point(79, 23));
    CHECK_FALSE(game_client::resolve_input_command(
        {.mouse_position = point(80, 23), .mouse_button = "left"}, point(80, 24)));
    CHECK_FALSE(game_client::resolve_input_command(
        {.mouse_position = point(-1, 0), .mouse_button = "left"}, point(80, 24)));
    CHECK_FALSE(game_client::resolve_input_command(
        {.key = "ENTER", .mouse_position = point(1, 1), .mouse_button = "left"}, point(80, 24)));
}
