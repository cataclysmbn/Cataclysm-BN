#pragma once

#include <condition_variable>
#include <future>
#include <mutex>
#include <string>
#include <unordered_set>
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
         *     g_active_dimension_id.
         *
         *     Per-OMT synchronisation: gen_in_progress_ (guarded by
         *     gen_in_progress_mutex_ + gen_in_progress_cv_) serialises only
         *     workers that collide on the same OMT-aligned position.  Workers
         *     for different OMTs run fully concurrently.  A colliding worker
         *     waits on gen_in_progress_cv_ until the owner signals completion,
         *     then re-checks the mapbuffer and returns the result.
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
         * any main-thread mutation of mapbuffer::submaps.
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
        std::unordered_set<tripoint_abs_sm> pending_positions_;
        mutable std::mutex mutex_;

        // Per-OMT generation synchronisation.
        //
        // gen_in_progress_ holds the OMT-aligned positions currently being
        // generated by a worker.  A second worker that reaches step 3 for the
        // same OMT waits on gen_in_progress_cv_ until the owner erases the
        // entry and signals, then re-checks the mapbuffer.
        //
        // Workers for different OMT positions never block each other, enabling
        // full parallel generation across the thread pool.
        //
        // Locking order: mutex_ must NOT be held when acquiring
        // gen_in_progress_mutex_, and vice versa.  The worker lambda never
        // acquires mutex_; the public methods never acquire gen_in_progress_mutex_.
        std::mutex                           gen_in_progress_mutex_;
        std::condition_variable              gen_in_progress_cv_;
        std::unordered_set<tripoint_abs_omt> gen_in_progress_;
};

/** Process-lifetime streaming instance used by map::shift() and game::update_map(). */
extern submap_stream submap_streamer;
