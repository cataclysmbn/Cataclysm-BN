#include "catalua_iuse_actor.h"

#include "catalua_impl.h"
#include "player.h"

lua_iuse_actor::lua_iuse_actor( const std::string &type,
                                sol::protected_function &&use_func,
                                sol::protected_function &&can_use_func,
                                sol::protected_function &&tick_func )
    : iuse_actor( type ),
      use_func( use_func ),
      can_use_func( can_use_func ),
      tick_func( tick_func ) {}

lua_iuse_actor::~lua_iuse_actor() = default;

void lua_iuse_actor::load( const JsonObject & )
{
    // TODO: custom data
}

int lua_iuse_actor::use( player &who, item &itm, bool tick, const tripoint &pos ) const
{
    try {
        if( !tick ) {
            sol::protected_function_result res = use_func( who.as_character(), itm, pos );
            check_func_result( res );
            int ret = res;
            return ret;
        } else if( tick_func != sol::lua_nil ) {
            sol::protected_function_result res = tick_func( who.as_character(), itm, pos );
            check_func_result( res );
            int ret = res;
            return ret;
        }
    } catch( std::runtime_error &e ) {
        debugmsg( "Failed to run iuse_function k='%s': %s", type, e.what() );
    }
    return 1;
}

ret_val<bool> lua_iuse_actor::can_use( const Character &who, const item &item, bool,
                                       const tripoint &pos ) const
{
    if( can_use_func != sol::lua_nil ) {
        sol::protected_function_result res = can_use_func( who.as_character(), item, pos );
        check_func_result( res );
        const bool ret = res;
        return ret
               ? ret_val<bool>::make_success()
               : ret_val<bool>::make_failure();
    }
    return ret_val<bool>::make_success();
}

std::unique_ptr<iuse_actor> lua_iuse_actor::clone() const
{
    return std::make_unique<lua_iuse_actor>( *this );
}
