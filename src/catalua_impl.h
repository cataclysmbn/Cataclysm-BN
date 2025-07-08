#pragma once

#include "calendar.h"
#include "catalua_sol.h"

namespace cata
{
struct on_every_x_hooks {
    time_duration interval;
    std::vector<sol::protected_function> functions;
};

/**
 * Lua state handle.
 * Definition is hidden from outside code to prevent sol::state
 * usage and visibility outside catalua files.
 */
struct lua_state {
    sol::state lua;

    lua_state() = default;
    ~lua_state() = default;
};

} // namespace cata

sol::state make_lua_state();
void run_lua_script( sol::state &lua, const std::string &script_name );
void run_console_input( sol::state &lua, const std::string &chunk );
void check_func_result( sol::protected_function_result &res );

// Numbers in Lua can be either integers or floating-point,
// but you can't determine that with simple get_type()
bool is_number_integer( sol::state_view lua, const sol::object &val );

// Returns type name as registered with luna.
// If type was not registered with luna, or the value is not a userdata, returns nullopt.
std::optional<std::string> get_luna_type( const sol::object &val );

/**
 * Compare 2 Lua values for equality (by value).
 *
 * May contain unhandled cases, use with care.
 */
bool compare_values( const sol::object &a, const sol::object &b );

/**
 * Compare 2 Lua tables for equality (by value).
 *
 * May contain unhandled cases, use with care.
 */
bool compare_tables( sol::table a, sol::table b );

/**
 * @brief Find key of equivalent value in provided table.
 *
 * Since userdata and tables are compared by their reference when used as keys,
 * if we want to find value with a different userdata instance
 * we first need to find a key that is equal *by value* to the one we want to use.
 *
 * @param t table to search in for the key
 * @param desired_key found key will be equal to this value
 * @returns sol::nil on failure, key object from table \p t on success
 */
sol::object find_equivalent_key( const sol::table &t, sol::object desired_key );


