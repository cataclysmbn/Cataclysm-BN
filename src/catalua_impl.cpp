#include "catalua_impl.h"

#include "catalua_bindings.h"
#include "catalua_loader.h"
#include "catalua_log.h"
#include "catalua_sol.h"
#include "debug.h"
#include "string_formatter.h"

#include <cmath>
#include <limits>
#include <optional>
#include <stdexcept>
#include <utility>

sol::state make_lua_state()
{
    sol::state lua;

    lua.open_libraries(
        sol::lib::base,
        sol::lib::package,
        sol::lib::math,
        sol::lib::string,
        sol::lib::table
    );

    // Register custom module loader for relative/absolute imports
    cata::lua_loader::register_searcher( lua.lua_state() );

    cata::reg_all_bindings( lua );

    return lua;
}

void run_lua_script( sol::state &lua, const std::string &script_name )
{
    // RAII guard ensures stack cleanup even if exception occurs
    const auto guard = cata::lua_loader::script_context_guard{ script_name };

    sol::load_result load_res = lua.load_file( script_name );

    if( !load_res.valid() ) {
        sol::error err = load_res;
        throw std::runtime_error(
            string_format( "Failed to load script %s: %s", script_name, err.what() )
        );
    }

    sol::protected_function exec = load_res;

    // Sandboxing: create environment that uses globals table as fallback
    // to prevent script from accidentally (or intentionally) modifying globals table.
    // All modifications will be stored in an environment which then gets discarded.
    sol::environment my_env( lua, sol::create, lua.globals() );
    sol::set_environment( my_env, exec );

    sol::protected_function_result exec_res = exec();

    if( !exec_res.valid() ) {
        sol::error err = exec_res;
        throw std::runtime_error(
            string_format( "Script runtime error in %s: %s", script_name, err.what() )
        );
    }
}

void run_console_input( sol::state &lua, const std::string &chunk )
{
    DebugLog( DL::Info, DC::Lua ) << "CONSOLE: " << chunk;
    cata::get_lua_log_instance().add(
        cata::LuaLogLevel::Input,
        std::string( chunk )
    );

    sol::load_result load_res = lua.load( chunk, "console input" );

    if( !load_res.valid() ) {
        // No need to use obnoxious debugmsgs, user will see the error in log
        sol::error err = load_res;
        cata::get_lua_log_instance().add(
            cata::LuaLogLevel::Error,
            string_format( "Failed to load: %s", err.what() )
        );
        return;
    }

    sol::protected_function exec = load_res;

    // No sandboxing here, we trust console user :)

    sol::protected_function_result exec_res = exec();

    if( !exec_res.valid() ) {
        // No need to use obnoxious debugmsgs, user will see the error in log
        sol::error err = exec_res;
        cata::get_lua_log_instance().add(
            cata::LuaLogLevel::Error,
            string_format( "Runtime error: %s", err.what() )
        );
        return;
    }

    try {
        sol::object retval = exec_res;
        if( retval == sol::nil ) {
            // No return - don't spam
            return;
        }
        std::string val = lua["tostring"]( retval );
        cata::get_lua_log_instance().add(
            cata::LuaLogLevel::Info,
            string_format( "# %s", val )
        );
    } catch( std::runtime_error &e ) {
        cata::get_lua_log_instance().add(
            cata::LuaLogLevel::Info,
            string_format( "# ??? %s", e.what() )
        );
    }
}

void check_func_result( sol::protected_function_result &res )
{
    if( !res.valid() ) {
        sol::error err = res;
        throw std::runtime_error(
            string_format( "Script runtime error: %s", err.what() )
        );
    }
}

bool is_number_integer( sol::state_view lua, const sol::object &val )
{
    if( val.get_type() != sol::type::number ) {
        throw std::runtime_error( "is_number_integer: called on a non-number type" );
    }
    // HACK: get_type() does not report precise number type, so we have to improvise
    sol::protected_function math_type = lua.script( "return math.type" );
    if( !math_type ) {
        throw std::runtime_error( "is_number_integer: failed to obtain math.type" );
    }
    sol::protected_function_result res = math_type( val );
    if( res.status() != sol::call_status::ok ) {
        sol::error err = res;
        std::string msg = string_format( "is_number_integer runtime error: %s", err.what() );
        throw std::runtime_error( msg );
    }
    std::string mtype = res;
    if( mtype == "integer" ) {
        return true;
    } else if( mtype == "float" ) {
        return false;
    } else {
        std::string msg = string_format( "is_number_integer runtime error: unexpected return value %s",
                                         mtype );
        throw std::runtime_error( msg );
    }
}

std::optional<std::string> get_luna_type( const sol::object &val )
{
    sol::state_view lua( val.lua_state() );
    if( val.get_type() == sol::type::userdata ) {
        sol::protected_function glt = lua.script( "return function(a) return a:get_luna_type() end" );
        if( glt ) {
            sol::protected_function_result res = glt( val );
            if( res.status() == sol::call_status::ok ) {
                sol::object retval = res;
                return retval.as<std::string>();
            }
        } else {
            throw std::runtime_error( "get_luna_type: failed to obtain lua type getter function" );
        }
    }
    return std::nullopt;
}

bool compare_values( const sol::object &a, const sol::object &b )
{
    sol::state_view lua( a.lua_state() );
    sol::type type = a.get_type();
    if( type != b.get_type() ) {
        return false;
    }
    if( type == sol::type::table ) {
        return compare_tables( a, b );
    }
    if( type == sol::type::function ) {
        throw std::runtime_error( "Can't compare functions" );
    }
    if( type == sol::type::thread ) {
        throw std::runtime_error( "Can't compare threads" );
    }
    if( type == sol::type::number ) {
        if( is_number_integer( lua, a ) != is_number_integer( lua, b ) ) {
            return false;
        }
        if( is_number_integer( lua, a ) ) {
            int num_a = a.as<int>();
            int num_b = b.as<int>();
            return num_a == num_b;
        } else {
            double num_a = a.as<double>();
            double num_b = b.as<double>();
            // FIXME: not suitable for all cases
            return std::fabs( num_a - num_b ) < 0.0001;
        }
        return false;
    }
    if( type == sol::type::userdata ) {
        std::optional<std::string> luna_a = get_luna_type( a );
        std::optional<std::string> luna_b = get_luna_type( b );
        if( luna_a != luna_b ) {
            return false;
        }
    }
    // HACK: We depend on presumption that type implements eq operator.
    //       If not, Lua WILL compare by reference, and not by value.
    sol::protected_function cmp_func = lua.script( "return function(a, b) return a == b end" );
    sol::protected_function_result res = cmp_func( a, b );
    if( res.status() != sol::call_status::ok ) {
        sol::error err = res;
        throw std::runtime_error( string_format( "compare_values - lua error: %s", err.what() ) );
    }
    sol::object retval = res;
    return retval.as<bool>();
}

sol::object find_equivalent_key( const sol::table &t, sol::object desired_key )
{
    sol::object ret = sol::nil;
    t.for_each( [&]( const sol::object & key, const sol::object & ) {
        if( ret != sol::nil ) {
            return;
        }
        if( compare_values( desired_key, key ) ) {
            ret = key;
        }
    } );
    return ret;
}

bool compare_tables( sol::table a, sol::table b )
{
    if( a.size() != b.size() ) {
        return false;
    }

    bool are_equal = true;
    a.for_each( [&]( sol::object a_key, const sol::object & a_val ) {
        if( !are_equal ) {
            // Short-circuit
            return;
        }
        sol::object b_key = find_equivalent_key( b, std::move( a_key ) );
        if( b_key == sol::nil ) {
            are_equal = false;
            return;
        } else {
            sol::object b_val = b[b_key];
            if( b_val == sol::nil ) {
                are_equal = false;
            } else if( !compare_values( a_val, b_val ) ) {
                are_equal = false;
            }
        }
    } );
    if( are_equal ) {
        b.for_each( [&]( sol::object b_key, const sol::object & /*b_val*/ ) {
            sol::object a_key = find_equivalent_key( a, std::move( b_key ) );
            if( a_key == sol::nil ) {
                are_equal = false;
            } else {
                sol::object a_val = a[a_key];
                if( a_val == sol::nil ) {
                    are_equal = false;
                }
            }
        } );
    }
    return are_equal;
}
