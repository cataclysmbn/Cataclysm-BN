#include "submap_load_manager.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <future>
#include <ranges>
#include <set>
#include <utility>
#include <vector>

#include "cata_cartesian_product.h"
#include "coordinate_conversions.h"
#include "mapbuffer.h"
#include "mapbuffer_registry.h"
#include "point.h"
#include "profile.h"
#include "thread_pool.h"

submap_load_manager submap_loader;

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

auto submap_load_manager::update_load_shape( int radius ) -> void
{
    const auto axis = std::views::iota( -radius, radius + 1 );
    auto to_point = []( auto pair ) {
        auto [dx, dy] = pair;
        return point{ dx, dy };
    };
    bubble_offsets_.clear();
    std::ranges::for_each( cata::views::cartesian_product( axis, axis ),
    [&]( auto pair ) {
        auto [dx, dy] = pair;
        bubble_offsets_.emplace_back( dx, dy );
    } );
}

auto submap_load_manager::compute_desired_set() const -> key_set
{
    ZoneScoped;
    key_set desired;
    std::ranges::for_each( requests_, [&]( const auto & kv ) {
        const submap_load_request &req = kv.second;
        // lazy_border positions are handled separately by compute_border_into().
        if( req.source == load_request_source::lazy_border ) {
            return;
        }
        const tripoint c = req.center.raw();
        const auto z_range = std::views::iota( req.z_min, req.z_max + 1 );

        if( req.source == load_request_source::reality_bubble ) {
            // Use the precomputed square offsets so all submaps in the full
            // (2*radius+1)×(2*radius+1) grid are protected from eviction.
            // bubble_offsets_ is populated by update_load_shape() in map::resize().
            std::ranges::for_each(
                cata::views::cartesian_product( bubble_offsets_, z_range ),
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
                cata::views::cartesian_product( axis, axis, z_range ),
            [&]( auto tuple ) {
                auto [dx, dy, z] = tuple;
                desired.emplace( req.dimension_id,
                                 tripoint{ c.x + dx, c.y + dy, z } );
            } );
        }
    } );
    return desired;
}

void submap_load_manager::compute_border_into( key_set &target ) const
{
    ZoneScoped;
    std::ranges::for_each( requests_, [&]( const auto & kv ) {
        const submap_load_request &req = kv.second;
        if( req.source != load_request_source::lazy_border ) {
            return;
        }
        const tripoint c = req.center.raw();
        const int r = req.radius;

        // Plain square — no quad-boundary alignment needed.  The eviction
        // pass already checks all 4 submaps in a quad before evicting, so
        // partial-quad fringes are handled there.
        const auto x_range = std::views::iota( c.x - r, c.x + r + 1 );
        const auto y_range = std::views::iota( c.y - r, c.y + r + 1 );
        const auto z_range = std::views::iota( req.z_min, req.z_max + 1 );
        std::ranges::for_each(
            cata::views::cartesian_product( x_range, y_range, z_range ),
        [&]( auto tuple ) {
            auto [x, y, z] = tuple;
            target.emplace( req.dimension_id, tripoint{ x, y, z } );
        } );
    } );
}

void submap_load_manager::drain_lazy_loads()
{
    ZoneScopedN( "drain_lazy_loads" );
    std::ranges::for_each( lazy_futures_, []( auto & entry ) {
        entry.second.get();
    } );
    lazy_futures_.clear();
    lazy_in_flight_.clear();

    // preload_quad workers may have deferred submap destructions.
    // Drain them on the main thread (safe_reference / cata_arena not thread-safe).
    auto dims_to_drain = std::set<std::string> {};
    std::ranges::for_each( requests_, [&]( const auto & kv ) {
        if( kv.second.source == load_request_source::lazy_border ) {
            dims_to_drain.insert( kv.second.dimension_id );
        }
    } );
    std::ranges::for_each( dims_to_drain, []( const std::string & dim_id ) {
        MAPBUFFER_REGISTRY.get( dim_id ).drain_pending_submap_destroy();
    } );
}

void submap_load_manager::update()
{
    ZoneScoped;

    // Non-blocking reap: collect completed lazy futures without stalling on
    // in-flight ones.  Workers may still be running — that is fine because
    // mapbuffer iteration in world_tick() now uses for_each_submap() which
    // holds submaps_mutex_, and preload_quad() only briefly acquires it per
    // add_submap().  Eviction below also acquires the mutex via unload_quad().
    {
        auto it = std::remove_if( lazy_futures_.begin(), lazy_futures_.end(),
        [this]( auto & entry ) {
            if( entry.second.wait_for( std::chrono::seconds( 0 ) ) == std::future_status::ready ) {
                entry.second.get();
                lazy_in_flight_.erase( entry.first );
                return true;
            }
            return false;
        } );
        lazy_futures_.erase( it, lazy_futures_.end() );
    }

    // Drain deferred submap destructions from completed preload_quad workers.
    // safe_reference / cata_arena are not thread-safe, so destruction must
    // happen on the main thread.  This is always safe to call — it only
    // processes entries already in the queue from finished workers.
    {
        auto dims_to_drain = std::set<std::string> {};
        std::ranges::for_each( requests_, [&]( const auto & kv ) {
            if( kv.second.source == load_request_source::lazy_border ) {
                dims_to_drain.insert( kv.second.dimension_id );
            }
        } );
        std::ranges::for_each( dims_to_drain, []( const std::string & dim_id ) {
            MAPBUFFER_REGISTRY.get( dim_id ).drain_pending_submap_destroy();
        } );
    }

    TracyPlot( "Thread Pool Workers", static_cast<int64_t>( get_thread_pool().num_workers() ) );
    TracyPlot( "Thread Pool Queue", static_cast<int64_t>( get_thread_pool().queue_size() ) );

    // Early exit: if no request centers have changed since the last update,
    // the desired/simulated/border sets are identical — skip the expensive
    // set construction, diffing, eviction, and lazy submission.
    {
        std::vector<std::pair<load_request_handle, tripoint>> cur_centers;
        cur_centers.reserve( requests_.size() );
        std::ranges::for_each( requests_, [&]( const auto & kv ) {
            cur_centers.emplace_back( kv.first, kv.second.center.raw() );
        } );
        if( cur_centers == prev_centers_ ) {
            return;
        }
        prev_centers_ = std::move( cur_centers );
    }

    // Simulated set: positions that need full per-turn processing.
    auto simulated = compute_desired_set();

    // Full set: simulated + lazy border (eviction protection only).
    auto all_desired = simulated;
    compute_border_into( all_desired );

    TracyPlot( "Simulated Submaps", static_cast<int64_t>( simulated.size() ) );
    TracyPlot( "Border Submaps",
               static_cast<int64_t>( all_desired.size() - simulated.size() ) );
    TracyPlot( "Total Desired Submaps", static_cast<int64_t>( all_desired.size() ) );

    // ---- Synchronous loading for newly-simulated positions ----
    std::unordered_set<quad_key, pair_hash> new_quads;
    for( const desired_key &key : simulated ) {
        if( prev_simulated_.count( key ) == 0 ) {
            new_quads.emplace( key.first, sm_to_omt_copy( key.second ) );
        }
    }

    std::vector<std::future<void>> load_futures;
    load_futures.reserve( new_quads.size() );
    for( const auto &[dim_id, om_addr] : new_quads ) {
        auto &mb = MAPBUFFER_REGISTRY.get( dim_id );
        // Skip quads already fully resident (e.g. pre-loaded by lazy border).
        // preload_quad always hits disk even for duplicates, so this avoids
        // a redundant disk read + JSON parse.
        const tripoint base = omt_to_sm_copy( om_addr );
        const bool all_loaded =
            mb.lookup_submap_in_memory( base )
            && mb.lookup_submap_in_memory( { base.x + 1, base.y, base.z } )
            &&mb.lookup_submap_in_memory( { base.x, base.y + 1, base.z } )
            &&mb.lookup_submap_in_memory( { base.x + 1, base.y + 1, base.z } );
        if( all_loaded ) {
            continue;
        }
        load_futures.push_back( get_thread_pool().submit_returning(
        [&mb, om_addr]() {
            mb.preload_quad( om_addr );
        } ) );
    }
    std::ranges::for_each( load_futures, []( auto & f ) {
        f.get();
    } );

    // Drain deferred submap destructions from the sync loads.
    auto drained_dims = std::set<std::string> {};
    std::ranges::transform( new_quads, std::inserter( drained_dims, drained_dims.end() ),
    []( const auto & qk ) {
        return qk.first;
    } );
    std::ranges::for_each( drained_dims, []( const std::string & dim_id ) {
        MAPBUFFER_REGISTRY.get( dim_id ).drain_pending_submap_destroy();
    } );

    // ---- Listener notifications (simulated set only) ----
    for( const desired_key &key : simulated ) {
        if( prev_simulated_.count( key ) == 0 ) {
            const tripoint_abs_sm pos( key.second );
            for( submap_load_listener *listener : listeners_ ) {
                listener->on_submap_loaded( pos, key.first );
            }
        }
    }

    for( const desired_key &key : prev_simulated_ ) {
        if( simulated.count( key ) == 0 ) {
            const tripoint_abs_sm pos( key.second );
            for( submap_load_listener *listener : listeners_ ) {
                listener->on_submap_unloaded( pos, key.first );
            }
        }
    }

    // ---- Eviction (full set: simulated + border) ----
    // Only evict when ALL 4 submaps in a quad are absent from all_desired.
    {
        std::unordered_set<quad_key, pair_hash> quads_checked;
        for( const desired_key &key : prev_desired_ ) {
            if( all_desired.count( key ) != 0 ) {
                continue;  // still desired — skip
            }
            const tripoint om_addr = sm_to_omt_copy( key.second );
            const quad_key qk{ key.first, om_addr };
            if( !quads_checked.insert( qk ).second ) {
                continue;  // already handled this quad in this cycle
            }
            bool any_still_desired = false;
            const tripoint base = omt_to_sm_copy( om_addr );
            for( const point &off : { point_zero, point_south, point_east, point_south_east } ) {
                const tripoint sibling{ base.x + off.x, base.y + off.y, base.z };
                if( all_desired.count( { key.first, sibling } ) ) {
                    any_still_desired = true;
                    break;
                }
            }
            if( !any_still_desired ) {
                MAPBUFFER_REGISTRY.get( key.first ).unload_quad( om_addr );
            }
        }
    }

    // ---- Async loading for lazy border (disk-only, no generation) ----
    // Submit preload_quad tasks to the thread pool for border quads not yet
    // in MAPBUFFER and not already in-flight.  Completed futures are reaped
    // non-blockingly at the start of the next update().  world_tick() uses
    // for_each_submap() (locked iteration) so workers can safely insert while
    // the main thread iterates.  preload_quad only reads from disk (no
    // generation fallback), so unvisited areas are simply skipped — they'll
    // be generated synchronously if the bubble reaches them.
    {
        std::unordered_set<quad_key, pair_hash> lazy_quads;
        for( const desired_key &key : all_desired ) {
            if( simulated.count( key ) ) {
                continue;
            }
            const quad_key qk{ key.first, sm_to_omt_copy( key.second ) };
            if( lazy_in_flight_.count( qk ) ) {
                continue;  // already has an in-flight future — don't resubmit
            }
            auto &mb = MAPBUFFER_REGISTRY.get( key.first );
            if( !mb.lookup_submap_in_memory( key.second ) ) {
                lazy_quads.insert( qk );
            }
        }
        lazy_futures_.reserve( lazy_futures_.size() + lazy_quads.size() );
        for( const auto &[dim_id, om_addr] : lazy_quads ) {
            auto &mb = MAPBUFFER_REGISTRY.get( dim_id );
            lazy_in_flight_.insert( { dim_id, om_addr } );
            lazy_futures_.emplace_back(
                quad_key{ dim_id, om_addr },
                get_thread_pool().submit_returning(
            [&mb, om_addr]() {
                mb.preload_quad( om_addr );
            } ) );
        }
    }

    TracyPlot( "Lazy Futures In-Flight", static_cast<int64_t>( lazy_futures_.size() ) );

    prev_simulated_ = std::move( simulated );
    prev_desired_ = std::move( all_desired );
}

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
        const int dx = std::abs( p.x - c.x );
        const int dy = std::abs( p.y - c.y );
        return dx <= req.radius && dy <= req.radius
               && p.z >= req.z_min && p.z <= req.z_max;
    } );
}

bool submap_load_manager::is_simulated( const std::string &dim_id,
                                        const tripoint_abs_sm &pos ) const
{
    // No request covers this position; it was loaded outside the request
    // system (e.g. direct map::load in tests).  Treat as simulated iff the
    // submap is actually resident in memory.
    if( !is_loaded( dim_id, pos ) ) { return false; }
    const tripoint p = pos.raw();
    bool covered_by_lazy_only = false;
    for( const auto &[handle, req] : requests_ ) {
        if( req.dimension_id != dim_id ) {
            continue;
        }
        const tripoint c = req.center.raw();
        const int dx = std::abs( p.x - c.x );
        const int dy = std::abs( p.y - c.y );
        if( !( dx <= req.radius && dy <= req.radius
               && p.z >= req.z_min && p.z <= req.z_max ) ) {
            continue;
        }
        if( req.source != load_request_source::lazy_border ) {
            return true;
        }
        covered_by_lazy_only = true;
    }
    if( covered_by_lazy_only ) {
        // Only covered by lazy-border requests — not simulated.
        return false;
    }
    return true;
}

bool submap_load_manager::is_loaded( const std::string &dim_id,
                                     const tripoint_abs_sm &pos ) const
{
    return MAPBUFFER_REGISTRY.get( dim_id ).lookup_submap_in_memory( pos.raw() ) != nullptr;
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
        return kv.second.source != load_request_source::reality_bubble
               && kv.second.source != load_request_source::lazy_border;
    };
    auto to_request = []( const auto & kv ) -> const submap_load_request & {
        return kv.second;
    };
    auto view = requests_ | std::views::filter( is_non_bubble )
                | std::views::transform( to_request );
    return { view.begin(), view.end() };
}

void submap_load_manager::flush_prev_desired()
{
    prev_desired_.clear();
    prev_simulated_.clear();
    prev_centers_.clear();
}

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
