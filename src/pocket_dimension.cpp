#include "pocket_dimension.h"

#include "coordinate_conversions.h"
#include "coordinates.h"
#include "debug.h"
#include "game.h"
#include "game_constants.h"
#include "item.h"
#include "json.h"
#include "map.h"
#include "messages.h"
#include "overmap_special.h"

void pocket_dimension_id::serialize( JsonOut &json ) const
{
    json.write( value );
}

void pocket_dimension_id::deserialize( JsonIn &jsin )
{
    value = jsin.get_int();
}

bool pocket_owner_item::is_valid() const
{
    return !ref.is_unassigned() && !ref.is_destroyed() && !ref.is_unloaded();
}

void pocket_owner_item::serialize( JsonOut &json ) const
{
    json.write( ref.serialize() );
}

void pocket_owner_item::deserialize( JsonIn &jsin )
{
    safe_reference<item>::id_type id = jsin.get_uint64();
    ref.deserialize( id );
}

bool pocket_dimension::is_owner_valid() const
{
    return owner.is_valid();
}

const boundary_bounds *pocket_dimension::get_bounds() const
{
    const boundary_section *section = boundary_section_manager::instance().get( section_id );
    if( section ) {
        return &section->bounds;
    }
    return nullptr;
}

tripoint_abs_ms pocket_dimension::get_entry_point() const
{
    // Return the center of the interior at the lowest Z level
    boundary_bounds interior_bounds = get_interior();

    // Calculate center of interior in OMT coordinates
    int center_x = ( interior_bounds.min.x() + interior_bounds.max.x() ) / 2;
    int center_y = ( interior_bounds.min.y() + interior_bounds.max.y() ) / 2;
    int entry_z = interior_bounds.min.z();  // Ground floor of pocket dimension

    tripoint_abs_omt center_omt( center_x, center_y, entry_z );

    // Convert OMT to map squares using proper projection
    tripoint_abs_ms corner_ms = project_to<coords::ms>( center_omt );

    // Add offset to center of the OMT (OMT is 24x24 tiles, so center is at +12, +12)
    tripoint_abs_ms result = corner_ms + tripoint( SEEX, SEEY, 0 );
    return result;
}

boundary_bounds pocket_dimension::get_interior() const
{
    const boundary_bounds *bounds = get_bounds();
    if( bounds ) {
        return bounds->interior();
    }
    // Fallback: return empty bounds
    return boundary_bounds{};
}

bool pocket_dimension::is_generated() const
{
    const boundary_section *section = boundary_section_manager::instance().get( section_id );
    return section && section->generated;
}

void pocket_dimension::serialize( JsonOut &json ) const
{
    json.start_object();
    json.member( "id", id );
    json.member( "section_id", section_id );
    json.member( "owner", owner );
    json.member( "return_location", return_location );
    json.end_object();
}

void pocket_dimension::deserialize( JsonIn &jsin )
{
    JsonObject data = jsin.get_object();
    data.read( "id", id );
    data.read( "section_id", section_id );
    data.read( "owner", owner );
    data.read( "return_location", return_location );
}

pocket_dimension_manager &pocket_dimension_manager::instance()
{
    static pocket_dimension_manager manager;
    return manager;
}

pocket_dimension_id pocket_dimension_manager::create( item &owner_item,
        const overmap_special_id &special_id_param )
{
    // Default to "Cave" if not provided or invalid
    overmap_special_id actual_special_id = special_id_param;
    if( !actual_special_id.is_valid() ) {
        actual_special_id = overmap_special_id( "Cave" );
    }

    // Validate the special exists
    if( !actual_special_id.is_valid() ) {
        debugmsg( "Cannot create pocket dimension: invalid overmap_special_id '%s' and default 'Cave' also invalid",
                  special_id_param.str() );
        return pocket_dimension_id{ -1 };
    }

    const overmap_special &special = *actual_special_id;

    // Calculate bounds from special's terrain layout
    auto [min_pos, max_pos] = boundary_section_manager::calculate_special_bounds( special );
    tripoint size_omt = max_pos - min_pos;

    if( size_omt.x <= 0 || size_omt.y <= 0 ) {
        debugmsg( "Cannot create pocket dimension from special '%s': invalid size %s",
                  actual_special_id.str(), size_omt.to_string() );
        return pocket_dimension_id{ -1 };
    }

    // Ensure z-range is at least 1 (for 2D specials)
    if( size_omt.z <= 0 ) {
        size_omt.z = 1;
    }

    pocket_dimension_id id{ next_id++ };

    // Configure mapgen for the boundary section
    boundary_section_mapgen mapgen_config;
    mapgen_config.special_id = actual_special_id;
    // No offset adjustment needed - terrain positions from the special are used directly
    mapgen_config.special_offset = tripoint_zero - min_pos;

    // Allocate space
    tripoint_abs_omt origin = boundary_section_manager::instance().allocate_space(
                                  world_layer::POCKET_DIMENSION, size_omt );

    // Calculate total bounds
    int border = boundary_bounds::DEFAULT_BORDER_WIDTH_OMT;
    tripoint total_size = size_omt + tripoint( border * 2, border * 2, border * 2 );

    // Create the boundary bounds
    boundary_bounds bounds;
    bounds.min = origin;
    bounds.max = origin + total_size;
    bounds.border_width_omt = border;

    // Register the boundary section with mapgen config
    boundary_section_id section_id = boundary_section_manager::instance().register_section(
                                         world_layer::POCKET_DIMENSION, bounds, mapgen_config );

    // Create the pocket dimension
    pocket_dimension pd;
    pd.id = id;
    pd.section_id = section_id;
    pd.owner = pocket_owner_item{ safe_reference<item>( owner_item ) };

    dimensions[id] = std::move( pd );

    return id;
}

pocket_dimension *pocket_dimension_manager::get( pocket_dimension_id id )
{
    auto it = dimensions.find( id );
    if( it == dimensions.end() ) {
        return nullptr;
    }
    return &it->second;
}

const pocket_dimension *pocket_dimension_manager::get( pocket_dimension_id id ) const
{
    auto it = dimensions.find( id );
    if( it == dimensions.end() ) {
        return nullptr;
    }
    return &it->second;
}

void pocket_dimension_manager::destroy( pocket_dimension_id id )
{
    auto it = dimensions.find( id );
    if( it == dimensions.end() ) {
        return;
    }

    // If player is in this dimension, force exit first
    if( current_dimension && *current_dimension == id ) {
        exit_current();
    }

    // Clear all submaps and unregister boundary section
    boundary_section_manager::instance().clear_section_submaps( it->second.section_id );
    boundary_section_manager::instance().unregister_section( it->second.section_id );

    dimensions.erase( it );
}

void pocket_dimension_manager::enter( pocket_dimension_id id,
                                      const tripoint_abs_omt &return_loc )
{
    pocket_dimension *pd = get( id );
    if( !pd ) {
        debugmsg( "Attempted to enter non-existent pocket dimension %d", id.value );
        return;
    }

    // Store return location
    pd->return_location = return_loc;

    // Generate map if not already done (delegates to boundary_section_manager)
    boundary_section_manager::instance().generate_section_terrain( pd->section_id );

    // Calculate destination OMT (center of interior)
    boundary_bounds interior = pd->get_interior();
    int center_x = ( interior.min.x() + interior.max.x() ) / 2;
    int center_y = ( interior.min.y() + interior.max.y() ) / 2;
    tripoint_abs_omt dest_omt( center_x, center_y, interior.min.z() );

    // Mark that we're now in a pocket dimension
    current_dimension = id;

    // Use existing teleportation infrastructure
    g->place_player_overmap( dest_omt );

    // Position player at entry point
    tripoint_abs_ms entry = pd->get_entry_point();
    tripoint local_entry = get_map().getlocal( entry.raw() );
    g->place_player( local_entry );

    add_msg( m_info, _( "You enter the pocket dimension." ) );
}

void pocket_dimension_manager::exit_current()
{
    if( !current_dimension ) {
        debugmsg( "Attempted to exit pocket dimension when not in one" );
        return;
    }

    pocket_dimension *pd = get( *current_dimension );
    if( !pd ) {
        debugmsg( "Current pocket dimension no longer exists" );
        current_dimension = std::nullopt;
        return;
    }

    tripoint_abs_omt return_loc = pd->return_location;
    current_dimension = std::nullopt;

    // Teleport back
    g->place_player_overmap( return_loc );

    add_msg( m_info, _( "You return to the real world." ) );
}

bool pocket_dimension_manager::player_in_pocket_dimension() const
{
    return current_dimension.has_value();
}

std::optional<pocket_dimension_id> pocket_dimension_manager::current_pocket_id() const
{
    return current_dimension;
}

bool pocket_dimension_manager::is_in_pocket_border( const tripoint_abs_ms &pos ) const
{
    // Early out for normal Z levels
    if( !is_pocket_dimension_z( pos.z() ) ) {
        return false;
    }

    // Delegate to boundary section manager
    return boundary_section_manager::instance().is_in_any_border( pos );
}

bool pocket_dimension_manager::is_in_pocket_border( const tripoint_abs_omt &pos ) const
{
    // Early out for normal Z levels
    if( !is_pocket_dimension_z( pos.z() ) ) {
        return false;
    }

    // Delegate to boundary section manager
    return boundary_section_manager::instance().is_in_any_border( pos );
}

pocket_dimension *pocket_dimension_manager::get_at_position( const tripoint_abs_omt &pos )
{
    // Find the boundary section at this position
    const boundary_section *section = boundary_section_manager::instance().get_at( pos );
    if( !section ) {
        return nullptr;
    }

    // Find the pocket dimension with this section ID
    for( auto &[id, pd] : dimensions ) {
        if( pd.section_id == section->id ) {
            return &pd;
        }
    }
    return nullptr;
}

pocket_dimension *pocket_dimension_manager::get_at_position( const tripoint_abs_ms &pos )
{
    tripoint_abs_omt omt_pos = project_to<coords::omt>( pos );
    return get_at_position( omt_pos );
}

void pocket_dimension_manager::process()
{
    // Check all pocket dimensions for orphaned owners
    std::vector<pocket_dimension_id> to_destroy;

    for( const auto &[id, pd] : dimensions ) {
        if( !pd.is_owner_valid() ) {
            // If player is inside an orphaned dimension, exit them first
            if( current_dimension && *current_dimension == id ) {
                add_msg( m_warning,
                         _( "The pocket dimension collapses around you!  You are ejected back to reality." ) );
                exit_current();
            }
            to_destroy.push_back( id );
        }
    }

    for( const auto &id : to_destroy ) {
        destroy( id );
    }
}

void pocket_dimension_manager::clear()
{
    // Unregister all boundary sections
    for( const auto &[id, pd] : dimensions ) {
        boundary_section_manager::instance().unregister_section( pd.section_id );
    }

    dimensions.clear();
    next_id = 1;
    current_dimension = std::nullopt;
}

void pocket_dimension_manager::serialize( JsonOut &json ) const
{
    json.start_object();

    json.member( "next_id", next_id );

    json.member( "current_dimension" );
    if( current_dimension ) {
        current_dimension->serialize( json );
    } else {
        json.write( -1 );
    }

    json.member( "dimensions" );
    json.start_array();
    for( const auto &[id, pd] : dimensions ) {
        pd.serialize( json );
    }
    json.end_array();

    // Also serialize the boundary section manager data
    json.member( "boundary_sections" );
    boundary_section_manager::instance().serialize( json );

    json.end_object();
}

void pocket_dimension_manager::deserialize( JsonIn &jsin )
{
    JsonObject data = jsin.get_object();

    data.read( "next_id", next_id );

    int current_id = data.get_int( "current_dimension" );
    if( current_id >= 0 ) {
        current_dimension = pocket_dimension_id{ current_id };
    } else {
        current_dimension = std::nullopt;
    }

    dimensions.clear();
    for( JsonObject jo : data.get_array( "dimensions" ) ) {
        pocket_dimension pd;
        jo.read( "id", pd.id );
        jo.read( "section_id", pd.section_id );
        jo.read( "owner", pd.owner );
        jo.read( "return_location", pd.return_location );
        dimensions[pd.id] = std::move( pd );
    }

    // Deserialize the boundary section manager data
    if( data.has_object( "boundary_sections" ) ) {
        data.read( "boundary_sections", boundary_section_manager::instance() );
    }
}
