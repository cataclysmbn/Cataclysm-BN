#include "catalua_bindings_coords_common.h"

namespace cata::detail::lua_coords
{

auto lua_point_add( const lua_point_coord &lhs, const sol::object &rhs,
                    sol::this_state lua_state ) -> sol::object
{
    if( rhs.is<point>() ) {
        return sol::make_object( lua_state, make_point_coord( lhs.origin, lhs.scale,
                                 lhs.raw + rhs.as<point>() ) );
    }
    if( rhs.is<lua_point_coord>() ) {
        const auto other = rhs.as<lua_point_coord>();
        if( lhs.scale == other.scale && other.origin == coords::origin::relative ) {
            return sol::make_object( lua_state, make_point_coord( lhs.origin, lhs.scale,
                                     lhs.raw + other.raw ) );
        }
        if( lhs.scale == other.scale && lhs.origin == coords::origin::relative ) {
            return sol::make_object( lua_state, make_point_coord( other.origin, other.scale,
                                     lhs.raw + other.raw ) );
        }
    }
    debugmsg( "PointCoord addition expects a raw Point or a relative PointCoord at the same scale" );
    return sol::nil;
}

auto lua_point_subtract( const lua_point_coord &lhs, const sol::object &rhs,
                         sol::this_state lua_state ) -> sol::object
{
    if( rhs.is<point>() ) {
        return sol::make_object( lua_state, make_point_coord( lhs.origin, lhs.scale,
                                 lhs.raw - rhs.as<point>() ) );
    }
    if( rhs.is<lua_point_coord>() ) {
        const auto other = rhs.as<lua_point_coord>();
        if( lhs.scale == other.scale && other.origin == coords::origin::relative ) {
            return sol::make_object( lua_state, make_point_coord( lhs.origin, lhs.scale,
                                     lhs.raw - other.raw ) );
        }
        if( same_coord_kind( lhs, other ) && lhs.origin != coords::origin::relative ) {
            return sol::make_object( lua_state, make_point_coord( coords::origin::relative,
                                     lhs.scale, lhs.raw - other.raw ) );
        }
    }
    debugmsg( "PointCoord subtraction expects a raw Point, a relative PointCoord at the same scale, or a matching PointCoord" );
    return sol::nil;
}

auto lua_point_multiply( const lua_point_coord &lhs, const int rhs,
                         sol::this_state lua_state ) -> sol::object
{
    if( lhs.origin == coords::origin::relative ) {
        return sol::make_object( lua_state, make_point_coord( lhs.origin, lhs.scale, lhs.raw * rhs ) );
    }
    debugmsg( "PointCoord multiplication is only valid for relative coordinates" );
    return sol::nil;
}

auto lua_point_rotate( const lua_point_coord &coord, const int turns,
                       const point &dim ) -> lua_point_coord
{
    return make_point_coord( coord.origin, coord.scale, coord.raw.rotate( turns, dim ) );
}

auto lua_point_less_than( const lua_point_coord &lhs, const lua_point_coord &rhs ) -> bool
{
    return std::tie( lhs.origin, lhs.scale, lhs.raw ) < std::tie( rhs.origin, rhs.scale, rhs.raw );
}

auto bind_point_projection_methods( sol::usertype<lua_point_coord> &ut ) -> void
{
    luna::set_fx( ut, "to", &lua_project_point_to );
    luna::set_fx( ut, "to_ms", []( const lua_point_coord & coord, sol::this_state L ) {
        return lua_project_point_to( coord, "ms", L );
    } );
    luna::set_fx( ut, "to_veh", []( const lua_point_coord & coord, sol::this_state L ) {
        return lua_project_point_to( coord, "veh", L );
    } );
    luna::set_fx( ut, "to_sm", []( const lua_point_coord & coord, sol::this_state L ) {
        return lua_project_point_to( coord, "sm", L );
    } );
    luna::set_fx( ut, "to_omt", []( const lua_point_coord & coord, sol::this_state L ) {
        return lua_project_point_to( coord, "omt", L );
    } );
    luna::set_fx( ut, "to_mmr", []( const lua_point_coord & coord, sol::this_state L ) {
        return lua_project_point_to( coord, "mmr", L );
    } );
    luna::set_fx( ut, "to_seg", []( const lua_point_coord & coord, sol::this_state L ) {
        return lua_project_point_to( coord, "seg", L );
    } );
    luna::set_fx( ut, "to_om", []( const lua_point_coord & coord, sol::this_state L ) {
        return lua_project_point_to( coord, "om", L );
    } );

    luna::set_fx( ut, "project_remain", &lua_project_point_remain_to );
    luna::set_fx( ut, "project_remain_sm", []( const lua_point_coord & coord, sol::this_state L ) {
        return lua_project_point_remain_to( coord, "sm", L );
    } );
    luna::set_fx( ut, "project_remain_omt", []( const lua_point_coord & coord, sol::this_state L ) {
        return lua_project_point_remain_to( coord, "omt", L );
    } );
    luna::set_fx( ut, "project_remain_mmr", []( const lua_point_coord & coord, sol::this_state L ) {
        return lua_project_point_remain_to( coord, "mmr", L );
    } );
    luna::set_fx( ut, "project_remain_seg", []( const lua_point_coord & coord, sol::this_state L ) {
        return lua_project_point_remain_to( coord, "seg", L );
    } );
    luna::set_fx( ut, "project_remain_om", []( const lua_point_coord & coord, sol::this_state L ) {
        return lua_project_point_remain_to( coord, "om", L );
    } );
    luna::set_fx( ut, "project_combine", []( const lua_point_coord & coord, const sol::object & fine,
    sol::this_state L ) {
        return lua_project_combine( sol::make_object( L, coord ), fine, L );
    } );
}

auto reg_lua_point_coord( sol::state &lua ) -> void
{
    auto ut = luna::new_usertype<lua_point_coord>(
                  lua,
                  luna::no_bases,
                  luna::no_constructor
              );

    luna::detail::doc_member_fake<int>( ut, "x" );
    luna::detail::doc_member_fake<int>( ut, "y" );

    luna::set_fx( ut, sol::meta_function::index, []( const lua_tripoint_coord & pt,
    const sol::object & k, sol::this_state L ) -> sol::object {
        const auto key = k.as<std::optional<std::string>>();
        if( key.has_value() )
        {
            const auto &ss = key.value();
            if( ss == "x" ) {
                return sol::make_object( L, pt.raw.x );
            }
            if( ss == "y" ) {
                return sol::make_object( L, pt.raw.y );
            }
        }
        return sol::nil;
    } );
    luna::set_fx( ut, sol::meta_function::new_index, []( lua_tripoint_coord & pt, const sol::object & k,
    const sol::object & v, sol::this_state L ) -> void {
        const auto key = k.as<std::optional<std::string>>();
        const auto val = v.as<std::optional<int>>();
        if( key.has_value() && val.has_value() )
        {
            const auto &ss = key.value();
            if( ss == "x" ) {
                pt.raw.x = val.value();
            }
            if( ss == "y" ) {
                pt.raw.y = val.value();
            }
        }
    } );


    luna::set_fx( ut, "origin", []( const lua_point_coord & pt ) { return origin_lua_name( pt.origin ); } );
    luna::set_fx( ut, "scale", []( const lua_point_coord & pt ) { return scale_lua_name( pt.scale ); } );
    luna::set_fx( ut, "type", []( const lua_point_coord & pt ) {
        return coord_type_name( false, pt.origin, pt.scale );
    } );
    luna::set_fx( ut, "raw", []( const lua_point_coord & pt ) { return pt.raw; } );
    luna::set_fx( ut, "rotate", &lua_point_rotate );

    bind_point_projection_methods( ut );

    luna::set_fx( ut, "rl_dist", []( const lua_point_coord & lhs, const sol::object & rhs,
    sol::this_state L ) {
        return lua_rl_dist( sol::make_object( L, lhs ), rhs, L );
    } );
    luna::set_fx( ut, "trig_dist", []( const lua_point_coord & lhs, const sol::object & rhs,
    sol::this_state L ) {
        return lua_trig_dist( sol::make_object( L, lhs ), rhs, L );
    } );
    luna::set_fx( ut, "square_dist", []( const lua_point_coord & lhs, const sol::object & rhs,
    sol::this_state L ) {
        return lua_square_dist( sol::make_object( L, lhs ), rhs, L );
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
