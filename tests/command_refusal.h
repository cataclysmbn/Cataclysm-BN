#pragma once

#include "catch/catch.hpp"
#include "engine_client_contract.h"

#include <string>

/// Why the command lifecycle turns away an invoke of `affordance` with `args` at `state`, as the
/// client reads it; empty when it takes the invoke.
inline auto refusal(
    const engine_client::boundary_state& state, const std::string& affordance,
    const engine_client::invoke_args& args) -> std::string {
    namespace ec = engine_client;
    class authority final: public ec::command_authority {
    public:
        auto current_permissions() const -> ec::command_permissions override {
            return {.accepts_interaction_commands = true, .accepts_registered_actions = true};
        }
    };
    const auto owner = authority{};
    auto lifecycle = ec::command_lifecycle{state.epoch, owner};
    const auto submitted = lifecycle.submit(
        {.epoch = state.epoch,
         .expect = {.revision = 1, .boundary_id = state.id},
         .operation = ec::invoke_operation{.affordance_id = affordance, .args = args}});
    REQUIRE(submitted);
    if (lifecycle.validate({.epoch = state.epoch, .sequence = 1, .revision = 1}, state, {80, 24})) {
        return {};
    }
    const auto result = lifecycle.result(submitted->command_id);
    REQUIRE(result);
    REQUIRE(result->failure == ec::error::invalid_arguments);
    return result->message;
}
