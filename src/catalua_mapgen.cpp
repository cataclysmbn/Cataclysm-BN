#include "catalua_mapgen.h"

#include "catalua.h"
#include "catalua_impl.h"
#include "editor/mapgen_source.h"
#include "game.h"
#include "init.h"
#include "mapgen/mapgen_constructor.h"
#include "mapgen/mapgendata.h"
#include "player.h"
#include "profile.h"
#include "sol/sol.hpp"
#include "string_formatter.h"
#include "thread_pool.h"

#include <cassert>

mapgen_function_lua::mapgen_function_lua( const std::string &func,
        int weight ) : mapgen_function( weight )
{
    sol::state &lua = DynamicDataLoader::get_instance().lua->lua;
    sol::object ref = lua.globals()["game"]["mapgen_functions"][func];
    if( ref.get_type() == sol::type::function ) {
        auto luafunc = ref.as<sol::function>();
        generate_func = std::move( luafunc );
    }
}

void mapgen_function_lua::generate( mapgendata &dat )
{
    ZoneScopedN( "mapgen_lua_generate" );
    // Lua mapgen must always run on the main thread.
    assert( !is_pool_worker_thread() );
    if( generate_func.valid() ) {
        sol::protected_function_result res = generate_func( &dat, &dat.m );
        check_func_result( res );
    }
}

auto editor::lua_mapgen_defined_at( const std::string &name ) -> std::optional<std::string>
{
    auto *const loaded = DynamicDataLoader::get_instance().lua.get();
    if( loaded == nullptr ) {
        return std::nullopt;
    }
    const sol::optional<sol::function> function = loaded->lua["game"]["mapgen_functions"][name];
    if( !function ) {
        return std::nullopt;
    }
    lua_State *const state = loaded->lua.lua_state();
    function->push( state );
    auto debug = lua_Debug{};
    lua_getinfo( state, ">S", &debug );
    // A chunk loaded from a file names it after an `@`.
    if( debug.source == nullptr || debug.source[0] != '@' ) {
        return std::nullopt;
    }
    return string_format( "%s:%d", debug.source + 1, debug.linedefined );
}
