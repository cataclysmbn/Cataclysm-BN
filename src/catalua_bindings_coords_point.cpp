#include "catalua_bindings_coords_common.h"

#include <algorithm>
#include <array>

namespace cata::detail::lua_coords
{

namespace
{

struct point_coord_axis {
    const char *name;
    int point::*member;
};

auto bind_point_axis_properties( sol::usertype<lua_point_coord> &ut ) -> void
{
    const auto axes = std::array{
        point_coord_axis{ "x", &point::x },
        point_coord_axis{ "y", &point::y },
    };

    std::ranges::for_each( axes, [&ut]( const point_coord_axis & axis ) {
        luna::set_prop( ut, axis.name,
        [member = axis.member]( const lua_point_coord & coord ) { return coord.raw.*member; },
        [member = axis.member]( lua_point_coord & coord, const int value ) { coord.raw.*member = value; }
                      );
    } );
}

} // namespace

auto lua_point_add( const lua_point_coord &lhs,
                    const sol::object &rhs ) -> std::optional<lua_point_coord>
{
    if( rhs.is<lua_point_coord>() ) {
        const auto other = rhs.as<lua_point_coord>();
        if( lhs.scale == other.scale && other.origin == coords::origin::relative ) {
            return make_point_coord( lhs.origin, lhs.scale, lhs.raw + other.raw );
        }
        if( lhs.scale == other.scale && lhs.origin == coords::origin::relative ) {
            return make_point_coord( other.origin, other.scale, lhs.raw + other.raw );
        }
    }
    debugmsg( "PointCoord addition expects a relative PointCoord at the same scale" );
    return std::nullopt;
}

auto lua_point_subtract( const lua_point_coord &lhs,
                         const sol::object &rhs ) -> std::optional<lua_point_coord>
{
    if( rhs.is<lua_point_coord>() ) {
        const auto other = rhs.as<lua_point_coord>();
        if( lhs.scale == other.scale && other.origin == coords::origin::relative ) {
            return make_point_coord( lhs.origin, lhs.scale, lhs.raw - other.raw );
        }
        if( same_coord_kind( lhs, other ) && lhs.origin != coords::origin::relative ) {
            return make_point_coord( coords::origin::relative, lhs.scale, lhs.raw - other.raw );
        }
    }
    debugmsg( "PointCoord subtraction expects a relative PointCoord at the same scale, or a matching PointCoord" );
    return std::nullopt;
}

auto lua_point_multiply( const lua_point_coord &lhs,
                         const int rhs ) -> std::optional<lua_point_coord>
{
    if( lhs.origin == coords::origin::relative ) {
        return make_point_coord( lhs.origin, lhs.scale, lhs.raw * rhs );
    }
    debugmsg( "PointCoord multiplication is only valid for relative coordinates" );
    return std::nullopt;
}

auto lua_point_rotate( const lua_point_coord &coord, const int turns,
                       const point &dim ) -> lua_point_coord
{
    return make_point_coord( coord.origin, coord.scale, coord.raw.rotate( turns, dim ) );
}

auto lua_point_reinterpret_as( const lua_point_coord &coord, const std::string &origin,
                               const std::string &scale ) -> lua_point_coord
{
    return make_point_coord( origin, scale, coord.raw );
}

auto raw_point_reinterpret_as( const point &raw, const std::string &origin,
                               const std::string &scale ) -> lua_point_coord
{
    return make_point_coord( origin, scale, raw );
}

auto lua_point_less_than( const lua_point_coord &lhs, const lua_point_coord &rhs ) -> bool
{
    return std::tie( lhs.origin, lhs.scale, lhs.raw ) < std::tie( rhs.origin, rhs.scale, rhs.raw );
}

auto bind_point_projection_methods( sol::usertype<lua_point_coord> &ut ) -> void
{
    DOC( "Projects this coordinate to another scale, preserving its origin and point dimension. Returns nil if the conversion is not valid." );
    DOC_PARAMS( "scale" );
    luna::set_fx( ut, "to", &lua_project_point_to );
    DOC( "Shortcut for `to(\"ms\")`." );
    luna::set_fx( ut, "to_ms", []( const lua_point_coord & coord ) {
        return lua_project_point_to( coord, "ms" );
    } );
    DOC( "Shortcut for `to(\"veh\")`." );
    luna::set_fx( ut, "to_veh", []( const lua_point_coord & coord ) {
        return lua_project_point_to( coord, "veh" );
    } );
    DOC( "Shortcut for `to(\"sm\")`." );
    luna::set_fx( ut, "to_sm", []( const lua_point_coord & coord ) {
        return lua_project_point_to( coord, "sm" );
    } );
    DOC( "Shortcut for `to(\"omt\")`." );
    luna::set_fx( ut, "to_omt", []( const lua_point_coord & coord ) {
        return lua_project_point_to( coord, "omt" );
    } );
    DOC( "Shortcut for `to(\"mmr\")`." );
    luna::set_fx( ut, "to_mmr", []( const lua_point_coord & coord ) {
        return lua_project_point_to( coord, "mmr" );
    } );
    DOC( "Shortcut for `to(\"seg\")`." );
    luna::set_fx( ut, "to_seg", []( const lua_point_coord & coord ) {
        return lua_project_point_to( coord, "seg" );
    } );
    DOC( "Shortcut for `to(\"om\")`." );
    luna::set_fx( ut, "to_om", []( const lua_point_coord & coord ) {
        return lua_project_point_to( coord, "om" );
    } );

    DOC( "Splits this point into a coarse PointCoord at the requested scale and a PointCoord remainder at this point's original scale. Returns nil, nil if the split is not valid." );
    DOC_PARAMS( "scale" );
    luna::set_fx( ut, "project_remain", &lua_project_point_remain_to );
    DOC( "Shortcut for `project_remain(\"sm\")`." );
    luna::set_fx( ut, "project_remain_sm", []( const lua_point_coord & coord ) {
        return lua_project_point_remain_to( coord, "sm" );
    } );
    DOC( "Shortcut for `project_remain(\"omt\")`." );
    luna::set_fx( ut, "project_remain_omt", []( const lua_point_coord & coord ) {
        return lua_project_point_remain_to( coord, "omt" );
    } );
    DOC( "Shortcut for `project_remain(\"mmr\")`." );
    luna::set_fx( ut, "project_remain_mmr", []( const lua_point_coord & coord ) {
        return lua_project_point_remain_to( coord, "mmr" );
    } );
    DOC( "Shortcut for `project_remain(\"seg\")`." );
    luna::set_fx( ut, "project_remain_seg", []( const lua_point_coord & coord ) {
        return lua_project_point_remain_to( coord, "seg" );
    } );
    DOC( "Shortcut for `project_remain(\"om\")`." );
    luna::set_fx( ut, "project_remain_om", []( const lua_point_coord & coord ) {
        return lua_project_point_remain_to( coord, "om" );
    } );
    DOC( "Combines this coarse point with a remainder from project_remain. Returns PointCoord or TripointCoord depending on the inputs, or nil if the pair is incompatible." );
    DOC_PARAMS( "fine" );
    luna::set_fx( ut, "project_combine",
                  sol::resolve<lua_coord_result( const lua_point_coord &, const sol::object & )>
                  ( &lua_project_combine ) );
}

auto reg_lua_point_coord( sol::state &lua ) -> void
{
    DOC( "A typed two-dimensional coordinate. Its origin and scale describe how to interpret x and y, and coordinate operations only combine compatible coordinate spaces." );
    auto ut = luna::new_usertype<lua_point_coord>(
                  lua,
                  luna::no_bases,
                  luna::no_constructor
              );

    bind_point_axis_properties( ut );

    luna::set_fx( ut, "origin", []( const lua_point_coord & pt ) { return origin_lua_name( pt.origin ); } );
    luna::set_fx( ut, "scale", []( const lua_point_coord & pt ) { return scale_lua_name( pt.scale ); } );
    luna::set_fx( ut, "type", []( const lua_point_coord & pt ) {
        return coord_type_name( false, pt.origin, pt.scale );
    } );
    luna::set_fx( ut, "raw", []( const lua_point_coord & pt ) { return pt.raw; } );
    luna::set_fx( ut, "rotate", &lua_point_rotate );
    DOC( "Reinterpret this coordinate as another point coordinate kind without changing x or y." );
    DOC_PARAMS( "origin", "scale" );
    luna::set_fx( ut, "reinterpret_as", &lua_point_reinterpret_as );

    bind_point_projection_methods( ut );

    DOC( "Rectilinear distance to another PointCoord with matching origin and scale. Returns nil if the argument is incompatible." );
    DOC_PARAMS( "other" );
    luna::set_fx( ut, "rl_dist", []( const lua_point_coord & lhs,
    const lua_point_coord & rhs ) {
        return lua_point_coord_rl_dist( lhs, rhs );
    } );
    DOC( "Euclidean distance to another PointCoord with matching origin and scale. Returns nil if the argument is incompatible." );
    DOC_PARAMS( "other" );
    luna::set_fx( ut, "trig_dist", []( const lua_point_coord & lhs,
    const lua_point_coord & rhs ) {
        return lua_point_coord_trig_dist( lhs, rhs );
    } );
    DOC( "Chebyshev distance to another PointCoord with matching origin and scale. Returns nil if the argument is incompatible." );
    DOC_PARAMS( "other" );
    luna::set_fx( ut, "square_dist", []( const lua_point_coord & lhs,
    const lua_point_coord & rhs ) {
        return lua_point_coord_square_dist( lhs, rhs );
    } );

    luna::set_fx( ut, sol::meta_function::to_string, &point_to_string );
    luna::set_fx( ut, sol::meta_function::equal_to, []( const lua_point_coord & lhs,
    const lua_point_coord & rhs ) -> bool {
        return same_coord_kind( lhs, rhs ) && lhs.raw == rhs.raw;
    } );
    luna::set_fx( ut, sol::meta_function::less_than, &lua_point_less_than );
    luna::set_fx( ut, sol::meta_function::addition, &lua_point_add );
    luna::set_fx( ut, sol::meta_function::subtraction, &lua_point_subtract );
    luna::set_fx( ut, sol::meta_function::multiplication, &lua_point_multiply );
}

auto reg_raw_point( sol::state &lua ) -> void
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

    luna::set( ut, "x", &point::x );
    luna::set( ut, "y", &point::y );

    luna::set_fx( ut, "abs", &point::abs );
    luna::set_fx( ut, "rotate", &point::rotate );
    DOC( "Reinterpret this raw point as a typed point coordinate without changing x or y." );
    DOC_PARAMS( "origin", "scale" );
    luna::set_fx( ut, "reinterpret_as", &raw_point_reinterpret_as );

    reg_serde_functions( ut );

    luna::set_fx( ut, sol::meta_function::to_string, &point::to_string );
    luna::set_fx( ut, sol::meta_function::equal_to, []( const point & a, const point & b ) -> bool { return a == b; } );
    luna::set_fx( ut, sol::meta_function::less_than, []( const point & a, const point & b ) -> bool { return a < b; } );

    luna::set_fx( ut, sol::meta_function::addition, &point::operator+ );
    luna::set_fx( ut, sol::meta_function::subtraction, sol::resolve< point( point ) const >
                  ( &point::operator- ) );
    luna::set_fx( ut, sol::meta_function::multiplication, &point::operator* );
    luna::set_fx( ut, sol::meta_function::division, &point::operator/ );
    luna::set_fx( ut, sol::meta_function::floor_division, &point::operator/ );
    luna::set_fx( ut, sol::meta_function::unary_minus,
                  sol::resolve< point() const >( &point::operator- ) );
}

} // namespace cata::detail::lua_coords
