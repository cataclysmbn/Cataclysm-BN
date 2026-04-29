#include "submap_load_manager.h"

#include <algorithm>
#include <cassert>
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
#include "game_constants.h"
#include "mapbuffer.h"
#include "clzones.h"
#include "mapgen_async.h"
#include "mapbuffer_registry.h"
#include "point.h"
#include "profile.h"
#include "thread_pool.h"

submap_load_manager submap_loader;

load_request_handle submap_load_manager::request_load(
    load_request_source source,
    const std::string &dim_id,
    const tripoint_abs_sm &center,
    int radius )
{
    const load_request_handle handle = next_handle_++;
    submap_load_request req;
    req.source = source;
    req.dimension_id = dim_id;
    req.center = center;
    req.radius = radius;
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
        const auto z_range = std::views::iota( -OVERMAP_DEPTH, OVERMAP_HEIGHT + 1 );

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
        const auto z_range = std::views::iota( -OVERMAP_DEPTH, OVERMAP_HEIGHT + 1 );
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
    // Drain in-flight presave futures so no worker holds raw submap pointers
    // across a dimension switch, shutdown, or full game save.
    // dirty_quads_ is NOT cleared here — the presaved data is in pending_writes_
    // (in-memory cache), not on disk yet.  flush_prev_desired() clears dirty_quads_
    // and the subsequent mapbuffer::save() call flushes pending_writes_ to disk.
    std::ranges::for_each( presave_futures_, []( auto & entry ) {
        entry.second.get();
    } );
    presave_futures_.clear();
}

void submap_load_manager::update()
{
    ZoneScoped;

    // Non-blocking reap: collect completed presave futures.  When a presave
    // finishes we remove it from the in-flight set, but intentionally keep
    // dirty_quads_ intact.  presave_quad() only writes to the pending-writes
    // cache (no disk I/O); between the snapshot and eventual eviction, border
    // submaps can still be modified (e.g. fire spreading in from the bubble).
    // Keeping the dirty mark ensures eviction re-serialises the current state
    // rather than silently discarding those post-presave modifications.
    {
        ZoneScopedN( "slm_presave_reap" );
        std::erase_if( presave_futures_, []( auto & entry ) {
            auto &[key, fut] = entry;
            if( fut.wait_for( std::chrono::seconds( 0 ) ) == std::future_status::ready ) {
                fut.get();
                // dirty_quads_ deliberately NOT cleared here — see comment above.
                return true;
            }
            return false;
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
    key_set simulated;
    key_set all_desired;
    {
        ZoneScopedN( "slm_compute_sets" );
        simulated = compute_desired_set();
        all_desired = simulated;
        compute_border_into( all_desired );
    }

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

    // Mark newly-simulated quads as dirty: they will receive game logic and
    // must be saved to disk when evicted.
    std::ranges::for_each( new_quads, [&]( const auto & qk ) {
        dirty_quads_.insert( qk );
    } );

    // ---- Step 1: parallel disk preload for newly-simulated quads ----
    // preload_quad() is thread-safe (disk I/O outside submaps_mutex_; add
    // under the lock).  Running multiple quads in parallel hides disk latency.
    {
        ZoneScopedN( "slm_preload_new_quads" );
        std::vector<std::future<void>> preload_futures;
        preload_futures.reserve( new_quads.size() );
        for( const auto &[dim_id, om_addr] : new_quads ) {
            const quad_key qk{ dim_id, om_addr };

            // If a presave is in-flight for this quad, wait for it before allowing
            // game logic to modify the submaps.  The worker holds raw submap pointers
            // and reads them for serialization; concurrent writes would corrupt the save.
            if( auto it = presave_futures_.find( qk ); it != presave_futures_.end() ) {
                it->second.get();
                presave_futures_.erase( it );
                // Leave dirty_quads_ intact — the quad was re-inserted above and
                // must still be saved on eventual eviction.
            }

            auto &mb = MAPBUFFER_REGISTRY.get( dim_id );
            // Skip quads already fully resident (e.g. re-entered from pending_writes cache).
            const tripoint base = omt_to_sm_copy( om_addr );
            const bool all_loaded =
                mb.lookup_submap_in_memory( base )
                && mb.lookup_submap_in_memory( { base.x + 1, base.y, base.z } )
                && mb.lookup_submap_in_memory( { base.x, base.y + 1, base.z } )
                && mb.lookup_submap_in_memory( { base.x + 1, base.y + 1, base.z } );
            if( all_loaded ) {
                continue;
            }
            preload_futures.push_back( get_thread_pool().submit_returning(
            [&mb, om_addr]() {
                mb.preload_quad( om_addr );
            } ) );
        }
        std::ranges::for_each( preload_futures, []( auto & f ) {
            f.get();
        } );
    } // slm_preload_new_quads

    // Drain duplicate submaps created by concurrent preload_quad workers.
    // Must happen on the main thread (safe_reference / cata_arena not thread-safe).
    {
        auto drained_dims = std::set<std::string>{};
        std::ranges::transform( new_quads, std::inserter( drained_dims, drained_dims.end() ),
        []( const auto & qk ) { return qk.first; } );
        std::ranges::for_each( drained_dims, []( const std::string & dim_id ) {
            MAPBUFFER_REGISTRY.get( dim_id ).drain_pending_submap_destroy();
        } );
    }

    // ---- Step 2: synchronous mapgen on the main thread ----
    // generate_quad() calls tinymap::generate() which may invoke Lua mapgen.
    // Lua is not reentrant, so this must always run on the main thread.
    // Skip quads already fully resident: preload_quad loaded them from disk or
    // the pending_writes cache, so no generation is needed.  Checking here avoids
    // the registry lookup + coordinate projection + 4 hash probes inside
    // generate_quad() for the common case (already-visited terrain).
    {
        ZoneScopedN( "slm_generate_new_quads" );
        for( const auto &[dim_id, om_addr] : new_quads ) {
            auto &mb = MAPBUFFER_REGISTRY.get( dim_id );
            const tripoint base = omt_to_sm_copy( om_addr );
            const bool all_loaded =
                mb.lookup_submap_in_memory( base )
                && mb.lookup_submap_in_memory( { base.x + 1, base.y, base.z } )
                && mb.lookup_submap_in_memory( { base.x, base.y + 1, base.z } )
                && mb.lookup_submap_in_memory( { base.x + 1, base.y + 1, base.z } );
            if( !all_loaded ) {
                mb.generate_quad( om_addr );
            }
        }
    }

    // Drain Lua postprocess hooks queued by mapgen above.
    {
        ZoneScopedN( "slm_mapgen_hooks_sim" );
        run_deferred_mapgen_hooks();
        flush_deferred_zones();
        run_deferred_autonotes();
    }

    // ---- Listener notifications (simulated set only) ----
    {
        ZoneScopedN( "slm_listener_notifications" );
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
    } // slm_listener_notifications

    // ---- Submit async presaves for dirty quads leaving simulation ----
    // Quads entering the border zone are no longer touched by game logic.
    // We can serialize them to disk on a worker thread so that eviction
    // (when they later leave the border zone) is just a fast memory free.
    {
        ZoneScopedN( "slm_presave_departing" );
        std::unordered_set<quad_key, pair_hash> presaved_this_turn;
        for( const desired_key &key : prev_simulated_ ) {
            if( simulated.count( key ) ) {
                continue;  // still simulated — not departing
            }
            if( !all_desired.count( key ) ) {
                continue;  // direct sim→evict; handled synchronously in eviction below
            }
            const quad_key qk{ key.first, sm_to_omt_copy( key.second ) };
            if( presave_futures_.count( qk ) ) {
                continue;  // already has an in-flight presave
            }
            if( !dirty_quads_.count( qk ) ) {
                continue;  // quad was never simulated (guard, shouldn't happen here)
            }
            if( !presaved_this_turn.insert( qk ).second ) {
                continue;  // multiple submaps map to the same quad — only submit once
            }
            auto &mb = MAPBUFFER_REGISTRY.get( qk.first );
            presave_futures_.emplace( qk,
            get_thread_pool().submit_returning( [&mb, om_addr = qk.second]() {
                mb.presave_quad( om_addr );
            } ) );
        }
        TracyPlot( "Presave Futures In-Flight", static_cast<int64_t>( presave_futures_.size() ) );
    }

    // ---- Eviction (full set: simulated + border) ----
    // Only evict when ALL 4 submaps in a quad are absent from all_desired.
    {
        ZoneScopedN( "slm_eviction" );
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
                const bool was_dirty = dirty_quads_.count( qk ) > 0;
                if( was_dirty ) {
                    if( auto it = presave_futures_.find( qk ); it != presave_futures_.end() ) {
                        // A presave worker still holds raw pointers to these submaps.
                        // Wait for it to finish before re-serialising and freeing.
                        // This path should be rare — presaves normally complete between
                        // two update() calls.
                        it->second.get();
                        presave_futures_.erase( it );
                    }
                    // Serialise the current submap state before evicting.  This
                    // intentionally re-serialises even when a presave already ran, to
                    // capture modifications made after the presave snapshot (e.g. fire
                    // spreading into a border submap).  The presave wrote an earlier
                    // snapshot to pending_writes_; unload_quad(true) overwrites it with
                    // the fresh state — no data is lost.
                    dirty_quads_.erase( qk );
                    MAPBUFFER_REGISTRY.get( key.first ).unload_quad( om_addr, true );
                } else {
                    // Not dirty: quad was never simulated — evict without I/O.
                    MAPBUFFER_REGISTRY.get( key.first ).unload_quad( om_addr, false );
                }
            }
        }
    }

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
        return dx <= req.radius && dy <= req.radius;
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
        if( !( dx <= req.radius && dy <= req.radius ) ) {
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
    // No request covers this position.  Two distinct cases:
    //   • requests_ is empty  — map was loaded directly (e.g. in tests via
    //     map::load) without going through the request system.  Treat the
    //     submap as simulated so items, fields, and NPCs are processed normally.
    //   • requests_ is non-empty — the submap was loaded as a quad-alignment
    //     overflow beyond the lazy-border zone (odd bubble size forces an extra
    //     row/column of submaps to be resident).  It should not be simulated.
    return requests_.empty();
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

auto submap_load_manager::is_fully_drained() const noexcept -> bool
{
    return presave_futures_.empty();
}

void submap_load_manager::flush_prev_desired()
{
    assert( is_fully_drained() );
    prev_desired_.clear();
    prev_simulated_.clear();
    prev_centers_.clear();
    dirty_quads_.clear();
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
