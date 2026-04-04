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

    // Also drain in-flight presave futures so no worker holds submap pointers
    // across a dimension switch, shutdown, or full game save.
    // Clear dirty marks for each completed presave — the quad is now on disk, so
    // the next eviction takes the fast unload_quad(false) path instead of re-saving.
    std::ranges::for_each( presave_futures_, [this]( auto & entry ) {
        entry.second.get();
        dirty_quads_.erase( entry.first );
    } );
    presave_futures_.clear();
    presave_in_flight_.clear();

    // load_or_generate_quad workers may have deferred submap destructions.
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

    // Non-blocking reap: collect completed presave futures.  When a presave
    // finishes the quad's dirty mark is cleared — eviction only needs to free
    // the in-memory objects (no I/O stall).
    {
        ZoneScopedN( "slm_presave_reap" );
        auto it = std::remove_if( presave_futures_.begin(), presave_futures_.end(),
        [this]( auto & entry ) {
            if( entry.second.wait_for( std::chrono::seconds( 0 ) ) == std::future_status::ready ) {
                entry.second.get();
                presave_in_flight_.erase( entry.first );
                dirty_quads_.erase( entry.first );  // quad is now on disk
                return true;
            }
            return false;
        } );
        presave_futures_.erase( it, presave_futures_.end() );
    }

    // Non-blocking reap: collect completed lazy futures without stalling on
    // in-flight ones.  Workers may still be running — that is fine because
    // mapbuffer iteration in world_tick() uses for_each_submap() (holds
    // submaps_mutex_), and load_or_generate_quad() only briefly acquires it
    // per add_submap().  Eviction below also acquires the mutex via unload_quad().
    {
        ZoneScopedN( "slm_lazy_reap" );
        auto it = std::remove_if( lazy_futures_.begin(), lazy_futures_.end(),
        [this]( auto & entry ) {
            if( entry.second.wait_for( std::chrono::seconds( 0 ) ) == std::future_status::ready ) {
                // Mark dirty if mapgen ran — the quad has new content that must be
                // saved before eviction.  Quads that were already on disk return false
                // and are left clean so eviction frees them without a redundant write.
                if( entry.second.get() ) {
                    dirty_quads_.insert( entry.first );
                }
                lazy_in_flight_.erase( entry.first );
                return true;
            }
            return false;
        } );
        lazy_futures_.erase( it, lazy_futures_.end() );
    }

    // Drain deferred submap destructions from completed load_or_generate_quad workers.
    // safe_reference / cata_arena are not thread-safe, so destruction must
    // happen on the main thread.  This is always safe to call — it only
    // processes entries already in the queue from finished workers.
    {
        ZoneScopedN( "slm_drain_destroy" );
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

    // Drain any Lua mapgen postprocess hooks queued by completed lazy generation
    // workers.  Hooks are batched per-turn here rather than accumulating until
    // the next map::shift(), which avoids a large synchronous spike on the first
    // shift into a new area.
    {
        ZoneScopedN( "slm_mapgen_hooks_lazy" );
        run_deferred_mapgen_hooks();
        flush_deferred_zones();
        run_deferred_autonotes();
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

    std::vector<std::future<void>> load_futures;
    {
        ZoneScopedN( "slm_load_new_quads" );
        load_futures.reserve( new_quads.size() );
        for( const auto &[dim_id, om_addr] : new_quads ) {
            const quad_key qk_presave{ dim_id, om_addr };

            // If a lazy-border future is in-flight for this quad, drain it before
            // submitting the sync load.  Both paths call tinymap::generate(), so a
            // concurrent lazy worker would race with the sync load and cause two
            // apply_map_extra() calls on the same quad → duplicate autonotes and
            // potentially duplicate special spawns.
            if( lazy_in_flight_.count( qk_presave ) ) {
                for( auto &entry : lazy_futures_ ) {
                    if( entry.first == qk_presave ) {
                        entry.second.get();
                        break;
                    }
                }
                lazy_in_flight_.erase( qk_presave );
                lazy_futures_.erase(
                    std::remove_if( lazy_futures_.begin(), lazy_futures_.end(),
                [&qk_presave]( const auto & e ) {
                    return e.first == qk_presave;
                } ),
                lazy_futures_.end() );
            }

            // If a presave is in-flight for this quad, wait for it before allowing
            // game logic to modify the submaps.  The worker holds raw submap pointers
            // and reads them for serialization; concurrent writes would corrupt the save.
            if( presave_in_flight_.count( qk_presave ) ) {
                for( auto &entry : presave_futures_ ) {
                    if( entry.first == qk_presave ) {
                        entry.second.get();
                        break;
                    }
                }
                presave_in_flight_.erase( qk_presave );
                presave_futures_.erase(
                    std::remove_if( presave_futures_.begin(), presave_futures_.end(),
                [&qk_presave]( const auto & e ) {
                    return e.first == qk_presave;
                } ),
                presave_futures_.end() );
                // Leave dirty_quads_ intact — the quad was re-inserted above and
                // must still be saved on eventual eviction.
            }

            auto &mb = MAPBUFFER_REGISTRY.get( dim_id );
            // Skip quads already fully resident (e.g. pre-generated by lazy border).
            // load_or_generate_quad always hits disk even for duplicates, so this
            // avoids a redundant disk read + JSON parse for the already-loaded case.
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
                mb.load_or_generate_quad( om_addr );
            } ) );
        }
        std::ranges::for_each( load_futures, []( auto & f ) {
            f.get();
        } );

    } // slm_load_new_quads

    // Drain deferred submap destructions from the sync loads.
    auto drained_dims = std::set<std::string> {};
    std::ranges::transform( new_quads, std::inserter( drained_dims, drained_dims.end() ),
    []( const auto & qk ) {
        return qk.first;
    } );
    std::ranges::for_each( drained_dims, []( const std::string & dim_id ) {
        MAPBUFFER_REGISTRY.get( dim_id ).drain_pending_submap_destroy();
    } );

    // Drain Lua postprocess hooks from any async generation above.
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
            if( presave_in_flight_.count( qk ) ) {
                continue;  // already has an in-flight presave
            }
            if( !dirty_quads_.count( qk ) ) {
                continue;  // quad was never simulated (guard, shouldn't happen here)
            }
            if( !presaved_this_turn.insert( qk ).second ) {
                continue;  // multiple submaps map to the same quad — only submit once
            }
            auto &mb = MAPBUFFER_REGISTRY.get( qk.first );
            presave_in_flight_.insert( qk );
            presave_futures_.emplace_back(
                qk,
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
                // If a lazy future is still in-flight for this quad, drain it now
                // before consulting dirty_quads_.  The future may have run mapgen
                // and will return true, but the lazy reap (non-blocking) could have
                // missed it if it completed after the reap already ran this turn.
                // Draining here ensures the dirty mark is applied before we decide
                // whether to save on eviction.
                if( lazy_in_flight_.count( qk ) ) {
                    for( auto &entry : lazy_futures_ ) {
                        if( entry.first == qk ) {
                            if( entry.second.get() ) {
                                dirty_quads_.insert( qk );
                            }
                            break;
                        }
                    }
                    lazy_in_flight_.erase( qk );
                    lazy_futures_.erase(
                        std::remove_if( lazy_futures_.begin(), lazy_futures_.end(),
                    [&qk]( const auto & e ) {
                        return e.first == qk;
                    } ),
                    lazy_futures_.end() );
                }

                const bool was_dirty = dirty_quads_.count( qk ) > 0;
                if( was_dirty ) {
                    if( presave_in_flight_.count( qk ) ) {
                        // A presave worker still holds raw pointers to these submaps.
                        // Wait for it to finish before freeing them.  This path should
                        // be rare — presaves normally complete between two update() calls.
                        for( auto &entry : presave_futures_ ) {
                            if( entry.first == qk ) {
                                entry.second.get();
                                break;
                            }
                        }
                        presave_in_flight_.erase( qk );
                        presave_futures_.erase(
                            std::remove_if( presave_futures_.begin(), presave_futures_.end(),
                        [&qk]( const auto & e ) {
                            return e.first == qk;
                        } ),
                        presave_futures_.end() );
                        dirty_quads_.erase( qk );
                        // Quad was presaved — evict without a redundant write.
                        MAPBUFFER_REGISTRY.get( key.first ).unload_quad( om_addr, false );
                    } else {
                        // No presave was submitted for this quad (direct sim→evict path).
                        // Save synchronously before evicting.
                        dirty_quads_.erase( qk );
                        MAPBUFFER_REGISTRY.get( key.first ).unload_quad( om_addr, true );
                    }
                } else {
                    // Not dirty: either never simulated, or presave was already reaped
                    // (dirty mark cleared in slm_presave_reap) — evict without I/O.
                    MAPBUFFER_REGISTRY.get( key.first ).unload_quad( om_addr, false );
                }
            }
        }
    }

    // ---- Async load-or-generate for lazy border ----
    {
        ZoneScopedN( "slm_lazy_submit" );
        // Submit load_or_generate_quad tasks to the thread pool for border quads not
        // yet in MAPBUFFER and not already in-flight.  Spreading both disk loads and
        // first-time mapgen across worker threads reduces hitching when the player
        // crosses into new areas.
        //
        // Newly-generated quads (load_or_generate_quad returns true) are marked dirty
        // in the lazy reap above, so they are saved to disk on eviction just like
        // simulated quads.  This prevents the "rocking at a quad boundary" problem:
        // without saving, the same never-persisted quad would be regenerated on each
        // border pass, accumulating mongroups and firing duplicate autonotes.
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
                    return mb.load_or_generate_quad( om_addr );
                } ) );
            }
        }
    } // slm_lazy_submit

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

void submap_load_manager::flush_prev_desired()
{
    prev_desired_.clear();
    prev_simulated_.clear();
    prev_centers_.clear();
    dirty_quads_.clear();
    // Presave futures should be drained before this point (via drain_lazy_loads).
    // Clear defensively to prevent stale entries after a dimension switch.
    presave_futures_.clear();
    presave_in_flight_.clear();
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
