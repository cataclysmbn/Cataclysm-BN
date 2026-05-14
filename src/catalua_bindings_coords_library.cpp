#include "catalua_bindings.h"
#include "catalua_bindings_coords_common.h"

#include <algorithm>
#include <array>
#include <iterator>
#include <optional>
#include <ranges>
#include <string>

namespace cata::detail::lua_coords
{

struct lua_coord_value {
    coords::origin origin;
    coords::scale scale;
    point xy;
    int z;
    bool is_tripoint;
};

struct distance_call {
    sol::this_state lua_state;
    enum class kind {
        rl,
        trig,
        square
    } distance_kind;
};

struct distance_args {
    const sol::object &lhs;
    const sol::object &rhs;
    sol::this_state lua_state;
    distance_call::kind distance_kind;
};

struct coord_factory_spec {
    std::string_view name;
    coords::origin origin;
    coords::scale scale;
};

auto nil_tuple() -> std::tuple<sol::object, sol::object>
{
    return std::make_tuple( sol::nil, sol::nil );
}

auto parsed_scale_or_debug( const std::string &result_scale ) -> std::optional<coords::scale>
{
    const auto parsed = parse_scale( result_scale );
    if( !parsed ) {
        debugmsg( "Unknown coordinate scale '%s'", result_scale );
    }
    return parsed;
}

auto coord_from_object( const sol::object &obj ) -> std::optional<lua_coord_value>
{
    if( obj.is<lua_point_coord>() ) {
        const auto coord = obj.as<lua_point_coord>();
        return lua_coord_value{ coord.origin, coord.scale, coord.raw, 0, false };
    }
    if( obj.is<lua_tripoint_coord>() ) {
        const auto coord = obj.as<lua_tripoint_coord>();
        return lua_coord_value{ coord.origin, coord.scale, coord.raw.xy(), coord.raw.z, true };
    }
    return std::nullopt;
}

auto make_coord_object( const lua_coord_value &coord,
                        sol::this_state lua_state ) -> sol::object
{
    if( coord.is_tripoint ) {
        return sol::make_object( lua_state, make_tripoint_coord( coord.origin, coord.scale,
                                 tripoint( coord.xy, coord.z ) ) );
    }
    return sol::make_object( lua_state, make_point_coord( coord.origin, coord.scale, coord.xy ) );
}

auto lua_project_point_to( const lua_point_coord &coord, const std::string &result_scale,
                           sol::this_state lua_state ) -> sol::object
{
    const auto parsed_scale = parsed_scale_or_debug( result_scale );
    if( !parsed_scale ) {
        return sol::nil;
    }
    const auto projected = project_to( coord, *parsed_scale );
    if( !projected ) {
        debugmsg( "Cannot project %s to scale %s", point_to_string( coord ), result_scale );
        return sol::nil;
    }
    return sol::make_object( lua_state, *projected );
}

auto lua_project_tripoint_to( const lua_tripoint_coord &coord, const std::string &result_scale,
                              sol::this_state lua_state ) -> sol::object
{
    const auto parsed_scale = parsed_scale_or_debug( result_scale );
    if( !parsed_scale ) {
        return sol::nil;
    }
    const auto projected = project_to( coord, *parsed_scale );
    if( !projected ) {
        debugmsg( "Cannot project %s to scale %s", tripoint_to_string( coord ), result_scale );
        return sol::nil;
    }
    return sol::make_object( lua_state, *projected );
}

auto lua_project_point_remain_to( const lua_point_coord &coord, const std::string &result_scale,
                                  sol::this_state lua_state ) -> std::tuple<sol::object, sol::object>
{
    const auto parsed_scale = parsed_scale_or_debug( result_scale );
    if( !parsed_scale ) {
        return nil_tuple();
    }
    if( !can_project_remain( coord.origin, coord.scale, *parsed_scale ) ) {
        debugmsg( "Cannot project_remain %s to scale %s", point_to_string( coord ), result_scale );
        return nil_tuple();
    }

    const auto scale_down = coords::map_squares_per( *parsed_scale ) /
                            coords::map_squares_per( coord.scale );
    const auto quotient = divide_xy_round_to_minus_infinity( coord.raw, scale_down );
    const auto remainder = coord.raw - quotient * scale_down;
    return std::make_tuple(
               sol::make_object( lua_state, make_point_coord( coord.origin, *parsed_scale, quotient ) ),
               sol::make_object( lua_state, make_point_coord( coords::origin_from_scale( *parsed_scale ),
                                 coord.scale, remainder ) )
           );
}

auto lua_project_tripoint_remain_to( const lua_tripoint_coord &coord,
                                     const std::string &result_scale,
                                     sol::this_state lua_state ) -> std::tuple<sol::object, sol::object>
{
    const auto parsed_scale = parsed_scale_or_debug( result_scale );
    if( !parsed_scale ) {
        return nil_tuple();
    }
    if( !can_project_remain( coord.origin, coord.scale, *parsed_scale ) ) {
        debugmsg( "Cannot project_remain %s to scale %s", tripoint_to_string( coord ), result_scale );
        return nil_tuple();
    }

    const auto scale_down = coords::map_squares_per( *parsed_scale ) /
                            coords::map_squares_per( coord.scale );
    const auto quotient_xy = divide_xy_round_to_minus_infinity( coord.raw.xy(), scale_down );
    const auto remainder = coord.raw.xy() - quotient_xy * scale_down;
    return std::make_tuple(
               sol::make_object( lua_state, make_tripoint_coord( coord.origin, *parsed_scale,
                                 tripoint( quotient_xy, coord.raw.z ) ) ),
               sol::make_object( lua_state, make_point_coord( coords::origin_from_scale( *parsed_scale ),
                                 coord.scale, remainder ) )
           );
}

auto lua_project_remain_to( const sol::object &val, const std::string &result_scale,
                            sol::this_state lua_state ) -> std::tuple<sol::object, sol::object>
{
    if( val.is<lua_point_coord>() ) {
        return lua_project_point_remain_to( val.as<lua_point_coord>(), result_scale, lua_state );
    }
    if( val.is<lua_tripoint_coord>() ) {
        return lua_project_tripoint_remain_to( val.as<lua_tripoint_coord>(), result_scale,
                                               lua_state );
    }
    debugmsg( "project_remain expected a PointCoord or TripointCoord" );
    return nil_tuple();
}

auto lua_project_combine( const sol::object &coarse, const sol::object &fine,
                          sol::this_state lua_state ) -> sol::object
{
    const auto coarse_coord = coord_from_object( coarse );
    const auto fine_coord = coord_from_object( fine );
    if( !coarse_coord || !fine_coord ) {
        debugmsg( "project_combine expected PointCoord or TripointCoord arguments" );
        return sol::nil;
    }

    const auto can_combine = can_project_combine( project_combine_check{
        coarse_coord->origin,
        coarse_coord->scale,
        fine_coord->origin,
        fine_coord->scale,
        coarse_coord->is_tripoint,
        fine_coord->is_tripoint
    } );
    if( !can_combine ) {
        debugmsg( "Cannot project_combine %s%s with %s%s",
                  origin_type_name( coarse_coord->origin ), scale_type_name( coarse_coord->scale ),
                  origin_type_name( fine_coord->origin ), scale_type_name( fine_coord->scale ) );
        return sol::nil;
    }

    const auto refined_coarse = project_xy( coarse_coord->xy, coarse_coord->scale,
                                            fine_coord->scale );
    const auto result_xy = refined_coarse + fine_coord->xy;
    if( coarse_coord->is_tripoint ) {
        return make_coord_object( lua_coord_value{ coarse_coord->origin, fine_coord->scale,
                                  result_xy, coarse_coord->z, true }, lua_state );
    }
    if( fine_coord->is_tripoint ) {
        return make_coord_object( lua_coord_value{ coarse_coord->origin, fine_coord->scale,
                                  result_xy, fine_coord->z, true }, lua_state );
    }
    return make_coord_object( lua_coord_value{ coarse_coord->origin, fine_coord->scale,
                              result_xy, 0, false }, lua_state );
}

auto make_distance_object( const point &lhs, const point &rhs,
                           const distance_call &call ) -> sol::object
{
    switch( call.distance_kind ) {
        case distance_call::kind::rl:
            return sol::make_object( call.lua_state, rl_dist( lhs, rhs ) );
        case distance_call::kind::trig:
            return sol::make_object( call.lua_state, trig_dist( lhs, rhs ) );
        case distance_call::kind::square:
            return sol::make_object( call.lua_state, square_dist( lhs, rhs ) );
    }
    return sol::nil;
}

auto make_distance_object( const tripoint &lhs, const tripoint &rhs,
                           const distance_call &call ) -> sol::object
{
    switch( call.distance_kind ) {
        case distance_call::kind::rl:
            return sol::make_object( call.lua_state, rl_dist( lhs, rhs ) );
        case distance_call::kind::trig:
            return sol::make_object( call.lua_state, trig_dist( lhs, rhs ) );
        case distance_call::kind::square:
            return sol::make_object( call.lua_state, square_dist( lhs, rhs ) );
    }
    return sol::nil;
}

auto lua_distance( const distance_args &args ) -> sol::object
{
    const auto call = distance_call{ args.lua_state, args.distance_kind };
    if( args.lhs.is<point>() && args.rhs.is<point>() ) {
        return make_distance_object( args.lhs.as<point>(), args.rhs.as<point>(), call );
    }
    if( args.lhs.is<tripoint>() && args.rhs.is<tripoint>() ) {
        return make_distance_object( args.lhs.as<tripoint>(), args.rhs.as<tripoint>(), call );
    }
    if( args.lhs.is<lua_point_coord>() && args.rhs.is<lua_point_coord>() ) {
        const auto lhs = args.lhs.as<lua_point_coord>();
        const auto rhs = args.rhs.as<lua_point_coord>();
        if( same_coord_kind( lhs, rhs ) ) {
            return make_distance_object( lhs.raw, rhs.raw, call );
        }
    }
    if( args.lhs.is<lua_tripoint_coord>() && args.rhs.is<lua_tripoint_coord>() ) {
        const auto lhs = args.lhs.as<lua_tripoint_coord>();
        const auto rhs = args.rhs.as<lua_tripoint_coord>();
        if( same_coord_kind( lhs, rhs ) ) {
            return make_distance_object( lhs.raw, rhs.raw, call );
        }
    }
    debugmsg( "Distance expects two raw coordinates, or two typed coordinates with matching origin and scale" );
    return sol::nil;
}

auto lua_rl_dist( const sol::object &lhs, const sol::object &rhs,
                  sol::this_state lua_state ) -> sol::object
{
    return lua_distance( distance_args{ lhs, rhs, lua_state, distance_call::kind::rl } );
}

auto lua_trig_dist( const sol::object &lhs, const sol::object &rhs,
                    sol::this_state lua_state ) -> sol::object
{
    return lua_distance( distance_args{ lhs, rhs, lua_state, distance_call::kind::trig } );
}

auto lua_square_dist( const sol::object &lhs, const sol::object &rhs,
                      sol::this_state lua_state ) -> sol::object
{
    return lua_distance( distance_args{ lhs, rhs, lua_state, distance_call::kind::square } );
}

auto point_factory( const coord_factory_spec &spec, const point &raw ) -> lua_point_coord
{
    return make_point_coord( spec.origin, spec.scale, raw );
}

auto tripoint_factory( const coord_factory_spec &spec, const tripoint &raw ) -> lua_tripoint_coord
{
    return make_tripoint_coord( spec.origin, spec.scale, raw );
}

auto bind_point_factory( luna::userlib &lib, const coord_factory_spec &spec ) -> void
{
    luna::set_fx( lib, std::string( "point_" ) + std::string( spec.name ),
    [spec]( const int x, const int y ) {
        return point_factory( spec, point( x, y ) );
    } );
}

auto bind_tripoint_factory( luna::userlib &lib, const coord_factory_spec &spec ) -> void
{
    luna::set_fx( lib, std::string( "tripoint_" ) + std::string( spec.name ),
    [spec]( const int x, const int y, const int z ) {
        return tripoint_factory( spec, tripoint( x, y, z ) );
    } );
}

constexpr auto coord_factory_specs() -> std::array<coord_factory_spec, 31>
{
    return std::array{
        coord_factory_spec{ "rel_ms", coords::origin::relative, coords::scale::map_square },
        coord_factory_spec{ "abs_ms", coords::origin::abs, coords::scale::map_square },
        coord_factory_spec{ "bub_ms", coords::origin::bubble, coords::scale::map_square },
        coord_factory_spec{ "sm_ms", coords::origin::submap, coords::scale::map_square },
        coord_factory_spec{ "omt_ms", coords::origin::overmap_terrain, coords::scale::map_square },
        coord_factory_spec{ "mmr_ms", coords::origin::mem_map_region, coords::scale::map_square },
        coord_factory_spec{ "seg_ms", coords::origin::segment, coords::scale::map_square },
        coord_factory_spec{ "om_ms", coords::origin::overmap, coords::scale::map_square },
        coord_factory_spec{ "rel_veh", coords::origin::relative, coords::scale::vehicle },
        coord_factory_spec{ "mnt_veh", coords::origin::vehicle, coords::scale::vehicle },
        coord_factory_spec{ "rel_sm", coords::origin::relative, coords::scale::submap },
        coord_factory_spec{ "abs_sm", coords::origin::abs, coords::scale::submap },
        coord_factory_spec{ "bub_sm", coords::origin::bubble, coords::scale::submap },
        coord_factory_spec{ "omt_sm", coords::origin::overmap_terrain, coords::scale::submap },
        coord_factory_spec{ "mmr_sm", coords::origin::mem_map_region, coords::scale::submap },
        coord_factory_spec{ "seg_sm", coords::origin::segment, coords::scale::submap },
        coord_factory_spec{ "om_sm", coords::origin::overmap, coords::scale::submap },
        coord_factory_spec{ "rel_omt", coords::origin::relative, coords::scale::overmap_terrain },
        coord_factory_spec{ "abs_omt", coords::origin::abs, coords::scale::overmap_terrain },
        coord_factory_spec{ "mmr_omt", coords::origin::mem_map_region, coords::scale::overmap_terrain },
        coord_factory_spec{ "seg_omt", coords::origin::segment, coords::scale::overmap_terrain },
        coord_factory_spec{ "om_omt", coords::origin::overmap, coords::scale::overmap_terrain },
        coord_factory_spec{ "rel_mmr", coords::origin::relative, coords::scale::mem_map_region },
        coord_factory_spec{ "abs_mmr", coords::origin::abs, coords::scale::mem_map_region },
        coord_factory_spec{ "seg_mmr", coords::origin::segment, coords::scale::mem_map_region },
        coord_factory_spec{ "om_mmr", coords::origin::overmap, coords::scale::mem_map_region },
        coord_factory_spec{ "rel_seg", coords::origin::relative, coords::scale::segment },
        coord_factory_spec{ "abs_seg", coords::origin::abs, coords::scale::segment },
        coord_factory_spec{ "om_seg", coords::origin::overmap, coords::scale::segment },
        coord_factory_spec{ "rel_om", coords::origin::relative, coords::scale::overmap },
        coord_factory_spec{ "abs_om", coords::origin::abs, coords::scale::overmap }
    };
}

auto bind_coord_factories( luna::userlib &lib ) -> void
{
    std::ranges::for_each( coord_factory_specs(), [&lib]( const coord_factory_spec & spec ) {
        bind_point_factory( lib, spec );
        bind_tripoint_factory( lib, spec );
    } );
}

auto bind_named_point_constructor( sol::state_view lua, const coord_factory_spec &spec ) -> void
{
    auto factory = lua.create_table();
    factory["new"] = sol::overload(
    [spec]() {
        return point_factory( spec, point_zero );
    },
    [spec]( const point & raw ) {
        return point_factory( spec, raw );
    },
    [spec]( const int x, const int y ) {
        return point_factory( spec, point( x, y ) );
    } );
    lua[coord_type_name( false, spec.origin, spec.scale )] = factory;
}

auto bind_named_tripoint_constructor( sol::state_view lua, const coord_factory_spec &spec ) -> void
{
    auto factory = lua.create_table();
    factory["new"] = sol::overload(
    [spec]() {
        return tripoint_factory( spec, tripoint_zero );
    },
    [spec]( const tripoint & raw ) {
        return tripoint_factory( spec, raw );
    },
    [spec]( const point & xy, const int z ) {
        return tripoint_factory( spec, tripoint( xy, z ) );
    },
    [spec]( const lua_point_coord & xy, const int z ) {
        if( xy.origin != spec.origin || xy.scale != spec.scale ) {
            throw std::runtime_error( string_format( "Expected %s for %s constructor",
                                      coord_type_name( false, spec.origin, spec.scale ),
                                      coord_type_name( true, spec.origin, spec.scale ) ) );
        }
        return tripoint_factory( spec, tripoint( xy.raw, z ) );
    },
    [spec]( const int x, const int y, const int z ) {
        return tripoint_factory( spec, tripoint( x, y, z ) );
    } );
    lua[coord_type_name( true, spec.origin, spec.scale )] = factory;
}

auto bind_named_coord_constructors( sol::state_view lua ) -> void
{
    std::ranges::for_each( coord_factory_specs(), [lua]( const coord_factory_spec & spec ) {
        bind_named_point_constructor( lua, spec );
        bind_named_tripoint_constructor( lua, spec );
    } );
}

template<typename Range, typename Maker>
auto make_point_coord_vector( Range &&range, Maker maker ) -> std::vector<lua_point_coord>
{
    auto result = std::vector<lua_point_coord>{};
    std::ranges::transform( range, std::back_inserter( result ), maker );
    return result;
}

auto make_submap_tiles() -> std::vector<lua_point_coord>
{
    return make_point_coord_vector( ::submap_tiles(), []( const point_sm_ms & p ) {
        return make_point_coord( coords::origin::submap, coords::scale::map_square, p.raw() );
    } );
}

auto make_tinymap_tiles() -> std::vector<lua_point_coord>
{
    return make_point_coord_vector( ::tinymap_tiles(), []( const point_bub_ms & p ) {
        return make_point_coord( coords::origin::bubble, coords::scale::map_square, p.raw() );
    } );
}

auto make_overmap_terrain_tiles() -> std::vector<lua_point_coord>
{
    return make_point_coord_vector( ::overmap_terrain_tiles(), []( const point_omt_ms & p ) {
        return make_point_coord( coords::origin::overmap_terrain, coords::scale::map_square,
                                 p.raw() );
    } );
}

auto make_overmap_tiles() -> std::vector<lua_point_coord>
{
    return make_point_coord_vector( ::overmap_tiles(), []( const point_om_ms & p ) {
        return make_point_coord( coords::origin::overmap, coords::scale::map_square, p.raw() );
    } );
}

} // namespace cata::detail::lua_coords

auto cata::detail::reg_coords_library( sol::state &lua ) -> void
{
    auto lua_view = sol::state_view( lua );
    auto lib = luna::begin_lib( lua_view, "coords" );

    luna::set_fx( lib, "point", sol::overload(
    []( const std::string & origin, const std::string & scale, const point & raw ) {
        return lua_coords::make_point_coord( origin, scale, raw );
    },
    []( const std::string & origin, const std::string & scale, const int x, const int y ) {
        return lua_coords::make_point_coord( origin, scale, point( x, y ) );
    } ) );
    luna::set_fx( lib, "tripoint", sol::overload(
    []( const std::string & origin, const std::string & scale, const tripoint & raw ) {
        return lua_coords::make_tripoint_coord( origin, scale, raw );
    },
    []( const std::string & origin, const std::string & scale, const int x, const int y,
    const int z ) {
        return lua_coords::make_tripoint_coord( origin, scale, tripoint( x, y, z ) );
    } ) );
    lua_coords::bind_coord_factories( lib );

    luna::set_fx( lib, "project_remain", &lua_coords::lua_project_remain_to );
    luna::set_fx( lib, "project_remain_sm", []( const sol::object & val, sol::this_state L ) {
        return lua_coords::lua_project_remain_to( val, "sm", L );
    } );
    luna::set_fx( lib, "project_remain_omt", []( const sol::object & val, sol::this_state L ) {
        return lua_coords::lua_project_remain_to( val, "omt", L );
    } );
    luna::set_fx( lib, "project_remain_mmr", []( const sol::object & val, sol::this_state L ) {
        return lua_coords::lua_project_remain_to( val, "mmr", L );
    } );
    luna::set_fx( lib, "project_remain_seg", []( const sol::object & val, sol::this_state L ) {
        return lua_coords::lua_project_remain_to( val, "seg", L );
    } );
    luna::set_fx( lib, "project_remain_om", []( const sol::object & val, sol::this_state L ) {
        return lua_coords::lua_project_remain_to( val, "om", L );
    } );
    luna::set_fx( lib, "project_combine", &lua_coords::lua_project_combine );
    luna::set_fx( lib, "rl_dist", &lua_coords::lua_rl_dist );
    luna::set_fx( lib, "trig_dist", &lua_coords::lua_trig_dist );
    luna::set_fx( lib, "square_dist", &lua_coords::lua_square_dist );

    luna::set_fx( lib, "submap_tiles", &lua_coords::make_submap_tiles );
    luna::set_fx( lib, "tinymap_tiles", &lua_coords::make_tinymap_tiles );
    luna::set_fx( lib, "overmap_terrain_tiles", &lua_coords::make_overmap_terrain_tiles );
    luna::set_fx( lib, "overmap_tiles", &lua_coords::make_overmap_tiles );

    luna::finalize_lib( lib );
    lua_coords::bind_named_coord_constructors( lua_view );
}
