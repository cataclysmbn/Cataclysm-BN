#pragma once

#include "catalua_luna.h"
#include "coordinates.h"
#include "point.h"

#include <optional>
#include <type_traits>

namespace cata::detail::lua_coords
{

struct lua_point_coord {
    point raw;
    coords::origin origin;
    coords::scale scale;
};

struct lua_tripoint_coord {
    tripoint raw;
    coords::origin origin;
    coords::scale scale;
};

template<typename Coord>
auto coord_from_raw_point( lua_State *L, const int index,
                           sol::stack::record &tracking ) -> std::optional<Coord>
{
    if constexpr( Coord::dimension == 2 ) {
        if( sol::stack::check<point>( L, index, &sol::no_panic ) ) {
            tracking.use( 1 );
            return Coord( sol::stack::get<point>( L, index ) );
        }
    } else {
        if( sol::stack::check<tripoint>( L, index, &sol::no_panic ) ) {
            tracking.use( 1 );
            return Coord( sol::stack::get<tripoint>( L, index ) );
        }
    }
    return std::nullopt;
}

template<typename Coord>
auto coord_from_lua_coord( lua_State *L, const int index,
                           sol::stack::record &tracking ) -> std::optional<Coord>
{
    if constexpr( Coord::dimension == 2 ) {
        if( sol::stack::check<lua_point_coord>( L, index, &sol::no_panic ) ) {
            const auto coord = sol::stack::get<lua_point_coord>( L, index );
            if( coord.origin == Coord::origin_tag && coord.scale == Coord::scale_tag ) {
                tracking.use( 1 );
                return Coord( coord.raw );
            }
        }
    } else {
        if( sol::stack::check<lua_tripoint_coord>( L, index, &sol::no_panic ) ) {
            const auto coord = sol::stack::get<lua_tripoint_coord>( L, index );
            if( coord.origin == Coord::origin_tag && coord.scale == Coord::scale_tag ) {
                tracking.use( 1 );
                return Coord( coord.raw );
            }
        }
    }
    return std::nullopt;
}

template<typename Coord>
auto coord_from_lua( lua_State *L, const int index,
                     sol::stack::record &tracking ) -> std::optional<Coord>
{
    if( const auto coord = coord_from_lua_coord<Coord>( L, index, tracking ) ) {
        return coord;
    }
    return coord_from_raw_point<Coord>( L, index, tracking );
}

template<typename Coord>
auto push_raw_coord( lua_State *L, const Coord &coord ) -> int
{
    return sol::stack::push( L, coord.raw() );
}

} // namespace cata::detail::lua_coords

LUNA_VAL( cata::detail::lua_coords::lua_point_coord, "PointCoord" );
LUNA_VAL( cata::detail::lua_coords::lua_tripoint_coord, "TripointCoord" );

namespace coords
{

template<typename Point, origin Origin, scale Scale, typename Handler>
auto sol_lua_check( sol::types<coord_point<Point, Origin, Scale>>, lua_State *L,
                    const int index, Handler &&handler,
                    sol::stack::record &tracking ) -> bool
{
    using Coord = coord_point<Point, Origin, Scale>;
    auto local_tracking = sol::stack::record{};
    if( cata::detail::lua_coords::coord_from_lua<Coord>( L, index, local_tracking ) ) {
        tracking.use( 1 );
        return true;
    }
    tracking.use( 1 );
    handler( L, index, sol::type::userdata, sol::type_of( L, index ),
             "expected raw point/tripoint or matching PointCoord/TripointCoord" );
    return false;
}

template<typename Point, origin Origin, scale Scale>
auto sol_lua_get( sol::types<coord_point<Point, Origin, Scale>>, lua_State *L, const int index,
                  sol::stack::record &tracking ) -> coord_point<Point, Origin, Scale>
{
    using Coord = coord_point<Point, Origin, Scale>;
    if( const auto coord = cata::detail::lua_coords::coord_from_lua<Coord>( L, index,
                           tracking ) ) {
        return *coord;
    }
    tracking.use( 1 );
    return Coord();
}

template<typename Point, origin Origin, scale Scale, typename Handler>
auto sol_lua_check_get( sol::types<coord_point<Point, Origin, Scale>>, lua_State *L,
                        const int index, Handler &&handler,
                        sol::stack::record &tracking ) -> sol::optional<coord_point<Point, Origin, Scale>>
{
    using Coord = coord_point<Point, Origin, Scale>;
    if( const auto coord = cata::detail::lua_coords::coord_from_lua<Coord>( L, index,
                           tracking ) ) {
        return *coord;
    }
    tracking.use( 1 );
    handler( L, index, sol::type::userdata, sol::type_of( L, index ),
             "expected raw point/tripoint or matching PointCoord/TripointCoord" );
    return sol::nullopt;
}

template<typename Point, origin Origin, scale Scale>
auto sol_lua_push( sol::types<coord_point<Point, Origin, Scale>>, lua_State *L,
                   const coord_point<Point, Origin, Scale> &coord ) -> int
{
    return cata::detail::lua_coords::push_raw_coord( L, coord );
}

} // namespace coords
