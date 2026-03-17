#include "submap_stream.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "calendar.h"
#include "coordinate_conversions.h"
#include "coordinates.h"
#include "debug.h"
#include "map.h"
#include "mapbuffer.h"
#include "mapbuffer_registry.h"
#include "profile.h"
#include "submap.h"
#include "thread_pool.h"

submap_stream submap_streamer;

void submap_stream::request_load( const std::string &dim, tripoint_abs_sm pos )
{
    ZoneScoped;
    {
        std::lock_guard<std::mutex> lk( mutex_ );
        // Capacity cap: drop the new request rather than growing the queue
        // without bound.  The main thread falls back to synchronous loadn() if
        // the submap is actually needed before a worker delivers it.
        if( pending_.size() >= MAX_PENDING_LOADS ) {
            return;
        }
        // Deduplication: skip if an in-flight request already covers this position.
        if( pending_positions_.contains( pos ) ) {
            return;
        }
    }

    // Workers load from disk concurrently (mapbuffer::load_submap is internally
    // synchronised).  When a disk file is absent, per-OMT synchronisation via
    // gen_in_progress_ ensures only one worker generates each OMT at a time;
    // workers for *different* OMTs run fully in parallel.  NPC operations
    // (insert_npc / remove_npc) are protected by overmapbuffer::npc_mutex_.
    auto fut = get_thread_pool().submit_returning( [this, dim, pos]() -> submap * {
        ZoneScopedN( "async_submap_load" );
        ZoneText( dim.c_str(), dim.size() );
        mapbuffer &mb = MAPBUFFER_REGISTRY.get( dim );

        // Early-out — submap already present (concurrent read, no lock needed).
        if( submap *existing = mb.lookup_submap_in_memory( pos.raw() ) )
        {
            return existing;
        }

        // Try to load from disk.  Concurrent disk reads across workers
        // are safe; mapbuffer::load_submap() is internally synchronised.
        if( submap *sm = mb.load_submap( pos ) )
        {
            return sm;
        }

        // No disk file — fall back to tinymap generation.
        // Per-OMT synchronisation via gen_in_progress_ ensures only one worker
        // generates each OMT at a time; others wait and re-check the mapbuffer.
        // Generation rounds down to the 2x2 OMT-aligned submap quad origin.
        const tripoint_abs_omt abs_omt( sm_to_omt_copy( pos.raw() ) );
        const tripoint abs_rounded = omt_to_sm_copy( abs_omt.raw() );

        {
            std::unique_lock<std::mutex> lk( gen_in_progress_mutex_ );
            // Wait until no other worker owns the claim for this OMT.
            gen_in_progress_cv_.wait( lk, [&] {
                // If the submap appeared in the mapbuffer while we waited
                // (the owner finished), we can skip generation entirely.
                if( mb.lookup_submap_in_memory( pos.raw() ) )
                {
                    return true;
                }
                return !gen_in_progress_.contains( abs_omt );
            } );
            // Re-check after the wait: the owning worker may have completed.
            if( submap *existing = mb.lookup_submap_in_memory( pos.raw() ) )
            {
                return existing;
            }
            // Claim this OMT; lk is released at scope exit.
            gen_in_progress_.insert( abs_omt );
        }

        // bind_dimension() ensures generate() writes into the correct registry
        // slot (MAPBUFFER_REGISTRY.get(dim)).  drain_to_mapbuffer() is a no-op.
        tinymap tm;
        tm.bind_dimension( dim );
        tm.generate( abs_rounded, calendar::turn );
        tm.drain_to_mapbuffer( mb );  // no-op: submaps already in correct registry slot

        {
            std::lock_guard<std::mutex> lk( gen_in_progress_mutex_ );
            gen_in_progress_.erase( abs_omt );
        }
        gen_in_progress_cv_.notify_all();

        return mb.lookup_submap_in_memory( pos.raw() );
    } );

    std::lock_guard<std::mutex> lk( mutex_ );
    pending_positions_.insert( pos );
    pending_.push_back( { dim, pos, std::move( fut ) } );
}

void submap_stream::drain_completed( map &m,
                                     const std::vector<tripoint_abs_sm> &must_have )
{
    ZoneScoped;
    TracyPlot( "Stream Queue Depth", static_cast<int64_t>( pending_.size() ) );
    std::lock_guard<std::mutex> lk( mutex_ );

    // First pass: block until each must_have submap's future is ready.
    // These are the immediately-visible edge submaps that shift() requires.
    // Build a position→index map for O(1) lookup instead of O(n²) scanning.
    std::unordered_map<tripoint_abs_sm, std::size_t> pos_to_idx;
    pos_to_idx.reserve( pending_.size() );
    for( std::size_t i = 0; i < pending_.size(); ++i ) {
        pos_to_idx.emplace( pending_[i].pos, i );
    }
    for( const tripoint_abs_sm &need : must_have ) {
        auto it = pos_to_idx.find( need );
        if( it != pos_to_idx.end() ) {
            pending_[it->second].future.wait();
        }
    }

    // Second pass: non-blocking drain of all futures that are ready right now.
    // We do NOT call m.on_submap_loaded() here — the submap is already in
    // MAPBUFFER and loadn() will find it on the fast in-memory path.
    // Calling on_submap_loaded() before shift()'s grid-copy loop would alias
    // adjacent grid slots, producing a duplication bug.
    pending_.erase(
    std::remove_if( pending_.begin(), pending_.end(), [this]( pending_load & p ) {
        if( p.future.wait_for( std::chrono::seconds( 0 ) ) != std::future_status::ready ) {
            return false;
        }
        p.future.get();  // consume the future; submap is already in MAPBUFFER
        pending_positions_.erase( p.pos );
        return true;
    } ),
    pending_.end() );
}

auto submap_stream::flush_all() -> void
{
    ZoneScopedN( "submap_stream_flush" );
    std::lock_guard<std::mutex> lk( mutex_ );
    TracyPlot( "Pending Stream Tasks", static_cast<int64_t>( pending_.size() ) );
    std::ranges::for_each( pending_, []( auto & p ) {
        p.future.wait();
        p.future.get(); // consume result; submap data already in mapbuffer
    } );
    pending_.clear();
    pending_positions_.clear();
}

bool submap_stream::is_pending( const std::string &dim, tripoint_abs_sm pos ) const
{
    std::lock_guard<std::mutex> lk( mutex_ );
    return pending_positions_.contains( pos );
}
