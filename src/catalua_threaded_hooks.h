#pragma once

#include "catalua_sol_fwd.h"
#include "catalua_threaded_hook_types.h"

#include <functional>
#include <string_view>
#include <vector>

namespace cata
{

struct lua_state;

/// Create the game.threaded_hooks table and register game.register_threaded_hook.
/// Called from init_global_state_tables alongside define_hooks().
void define_threaded_hooks( lua_state &state );

/// Serialize a hook_intent to a Lua table in the given state (for passing to post functions).
auto intent_to_table( sol::state_view lua, const hook_intent &intent ) -> sol::table;

/// Run all pre-pass functions for the given hook name in the calling thread's worker state.
/// Returns one result per registered entry; entries with run_post=false need no follow-up.
/// Safe to call from worker threads.
auto run_threaded_hook_pre(
    std::string_view hook_name,
    std::function<void( sol::table & )> init
) -> std::vector<hook_pre_result>;

/// Run post-pass functions for all results that have run_post=true.
/// Must be called on the main thread only.
/// @param global   The main-thread Lua state.
/// @param results  Pre-pass results from run_threaded_hook_pre.
/// @param init     Populates the params table passed to each post function.
void run_threaded_hook_post(
    lua_state &global,
    const std::vector<hook_pre_result> &results,
    std::function<void( sol::table & )> init
);

/// Returns true if any threaded on_mapgen_postprocess hooks are registered.
/// Thread-safe (reads a cached atomic flag).
auto has_threaded_mapgen_hooks() -> bool;

/// Update the cached flag returned by has_threaded_mapgen_hooks().
/// Must be called on the main thread after hook registration changes.
void refresh_threaded_mapgen_hook_presence();

} // namespace cata
