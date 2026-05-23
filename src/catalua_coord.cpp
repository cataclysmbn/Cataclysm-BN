#include "catalua_coord.h"

namespace cata::detail::lua_coords
{

auto read_point_coord_from_lua( const point_coord_lua_read_options &options ) -> bool
{
    const auto obj = sol::stack::get<sol::object>( options.L, options.index );
    if( obj.is<lua_point_coord>() ) {
        const auto coord = obj.as<lua_point_coord>();
        if( coord.origin == options.origin && coord.scale == options.scale ) {
            *options.out = coord.raw;
            return true;
        }
        return false;
    }
    return false;
}

auto read_tripoint_coord_from_lua( const tripoint_coord_lua_read_options &options ) -> bool
{
    const auto obj = sol::stack::get<sol::object>( options.L, options.index );
    if( obj.is<lua_tripoint_coord>() ) {
        const auto coord = obj.as<lua_tripoint_coord>();
        if( coord.origin == options.origin && coord.scale == options.scale ) {
            *options.out = coord.raw;
            return true;
        }
        return false;
    }
    return false;
}

auto push_raw_point( lua_State *L, const point &raw ) -> int
{
    return sol::stack::push( L, raw );
}

auto push_raw_tripoint( lua_State *L, const tripoint &raw ) -> int
{
    return sol::stack::push( L, raw );
}

} // namespace cata::detail::lua_coords
