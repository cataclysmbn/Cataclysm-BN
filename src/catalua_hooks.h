#pragma once

#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "catalua_sol.h"

namespace cata
{

struct lua_state;

/// Result from a single hook callback invocation
struct hook_result {
    sol::object value;           // The returned value (may be nil)
    std::string mod_id;          // Which mod returned this value
    bool has_value = false;      // True if the hook returned a non-nil value
};

/// Options for running hooks
struct hook_opts {
    /// Optional: specific lua_state to use. If nullptr, uses global state.
    lua_state *state = nullptr;

    /// If true, stop iterating after first veto or first on_result returning true.
    /// If false, iterate all hooks regardless of results.
    /// Default: true (stop on first veto/stopping result)
    bool stop_on_result = true;

    /// If true, pass current accumulated result to subsequent hooks via params.
    /// Enables chaining where later hooks can see/modify earlier hook results.
    /// Sets params["current_return"] and params["previous_returning_mod_id"].
    /// Default: false
    bool chain_results = false;
};

/// Callback to process each hook's result.
/// @param result - The hook's return value and metadata
/// @param params - The params table (mutable, can inject current_return etc.)
/// @return true to stop iteration, false to continue
using hook_result_handler = std::function<bool( const hook_result &result, sol::table &params )>;

namespace lua_hooks_detail
{

/// Low-level hook runner with full control over result handling.
/// @param hook_name - Name of the hook to run
/// @param opts - Hook options
/// @param init - Optional callback to initialize params table
/// @param on_result - Optional callback for each hook result
/// @return true if iteration was stopped early, false if all hooks ran
auto run_hooks( std::string_view hook_name, const hook_opts &opts,
                const std::function<void( sol::table &params )> &init,
                const hook_result_handler &on_result ) -> bool;

} // namespace lua_hooks_detail

/// Run Lua hooks registered with given name.
/// Default behavior: veto semantics where returning `false` blocks the action.
///
/// Hook return values are interpreted as "veto" results:
/// - If any hook returns a boolean false, the result will be `false`.
/// - If no hook returns a boolean false, the result will be `std::nullopt`.
///
/// @param hook_name - Name of the hook
/// @param init - Optional callback to initialize params
/// @param opts - Hook options (default: stop on veto)
/// @return std::nullopt if no veto, false if vetoed
auto run_hooks( std::string_view hook_name,
                std::function<void( sol::table &params )> init = nullptr,
const hook_opts &opts = {} ) -> std::optional<bool>;

/// Run hooks and collect all results without stopping.
/// Useful for "poll all mods" type hooks.
/// @param hook_name - Name of the hook
/// @param init - Optional callback to initialize params
/// @param opts - Hook options (stop_on_result is forced false)
/// @return Vector of all hook results with mod identification
auto run_hooks_collect( std::string_view hook_name,
                        std::function<void( sol::table &params )> init = nullptr,
hook_opts opts = {} ) -> std::vector<hook_result>;

/// Define all hooks that are used in the game.
void define_hooks( lua_state &state );

} // namespace cata
