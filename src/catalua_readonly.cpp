#include "catalua_readonly.h"

#include "catalua_sol.h"

static int deny_table_readonly( sol::this_state L )
{
    return luaL_error( L.lua_state(), "This table is read-only." );
}

// HACK that reimplements pairs() to work on the readonly table.
// TODO: check how broken this HACK is
static std::tuple<sol::object, sol::object, sol::nil_t>
reimpl_default_pairs( sol::this_state L, sol::table t )
{
    sol::state_view lua( L.lua_state() );
    // Forward to the table we're reading from
    return std::make_tuple( lua["next"], t[sol::meta_function::index], sol::nil );
}

sol::table cata::make_readonly_table( sol::state_view &lua, sol::table read_from )
{
    sol::table ret = lua.create_table();

    read_from[sol::meta_function::index] = read_from;
    read_from[sol::meta_function::new_index] = deny_table_readonly;
    read_from[sol::meta_function::pairs] = reimpl_default_pairs;

    ret[sol::metatable_key] = read_from;
    return ret;
}

sol::table cata::make_readonly_table( sol::state_view &lua, sol::table read_from,
                                      const std::string &error_msg )
{
    sol::table ret = lua.create_table();

    const std::string &copy = error_msg;
    read_from[sol::meta_function::index] = read_from;
    read_from[sol::meta_function::new_index] = [copy]( sol::this_state L ) {
        return luaL_error( L.lua_state(), copy.c_str() );
    };
    read_from[sol::meta_function::pairs] = reimpl_default_pairs;

    ret[sol::metatable_key] = read_from;
    return ret;
}
