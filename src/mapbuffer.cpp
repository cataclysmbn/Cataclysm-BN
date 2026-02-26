#include "mapbuffer.h"

#include <algorithm>
#include <chrono>
#include <exception>
#include <functional>
#include <memory>
#include <mutex>
#include <set>
#include <sstream>
#include <utility>
#include <vector>

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
    // lookup_submap already handles the disk-read path transparently.
    return lookup_submap( pos.raw() );
}

void mapbuffer::unload_submap( const tripoint_abs_sm &pos )
{
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

void mapbuffer::unload_quad( const tripoint &om_addr )
{
    // Save the quad once and collect all in-memory submaps for deletion.
    // Using delete_after_save=true ensures save_quad() enumerates what to delete
    // so we don't need to recompute the 4 addresses separately.
    std::list<tripoint> to_delete;
    save_quad( om_addr, to_delete, /*delete_after_save=*/true );
    for( const tripoint &p : to_delete ) {
        submaps.erase( p );
    }
}

submap *mapbuffer::lookup_submap( const tripoint &p )
{
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

    map &here = get_map();
    const tripoint map_origin = sm_to_omt_copy( here.get_abs_sub() );
    const bool map_has_zlevels = g != nullptr && here.has_zlevels();

    // Phase 1 — Serial collection of unique OMT quad addresses with per-quad delete flags.
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

            // Submaps outside the current map bounds or on wrong z-level
            // are deleted from memory after saving.
            const bool zlev_del = !map_has_zlevels && om_addr.z != g->get_levz();
            const bool quad_delete = delete_after_save || zlev_del ||
                                     om_addr.x < map_origin.x ||
                                     om_addr.y < map_origin.y ||
                                     om_addr.x > map_origin.x + HALF_MAPSIZE ||
                                     om_addr.y > map_origin.y + HALF_MAPSIZE;

            quads_to_process.push_back( { om_addr, quad_delete } );
        }
    }

    // Phase 2 — Write non-uniform quads in parallel. Each write targets a distinct file/key,
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

    // Phase 3 — Evict submaps from memory. std::map mutation is not thread-safe,
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

    g->get_active_world()->write_map_quad( om_addr, [&]( std::ostream & fout ) {
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
    if( !g->get_active_world()->read_map_quad( om_addr, std::bind( &mapbuffer::deserialize,
            this, _1 ) ) ) {
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

void mapbuffer::deserialize( JsonIn &jsin )
{
    jsin.start_array();
    while( !jsin.end_array() ) {
        std::unique_ptr<submap> sm;
        tripoint submap_coordinates;
        jsin.start_object();
        int version = 0;
        while( !jsin.end_object() ) {
            std::string submap_member_name = jsin.get_member_name();
            if( submap_member_name == "version" ) {
                version = jsin.get_int();
            } else if( submap_member_name == "coordinates" ) {
                jsin.start_array();
                int i = jsin.get_int();
                int j = jsin.get_int();
                int k = jsin.get_int();
                tripoint loc{ i, j, k };
                jsin.end_array();
                submap_coordinates = loc;
                sm = std::make_unique<submap>( sm_to_ms_copy( submap_coordinates ) );
            } else {
                if( !sm ) { //This whole thing is a nasty hack that relys on coordinates coming first...
                    debugmsg( "coordinates was not at the top of submap json" );
                }
                sm->load( jsin, submap_member_name, version, multiply_xy( submap_coordinates, 12 ) );
            }
        }

        if( !add_submap( submap_coordinates, sm ) ) {
            // In-memory version takes precedence; the disk entry is stale.
            // This can happen legitimately when a quad is partially reloaded after
            // unload_submap() broke quad consistency (pre-unload_quad fix).
            // With quad-level eviction (unload_quad) this should not occur in normal play.
            DebugLog( DL::Warn, DC::Map ) << string_format(
                "submap %d,%d,%d was already loaded; keeping in-memory version",
                submap_coordinates.x, submap_coordinates.y, submap_coordinates.z );
        }
    }
}
