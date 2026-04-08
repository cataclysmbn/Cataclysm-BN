// catalua_bindings_yarn.cpp — Lua bridge for the Yarn Spinner dialogue system.
//
// Registers:
//   lua_fn("path.to.fn", args...)  — call any Lua global by dotted path; returns yarn value
//   <<lua_cmd "path.to.fn" args>>  — same on the command side; returns command_signal
//   game.dialogue.register_condition(name, fn)
//   game.dialogue.register_command(name, fn)

#include "catalua_bindings.h"
#include "catalua_bindings_utils.h"
#include "catalua_impl.h"
#include "catalua_luna.h"
#include "catalua_luna_doc.h"
#include "catalua_sol.h"
#include "init.h"
#include "yarn_dialogue.h"

namespace
{

// ============================================================
// Lua ↔ yarn value conversion
// ============================================================

auto yarn_to_sol( sol::state_view lua, const yarn::value &v ) -> sol::object
{
    return std::visit( [&lua]<typename T>( const T &val ) -> sol::object {
        return sol::make_object( lua, val );
    }, v );
}

auto sol_to_yarn( const sol::object &obj ) -> yarn::value
{
    switch( obj.get_type() ) {
        case sol::type::boolean:
            return obj.as<bool>();
        case sol::type::number:
            return obj.as<double>();
        case sol::type::string:
            return obj.as<std::string>();
        default:
            return false;
    }
}

// ============================================================
// Dotted-path Lua global resolver  ("my_mod.check_something")
// ============================================================

auto resolve_lua_path( sol::state_view lua, const std::string &path ) -> sol::object
{
    sol::object cur = lua.globals();
    std::string_view remaining( path );

    while( !remaining.empty() ) {
        auto dot = remaining.find( '.' );
        auto key = dot == std::string_view::npos ? remaining : remaining.substr( 0, dot );
        remaining = dot == std::string_view::npos ? "" : remaining.substr( dot + 1 );

        if( cur.get_type() != sol::type::table ) {
            return sol::lua_nil;
        }
        cur = cur.as<sol::table>()[std::string( key )];
    }
    return cur;
}

// ============================================================
// Get the global Lua state
// ============================================================

auto get_sol() -> sol::state &
{
    return DynamicDataLoader::get_instance().lua->lua;
}

} // anonymous namespace

void cata::detail::reg_yarn_dialogue( sol::state &lua )
{
    using vt = yarn::value_type;

    // ============================================================
    // lua_fn("path.to.fn", args...) — call any Lua global from a yarn expression.
    //
    // The first argument is always the dotted-path string to the function.
    // Additional arguments (of any yarn type) are forwarded to Lua.
    // The return value must be bool, number, or string; anything else → false.
    //
    // Caution: Lua errors inside lua_fn are caught and logged; the expression
    // returns false on failure rather than crashing the conversation.
    // ============================================================

    yarn::func_registry::global().register_func( {
        .name        = "lua_fn",
        .param_types = { vt::string },
        .return_type = vt::boolean,   // runtime-polymorphic; actual type depends on the function
        .variadic    = true,
        .impl        = []( const std::vector<yarn::value> &args ) -> yarn::value {
            auto &sol_state  = get_sol();
            sol::state_view lua( sol_state );
            const auto &path = std::get<std::string>( args[0] );
            auto fn_obj = resolve_lua_path( lua, path );

            if( fn_obj.get_type() != sol::type::function ) {
                debugmsg( "yarn: lua_fn: '%s' is not a Lua function", path );
                return false;
            }

            sol::protected_function fn( fn_obj );
            std::vector<sol::object> lua_args;
            lua_args.reserve( args.size() - 1 );
            std::ranges::transform( args | std::views::drop( 1 ),
                                    std::back_inserter( lua_args ),
                                    [&lua]( const yarn::value &v ) {
                return yarn_to_sol( lua, v );
            } );

            auto result = fn( sol::as_args( lua_args ) );
            if( !result.valid() ) {
                sol::error err = result;
                debugmsg( "yarn: lua_fn: '%s' threw: %s", path, err.what() );
                return false;
            }
            sol::object ret_obj = result;
            return sol_to_yarn( ret_obj );
        }
    } );

    // ============================================================
    // lua_cmd "path.to.fn" args... — call any Lua global as a yarn command.
    //
    // Return value from Lua: if the function returns the string "stop" or
    // the boolean false, the conversation ends. Any other return (including
    // nil, true, or no return) continues normally.
    // ============================================================

    yarn::command_registry::global().add( "lua_cmd", 1, -1,
    []( const std::vector<yarn::value> &args ) -> yarn::command_signal {
        auto &sol_state  = get_sol();
        sol::state_view lua( sol_state );
        const auto &path = std::get<std::string>( args[0] );
        auto fn_obj = resolve_lua_path( lua, path );

        if( fn_obj.get_type() != sol::type::function ) {
            debugmsg( "yarn: lua_cmd: '%s' is not a Lua function", path );
            return yarn::command_signal::none;
        }

        sol::protected_function fn( fn_obj );
        std::vector<sol::object> lua_args;
        lua_args.reserve( args.size() - 1 );
        std::ranges::transform( args | std::views::drop( 1 ),
                                std::back_inserter( lua_args ),
                                [&lua]( const yarn::value &v ) {
            return yarn_to_sol( lua, v );
        } );

        auto result = fn( sol::as_args( lua_args ) );
        if( !result.valid() ) {
            sol::error err = result;
            debugmsg( "yarn: lua_cmd: '%s' threw: %s", path, err.what() );
            return yarn::command_signal::none;
        }

        sol::object ret_obj = result;
        if( ret_obj.get_type() == sol::type::string && ret_obj.as<std::string>() == "stop" ) {
            return yarn::command_signal::stop;
        }
        if( ret_obj.get_type() == sol::type::boolean && !ret_obj.as<bool>() ) {
            return yarn::command_signal::stop;
        }
        return yarn::command_signal::none;
    } );

    // ============================================================
    // game.dialogue table
    // ============================================================

    sol::object game_obj = lua.globals()["game"];
    sol::table game_table = game_obj.get_type() == sol::type::table
                            ? game_obj.as<sol::table>()
                            : lua.create_table();
    if( game_obj.get_type() != sol::type::table ) {
        lua.globals()["game"] = game_table;
    }
    sol::table dialogue_table = lua.create_table();

    // game.dialogue.register_condition(name, fn)
    //   fn(arg1, arg2, ...) → bool (truthy/falsy)
    //   Registers a named condition callable from <<if name(args)>> in .yarn files.
    dialogue_table.set_function( "register_condition",
    [&lua]( const std::string &name, sol::protected_function fn ) {
        yarn::func_registry::global().register_func( {
            .name        = name,
            .param_types = {},
            .return_type = vt::boolean,
            .variadic    = true,
            .impl        = [fn_cap = fn, &lua, name]( const std::vector<yarn::value> &args ) -> yarn::value {
                sol::state_view sv( lua );
                std::vector<sol::object> lua_args;
                lua_args.reserve( args.size() );
                std::ranges::transform( args, std::back_inserter( lua_args ),
                                        [&sv]( const yarn::value &v ) {
                    return yarn_to_sol( sv, v );
                } );
                auto result = fn_cap( sol::as_args( lua_args ) );
                if( !result.valid() ) {
                    sol::error err = result;
                    debugmsg( "yarn: register_condition '%s' threw: %s", name, err.what() );
                    return false;
                }
                sol::object ret = result;
                if( ret.get_type() == sol::type::boolean ) {
                    return ret.as<bool>();
                }
                return ret.get_type() != sol::type::nil;
            }
        } );
    } );

    // game.dialogue.register_command(name, fn)
    //   fn(arg1, arg2, ...) — return "stop" or false to end conversation; otherwise continues.
    //   Registers a named command callable from <<name args>> in .yarn files.
    dialogue_table.set_function( "register_command",
    [&lua]( const std::string &name, sol::protected_function fn ) {
        yarn::command_registry::global().add( name, 0, -1,
        [fn_cap = fn, &lua, name]( const std::vector<yarn::value> &args ) -> yarn::command_signal {
            sol::state_view sv( lua );
            std::vector<sol::object> lua_args;
            lua_args.reserve( args.size() );
            std::ranges::transform( args, std::back_inserter( lua_args ),
                                    [&sv]( const yarn::value &v ) {
                return yarn_to_sol( sv, v );
            } );
            auto result = fn_cap( sol::as_args( lua_args ) );
            if( !result.valid() ) {
                sol::error err = result;
                debugmsg( "yarn: register_command '%s' threw: %s", name, err.what() );
                return yarn::command_signal::none;
            }
            sol::object ret = result;
            if( ret.get_type() == sol::type::string && ret.as<std::string>() == "stop" ) {
                return yarn::command_signal::stop;
            }
            if( ret.get_type() == sol::type::boolean && !ret.as<bool>() ) {
                return yarn::command_signal::stop;
            }
            return yarn::command_signal::none;
        } );
    } );

    game_table["dialogue"] = dialogue_table;
}
