#include "catalua_bindings.h"
#include "catalua_bindings_coords_common.h"
#include "line.h"
#include "string_formatter.h"

#include <algorithm>
#include <array>
#include <iterator>
#include <optional>
#include <ranges>
#include <string>
#include <tuple>
#include <vector>

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
    enum class kind {
        rl,
        trig,
        square
    } distance_kind;
};

struct coord_factory_spec {
    std::string_view name;
    coords::origin origin;
    coords::scale scale;
};

auto parsed_scale_or_debug( const std::string &result_scale ) -> std::optional<coords::scale>
{
    const auto parsed = parse_scale( result_scale );
    if( !parsed ) {
        debugmsg( "Unknown coordinate scale '%s'", result_scale );
    }
    return parsed;
}

auto coord_from_lua_coord( const lua_point_coord &coord ) -> lua_coord_value
{
    return lua_coord_value{ coord.origin, coord.scale, coord.raw, 0, false };
}

auto coord_from_lua_coord( const lua_tripoint_coord &coord ) -> lua_coord_value
{
    return lua_coord_value{ coord.origin, coord.scale, coord.raw.xy(), coord.raw.z, true };
}

auto coord_from_object( const sol::object &obj ) -> std::optional<lua_coord_value>
{
    if( obj.is<lua_point_coord>() ) {
        return coord_from_lua_coord( obj.as<lua_point_coord>() );
    }
    if( obj.is<lua_tripoint_coord>() ) {
        return coord_from_lua_coord( obj.as<lua_tripoint_coord>() );
    }
    return std::nullopt;
}

auto make_coord_result( const lua_coord_value &coord ) -> lua_coord_result
{
    if( coord.is_tripoint ) {
        return make_tripoint_coord( coord.origin, coord.scale, tripoint( coord.xy, coord.z ) );
    }
    return make_point_coord( coord.origin, coord.scale, coord.xy );
}

auto lua_project_point_to( const lua_point_coord &coord,
                           const std::string &result_scale ) -> std::optional<lua_point_coord>
{
    const auto parsed_scale = parsed_scale_or_debug( result_scale );
    if( !parsed_scale ) {
        return std::nullopt;
    }
    const auto projected = project_to( coord, *parsed_scale );
    if( !projected ) {
        debugmsg( "Cannot project %s to scale %s", point_to_string( coord ), result_scale );
        return std::nullopt;
    }
    return projected;
}

auto lua_project_tripoint_to( const lua_tripoint_coord &coord,
                              const std::string &result_scale ) -> std::optional<lua_tripoint_coord>
{
    const auto parsed_scale = parsed_scale_or_debug( result_scale );
    if( !parsed_scale ) {
        return std::nullopt;
    }
    const auto projected = project_to( coord, *parsed_scale );
    if( !projected ) {
        debugmsg( "Cannot project %s to scale %s", tripoint_to_string( coord ), result_scale );
        return std::nullopt;
    }
    return projected;
}

auto lua_project_to( const sol::object &val, const std::string &result_scale ) -> lua_coord_result
{
    if( val.is<lua_point_coord>() ) {
        const auto projected = lua_project_point_to( val.as<lua_point_coord>(), result_scale );
        if( !projected ) {
            return sol::nil;
        }
        return *projected;
    }
    if( val.is<lua_tripoint_coord>() ) {
        const auto projected = lua_project_tripoint_to( val.as<lua_tripoint_coord>(), result_scale );
        if( !projected ) {
            return sol::nil;
        }
        return *projected;
    }
    debugmsg( "project_to expected a PointCoord or TripointCoord" );
    return sol::nil;
}

auto lua_project_point_remain_to( const lua_point_coord &coord,
                                  const std::string &result_scale ) ->
std::tuple<std::optional<lua_point_coord>, std::optional<lua_point_coord>>
{
    const auto parsed_scale = parsed_scale_or_debug( result_scale );
    if( !parsed_scale ) {
        return std::make_tuple( std::optional<lua_point_coord>(), std::optional<lua_point_coord>() );
    }
    if( !can_project_remain( coord.origin, coord.scale, *parsed_scale ) ) {
        debugmsg( "Cannot project_remain %s to scale %s", point_to_string( coord ), result_scale );
        return std::make_tuple( std::optional<lua_point_coord>(), std::optional<lua_point_coord>() );
    }

    const auto scale_down = coords::map_squares_per( *parsed_scale ) /
                            coords::map_squares_per( coord.scale );
    const auto quotient = divide_xy_round_to_minus_infinity( coord.raw, scale_down );
    const auto remainder = coord.raw - quotient * scale_down;
    return std::make_tuple(
               make_point_coord( coord.origin, *parsed_scale, quotient ),
               make_point_coord( coords::origin_from_scale( *parsed_scale ), coord.scale, remainder )
           );
}

auto lua_project_tripoint_remain_to( const lua_tripoint_coord &coord,
                                     const std::string &result_scale ) ->
std::tuple<std::optional<lua_tripoint_coord>, std::optional<lua_point_coord>>
{
    const auto parsed_scale = parsed_scale_or_debug( result_scale );
    if( !parsed_scale ) {
        return std::make_tuple( std::optional<lua_tripoint_coord>(), std::optional<lua_point_coord>() );
    }
    if( !can_project_remain( coord.origin, coord.scale, *parsed_scale ) ) {
        debugmsg( "Cannot project_remain %s to scale %s", tripoint_to_string( coord ), result_scale );
        return std::make_tuple( std::optional<lua_tripoint_coord>(), std::optional<lua_point_coord>() );
    }

    const auto scale_down = coords::map_squares_per( *parsed_scale ) /
                            coords::map_squares_per( coord.scale );
    const auto quotient_xy = divide_xy_round_to_minus_infinity( coord.raw.xy(), scale_down );
    const auto remainder = coord.raw.xy() - quotient_xy * scale_down;
    return std::make_tuple(
               make_tripoint_coord( coord.origin, *parsed_scale, tripoint( quotient_xy, coord.raw.z ) ),
               make_point_coord( coords::origin_from_scale( *parsed_scale ), coord.scale, remainder )
           );
}

auto lua_project_remain_to( const sol::object &val,
                            const std::string &result_scale ) -> std::tuple<lua_coord_result, std::optional<lua_point_coord>>
{
    if( val.is<lua_point_coord>() ) {
        const auto [coarse, remainder] = lua_project_point_remain_to( val.as<lua_point_coord>(),
                                         result_scale );
        if( !coarse || !remainder ) {
            return std::make_tuple( lua_coord_result{ sol::nil }, std::optional<lua_point_coord>() );
        }
        return std::make_tuple( lua_coord_result{ *coarse }, remainder );
    }
    if( val.is<lua_tripoint_coord>() ) {
        const auto [coarse, remainder] = lua_project_tripoint_remain_to( val.as<lua_tripoint_coord>(),
                                         result_scale );
        if( !coarse || !remainder ) {
            return std::make_tuple( lua_coord_result{ sol::nil }, std::optional<lua_point_coord>() );
        }
        return std::make_tuple( lua_coord_result{ *coarse }, remainder );
    }
    debugmsg( "project_remain expected a PointCoord or TripointCoord" );
    return std::make_tuple( lua_coord_result{ sol::nil }, std::optional<lua_point_coord>() );
}

auto lua_project_combine_impl( const lua_coord_value &coarse_coord,
                               const std::optional<lua_coord_value> &fine_coord ) -> lua_coord_result
{
    if( !fine_coord ) {
        debugmsg( "project_combine expected PointCoord or TripointCoord arguments" );
        return sol::nil;
    }

    const auto can_combine = can_project_combine( project_combine_check{
        coarse_coord.origin,
        coarse_coord.scale,
        fine_coord->origin,
        fine_coord->scale,
        coarse_coord.is_tripoint,
        fine_coord->is_tripoint
    } );
    if( !can_combine ) {
        debugmsg( "Cannot project_combine %s%s with %s%s",
                  origin_type_name( coarse_coord.origin ), scale_type_name( coarse_coord.scale ),
                  origin_type_name( fine_coord->origin ), scale_type_name( fine_coord->scale ) );
        return sol::nil;
    }

    const auto refined_coarse = project_xy( coarse_coord.xy, coarse_coord.scale,
                                            fine_coord->scale );
    const auto result_xy = refined_coarse + fine_coord->xy;
    if( coarse_coord.is_tripoint ) {
        return make_coord_result( lua_coord_value{ coarse_coord.origin, fine_coord->scale,
                                  result_xy, coarse_coord.z, true } );
    }
    if( fine_coord->is_tripoint ) {
        return make_coord_result( lua_coord_value{ coarse_coord.origin, fine_coord->scale,
                                  result_xy, fine_coord->z, true } );
    }
    return make_coord_result( lua_coord_value{ coarse_coord.origin, fine_coord->scale,
                              result_xy, 0, false } );
}

auto lua_project_combine( const sol::object &coarse, const sol::object &fine ) -> lua_coord_result
{
    const auto coarse_coord = coord_from_object( coarse );
    if( !coarse_coord ) {
        debugmsg( "project_combine expected PointCoord or TripointCoord arguments" );
        return sol::nil;
    }
    return lua_project_combine_impl( *coarse_coord, coord_from_object( fine ) );
}

auto lua_project_combine( const lua_point_coord &coarse,
                          const sol::object &fine ) -> lua_coord_result
{
    return lua_project_combine_impl( coord_from_lua_coord( coarse ), coord_from_object( fine ) );
}

auto lua_project_combine( const lua_tripoint_coord &coarse,
                          const sol::object &fine ) -> lua_coord_result
{
    return lua_project_combine_impl( coord_from_lua_coord( coarse ), coord_from_object( fine ) );
}

template<typename Result>
auto make_distance_value( const point &lhs, const point &rhs,
                          const distance_call &call ) -> Result
{
    switch( call.distance_kind ) {
        case distance_call::kind::rl:
            return static_cast<Result>( rl_dist( lhs, rhs ) );
        case distance_call::kind::trig:
            return static_cast<Result>( trig_dist( lhs, rhs ) );
        case distance_call::kind::square:
            return static_cast<Result>( square_dist( lhs, rhs ) );
    }
    return Result();
}

template<typename Result>
auto make_distance_value( const tripoint &lhs, const tripoint &rhs,
                          const distance_call &call ) -> Result
{
    switch( call.distance_kind ) {
        case distance_call::kind::rl:
            return static_cast<Result>( rl_dist( lhs, rhs ) );
        case distance_call::kind::trig:
            return static_cast<Result>( trig_dist( lhs, rhs ) );
        case distance_call::kind::square:
            return static_cast<Result>( square_dist( lhs, rhs ) );
    }
    return Result();
}

template<typename Result>
auto lua_point_coord_distance( const lua_point_coord &lhs, const lua_point_coord &rhs,
                               const distance_call &call ) -> std::optional<Result>
{
    if( same_coord_kind( lhs, rhs ) ) {
        return make_distance_value<Result>( lhs.raw, rhs.raw, call );
    }
    debugmsg( "Distance expects two PointCoord values with matching origin and scale" );
    return std::nullopt;
}

template<typename Result>
auto lua_tripoint_coord_distance( const lua_tripoint_coord &lhs, const lua_tripoint_coord &rhs,
                                  const distance_call &call ) -> std::optional<Result>
{
    if( same_coord_kind( lhs, rhs ) ) {
        return make_distance_value<Result>( lhs.raw, rhs.raw, call );
    }
    debugmsg( "Distance expects two TripointCoord values with matching origin and scale" );
    return std::nullopt;
}

auto raw_point_rl_dist( const point &lhs, const point &rhs ) -> int
{
    return make_distance_value<int>( lhs, rhs, distance_call{ distance_call::kind::rl } );
}

auto raw_point_trig_dist( const point &lhs, const point &rhs ) -> double
{
    return make_distance_value<double>( lhs, rhs, distance_call{ distance_call::kind::trig } );
}

auto raw_point_square_dist( const point &lhs, const point &rhs ) -> int
{
    return make_distance_value<int>( lhs, rhs, distance_call{ distance_call::kind::square } );
}

auto raw_tripoint_rl_dist( const tripoint &lhs, const tripoint &rhs ) -> int
{
    return make_distance_value<int>( lhs, rhs, distance_call{ distance_call::kind::rl } );
}

auto raw_tripoint_trig_dist( const tripoint &lhs, const tripoint &rhs ) -> double
{
    return make_distance_value<double>( lhs, rhs, distance_call{ distance_call::kind::trig } );
}

auto raw_tripoint_square_dist( const tripoint &lhs, const tripoint &rhs ) -> int
{
    return make_distance_value<int>( lhs, rhs, distance_call{ distance_call::kind::square } );
}

auto lua_point_coord_rl_dist( const lua_point_coord &lhs,
                              const lua_point_coord &rhs ) -> std::optional<int>
{
    return lua_point_coord_distance<int>( lhs, rhs, distance_call{ distance_call::kind::rl } );
}

auto lua_point_coord_trig_dist( const lua_point_coord &lhs,
                                const lua_point_coord &rhs ) -> std::optional<double>
{
    return lua_point_coord_distance<double>( lhs, rhs, distance_call{ distance_call::kind::trig } );
}

auto lua_point_coord_square_dist( const lua_point_coord &lhs,
                                  const lua_point_coord &rhs ) -> std::optional<int>
{
    return lua_point_coord_distance<int>( lhs, rhs, distance_call{ distance_call::kind::square } );
}

auto lua_tripoint_coord_rl_dist( const lua_tripoint_coord &lhs,
                                 const lua_tripoint_coord &rhs ) -> std::optional<int>
{
    return lua_tripoint_coord_distance<int>( lhs, rhs, distance_call{ distance_call::kind::rl } );
}

auto lua_tripoint_coord_trig_dist( const lua_tripoint_coord &lhs,
                                   const lua_tripoint_coord &rhs ) -> std::optional<double>
{
    return lua_tripoint_coord_distance<double>( lhs, rhs, distance_call{ distance_call::kind::trig } );
}

auto lua_tripoint_coord_square_dist( const lua_tripoint_coord &lhs,
                                     const lua_tripoint_coord &rhs ) -> std::optional<int>
{
    return lua_tripoint_coord_distance<int>( lhs, rhs, distance_call{ distance_call::kind::square } );
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

template<typename Coord>
auto bind_named_point_constructor( sol::state_view lua, const std::string_view name ) -> void
{
    auto lib = luna::begin_lib( lua, name );
    luna::set_fx( lib, "new", sol::overload(
                      []() -> Coord { return Coord::zero(); },
                      []( const point & raw ) -> Coord { return Coord( raw ); },
                      []( const int x, const int y ) -> Coord { return Coord( x, y ); }
                  ) );
    luna::finalize_lib( lib );
}

template<typename Coord>
auto bind_named_tripoint_constructor( sol::state_view lua, const std::string_view name ) -> void
{
    using point_coord = coords::coord_point<point, Coord::origin_tag, Coord::scale_tag>;
    auto lib = luna::begin_lib( lua, name );
    luna::set_fx( lib, "new", sol::overload(
                      []() -> Coord { return Coord::zero(); },
                      []( const tripoint & raw ) -> Coord { return Coord( raw ); },
                      []( const point & xy, const int z ) -> Coord { return Coord( tripoint( xy, z ) ); },
                      []( const point_coord & xy, const int z ) -> Coord { return Coord( xy, z ); },
                      []( const int x, const int y, const int z ) -> Coord { return Coord( x, y, z ); }
                  ) );
    luna::finalize_lib( lib );
}

auto bind_named_coord_constructors( sol::state_view lua ) -> void
{
#define BIND_NAMED_COORD_CONSTRUCTORS( point_type, tripoint_type, point_name, tripoint_name ) \
    bind_named_point_constructor<point_type>( lua, point_name ); \
    bind_named_tripoint_constructor<tripoint_type>( lua, tripoint_name )

    BIND_NAMED_COORD_CONSTRUCTORS( point_rel_ms, tripoint_rel_ms, "PointRelMs", "TripointRelMs" );
    BIND_NAMED_COORD_CONSTRUCTORS( point_abs_ms, tripoint_abs_ms, "PointAbsMs", "TripointAbsMs" );
    BIND_NAMED_COORD_CONSTRUCTORS( point_bub_ms, tripoint_bub_ms, "PointBubMs", "TripointBubMs" );
    BIND_NAMED_COORD_CONSTRUCTORS( point_sm_ms, tripoint_sm_ms, "PointSmMs", "TripointSmMs" );
    BIND_NAMED_COORD_CONSTRUCTORS( point_omt_ms, tripoint_omt_ms, "PointOmtMs", "TripointOmtMs" );
    BIND_NAMED_COORD_CONSTRUCTORS( point_mmr_ms, tripoint_mmr_ms, "PointMmrMs", "TripointMmrMs" );
    BIND_NAMED_COORD_CONSTRUCTORS( point_seg_ms, tripoint_seg_ms, "PointSegMs", "TripointSegMs" );
    BIND_NAMED_COORD_CONSTRUCTORS( point_om_ms, tripoint_om_ms, "PointOmMs", "TripointOmMs" );
    BIND_NAMED_COORD_CONSTRUCTORS( point_rel_veh, tripoint_rel_veh, "PointRelVeh", "TripointRelVeh" );
    BIND_NAMED_COORD_CONSTRUCTORS( point_mnt_veh, tripoint_mnt_veh, "PointMntVeh", "TripointMntVeh" );
    BIND_NAMED_COORD_CONSTRUCTORS( point_rel_sm, tripoint_rel_sm, "PointRelSm", "TripointRelSm" );
    BIND_NAMED_COORD_CONSTRUCTORS( point_abs_sm, tripoint_abs_sm, "PointAbsSm", "TripointAbsSm" );
    BIND_NAMED_COORD_CONSTRUCTORS( point_bub_sm, tripoint_bub_sm, "PointBubSm", "TripointBubSm" );
    BIND_NAMED_COORD_CONSTRUCTORS( point_omt_sm, tripoint_omt_sm, "PointOmtSm", "TripointOmtSm" );
    BIND_NAMED_COORD_CONSTRUCTORS( point_mmr_sm, tripoint_mmr_sm, "PointMmrSm", "TripointMmrSm" );
    BIND_NAMED_COORD_CONSTRUCTORS( point_seg_sm, tripoint_seg_sm, "PointSegSm", "TripointSegSm" );
    BIND_NAMED_COORD_CONSTRUCTORS( point_om_sm, tripoint_om_sm, "PointOmSm", "TripointOmSm" );
    BIND_NAMED_COORD_CONSTRUCTORS( point_rel_omt, tripoint_rel_omt, "PointRelOmt", "TripointRelOmt" );
    BIND_NAMED_COORD_CONSTRUCTORS( point_abs_omt, tripoint_abs_omt, "PointAbsOmt", "TripointAbsOmt" );
    BIND_NAMED_COORD_CONSTRUCTORS( point_mmr_omt, tripoint_mmr_omt, "PointMmrOmt", "TripointMmrOmt" );
    BIND_NAMED_COORD_CONSTRUCTORS( point_seg_omt, tripoint_seg_omt, "PointSegOmt", "TripointSegOmt" );
    BIND_NAMED_COORD_CONSTRUCTORS( point_om_omt, tripoint_om_omt, "PointOmOmt", "TripointOmOmt" );
    BIND_NAMED_COORD_CONSTRUCTORS( point_rel_mmr, tripoint_rel_mmr, "PointRelMmr", "TripointRelMmr" );
    BIND_NAMED_COORD_CONSTRUCTORS( point_abs_mmr, tripoint_abs_mmr, "PointAbsMmr", "TripointAbsMmr" );
    BIND_NAMED_COORD_CONSTRUCTORS( point_seg_mmr, tripoint_seg_mmr, "PointSegMmr", "TripointSegMmr" );
    BIND_NAMED_COORD_CONSTRUCTORS( point_om_mmr, tripoint_om_mmr, "PointOmMmr", "TripointOmMmr" );
    BIND_NAMED_COORD_CONSTRUCTORS( point_rel_seg, tripoint_rel_seg, "PointRelSeg", "TripointRelSeg" );
    BIND_NAMED_COORD_CONSTRUCTORS( point_abs_seg, tripoint_abs_seg, "PointAbsSeg", "TripointAbsSeg" );
    BIND_NAMED_COORD_CONSTRUCTORS( point_om_seg, tripoint_om_seg, "PointOmSeg", "TripointOmSeg" );
    BIND_NAMED_COORD_CONSTRUCTORS( point_rel_om, tripoint_rel_om, "PointRelOm", "TripointRelOm" );
    BIND_NAMED_COORD_CONSTRUCTORS( point_abs_om, tripoint_abs_om, "PointAbsOm", "TripointAbsOm" );

#undef BIND_NAMED_COORD_CONSTRUCTORS
}

template<typename Range, typename Maker>
auto make_point_coord_vector( Range &&range, Maker maker ) -> std::vector<lua_point_coord>
{
    auto result = std::vector<lua_point_coord> {};
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

    DOC( "Projects a PointCoord or TripointCoord to another scale, preserving its origin and point dimension. Returns nil if the conversion is not valid." );
    DOC_PARAMS( "coord", "scale" );
    luna::set_fx( lib, "project_to", &lua_coords::lua_project_to );
    DOC( "Shortcut for `project_to(coord, \"ms\")`." );
    DOC_PARAMS( "coord" );
    luna::set_fx( lib, "project_to_ms", []( const sol::object & val ) {
        return lua_coords::lua_project_to( val, "ms" );
    } );
    DOC( "Shortcut for `project_to(coord, \"veh\")`." );
    DOC_PARAMS( "coord" );
    luna::set_fx( lib, "project_to_veh", []( const sol::object & val ) {
        return lua_coords::lua_project_to( val, "veh" );
    } );
    DOC( "Shortcut for `project_to(coord, \"sm\")`." );
    DOC_PARAMS( "coord" );
    luna::set_fx( lib, "project_to_sm", []( const sol::object & val ) {
        return lua_coords::lua_project_to( val, "sm" );
    } );
    DOC( "Shortcut for `project_to(coord, \"omt\")`." );
    DOC_PARAMS( "coord" );
    luna::set_fx( lib, "project_to_omt", []( const sol::object & val ) {
        return lua_coords::lua_project_to( val, "omt" );
    } );
    DOC( "Shortcut for `project_to(coord, \"mmr\")`." );
    DOC_PARAMS( "coord" );
    luna::set_fx( lib, "project_to_mmr", []( const sol::object & val ) {
        return lua_coords::lua_project_to( val, "mmr" );
    } );
    DOC( "Shortcut for `project_to(coord, \"seg\")`." );
    DOC_PARAMS( "coord" );
    luna::set_fx( lib, "project_to_seg", []( const sol::object & val ) {
        return lua_coords::lua_project_to( val, "seg" );
    } );
    DOC( "Shortcut for `project_to(coord, \"om\")`." );
    DOC_PARAMS( "coord" );
    luna::set_fx( lib, "project_to_om", []( const sol::object & val ) {
        return lua_coords::lua_project_to( val, "om" );
    } );

    DOC( "Splits a PointCoord or TripointCoord into a coarser coordinate plus a remainder. Point input returns PointCoord, PointCoord; tripoint input returns TripointCoord, PointCoord. Returns nil, nil if the split is not valid." );
    DOC_PARAMS( "coord", "scale" );
    luna::set_fx( lib, "project_remain", &lua_coords::lua_project_remain_to );
    DOC( "Shortcut for `project_remain(coord, \"sm\")`." );
    DOC_PARAMS( "coord" );
    luna::set_fx( lib, "project_remain_sm", []( const sol::object & val ) {
        return lua_coords::lua_project_remain_to( val, "sm" );
    } );
    DOC( "Shortcut for `project_remain(coord, \"omt\")`." );
    DOC_PARAMS( "coord" );
    luna::set_fx( lib, "project_remain_omt", []( const sol::object & val ) {
        return lua_coords::lua_project_remain_to( val, "omt" );
    } );
    DOC( "Shortcut for `project_remain(coord, \"mmr\")`." );
    DOC_PARAMS( "coord" );
    luna::set_fx( lib, "project_remain_mmr", []( const sol::object & val ) {
        return lua_coords::lua_project_remain_to( val, "mmr" );
    } );
    DOC( "Shortcut for `project_remain(coord, \"seg\")`." );
    DOC_PARAMS( "coord" );
    luna::set_fx( lib, "project_remain_seg", []( const sol::object & val ) {
        return lua_coords::lua_project_remain_to( val, "seg" );
    } );
    DOC( "Shortcut for `project_remain(coord, \"om\")`." );
    DOC_PARAMS( "coord" );
    luna::set_fx( lib, "project_remain_om", []( const sol::object & val ) {
        return lua_coords::lua_project_remain_to( val, "om" );
    } );
    DOC( "Combines a coarse coordinate with a remainder from project_remain. Returns PointCoord or TripointCoord depending on the inputs, or nil if the pair is incompatible." );
    DOC_PARAMS( "coarse", "fine" );
    luna::set_fx( lib, "project_combine",
                  sol::resolve<lua_coords::lua_coord_result( const sol::object &, const sol::object & )>
                  ( &lua_coords::lua_project_combine ) );
    DOC( "Rectilinear distance between two raw coordinates, or between two typed coordinates with matching origin and scale. Returns nil if the typed coordinates are incompatible." );
    DOC_PARAMS( "lhs", "rhs" );
    luna::set_fx( lib, "rl_dist", sol::overload(
                      &lua_coords::raw_point_rl_dist,
                      &lua_coords::raw_tripoint_rl_dist,
                      &lua_coords::lua_point_coord_rl_dist,
                      &lua_coords::lua_tripoint_coord_rl_dist
                  ) );
    DOC( "Euclidean distance between two raw coordinates, or between two typed coordinates with matching origin and scale. Returns nil if the typed coordinates are incompatible." );
    DOC_PARAMS( "lhs", "rhs" );
    luna::set_fx( lib, "trig_dist", sol::overload(
                      &lua_coords::raw_point_trig_dist,
                      &lua_coords::raw_tripoint_trig_dist,
                      &lua_coords::lua_point_coord_trig_dist,
                      &lua_coords::lua_tripoint_coord_trig_dist
                  ) );
    DOC( "Chebyshev distance between two raw coordinates, or between two typed coordinates with matching origin and scale. Returns nil if the typed coordinates are incompatible." );
    DOC_PARAMS( "lhs", "rhs" );
    luna::set_fx( lib, "square_dist", sol::overload(
                      &lua_coords::raw_point_square_dist,
                      &lua_coords::raw_tripoint_square_dist,
                      &lua_coords::lua_point_coord_square_dist,
                      &lua_coords::lua_tripoint_coord_square_dist
                  ) );

    DOC( "Returns every map-square offset within one submap as PointCoord values." );
    luna::set_fx( lib, "submap_tiles", &lua_coords::make_submap_tiles );
    DOC( "Returns every map-square offset within the tinymap as PointCoord values." );
    luna::set_fx( lib, "tinymap_tiles", &lua_coords::make_tinymap_tiles );
    DOC( "Returns every map-square offset within one overmap terrain tile as PointCoord values." );
    luna::set_fx( lib, "overmap_terrain_tiles", &lua_coords::make_overmap_terrain_tiles );
    DOC( "Returns every map-square offset within one overmap as PointCoord values." );
    luna::set_fx( lib, "overmap_tiles", &lua_coords::make_overmap_tiles );

    luna::finalize_lib( lib );
    lua_coords::bind_named_coord_constructors( lua_view );
}
