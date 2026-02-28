#include "secondary_world.h"

#include <utility>

#include "debug.h"
#include "mapbuffer.h"
#include "mapbuffer_registry.h"
#include "overmapbuffer.h"
#include "submap.h"
#include "world_type.h"

secondary_world::secondary_world( const world_type_id &type, const std::string &instance_id )
    : world_type_( type )
    , instance_id_( instance_id )
{
}

secondary_world::~secondary_world()
{
    if( loaded_ ) {
        unload();
    }
}

std::string secondary_world::get_dimension_id() const
{
    // Format: save_prefix + instance_id + "_" (if instance_id is non-empty),
    // or just save_prefix (if instance_id is empty).
    // This must match game::get_dimension_prefix() exactly so that the two
    // systems agree on which registry slot belongs to which dimension.
    //
    // Collision risk: there is no separator between save_prefix and instance_id,
    // so a prefix "foo" with instance "bar_" and a prefix "foobar" with instance "_"
    // would produce the same key.  In practice, save_prefix values are short
    // alphabetic strings and instance IDs are UUIDs, so collisions are impossible.
    // If that assumption ever changes, add an explicit separator character here
    // and in game::get_dimension_prefix().
    std::string dim_id;
    if( world_type_.is_valid() ) {
        dim_id = world_type_.obj().save_prefix;
    }
    if( !instance_id_.empty() ) {
        dim_id += instance_id_ + "_";
    }
    return dim_id;
}

void secondary_world::capture_from_primary(
    const std::optional<dimension_bounds> & /*bounds*/,
    const tripoint_abs_sm & /*simulation_center*/ )
{
    // Phase 6: With dimension-aware generation, submaps are already in their
    // dimension's registry slot (MAPBUFFER_REGISTRY.get(dim_id)).  No transfer
    // from primary is needed.  This method now just marks the dimension as
    // kept-loaded so we know to preserve its submaps across dimension travel.

    // Overmaps are already saved to disk by the caller before the prefix switch;
    // they will reload from disk when needed in the dimension's context.
    overmap_buffer.clear();

    loaded_ = true;
}

void secondary_world::restore_to_primary()
{
    if( !loaded_ ) {
        debugmsg( "Attempted to restore from unloaded secondary_world" );
        return;
    }

    // Phase 6: With dimension-aware generation, submaps are already in their
    // dimension's registry slot.  No transfer to primary is needed.  The caller
    // will rebind the map to this dimension and load submaps from the dimension's
    // slot directly.
    //
    // Overmaps will be loaded from disk by the normal load path.
    //
    // We do NOT unload the dimension here — the submaps are about to be used.
    // The dimension slot stays populated for the map to reference.

    loaded_ = false;
}

void secondary_world::save_state()
{
    if( !loaded_ ) {
        return;
    }

    // Phase 6: Kept dimensions have their submaps in their own registry slot.
    // Saving a kept dimension while another dimension is active would require
    // temporarily switching map contexts, which is complex and risky.
    //
    // For now, kept dimensions are saved when they become active again (via the
    // normal game::save_maps() path after travel_to_dimension()).  Mid-session
    // autosaves while a dimension is kept will NOT persist its state.
    //
    // Future improvement: call MAPBUFFER_REGISTRY.get(get_dimension_id()).save()
    // with appropriate map context switching.
}

void secondary_world::unload()
{
    if( !loaded_ ) {
        return;
    }

    save_state();

    // Remove the dimension's submaps from the registry.
    const std::string dim_id = get_dimension_id();
    MAPBUFFER_REGISTRY.unload_dimension( dim_id );

    // Clear any legacy in-memory submaps (old-save deserialization remnants).
    submaps_.clear();

    loaded_ = false;
}

void secondary_world::migrate_submaps_to_registry()
{
    if( submaps_.empty() ) {
        return;
    }

    // Move submaps from the legacy field into the appropriate registry slot.
    const std::string dim_id = get_dimension_id();
    mapbuffer &dim_buf = MAPBUFFER_REGISTRY.get( dim_id );

    for( auto &kv : submaps_ ) {
        dim_buf.add_submap( kv.first, kv.second );
    }
    submaps_.clear();
}
