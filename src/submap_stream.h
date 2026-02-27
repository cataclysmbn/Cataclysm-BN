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
         *     Generation is serialised under a static generation mutex because
         *     tinymap::generate() currently writes into the global MAPBUFFER,
         *     which is not thread-safe for concurrent writes.  Phase 6 will
         *     relax this by making loadn() dimension-aware.
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
         * For each completed load, calls m.on_submap_loaded(pos, dim) to insert
         * the submap into the map grid and invalidate the relevant caches.
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

    private:
        struct pending_load {
            std::string      dim;
            tripoint_abs_sm  pos;
            std::future<submap *> future;
        };

        std::vector<pending_load> pending_;
        mutable std::mutex mutex_;
};

/** Process-lifetime streaming instance used by map::shift() and game::update_map(). */
extern submap_stream submap_streamer;
