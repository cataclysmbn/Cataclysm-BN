#include "boundary_section.h"

#include <limits>

#include "coordinate_conversions.h"
#include "coordinates.h"
#include "debug.h"
#include "game_constants.h"
#include "json.h"
#include "mapbuffer.h"
#include "mapdata.h"
#include "mapgen.h"
#include "mapgendata.h"
#include "mapgen_functions.h"
#include "omdata.h"
#include "overmap_special.h"
#include "submap.h"

void boundary_section_id::serialize( JsonOut &json ) const
{
    json.write( value );
}

void boundary_section_id::deserialize( JsonIn &jsin )
{
    value = jsin.get_int();
}

bool boundary_bounds::contains( const tripoint_abs_omt &p ) const
{
    return p.x() >= min.x() && p.x() < max.x() &&
           p.y() >= min.y() && p.y() < max.y() &&
           p.z() >= min.z() && p.z() < max.z();
}

bool boundary_bounds::contains( const tripoint_abs_sm &p ) const
{
    tripoint_abs_omt omt_p = project_to<coords::omt>( p );
    return contains( omt_p );
}

bool boundary_bounds::contains( const tripoint_abs_ms &p ) const
{
    tripoint_abs_omt omt_p = project_to<coords::omt>( p );
    return contains( omt_p );
}

bool boundary_bounds::intersects( const boundary_bounds &other ) const
{
    // AABB intersection test
    return !( max.x() <= other.min.x() || other.max.x() <= min.x() ||
              max.y() <= other.min.y() || other.max.y() <= min.y() ||
              max.z() <= other.min.z() || other.max.z() <= min.z() );
}

boundary_bounds boundary_bounds::interior() const
{
    boundary_bounds result;
    result.min = min + tripoint( border_width_omt, border_width_omt, border_width_omt );
    result.max = max - tripoint( border_width_omt, border_width_omt, border_width_omt );
    result.border_width_omt = 0;  // Interior has no border of its own
    return result;
}

bool boundary_bounds::is_in_border( const tripoint_abs_omt &p ) const
{
    if( !contains( p ) ) {
        return false;
    }
    // Check if within border_width_omt of any edge
    return p.x() < min.x() + border_width_omt ||
           p.x() >= max.x() - border_width_omt ||
           p.y() < min.y() + border_width_omt ||
           p.y() >= max.y() - border_width_omt ||
           p.z() < min.z() + border_width_omt ||
           p.z() >= max.z() - border_width_omt;
}

bool boundary_bounds::is_in_border( const tripoint_abs_sm &p ) const
{
    tripoint_abs_omt omt_p = project_to<coords::omt>( p );
    return is_in_border( omt_p );
}

bool boundary_bounds::is_in_border( const tripoint_abs_ms &p ) const
{
    tripoint_abs_omt omt_p = project_to<coords::omt>( p );
    return is_in_border( omt_p );
}

tripoint boundary_bounds::size() const
{
    return tripoint( max.x() - min.x(), max.y() - min.y(), max.z() - min.z() );
}

tripoint boundary_bounds::interior_size() const
{
    tripoint full = size();
    int border2 = border_width_omt * 2;
    return tripoint( full.x - border2, full.y - border2, full.z - border2 );
}

void boundary_bounds::serialize( JsonOut &json ) const
{
    json.start_object();
    json.member( "min", min );
    json.member( "max", max );
    json.member( "border_width_omt", border_width_omt );
    json.end_object();
}

void boundary_bounds::deserialize( JsonIn &jsin )
{
    JsonObject data = jsin.get_object();
    data.read( "min", min );
    data.read( "max", max );
    // Use default if not present (for backward compatibility)
    border_width_omt = data.get_int( "border_width_omt", DEFAULT_BORDER_WIDTH_OMT );
}

void boundary_section_mapgen::serialize( JsonOut &json ) const
{
    json.start_object();
    if( !mapgen_id.empty() ) {
        json.member( "mapgen_id", mapgen_id );
    }
    if( special_id.has_value() ) {
        json.member( "special_id", special_id->str() );
        json.member( "special_offset", special_offset );
    }
    json.end_object();
}

void boundary_section_mapgen::deserialize( JsonIn &jsin )
{
    JsonObject data = jsin.get_object();
    if( data.has_string( "mapgen_id" ) ) {
        data.read( "mapgen_id", mapgen_id );
    }
    if( data.has_string( "special_id" ) ) {
        special_id = overmap_special_id( data.get_string( "special_id" ) );
        data.read( "special_offset", special_offset );
    } else {
        special_id = std::nullopt;
        special_offset = tripoint_zero;
    }
}

bool boundary_section::contains( const tripoint_abs_omt &p ) const
{
    return bounds.contains( p );
}

bool boundary_section::contains( const tripoint_abs_ms &p ) const
{
    return bounds.contains( p );
}

bool boundary_section::is_in_border( const tripoint_abs_omt &p ) const
{
    return bounds.is_in_border( p );
}

bool boundary_section::is_in_border( const tripoint_abs_ms &p ) const
{
    return bounds.is_in_border( p );
}

boundary_bounds boundary_section::get_interior() const
{
    return bounds.interior();
}

void boundary_section::serialize( JsonOut &json ) const
{
    json.start_object();
    json.member( "id", id );
    json.member( "layer", static_cast<int>( layer ) );
    json.member( "bounds", bounds );
    json.member( "mapgen", mapgen );
    json.member( "generated", generated );
    json.end_object();
}

void boundary_section::deserialize( JsonIn &jsin )
{
    JsonObject data = jsin.get_object();
    data.read( "id", id );
    int layer_int = data.get_int( "layer" );
    layer = static_cast<world_layer>( layer_int );
    data.read( "bounds", bounds );
    if( data.has_object( "mapgen" ) ) {
        data.read( "mapgen", mapgen );
    }
    generated = data.get_bool( "generated", false );
}

boundary_section_manager &boundary_section_manager::instance()
{
    static boundary_section_manager manager;
    return manager;
}

boundary_section_id boundary_section_manager::register_section( world_layer layer,
        const boundary_bounds &bounds,
        const boundary_section_mapgen &mapgen_config )
{
    boundary_section_id id{ next_id++ };

    boundary_section section;
    section.id = id;
    section.layer = layer;
    section.bounds = bounds;
    section.mapgen = mapgen_config;
    section.generated = false;

    sections[id] = std::move( section );

    return id;
}

void boundary_section_manager::unregister_section( boundary_section_id id )
{
    sections.erase( id );
}

boundary_section *boundary_section_manager::get( boundary_section_id id )
{
    auto it = sections.find( id );
    if( it == sections.end() ) {
        return nullptr;
    }
    return &it->second;
}

const boundary_section *boundary_section_manager::get( boundary_section_id id ) const
{
    auto it = sections.find( id );
    if( it == sections.end() ) {
        return nullptr;
    }
    return &it->second;
}

boundary_section *boundary_section_manager::get_at( const tripoint_abs_omt &pos )
{
    for( auto &[id, section] : sections ) {
        if( section.contains( pos ) ) {
            return &section;
        }
    }
    return nullptr;
}

boundary_section *boundary_section_manager::get_at( const tripoint_abs_ms &pos )
{
    tripoint_abs_omt omt_pos = project_to<coords::omt>( pos );
    return get_at( omt_pos );
}

const boundary_section *boundary_section_manager::get_at( const tripoint_abs_omt &pos ) const
{
    for( const auto &[id, section] : sections ) {
        if( section.contains( pos ) ) {
            return &section;
        }
    }
    return nullptr;
}

const boundary_section *boundary_section_manager::get_at( const tripoint_abs_ms &pos ) const
{
    tripoint_abs_omt omt_pos = project_to<coords::omt>( pos );
    return get_at( omt_pos );
}

bool boundary_section_manager::is_in_any_border( const tripoint_abs_omt &pos ) const
{
    for( const auto &[id, section] : sections ) {
        if( section.is_in_border( pos ) ) {
            return true;
        }
    }
    return false;
}

bool boundary_section_manager::is_in_any_border( const tripoint_abs_ms &pos ) const
{
    tripoint_abs_omt omt_pos = project_to<coords::omt>( pos );
    return is_in_any_border( omt_pos );
}

bool boundary_section_manager::would_collide( const boundary_bounds &bounds ) const
{
    for( const auto &[id, section] : sections ) {
        if( section.bounds.intersects( bounds ) ) {
            return true;
        }
    }
    return false;
}

std::pair<int, int> boundary_section_manager::get_z_bounds_at( const tripoint_abs_sm &pos ) const
{
    tripoint_abs_omt omt_pos = project_to<coords::omt>( pos );
    for( const auto &[id, section] : sections ) {
        if( section.bounds.contains( omt_pos ) ) {
            // Return the z-range of this section's bounds (max is exclusive, so subtract 1)
            return { section.bounds.min.z(), section.bounds.max.z() - 1 };
        }
    }
    // Not in any section - return (0, 0) to indicate no constraint
    return { 0, 0 };
}

oter_id boundary_section_manager::get_oter_at( const tripoint_abs_omt &pos ) const
{
    // Find the section containing this position
    const boundary_section *section = get_at( pos );
    if( !section ) {
        // Not in any boundary section - return null terrain
        return oter_id();
    }

    // Check if in border zone
    if( section->is_in_border( pos ) ) {
        // Border terrain - use a distinct overmap terrain type
        static const oter_id ot_pd_border( "pd_border" );
        return ot_pd_border;
    }

    // Interior - return the appropriate terrain based on mapgen configuration
    if( !section->mapgen.mapgen_id.empty() ) {
        // Return the mapgen_id as oter_id (e.g., "field" -> oter_id("field"))
        return oter_id( section->mapgen.mapgen_id );
    }

    // Default interior terrain
    static const oter_id ot_field( "field" );
    return ot_field;
}

void boundary_section_manager::clear()
{
    sections.clear();
    next_id = 1;
}

void boundary_section_manager::serialize( JsonOut &json ) const
{
    json.start_object();

    json.member( "next_id", next_id );

    json.member( "sections" );
    json.start_array();
    for( const auto &[id, section] : sections ) {
        section.serialize( json );
    }
    json.end_array();

    json.end_object();
}

void boundary_section_manager::deserialize( JsonIn &jsin )
{
    JsonObject data = jsin.get_object();

    data.read( "next_id", next_id );

    sections.clear();
    for( JsonObject jo : data.get_array( "sections" ) ) {
        boundary_section section;
        jo.read( "id", section.id );
        int layer_int = jo.get_int( "layer" );
        section.layer = static_cast<world_layer>( layer_int );
        jo.read( "bounds", section.bounds );
        if( jo.has_object( "mapgen" ) ) {
            jo.read( "mapgen", section.mapgen );
        }
        section.generated = jo.get_bool( "generated", false );
        sections[section.id] = std::move( section );
    }
}

std::pair<tripoint, tripoint> boundary_section_manager::calculate_special_bounds(
    const overmap_special &special )
{
    std::vector<overmap_special_locations> locations = special.required_locations();

    if( locations.empty() ) {
        // Empty special - return zero-size bounds
        return { tripoint_zero, tripoint_zero };
    }

    // Find min/max coordinates
    tripoint min_pos( std::numeric_limits<int>::max(),
                      std::numeric_limits<int>::max(),
                      std::numeric_limits<int>::max() );
    tripoint max_pos( std::numeric_limits<int>::min(),
                      std::numeric_limits<int>::min(),
                      std::numeric_limits<int>::min() );

    for( const overmap_special_locations &loc : locations ) {
        min_pos.x = std::min( min_pos.x, loc.p.x );
        min_pos.y = std::min( min_pos.y, loc.p.y );
        min_pos.z = std::min( min_pos.z, loc.p.z );
        max_pos.x = std::max( max_pos.x, loc.p.x );
        max_pos.y = std::max( max_pos.y, loc.p.y );
        max_pos.z = std::max( max_pos.z, loc.p.z );
    }

    // Convert from min/max to min and size (max is exclusive)
    // Size = max - min + 1 (since both endpoints are inclusive in the terrain list)
    return { min_pos, max_pos + tripoint( 1, 1, 1 ) };
}

tripoint_abs_omt boundary_section_manager::allocate_space( world_layer layer,
        const tripoint &size_omt )
{
    // Total size including borders
    int border = boundary_bounds::DEFAULT_BORDER_WIDTH_OMT;
    tripoint total_size = size_omt + tripoint( border * 2, border * 2, border * 2 );

    // Simple allocation: scan for non-colliding position
    // Use wide spacing to minimize collision checks
    constexpr int spacing = 200;  // 200 OMT spacing between allocation attempts

    for( int attempt = 0; attempt < 10000; attempt++ ) {
        int x = ( attempt % 100 ) * spacing;
        int y = ( attempt / 100 ) * spacing;

        tripoint_abs_omt candidate( x, y, get_layer_base_z( layer ) );
        boundary_bounds test_bounds;
        test_bounds.min = candidate;
        test_bounds.max = candidate + total_size;
        test_bounds.border_width_omt = border;

        if( !would_collide( test_bounds ) ) {
            return candidate;
        }
    }

    debugmsg( "Failed to allocate boundary section space after 10000 attempts" );
    return tripoint_abs_omt( 0, 0, get_layer_base_z( layer ) );
}

void boundary_section_manager::generate_section_terrain( boundary_section_id id )
{
    boundary_section *section = get( id );
    if( !section ) {
        debugmsg( "Cannot generate terrain: section %d not found", id.value );
        return;
    }

    if( section->generated ) {
        return;  // Already generated
    }

    // First pass: create base submaps (border and floor)
    create_base_submaps( *section );

    // Second pass: apply mapgen if configured
    if( section->mapgen.uses_special() ) {
        if( !section->mapgen.special_id->is_valid() ) {
            debugmsg( "Cannot generate from special: invalid overmap_special_id '%s'",
                      section->mapgen.special_id->str() );
        } else {
            apply_special_mapgen( *section, **section->mapgen.special_id );
        }
    } else if( !section->mapgen.mapgen_id.empty() ) {
        apply_legacy_mapgen( *section );
    }

    section->generated = true;
}

void boundary_section_manager::clear_section_submaps( boundary_section_id id )
{
    const boundary_section *section = get( id );
    if( !section ) {
        return;
    }

    tripoint_abs_sm min_sm = project_to<coords::sm>( section->bounds.min );
    tripoint_abs_sm max_sm = project_to<coords::sm>( section->bounds.max );

    for( int z = min_sm.z(); z < max_sm.z(); z++ ) {
        for( int y = min_sm.y(); y < max_sm.y(); y++ ) {
            for( int x = min_sm.x(); x < max_sm.x(); x++ ) {
                tripoint sm_pos( x, y, z );
                MAPBUFFER.remove_submap_safe( sm_pos );
            }
        }
    }
}

void boundary_section_manager::create_base_submaps( const boundary_section &section )
{
    // Convert to submap coordinates
    tripoint_abs_sm full_min_sm = project_to<coords::sm>( section.bounds.min );
    tripoint_abs_sm full_max_sm = project_to<coords::sm>( section.bounds.max );

    // Add buffer for the reality bubble (MAPSIZE = 11 submaps)
    // We need extra submaps beyond our bounds to prevent the game from trying
    // to generate submaps when the reality bubble extends past our section
    constexpr int buffer_submaps = MAPSIZE;  // Full reality bubble size as buffer

    tripoint_abs_sm gen_min_sm = full_min_sm - tripoint( buffer_submaps, buffer_submaps, 0 );
    tripoint_abs_sm gen_max_sm = full_max_sm + tripoint( buffer_submaps, buffer_submaps, 0 );

    // Create ALL submaps filled with pd_border terrain.
    // The mapgen functions (apply_special_mapgen) will overwrite the terrain
    // for OMTs that are part of the overmap_special. Any OMT not covered by
    // the special will remain as pd_border.
    for( int z = full_min_sm.z(); z < full_max_sm.z(); z++ ) {
        for( int y = gen_min_sm.y(); y < gen_max_sm.y(); y++ ) {
            for( int x = gen_min_sm.x(); x < gen_max_sm.x(); x++ ) {
                tripoint sm_pos( x, y, z );

                if( MAPBUFFER.is_submap_loaded( sm_pos ) ) {
                    continue;
                }

                // Create submap with absolute map square position
                tripoint abs_ms = sm_to_ms_copy( sm_pos );
                auto sm = std::make_unique<submap>( abs_ms );

                // Fill all tiles with pd_border - mapgen will overwrite as needed
                for( int sy = 0; sy < SEEY; sy++ ) {
                    for( int sx = 0; sx < SEEX; sx++ ) {
                        sm->set_ter( point( sx, sy ), t_pd_border );
                    }
                }

                MAPBUFFER.add_submap( sm_pos, sm );
            }
        }
    }
}

void boundary_section_manager::apply_legacy_mapgen( boundary_section &section )
{
    boundary_bounds interior = section.get_interior();

    // Parse mapgen_ids - can be comma-separated for tiled sections
    // Format: "id1,id2,id3,..." where IDs are applied in order to OMTs
    // row by row (Y), then column (X), then level (Z)
    std::vector<std::string> mapgen_ids;
    size_t start = 0;
    size_t end = section.mapgen.mapgen_id.find( ',' );
    while( end != std::string::npos ) {
        std::string id = section.mapgen.mapgen_id.substr( start, end - start );
        // Trim whitespace
        id.erase( 0, id.find_first_not_of( " \t" ) );
        id.erase( id.find_last_not_of( " \t" ) + 1 );
        if( !id.empty() ) {
            mapgen_ids.push_back( id );
        }
        start = end + 1;
        end = section.mapgen.mapgen_id.find( ',', start );
    }
    // Don't forget the last (or only) element
    std::string last_id = section.mapgen.mapgen_id.substr( start );
    last_id.erase( 0, last_id.find_first_not_of( " \t" ) );
    last_id.erase( last_id.find_last_not_of( " \t" ) + 1 );
    if( !last_id.empty() ) {
        mapgen_ids.push_back( last_id );
    }

    if( mapgen_ids.empty() ) {
        return;
    }

    // Apply mapgen to each OMT
    size_t mapgen_index = 0;
    for( int z = interior.min.z(); z < interior.max.z(); z++ ) {
        for( int y = interior.min.y(); y < interior.max.y(); y++ ) {
            for( int x = interior.min.x(); x < interior.max.x(); x++ ) {
                // Get the mapgen ID for this OMT (cycle through list if needed)
                const std::string &current_mapgen = mapgen_ids[mapgen_index % mapgen_ids.size()];
                mapgen_index++;

                if( !has_mapgen_for( current_mapgen ) ) {
                    // Mapgen not found - skip, submap will retain default floor terrain
                    continue;
                }

                // Create a tinymap and load the submaps we created earlier
                tripoint_abs_omt omt_pos( x, y, z );
                tripoint_abs_sm sm_pos = project_to<coords::sm>( omt_pos );

                tinymap tm;
                // Load with update_vehicles=false since there are none yet
                tm.load( sm_pos.raw(), false );

                // Run mapgen on the tinymap
                mapgendata dat( tm, mapgendata::dummy_settings );
                if( run_mapgen_func( current_mapgen, dat ) ) {
                    // Mapgen succeeded - save the modified submaps back to MAPBUFFER
                    tm.save();
                }
            }
        }
    }
}

void boundary_section_manager::apply_special_mapgen( boundary_section &section,
        const overmap_special &special )
{
    boundary_bounds interior = section.get_interior();

    // Get all terrains from the special
    std::vector<overmap_special_terrain> terrains = special.preview_terrains();

    // For multi-z specials, we need all terrains, not just z=0
    // preview_terrains() only returns z=0, so we use required_locations() for positions
    // and get_terrain_at() for the actual terrain
    std::vector<overmap_special_locations> locations = special.required_locations();

    for( const overmap_special_locations &loc : locations ) {
        // Apply the offset stored in mapgen config
        tripoint adjusted_pos = loc.p + section.mapgen.special_offset;

        // Calculate absolute OMT position within interior
        tripoint_abs_omt omt_pos( interior.min.x() + adjusted_pos.x,
                                  interior.min.y() + adjusted_pos.y,
                                  interior.min.z() + adjusted_pos.z );

        // Validate position is within interior bounds
        if( omt_pos.x() < interior.min.x() || omt_pos.x() >= interior.max.x() ||
            omt_pos.y() < interior.min.y() || omt_pos.y() >= interior.max.y() ||
            omt_pos.z() < interior.min.z() || omt_pos.z() >= interior.max.z() ) {
            debugmsg( "Special terrain at %s (adjusted to %s) is outside interior bounds",
                      loc.p.to_string(), omt_pos.to_string() );
            continue;
        }

        // Get the terrain type for this position
        const oter_str_id &terrain = special.get_terrain_at( loc.p );
        if( !terrain.is_valid() ) {
            continue;  // No terrain at this position (shouldn't happen)
        }

        // Get the mapgen ID from the terrain type
        // Note: We need to get the actual oter_id to call get_mapgen_id()
        oter_id ter_id = terrain.id();
        std::string mapgen_id = ter_id->get_mapgen_id();

        if( !has_mapgen_for( mapgen_id ) ) {
            // Mapgen not found - skip, submap will retain default floor terrain
            continue;
        }

        // Create a tinymap and load the submaps
        tripoint_abs_sm sm_pos = project_to<coords::sm>( omt_pos );

        tinymap tm;
        tm.load( sm_pos.raw(), false );

        // Run mapgen on the tinymap
        mapgendata dat( tm, mapgendata::dummy_settings );
        if( run_mapgen_func( mapgen_id, dat ) ) {
            tm.save();
        }
    }
}
