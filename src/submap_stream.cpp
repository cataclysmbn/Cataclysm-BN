#include "submap_stream.h"

#include <algorithm>
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

    // The generation mutex serialises writes to the mapbuffer from worker threads
    // so that two workers never race on the same position's add_submap() call.
    // mapbuffer::add_submap() and lookup_submap_in_memory() are also individually
    // protected by mapbuffer::submaps_mutex_ (a recursive_mutex), which guards
    // concurrent access to the underlying std::map between worker threads and the
    // main thread's lookup_submap() calls in loadn().
    static std::mutex gen_mutex;

    auto fut = get_thread_pool().submit_returning( [dim, pos]() -> submap * {
        mapbuffer &mb = MAPBUFFER_REGISTRY.get( dim );

        // Step 1: early-out — submap already present (concurrent read, no lock needed).
        if( submap *existing = mb.lookup_submap_in_memory( pos.raw() ) )
        {
            return existing;
        }

        // Steps 2 and 3 write to the mapbuffer and must be serialised.
        std::lock_guard<std::mutex> lk( gen_mutex );

        // Re-check after acquiring lock: another worker may have loaded it already.
        if( submap *existing = mb.lookup_submap_in_memory( pos.raw() ) )
        {
            return existing;
        }

        // Step 2: try to load from disk.  Returns nullptr if no saved file exists.
        submap *sm = mb.load_submap( pos );
        if( sm )
        {
            return sm;
        }

        // Step 3: no disk file — fall back to synchronous tinymap generation.
        // bind_dimension() ensures loadn() inside generate() writes into the correct
        // registry slot (MAPBUFFER_REGISTRY.get(dim)) for all dimensions.
        // drain_to_mapbuffer() is therefore a true no-op.
        //
        // Note: generation rounds down to the 2×2 OMT-aligned submap quad origin,
        // matching the same rounding that map::loadn() applies.
        const tripoint_abs_omt abs_omt( sm_to_omt_copy( pos.raw() ) );
        const tripoint abs_rounded = omt_to_sm_copy( abs_omt.raw() );
        tinymap tm;
        tm.bind_dimension( dim );
        tm.generate( abs_rounded, calendar::turn );
        tm.drain_to_mapbuffer( mb );  // no-op: submaps already in correct registry slot

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
    // NOTE: we intentionally do NOT call m.on_submap_loaded() here.
    //
    // The streamer only ever pre-loads positions for the NEXT shift's new edge
    // (one step ahead of the current view).  drain_completed() is called at the
    // START of shift(), before the grid-copy loop that moves existing submaps
    // and before loadn() fills the new-edge slots.  Calling on_submap_loaded()
    // at this point writes the new-edge submap into grid[0] (or grid[MAPSIZE-1])
    // using the already-updated abs_sub.  The copy loop then reads grid[0] and
    // propagates it to grid[1], making two adjacent slots share the same submap
    // pointer — producing the visible "left and right halves of the OMT are
    // identical" duplication bug on every movement step.
    //
    // The submap is already in MAPBUFFER after the worker completes.  loadn()
    // finds it there on the fast in-memory path and calls setsubmap() correctly.
    // No grid update is needed here.
    pending_.erase(
    std::remove_if( pending_.begin(), pending_.end(), [&]( pending_load & p ) {
        if( p.future.wait_for( std::chrono::seconds( 0 ) ) != std::future_status::ready ) {
            return false;
        }
        p.future.get();  // consume the future; submap is already in MAPBUFFER
        return true;
    } ),
    pending_.end() );
}

auto submap_stream::flush_all() -> void
{
    std::lock_guard<std::mutex> lk( mutex_ );
    std::ranges::for_each( pending_, []( auto & p ) {
        p.future.wait();
        p.future.get(); // consume result; submap data already in mapbuffer
    } );
    pending_.clear();
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
