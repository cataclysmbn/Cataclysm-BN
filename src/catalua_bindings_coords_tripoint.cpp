#include "catalua_bindings_coords_common.h"

namespace cata::detail::lua_coords
{

auto lua_tripoint_add( const lua_tripoint_coord &lhs, const sol::object &rhs,
                       sol::this_state lua_state ) -> sol::object
{
    if( rhs.is<point>() ) {
        return sol::make_object( lua_state, make_tripoint_coord( lhs.origin, lhs.scale,
                                 lhs.raw + rhs.as<point>() ) );
    }
    if( rhs.is<tripoint>() ) {
        return sol::make_object( lua_state, make_tripoint_coord( lhs.origin, lhs.scale,
                                 lhs.raw + rhs.as<tripoint>() ) );
    }
    if( rhs.is<lua_point_coord>() ) {
        const auto other = rhs.as<lua_point_coord>();
        if( lhs.scale == other.scale && other.origin == coords::origin::relative ) {
            return sol::make_object( lua_state, make_tripoint_coord( lhs.origin, lhs.scale,
                                     lhs.raw + other.raw ) );
        }
    }
    if( rhs.is<lua_tripoint_coord>() ) {
        const auto other = rhs.as<lua_tripoint_coord>();
        if( lhs.scale == other.scale && other.origin == coords::origin::relative ) {
            return sol::make_object( lua_state, make_tripoint_coord( lhs.origin, lhs.scale,
                                     lhs.raw + other.raw ) );
        }
        if( lhs.scale == other.scale && lhs.origin == coords::origin::relative ) {
            return sol::make_object( lua_state, make_tripoint_coord( other.origin, other.scale,
                                     lhs.raw + other.raw ) );
        }
    }
    debugmsg( "TripointCoord addition expects a raw Point, raw Tripoint, or a relative coordinate at the same scale" );
    return sol::nil;
}

auto lua_tripoint_subtract( const lua_tripoint_coord &lhs, const sol::object &rhs,
                            sol::this_state lua_state ) -> sol::object
{
    if( rhs.is<point>() ) {
        return sol::make_object( lua_state, make_tripoint_coord( lhs.origin, lhs.scale,
                                 lhs.raw - rhs.as<point>() ) );
    }
    if( rhs.is<tripoint>() ) {
        return sol::make_object( lua_state, make_tripoint_coord( lhs.origin, lhs.scale,
                                 lhs.raw - rhs.as<tripoint>() ) );
    }
    if( rhs.is<lua_point_coord>() ) {
        const auto other = rhs.as<lua_point_coord>();
        if( lhs.scale == other.scale && other.origin == coords::origin::relative ) {
            return sol::make_object( lua_state, make_tripoint_coord( lhs.origin, lhs.scale,
                                     lhs.raw - other.raw ) );
        }
    }
    if( rhs.is<lua_tripoint_coord>() ) {
        const auto other = rhs.as<lua_tripoint_coord>();
        if( lhs.scale == other.scale && other.origin == coords::origin::relative ) {
            return sol::make_object( lua_state, make_tripoint_coord( lhs.origin, lhs.scale,
                                     lhs.raw - other.raw ) );
        }
        if( same_coord_kind( lhs, other ) && lhs.origin != coords::origin::relative ) {
            return sol::make_object( lua_state, make_tripoint_coord( coords::origin::relative,
                                     lhs.scale, lhs.raw - other.raw ) );
        }
    }
    debugmsg( "TripointCoord subtraction expects a raw Point, raw Tripoint, a relative coordinate at the same scale, or a matching TripointCoord" );
    return sol::nil;
}

auto lua_tripoint_multiply( const lua_tripoint_coord &lhs, const int rhs,
                            sol::this_state lua_state ) -> sol::object
{
    if( lhs.origin == coords::origin::relative ) {
        return sol::make_object( lua_state, make_tripoint_coord( lhs.origin, lhs.scale,
                                 lhs.raw * rhs ) );
    }
    debugmsg( "TripointCoord multiplication is only valid for relative coordinates" );
    return sol::nil;
}

auto lua_tripoint_rotate( const lua_tripoint_coord &coord, const int turns,
                          const point &dim ) -> lua_tripoint_coord
{
    return make_tripoint_coord( coord.origin, coord.scale, coord.raw.rotate_2d( turns, dim ) );
}

auto lua_tripoint_less_than( const lua_tripoint_coord &lhs,
                             const lua_tripoint_coord &rhs ) -> bool
{
    return std::tie( lhs.origin, lhs.scale, lhs.raw ) < std::tie( rhs.origin, rhs.scale, rhs.raw );
}

auto bind_tripoint_projection_methods( sol::usertype<lua_tripoint_coord> &ut ) -> void
{
    luna::set_fx( ut, "to", &lua_project_tripoint_to );
    luna::set_fx( ut, "to_ms", []( const lua_tripoint_coord & coord, sol::this_state L ) {
        return lua_project_tripoint_to( coord, "ms", L );
    } );
    luna::set_fx( ut, "to_veh", []( const lua_tripoint_coord & coord, sol::this_state L ) {
        return lua_project_tripoint_to( coord, "veh", L );
    } );
    luna::set_fx( ut, "to_sm", []( const lua_tripoint_coord & coord, sol::this_state L ) {
        return lua_project_tripoint_to( coord, "sm", L );
    } );
    luna::set_fx( ut, "to_omt", []( const lua_tripoint_coord & coord, sol::this_state L ) {
        return lua_project_tripoint_to( coord, "omt", L );
    } );
    luna::set_fx( ut, "to_mmr", []( const lua_tripoint_coord & coord, sol::this_state L ) {
        return lua_project_tripoint_to( coord, "mmr", L );
    } );
    luna::set_fx( ut, "to_seg", []( const lua_tripoint_coord & coord, sol::this_state L ) {
        return lua_project_tripoint_to( coord, "seg", L );
    } );
    luna::set_fx( ut, "to_om", []( const lua_tripoint_coord & coord, sol::this_state L ) {
        return lua_project_tripoint_to( coord, "om", L );
    } );

    luna::set_fx( ut, "project_remain", &lua_project_tripoint_remain_to );
    luna::set_fx( ut, "project_remain_sm", []( const lua_tripoint_coord & coord,
    sol::this_state L ) {
        return lua_project_tripoint_remain_to( coord, "sm", L );
    } );
    luna::set_fx( ut, "project_remain_omt", []( const lua_tripoint_coord & coord,
    sol::this_state L ) {
        return lua_project_tripoint_remain_to( coord, "omt", L );
    } );
    luna::set_fx( ut, "project_remain_mmr", []( const lua_tripoint_coord & coord,
    sol::this_state L ) {
        return lua_project_tripoint_remain_to( coord, "mmr", L );
    } );
    luna::set_fx( ut, "project_remain_seg", []( const lua_tripoint_coord & coord,
    sol::this_state L ) {
        return lua_project_tripoint_remain_to( coord, "seg", L );
    } );
    luna::set_fx( ut, "project_remain_om", []( const lua_tripoint_coord & coord,
    sol::this_state L ) {
        return lua_project_tripoint_remain_to( coord, "om", L );
    } );
    luna::set_fx( ut, "project_combine", []( const lua_tripoint_coord & coord, const sol::object & fine,
    sol::this_state L ) {
        return lua_project_combine( sol::make_object( L, coord ), fine, L );
    } );
}

auto reg_lua_tripoint_coord( sol::state &lua ) -> void
{
    auto ut = luna::new_usertype<lua_tripoint_coord>(
                  lua,
                  luna::no_bases,
                  luna::no_constructor
              );

    DOC( "Gets x" );
    luna::set_fx( ut, "x", []( const lua_tripoint_coord & pt ) -> int { return pt.raw.x; } );
    DOC( "Sets x" );
    luna::set_fx( ut, "set_x", []( lua_tripoint_coord & pt, const int x ) -> void { pt.raw.x = x; } );
    DOC( "Gets y" );
    luna::set_fx( ut, "y", []( const lua_tripoint_coord & pt ) -> int { return pt.raw.y; } );
    DOC( "Sets y" );
    luna::set_fx( ut, "set_y", []( lua_tripoint_coord & pt, const int y ) -> void { pt.raw.y = y; } );
    DOC( "Gets z" );
    luna::set_fx( ut, "z", []( const lua_tripoint_coord & pt ) -> int { return pt.raw.z; } );
    DOC( "Sets z" );
    luna::set_fx( ut, "set_z", []( lua_tripoint_coord & pt, const int z ) -> void { pt.raw.z = z; } );
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

    bind_tripoint_projection_methods( ut );

    luna::set_fx( ut, "rl_dist", []( const lua_tripoint_coord & lhs, const sol::object & rhs,
    sol::this_state L ) {
        return lua_rl_dist( sol::make_object( L, lhs ), rhs, L );
    } );
    luna::set_fx( ut, "trig_dist", []( const lua_tripoint_coord & lhs, const sol::object & rhs,
    sol::this_state L ) {
        return lua_trig_dist( sol::make_object( L, lhs ), rhs, L );
    } );
    luna::set_fx( ut, "square_dist", []( const lua_tripoint_coord & lhs, const sol::object & rhs,
    sol::this_state L ) {
        return lua_square_dist( sol::make_object( L, lhs ), rhs, L );
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
