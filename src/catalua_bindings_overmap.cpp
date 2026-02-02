#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include "catalua_bindings.h"
#include "catalua.h"
#include "catalua_bindings_utils.h"
#include "catalua_luna.h"
#include "catalua_luna_doc.h"

#include "coordinates.h"
#include "enums.h"
#include "overmap_types.h"
#include "overmapbuffer.h"
#include "type_id.h"

void cata::detail::reg_overmap( sol::state &lua )
{
    // Register overmapbuffer class
    {
        DOC( "Global overmap buffer that manages all overmap data" );
        sol::usertype<overmapbuffer> ut =
            luna::new_usertype<overmapbuffer>(
                lua,
                luna::no_bases,
                luna::no_constructor
            );

        DOC( "Get all overmap tiles belonging to the electric grid at the given position" );
        luna::set_fx( ut, "electric_grid_at",
        []( overmapbuffer & buf, const tripoint & p ) -> std::vector<tripoint> {
            return buf.electric_grid_at( tripoint_abs_omt( p ) )
            | std::views::transform( []( const auto & p ) { return p.raw(); } )
            | std::ranges::to<std::vector<tripoint>>();
        } );

        DOC( "Get all electric grid connections from the given position" );
        luna::set_fx( ut, "electric_grid_connectivity_at",
        []( overmapbuffer & buf, const tripoint & p ) -> std::vector<tripoint> {
            return buf.electric_grid_connectivity_at( tripoint_abs_omt( p ) )
            | std::views::transform( []( const auto & p ) { return p.raw(); } )
            | std::ranges::to<std::vector<tripoint>>();
        } );

        DOC( "Add an electric grid connection between two positions" );
        luna::set_fx( ut, "add_grid_connection",
        []( overmapbuffer & buf, const tripoint & lhs, const tripoint & rhs ) -> bool {
            return buf.add_grid_connection( tripoint_abs_omt( lhs ), tripoint_abs_omt( rhs ) );
        } );

        DOC( "Remove an electric grid connection between two positions" );
        luna::set_fx( ut, "remove_grid_connection",
        []( overmapbuffer & buf, const tripoint & lhs, const tripoint & rhs ) -> bool {
            return buf.remove_grid_connection( tripoint_abs_omt( lhs ), tripoint_abs_omt( rhs ) );
        } );
    }

    // Register omt_find_params struct
#define UT_CLASS omt_find_params
    {
        sol::usertype<UT_CLASS> ut =
            luna::new_usertype<UT_CLASS>(
                lua,
                luna::no_bases,
                luna::constructors <
                omt_find_params()
                > ()
            );

        DOC( "Vector of (terrain_type, match_type) pairs to search for." );
        SET_MEMB( types );
        DOC( "Vector of (terrain_type, match_type) pairs to exclude from search." );
        SET_MEMB( exclude_types );
        DOC( "If set, filters by terrain seen status (true = seen only, false = unseen only)." );
        SET_MEMB( seen );
        DOC( "If set, filters by terrain explored status (true = explored only, false = unexplored only)." );
        SET_MEMB( explored );
        DOC( "If true, restricts search to existing overmaps only." );
        SET_MEMB( existing_only );
        // NOTE: om_special field omitted - requires overmap_special type to have comparison operators
        // TODO: Add om_special field after implementing comparison operators for overmap_special
        DOC( "If set, limits the number of results returned." );
        SET_MEMB( max_results );
        // NOTE: force_sync field omitted - automatically set to true in Lua bindings for thread safety

        DOC( "Helper method to add a terrain type to search for." );
        luna::set_fx( ut, "add_type",
        []( omt_find_params & p, const std::string & type, ot_match_type match ) -> void {
            p.types.emplace_back( type, match );
        } );

        DOC( "Helper method to add a terrain type to exclude from search." );
        luna::set_fx( ut, "add_exclude_type",
        []( omt_find_params & p, const std::string & type, ot_match_type match ) -> void {
            p.exclude_types.emplace_back( type, match );
        } );

        DOC( "Set the search range in overmap tiles (min, max)." );
        luna::set_fx( ut, "set_search_range",
        []( omt_find_params & p, int min, int max ) -> void {
            p.search_range = { min, max };
        } );

        DOC( "Set the search layer range (z-levels)." );
        luna::set_fx( ut, "set_search_layers",
        []( omt_find_params & p, int min, int max ) -> void {
            p.search_layers = std::make_pair( min, max );
        } );
    }
#undef UT_CLASS

    // Register overmapbuffer global library
    DOC( "Global overmap buffer interface for finding and inspecting overmap terrain." );
    luna::userlib lib = luna::begin_lib( lua, "overmapbuffer" );

    // Finding methods
    DOC( "Find all overmap terrain tiles matching the given parameters. Returns a vector of tripoints." );
    luna::set_fx( lib, "find_all",
    []( const tripoint & origin, omt_find_params params ) -> std::vector<tripoint> {
        params.force_sync = true;
        return overmap_buffer.find_all( tripoint_abs_omt( origin ), params )
        | std::views::transform( []( const auto & p ) { return p.raw(); } )
        | std::ranges::to<std::vector<tripoint>>();
    } );

    DOC( "Find the closest overmap terrain tile matching the given parameters. Returns a tripoint or nil if not found." );
    luna::set_fx( lib, "find_closest",
    []( const tripoint & origin, omt_find_params params ) -> sol::optional<tripoint> {
        params.force_sync = true;
        tripoint_abs_omt result = overmap_buffer.find_closest( tripoint_abs_omt( origin ), params );
        if( result == tripoint_abs_omt( tripoint_min ) )
        {
            return sol::nullopt;
        }
        return result.raw();
    } );

    DOC( "Find a random overmap terrain tile matching the given parameters. Returns a tripoint or nil if not found." );
    luna::set_fx( lib, "find_random",
    []( const tripoint & origin, omt_find_params params ) -> sol::optional<tripoint> {
        params.force_sync = true;
        tripoint_abs_omt result = overmap_buffer.find_random( tripoint_abs_omt( origin ), params );
        if( result == tripoint_abs_omt( tripoint_min ) )
        {
            return sol::nullopt;
        }
        return result.raw();
    } );

    // Terrain inspection methods
    DOC( "Get the overmap terrain type at the given position. Returns an oter_id." );
    luna::set_fx( lib, "ter",
                  []( const tripoint & p ) -> oter_id { return overmap_buffer.ter( tripoint_abs_omt( p ) ); } );

    DOC( "Check if the terrain at the given position matches the type and match mode. Returns boolean." );
    luna::set_fx( lib, "check_ot",
    []( const std::string & otype, ot_match_type match_type, const tripoint & p ) -> bool {
        return overmap_buffer.check_ot( otype, match_type, tripoint_abs_omt( p ) );
    } );

    // Visibility methods
    DOC( "Check if the terrain at the given position has been seen by the player. Returns boolean." );
    luna::set_fx( lib, "seen",
    []( const tripoint & p ) -> bool {
        return overmap_buffer.seen( tripoint_abs_omt( p ) );
    } );

    DOC( "Set the seen status of terrain at the given position." );
    luna::set_fx( lib, "set_seen",
    []( const tripoint & p, sol::optional<bool> seen_val ) -> void {
        overmap_buffer.set_seen( tripoint_abs_omt( p ), seen_val.value_or( true ) );
    } );

    DOC( "Check if the terrain at the given position has been explored by the player. Returns boolean." );
    luna::set_fx( lib, "is_explored",
    []( const tripoint & p ) -> bool {
        return overmap_buffer.is_explored( tripoint_abs_omt( p ) );
    } );

    // Electric grid methods
    DOC( "Get all overmap tiles belonging to the electric grid at the given position. Returns vector of tripoints." );
    luna::set_fx( lib, "electric_grid_at",
    []( const tripoint & p ) -> std::vector<tripoint> {
        return overmap_buffer.electric_grid_at( tripoint_abs_omt( p ) )
        | std::views::transform( []( const auto & p ) { return p.raw(); } )
        | std::ranges::to<std::vector<tripoint>>();
    } );

    DOC( "Get all electric grid connections from the given position. Returns vector of relative tripoint offsets." );
    luna::set_fx( lib, "electric_grid_connectivity_at",
    []( const tripoint & p ) -> std::vector<tripoint> {
        return overmap_buffer.electric_grid_connectivity_at( tripoint_abs_omt( p ) )
        | std::views::transform( []( const auto & p ) { return p.raw(); } )
        | std::ranges::to<std::vector<tripoint>>();
    } );

    DOC( "Add an electric grid connection between two positions. Returns true on success." );
    luna::set_fx( lib, "add_grid_connection",
    []( const tripoint & lhs, const tripoint & rhs ) -> bool {
        return overmap_buffer.add_grid_connection( tripoint_abs_omt( lhs ), tripoint_abs_omt( rhs ) );
    } );

    DOC( "Remove an electric grid connection between two positions. Returns true on success." );
    luna::set_fx( lib, "remove_grid_connection",
    []( const tripoint & lhs, const tripoint & rhs ) -> bool {
        return overmap_buffer.remove_grid_connection( tripoint_abs_omt( lhs ), tripoint_abs_omt( rhs ) );
    } );

    luna::finalize_lib( lib );
}
