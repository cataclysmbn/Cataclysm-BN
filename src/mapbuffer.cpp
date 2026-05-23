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
    std::lock_guard<std::mutex> pw_lk( pending_writes_mutex_ );
    pending_writes_.clear();
}

bool mapbuffer::add_submap( const tripoint_abs_sm &p, std::unique_ptr<submap> &sm )
{
    std::lock_guard<std::recursive_mutex> lk( submaps_mutex_ );
    if( submaps.contains( p ) ) {
        return false;
    }

    submaps[p] = std::move( sm );

    return true;
}

void mapbuffer::remove_submap( tripoint_abs_sm addr )
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
    return lookup_submap( pos );
}

void mapbuffer::unload_omt( const tripoint_abs_omt &omt_addr, bool save )
{
    // Hold the mutex for the entire save+erase so that background lazy-border
    // preload_omt() workers (which acquire the mutex per add_submap()) cannot
    // race with our submaps.find()/erase() calls.
    std::lock_guard<std::recursive_mutex> lk( submaps_mutex_ );
    std::list<tripoint_abs_sm> to_delete;
    if( save ) {
        // Serialise into the pending-writes cache (no disk I/O).  The data will
        // be flushed to disk on the next explicit save.
        const auto base = project_to<coords::sm>( omt_addr );
        const std::array<tripoint_abs_sm, 4> addrs = { {
                base,
                base + point_rel_sm::east(),
                base + point_rel_sm::south(),
                base + point_rel_sm::south_east()
            }
        };

        bool all_uniform = true;
        for( const tripoint_abs_sm &addr : addrs ) {
            const auto it = submaps.find( addr );
            if( it != submaps.end() && it->second && !it->second->is_uniform ) {
                all_uniform = false;
                break;
            }
        }

        if( !all_uniform && !disable_mapgen ) {
            std::ostringstream buf;
            {
                JsonOut jsout( buf );
                jsout.start_array();
                for( const tripoint_abs_sm &addr : addrs ) {
                    const auto it = submaps.find( addr );
                    if( it == submaps.end() || !it->second ) {
                        continue;
                    }
                    jsout.start_object();
                    jsout.member( "version", savegame_version );
                    jsout.member( "coordinates" );
                    jsout.start_array();
                    jsout.write( addr.x() );
                    jsout.write( addr.y() );
                    jsout.write( addr.z() );
                    jsout.end_array();
                    it->second->store( jsout );
                    jsout.end_object();
                }
                jsout.end_array();
            }
            std::lock_guard<std::mutex> pw_lk( pending_writes_mutex_ );
            pending_writes_[omt_addr] = std::move( buf ).str();
        }

        for( const tripoint_abs_sm &addr : addrs ) {
            if( submaps.contains( addr ) ) {
                to_delete.push_back( addr );
            }
        }
    } else {
        // Border-only omt: content is identical to what is already on disk.
        // Skip serialisation; just collect the four submap addresses to discard.
        const auto base = project_to<coords::sm>( omt_addr );
        for( const auto &off : { point_rel_sm::zero(), point_rel_sm::south(), point_rel_sm::east(), point_rel_sm::south_east() } ) {
            to_delete.push_back( base + off );
        }
    }
    // Safety: skip freeing submaps that map::grid[] still references.
    // This prevents use-after-free when submap_loader eviction races with
    // map::shift() / copy_grid() during large map shifts (e.g. pocket entry).
    if( g != nullptr ) {
        const map &here = get_map();
        const auto &grid_vec = here.grid;
        to_delete.remove_if( [&]( const tripoint_abs_sm & p ) {
            const auto it = submaps.find( p );
            if( it == submaps.end() || !it->second ) {
                return false;
            }
            const submap *doomed = it->second.get();
            for( size_t i = 0; i < grid_vec.size(); ++i ) {
                if( grid_vec[i] == doomed ) {
                    debugmsg( "unload_omt: skipping free of submap at %s (ptr %p) "
                              "— map::grid[%zu] still references it (dim='%s')",
                              p.to_string(), static_cast<const void *>( doomed ),
                              i, dimension_id_ );
                    return true;  // remove from to_delete → keep alive
                }
            }
            return false;
        } );
    }
    for( const auto &p : to_delete ) {
        submaps.erase( p );
    }
}

submap *mapbuffer::lookup_submap( const tripoint_abs_sm &p )
{
    // Fast path: submap already resident in memory.
    if( submap *sm = lookup_submap_in_memory( p ) ) {
        return sm;
    }

    // Cache miss — perform disk I/O outside submaps_mutex_ so that concurrent
    // preload_omt() workers on other omts are not stalled behind this call.
    const tripoint_abs_omt omt_addr = project_to<coords::omt>( p );

    std::string pending_data;
    {
        std::lock_guard<std::mutex> pw_lk( pending_writes_mutex_ );
        const auto it = pending_writes_.find( omt_addr );
        if( it != pending_writes_.end() ) {
            pending_data = std::move( it->second );
            pending_writes_.erase( it );
        }
    }

    std::vector<std::pair<tripoint_abs_sm, std::unique_ptr<submap>>> loaded;
    auto already_loaded = [this]( const tripoint_abs_sm & q ) {
        return lookup_submap_in_memory( q ) != nullptr;
    };

    try {
        bool found = false;
        if( !pending_data.empty() ) {
            std::istringstream iss( pending_data );
            JsonIn jsin( iss );
            deserialize_into_vec( jsin, loaded, already_loaded );
            found = true;
        } else {
            found = g->get_active_world()->read_map_omt( dimension_id_, omt_addr,
            [this, &loaded, &already_loaded]( JsonIn & jsin ) {
                deserialize_into_vec( jsin, loaded, already_loaded );
            } );
        }
        if( !found ) {
            return nullptr;
        }
    } catch( const std::exception &err ) {
        debugmsg( "Failed to load submap %s: %s", p.to_string(), err.what() );
        return nullptr;
    }

    for( auto &[pos, sm] : loaded ) {
        if( !add_submap( pos, sm ) ) {
            DebugLog( DL::Warn, DC::Map ) << string_format(
                                              "lookup_submap: submap %d,%d,%d already loaded; keeping in-memory version",
                                              pos.x(), pos.y(), pos.z() );
        }
    }

    submap *result = lookup_submap_in_memory( p );
    if( !result ) {
        debugmsg( "file did not contain the expected submap %d,%d,%d", p.x(), p.y(), p.z() );
    }
    return result;
}

void mapbuffer::save( bool delete_after_save, bool notify_tracker, bool show_progress )
{
    const int num_total_submaps = static_cast<int>( submaps.size() );

    // Spatial eviction only makes sense for the dimension the player is
    // currently in — it has a reality bubble whose origin defines which omts
    // are "near" enough to keep resident.  Non-current dimensions have no
    // bubble, so their submaps are kept in memory (the submap_load_manager
    // handles eviction for those independently).
    const bool is_current_dimension =
        g != nullptr && dimension_id_ == get_map().get_bound_dimension();

    map &here = get_map();
    const tripoint_abs_omt map_origin = is_current_dimension
                                        ? project_to<coords::omt>( here.get_abs_sub() )
                                        : tripoint_abs_omt{};
    const bool map_has_zlevels = g != nullptr && here.has_zlevels();

    // Serial collection of unique OMT addresses with per-omt delete flags.
    // The UI progress popup runs here on the main thread only (show_progress=true).
    // When save() is dispatched from a worker thread (show_progress=false), the popup
    // is skipped to avoid calling UI functions off the main thread.
    struct omt_entry {
        tripoint_abs_omt omt_addr;
        bool     delete_after;
    };
    std::vector<omt_entry> omts_to_process;
    {
        std::set<tripoint_abs_omt> seen_omts;
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

            const auto omt_addr = project_to<coords::omt>( pos );
            if( !seen_omts.insert( omt_addr ).second ) {
                continue;
            }

            bool omt_delete = delete_after_save;
            /* I don't think this is right
            if( is_current_dimension ) {
                // Submaps outside the current map bounds or on wrong z-level
                // are deleted from memory after saving.
                const bool zlev_del = !map_has_zlevels && omt_addr.z() != g->get_levz();
                omt_delete = omt_delete || zlev_del ||
                              omt_addr.x() < map_origin.x() ||
                              omt_addr.y() < map_origin.y() ||
                              omt_addr.x() > map_origin.x() + g_half_mapsize ||
                              omt_addr.y() > map_origin.y() + g_half_mapsize;
            }
            */

            omts_to_process.push_back( { omt_addr, omt_delete } );
        }
    }

    // Write non-uniform omts in parallel. Each write targets a distinct file/key,
    // so there are no shared-state concerns between concurrent save_omt() calls.
    // save_omt() uses submaps.find() for read-only access (safe for concurrent reads).
    // Per-task local_delete lists are merged into the shared list under a mutex.
    std::list<tripoint_abs_sm> submaps_to_delete;
    std::mutex delete_mutex;

    parallel_for( 0, static_cast<int>( omts_to_process.size() ), [&]( int i ) {
        std::list<tripoint_abs_sm> local_delete;
        save_omt( omts_to_process[i].omt_addr, local_delete, omts_to_process[i].delete_after );
        if( !local_delete.empty() ) {
            std::lock_guard<std::mutex> lk( delete_mutex );
            submaps_to_delete.splice( submaps_to_delete.end(), local_delete );
        }
    } );

    // Evict submaps from memory. std::unordered_map mutation is not thread-safe,
    // so this is done serially after the parallel write phase completes.
    for( const auto &pos : submaps_to_delete ) {
        remove_submap( pos );
    }

    // Notify the distribution grid tracker for each evicted submap.
    if( notify_tracker ) {
        auto &tracker = get_distribution_grid_tracker();
        for( const auto &pos : submaps_to_delete ) {
            tracker.on_submap_unloaded( tripoint_abs_sm( pos ), "" );
        }
    }

    // Flush the pending-writes cache to disk.  These are omts that were
    // serialised in memory (by presave_omt or unload_omt) but not yet written.
    // Omts still resident in submaps were already handled by save_omt() above;
    // only evicted omts need to be written here.
    //
    // Snapshot under the lock so disk I/O is not performed while holding it.
    std::map<tripoint_abs_omt, std::string> pending_snapshot;
    {
        std::lock_guard<std::mutex> pw_lk( pending_writes_mutex_ );
        pending_snapshot = std::move( pending_writes_ );
    }
    std::ranges::for_each( pending_snapshot, [&]( auto & entry ) {
        const auto &[omt_addr, data] = entry;
        const auto base = project_to<coords::sm>( omt_addr );
        const bool in_memory =
            submaps.contains( base ) ||
            submaps.contains( base + point_east ) ||
            submaps.contains( base + point_south ) ||
            submaps.contains( base + point_south_east );
        if( !in_memory ) {
            g->get_active_world()->write_map_omt( dimension_id_, omt_addr,
            [&data]( std::ostream & fout ) {
                fout << data;
            } );
        }
    } );
}

void mapbuffer::save_omt( const tripoint_abs_omt &omt_addr,
                          std::list<tripoint_abs_sm> &submaps_to_delete,
                          bool delete_after_save )
{
    ZoneScoped;
    // Build the 4 submap addresses that form this OMT omt.
    std::vector<tripoint_abs_sm> submap_addrs;
    submap_addrs.reserve( 4 );
    for( const point &off : { point_zero, point_south, point_east, point_south_east } ) {
        auto submap_addr = project_to<coords::sm>( omt_addr );
        submap_addr += off;
        submap_addrs.push_back( submap_addr );
    }

    // Use find() throughout (not operator[]) so this function is safe to call
    // from multiple threads concurrently for distinct omt_addr values.
    // operator[] would insert a default entry for missing keys, mutating the map.
    bool all_uniform = true;
    for( const tripoint_abs_sm &submap_addr : submap_addrs ) {
        const auto it = submaps.find( submap_addr );
        if( it != submaps.end() && it->second && !it->second->is_uniform ) {
            all_uniform = false;
            break;
        }
    }

    if( all_uniform ) {
        // Nothing to save — this omt will be regenerated faster than it would be re-read.
        if( delete_after_save ) {
            for( const tripoint_abs_sm &submap_addr : submap_addrs ) {
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

    g->get_active_world()->write_map_omt( dimension_id_, omt_addr, [&]( std::ostream & fout ) {
        JsonOut jsout( fout );
        jsout.start_array();
        for( const tripoint_abs_sm &submap_addr : submap_addrs ) {
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
            jsout.write( submap_addr.x() );
            jsout.write( submap_addr.y() );
            jsout.write( submap_addr.z() );
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

void mapbuffer::deserialize_into_vec(
    JsonIn &jsin,
    std::vector<std::pair<tripoint_abs_sm, std::unique_ptr<submap>>> &out,
    const std::function<bool( const tripoint_abs_sm & )> &skip_if )
{
    jsin.start_array();
    while( !jsin.end_array() ) {
        std::unique_ptr<submap> sm;
        tripoint_abs_sm submap_coordinates;
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
                tripoint_abs_sm loc{ i, j, k };
                jsin.end_array();
                submap_coordinates = loc;
                if( skip_if && skip_if( loc ) ) {
                    skip = true;
                } else {
                    sm = std::make_unique<submap>( submap_coordinates );
                }
            } else if( skip ) {
                jsin.skip_value();
            } else {
                if( !sm ) { //This whole thing is a nasty hack that relys on coordinates coming first...
                    debugmsg( "coordinates was not at the top of submap json" );
                }
                sm->load( jsin, submap_member_name, version, project_to<coords::ms>( submap_coordinates ) );
            }
        }
        if( !skip ) {
            out.emplace_back( submap_coordinates, std::move( sm ) );
        }
    }
}

bool mapbuffer::preload_omt( const tripoint_abs_omt &omt_addr )
{
    ZoneScoped;
    // Disk I/O and JSON parsing — runs outside submaps_mutex_ so
    // different omts can be prefetched concurrently on worker threads.
    std::vector<std::pair<tripoint_abs_sm, std::unique_ptr<submap>>> loaded;
    // Skip submaps already resident in memory during deserialization.
    // This avoids the expensive sm->load() (items, vehicles, terrain construction)
    // for submaps that were already loaded by a prior lazy-border or sync pass.
    auto already_loaded = [this]( const tripoint_abs_sm & p ) {
        return lookup_submap_in_memory( p ) != nullptr;
    };

    // Check the in-memory write-back cache before going to disk.  A omt that
    // was presaved but not yet explicitly saved lives here instead of on disk.
    std::string pending_data;
    bool from_cache = false;
    {
        std::lock_guard<std::mutex> pw_lk( pending_writes_mutex_ );
        const auto it = pending_writes_.find( omt_addr );
        if( it != pending_writes_.end() ) {
            pending_data = std::move( it->second );
            pending_writes_.erase( it );
            from_cache = true;
        }
    }

    if( !pending_data.empty() ) {
        std::istringstream iss( pending_data );
        JsonIn jsin( iss );
        deserialize_into_vec( jsin, loaded, already_loaded );
    } else {
        g->get_active_world()->read_map_omt( dimension_id_, omt_addr,
        [this, &loaded, &already_loaded]( JsonIn & jsin ) {
            deserialize_into_vec( jsin, loaded, already_loaded );
        } );
    }

    // Add parsed submaps to the in-memory buffer under submaps_mutex_.
    // add_submap() handles concurrent duplicate-add gracefully (keeps in-memory version).
    for( auto &[pos, sm] : loaded ) {
        if( !add_submap( pos, sm ) ) {
            DebugLog( DL::Warn, DC::Map ) << string_format(
                                              "preload_omt: submap %d,%d,%d already loaded; keeping in-memory version",
                                              pos.x(), pos.y(), pos.z() );
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
    return from_cache;
}

bool mapbuffer::generate_omt( const tripoint_abs_omt &omt_addr )
{
    ZoneScoped;
    const auto base = project_to<coords::sm>( omt_addr );
    const bool all_loaded =
        lookup_submap_in_memory( base )
        && lookup_submap_in_memory( base + point_east )
        && lookup_submap_in_memory( base + point_south )
        && lookup_submap_in_memory( base + point_south_east );
    if( all_loaded ) {
        return false;
    }
    tinymap tmp_map;
    tmp_map.bind_dimension( dimension_id_ );
    tmp_map.generate( base, calendar::turn );
    return true;
}

void mapbuffer::presave_omt( const tripoint_abs_omt &omt_addr )
{
    ZoneScoped;
    const auto base = project_to<coords::sm>( omt_addr );
    const std::array<tripoint_abs_sm, 4> addrs = { {
            base,
            base + point_rel_sm::east(),
            base + point_rel_sm::south(),
            base + point_rel_sm::south_east()
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

    // Uniform omts regenerate faster than a disk round-trip.
    if( all_uniform || disable_mapgen ) {
        return;
    }

    // Serialise into the pending-writes cache outside the lock.  The submap
    // objects stay alive until after this future resolves (submap_load_manager
    // withholds eviction until the presave completes).  No disk I/O occurs here;
    // the cache is flushed to disk only on an explicit save.
    std::ostringstream buf;
    {
        JsonOut jsout( buf );
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
            jsout.write( addrs[i].x() );
            jsout.write( addrs[i].y() );
            jsout.write( addrs[i].z() );
            jsout.end_array();
            sm->store( jsout );
            jsout.end_object();
        }
        jsout.end_array();
    }
    std::lock_guard<std::mutex> pw_lk( pending_writes_mutex_ );
    pending_writes_[omt_addr] = std::move( buf ).str();
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
