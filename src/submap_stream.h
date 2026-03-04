#pragma once

#include <future>
#include <mutex>
#include <string>
#include <vector>

#include "coordinates.h"

class map;
class submap;

/**
 * Asynchronous submap loader.
 *
 * Submits disk-load (and mapgen fallback) tasks to the thread pool so that
 * map::shift() can overlap disk I/O and map generation with game-loop work on
 * the main thread.
 *
 * Thread-safety contract
 * ----------------------
 * All public methods may be called from the main thread only.
 * The internal worker lambda runs on a pool thread; it accesses MAPBUFFER under
 * a per-instance mutex and must not call any main-thread-only APIs (SDL, Lua).
 *
 * Fallback
 * --------
 * When the thread pool has no workers (single-core machine), request_load()
 * executes the load task synchronously via submit_returning()'s zero-worker
 * fallback, so the interface remains correct without a thread pool.
 */
/**
 * Maximum number of in-flight background load requests.
 *
 * When request_load() is called and the queue is already at this depth, the
 * new request is silently dropped.  The main thread will load synchronously
 * via loadn() if the submap is actually needed before a worker delivers it.
 * This bounds memory usage and flush_all() stall time during rapid movement.
 */
inline constexpr std::size_t MAX_PENDING_LOADS = 128;

class submap_stream
{
    public:
        submap_stream() = default;

        // Non-copyable / non-movable (contains a mutex and futures).
        submap_stream( const submap_stream & ) = delete;
        submap_stream &operator=( const submap_stream & ) = delete;

        /**
         * Asynchronously load the submap at @p pos in dimension @p dim.
         *
         * If an in-flight request for the same (dim, pos) already exists, this
         * call is a no-op (deduplication).  Returns immediately.
         *
         * The worker performs in order:
         *  1. Check if the submap is already in the mapbuffer (early-out).
         *  2. Try to load from disk (mb.load_submap).
         *  3. If the file does not exist, fall back to tinymap generation.
         *     Generation calls tinymap::bind_dimension(dim) so all mapgen reads
         *     use get_overmapbuffer(dim) rather than the active-dimension global
         *     g_active_dimension_id.  gen_mutex_ serialises the dedup re-check
         *     and NPC write operations (insert_npc / remove_npc) within this
         *     submap_stream instance; concurrent workers across different
         *     submap_stream instances (future per-dimension streams) are unblocked.
         *     Full removal of gen_mutex_ requires auditing overmapbuffer read
         *     thread-safety; see F2-3 in Map Overhaul Plan.
         */
        void request_load( const std::string &dim, tripoint_abs_sm pos );

        /**
         * Drain completed load futures into the live map @p m.
         *
         * First pass: block until every submap listed in @p must_have is ready.
         * This is used for the immediately-visible edge row/column after shift().
         *
         * Second pass: non-blocking drain of any futures that are already ready.
         *
         * NOTE: This function does NOT call m.on_submap_loaded(); that is the
         * responsibility of submap_load_manager (which calls it separately after
         * drain_completed returns).  Calling on_submap_loaded here would trigger
         * the grid-duplication bug where the grid-copy loop in shift() propagates
         * the newly-set pointer so two adjacent slots point to the same submap.
         * See F2-7 in Map Overhaul Plan.
         */
        void drain_completed( map &m, const std::vector<tripoint_abs_sm> &must_have );

        /**
         * Block until all pending background load tasks finish, then clear the
         * pending list.
         *
         * MUST be called before any main-thread code that mutates mapbuffer::submaps
         * (e.g. unload_quad, save).  Background workers access submaps via
         * lookup_submap_in_memory / add_submap without a mapbuffer-level lock; the
         * only safety guarantee is that the main thread does not mutate the map
         * concurrently.  Flushing before mutation preserves that contract.
         */
        auto flush_all() -> void;

        /** Returns true if an in-flight request exists for (dim, pos). */
        bool is_pending( const std::string &dim, tripoint_abs_sm pos ) const;

        /**
         * Returns true if there are any in-flight background load tasks.
         * Used by debug assertions to verify flush_all() was called before
         * any main-thread mutation of mapbuffer::submaps.  See F2-1.
         */
        bool has_pending() const {
            return !pending_.empty();
        }

    private:
        struct pending_load {
            std::string      dim;
            tripoint_abs_sm  pos;
            std::future<submap *> future;
        };

        std::vector<pending_load> pending_;
        mutable std::mutex mutex_;

        // Serialises the dedup re-check (step 3 double-check in request_load)
        // and NPC write operations (insert_npc / remove_npc in generate()) across
        // all worker threads of this instance.  Per-member rather than static so
        // separate submap_stream instances (e.g. future per-dimension streams)
        // do not block each other.  Removal of this mutex requires confirming
        // that overmapbuffer reads are concurrency-safe; see F2-3.
        std::mutex gen_mutex_;
};

/** Process-lifetime streaming instance used by map::shift() and game::update_map(). */
extern submap_stream submap_streamer;
