#pragma once

#include "catalua_luna.h"
#include "coordinates.h"
#include "point.h"

#include <optional>

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

struct point_coord_lua_read_options {
    lua_State *L;
    int index;
    coords::origin origin;
    coords::scale scale;
    point *out;
};

struct tripoint_coord_lua_read_options {
    lua_State *L;
    int index;
    coords::origin origin;
    coords::scale scale;
    tripoint *out;
};

auto read_point_coord_from_lua( const point_coord_lua_read_options &options ) -> bool;
auto read_tripoint_coord_from_lua( const tripoint_coord_lua_read_options &options ) -> bool;
auto push_raw_point( lua_State *L, const point &raw ) -> int;
auto push_raw_tripoint( lua_State *L, const tripoint &raw ) -> int;

template<typename Coord>
auto coord_from_lua( lua_State *L, const int index,
                     sol::stack::record &tracking ) -> std::optional<Coord>
{
    if constexpr( Coord::dimension == 2 ) {
        auto raw = point{};
        if( read_point_coord_from_lua( { .L = L, .index = index, .origin = Coord::origin_tag,
                                         .scale = Coord::scale_tag, .out = &raw } ) ) {
            tracking.use( 1 );
            return Coord( raw );
        }
    } else {
        auto raw = tripoint{};
        if( read_tripoint_coord_from_lua( { .L = L, .index = index, .origin = Coord::origin_tag,
                                            .scale = Coord::scale_tag, .out = &raw } ) ) {
            tracking.use( 1 );
            return Coord( raw );
        }
    }
    return std::nullopt;
}

template<typename Coord>
auto push_raw_coord( lua_State *L, const Coord &coord ) -> int
{
    if constexpr( Coord::dimension == 2 ) {
        return push_raw_point( L, coord.raw() );
    } else {
        return push_raw_tripoint( L, coord.raw() );
    }
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
