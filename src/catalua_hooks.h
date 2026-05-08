#pragma once

#include <functional>
#include <string_view>

#include "catalua_sol_fwd.h"

namespace cata
{

struct lua_state;

struct hook_opts {
    bool exit_early = false;
    lua_state *state = nullptr;
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
/// Returns `params.results`.
auto run_hooks(
    std::string_view hook_name,
    std::function < auto( sol::table &params ) -> void > init = nullptr,
const hook_opts &opts = {}
) -> sol::table;

/// Define all hooks that are used in the game.
void define_hooks( lua_state &state );

} // namespace cata
