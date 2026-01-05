#pragma once

#include <functional>
#include <string_view>

#include "catalua_sol.h"
#include "catalua.h"

namespace cata
{

/// Run Lua hooks registered with given name.
/// Register hooks with an empty table in `init_global_state_tables` first.
///
/// @param state Lua state to run hooks in. Defaults to the global state.
/// @param hooks_table Name of the hooks table to run. e.g "on_game_load".
/// @param init Function to initialize parameter table for the hook.
void run_hooks( lua_state &state, std::string_view hook_name,
                std::function < auto( sol::table &params ) -> void > init );
void run_hooks( std::string_view hook_name,
                std::function < auto( sol::table &params ) -> void > init );
void run_hooks( lua_state &state, std::string_view hook_name );
void run_hooks( std::string_view hook_name );
bool run_hooks_bool( lua_state &state, std::string_view hook_name,
                     std::function < auto( sol::table &params ) -> void > init,
                     bool default_result = true );
bool run_hooks_bool( std::string_view hook_name,
                     std::function < auto( sol::table &params ) -> void > init,
                     bool default_result = true );

/// Define all hooks that are used in the game.
void define_hooks( lua_state &state );

} // namespace cata
