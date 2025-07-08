#include "catalua_iuse_actor.h"

#include "catalua_impl.h"
#include "player.h"

lua_iuse_actor::lua_iuse_actor( const std::string &type, sol::protected_function &&luafunc )
    : iuse_actor( type ), luafunc( luafunc ) {}

lua_iuse_actor::~lua_iuse_actor() = default;

void lua_iuse_actor::load( const JsonObject & )
{
    // TODO: custom data
}

int lua_iuse_actor::use( player &who, item &itm, bool tick, const tripoint &pos ) const
{
    if( !tick ) {
        try {
            sol::protected_function_result res = luafunc( who.as_character(), itm, pos );
            check_func_result( res );
            int ret = res;
            return ret;
        } catch( std::runtime_error &e ) {
            debugmsg( "Failed to run iuse_function k='%s': %s", type, e.what() );
        }
    } else {
        // TODO: ticking use
    }
    return 1;
}

ret_val<bool> lua_iuse_actor::can_use( const Character &, const item &, bool,
                                       const tripoint & ) const
{
    // TODO: check if can use
    return ret_val<bool>::make_success();
}

std::unique_ptr<iuse_actor> lua_iuse_actor::clone() const
{
    return std::make_unique<lua_iuse_actor>( *this );
}
