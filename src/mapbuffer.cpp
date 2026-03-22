#include "mapbuffer.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <exception>
#include <functional>
#include <memory>
#include <mutex>
#include <set>
#include <sstream>
#include <utility>
#include <vector>

#include "calendar.h"
#include "cata_utility.h"
#include "coordinate_conversions.h"
#include "debug.h"
#include "distribution_grid.h"
#include "filesystem.h"
#include "fstream_utils.h"
#include "game.h"
#include "game_constants.h"
#include "json.h"
#include "map.h"
#include "output.h"
#include "popup.h"
#include "profile.h"
#include "string_formatter.h"
#include "submap.h"
#include "thread_pool.h"
#include "translations.h"
#include "ui_manager.h"
#include "world.h"

mapbuffer::mapbuffer() = default;
mapbuffer::~mapbuffer() = default;

void mapbuffer::clear()
{
    submaps.clear();
}

bool mapbuffer::add_submap( const tripoint &p, std::unique_ptr<submap> &sm )
{
    std::lock_guard<std::recursive_mutex> lk( submaps_mutex_ );
    if( submaps.contains( p ) ) {
        return false;
    }

    submaps[p] = std::move( sm );

    return true;
}

bool mapbuffer::add_submap( const tripoint &p, submap *sm )
{
    // FIXME: get rid of this overload and make submap ownership semantics sane.
    std::unique_ptr<submap> temp( sm );
    bool result = add_submap( p, temp );
    if( !result ) {
        // NOLINTNEXTLINE( bugprone-unused-return-value )
        temp.release();
    }
    return result;
}

void mapbuffer::remove_submap( tripoint addr )
{
    auto m_target = submaps.find( addr );
    if( m_target == submaps.end() ) {
        debugmsg( "Tried to remove non-existing submap %s", addr.to_string() );
        return;
    }
    // Safety: skip freeing if map::grid[] still references this submap.
    if( g != nullptr && m_target->second ) {
        const submap *doomed = m_target->second.get();
        const map &here = get_map();
        const auto &grid_vec = here.grid;
        for( size_t i = 0; i < grid_vec.size(); ++i ) {
            if( grid_vec[i] == doomed ) {
                debugmsg( "remove_submap: skipping free of submap at %s (ptr %p) "
                          "— map::grid[%zu] still references it (dim='%s')",
                          addr.to_string(), static_cast<const void *>( doomed ),
                          i, dimension_id_ );
                return;  // do NOT erase — prevent use-after-free
            }
        }
    }
    submaps.erase( m_target );
}

void mapbuffer::transfer_all_to( mapbuffer &dest )
{
    for( auto &kv : submaps ) {
        if( dest.submaps.count( kv.first ) ) {
            // Destination already has a submap at this position.  This should
            // never happen when the callers (capture_from_primary /
            // restore_to_primary) clear the destination first.  Log an error
            // and keep the destination entry rather than silently losing either.
            debugmsg( "transfer_all_to: collision at %s; destination entry retained, source lost",
                      kv.first.to_string() );
            continue;
        }
        dest.submaps.emplace( kv.first, std::move( kv.second ) );
    }
    submaps.clear();
}

submap *mapbuffer::load_submap( const tripoint_abs_sm &pos )
{
    ZoneScoped;
    // lookup_submap already handles the disk-read path transparently.
    return lookup_submap( pos.raw() );
}

void mapbuffer::unload_submap( const tripoint_abs_sm &pos )
{
    ZoneScoped;
    const tripoint &p = pos.raw();
    if( !submaps.contains( p ) ) {
        return;
    }

    // Save the quad containing this submap to disk before evicting it.
    const tripoint om_addr = sm_to_omt_copy( p );
    std::list<tripoint> ignored_delete;
    // Save without deleting the other three submaps from the buffer —
    // only this specific submap is being evicted by the caller.
    save_quad( om_addr, ignored_delete, false );

    remove_submap( p );
}

void mapbuffer::unload_quad( const tripoint &om_addr, bool save )
{
    // Hold the mutex for the entire save+erase so that background lazy-border
    // preload_quad() workers (which acquire the mutex per add_submap()) cannot
    // race with our submaps.find()/erase() calls.
    std::lock_guard<std::recursive_mutex> lk( submaps_mutex_ );
    std::list<tripoint> to_delete;
    if( save ) {
        // Save the quad once and collect all in-memory submaps for deletion.
        // Using delete_after_save=true ensures save_quad() enumerates what to delete
        // so we don't need to recompute the 4 addresses separately.
        save_quad( om_addr, to_delete, /*delete_after_save=*/true );
    } else {
        // Border-only quad: content is identical to what is already on disk.
        // Skip serialisation; just collect the four submap addresses to discard.
        const tripoint base = omt_to_sm_copy( om_addr );
        for( const point &off : { point_zero, point_south, point_east, point_south_east } ) {
            to_delete.push_back( { base.x + off.x, base.y + off.y, base.z } );
        }
    }
    // Safety: skip freeing submaps that map::grid[] still references.
    // This prevents use-after-free when submap_loader eviction races with
    // map::shift() / copy_grid() during large map shifts (e.g. pocket entry).
    if( g != nullptr ) {
        const map &here = get_map();
        const auto &grid_vec = here.grid;
        to_delete.remove_if( [&]( const tripoint & p ) {
            const auto it = submaps.find( p );
            if( it == submaps.end() || !it->second ) {
                return false;
            }
            const submap *doomed = it->second.get();
            for( size_t i = 0; i < grid_vec.size(); ++i ) {
                if( grid_vec[i] == doomed ) {
                    debugmsg( "unload_quad: skipping free of submap at %s (ptr %p) "
                              "— map::grid[%zu] still references it (dim='%s')",
                              p.to_string(), static_cast<const void *>( doomed ),
                              i, dimension_id_ );
                    return true;  // remove from to_delete → keep alive
                }
            }
            return false;
        } );
    }
    for( const tripoint &p : to_delete ) {
        submaps.erase( p );
    }
}

submap *mapbuffer::lookup_submap( const tripoint &p )
{
    // Hold submaps_mutex_ for the entire call so that concurrent background
    // add_submap() calls cannot race with our submaps.find() or the subsequent
    // unserialize_submaps() → add_submap() path.  std::recursive_mutex allows
    // the nested add_submap() call (inside unserialize_submaps) to re-acquire.
    std::lock_guard<std::recursive_mutex> lk( submaps_mutex_ );
    const auto iter = submaps.find( p );
    if( iter == submaps.end() ) {
        try {
            return unserialize_submaps( p );
        } catch( const std::exception &err ) {
            debugmsg( "Failed to load submap %s: %s", p.to_string(), err.what() );
        }
        return nullptr;
    }

    return iter->second.get();
}

void mapbuffer::save( bool delete_after_save, bool notify_tracker, bool show_progress )
{
    const int num_total_submaps = static_cast<int>( submaps.size() );

    // Spatial eviction only makes sense for the dimension the player is
    // currently in — it has a reality bubble whose origin defines which quads
    // are "near" enough to keep resident.  Non-current dimensions have no
    // bubble, so their submaps are kept in memory (the submap_load_manager
    // handles eviction for those independently).
    const bool is_current_dimension =
        g != nullptr && dimension_id_ == get_map().get_bound_dimension();

    map &here = get_map();
    const tripoint map_origin = is_current_dimension
                                ? sm_to_omt_copy( here.get_abs_sub() )
                                : tripoint_zero;
    const bool map_has_zlevels = g != nullptr && here.has_zlevels();

    // Serial collection of unique OMT quad addresses with per-quad delete flags.
    // The UI progress popup runs here on the main thread only (show_progress=true).
    // When save() is dispatched from a worker thread (show_progress=false), the popup
    // is skipped to avoid calling UI functions off the main thread.
    struct quad_entry {
        tripoint om_addr;
        bool     delete_after;
    };
    std::vector<quad_entry> quads_to_process;
    {
        std::set<tripoint> seen_quads;
        int num_processed = 0;
        std::unique_ptr<static_popup> popup;
        if( show_progress ) {
            popup = std::make_unique<static_popup>();
        }
        static constexpr std::chrono::milliseconds update_interval( 500 );
        auto last_update = std::chrono::steady_clock::now();

        for( auto &[pos, sm_ptr] : submaps ) {
            if( show_progress ) {
                const auto now = std::chrono::steady_clock::now();
                if( last_update + update_interval < now ) {
                    popup->message( _( "Please wait as the map saves [%d/%d]" ),
                                    num_processed, num_total_submaps );
                    ui_manager::redraw();
                    refresh_display();
                    inp_mngr.pump_events();
                    last_update = now;
                }
            }
            ++num_processed;

            const tripoint om_addr = sm_to_omt_copy( pos );
            if( !seen_quads.insert( om_addr ).second ) {
                continue;
            }

            bool quad_delete = delete_after_save;
            if( is_current_dimension ) {
                // Submaps outside the current map bounds or on wrong z-level
                // are deleted from memory after saving.
                const bool zlev_del = !map_has_zlevels && om_addr.z != g->get_levz();
                quad_delete = quad_delete || zlev_del ||
                              om_addr.x < map_origin.x ||
                              om_addr.y < map_origin.y ||
                              om_addr.x > map_origin.x + g_half_mapsize ||
                              om_addr.y > map_origin.y + g_half_mapsize;
            }

            quads_to_process.push_back( { om_addr, quad_delete } );
        }
    }

    // Write non-uniform quads in parallel. Each write targets a distinct file/key,
    // so there are no shared-state concerns between concurrent save_quad() calls.
    // save_quad() uses submaps.find() for read-only access (safe for concurrent reads).
    // Per-task local_delete lists are merged into the shared list under a mutex.
    std::list<tripoint> submaps_to_delete;
    std::mutex delete_mutex;

    parallel_for( 0, static_cast<int>( quads_to_process.size() ), [&]( int i ) {
        std::list<tripoint> local_delete;
        save_quad( quads_to_process[i].om_addr, local_delete, quads_to_process[i].delete_after );
        if( !local_delete.empty() ) {
            std::lock_guard<std::mutex> lk( delete_mutex );
            submaps_to_delete.splice( submaps_to_delete.end(), local_delete );
        }
    } );

    // Evict submaps from memory. std::map mutation is not thread-safe,
    // so this is done serially after the parallel write phase completes.
    for( const tripoint &pos : submaps_to_delete ) {
        remove_submap( pos );
    }

    // Notify the distribution grid tracker for each evicted submap.
    if( notify_tracker ) {
        auto &tracker = get_distribution_grid_tracker();
        for( const tripoint &pos : submaps_to_delete ) {
            tracker.on_submap_unloaded( tripoint_abs_sm( pos ), "" );
        }
    }
}

void mapbuffer::save_quad( const tripoint &om_addr, std::list<tripoint> &submaps_to_delete,
                           bool delete_after_save )
{
    ZoneScoped;
    // Build the 4 submap addresses that form this OMT quad.
    std::vector<tripoint> submap_addrs;
    submap_addrs.reserve( 4 );
    for( const point &off : { point_zero, point_south, point_east, point_south_east } ) {
        tripoint submap_addr = omt_to_sm_copy( om_addr );
        submap_addr.x += off.x;
        submap_addr.y += off.y;
        submap_addrs.push_back( submap_addr );
    }

    // Use find() throughout (not operator[]) so this function is safe to call
    // from multiple threads concurrently for distinct om_addr values.
    // operator[] would insert a default entry for missing keys, mutating the map.
    bool all_uniform = true;
    for( const tripoint &submap_addr : submap_addrs ) {
        const auto it = submaps.find( submap_addr );
        if( it != submaps.end() && it->second && !it->second->is_uniform ) {
            all_uniform = false;
            break;
        }
    }

    if( all_uniform ) {
        // Nothing to save — this quad will be regenerated faster than it would be re-read.
        if( delete_after_save ) {
            for( const tripoint &submap_addr : submap_addrs ) {
                const auto it = submaps.find( submap_addr );
                if( it != submaps.end() && it->second ) {
                    submaps_to_delete.push_back( submap_addr );
                }
            }
        }
        return;
    }

    if( disable_mapgen ) {
        return;
    }

    g->get_active_world()->write_map_quad( dimension_id_, om_addr, [&]( std::ostream & fout ) {
        JsonOut jsout( fout );
        jsout.start_array();
        for( const tripoint &submap_addr : submap_addrs ) {
            const auto it = submaps.find( submap_addr );
            if( it == submaps.end() ) {
                continue;
            }

            submap *sm = it->second.get();
            if( sm == nullptr ) {
                continue;
            }

            jsout.start_object();

            jsout.member( "version", savegame_version );
            jsout.member( "coordinates" );

            jsout.start_array();
            jsout.write( submap_addr.x );
            jsout.write( submap_addr.y );
            jsout.write( submap_addr.z );
            jsout.end_array();

            sm->store( jsout );

            jsout.end_object();

            if( delete_after_save ) {
                submaps_to_delete.push_back( submap_addr );
            }
        }

        jsout.end_array();
    } );
}

// We're reading in way too many entities here to mess around with creating sub-objects and
// seeking around in them, so we're using the json streaming API.
submap *mapbuffer::unserialize_submaps( const tripoint &p )
{
    // Map the tripoint to the submap quad that stores it.
    const tripoint om_addr = sm_to_omt_copy( p );

    using namespace std::placeholders;
    if( !g->get_active_world()->read_map_quad( dimension_id_, om_addr,
            std::bind( &mapbuffer::deserialize, this, _1 ) ) ) {
        // If it doesn't exist, trigger generating it.
        return nullptr;
    }
    if( !submaps.contains( p ) ) {
        debugmsg( "file did not contain the expected submap %d,%d,%d",
                  p.x, p.y, p.z );
        return nullptr;
    }
    return submaps[ p ].get();
}

void mapbuffer::deserialize_into_vec(
    JsonIn &jsin,
    std::vector<std::pair<tripoint, std::unique_ptr<submap>>> &out,
    const std::function<bool( const tripoint & )> &skip_if )
{
    jsin.start_array();
    while( !jsin.end_array() ) {
        std::unique_ptr<submap> sm;
        tripoint submap_coordinates;
        jsin.start_object();
        auto version = 0;
        auto skip = false;
        while( !jsin.end_object() ) {
            auto submap_member_name = jsin.get_member_name();
            if( submap_member_name == "version" ) {
                version = jsin.get_int();
            } else if( submap_member_name == "coordinates" ) {
                jsin.start_array();
                auto i = jsin.get_int();
                auto j = jsin.get_int();
                auto k = jsin.get_int();
                tripoint loc{ i, j, k };
                jsin.end_array();
                submap_coordinates = loc;
                if( skip_if && skip_if( loc ) ) {
                    skip = true;
                } else {
                    sm = std::make_unique<submap>( sm_to_ms_copy( submap_coordinates ) );
                }
            } else if( skip ) {
                jsin.skip_value();
            } else {
                if( !sm ) { //This whole thing is a nasty hack that relys on coordinates coming first...
                    debugmsg( "coordinates was not at the top of submap json" );
                }
                sm->load( jsin, submap_member_name, version, multiply_xy( submap_coordinates, 12 ) );
            }
        }
        if( !skip ) {
            out.emplace_back( submap_coordinates, std::move( sm ) );
        }
    }
}

void mapbuffer::deserialize( JsonIn &jsin )
{
    std::vector<std::pair<tripoint, std::unique_ptr<submap>>> loaded;
    // submaps_mutex_ is already held (recursive_mutex via lookup_submap),
    // so lookup_submap_in_memory re-acquires safely.
    deserialize_into_vec( jsin, loaded, [this]( const tripoint & p ) {
        return lookup_submap_in_memory( p ) != nullptr;
    } );
    for( auto &[pos, sm] : loaded ) {
        if( !add_submap( pos, sm ) ) {
            // In-memory version takes precedence; the disk entry is stale.
            // This can happen legitimately when a quad is partially reloaded after
            // unload_submap() broke quad consistency (pre-unload_quad fix).
            // With quad-level eviction (unload_quad) this should not occur in normal play.
            DebugLog( DL::Warn, DC::Map ) << string_format(
                                              "submap %d,%d,%d was already loaded; keeping in-memory version",
                                              pos.x, pos.y, pos.z );
        }
    }
}

void mapbuffer::preload_quad( const tripoint &om_addr )
{
    ZoneScoped;
    // Disk I/O and JSON parsing — runs outside submaps_mutex_ so
    // different quads can be prefetched concurrently on worker threads.
    std::vector<std::pair<tripoint, std::unique_ptr<submap>>> loaded;
    using namespace std::placeholders;
    // Skip submaps already resident in memory during deserialization.
    // This avoids the expensive sm->load() (items, vehicles, terrain construction)
    // for submaps that were already loaded by a prior lazy-border or sync pass.
    auto already_loaded = [this]( const tripoint & p ) {
        return lookup_submap_in_memory( p ) != nullptr;
    };
    g->get_active_world()->read_map_quad( dimension_id_, om_addr,
    [this, &loaded, &already_loaded]( JsonIn & jsin ) {
        deserialize_into_vec( jsin, loaded, already_loaded );
    } );

    // Add parsed submaps to the in-memory buffer under submaps_mutex_.
    // add_submap() handles concurrent duplicate-add gracefully (keeps in-memory version).
    for( auto &[pos, sm] : loaded ) {
        if( !add_submap( pos, sm ) ) {
            DebugLog( DL::Warn, DC::Map ) << string_format(
                                              "preload_quad: submap %d,%d,%d already loaded; keeping in-memory version",
                                              pos.x, pos.y, pos.z );
            // Do NOT let sm destruct here on the worker thread.  Submap/item destruction
            // touches safe_reference<T>::records_by_pointer, cache_reference<T>::reference_map,
            // and cata_arena<T>::pending_deletion — all unsynchronised global statics.
            // Defer to drain_pending_submap_destroy(), called on the main thread after join.
            if( sm ) {
                auto lk = std::lock_guard( pending_destroy_mutex_ );
                pending_destroy_submaps_.push_back( std::move( sm ) );
            }
        }
    }
}

void mapbuffer::generate_quad( const tripoint &om_addr )
{
    ZoneScoped;
    const tripoint base = omt_to_sm_copy( om_addr );
    const bool all_loaded =
        lookup_submap_in_memory( base )
        && lookup_submap_in_memory( { base.x + 1, base.y,     base.z } )
        &&lookup_submap_in_memory( { base.x,     base.y + 1, base.z } )
        &&lookup_submap_in_memory( { base.x + 1, base.y + 1, base.z } );
    if( all_loaded ) {
        return;
    }
    tinymap tmp_map;
    tmp_map.bind_dimension( dimension_id_ );
    tmp_map.generate( base, calendar::turn );
}

void mapbuffer::load_or_generate_quad( const tripoint &om_addr )
{
    ZoneScoped;
    preload_quad( om_addr );
    generate_quad( om_addr );
}

void mapbuffer::presave_quad( const tripoint &om_addr )
{
    ZoneScoped;
    const tripoint base = omt_to_sm_copy( om_addr );
    const std::array<tripoint, 4> addrs = { {
            base,
            { base.x + 1, base.y,     base.z },
            { base.x,     base.y + 1, base.z },
            { base.x + 1, base.y + 1, base.z },
        }
    };

    // Collect raw submap pointers under the lock — brief hold only.
    std::array<submap *, 4> ptrs = {};
    bool all_uniform = true;
    {
        std::lock_guard<std::recursive_mutex> lk( submaps_mutex_ );
        for( int i = 0; i < 4; ++i ) {
            const auto it = submaps.find( addrs[i] );
            if( it != submaps.end() && it->second ) {
                ptrs[i] = it->second.get();
                if( !it->second->is_uniform ) {
                    all_uniform = false;
                }
            }
        }
    }

    // Uniform quads regenerate faster than a disk round-trip.
    if( all_uniform || disable_mapgen ) {
        return;
    }

    // Serialize and write outside the lock.  The submap objects stay alive
    // in the mapbuffer until after this future resolves (submap_load_manager
    // withholds eviction until the presave completes).
    g->get_active_world()->write_map_quad( dimension_id_, om_addr, [&]( std::ostream & fout ) {
        JsonOut jsout( fout );
        jsout.start_array();
        for( int i = 0; i < 4; ++i ) {
            submap *sm = ptrs[i];
            if( !sm ) {
                continue;
            }
            jsout.start_object();
            jsout.member( "version", savegame_version );
            jsout.member( "coordinates" );
            jsout.start_array();
            jsout.write( addrs[i].x );
            jsout.write( addrs[i].y );
            jsout.write( addrs[i].z );
            jsout.end_array();
            sm->store( jsout );
            jsout.end_object();
        }
        jsout.end_array();
    } );
}

auto mapbuffer::drain_pending_submap_destroy() -> void
{
    auto to_destroy = std::vector<std::unique_ptr<submap>> {};
    {
        auto lk = std::lock_guard( pending_destroy_mutex_ );
        to_destroy = std::move( pending_destroy_submaps_ );
    }
    // unique_ptrs destruct here, on the main thread.
}
