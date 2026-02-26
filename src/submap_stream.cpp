#include "submap_stream.h"

#include <chrono>
#include <mutex>
#include <string>
#include <vector>

#include "calendar.h"
#include "coordinate_conversions.h"
#include "coordinates.h"
#include "debug.h"
#include "map.h"
#include "mapbuffer.h"
#include "mapbuffer_registry.h"
#include "submap.h"
#include "thread_pool.h"

submap_stream submap_streamer;

void submap_stream::request_load( const std::string &dim, tripoint_abs_sm pos )
{
    {
        std::lock_guard<std::mutex> lk( mutex_ );
        // Deduplication: skip if an in-flight request already covers this position.
        for( const auto &p : pending_ ) {
            if( p.dim == dim && p.pos == pos ) {
                return;
            }
        }
    }

    // The generation mutex serialises writes to the mapbuffer from worker threads.
    // Concurrent reads (lookup_submap_in_memory) are safe without the mutex.
    // The disk-read and mapgen paths both call add_submap() — a write — so they
    // must be serialised.  Phase 6 will add per-mapbuffer locking so that concurrent
    // disk reads for distinct positions can overlap.
    static std::mutex gen_mutex;

    auto fut = get_thread_pool().submit_returning( [dim, pos, &gen_mutex]() -> submap * {
        mapbuffer &mb = MAPBUFFER_REGISTRY.get( dim );

        // Step 1: early-out — submap already present (concurrent read, no lock needed).
        if( submap *existing = mb.lookup_submap_in_memory( pos.raw() ) ) {
            return existing;
        }

        // Steps 2 and 3 write to the mapbuffer and must be serialised.
        std::lock_guard<std::mutex> lk( gen_mutex );

        // Re-check after acquiring lock: another worker may have loaded it already.
        if( submap *existing = mb.lookup_submap_in_memory( pos.raw() ) ) {
            return existing;
        }

        // Step 2: try to load from disk.  Returns nullptr if no saved file exists.
        submap *sm = mb.load_submap( pos );
        if( sm ) {
            return sm;
        }

        // Step 3: no disk file — fall back to synchronous tinymap generation.
        // tinymap::generate() writes into MAPBUFFER via loadn(), which is why we hold
        // gen_mutex.  Phase 6 will use a per-tinymap private buffer (drain_to_mapbuffer)
        // to allow parallel generation; until then the generation step is serialised.
        //
        // Note: generation rounds down to the 2×2 OMT-aligned submap quad origin,
        // matching the same rounding that map::loadn() applies.
        const tripoint_abs_omt abs_omt( sm_to_omt_copy( pos.raw() ) );
        const tripoint abs_rounded = omt_to_sm_copy( abs_omt.raw() );
        tinymap tm;
        tm.generate( abs_rounded, calendar::turn );
        tm.drain_to_mapbuffer( mb );  // no-op for primary dim; Phase 6 extends this

        return mb.lookup_submap_in_memory( pos.raw() );
    } );

    std::lock_guard<std::mutex> lk( mutex_ );
    pending_.push_back( { dim, pos, std::move( fut ) } );
}

void submap_stream::drain_completed( map &m,
                                     const std::vector<tripoint_abs_sm> &must_have )
{
    std::lock_guard<std::mutex> lk( mutex_ );

    // First pass: block until each must_have submap's future is ready.
    // These are the immediately-visible edge submaps that shift() requires.
    for( const tripoint_abs_sm &need : must_have ) {
        for( auto &p : pending_ ) {
            if( p.dim == m.get_bound_dimension() && p.pos == need ) {
                p.future.wait();
                break;
            }
        }
    }

    // Second pass: non-blocking drain of all futures that are ready right now.
    pending_.erase(
        std::remove_if( pending_.begin(), pending_.end(), [&]( pending_load &p ) {
            if( p.future.wait_for( std::chrono::seconds( 0 ) ) != std::future_status::ready ) {
                return false;
            }
            submap *sm = p.future.get();
            if( sm ) {
                // Insert into the live map grid and invalidate caches.
                // Signature: on_submap_loaded(pos, dim_id)
                m.on_submap_loaded( p.pos, p.dim );
            }
            return true;
        } ),
        pending_.end() );
}

bool submap_stream::is_pending( const std::string &dim, tripoint_abs_sm pos ) const
{
    std::lock_guard<std::mutex> lk( mutex_ );
    for( const auto &p : pending_ ) {
        if( p.dim == dim && p.pos == pos ) {
            return true;
        }
    }
    return false;
}
