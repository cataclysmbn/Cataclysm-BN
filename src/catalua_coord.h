#pragma once

#include "catalua_luna.h"
#include "coordinates.h"
#include "point.h"

#include <algorithm>
#include <iterator>
#include <optional>
#include <stdexcept>
#include <type_traits>
#include <vector>

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
concept cpp_coord_point = coords::IsCoordPoint<std::remove_cvref_t<Coord>>;

template<typename Coord>
using cpp_coord_value_t = std::remove_cvref_t<Coord>;

template<typename Coord>
using lua_coord_for_t = std::conditional_t<cpp_coord_value_t<Coord>::dimension == 2,
      lua_point_coord, lua_tripoint_coord>;

template<cpp_coord_point Coord>
auto to_lua( const Coord &coord ) -> lua_coord_for_t<Coord>
{
    using Value = cpp_coord_value_t<Coord>;
    if constexpr( Value::dimension == 2 ) {
        return lua_point_coord{ coord.raw(), Value::origin_tag, Value::scale_tag };
    } else {
        return lua_tripoint_coord{ coord.raw(), Value::origin_tag, Value::scale_tag };
    }
}

template<cpp_coord_point Coord>
auto as_cpp( const lua_point_coord &coord ) -> std::optional<cpp_coord_value_t<Coord>>
{
    using Value = cpp_coord_value_t<Coord>;
    if constexpr( Value::dimension == 2 ) {
        if( coord.origin == Value::origin_tag && coord.scale == Value::scale_tag ) {
            return Value( coord.raw );
        }
    }
    return std::nullopt;
}

template<cpp_coord_point Coord>
auto as_cpp( const lua_tripoint_coord &coord ) -> std::optional<cpp_coord_value_t<Coord>>
{
    using Value = cpp_coord_value_t<Coord>;
    if constexpr( Value::dimension == 3 ) {
        if( coord.origin == Value::origin_tag && coord.scale == Value::scale_tag ) {
            return Value( coord.raw );
        }
    }
    return std::nullopt;
}

template<cpp_coord_point Coord>
auto as_cpp( const sol::object &obj ) -> std::optional<cpp_coord_value_t<Coord>>
{
    using Value = cpp_coord_value_t<Coord>;
    if constexpr( Value::dimension == 2 ) {
        if( obj.is<lua_point_coord>() ) {
            return as_cpp<Value>( obj.as<lua_point_coord>() );
        }
    } else {
        if( obj.is<lua_tripoint_coord>() ) {
            return as_cpp<Value>( obj.as<lua_tripoint_coord>() );
        }
    }
    return std::nullopt;
}

template<cpp_coord_point Coord, typename LuaCoord>
auto expect_cpp( const LuaCoord &coord ) -> cpp_coord_value_t<Coord>
{
    if( const auto result = as_cpp<Coord>( coord ) ) {
        return *result;
    }
    throw std::runtime_error( "Lua coordinate kind does not match requested C++ coordinate type" );
}

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
    return sol::stack::push( L, to_lua( coord ) );
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
                    const int index, Handler && handler,
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
                        const int index, Handler && handler,
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

template<typename Point, origin Origin, scale Scale>
auto sol_lua_push( sol::types<std::vector<coord_point<Point, Origin, Scale>>>, lua_State *L,
                   const std::vector<coord_point<Point, Origin, Scale>> &coords ) -> int
{
    using Coord = coord_point<Point, Origin, Scale>;
    auto lua_values = std::vector<cata::detail::lua_coords::lua_coord_for_t<Coord>> {};
    lua_values.reserve( coords.size() );
    std::ranges::transform( coords, std::back_inserter( lua_values ), []( const Coord & coord ) {
        return cata::detail::lua_coords::to_lua( coord );
    } );
    return sol::stack::push( L, std::move( lua_values ) );
}

} // namespace coords
