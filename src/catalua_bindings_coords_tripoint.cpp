#include "catalua_bindings_coords_common.h"

#include <algorithm>
#include <array>

namespace cata::detail::lua_coords
{

namespace
{

struct tripoint_coord_axis {
    const char *name;
    int tripoint::*member;
};

auto bind_tripoint_axis_properties( sol::usertype<lua_tripoint_coord> &ut ) -> void
{
    const auto axes = std::array{
        tripoint_coord_axis{ "x", &tripoint::x },
        tripoint_coord_axis{ "y", &tripoint::y },
        tripoint_coord_axis{ "z", &tripoint::z },
    };

    std::ranges::for_each( axes, [&ut]( const tripoint_coord_axis & axis ) {
        luna::set_prop( ut, axis.name,
        [member = axis.member]( const lua_tripoint_coord & coord ) { return coord.raw.*member; },
        [member = axis.member]( lua_tripoint_coord & coord, const int value ) { coord.raw.*member = value; }
                      );
    } );
}

} // namespace

auto lua_tripoint_add( const lua_tripoint_coord &lhs,
                       const sol::object &rhs ) -> std::optional<lua_tripoint_coord>
{
    if( rhs.is<lua_point_coord>() ) {
        const auto other = rhs.as<lua_point_coord>();
        if( lhs.scale == other.scale && other.origin == coords::origin::relative ) {
            return make_tripoint_coord( lhs.origin, lhs.scale, lhs.raw + other.raw );
        }
    }
    if( rhs.is<lua_tripoint_coord>() ) {
        const auto other = rhs.as<lua_tripoint_coord>();
        if( lhs.scale == other.scale && other.origin == coords::origin::relative ) {
            return make_tripoint_coord( lhs.origin, lhs.scale, lhs.raw + other.raw );
        }
        if( lhs.scale == other.scale && lhs.origin == coords::origin::relative ) {
            return make_tripoint_coord( other.origin, other.scale, lhs.raw + other.raw );
        }
    }
    debugmsg( "TripointCoord addition expects a relative coordinate at the same scale" );
    return std::nullopt;
}

auto lua_tripoint_subtract( const lua_tripoint_coord &lhs,
                            const sol::object &rhs ) -> std::optional<lua_tripoint_coord>
{
    if( rhs.is<lua_point_coord>() ) {
        const auto other = rhs.as<lua_point_coord>();
        if( lhs.scale == other.scale && other.origin == coords::origin::relative ) {
            return make_tripoint_coord( lhs.origin, lhs.scale, lhs.raw - other.raw );
        }
    }
    if( rhs.is<lua_tripoint_coord>() ) {
        const auto other = rhs.as<lua_tripoint_coord>();
        if( lhs.scale == other.scale && other.origin == coords::origin::relative ) {
            return make_tripoint_coord( lhs.origin, lhs.scale, lhs.raw - other.raw );
        }
        if( same_coord_kind( lhs, other ) && lhs.origin != coords::origin::relative ) {
            return make_tripoint_coord( coords::origin::relative, lhs.scale, lhs.raw - other.raw );
        }
    }
    debugmsg( "TripointCoord subtraction expects a relative coordinate at the same scale, or a matching TripointCoord" );
    return std::nullopt;
}

auto lua_tripoint_multiply( const lua_tripoint_coord &lhs,
                            const int rhs ) -> std::optional<lua_tripoint_coord>
{
    if( lhs.origin == coords::origin::relative ) {
        return make_tripoint_coord( lhs.origin, lhs.scale, lhs.raw * rhs );
    }
    debugmsg( "TripointCoord multiplication is only valid for relative coordinates" );
    return std::nullopt;
}

auto lua_tripoint_rotate( const lua_tripoint_coord &coord, const int turns,
                          const point &dim ) -> lua_tripoint_coord
{
    return make_tripoint_coord( coord.origin, coord.scale, coord.raw.rotate_2d( turns, dim ) );
}

auto lua_tripoint_reinterpret_as( const lua_tripoint_coord &coord, const std::string &origin,
                                  const std::string &scale ) -> lua_tripoint_coord
{
    return make_tripoint_coord( origin, scale, coord.raw );
}

auto raw_tripoint_reinterpret_as( const tripoint &raw, const std::string &origin,
                                  const std::string &scale ) -> lua_tripoint_coord
{
    return make_tripoint_coord( origin, scale, raw );
}

auto lua_tripoint_less_than( const lua_tripoint_coord &lhs,
                             const lua_tripoint_coord &rhs ) -> bool
{
    return std::tie( lhs.origin, lhs.scale, lhs.raw ) < std::tie( rhs.origin, rhs.scale, rhs.raw );
}

auto bind_tripoint_projection_methods( sol::usertype<lua_tripoint_coord> &ut ) -> void
{
    DOC( "Projects this coordinate to another scale, preserving its origin and tripoint dimension. Returns nil if the conversion is not valid." );
    DOC_PARAMS( "scale" );
    luna::set_fx( ut, "to", &lua_project_tripoint_to );
    DOC( "Shortcut for `to(\"ms\")`." );
    luna::set_fx( ut, "to_ms", []( const lua_tripoint_coord & coord ) {
        return lua_project_tripoint_to( coord, "ms" );
    } );
    DOC( "Shortcut for `to(\"veh\")`." );
    luna::set_fx( ut, "to_veh", []( const lua_tripoint_coord & coord ) {
        return lua_project_tripoint_to( coord, "veh" );
    } );
    DOC( "Shortcut for `to(\"sm\")`." );
    luna::set_fx( ut, "to_sm", []( const lua_tripoint_coord & coord ) {
        return lua_project_tripoint_to( coord, "sm" );
    } );
    DOC( "Shortcut for `to(\"omt\")`." );
    luna::set_fx( ut, "to_omt", []( const lua_tripoint_coord & coord ) {
        return lua_project_tripoint_to( coord, "omt" );
    } );
    DOC( "Shortcut for `to(\"mmr\")`." );
    luna::set_fx( ut, "to_mmr", []( const lua_tripoint_coord & coord ) {
        return lua_project_tripoint_to( coord, "mmr" );
    } );
    DOC( "Shortcut for `to(\"seg\")`." );
    luna::set_fx( ut, "to_seg", []( const lua_tripoint_coord & coord ) {
        return lua_project_tripoint_to( coord, "seg" );
    } );
    DOC( "Shortcut for `to(\"om\")`." );
    luna::set_fx( ut, "to_om", []( const lua_tripoint_coord & coord ) {
        return lua_project_tripoint_to( coord, "om" );
    } );

    DOC( "Splits this tripoint into a coarse TripointCoord at the requested scale and a PointCoord remainder at this tripoint's original scale. The z value stays on the coarse tripoint. Returns nil, nil if the split is not valid." );
    DOC_PARAMS( "scale" );
    luna::set_fx( ut, "project_remain", &lua_project_tripoint_remain_to );
    DOC( "Shortcut for `project_remain(\"sm\")`." );
    luna::set_fx( ut, "project_remain_sm", []( const lua_tripoint_coord & coord ) {
        return lua_project_tripoint_remain_to( coord, "sm" );
    } );
    DOC( "Shortcut for `project_remain(\"omt\")`." );
    luna::set_fx( ut, "project_remain_omt", []( const lua_tripoint_coord & coord ) {
        return lua_project_tripoint_remain_to( coord, "omt" );
    } );
    DOC( "Shortcut for `project_remain(\"mmr\")`." );
    luna::set_fx( ut, "project_remain_mmr", []( const lua_tripoint_coord & coord ) {
        return lua_project_tripoint_remain_to( coord, "mmr" );
    } );
    DOC( "Shortcut for `project_remain(\"seg\")`." );
    luna::set_fx( ut, "project_remain_seg", []( const lua_tripoint_coord & coord ) {
        return lua_project_tripoint_remain_to( coord, "seg" );
    } );
    DOC( "Shortcut for `project_remain(\"om\")`." );
    luna::set_fx( ut, "project_remain_om", []( const lua_tripoint_coord & coord ) {
        return lua_project_tripoint_remain_to( coord, "om" );
    } );
    DOC( "Combines this coarse tripoint with a remainder from project_remain. Returns TripointCoord, or nil if the pair is incompatible." );
    DOC_PARAMS( "fine" );
    luna::set_fx( ut, "project_combine",
                  sol::resolve<lua_coord_result( const lua_tripoint_coord &, const sol::object & )>
                  ( &lua_project_combine ) );
}

auto reg_lua_tripoint_coord( sol::state &lua ) -> void
{
    DOC( "A typed three-dimensional coordinate. Its origin and scale describe how to interpret x, y, and z, and coordinate operations only combine compatible coordinate spaces." );
    auto ut = luna::new_usertype<lua_tripoint_coord>(
                  lua,
                  luna::no_bases,
                  luna::no_constructor
              );

    bind_tripoint_axis_properties( ut );

    luna::set_fx( ut, "xy", []( const lua_tripoint_coord & pt ) {
        return make_point_coord( pt.origin, pt.scale, pt.raw.xy() );
    } );
    luna::set_fx( ut, "origin", []( const lua_tripoint_coord & pt ) { return origin_lua_name( pt.origin ); } );
    luna::set_fx( ut, "scale", []( const lua_tripoint_coord & pt ) { return scale_lua_name( pt.scale ); } );
    luna::set_fx( ut, "type", []( const lua_tripoint_coord & pt ) {
        return coord_type_name( true, pt.origin, pt.scale );
    } );
    luna::set_fx( ut, "raw", []( const lua_tripoint_coord & pt ) { return pt.raw; } );
    luna::set_fx( ut, "rotate_2d", &lua_tripoint_rotate );
    DOC( "Reinterpret this coordinate as another tripoint coordinate kind without changing x, y, or z." );
    DOC_PARAMS( "origin", "scale" );
    luna::set_fx( ut, "reinterpret_as", &lua_tripoint_reinterpret_as );

    bind_tripoint_projection_methods( ut );

    DOC( "Rectilinear distance to another TripointCoord with matching origin and scale. Returns nil if the argument is incompatible." );
    DOC_PARAMS( "other" );
    luna::set_fx( ut, "rl_dist", []( const lua_tripoint_coord & lhs,
    const lua_tripoint_coord & rhs ) {
        return lua_tripoint_coord_rl_dist( lhs, rhs );
    } );
    DOC( "Euclidean distance to another TripointCoord with matching origin and scale. Returns nil if the argument is incompatible." );
    DOC_PARAMS( "other" );
    luna::set_fx( ut, "trig_dist", []( const lua_tripoint_coord & lhs,
    const lua_tripoint_coord & rhs ) {
        return lua_tripoint_coord_trig_dist( lhs, rhs );
    } );
    DOC( "Chebyshev distance to another TripointCoord with matching origin and scale. Returns nil if the argument is incompatible." );
    DOC_PARAMS( "other" );
    luna::set_fx( ut, "square_dist", []( const lua_tripoint_coord & lhs,
    const lua_tripoint_coord & rhs ) {
        return lua_tripoint_coord_square_dist( lhs, rhs );
    } );

    luna::set_fx( ut, sol::meta_function::to_string, &tripoint_to_string );
    luna::set_fx( ut, sol::meta_function::equal_to, []( const lua_tripoint_coord & lhs,
    const lua_tripoint_coord & rhs ) -> bool {
        return same_coord_kind( lhs, rhs ) && lhs.raw == rhs.raw;
    } );
    luna::set_fx( ut, sol::meta_function::less_than, &lua_tripoint_less_than );
    luna::set_fx( ut, sol::meta_function::addition, &lua_tripoint_add );
    luna::set_fx( ut, sol::meta_function::subtraction, &lua_tripoint_subtract );
    luna::set_fx( ut, sol::meta_function::multiplication, &lua_tripoint_multiply );
}

auto reg_raw_tripoint( sol::state &lua ) -> void
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

    luna::set( ut, "x", &tripoint::x );
    luna::set( ut, "y", &tripoint::y );
    luna::set( ut, "z", &tripoint::z );

    luna::set_fx( ut, "abs", &tripoint::abs );
    luna::set_fx( ut, "xy", &tripoint::xy );
    luna::set_fx( ut, "rotate_2d", &tripoint::rotate_2d );
    DOC( "Reinterpret this raw tripoint as a typed tripoint coordinate without changing x, y, or z." );
    DOC_PARAMS( "origin", "scale" );
    luna::set_fx( ut, "reinterpret_as", &raw_tripoint_reinterpret_as );

    reg_serde_functions( ut );

    luna::set_fx( ut, sol::meta_function::to_string, &tripoint::to_string );
    luna::set_fx( ut, sol::meta_function::equal_to, []( const tripoint & a,
                  const tripoint & b ) -> bool { return a == b; } );
    luna::set_fx( ut, sol::meta_function::less_than, []( const tripoint & a,
                  const tripoint & b ) -> bool { return a < b; } );

    luna::set_fx( ut, sol::meta_function::addition, sol::overload(
                      sol::resolve< tripoint( const tripoint & ) const > ( &tripoint::operator+ ),
                      sol::resolve< tripoint( point ) const > ( &tripoint::operator+ )
                  ) );
    luna::set_fx( ut, sol::meta_function::subtraction, sol::overload(
                      sol::resolve< tripoint( const tripoint & ) const > ( &tripoint::operator- ),
                      sol::resolve< tripoint( point ) const > ( &tripoint::operator- )
                  ) );
    luna::set_fx( ut, sol::meta_function::multiplication, &tripoint::operator* );
    luna::set_fx( ut, sol::meta_function::division, &tripoint::operator/ );
    luna::set_fx( ut, sol::meta_function::floor_division, &tripoint::operator/ );
    luna::set_fx( ut, sol::meta_function::unary_minus,
                  sol::resolve< tripoint() const >( &tripoint::operator- ) );
}

} // namespace cata::detail::lua_coords
