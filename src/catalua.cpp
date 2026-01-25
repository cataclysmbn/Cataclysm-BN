#include "catalua.h"

#include "debug.h"

#include <algorithm>
#include <clocale>
#include <optional>
#include <ranges>
#include <string>
#include <vector>

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
            const auto str = lua["tostring"]( it ).get<std::string>();
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
        const auto ret = res.get<std::string>();
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

    const auto data = sol::stack::pop<std::string>( container );
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
    const auto ret = world->write_to_file( path, [&]( std::ostream & stream ) {
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

    const auto ret = world->read_from_file( path, [&]( std::istream & stream ) {
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
    it["hook_test_results"] = lua.create_table();
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

    gt["add_hook"] = [&lua]( const std::string & hook_name, const sol::object & entry ) {
        auto *L = lua.lua_state();
        sol::table hooks_table = lua["game"]["hooks"];
        sol::optional<sol::table> maybe_hook_list = hooks_table[hook_name];

        if( !maybe_hook_list ) {
            debugmsg( "Invalid hook name: %s", hook_name );
            return;
        }

        sol::table hook_list = *maybe_hook_list;

        const auto current_mod = lua["game"]["current_mod"];
        const auto mod_id = current_mod.valid() && current_mod.get_type() == sol::type::string
                            ? current_mod.get<std::string>()
                            : "<unknown>";

        const auto is_function = entry.is<sol::function>() || entry.is<sol::protected_function>();
        if( is_function ) {
            auto new_entry = lua.create_table();
            new_entry["mod_id"] = mod_id;
            new_entry["priority"] = 0;
            new_entry["fn"] = entry;

            sol::stack::push( L, hook_list );
            const auto next_index = static_cast<int>( lua_rawlen( L, -1 ) ) + 1;
            lua_pop( L, 1 );

            hook_list.set( next_index, new_entry );
            return;
        }

        if( entry.is<sol::table>() ) {
            auto tbl = entry.as<sol::table>();
            const auto has_mod_id = tbl["mod_id"].valid() && tbl["mod_id"].get_type() != sol::type::lua_nil;
            if( !has_mod_id ) {
                tbl["mod_id"] = mod_id;
            }

            sol::stack::push( L, hook_list );
            const auto next_index = static_cast<int>( lua_rawlen( L, -1 ) ) + 1;
            lua_pop( L, 1 );

            hook_list.set( next_index, tbl );
            return;
        }

        debugmsg( "add_hook expects function or table entry, got type: %s for hook: %s",
                  sol::type_name( lua, entry.get_type() ).c_str(), hook_name.c_str() );
    };
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
    const auto script_path = mod->path + "/" + "preload.lua";

    if( !file_exist( script_path ) ) {
        return;
    }

    run_lua_script( state.lua, script_path );
}

void run_mod_finalize_script( lua_state &state, const mod_id &mod )
{
    const auto script_path = mod->path + "/" + "finalize.lua";

    if( !file_exist( script_path ) ) {
        return;
    }

    run_lua_script( state.lua, script_path );
}

void run_mod_main_script( lua_state &state, const mod_id &mod )
{
    const auto script_path = mod->path + "/" + "main.lua";

    if( !file_exist( script_path ) ) {
        return;
    }

    run_lua_script( state.lua, script_path );
}

namespace
{

struct hook_entry {
    int priority = 0;
    int index = 0;
    bool is_table = false;
    std::string mod_id;
};

struct hook_cache_entry {
    int rawlen = -1;
    std::vector<hook_entry> entries;
};

auto get_hook_cache() -> std::unordered_map<std::string, hook_cache_entry> &
{
    static auto cache = std::unordered_map<std::string, hook_cache_entry> {};
    return cache;
}

auto table_rawlen( sol::state_view lua, const sol::table &t ) -> int
{
    auto *L = lua.lua_state();
    sol::stack::push( L, t );
    const auto len = static_cast<int>( lua_rawlen( L, -1 ) );
    lua_pop( L, 1 );
    return len;
}

auto build_hook_entries( sol::state_view lua, std::string_view hook_name,
                         const sol::table &hooks ) -> std::vector<hook_entry>
{
    auto entries = std::vector<hook_entry> {};

    const int len = table_rawlen( lua, hooks );
    entries.reserve( std::max( 0, len ) );

    auto needs_sort = false;

    for( int idx = 1; idx <= len; ++idx ) {
        try {
            const auto obj = hooks.get_or<sol::object>( idx, sol::lua_nil );
            if( obj == sol::lua_nil ) {
                continue;
            }

            if( obj.is<sol::protected_function>() || obj.is<sol::function>() ) {
                entries.emplace_back( hook_entry{
                    .priority = 0,
                    .index = idx,
                    .is_table = false,
                    .mod_id = "<unknown>",
                } );
                continue;
            }

            if( obj.is<sol::table>() ) {
                const auto tbl = obj.as<sol::table>();

                const auto mod_id = tbl.get_or<std::string>( "mod_id", "<unknown>" );
                const auto priority_obj = tbl.get<sol::optional<int>>( "priority" );
                const auto priority = priority_obj.value_or( 0 );
                const auto hook_obj = tbl.get_or<sol::object>( "fn", sol::lua_nil );

                if( !( hook_obj.is<sol::protected_function>() || hook_obj.is<sol::function>() ) ) {
                    throw std::runtime_error( "invalid hook entry: expected function at key 'fn'" );
                }

                needs_sort = needs_sort || priority != 0;

                entries.emplace_back( hook_entry{
                    .priority = priority,
                    .index = idx,
                    .is_table = true,
                    .mod_id = mod_id,
                } );
                continue;
            }

            throw std::runtime_error( "invalid hook entry: expected function or table" );
        } catch( const std::runtime_error &e ) {
            debugmsg( "Failed to parse hook %s[%d]: %s", hook_name, idx, e.what() );
        }
    }

    if( needs_sort ) {
        std::ranges::stable_sort( entries, std::ranges::greater{}, []( const hook_entry & e ) { return e.priority; } );
    }

    return entries;
}

auto get_hook_entries( sol::state_view lua, std::string_view hook_name,
                       const sol::table &hooks ) -> const std::vector<hook_entry> &
{
    auto &cache = get_hook_cache();
    auto &entry = cache[std::string{ hook_name }];

    const int len = table_rawlen( lua, hooks );
    if( entry.rawlen != len ) {
        entry.rawlen = len;
        entry.entries = build_hook_entries( lua, hook_name, hooks );
    }

    return entry.entries;
}

} // namespace

auto run_hooks( std::string_view hook_name,
                std::function < auto( sol::table &params ) -> void > init,
                const hook_opts &opts ) -> sol::table
{
    auto &state = opts.state ? *opts.state : *DynamicDataLoader::get_instance().lua;
    auto &lua = state.lua;

    auto params = lua.create_table();
    auto results = lua.create_table();
    results["allowed"] = true;

    params["results"] = results;
    params["prev"] = sol::lua_nil;

    if( init ) {
        init( params );
    }

    const auto maybe_hooks = lua.globals()["game"]["hooks"][hook_name].get<sol::optional<sol::table>>();
    if( !maybe_hooks ) {
        return results;
    }

    const auto &hooks = *maybe_hooks;
    const auto &entries = get_hook_entries( lua, hook_name, hooks );

    auto out_idx = 1;
    auto i = size_t{ 0 };
    while( i < entries.size() ) {
        const hook_entry &e = entries[i];
        try {
            const sol::object obj = hooks.get_or<sol::object>( e.index, sol::lua_nil );
            if( obj == sol::lua_nil ) {
                ++i;
                continue;
            }

            sol::protected_function func;
            if( e.is_table ) {
                const sol::table tbl = obj.as<sol::table>();
                const sol::object hook_obj = tbl.get_or<sol::object>( "fn", sol::lua_nil );
                func = hook_obj.as<sol::protected_function>();
            } else {
                func = obj.as<sol::protected_function>();
            }

            sol::protected_function_result res = func( params );
            check_func_result( res );

            sol::object result = sol::make_object( lua, sol::lua_nil );
            if( res.valid() ) {
                result = res.get<sol::object>();
            }

            params["prev"] = result;

            sol::table one = lua.create_table();
            one["mod_id"] = e.mod_id;
            one["priority"] = e.priority;
            if( result != sol::lua_nil ) {
                one["result"] = result;
            }
            results[out_idx++] = one;

            if( result.is<bool>() && !result.as<bool>() ) {
                results["allowed"] = false;
                if( opts.exit_early ) {
                    break;
                }
            }
        } catch( const std::runtime_error &e_err ) {
            debugmsg( "Failed to run hook %s[%d](%s): %s", hook_name, static_cast<int>( i ), e.mod_id.c_str(),
                      e_err.what() );
        }
        ++i;
    }

    return results;
}


void reg_lua_iuse_actors( lua_state &state, Item_factory &ifactory )
{
    sol::state &lua = state.lua;

    const sol::table funcs = lua.globals()["game"]["iuse_functions"];

    auto it = funcs.begin();
    while( it != funcs.end() ) {
        const auto ref = *it;
        std::string key;
        try {
            key = ref.first.as<std::string>();

            switch( ref.second.get_type() ) {
                case sol::type::function: {
                    auto func = ref.second.as<sol::function>();
                    ifactory.add_actor( std::make_unique<lua_iuse_actor>(
                                            key,
                                            std::move( func ),
                                            sol::lua_nil,
                                            sol::lua_nil ) );
                    break;
                }
                case sol::type::table: {
                    const auto tbl = ref.second.as<sol::table>();
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
        ++it;
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
