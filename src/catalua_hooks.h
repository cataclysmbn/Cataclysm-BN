#pragma once

#include <functional>
#include <optional>
#include <string_view>

#include "catalua_sol_fwd.h"

namespace cata
{

struct lua_state;

struct hook_opts {
    lua_state *state = nullptr;
};

namespace lua_hooks_detail
{
auto run_hooks( std::string_view hook_name, const hook_opts &opts,
                const std::function < auto( sol::table &params ) -> void > &init,
                const std::function < auto( const sol::object &res ) -> bool > &on_result ) -> bool;
} // namespace lua_hooks_detail

/// Run Lua hooks registered with given name.
/// Register hooks with an empty table in `init_global_state_tables` first.
///
/// Hook return values are interpreted as "veto" results:
/// - If any hook returns a boolean false, the result will be `false`.
/// - If no hook returns a boolean false, the result will be `std::nullopt`.
auto run_hooks( std::string_view hook_name,
                std::function < auto( sol::table &params ) -> void > init = nullptr,
hook_opts opts = {} ) -> std::optional<bool>;

/// Define all hooks that are used in the game.
void define_hooks( lua_state &state );

} // namespace cata
