#include "catalua_mapgen.h"
#include "catalua.h"
#include "catalua_impl.h"
#include "init.h"
#include "player.h"

mapgen_function_lua::mapgen_function_lua( const std::string &func, int weight ) : mapgen_function( weight )
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
    generate_func( dat, g->m );
}
