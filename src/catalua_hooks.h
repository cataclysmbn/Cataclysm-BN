#pragma once

#include <functional>
#include <string>
#include <string_view>
#include <vector>

#include "catalua_sol.h"

namespace cata
{

struct lua_state;

using hook_init_fn = std::function < auto( sol::table &params ) -> void >;

struct hook_opts {
    bool exit_early = false;
    lua_state *state = nullptr;
};

/// One Lua callback return value from a hook run.
struct hook_return {
    std::string mod_id;
    int priority = 0;
    sol::object value;
};

/// C++ result of running every callback for one hook.
struct hook_run_result {
    bool allowed = true;
    sol::table results;
    std::vector<hook_return> returns;
};

/// Run Lua hooks registered with given name.
/// Register hooks with an empty table in `init_global_state_tables` first.
///
/// Hooks are registered in Lua via `table.insert( game.hooks.<hook_name>, ... )`.
/// Each hook entry can be either:
/// - legacy function: `function( params ) ... end`
/// - table: `{ mod_id = "...", priority = 10, fn = function( params ) ... end }`
///
/// During execution, `params.results` is a table shared by all hooks, and `params.prev`
/// contains the previous hook's return value.
/// Returns a C++ struct with the shared `params.results`, aggregate `allowed` flag, and
/// each callback return value kept separate.
auto run_hooks( std::string_view hook_name, hook_init_fn init = nullptr, const hook_opts &opts = {} )
-> hook_run_result;

/// Return whether a hook currently has registered entries without building params/results tables.
auto has_hooks( std::string_view hook_name, const hook_opts &opts = {} ) -> bool;

/// Define all hooks that are used in the game.
void define_hooks( lua_state &state );

} // namespace cata
