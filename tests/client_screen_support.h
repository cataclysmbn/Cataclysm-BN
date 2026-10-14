#pragma once

#if defined(CATA_MCP)

#    include "catch/catch.hpp"
#    include "client_command.h"
#    include "client_input.h"
#    include "client_interaction.h"
#    include "client_memory.h"
#    include "client_memory_scope.h"
#    include "cursesdef.h"
#    include "cursesport.h"
#    include "engine_client_component.h"
#    include "engine_client_contract.h"
#    include "game_constants.h"
#    include "input.h"
#    include "output.h"

#    include <algorithm>
#    include <functional>
#    include <nlohmann/json.hpp>
#    include <string>
#    include <vector>

/// Drives a native screen through the client protocol: its input reads are answered with semantic
/// commands.
namespace client_screen_test {

struct screen_guard {
    const game_client::memory::scoped_state memory;
    bool old_test_mode = test_mode;
    int old_termx = TERMX;
    int old_termy = TERMY;
    int old_full_screen_width = FULL_SCREEN_WIDTH;
    int old_full_screen_height = FULL_SCREEN_HEIGHT;

    screen_guard() {
        test_mode = false;
        TERMX = 100;
        TERMY = 40;
        FULL_SCREEN_WIDTH = 80;
        FULL_SCREEN_HEIGHT = 24;
        game_client::memory::resize(TERMX, TERMY);
        catacurses::stdscr = catacurses::newwin(TERMY, TERMX, point_zero);
        catacurses::newscr = catacurses::newwin(TERMY, TERMX, point_zero);
    }

    ~screen_guard() {
        TERMX = old_termx;
        TERMY = old_termy;
        FULL_SCREEN_WIDTH = old_full_screen_width;
        FULL_SCREEN_HEIGHT = old_full_screen_height;
        test_mode = old_test_mode;
    }
};

using snapshot_t = game_client::interaction_snapshot;

inline auto event_of(game_client::input_command input) -> input_event {
    const auto result =
        game_client::resolve_input_command(input, game_client::memory::screen_size());
    REQUIRE(result.has_value());
    return *result;
}

inline auto operate(
    const snapshot_t& snapshot, game_client::interaction_operation operation,
    std::string target = {}, std::string value = {}) -> input_event {
    const auto fills = operation == game_client::interaction_operation::fill;
    return event_of({
        .interaction =
            game_client::interaction_command{
                .input_id = snapshot.input_id,
                .operation = operation,
                .target_id = std::move(target),
                .value = std::move(value),
                .submit = fills ? std::optional<bool>{true} : std::nullopt,
            },
    });
}

inline auto choose(const snapshot_t& snapshot, const std::string& id) -> input_event {
    return operate(snapshot, game_client::interaction_operation::choose, id);
}

inline auto cancel(const snapshot_t& snapshot) -> input_event {
    return operate(snapshot, game_client::interaction_operation::cancel);
}

inline auto row(const snapshot_t& snapshot, const std::string& id) {
    return std::ranges::find(snapshot.choices, id, &game_client::interaction_choice::id);
}

inline auto rows_with(const snapshot_t& snapshot, const std::string& prefix)
    -> std::vector<std::string> {
    auto ids = std::vector<std::string>{};
    for (const auto& choice : snapshot.choices) {
        if (choice.id.starts_with(prefix)) { ids.push_back(choice.id); }
    }
    return ids;
}

/// The most input reads a screen script may be asked for; a screen that asks for more is stuck.
constexpr auto max_screen_reads = 1000;

/// Answers each input read of the screen `open` shows with `script`. A screen that keeps reading
/// input without ending (a script answering with no input at all) fails instead of spinning.
inline auto open_screen(
    const std::function<auto()->void>& open,
    const std::function<auto(const snapshot_t&)->input_event>& script) -> void {
    auto reads = 0;
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        REQUIRE(++reads <= max_screen_reads);
        const auto snapshot = game_client::current_interaction();
        REQUIRE(snapshot.structured);
        return script(snapshot);
    });
    open();
}

/// The screen on top of the 0.2 stack now, with the 0.1 snapshot of the same read.
inline auto top_screen_now() -> engine_client::screen_view {
    const auto state = engine_client::capture_boundary(
        {.epoch = "epoch:test",
         .ready = {.phase = "waiting_for_input",
                   .accepts_interaction_commands = true,
                   .accepts_registered_actions = true}});
    REQUIRE(state.has_value());
    return engine_client::top_screen(*state);
}

/// Like `open_screen`, for a screen that publishes components: `script` gets the screen on top of
/// the stack, and the 0.1 snapshot, which a screen of components has none of.
inline auto open_stack(
    const std::function<auto()->void>& open,
    const std::function<auto(const snapshot_t&, const engine_client::screen_view&)->input_event>&
        script) -> void {
    auto reads = 0;
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        REQUIRE(++reads <= max_screen_reads);
        return script(game_client::current_interaction(), top_screen_now());
    });
    open();
}

/// The screen on top as the wire carries it, less the serial that is new on every open.
inline auto top_json() -> nlohmann::json {
    const auto state = engine_client::capture_boundary(
        {.epoch = "epoch:test",
         .ready = {.phase = "waiting_for_input",
                   .accepts_interaction_commands = true,
                   .accepts_registered_actions = true}});
    REQUIRE(state.has_value());
    auto top =
        nlohmann::json::parse(engine_client::serialize_interaction_boundary(*state))["stack"]
            .back();
    top.erase("screen_id");
    return top;
}

/// `value` with every string member emptied, as a client sees the shape of a screen without its
/// words.
inline auto without_words(nlohmann::json value) -> nlohmann::json {
    if (value.is_string()) { return ""; }
    if (!value.is_structured()) { return value; }
    for (auto& member : value) { member = without_words(std::move(member)); }
    return value;
}

/// The same screen at the sizes of a small and a large terminal: equal when it reads its model
/// only.
inline auto at_both_sizes() -> std::pair<nlohmann::json, nlohmann::json> {
    const auto saved_x = TERMX;
    const auto saved_y = TERMY;
    TERMX = 80;
    TERMY = 24;
    auto narrow = top_json();
    TERMX = 200;
    TERMY = 60;
    auto wide = top_json();
    TERMX = saved_x;
    TERMY = saved_y;
    return {std::move(narrow), std::move(wide)};
}

/// The input a client sends to run `affordance` of `top` with `args`, validated as the lifecycle
/// does.
inline auto invoke_event(
    const engine_client::screen_view& top, const std::string& affordance,
    engine_client::invoke_args args = {}) -> input_event {
    namespace ec = engine_client;
    class authority final: public ec::command_authority {
    public:
        auto current_permissions() const -> ec::command_permissions override {
            return {.accepts_interaction_commands = true, .accepts_registered_actions = true};
        }
    };
    const auto state = ec::capture_boundary(
        {.epoch = "epoch:test",
         .ready = {.phase = "waiting_for_input",
                   .accepts_interaction_commands = true,
                   .accepts_registered_actions = true}});
    REQUIRE(state.has_value());
    const auto found = std::ranges::find(top.affordances, affordance, &ec::affordance::id);
    REQUIRE(found != top.affordances.end());
    const auto owner = authority{};
    auto lifecycle = ec::command_lifecycle{"epoch:test", owner};
    const auto submitted = lifecycle.submit(
        {.epoch = "epoch:test",
         .expect = ec::scope_of(*found) == ec::expect_scope::screen
                     ? ec::expectation{.screen_id = top.screen_id}
                     : ec::expectation{.revision = 1, .boundary_id = state->id},
         .operation = ec::invoke_operation{.affordance_id = affordance, .args = std::move(args)}});
    REQUIRE(submitted.has_value());
    const auto event = lifecycle.validate(
        {.epoch = "epoch:test", .sequence = 1, .revision = 1}, *state, point{80, 24});
    REQUIRE(event.has_value());
    return *event;
}

/// The named action of the context the screen reads, as a client runs it.
inline auto run_action(const std::string& id) -> input_event { return event_of({.action = id}); }

} // namespace client_screen_test

#endif // CATA_MCP
