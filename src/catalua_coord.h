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
auto push_coord( lua_State *L, const Coord &coord ) -> int
{
    if constexpr( Coord::dimension == 2 ) {
        return sol::stack::push( L, lua_point_coord{
            coord.raw(), Coord::origin_tag, Coord::scale_tag
        } );
    } else {
        return sol::stack::push( L, lua_tripoint_coord{
            coord.raw(), Coord::origin_tag, Coord::scale_tag
        } );
    }
}

} // namespace cata::detail::lua_coords

namespace coords
{

template<typename T>
struct is_lua_coord_point : std::false_type {};

template<typename Point, origin Origin, scale Scale>
struct is_lua_coord_point<coord_point<Point, Origin, Scale>> : std::true_type {};

template<typename Coord>
using lua_coord_value_t = std::remove_cvref_t<Coord>;

template<typename Coord>
inline constexpr bool lua_coord_can_read_v =
    is_lua_coord_point<lua_coord_value_t<Coord>>::value &&
    ( !std::is_lvalue_reference_v<Coord> || std::is_const_v<std::remove_reference_t<Coord>> );

template<typename Coord>
using enable_lua_coord_point_t = std::enable_if_t<lua_coord_can_read_v<Coord>, int>;

template<typename Coord, typename Handler, enable_lua_coord_point_t<Coord> = 0>
auto sol_lua_check( sol::types<Coord>, lua_State *L,
                    const int index, Handler &&handler,
                    sol::stack::record &tracking ) -> bool
{
    using Value = lua_coord_value_t<Coord>;
    auto local_tracking = sol::stack::record{};
    if( cata::detail::lua_coords::coord_from_lua<Value>( L, index,
            local_tracking ) ) {
        tracking.use( 1 );
        return true;
    }
    tracking.use( 1 );
    handler( L, index, sol::type::userdata, sol::type_of( L, index ),
             "expected matching PointCoord/TripointCoord" );
    return false;
}

template<typename Coord, enable_lua_coord_point_t<Coord> = 0>
auto sol_lua_get( sol::types<Coord>, lua_State *L, const int index,
                  sol::stack::record &tracking ) -> lua_coord_value_t<Coord>
{
    using Value = lua_coord_value_t<Coord>;
    if( const auto coord = cata::detail::lua_coords::coord_from_lua<Value>( L, index,
                           tracking ) ) {
        return *coord;
    }
    tracking.use( 1 );
    return Value();
}

template<typename Coord, typename Handler, enable_lua_coord_point_t<Coord> = 0>
auto sol_lua_check_get( sol::types<Coord>, lua_State *L,
                        const int index, Handler &&handler,
                        sol::stack::record &tracking ) -> sol::optional<lua_coord_value_t<Coord>>
{
    using Value = lua_coord_value_t<Coord>;
    if( const auto coord = cata::detail::lua_coords::coord_from_lua<Value>( L, index,
                           tracking ) ) {
        return *coord;
    }
    tracking.use( 1 );
    handler( L, index, sol::type::userdata, sol::type_of( L, index ),
             "expected matching PointCoord/TripointCoord" );
    return sol::nullopt;
}

template<typename Point, origin Origin, scale Scale>
auto sol_lua_push( sol::types<coord_point<Point, Origin, Scale>>, lua_State *L,
                   const coord_point<Point, Origin, Scale> &coord ) -> int
{
    return cata::detail::lua_coords::push_coord( L, coord );
}

} // namespace coords
