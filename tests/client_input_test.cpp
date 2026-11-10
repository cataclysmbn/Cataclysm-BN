#include "catch/catch.hpp"
#include "client_input.h"
#include "input.h"

#include <stdexcept>

TEST_CASE("clients observe the innermost input context", "[input][client]") {
    const auto original = game_client::active_input_context();
    const auto game_context = input_context("DEFAULTMODE");
    const auto menu_context = input_context("INVENTORY");

    {
        const auto game_scope = game_client::input_context_scope(game_context, "DEFAULTMODE");
        CHECK(game_client::active_input_context().context == &game_context);
        CHECK(game_client::active_input_context().category == "DEFAULTMODE");

        {
            const auto menu_scope = game_client::input_context_scope(menu_context, "INVENTORY");
            CHECK(game_client::active_input_context().context == &menu_context);
            CHECK(game_client::active_input_context().category == "INVENTORY");
        }

        CHECK(game_client::active_input_context().context == &game_context);
        CHECK(game_client::active_input_context().category == "DEFAULTMODE");
    }

    CHECK(game_client::active_input_context().context == original.context);
    CHECK(game_client::active_input_context().category == original.category);
}

TEST_CASE("failed input requests restore the enclosing context", "[input][client]") {
    const auto outer_context = input_context("DEFAULTMODE");
    const auto inner_context = input_context("HELP_KEYBINDINGS");
    const auto outer_scope = game_client::input_context_scope(outer_context, "DEFAULTMODE");

    REQUIRE_THROWS_AS(
        [&]() {
            const auto inner_scope =
                game_client::input_context_scope(inner_context, "HELP_KEYBINDINGS");
            throw std::runtime_error("input unavailable");
        }(),
        std::runtime_error);

    CHECK(game_client::active_input_context().context == &outer_context);
    CHECK(game_client::active_input_context().category == "DEFAULTMODE");
}
