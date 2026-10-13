#pragma once

#if defined(CATA_MCP)

#    include "catch/catch.hpp"
#    include "client_interaction.h"
#    include "engine_client_component.h"
#    include "engine_client_contract.h"
#    include "input.h"

#    include <algorithm>
#    include <cstdint>
#    include <expected>
#    include <optional>
#    include <string>
#    include <variant>
#    include <vector>

/// Reading the NPC trade window as a protocol client does: from the stack of screens, inside the
/// input provider that stands for the client.
namespace trade_screen {

namespace ec = engine_client;

/// A boundary of a running game; the trade's own scope is the top of its stack.
inline auto live_state() -> ec::boundary_state {
    return {
        .id = "boundary:trade",
        .epoch = "epoch:trade",
        .ready =
            {.phase = "waiting_for_input",
             .game_ready = true,
             .accepts_interaction_commands = true,
             .accepts_registered_actions = true},
        .actions = {{.id = "UP", .name = "Move up", .keys = {"k"}}},
    };
}

/// A boundary captured as the server does, which reads the popup over the window.
inline auto captured() -> ec::boundary_state {
    auto state = ec::capture_boundary({.epoch = "epoch:trade", .ready = live_state().ready});
    REQUIRE(state.has_value());
    return std::move(*state);
}

/// The trade window while it is the screen input goes to; none under a popup of its own.
inline auto top() -> std::optional<ec::screen_view> {
    auto screen = ec::top_screen(live_state());
    return screen.context == "NPC_TRADE" ? std::optional{std::move(screen)} : std::nullopt;
}

inline auto part(const ec::screen_view& screen, const std::string& id) -> const ec::component& {
    REQUIRE(screen.root);
    const auto* const found = ec::find_component(*screen.root, id);
    INFO(id);
    REQUIRE(found != nullptr);
    return *found;
}

template <typename Body> auto body(const ec::component& value) -> const Body& {
    REQUIRE(std::holds_alternative<Body>(value.body));
    return std::get<Body>(value.body);
}

inline auto rows(const ec::screen_view& screen, const std::string& list_id)
    -> const std::vector<ec::row>& {
    const auto& found = part(screen, list_id);
    REQUIRE(std::holds_alternative<ec::list_body>(found.body));
    return std::get<ec::list_body>(found.body).rows;
}

/// The row of the item whose type is `type`; null when the list shows none.
inline auto item_row(
    const ec::screen_view& screen, const std::string& list_id, const std::string& type)
    -> const ec::row* {
    const auto& all = rows(screen, list_id);
    const auto found = std::ranges::find_if(all, [&](const ec::row& entry) {
        return !entry.header && entry.look && entry.look->id == type;
    });
    return found == all.end() ? nullptr : &*found;
}

inline auto affordance(const ec::screen_view& screen, const std::string& id)
    -> const ec::affordance& {
    const auto found = std::ranges::find(screen.affordances, id, &ec::affordance::id);
    INFO(id);
    REQUIRE(found != screen.affordances.end());
    return *found;
}

/// The input a client's invoke of an affordance of the screen becomes.
inline auto invoke(const std::string& affordance_id, game_client::interaction_arguments args = {})
    -> input_event {
    auto event = input_event{};
    event.type = input_event_t::interaction;
    event.interaction = game_client::interaction_event{
        .operation = game_client::interaction_operation::invoke,
        .schema_id = "NPC_TRADE",
        .target_id = affordance_id,
        .arguments = std::move(args)};
    return event;
}

inline auto set_count(const std::string& row, const std::int64_t count) -> input_event {
    return invoke("trade:set_count", {.row = row, .count = count});
}

inline auto filter(const std::string& pane, const std::string& text) -> input_event {
    return invoke("trade:filter", {.text = text, .pane = pane});
}

/// The message of the popup on top.
inline auto popup_message() -> std::string {
    const auto screen = ec::top_screen(captured());
    return body<ec::text_body>(part(screen, "text:message")).markup;
}

/// The input that answers the popup on top by choosing its row with `description` (the first when
/// empty), as a client does: an invoke of the list's choose affordance, resolved to its native
/// event.
inline auto answer(const std::string& description = {}) -> input_event {
    const auto state = captured();
    const auto screen = ec::top_screen(state);
    const auto& all = rows(screen, "list:choices");
    const auto found =
        description.empty()
            ? all.begin()
            : std::ranges::find(all, description, &ec::row::description);
    REQUIRE(found != all.end());
    const auto operation = ec::resolve_invoke(
        state, {.affordance_id = "bn:choose:list:choices", .args = {.row = found->id}});
    REQUIRE(operation.has_value());
    const auto* const semantic = std::get_if<ec::semantic_operation>(&*operation);
    REQUIRE(semantic != nullptr);
    auto command = semantic->command;
    command.input_id = game_client::current_input_id();
    const auto resolved = game_client::resolve_interaction_command(command);
    REQUIRE(resolved.has_value());
    auto event = input_event{};
    event.type = input_event_t::interaction;
    event.interaction = *resolved;
    return event;
}

/// What the invoke of `affordance_id` with `args` resolves to at the open trade window.
inline auto resolve(const std::string& affordance_id, ec::invoke_args args = {})
    -> std::expected<ec::command_operation, ec::error> {
    return ec::
        resolve_invoke(live_state(), {.affordance_id = affordance_id, .args = std::move(args)});
}

} // namespace trade_screen

#endif
