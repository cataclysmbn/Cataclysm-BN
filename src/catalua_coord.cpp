#include "catalua_coord.h"

namespace cata::detail::lua_coords
{

auto read_point_coord_from_lua( const point_coord_lua_read_options &options ) -> bool
{
    if( sol::stack::check<lua_point_coord>( options.L, options.index, &sol::no_panic ) ) {
        const auto coord = sol::stack::get<lua_point_coord>( options.L, options.index );
        if( coord.origin == options.origin && coord.scale == options.scale ) {
            *options.out = coord.raw;
            return true;
        }
        return false;
    }
    if( sol::stack::check<point>( options.L, options.index, &sol::no_panic ) ) {
        *options.out = sol::stack::get<point>( options.L, options.index );
        return true;
    }
    return false;
}

auto read_tripoint_coord_from_lua( const tripoint_coord_lua_read_options &options ) -> bool
{
    if( sol::stack::check<lua_tripoint_coord>( options.L, options.index, &sol::no_panic ) ) {
        const auto coord = sol::stack::get<lua_tripoint_coord>( options.L, options.index );
        if( coord.origin == options.origin && coord.scale == options.scale ) {
            *options.out = coord.raw;
            return true;
        }
        return false;
    }
    if( sol::stack::check<tripoint>( options.L, options.index, &sol::no_panic ) ) {
        *options.out = sol::stack::get<tripoint>( options.L, options.index );
        return true;
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
