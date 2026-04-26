#pragma once

#include <functional>
#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "coordinates.h"
#include "point.h"

class submap;
class JsonIn;

/**
 * Store, buffer, save and load the entire world map.
 */
class mapbuffer
{
    public:
        mapbuffer();
        ~mapbuffer();

        /** Store all submaps in this instance into savefiles.
         * @param delete_after_save If true, the saved submaps are removed
         * from the mapbuffer (and deleted).
         * @param notify_tracker If true, fire on_submap_unloaded() on the
         * distribution_grid_tracker for each submap evicted during save.
         * Pass false when saving a non-primary dimension's mapbuffer so that
         * the primary tracker is not spuriously updated.
         * @param show_progress If true (default), show a UI progress popup
         * during collection. Pass false when save() is called from a
         * worker thread (e.g. via mapbuffer_registry::save_all parallel path)
         * because UI functions must only be called on the main thread.
         **/
        void save( bool delete_after_save = false, bool notify_tracker = true,
                   bool show_progress = true );

        /** Delete all buffered submaps. **/
        void clear();

        /** Add a new submap to the buffer.
         *
         * @param x, y, z The absolute world position in submap coordinates.
         * Same as the ones in @ref lookup_submap.
         * @param sm The submap. If the submap has been added, the unique_ptr
         * is released (set to NULL).
         * @return true if the submap has been stored here. False if there
         * is already a submap with the specified coordinates. The submap
         * is not stored and the given unique_ptr retains ownsership.
         */
        bool add_submap( const tripoint &p, std::unique_ptr<submap> &sm );
        // Old overload that we should stop using, but it's complicated
        bool add_submap( const tripoint &p, submap *sm );

        /** Get a submap stored in this buffer.
         *
         * @param x, y, z The absolute world position in submap coordinates.
         * Same as the ones in @ref add_submap.
         * @return NULL if the submap is not in the mapbuffer
         * and could not be loaded. The mapbuffer takes care of the returned
         * submap object, don't delete it on your own.
         */
        submap *lookup_submap( const tripoint &p );
        submap *lookup_submap( const tripoint_abs_sm &p ) {
            return lookup_submap( p.raw() );
        }

        /** Get a submap only if it's already loaded in memory.
         * Unlike lookup_submap(), this does NOT query the database for missing submaps.
         * Use this for out-of-bounds positions where we know there's no DB entry,
         * to avoid ~2400 wasted SQLite queries per pocket dimension map load.
         *
         * Thread-safe: may be called from background worker threads (under gen_mutex).
         */
        submap *lookup_submap_in_memory( const tripoint &p ) {
            std::lock_guard<std::recursive_mutex> lk( submaps_mutex_ );
            const auto iter = submaps.find( p );
            return iter != submaps.end() ? iter->second.get() : nullptr;
        }

        /**
         * Load a submap from disk (if not already in memory) and return it.
         * This is the public disk-read counterpart to the internal lookup path,
         * intended for use by submap_load_manager and related systems.
         * Returns nullptr if the submap does not exist on disk.
         */
        submap *load_submap( const tripoint_abs_sm &pos );

        /**
         * Parallel-safe quad prefetch: reads all submaps in the OMT quad at
         * @p om_addr from disk and adds them to the in-memory buffer.
         *
         * May be called concurrently from worker threads for different quad
         * addresses.  The disk I/O phase runs outside @c submaps_mutex_; the
         * add phase acquires the mutex briefly per submap.
         *
         * If the quad file does not exist (submaps need generation), this is a
         * no-op; the caller must fall back to the synchronous generation path in
         * map::loadn().
         *
         * Thread-safety note: the dim-aware @c world::read_map_quad overload is
         * used, so no global (g_active_dimension_id) is read at worker-thread
         * execution time.  For SQLite-backed saves, the connection must be opened
         * in SQLITE_THREADSAFE ≥ 1 (serialised or multi-thread) mode — the
         * default for all supported SQLite builds.
         */
        /**
         * Returns true if data was loaded from the in-memory write-back cache
         * (pending_writes_) rather than from disk.  A cache-loaded quad has not yet
         * been flushed to actual disk files and must be re-saved before eviction.
         */
        bool preload_quad( const tripoint &om_addr );

        /**
         * Generate all submaps in the OMT quad at @p om_addr if any are not yet
         * resident in memory.  Called internally by load_or_generate_quad().
         *
         * Thread-safe: uses per-thread RNG, npc_mutex_ for NPC writes, and
         * submaps_mutex_ for add_submap().  If two workers race on the same quad
         * the duplicate submaps are deferred to drain_pending_submap_destroy().
         */
        /**
         * Returns true if mapgen actually ran (quad was not fully resident),
         * false if all submaps were already in memory and nothing was generated.
         */
        bool generate_quad( const tripoint &om_addr );

        /**
         * Try to load submaps from disk (@ref preload_quad), then generate any
         * still missing via @ref generate_quad.  Returns true if mapgen ran,
         * false if the quad was already fully resident after the disk load.
         *
         * Safe to call concurrently from worker threads for distinct quad addresses.
         * Identical thread-safety contract as preload_quad().
         */
        bool load_or_generate_quad( const tripoint &om_addr );

        /**
         * Serialise the OMT quad at @p om_addr into the in-memory write-back cache
         * (@c pending_writes_) without evicting submaps or touching disk.  Intended
         * to be called from a background worker thread while the quad is in the border
         * zone (not simulated), so that the subsequent eviction only needs to free the
         * in-memory objects without an I/O stall.  The cached data is flushed to disk
         * only on an explicit save; discarding it (via @c clear()) lets the player
         * revert to the pre-session state.
         *
         * Thread-safety contract:
         * - Briefly acquires @c submaps_mutex_ to collect raw submap pointers.
         * - Releases the lock before serialization, so concurrent @c preload_quad()
         *   or @c add_submap() calls on other quads are not blocked.
         * - The caller (submap_load_manager) guarantees the submaps remain alive
         *   in memory for the duration of this call by withholding eviction until
         *   the returned future is resolved.
         * - Writes to @c pending_writes_ under @c pending_writes_mutex_ (brief hold).
         */
        void presave_quad( const tripoint &om_addr );

        /**
         * Destroy submaps that were discarded by preload_quad() because the in-memory
         * version already existed.  Must be called on the main thread after all
         * preload_quad() futures have been joined.
         *
         * safe_reference<T>, cache_reference<T>, and cata_arena<T> all rely on
         * unsynchronised global statics; destructing submaps (and their items) on
         * worker threads would race on those statics.  preload_quad() defers such
         * destruction here instead of letting it happen on the worker.
         */
        auto drain_pending_submap_destroy() -> void;

        /**
         * Generate all quads that were deferred from worker threads because their
         * overmap terrain has a Lua-based mapgen function (Lua is not reentrant).
         *
         * Runs generate_quad() for each deferred entry synchronously on the calling
         * thread.  Must be called on the main thread only.
         *
         * Returns the om_addr of every quad that was actually generated (i.e. was
         * not already resident in memory).  Callers should mark these dirty.
         */
        auto drain_deferred_lua_quads() -> std::vector<tripoint>;

        /**
         * Conditionally save and then remove the submap at @p pos from the buffer.
         * The containing OMT quad is saved to disk first (unless it is fully uniform),
         * then the submap is erased from memory.  Does nothing if @p pos is not loaded.
         */
        void unload_submap( const tripoint_abs_sm &pos );

        /**
         * Evict all submaps in the OMT quad at @p om_addr.
         *
         * If @p save is true (default), the quad is serialised into the in-memory
         * write-back cache (@c pending_writes_) before the submap objects are freed.
         * The cache is flushed to disk only on an explicit save.
         * Pass @p save = false only for border-preloaded quads that were never
         * simulated — their in-memory content is identical to what is already on
         * disk, so no write is needed.
         *
         * This is the correct way to evict a quad: calling unload_submap() per-submap
         * repeatedly overwrites the quad file without the previously-removed siblings,
         * causing data loss and "file did not contain expected submap" errors on reload.
         * Does nothing for quads that are fully uniform (they regenerate on demand).
         */
        void unload_quad( const tripoint &om_addr, bool save = true );

        /**
         * Move all submaps from this buffer into @p dest, leaving this buffer empty.
         * Used by the dimension-transition system to migrate submaps between registry slots
         * without a disk round-trip.
         */
        void transfer_all_to( mapbuffer &dest );

    private:
        using submap_map_t = std::unordered_map<tripoint, std::unique_ptr<submap>>;

        /// Guards all accesses to `submaps` that may overlap with background
        /// worker threads calling add_submap().  std::recursive_mutex allows
        /// mapgen code (running under a held lock) to call lookup_submap_in_memory()
        /// or add_submap() without deadlocking.
        mutable std::recursive_mutex submaps_mutex_;

        /// Submaps that preload_quad() could not add (duplicate already in memory).
        /// Their destruction is deferred here and drained on the main thread via
        /// drain_pending_submap_destroy() to avoid racing on the global statics
        /// inside safe_reference<T>, cache_reference<T>, and cata_arena<T>.
        mutable std::mutex pending_destroy_mutex_;
        std::vector<std::unique_ptr<submap>> pending_destroy_submaps_;

        /// Serialised quads awaiting disk flush.  Written by presave_quad() (worker
        /// threads) and the save=true branch of unload_quad() (main thread); read back
        /// by preload_quad() (worker threads) before falling through to disk.  Flushed
        /// to disk by save() and discarded by clear(), leaving disk files untouched so
        /// the player can revert to the pre-session state by quitting without saving.
        mutable std::mutex pending_writes_mutex_;
        std::map<tripoint, std::string> pending_writes_;

        /// Quads whose overmap terrain has a Lua-based mapgen function.
        /// generate_quad() pushes here instead of generating on the worker (Lua is not
        /// reentrant).  Drained by drain_deferred_lua_quads() on the main thread.
        mutable std::mutex deferred_lua_quads_mutex_;
        std::vector<tripoint> deferred_lua_quads_;

    public:
        submap_map_t::iterator begin() {
            return submaps.begin();
        }
        submap_map_t::iterator end() {
            return submaps.end();
        }

        /**
         * Iterate all submaps under @c submaps_mutex_, allowing background
         * preload_quad() workers to run concurrently without UB.
         *
         * Use this instead of begin()/end() whenever the caller cannot
         * guarantee that no worker threads are inserting into the buffer.
         */
        template<typename Fn>
        void for_each_submap( Fn &&fn ) {
            std::lock_guard<std::recursive_mutex> lk( submaps_mutex_ );
            for( auto &entry : submaps ) {
                fn( entry );
            }
        }

        bool is_submap_loaded( const tripoint &p ) const {
            return submaps.contains( p );
        }

        /** Return true if no submaps are currently held in this buffer. */
        bool is_empty() const {
            return submaps.empty();
        }

        /**
         * Return the dimension ID this buffer belongs to.
         * Set by mapbuffer_registry::get() at construction time.
         * Empty string ("") = the overworld (primary dimension, legacy path).
         */
        const std::string &get_dimension_id() const {
            return dimension_id_;
        }

        /** Set the dimension ID — called only by mapbuffer_registry. */
        void set_dimension_id( const std::string &id ) {
            dimension_id_ = id;
        }

    private:
        // There's a very good reason this is private,
        // if not handled carefully, this can erase in-use submaps and crash the game.
        void remove_submap( tripoint addr );
        /**
         * Parse the quad JSON stream into @p out without acquiring @c submaps_mutex_
         * or touching the in-memory map.  Called by both @c deserialize() (which then
         * adds under the lock) and @c preload_quad() (which runs on a worker thread).
         */
        void deserialize_into_vec(
            JsonIn &jsin,
            std::vector<std::pair<tripoint, std::unique_ptr<submap>>> &out,
            const std::function<bool( const tripoint & )> &skip_if = nullptr );
        void save_quad( const tripoint &om_addr, std::list<tripoint> &submaps_to_delete,
                        bool delete_after_save );
        submap_map_t submaps;

        /// The dimension this buffer belongs to (set by mapbuffer_registry::get()).
        /// Used to construct the correct save/load path without querying global state.
        std::string dimension_id_;
};

// Included after the full mapbuffer definition to avoid circular dependencies.
// Provides the MAPBUFFER macro and MAPBUFFER_REGISTRY global.
#include "mapbuffer_registry.h"
