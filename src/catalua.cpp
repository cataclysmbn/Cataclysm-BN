#include "catalua.h"

#include "debug.h"

#include <algorithm>
#include <clocale>
#include <optional>

constexpr int LUA_API_VERSION = 2;

#include "catalua_sol.h"

#include "avatar.h"
#include "catalua_console.h"
#include "catalua_hooks.h"
#include "catalua_impl.h"
#include "catalua_iuse_actor.h"
#include "catalua_readonly.h"
#include "catalua_serde.h"
#include "filesystem.h"
#include "fstream_utils.h"
#include "init.h"
#include "item_factory.h"
#include "map.h"
#include "messages.h"
#include "mod_manager.h"
#include "path_info.h"
#include "point.h"
#include "worldfactory.h"

namespace cata
{

std::string get_lapi_version_string()
{
    return string_format( "%d", get_lua_api_version() );
}

void startup_lua_test()
{
    sol::state lua = make_lua_state();
    std::string lua_startup_script = PATH_INFO::datadir() + "raw/on_game_start.lua";
    try {
        run_lua_script( lua, lua_startup_script );
    } catch( std::runtime_error &e ) {
        debugmsg( "%s", e.what() );
    }
}

auto generate_lua_docs( const std::filesystem::path &script_path,
                        const std::filesystem::path &to ) -> bool
{
    // Force C locale for consistent string sorting in Lua (strcoll dependency)
    const auto *prev_locale_ptr = std::setlocale( LC_ALL, nullptr );
    const auto prev_locale = std::string{ prev_locale_ptr ? prev_locale_ptr : "" };
    std::setlocale( LC_ALL, "C" );

    sol::state lua = make_lua_state();
    lua.globals()["doc_gen_func"] = lua.create_table();
    lua.globals()["print"] = [&]( const sol::variadic_args & va ) {
        for( auto it : va ) {
            std::string str = lua["tostring"]( it );
            std::cout << str;
        }
        std::cout << std::endl;
    };
    lua.globals()["package"]["path"] = string_format(
                                           "%1$s/?.lua;%1$s/?/init.lua;%2$s/?.lua;%2$s/?/init.lua",
                                           PATH_INFO::datadir() + "/lua", PATH_INFO::datadir() + "/raw"
                                       );

    try {
        run_lua_script( lua, script_path.string() );
        sol::protected_function doc_gen_func = lua["doc_gen_func"]["impl"];
        sol::protected_function_result res = doc_gen_func();
        check_func_result( res );
        std::string ret = res;
        write_to_file( to.string(), [&]( std::ostream & s ) -> void {
            s << ret;
        } );
    } catch( std::runtime_error &e ) {
        cata_printf( "%s\n", e.what() );
        std::setlocale( LC_ALL, prev_locale.c_str() );
        return false;
    }
    std::setlocale( LC_ALL, prev_locale.c_str() );
    return true;
}

void show_lua_console()
{
    cata::show_lua_console_impl();
}

void reload_lua_code()
{
    cata::lua_state &state = *DynamicDataLoader::get_instance().lua;
    const auto &packs = world_generator->active_world->info->active_mod_order;
    try {
        const int lua_mods = init::load_main_lua_scripts( state, packs );
        add_msg( m_good, _( "Reloaded %1$d lua mods." ), lua_mods );
    } catch( std::runtime_error &e ) {
        debugmsg( "%s", e.what() );
    }
    clear_mod_being_loaded( state );
}

void debug_write_lua_backtrace( std::ostream &out )
{
    cata::lua_state *state = DynamicDataLoader::get_instance().lua.get();
    if( !state ) {
        return;
    }
    sol::state container;

    luaL_traceback( container.lua_state(), state->lua.lua_state(), "=== Lua backtrace report ===", 0 );

    std::string data = sol::stack::pop<std::string>( container );
    out << data << '\n';
}

static sol::table get_mod_storage_table( lua_state &state )
{
    return state.lua.globals()["game"]["cata_internal"]["mod_storage"];
}

bool save_world_lua_state( const world *world, const std::string &path )
{
    lua_state &state = *DynamicDataLoader::get_instance().lua;

    const mod_management::t_mod_list &mods = world_generator->active_world->info->active_mod_order;
    sol::table t = get_mod_storage_table( state );
    run_on_game_save_hooks( state );
    bool ret = world->write_to_file( path, [&]( std::ostream & stream ) {
        JsonOut jsout( stream );
        jsout.start_object();
        for( const mod_id &mod : mods ) {
            if( !mod.is_valid() ) {
                // The mod is missing from installation
                continue;
            }
            jsout.member( mod.str() );
            serialize_lua_table( t[mod.str()], jsout );
        }
        jsout.end_object();
    }, "world_lua_state" );
    return ret;
}

bool load_world_lua_state( const world *world, const std::string &path )
{
    lua_state &state = *DynamicDataLoader::get_instance().lua;
    const mod_management::t_mod_list &mods = world_generator->active_world->info->active_mod_order;
    sol::table t = get_mod_storage_table( state );

    bool ret = world->read_from_file( path, [&]( std::istream & stream ) {
        JsonIn jsin( stream );
        JsonObject jsobj = jsin.get_object();

        for( const mod_id &mod : mods ) {
            if( !jsobj.has_object( mod.str() ) ) {
                // Mod could have been added to existing save
                continue;
            }
            if( !mod.is_valid() ) {
                // Trying to load without the mod
                continue;
            }
            JsonObject mod_obj = jsobj.get_object( mod.str() );
            deserialize_lua_table( t[mod.str()], mod_obj );
        }
    }, true );

    run_on_game_load_hooks( state );
    return ret;
}

std::unique_ptr<lua_state, lua_state_deleter> make_wrapped_state()
{
    auto state = new lua_state{};
    state->lua = make_lua_state();
    std::unique_ptr<lua_state, lua_state_deleter> ret(
        state,
        lua_state_deleter{}
    );

    sol::state &lua = ret->lua;

    sol::table game_table = lua.create_table();
    lua.globals()["game"] = game_table;

    return ret;
}

void init_global_state_tables( lua_state &state, const std::vector<mod_id> &modlist )
{
    sol::state &lua = state.lua;

    sol::table active_mods = lua.create_table();
    sol::table mod_runtime = lua.create_table();
    sol::table mod_storage = lua.create_table();
    sol::table hooks = lua.create_table();

    for( size_t i = 0; i < modlist.size(); i++ ) {
        active_mods[ i + 1 ] = modlist[i].str();
        mod_runtime[ modlist[i].str() ] = lua.create_table();
        mod_storage[ modlist[i].str() ] = lua.create_table();
    }

    // Main game data table
    sol::table gt = lua.globals()["game"];

    // Internal table that bypasses read-only facades
    sol::table it = lua.create_table();
    gt["cata_internal"] = it;
    it["active_mods"] = active_mods;
    it["mod_runtime"] = mod_runtime;
    it["mod_storage"] = mod_storage;
    it["on_every_x_hooks"] = std::vector<cata::on_every_x_hooks>();
    gt["hooks"] = hooks;

    // Runtime infrastructure
    gt["active_mods"] = make_readonly_table( lua, active_mods );
    gt["mod_runtime"] = make_readonly_table( lua, mod_runtime );
    gt["mod_storage"] = make_readonly_table( lua, mod_storage );
    gt["hooks"] = make_readonly_table( lua, hooks );

    // iuse functions
    gt["iuse_functions"] = lua.create_table();

    // hooks
    cata::define_hooks( state );
}

void set_mod_being_loaded( lua_state &state, const mod_id &mod )
{
    sol::state &lua = state.lua;
    lua.globals()["game"]["current_mod"] = mod.str();
    lua.globals()["game"]["current_mod_path"] = mod->path + "/";
    lua.globals()["package"]["path"] =
        string_format(
            "%1$s/?.lua;%1$s/?/init.lua;%2$s/?.lua;%2$s/?/init.lua",
            PATH_INFO::datadir() + "/lua", mod->path
        );
}

void clear_mod_being_loaded( lua_state &state )
{
    sol::state &lua = state.lua;
    lua.globals()["game"]["current_mod"] = sol::nil;
    lua.globals()["game"]["current_mod_path"] = sol::nil;
    lua.globals()["package"]["path"] = sol::nil;
}

void run_mod_preload_script( lua_state &state, const mod_id &mod )
{
    std::string script_path = mod->path + "/" + "preload.lua";

    if( !file_exist( script_path ) ) {
        return;
    }

    run_lua_script( state.lua, script_path );
}

void run_mod_finalize_script( lua_state &state, const mod_id &mod )
{
    std::string script_path = mod->path + "/" + "finalize.lua";

    if( !file_exist( script_path ) ) {
        return;
    }

    run_lua_script( state.lua, script_path );
}

void run_mod_main_script( lua_state &state, const mod_id &mod )
{
    std::string script_path = mod->path + "/" + "main.lua";

    if( !file_exist( script_path ) ) {
        return;
    }

    run_lua_script( state.lua, script_path );
}

namespace lua_hooks_detail
{

auto run_hooks( std::string_view hook_name, const hook_opts &opts,
                const std::function<void( sol::table &params )> &init,
                const hook_result_handler &on_result ) -> bool
{
    lua_state &state = opts.state != nullptr ? *opts.state : *DynamicDataLoader::get_instance().lua;
    sol::state &lua = state.lua;

    auto maybe_hooks = lua.globals()["game"]["hooks"][hook_name].get<sol::optional<sol::table>>();
    if( !maybe_hooks ) {
        return false;
    }

    sol::table hooks_table = *maybe_hooks;

    // Create params table
    auto params = lua.create_table();
    if( init ) {
        init( params );
    }

    std::optional<std::string> error;
    int error_idx = -1;
    std::string error_mod_id;
    bool stopped = false;

    // Track current result for chaining
    sol::object current_return = sol::nil;
    std::string previous_returning_mod_id;

    // First pass: collect entries with their metadata for sorting
    // We store indices and metadata, but call functions in second pass
    struct hook_ref {
        int table_index;
        int priority;
        std::string mod_id;
        bool is_new_format;
    };
    std::vector<hook_ref> hook_refs;

    int table_idx = 0;
    for( auto &ref : hooks_table ) {
        ++table_idx;
        sol::object entry = ref.second;
        sol::type entry_type = entry.get_type();

        hook_ref hr;
        hr.table_index = table_idx;
        hr.priority = 0;
        hr.mod_id = "";
        hr.is_new_format = false;

        if( entry_type == sol::type::table ) {
            // New format: {func, mod_id, priority}
            sol::table t = entry.as<sol::table>();
            sol::object func_obj = t["func"];
            if( func_obj.get_type() != sol::type::function ) {
                continue;
            }
            hr.priority = t.get_or( "priority", 0 );
            hr.mod_id = t.get_or<std::string>( "mod_id", "" );
            hr.is_new_format = true;
        } else if( entry_type != sol::type::function ) {
            continue;
        }

        hook_refs.push_back( hr );
    }

    if( hook_refs.empty() ) {
        return false;
    }

    // Sort by priority (stable sort preserves registration order for same priority)
    std::stable_sort( hook_refs.begin(), hook_refs.end(),
    []( const hook_ref & a, const hook_ref & b ) {
        return a.priority < b.priority;
    } );

    // Second pass: execute hooks in priority order
    for( size_t idx = 0; idx < hook_refs.size(); ++idx ) {
        const hook_ref &hr = hook_refs[idx];

        try {
            // If chaining is enabled, inject current state into params
            if( opts.chain_results ) {
                params["current_return"] = current_return;
                params["previous_returning_mod_id"] = previous_returning_mod_id;
            }

            // Get the function from the table - use the exact pattern from old code
            sol::object entry = hooks_table[hr.table_index];
            sol::protected_function func;

            if( hr.is_new_format ) {
                sol::table t = entry.as<sol::table>();
                func = t["func"];
            } else {
                // Old format: direct function assignment like original code
                func = entry;
            }

            sol::protected_function_result res = func( params );
            check_func_result( res );

            if( res.valid() ) {
                sol::object result_value = res.get<sol::object>();

                hook_result result;
                result.value = result_value;
                result.mod_id = hr.mod_id;
                result.has_value = ( result_value.get_type() != sol::type::nil );

                // Update chain state if this hook returned a value
                if( result.has_value ) {
                    current_return = result_value;
                    previous_returning_mod_id = hr.mod_id;
                }

                // Call result handler if provided
                if( on_result ) {
                    bool should_stop = on_result( result, params );
                    if( should_stop && opts.stop_on_result ) {
                        stopped = true;
                        break;
                    }
                }
            }
        } catch( const std::runtime_error &e ) {
            error = e.what();
            error_idx = hr.table_index;
            error_mod_id = hr.mod_id;
            stopped = true;
            break;
        }
    }

    if( error ) {
        if( error_mod_id.empty() ) {
            debugmsg( "Failed to run hook %s[%d]: %s", hook_name, error_idx, error->c_str() );
        } else {
            debugmsg( "Failed to run hook %s[%d] (mod: %s): %s", hook_name, error_idx,
                      error_mod_id.c_str(), error->c_str() );
        }
    }

    return stopped;
}

} // namespace lua_hooks_detail

auto run_hooks( std::string_view hook_name,
                std::function<void( sol::table &params )> init,
                const hook_opts &opts ) -> std::optional<bool>
{
    bool vetoed = false;

    lua_hooks_detail::run_hooks( hook_name, opts, init,
    [&]( const hook_result & res, sol::table & ) -> bool {
        // Veto semantics: if hook returns boolean false, it's a veto
        if( res.value.is<bool>() && !res.value.as<bool>() )
        {
            vetoed = true;
            return true;  // Stop iteration
        }
        return false;  // Continue
    } );

    if( vetoed ) {
        return false;
    }
    return std::nullopt;
}

auto run_hooks_collect( std::string_view hook_name,
                        std::function<void( sol::table &params )> init,
                        hook_opts opts ) -> std::vector<hook_result>
{
    opts.stop_on_result = false;  // Force iteration through all hooks

    std::vector<hook_result> results;

    lua_hooks_detail::run_hooks( hook_name, opts, init,
    [&]( const hook_result & res, sol::table & ) -> bool {
        if( res.has_value )
        {
            results.push_back( res );
        }
        return false;  // Never stop
    } );

    return results;
}


void reg_lua_iuse_actors( lua_state &state, Item_factory &ifactory )
{
    sol::state &lua = state.lua;

    const sol::table funcs = lua.globals()["game"]["iuse_functions"];

    for( auto &ref : funcs ) {
        std::string key;
        try {
            key = ref.first.as<std::string>();

            switch( ref.second.get_type() ) {
                case sol::type::function: {
                    auto func =  ref.second.as<sol::function>();
                    ifactory.add_actor( std::make_unique<lua_iuse_actor>(
                                            key,
                                            std::move( func ),
                                            sol::lua_nil,
                                            sol::lua_nil ) );
                    break;
                }
                case sol::type::table: {
                    auto tbl = ref.second.as<sol::table>();
                    auto use_fn = tbl.get<sol::function>( "use" );
                    auto can_use_fn = tbl.get_or<sol::function>( "can_use", sol::lua_nil );
                    auto tick_fn = tbl.get_or<sol::function>( "tick", sol::lua_nil );
                    ifactory.add_actor( std::make_unique<lua_iuse_actor>(
                                            key,
                                            std::move( use_fn ),
                                            std::move( can_use_fn ),
                                            std::move( tick_fn ) ) );
                    break;
                }
                default: {
                    throw std::runtime_error( "invalid iuse object type, expected table or function" );
                }
            }
        } catch( std::runtime_error &e ) {
            debugmsg( "Failed to extract iuse_functions k='%s': %s", key, e.what() );
            break;
        }
    }
}

void run_on_every_x_hooks( lua_state &state )
{
    std::vector<cata::on_every_x_hooks> &master_table =
        state.lua["game"]["cata_internal"]["on_every_x_hooks"];
    for( auto &entry : master_table ) {
        if( calendar::once_every( entry.interval ) ) {
            entry.functions.erase(
                std::remove_if(
                    entry.functions.begin(), entry.functions.end(),
            [&entry]( auto & func ) {
                try {
                    sol::protected_function_result res = func();
                    check_func_result( res );
                    // erase function only if it returns a boolean AND it's false
                    return res.get_type() == sol::type::boolean && !res.get<bool>();
                } catch( std::runtime_error &e ) {
                    debugmsg(
                        "Failed to run hook on_every_x(interval = %s): %s",
                        to_string( entry.interval ), e.what()
                    );
                }
                return false;
            }
                ),
            entry.functions.end()
            );
        }
    }
}

} // namespace cata

namespace cata
{

int get_lua_api_version()
{
    return LUA_API_VERSION;
}

void lua_state_deleter::operator()( lua_state *state ) const
{
    delete state;
}

void run_on_game_save_hooks( lua_state &state )
{
    run_hooks( "on_game_save", nullptr, { .state = &state } );
}

void run_on_game_load_hooks( lua_state &state )
{
    run_hooks( "on_game_load", nullptr, { .state = &state } );
}

void run_on_mapgen_postprocess_hooks( lua_state &state, map &m, const tripoint &p,
                                      const time_point &when )
{
    run_hooks( "on_mapgen_postprocess", [&]( sol::table & params ) {
        params["map"] = &m;
        params["omt"] = p;
        params["when"] = when;
    }, { .state = &state } );
}

} // namespace cata
