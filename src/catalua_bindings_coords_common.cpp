#include "catalua_bindings_coords_common.h"

#include "line.h"
#include "string_formatter.h"

#include <stdexcept>

namespace cata::detail::lua_coords
{

auto origin_lua_name( const coords::origin origin ) -> std::string_view
{
    switch( origin ) {
        case coords::origin::relative:
            return "rel";
        case coords::origin::abs:
            return "abs";
        case coords::origin::bubble:
            return "bub";
        case coords::origin::vehicle:
            return "mnt";
        case coords::origin::submap:
            return "sm";
        case coords::origin::overmap_terrain:
            return "omt";
        case coords::origin::overmap:
            return "om";
        case coords::origin::segment:
            return "seg";
        case coords::origin::mem_map_region:
            return "mmr";
    }
    return "unknown";
}

auto scale_lua_name( const coords::scale scale ) -> std::string_view
{
    switch( scale ) {
        case coords::scale::map_square:
            return "ms";
        case coords::scale::vehicle:
            return "veh";
        case coords::scale::submap:
            return "sm";
        case coords::scale::overmap_terrain:
            return "omt";
        case coords::scale::segment:
            return "seg";
        case coords::scale::overmap:
            return "om";
        case coords::scale::mem_map_region:
            return "mmr";
    }
    return "unknown";
}

auto origin_type_name( const coords::origin origin ) -> std::string_view
{
    switch( origin ) {
        case coords::origin::relative:
            return "Rel";
        case coords::origin::abs:
            return "Abs";
        case coords::origin::bubble:
            return "Bub";
        case coords::origin::vehicle:
            return "Mnt";
        case coords::origin::submap:
            return "Sm";
        case coords::origin::overmap_terrain:
            return "Omt";
        case coords::origin::overmap:
            return "Om";
        case coords::origin::segment:
            return "Seg";
        case coords::origin::mem_map_region:
            return "Mmr";
    }
    return "Unknown";
}

auto scale_type_name( const coords::scale scale ) -> std::string_view
{
    switch( scale ) {
        case coords::scale::map_square:
            return "Ms";
        case coords::scale::vehicle:
            return "Veh";
        case coords::scale::submap:
            return "Sm";
        case coords::scale::overmap_terrain:
            return "Omt";
        case coords::scale::segment:
            return "Seg";
        case coords::scale::overmap:
            return "Om";
        case coords::scale::mem_map_region:
            return "Mmr";
    }
    return "Unknown";
}

auto parse_origin( const std::string_view name ) -> std::optional<coords::origin>
{
    if( name == "relative" || name == "rel" ) {
        return coords::origin::relative;
    }
    if( name == "abs" || name == "absolute" ) {
        return coords::origin::abs;
    }
    if( name == "bubble" || name == "bub" ) {
        return coords::origin::bubble;
    }
    if( name == "vehicle" || name == "mnt" ) {
        return coords::origin::vehicle;
    }
    if( name == "submap" || name == "sm" ) {
        return coords::origin::submap;
    }
    if( name == "overmap_terrain" || name == "omt" ) {
        return coords::origin::overmap_terrain;
    }
    if( name == "overmap" || name == "om" ) {
        return coords::origin::overmap;
    }
    if( name == "segment" || name == "seg" ) {
        return coords::origin::segment;
    }
    if( name == "mem_map_region" || name == "mmr" ) {
        return coords::origin::mem_map_region;
    }
    return std::nullopt;
}

auto parse_scale( const std::string_view name ) -> std::optional<coords::scale>
{
    if( name == "map_square" || name == "ms" ) {
        return coords::scale::map_square;
    }
    if( name == "vehicle" || name == "veh" ) {
        return coords::scale::vehicle;
    }
    if( name == "submap" || name == "sm" ) {
        return coords::scale::submap;
    }
    if( name == "overmap_terrain" || name == "omt" ) {
        return coords::scale::overmap_terrain;
    }
    if( name == "segment" || name == "seg" ) {
        return coords::scale::segment;
    }
    if( name == "overmap" || name == "om" ) {
        return coords::scale::overmap;
    }
    if( name == "mem_map_region" || name == "mmr" ) {
        return coords::scale::mem_map_region;
    }
    return std::nullopt;
}

auto is_registered_coord( const coords::origin origin, const coords::scale scale ) -> bool
{
    switch( origin ) {
        case coords::origin::relative:
            return true;
        case coords::origin::abs:
            return scale != coords::scale::vehicle;
        case coords::origin::bubble:
            return scale == coords::scale::map_square || scale == coords::scale::submap;
        case coords::origin::vehicle:
            return scale == coords::scale::vehicle;
        case coords::origin::submap:
            return scale == coords::scale::map_square;
        case coords::origin::overmap_terrain:
            return scale == coords::scale::map_square || scale == coords::scale::submap;
        case coords::origin::mem_map_region:
            return scale == coords::scale::map_square || scale == coords::scale::submap ||
                   scale == coords::scale::overmap_terrain;
        case coords::origin::segment:
            return scale == coords::scale::map_square || scale == coords::scale::submap ||
                   scale == coords::scale::overmap_terrain ||
                   scale == coords::scale::mem_map_region;
        case coords::origin::overmap:
            return scale == coords::scale::map_square || scale == coords::scale::submap ||
                   scale == coords::scale::overmap_terrain ||
                   scale == coords::scale::mem_map_region || scale == coords::scale::segment;
    }
    return false;
}

auto has_remainder_origin( const coords::scale scale ) -> bool
{
    switch( scale ) {
        case coords::scale::submap:
        case coords::scale::overmap_terrain:
        case coords::scale::mem_map_region:
        case coords::scale::segment:
        case coords::scale::overmap:
            return true;
        default:
            return false;
    }
}

auto exact_scale_conversion( const coords::scale source, const coords::scale result ) -> bool
{
    const auto source_squares = coords::map_squares_per( source );
    const auto result_squares = coords::map_squares_per( result );
    if( source_squares > result_squares ) {
        return source_squares % result_squares == 0;
    }
    return result_squares % source_squares == 0;
}

auto can_project_to( const coords::origin origin, const coords::scale source,
                     const coords::scale result ) -> bool
{
    return source != result && exact_scale_conversion( source, result ) &&
           is_registered_coord( origin, result );
}

auto can_project_remain( const coords::origin origin, const coords::scale source,
                         const coords::scale result ) -> bool
{
    const auto source_squares = coords::map_squares_per( source );
    const auto result_squares = coords::map_squares_per( result );
    return result_squares > source_squares && result_squares % source_squares == 0 &&
           has_remainder_origin( result ) && is_registered_coord( origin, result ) &&
           is_registered_coord( coords::origin_from_scale( result ), source );
}

auto can_project_combine( const project_combine_check &check ) -> bool
{
    const auto coarse_squares = coords::map_squares_per( check.coarse_scale );
    const auto fine_squares = coords::map_squares_per( check.fine_scale );
    return has_remainder_origin( check.coarse_scale ) && coarse_squares > fine_squares &&
           coarse_squares % fine_squares == 0 &&
           coords::origin_from_scale( check.coarse_scale ) == check.fine_origin &&
           !( check.coarse_is_tripoint && check.fine_is_tripoint ) &&
           is_registered_coord( check.coarse_origin, check.fine_scale );
}

auto coord_type_name( const bool is_tripoint, const coords::origin origin,
                      const coords::scale scale ) -> std::string
{
    auto result = std::string( is_tripoint ? "Tripoint" : "Point" );
    result += origin_type_name( origin );
    result += scale_type_name( scale );
    return result;
}

auto point_to_string( const lua_point_coord &coord ) -> std::string
{
    return string_format( "%s(%d,%d)", coord_type_name( false, coord.origin, coord.scale ),
                          coord.raw.x, coord.raw.y );
}

auto tripoint_to_string( const lua_tripoint_coord &coord ) -> std::string
{
    return string_format( "%s(%d,%d,%d)", coord_type_name( true, coord.origin, coord.scale ),
                          coord.raw.x, coord.raw.y, coord.raw.z );
}

auto make_point_coord( const coords::origin origin, const coords::scale scale,
                       const point &raw ) -> lua_point_coord
{
    if( !is_registered_coord( origin, scale ) ) {
        throw std::runtime_error( string_format( "%s%s is not a supported point coordinate type",
                                  origin_type_name( origin ), scale_type_name( scale ) ) );
    }
    return lua_point_coord{ raw, origin, scale };
}

auto make_tripoint_coord( const coords::origin origin, const coords::scale scale,
                          const tripoint &raw ) -> lua_tripoint_coord
{
    if( !is_registered_coord( origin, scale ) ) {
        throw std::runtime_error( string_format( "%s%s is not a supported tripoint coordinate type",
                                  origin_type_name( origin ), scale_type_name( scale ) ) );
    }
    return lua_tripoint_coord{ raw, origin, scale };
}

auto make_point_coord( const std::string &origin_name, const std::string &scale_name,
                       const point &raw ) -> lua_point_coord
{
    const auto origin = parse_origin( origin_name );
    const auto scale = parse_scale( scale_name );
    if( !origin || !scale ) {
        throw std::runtime_error( string_format( "Invalid coordinate kind %s/%s",
                                  origin_name, scale_name ) );
    }
    return make_point_coord( *origin, *scale, raw );
}

auto make_tripoint_coord( const std::string &origin_name, const std::string &scale_name,
                          const tripoint &raw ) -> lua_tripoint_coord
{
    const auto origin = parse_origin( origin_name );
    const auto scale = parse_scale( scale_name );
    if( !origin || !scale ) {
        throw std::runtime_error( string_format( "Invalid coordinate kind %s/%s",
                                  origin_name, scale_name ) );
    }
    return make_tripoint_coord( *origin, *scale, raw );
}

auto project_xy( const point &raw, const coords::scale source,
                 const coords::scale result ) -> point
{
    const auto source_squares = coords::map_squares_per( source );
    const auto result_squares = coords::map_squares_per( result );
    if( source_squares > result_squares ) {
        return multiply_xy( raw, source_squares / result_squares );
    }
    if( result_squares > source_squares ) {
        return divide_xy_round_to_minus_infinity( raw, result_squares / source_squares );
    }
    return raw;
}

auto project_tripoint_raw( const tripoint &raw, const coords::scale source,
                           const coords::scale result ) -> tripoint
{
    const auto xy = project_xy( raw.xy(), source, result );
    return tripoint( xy, raw.z );
}

auto project_to( const lua_point_coord &coord,
                 const coords::scale result ) -> std::optional<lua_point_coord>
{
    if( !can_project_to( coord.origin, coord.scale, result ) ) {
        return std::nullopt;
    }
    return make_point_coord( coord.origin, result, project_xy( coord.raw, coord.scale, result ) );
}

auto project_to( const lua_tripoint_coord &coord,
                 const coords::scale result ) -> std::optional<lua_tripoint_coord>
{
    if( !can_project_to( coord.origin, coord.scale, result ) ) {
        return std::nullopt;
    }
    return make_tripoint_coord( coord.origin, result,
                                project_tripoint_raw( coord.raw, coord.scale, result ) );
}

auto same_coord_kind( const lua_point_coord &lhs, const lua_point_coord &rhs ) -> bool
{
    return lhs.origin == rhs.origin && lhs.scale == rhs.scale;
}

auto same_coord_kind( const lua_tripoint_coord &lhs,
                      const lua_tripoint_coord &rhs ) -> bool
{
    return lhs.origin == rhs.origin && lhs.scale == rhs.scale;
}

} // namespace cata::detail::lua_coords
