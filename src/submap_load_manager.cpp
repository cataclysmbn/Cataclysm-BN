#include "submap_load_manager.h"

#include <algorithm>
#include <cstdint>
#include <future>
#include <ranges>
#include <set>
#include <utility>
#include <vector>

#include "coordinate_conversions.h"
#include "mapbuffer.h"
#include "mapbuffer_registry.h"
#include "point.h"
#include "profile.h"
#include "thread_pool.h"

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
// load shape cache
// ---------------------------------------------------------------------------

auto submap_load_manager::update_load_shape( int radius ) -> void
{
    const auto axis = std::views::iota( -radius, radius + 1 );
    auto to_point = []( auto pair ) {
        auto [dx, dy] = pair;
        return point{ dx, dy };
    };
    auto offsets = std::views::cartesian_product( axis, axis )
                   | std::views::transform( to_point );
    bubble_offsets_.assign( offsets.begin(), offsets.end() );
}

// ---------------------------------------------------------------------------
// desired-set computation
// ---------------------------------------------------------------------------

std::set<submap_load_manager::desired_key> submap_load_manager::compute_desired_set() const
{
    ZoneScoped;
    std::set<desired_key> desired;
    std::ranges::for_each( requests_, [&]( const auto & kv ) {
        const submap_load_request &req = kv.second;
        const tripoint c = req.center.raw();
        const auto z_range = std::views::iota( req.z_min, req.z_max + 1 );

        if( req.source == load_request_source::reality_bubble ) {
            // Use the precomputed square offsets so all submaps in the full
            // (2*radius+1)×(2*radius+1) grid are protected from eviction.
            // bubble_offsets_ is populated by update_load_shape() in map::resize().
            std::ranges::for_each(
                std::views::cartesian_product( bubble_offsets_, z_range ),
            [&]( auto pair ) {
                auto [off, z] = pair;
                desired.emplace( req.dimension_id,
                                 tripoint{ c.x + off.x, c.y + off.y, z } );
            } );
        } else {
            // Other sources (player_base, script, fire_spread) also use square.
            const int r = req.radius;
            const auto axis = std::views::iota( -r, r + 1 );
            std::ranges::for_each(
                std::views::cartesian_product( axis, axis, z_range ),
            [&]( auto tuple ) {
                auto [dx, dy, z] = tuple;
                desired.emplace( req.dimension_id,
                                 tripoint{ c.x + dx, c.y + dy, z } );
            } );
        }
    } );
    return desired;
}

// ---------------------------------------------------------------------------
// update
// ---------------------------------------------------------------------------

void submap_load_manager::update()
{
    ZoneScoped;
    TracyPlot( "Loaded Submaps", static_cast<int64_t>( prev_desired_.size() ) );
    TracyPlot( "Thread Pool Workers", static_cast<int64_t>( get_thread_pool().num_workers() ) );
    TracyPlot( "Thread Pool Queue", static_cast<int64_t>( get_thread_pool().queue_size() ) );
    std::set<desired_key> new_desired = compute_desired_set();

    // Collect the unique (dim_id, om_addr) quad addresses that are newly desired.
    // Multiple submap positions map to the same quad; deduplicate here so each
    // quad file is read exactly once.
    using quad_key = std::pair<std::string, tripoint>;
    std::set<quad_key> new_quads;
    for( const desired_key &key : new_desired ) {
        if( prev_desired_.count( key ) == 0 ) {
            new_quads.emplace( key.first, sm_to_omt_copy( key.second ) );
        }
    }

    // Dispatch all new quad disk reads to the thread pool in parallel.
    // mapbuffer::preload_quad() performs the slow I/O phase outside its internal
    // lock, allowing different quads to be read concurrently.  After all futures
    // complete, every newly-desired submap that existed on disk is resident.
    // Submaps without a disk file are generated asynchronously by submap_stream
    // workers via submap_streamer.request_load() (called in map::shift()).  Those
    // workers use tinymap::bind_dimension() and get_overmapbuffer(dim) so no
    // global state is read at worker-thread execution time.
    std::vector<std::future<void>> load_futures;
    load_futures.reserve( new_quads.size() );
    for( const auto &[dim_id, om_addr] : new_quads ) {
        auto &mb = MAPBUFFER_REGISTRY.get( dim_id );
        load_futures.push_back( get_thread_pool().submit_returning(
        [&mb, om_addr]() {
            mb.preload_quad( om_addr );
        } ) );
    }
    // Block until all disk reads complete before notifying listeners.
    // Listeners (e.g. distribution_grid_tracker) may call lookup_submap_in_memory();
    // the submap must be present at that point.
    std::ranges::for_each( load_futures, []( auto & f ) {
        f.get();
    } );

    // Notify listeners for all newly-desired positions.
    for( const desired_key &key : new_desired ) {
        if( prev_desired_.count( key ) == 0 ) {
            const tripoint_abs_sm pos( key.second );
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
                // Circle members already had on_submap_unloaded fired above (clearing
                // their grid[] slots).  Corner members outside the circle were never
                // loaded into grid[] by loadn(), so freeing them is unconditionally
                // safe — no dangling grid[] pointer exists for them.
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
    const tripoint p = pos.raw();
    return std::ranges::any_of( requests_, [&]( const auto & kv ) {
        const submap_load_request &req = kv.second;
        if( req.source != load_request_source::reality_bubble ) {
            return false;
        }
        if( req.dimension_id != dim_id ) {
            return false;
        }
        const tripoint c = req.center.raw();
        const int dx = p.x - c.x;
        const int dy = p.y - c.y;
        return dx * dx + dy * dy <= req.radius * req.radius
               && p.z >= req.z_min && p.z <= req.z_max;
    } );
}

std::vector<std::string> submap_load_manager::active_dimensions() const
{
    std::set<std::string> dims;
    for( const auto &kv : requests_ ) {
        dims.insert( kv.second.dimension_id );
    }
    return { dims.begin(), dims.end() };
}

auto submap_load_manager::non_bubble_requests() const -> std::vector<submap_load_request>
{
    auto is_non_bubble = []( const auto & kv ) {
        return kv.second.source != load_request_source::reality_bubble;
    };
    auto to_request = []( const auto & kv ) -> const submap_load_request & {
        return kv.second;
    };
    auto view = requests_ | std::views::filter( is_non_bubble )
                | std::views::transform( to_request );
    return { view.begin(), view.end() };
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
