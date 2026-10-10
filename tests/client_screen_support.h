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
#    include "game_constants.h"
#    include "input.h"
#    include "output.h"

#    include <algorithm>
#    include <functional>
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

/// Answers each input read of the screen `open` shows with `script`.
inline auto open_screen(
    const std::function<auto()->void>& open,
    const std::function<auto(const snapshot_t&)->input_event>& script) -> void {
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto snapshot = game_client::current_interaction();
        REQUIRE(snapshot.structured);
        return script(snapshot);
    });
    open();
}


} // namespace client_screen_test

#endif // CATA_MCP
