#include "catalua_mapgen.h"
#include "catalua.h"
#include "catalua_impl.h"
#include "init.h"
#include "player.h"
#include "game.h"
#include "mapgendata.h"
#include "thread_pool.h"
#include "sol/sol.hpp"

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
    // Must not be called from a pool worker thread: generate_quad() detects
    // Lua-based terrain and defers the whole quad to drain_deferred_lua_quads().
    assert( !is_pool_worker_thread() );
    if( generate_func.valid() ) {
        sol::protected_function_result res = generate_func( &dat, &dat.m );
        check_func_result( res );
    }
}
