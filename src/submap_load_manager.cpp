#include "submap_load_manager.h"

#include <algorithm>
#include <set>
#include <utility>

#include "coordinate_conversions.h"
#include "mapbuffer.h"
#include "mapbuffer_registry.h"
#include "point.h"

submap_load_manager submap_loader;

// ---------------------------------------------------------------------------
// request management
// ---------------------------------------------------------------------------

load_request_handle submap_load_manager::request_load(
    load_request_source source,
    const std::string &dim_id,
    const tripoint_abs_sm &center,
    int radius,
    int z_min,
    int z_max )
{
    const load_request_handle handle = next_handle_++;
    submap_load_request req;
    req.source = source;
    req.dimension_id = dim_id;
    req.center = center;
    req.radius = radius;
    req.z_min = z_min;
    req.z_max = z_max;
    requests_[handle] = std::move( req );
    return handle;
}

void submap_load_manager::update_request( load_request_handle handle,
        const tripoint_abs_sm &new_center )
{
    auto it = requests_.find( handle );
    if( it == requests_.end() ) {
        return;
    }
    it->second.center = new_center;
}

void submap_load_manager::release_load( load_request_handle handle )
{
    requests_.erase( handle );
}

// ---------------------------------------------------------------------------
// desired-set computation
// ---------------------------------------------------------------------------

std::set<submap_load_manager::desired_key> submap_load_manager::compute_desired_set() const
{
    std::set<desired_key> desired;
    for( const auto &kv : requests_ ) {
        const submap_load_request &req = kv.second;
        const tripoint c = req.center.raw();
        const int r = req.radius;
        // Iterate all submaps in the (2r+1)×(2r+1) XY square across all requested
        // z-levels.  For most sources z_min == z_max == c.z (single level); for
        // reality_bubble requests in z-level builds z_min and z_max span the full
        // playable z range so that all loaded z-slices are tracked.
        for( int z = req.z_min; z <= req.z_max; ++z ) {
            for( int dx = -r; dx <= r; ++dx ) {
                for( int dy = -r; dy <= r; ++dy ) {
                    desired.emplace( req.dimension_id,
                                     tripoint{ c.x + dx, c.y + dy, z } );
                }
            }
        }
    }
    return desired;
}

// ---------------------------------------------------------------------------
// update
// ---------------------------------------------------------------------------

void submap_load_manager::update()
{
    std::set<desired_key> new_desired = compute_desired_set();

    // Positions that are newly desired
    for( const desired_key &key : new_desired ) {
        if( prev_desired_.count( key ) == 0 ) {
            const tripoint_abs_sm pos( key.second );
            // Ensure the submap is loaded in the appropriate mapbuffer
            MAPBUFFER_REGISTRY.get( key.first ).load_submap( pos );
            for( submap_load_listener *listener : listeners_ ) {
                listener->on_submap_loaded( pos, key.first );
            }
        }
    }

    // Positions that are no longer desired — notify listeners per-submap.
    for( const desired_key &key : prev_desired_ ) {
        if( new_desired.count( key ) == 0 ) {
            const tripoint_abs_sm pos( key.second );
            for( submap_load_listener *listener : listeners_ ) {
                listener->on_submap_unloaded( pos, key.first );
            }
        }
    }

    // Evict mapbuffer entries at OMT-quad granularity.
    // Only evict when ALL 4 submaps in a quad are absent from new_desired.
    // Partial eviction (one unload_submap call per member) progressively
    // overwrites the quad save file without the previously-removed siblings,
    // causing data loss and spurious "file did not contain expected submap" errors.
    {
        using quad_key = std::pair<std::string, tripoint>;
        std::set<quad_key> quads_checked;
        for( const desired_key &key : prev_desired_ ) {
            if( new_desired.count( key ) != 0 ) {
                continue;  // still desired — skip
            }
            const tripoint om_addr = sm_to_omt_copy( key.second );
            const quad_key qk{ key.first, om_addr };
            if( !quads_checked.insert( qk ).second ) {
                continue;  // already handled this quad in this cycle
            }
            // Check whether any of the 4 siblings remain in the desired set.
            bool any_still_desired = false;
            const tripoint base = omt_to_sm_copy( om_addr );
            for( const point &off : { point_zero, point_south, point_east, point_south_east } ) {
                const tripoint sibling{ base.x + off.x, base.y + off.y, base.z };
                if( new_desired.count( { key.first, sibling } ) ) {
                    any_still_desired = true;
                    break;
                }
            }
            if( !any_still_desired ) {
                // Safe: save the quad once and evict all 4 members atomically.
                MAPBUFFER_REGISTRY.get( key.first ).unload_quad( om_addr );
            }
        }
    }

    prev_desired_ = std::move( new_desired );
}

// ---------------------------------------------------------------------------
// query helpers
// ---------------------------------------------------------------------------

bool submap_load_manager::is_requested( const std::string &dim_id,
                                        const tripoint_abs_sm &pos ) const
{
    return prev_desired_.count( { dim_id, pos.raw() } ) > 0;
}

bool submap_load_manager::is_properly_requested( const std::string &dim_id,
        const tripoint_abs_sm &pos ) const
{
    for( const auto &kv : requests_ ) {
        const submap_load_request &req = kv.second;
        if( req.source != load_request_source::reality_bubble ) {
            continue;
        }
        if( req.dimension_id != dim_id ) {
            continue;
        }
        const tripoint c = req.center.raw();
        const tripoint p = pos.raw();
        if( std::abs( p.x - c.x ) <= req.radius &&
            std::abs( p.y - c.y ) <= req.radius &&
            p.z >= req.z_min && p.z <= req.z_max ) {
            return true;
        }
    }
    return false;
}

std::vector<std::string> submap_load_manager::active_dimensions() const
{
    std::set<std::string> dims;
    for( const auto &kv : requests_ ) {
        dims.insert( kv.second.dimension_id );
    }
    return { dims.begin(), dims.end() };
}

// ---------------------------------------------------------------------------
// flush_prev_desired
// ---------------------------------------------------------------------------

void submap_load_manager::flush_prev_desired()
{
    prev_desired_.clear();
}

// listener management
// ---------------------------------------------------------------------------

void submap_load_manager::add_listener( submap_load_listener *listener )
{
    if( std::find( listeners_.begin(), listeners_.end(), listener ) == listeners_.end() ) {
        listeners_.push_back( listener );
    }
}

void submap_load_manager::remove_listener( submap_load_listener *listener )
{
    listeners_.erase( std::remove( listeners_.begin(), listeners_.end(), listener ),
                      listeners_.end() );
}
