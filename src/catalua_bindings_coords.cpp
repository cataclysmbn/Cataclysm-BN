#include "catalua_bindings.h"

#include "catalua_bindings_utils.h"
#include "catalua_luna_doc.h"
#include "catalua_luna.h"
#include "coordinates.h"
#include "json.h"
#include "line.h"
#include "point.h"
#include "sol/forward.hpp"

#include <optional>
#include <stdexcept>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>
#include <variant>

namespace
{

namespace detail
{
template<coords::origin, coords::scale>
struct is_registered_coord_t : std::false_type {};

template<coords::scale>
struct has_remainder_origin_t : std::false_type {};

}

// *INDENT-OFF*
template<> struct detail::is_registered_coord_t<coords::origin::relative        , coords::ms    > : std::true_type {};
template<> struct detail::is_registered_coord_t<coords::origin::relative        , coords::veh   > : std::true_type {};
template<> struct detail::is_registered_coord_t<coords::origin::relative        , coords::sm    > : std::true_type {};
template<> struct detail::is_registered_coord_t<coords::origin::relative        , coords::omt   > : std::true_type {};
template<> struct detail::is_registered_coord_t<coords::origin::relative        , coords::mmr   > : std::true_type {};
template<> struct detail::is_registered_coord_t<coords::origin::relative        , coords::seg   > : std::true_type {};
template<> struct detail::is_registered_coord_t<coords::origin::relative        , coords::om    > : std::true_type {};
template<> struct detail::is_registered_coord_t<coords::origin::abs             , coords::ms    > : std::true_type {};
template<> struct detail::is_registered_coord_t<coords::origin::abs             , coords::sm    > : std::true_type {};
template<> struct detail::is_registered_coord_t<coords::origin::abs             , coords::omt   > : std::true_type {};
template<> struct detail::is_registered_coord_t<coords::origin::abs             , coords::mmr   > : std::true_type {};
template<> struct detail::is_registered_coord_t<coords::origin::abs             , coords::seg   > : std::true_type {};
template<> struct detail::is_registered_coord_t<coords::origin::abs             , coords::om    > : std::true_type {};
template<> struct detail::is_registered_coord_t<coords::origin::bubble          , coords::ms    > : std::true_type {};
template<> struct detail::is_registered_coord_t<coords::origin::bubble          , coords::sm    > : std::true_type {};
template<> struct detail::is_registered_coord_t<coords::origin::vehicle         , coords::veh   > : std::true_type {};
template<> struct detail::is_registered_coord_t<coords::origin::submap          , coords::ms    > : std::true_type {};
template<> struct detail::is_registered_coord_t<coords::origin::overmap_terrain , coords::ms    > : std::true_type {};
template<> struct detail::is_registered_coord_t<coords::origin::overmap_terrain , coords::sm    > : std::true_type {};
template<> struct detail::is_registered_coord_t<coords::origin::mem_map_region  , coords::ms    > : std::true_type {};
template<> struct detail::is_registered_coord_t<coords::origin::mem_map_region  , coords::sm    > : std::true_type {};
template<> struct detail::is_registered_coord_t<coords::origin::mem_map_region  , coords::omt   > : std::true_type {};
template<> struct detail::is_registered_coord_t<coords::origin::segment         , coords::ms    > : std::true_type {};
template<> struct detail::is_registered_coord_t<coords::origin::segment         , coords::sm    > : std::true_type {};
template<> struct detail::is_registered_coord_t<coords::origin::segment         , coords::omt   > : std::true_type {};
template<> struct detail::is_registered_coord_t<coords::origin::segment         , coords::mmr   > : std::true_type {};
template<> struct detail::is_registered_coord_t<coords::origin::overmap         , coords::ms    > : std::true_type {};
template<> struct detail::is_registered_coord_t<coords::origin::overmap         , coords::sm    > : std::true_type {};
template<> struct detail::is_registered_coord_t<coords::origin::overmap         , coords::omt   > : std::true_type {};
template<> struct detail::is_registered_coord_t<coords::origin::overmap         , coords::mmr   > : std::true_type {};
template<> struct detail::is_registered_coord_t<coords::origin::overmap         , coords::seg   > : std::true_type {};

template<> struct detail::has_remainder_origin_t< coords::sm    > : std::true_type {};
template<> struct detail::has_remainder_origin_t< coords::omt   > : std::true_type {};
template<> struct detail::has_remainder_origin_t< coords::mmr   > : std::true_type {};
template<> struct detail::has_remainder_origin_t< coords::seg   > : std::true_type {};
template<> struct detail::has_remainder_origin_t< coords::om    > : std::true_type {};
// *INDENT-ON*

template<coords::origin Origin, coords::scale Scale>
inline constexpr bool is_registered_coord_v = detail::is_registered_coord_t<Origin, Scale>::value;

template<coords::scale Scale>
inline constexpr bool has_remainder_origin_v = detail::has_remainder_origin_t<Scale>::value;

template<coords::scale SourceScale, coords::scale ResultScale>
inline constexpr bool exact_scale_conversion_v =
    coords::map_squares_per( SourceScale ) > coords::map_squares_per( ResultScale ) ?
    coords::map_squares_per( SourceScale ) % coords::map_squares_per( ResultScale ) == 0 :
    coords::map_squares_per( ResultScale ) % coords::map_squares_per( SourceScale ) == 0;

template<typename... Types>
struct type_list {};

template<typename TypeList>
struct as_variant;

template<typename... Types>
struct as_variant<type_list<Types...>> {
    using type = std::variant<Types...>;
};

template<typename TypeList>
using as_variant_t = typename as_variant<TypeList>::type;

using coord_types = type_list <
                    point_bub_sm, tripoint_bub_sm,
                    point_bub_ms, tripoint_bub_ms,
                    point_rel_ms, tripoint_rel_ms,
                    point_abs_ms, tripoint_abs_ms,
                    point_sm_ms, tripoint_sm_ms,
                    point_omt_ms, tripoint_omt_ms,
                    point_mmr_ms, tripoint_mmr_ms,
                    point_seg_ms, tripoint_seg_ms,
                    point_om_ms, tripoint_om_ms,
                    point_rel_veh, tripoint_rel_veh,
                    point_mnt_veh, tripoint_mnt_veh,
                    point_rel_sm, tripoint_rel_sm,
                    point_abs_sm, tripoint_abs_sm,
                    point_omt_sm, tripoint_omt_sm,
                    point_mmr_sm, tripoint_mmr_sm,
                    point_seg_sm, tripoint_seg_sm,
                    point_om_sm, tripoint_om_sm,
                    point_rel_omt, tripoint_rel_omt,
                    point_abs_omt, tripoint_abs_omt,
                    point_om_omt, tripoint_om_omt,
                    point_seg_omt, tripoint_seg_omt,
                    point_mmr_omt, tripoint_mmr_omt,
                    point_rel_mmr, tripoint_rel_mmr,
                    point_abs_mmr, tripoint_abs_mmr,
                    point_seg_mmr, tripoint_seg_mmr,
                    point_om_mmr, tripoint_om_mmr,
                    point_rel_seg, tripoint_rel_seg,
                    point_abs_seg, tripoint_abs_seg,
                    point_om_seg, tripoint_om_seg,
                    point_rel_om, tripoint_rel_om,
                    point_abs_om, tripoint_abs_om
                    >;

using coord_variant = as_variant_t<coord_types>;

template<typename TypeList>
struct with_raw_coord_types;

template<typename... Types>
struct with_raw_coord_types<type_list<Types...>> {
    using type = type_list<point, tripoint, Types...>;
};

using distance_coord_types = typename with_raw_coord_types<coord_types>::type;

template<typename Coord, coords::scale ResultScale>
inline constexpr bool can_project_to_v =
    Coord::scale_tag != ResultScale &&
    coords::map_squares_per( Coord::scale_tag ) != coords::map_squares_per( ResultScale ) &&
    exact_scale_conversion_v<Coord::scale_tag, ResultScale> &&
    is_registered_coord_v<Coord::origin_tag, ResultScale>;

template<typename Coord, coords::scale ResultScale>
inline constexpr bool can_project_remain_v =
    has_remainder_origin_v<ResultScale> &&
    coords::map_squares_per( ResultScale ) > coords::map_squares_per( Coord::scale_tag ) &&
    coords::map_squares_per( ResultScale ) % coords::map_squares_per( Coord::scale_tag ) == 0 &&
    is_registered_coord_v<Coord::origin_tag, ResultScale> &&
    is_registered_coord_v<coords::origin_from_scale( ResultScale ), Coord::scale_tag>;

template<typename Coord>
consteval auto can_project_combine_any() -> bool
{
    return has_remainder_origin_v<Coord::scale_tag>;
}

struct lua_coord_value {
    coords::origin origin;
    coords::scale scale;
    point xy;
    int z;
    bool is_tripoint;
};

auto has_remainder_origin( const coords::scale scale ) -> bool
{
    switch( scale ) {
        case coords::sm:
        case coords::omt:
        case coords::mmr:
        case coords::seg:
        case coords::om:
            return true;
        default:
            return false;
    }
}

auto is_registered_coord( const coords::origin, const coords::scale,
                          type_list<> ) -> bool
{
    return false;
}

template<typename Coord, typename... Rest>
auto is_registered_coord( const coords::origin origin, const coords::scale scale,
                          type_list<Coord, Rest...> ) -> bool
{
    if( Coord::origin_tag == origin && Coord::scale_tag == scale ) {
        return true;
    }
    return is_registered_coord( origin, scale, type_list<Rest...> {} );
}

auto lua_coord_from_object( const sol::object &, type_list<> ) -> std::optional<lua_coord_value>
{
    return std::nullopt;
}

template<typename Coord, typename... Rest>
auto lua_coord_from_object( const sol::object &obj,
                            type_list<Coord, Rest...> ) -> std::optional<lua_coord_value>
{
    if( obj.is<Coord>() ) {
        const auto coord = obj.as<Coord>();
        if constexpr( Coord::dimension == 3 ) {
            return lua_coord_value{ Coord::origin_tag, Coord::scale_tag, coord.raw().xy(),
                                    coord.z(), true };
        } else {
            return lua_coord_value{ Coord::origin_tag, Coord::scale_tag, coord.raw(), 0,
                                    false };
        }
    }
    return lua_coord_from_object( obj, type_list<Rest...> {} );
}

auto can_project_combine( const lua_coord_value &coarse, const lua_coord_value &fine ) -> bool
{
    if( !has_remainder_origin( coarse.scale ) ) {
        return false;
    }

    const auto coarse_scale = coords::map_squares_per( coarse.scale );
    const auto fine_scale = coords::map_squares_per( fine.scale );
    return coarse_scale > fine_scale &&
           coarse_scale % fine_scale == 0 &&
           coords::origin_from_scale( coarse.scale ) == fine.origin &&
           !( coarse.is_tripoint && fine.is_tripoint ) &&
           is_registered_coord( coarse.origin, fine.scale, coord_types {} );
}

auto make_coord_object( const coords::origin, const coords::scale, const bool,
                        const point &, const int, sol::this_state,
                        type_list<> ) -> sol::object
{
    return sol::nil;
}

template<typename Coord, typename... Rest>
auto make_coord_object( const coords::origin origin, const coords::scale scale,
                        const bool is_tripoint, const point &xy, const int z,
                        sol::this_state lua_state, type_list<Coord, Rest...> ) -> sol::object
{
    const auto dimension_matches = ( Coord::dimension == 3 ) == is_tripoint;
    if( Coord::origin_tag == origin && Coord::scale_tag == scale && dimension_matches ) {
        if constexpr( Coord::dimension == 3 ) {
            return sol::make_object( lua_state, Coord( tripoint( xy, z ) ) );
        } else {
            return sol::make_object( lua_state, Coord( xy ) );
        }
    }
    return make_coord_object( origin, scale, is_tripoint, xy, z, lua_state,
                              type_list<Rest...> {} );
}

template<typename Coord, coords::scale ResultScale>
auto lua_project_remain( const Coord &val,
                         sol::this_state L ) -> std::tuple<sol::object, sol::object>
{
    if constexpr( can_project_remain_v<Coord, ResultScale> ) {
        const auto r = coords::project_remain<ResultScale>( val );
        if constexpr( Coord::dimension == 3 ) {
            return std::make_tuple( sol::make_object( L, r.quotient_tripoint ), sol::make_object( L,
                                    r.remainder ) );
        } else {
            return std::make_tuple( sol::make_object( L, r.quotient ),  sol::make_object( L, r.remainder ) );
        }
    }
    debugmsg( "project_remain got an incompatible coordinate type / scale" );
    return std::make_tuple( sol::nil, sol::nil );
}

template<coords::scale ResultScale>
auto lua_project_remain( const sol::object &val,
                         sol::this_state L ) -> std::tuple<sol::object, sol::object>
{
    const auto vCoord = val.as<std::optional<coord_variant>>();
    if( !vCoord.has_value() ) {
        debugmsg( "project_remain expected a typed coordinate as its first argument" );
        return std::make_tuple( sol::nil, sol::nil );
    }

    const auto visitor = [&]<typename TCoord>( const TCoord & c ) ->
    std::tuple<sol::object, sol::object> {
        return lua_project_remain<TCoord, ResultScale>( c, L );
    };

    return std::visit( visitor, vCoord.value() );
}

auto lua_project_remain_to( const sol::object &val, const std::string &result_scale,
                            sol::this_state L ) -> std::tuple<sol::object, sol::object>
{
    if( result_scale == "sm" ) {
        return lua_project_remain<coords::sm>( val, L );
    }
    if( result_scale == "omt" ) {
        return lua_project_remain<coords::omt>( val, L );
    }
    if( result_scale == "mmr" ) {
        return lua_project_remain<coords::mmr>( val, L );
    }
    if( result_scale == "seg" ) {
        return lua_project_remain<coords::seg>( val, L );
    }
    if( result_scale == "om" ) {
        return lua_project_remain<coords::om>( val, L );
    }
    throw std::runtime_error( "project_remain expected scale sm, omt, mmr, seg, or om" );
}

auto lua_project_combine( const sol::object &coarse, const sol::object &fine,
                          sol::this_state lua_state ) -> sol::object
{
    const auto vCoarse = lua_coord_from_object( coarse, coord_types {} );
    if( !vCoarse.has_value() ) {
        debugmsg( "project_combine expected a typed coordinate as its first argument" );
        return sol::nil;
    }
    const auto vFine = lua_coord_from_object( fine, coord_types {} );
    if( !vFine.has_value() ) {
        debugmsg( "project_combine expected a typed coordinate as its second argument" );
        return sol::nil;
    }

    const auto &coarse_value = vCoarse.value();
    const auto &fine_value = vFine.value();
    if( !can_project_combine( coarse_value, fine_value ) ) {
        debugmsg( "cannot project_combine the selected coordinates" );
        return sol::nil;
    }

    const auto scale_up = coords::map_squares_per( coarse_value.scale ) /
                          coords::map_squares_per( fine_value.scale );
    const auto xy = multiply_xy( coarse_value.xy, scale_up ) + fine_value.xy;
    const auto is_tripoint = coarse_value.is_tripoint || fine_value.is_tripoint;
    const auto z = coarse_value.is_tripoint ? coarse_value.z : fine_value.z;

    return make_coord_object( coarse_value.origin, fine_value.scale, is_tripoint, xy, z,
                              lua_state, coord_types {} );
}

struct rl_dist_fn {
    template<typename Coord>
    auto operator()( const Coord &lhs, const Coord &rhs ) const -> int {
        return rl_dist( lhs, rhs );
    }
};

struct trig_dist_fn {
    template<typename Coord>
    auto operator()( const Coord &lhs, const Coord &rhs ) const -> float {
        return trig_dist( lhs, rhs );
    }
};

struct square_dist_fn {
    template<typename Coord>
    auto operator()( const Coord &lhs, const Coord &rhs ) const -> int {
        return square_dist( lhs, rhs );
    }
};

template<typename DistFn>
auto lua_distance_impl( const sol::object &, const sol::object &,
                        sol::this_state, type_list<> ) -> sol::object
{
    debugmsg( "distance functions need 2 matching point/coordinate types" );
    return sol::nil;
}

template<typename DistFn, typename Coord, typename... Rest>
auto lua_distance_impl( const sol::object &lhs, const sol::object &rhs,
                        sol::this_state L, type_list<Coord, Rest...> ) -> sol::object
{
    const auto lhs_is_coord = lhs.is<Coord>();
    const auto rhs_is_coord = rhs.is<Coord>();
    if( lhs_is_coord || rhs_is_coord ) {
        if( !lhs_is_coord || !rhs_is_coord ) {
            debugmsg( "distance functions need matching point/coordinate types" );
            return sol::nil;
        }
        const auto r = DistFn{}( lhs.as<Coord>(), rhs.as<Coord>() );
        return sol::make_object( L, r );
    }

    return lua_distance_impl<DistFn>( lhs, rhs, L, type_list<Rest...> {} );
}

template<typename DistFn>
auto lua_distance( const sol::object &lhs, const sol::object &rhs,
                   sol::this_state L ) -> sol::object
{
    return lua_distance_impl<DistFn>( lhs, rhs, L, distance_coord_types {} );
}

template<coords::scale ResultScale, typename Coord>
auto bind_project_to( sol::usertype<Coord> &ut, const char *name ) -> void
{
    if constexpr( can_project_to_v<Coord, ResultScale> ) {
        luna::set_fx( ut, name, []( const Coord & src ) {
            return coords::project_to<ResultScale>( src );
        } );
    }
}

template<coords::scale ResultScale, typename Coord>
auto bind_project_remain( sol::usertype<Coord> &ut, const char *name ) -> void
{
    if constexpr( can_project_remain_v<Coord, ResultScale> ) {
        luna::set_fx( ut, name, &lua_project_remain<Coord, ResultScale> );
    }
}

template<typename Coord>
auto bind_project_combine( sol::usertype<Coord> &ut ) -> void
{
    if constexpr( can_project_combine_any<Coord>() ) {
        luna::set_fx( ut, "project_combine", lua_project_combine );
    }
}

template<typename Coord>
auto bind_projection_methods( sol::usertype<Coord> &ut ) -> void
{
    bind_project_to<coords::ms>( ut, "to_ms" );
    bind_project_to<coords::veh>( ut, "to_veh" );
    bind_project_to<coords::sm>( ut, "to_sm" );
    bind_project_to<coords::omt>( ut, "to_omt" );
    bind_project_to<coords::mmr>( ut, "to_mmr" );
    bind_project_to<coords::seg>( ut, "to_seg" );
    bind_project_to<coords::om>( ut, "to_om" );

    bind_project_remain<coords::sm>( ut, "project_remain_sm" );
    bind_project_remain<coords::omt>( ut, "project_remain_omt" );
    bind_project_remain<coords::mmr>( ut, "project_remain_mmr" );
    bind_project_remain<coords::seg>( ut, "project_remain_seg" );
    bind_project_remain<coords::om>( ut, "project_remain_om" );

    bind_project_combine( ut );
}

} // namespace

// Register typed point coordinates.
template<coords::origin Origin, coords::scale Scale>
auto register_point( sol::state &lua ) -> void
{
    using Point = coords::coord_point<point, Origin, Scale>;
    using RelPoint = coords::coord_point<point, coords::origin::relative, Scale>;

    auto ut = luna::new_usertype<Point>(
                  lua,
                  luna::no_bases,
                  luna::constructors <
                  Point(),
                  Point( const point & ),
                  Point( int, int )
                  > ()
              );

    DOC( "Gets x" );
    luna::set_fx( ut, "x", []( const Point & pt ) -> int { return pt.x(); } );
    DOC( "Sets x" );
    luna::set_fx( ut, "set_x", []( Point & pt, int x ) { pt.x() = x; } );
    DOC( "Gets y" );
    luna::set_fx( ut, "y", []( const Point & pt ) -> int { return pt.y(); } );
    DOC( "Sets y" );
    luna::set_fx( ut, "set_y", []( Point & pt, int y ) { pt.y() = y; } );

    luna::set_fx( ut, "rotate", &Point::rotate );

    // To string
    // We're using Lua meta function here to make it work seamlessly with native Lua tostring()
    luna::set_fx( ut, sol::meta_function::to_string, &Point::to_string );

    // Equality operator
    // It's defined as inline friend function inside Point class, we can't access it and so have to improvise
    luna::set_fx( ut, sol::meta_function::equal_to, []( const Point & a, const Point & b ) { return a == b; } );

    // Less-then operator
    // Same deal as with equality operator
    luna::set_fx( ut, sol::meta_function::less_than, []( const Point & a, const Point & b ) { return a < b; } );

    // Arithmetic operators
    // Point + RelPoint
    DOC( "Adds point and relative point" );
    // Point + point
    DOC( "Adds point and raw point" );
    luna::set_fx( ut, sol::meta_function::addition, sol::overload(
    []( const Point & a, const RelPoint & b ) { return a + b; },
    []( const Point & a, const point & b ) { return a + b; }
                  ) );
    // Point - RelPoint
    DOC( "Subtracts point and relative point" );
    // Point - point
    DOC( "Subtracts point and raw point" );
    luna::set_fx( ut, sol::meta_function::subtraction, sol::overload(
    []( const Point & a, const RelPoint & b ) { return a - b; },
    []( const Point & a, const point & b ) { return a - b; }
                  ) );

    reg_serde_functions( ut );

    if constexpr( Origin == coords::origin::relative ) {
        // Point * int
        luna::set_fx( ut, sol::meta_function::multiplication, []( const Point & a, const int &b ) { return a * b; } );
    }
    bind_projection_methods( ut );
    luna::set_fx( ut, "rl_dist", []( const Point & a, const Point & b ) {
        return rl_dist( a, b );
    } );
    luna::set_fx( ut, "trig_dist", []( const Point & a, const Point & b ) {
        return trig_dist( a, b );
    } );
    luna::set_fx( ut, "square_dist", []( const Point & a, const Point & b ) {
        return square_dist( a, b );
    } );
}

template<coords::origin Origin, coords::scale Scale>
auto register_tripoint( sol::state &lua ) -> void
{
    using Point = coords::coord_point<tripoint, Origin, Scale>;
    using Point2d = coords::coord_point<point, Origin, Scale>;
    using RelPoint = coords::coord_point<tripoint, coords::origin::relative, Scale>;

    auto ut = luna::new_usertype<Point>(
                  lua,
                  luna::no_bases,
                  luna::constructors <
                  Point(),
                  Point( const tripoint & ),
                  Point( const Point2d &, int ),
                  Point( int, int, int )
                  > ()
              );

    DOC( "Gets x" );
    luna::set_fx( ut, "x", []( const Point & pt ) -> int { return pt.x(); } );
    DOC( "Sets x" );
    luna::set_fx( ut, "set_x", []( Point & pt, int x ) { pt.x() = x; } );
    DOC( "Gets y" );
    luna::set_fx( ut, "y", []( const Point & pt ) -> int { return pt.y(); } );
    DOC( "Sets y" );
    luna::set_fx( ut, "set_y", []( Point & pt, int y ) { pt.y() = y; } );
    DOC( "Gets z" );
    luna::set_fx( ut, "z", []( const Point & pt ) -> int { return pt.z(); } );
    DOC( "Sets z" );
    luna::set_fx( ut, "set_z", []( Point & pt, int z ) { pt.z() = z; } );

    luna::set_fx( ut, "xy", &Point::xy );

    // To string
    // We're using Lua meta function here to make it work seamlessly with native Lua tostring()
    luna::set_fx( ut, sol::meta_function::to_string, &Point::to_string );

    // Equality operator
    // It's defined as inline friend function inside Point class, we can't access it and so have to improvise
    luna::set_fx( ut, sol::meta_function::equal_to, []( const Point & a, const Point & b ) { return a == b; } );

    // Less-then operator
    // Same deal as with equality operator
    luna::set_fx( ut, sol::meta_function::less_than, []( const Point & a, const Point & b ) { return a < b; } );

    // Arithmetic operators
    // Point + RelPoint
    DOC( "Adds point and relative point" );
    // Point + point
    DOC( "Adds point and raw point" );
    // Point + tripoint
    DOC( "Adds point and raw tripoint" );
    luna::set_fx( ut, sol::meta_function::addition, sol::overload(
    []( const Point & a, const RelPoint & b ) { return a + b; },
    []( const Point & a, const point & b ) { return a + b; },
    []( const Point & a, const tripoint & b ) { return a + b; }
                  ) );
    // Point - RelPoint
    DOC( "Subtracts point and relative point" );
    // Point - point
    DOC( "Subtracts point and raw point" );
    // Point - tripoint
    DOC( "Subtracts point and raw point" );
    luna::set_fx( ut, sol::meta_function::subtraction, sol::overload(
    []( const Point & a, const RelPoint & b ) { return a - b; },
    []( const Point & a, const point & b ) { return a - b; },
    []( const Point & a, const tripoint & b ) { return a - b; }
                  ) );

    reg_serde_functions( ut );

    if constexpr( Origin == coords::origin::relative ) {
        // Point * int
        luna::set_fx( ut, sol::meta_function::multiplication, []( const Point & a, const int &b ) { return a * b; } );
    }
    bind_projection_methods( ut );
    luna::set_fx( ut, "rl_dist", []( const Point & a, const Point & b ) {
        return rl_dist( a, b );
    } );
    luna::set_fx( ut, "trig_dist", []( const Point & a, const Point & b ) {
        return trig_dist( a, b );
    } );
    luna::set_fx( ut, "square_dist", []( const Point & a, const Point & b ) {
        return square_dist( a, b );
    } );
}

template<coords::origin Origin, coords::scale Scale>
auto register_coord_pair( sol::state &lua ) -> void
{
    register_point<Origin, Scale>( lua );
    register_tripoint<Origin, Scale>( lua );
}

template<typename Coord>
auto register_coord_type( sol::state &lua ) -> void
{
    if constexpr( Coord::dimension == 3 ) {
        register_tripoint<Coord::origin_tag, Coord::scale_tag>( lua );
    } else {
        register_point<Coord::origin_tag, Coord::scale_tag>( lua );
    }
}

auto cata::detail::reg_point_tripoint( sol::state &lua ) -> void
{
    constexpr auto sz = std::variant_size_v<coord_variant>;
    constexpr auto seq = std::make_index_sequence<sz> {};

    const auto fn = [&]<std::size_t... Is >( std::index_sequence<Is...> ) {
        return ( register_coord_type<std::variant_alternative_t<Is, coord_variant>>( lua ), ... );
    };

    fn( seq );

    // Register 'point' class to be used in Lua
    {
        auto ut = luna::new_usertype<point>(
                      lua,
                      luna::no_bases,
                      luna::constructors <
                      point(),
                      point( const point & ),
                      point( int, int )
                      > ()
                  );

        // Members
        luna::set( ut, "x", &point::x );
        luna::set( ut, "y", &point::y );

        // Methods
        luna::set_fx( ut, "abs", &point::abs );
        luna::set_fx( ut, "rotate", &point::rotate );

        // (De-)Serialization
        reg_serde_functions( ut );

        // To string
        // We're using Lua meta function here to make it work seamlessly with native Lua tostring()
        luna::set_fx( ut, sol::meta_function::to_string, &point::to_string );

        // Equality operator
        // It's defined as inline friend function inside point class, we can't access it and so have to improvise
        luna::set_fx( ut, sol::meta_function::equal_to, []( const point & a, const point & b ) { return a == b; } );

        // Less-then operator
        // Same deal as with equality operator
        luna::set_fx( ut, sol::meta_function::less_than, []( const point & a, const point & b ) { return a < b; } );

        // Arithmetic operators
        // point + point
        luna::set_fx( ut, sol::meta_function::addition, &point::operator+ );
        // point - point
        // sol::resolve here makes it possible to specify which overload to use
        luna::set_fx( ut, sol::meta_function::subtraction, sol::resolve< point( point ) const >
                      ( &point::operator- ) );
        // point * int
        luna::set_fx( ut, sol::meta_function::multiplication, &point::operator* );
        // point / float
        luna::set_fx( ut, sol::meta_function::division, &point::operator/ );
        // point / int
        luna::set_fx( ut, sol::meta_function::floor_division, &point::operator/ );
        // -point
        // sol::resolve here makes it possible to specify which overload to use
        luna::set_fx( ut, sol::meta_function::unary_minus,
                      sol::resolve< point() const >( &point::operator- ) );
    }

    // Register 'tripoint' class to be used in Lua
    {
        auto ut = luna::new_usertype<tripoint>(
                      lua,
                      luna::no_bases,
                      luna::constructors <
                      tripoint(),
                      tripoint( const point &, int ),
                      tripoint( const tripoint & ),
                      tripoint( int, int, int )
                      > ()
                  );

        // Members
        luna::set( ut, "x", &tripoint::x );
        luna::set( ut, "y", &tripoint::y );
        luna::set( ut, "z", &tripoint::z );

        // Methods
        luna::set_fx( ut, "abs", &tripoint::abs );
        luna::set_fx( ut, "xy", &tripoint::xy );
        luna::set_fx( ut, "rotate_2d", &tripoint::rotate_2d );

        // (De-)Serialization
        reg_serde_functions( ut );

        // To string
        // We're using Lua meta function here to make it work seamlessly with native Lua tostring()
        luna::set_fx( ut, sol::meta_function::to_string, &tripoint::to_string );

        // Equality operator
        // It's defined as inline friend function inside point class, we can't access it and so have to improvise
        luna::set_fx( ut, sol::meta_function::equal_to, []( const tripoint & a, const tripoint & b ) { return a == b; } );

        // Less-then operator
        // Same deal as with equality operator
        luna::set_fx( ut, sol::meta_function::less_than, []( const tripoint & a, const tripoint & b ) { return a < b; } );

        // Arithmetic operators
        // tripoint + tripoint (overload 1)
        // tripoint + point (overload 2)
        luna::set_fx( ut, sol::meta_function::addition, sol::overload(
                          sol::resolve< tripoint( const tripoint & ) const > ( &tripoint::operator+ ),
                          sol::resolve< tripoint( point ) const > ( &tripoint::operator+ )
                      ) );
        // tripoint - tripoint (overload 1)
        // tripoint - point (overload 2)
        luna::set_fx( ut, sol::meta_function::subtraction, sol::overload(
                          sol::resolve< tripoint( const tripoint & ) const > ( &tripoint::operator- ),
                          sol::resolve< tripoint( point ) const > ( &tripoint::operator- )
                      ) );
        // tripoint * int
        luna::set_fx( ut, sol::meta_function::multiplication, &tripoint::operator* );
        // tripoint / float
        luna::set_fx( ut, sol::meta_function::division, &tripoint::operator/ );
        // tripoint / int
        luna::set_fx( ut, sol::meta_function::floor_division, &tripoint::operator/ );
        // -tripoint
        // sol::resolve here makes it possible to specify which overload to use
        luna::set_fx( ut, sol::meta_function::unary_minus,
                      sol::resolve< tripoint() const >( &tripoint::operator- ) );
    }
}

auto cata::detail::reg_coords_library( sol::state &lua ) -> void
{
    DOC( "Methods for manipulating raw and typed points and calculating distance" );
    auto lib = luna::begin_lib( lua, "coords" );

    luna::set_fx( lib, "rl_dist", &lua_distance<rl_dist_fn> );
    luna::set_fx( lib, "trig_dist", &lua_distance<trig_dist_fn> );
    luna::set_fx( lib, "square_dist", &lua_distance<square_dist_fn> );

    DOC( "Split a typed coordinate into a coarser coordinate and a remainder. Scale must be sm, omt, mmr, seg, or om." );
    luna::set_fx( lib, "project_remain", &lua_project_remain_to );
    luna::set_fx( lib, "project_remain_sm", &lua_project_remain<coords::sm> );
    luna::set_fx( lib, "project_remain_omt", &lua_project_remain<coords::omt> );
    luna::set_fx( lib, "project_remain_mmr", &lua_project_remain<coords::mmr> );
    luna::set_fx( lib, "project_remain_seg", &lua_project_remain<coords::seg> );
    luna::set_fx( lib, "project_remain_om", &lua_project_remain<coords::om> );

    DOC( "Combine a typed coarse coordinate with a compatible typed remainder coordinate." );
    luna::set_fx( lib, "project_combine", &lua_project_combine );

    luna::finalize_lib( lib );
}
