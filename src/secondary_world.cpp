#include "secondary_world.h"

#include <algorithm>
#include <utility>

#include "debug.h"
#include "game.h"
#include "map.h"
#include "mapbuffer.h"
#include "overmap.h"
#include "overmapbuffer.h"
#include "submap.h"

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

void secondary_world::capture_from_primary(
    const std::optional<dimension_bounds> &bounds,
    const tripoint_abs_sm &simulation_center )
{
    // Transfer submaps from MAPBUFFER
    for( auto it = MAPBUFFER.begin(); it != MAPBUFFER.end(); ++it ) {
        submaps_[it->first] = std::move( it->second );
    }

    // Overmaps are already saved to disk by the caller (travel_to_dimension)
    // with the correct dimension prefix BEFORE the world type switch.
    // Do NOT call overmap_buffer.save() here — the prefix has already changed
    // to the destination dimension, so saving here would corrupt the destination's
    // overmap files with this dimension's (mostly empty) overmap data.

    bounds_ = bounds;
    simulation_center_ = simulation_center;
    loaded_ = true;

    // Clear the primary buffers after transfer
    // MAPBUFFER submaps were moved, so just clear the container
    MAPBUFFER.clear();
    overmap_buffer.clear();
}

void secondary_world::restore_to_primary()
{
    if( !loaded_ ) {
        debugmsg( "Attempted to restore from unloaded secondary_world" );
        return;
    }

    // Save secondary state before restoring
    save_state();

    // Transfer submaps back to MAPBUFFER
    for( auto &pair : submaps_ ) {
        MAPBUFFER.add_submap( pair.first, pair.second );
    }
    submaps_.clear();

    // Overmaps will be loaded from disk by the normal load path
    // since we saved them above

    loaded_ = false;
}

void secondary_world::save_state()
{
    if( !loaded_ ) {
        return;
    }

    // Save is handled by the normal save path which writes to dimension-prefixed files
    // The submaps and overmaps in this secondary world are already associated with
    // the correct dimension through the game's dimension prefix system
}

void secondary_world::simulate_tick( const std::string &level, time_duration delta )
{
    if( !loaded_ ) {
        return;
    }

    // "none" level means keep loaded but frozen - no simulation
    if( level == "none" || level == "off" ) {
        return;
    }

    if( level == "minimal" ) {
        simulate_minimal( delta );
    } else if( level == "moderate" ) {
        simulate_minimal( delta );
        simulate_moderate( delta );
    } else if( level == "full" ) {
        simulate_minimal( delta );
        simulate_moderate( delta );
        simulate_full( delta );
    }
}

void secondary_world::unload()
{
    if( !loaded_ ) {
        return;
    }

    // Save before unloading
    save_state();

    submaps_.clear();
    overmaps_.clear();
    bounds_.reset();
    loaded_ = false;
}

bool secondary_world::is_in_bounds( const tripoint &sm_pos ) const
{
    if( !bounds_ ) {
        // No bounds means everything is in bounds (infinite dimension)
        return true;
    }
    return bounds_->contains( tripoint_abs_sm( sm_pos ) );
}

void secondary_world::simulate_minimal( time_duration /*delta*/ )
{
    // Minimal simulation: process fields (fire, gas)
    // This would iterate over loaded submaps and process their fields
    // For now, this is a stub - full implementation would require
    // creating a temporary map context or refactoring field processing
    // to work without the global map object

    // TODO: Implement field processing for secondary worlds
    // This requires either:
    // 1. Creating a lightweight field processor that works on raw submaps
    // 2. Temporarily swapping buffers, creating a map, processing, then swapping back
    // Option 2 is simpler but has more overhead
}

void secondary_world::simulate_moderate( time_duration /*delta*/ )
{
    // Moderate simulation: vehicle systems (solar charging, battery drain)
    // Vehicles are stored in submaps, so we'd need to iterate and process them
    // Similar challenges to minimal simulation

    // TODO: Implement vehicle idle processing for secondary worlds
    // Vehicle::idle() handles solar charging, fuel consumption, etc.
    // Would need to call this for each vehicle in loaded submaps
}

void secondary_world::simulate_full( time_duration /*delta*/ )
{
    // Full simulation: everything including monsters, NPCs, combat
    // This is the most complex and would essentially require running
    // a parallel game loop

    // TODO: Implement full simulation for secondary worlds
    // This would include:
    // - Monster AI and movement
    // - NPC needs and activities
    // - Combat resolution
    // For now, this level of simulation is not implemented
}
