#pragma once

#include <list>
#include <map>
#include <memory>
#include <string>

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
         * during Phase 1 collection. Pass false when save() is called from a
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
         */
        submap *lookup_submap_in_memory( const tripoint &p ) {
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
         * Conditionally save and then remove the submap at @p pos from the buffer.
         * The containing OMT quad is saved to disk first (unless it is fully uniform),
         * then the submap is erased from memory.  Does nothing if @p pos is not loaded.
         */
        void unload_submap( const tripoint_abs_sm &pos );

        /**
         * Save and evict all submaps in the OMT quad at @p om_addr in one shot.
         * This is the correct way to evict a quad: calling unload_submap() per-submap
         * repeatedly overwrites the quad file without the previously-removed siblings,
         * causing data loss and "file did not contain expected submap" errors on reload.
         * Does nothing for quads that are fully uniform (they regenerate on demand).
         */
        void unload_quad( const tripoint &om_addr );

        /**
         * Move all submaps from this buffer into @p dest, leaving this buffer empty.
         * Used by the dimension-transition system to migrate submaps between registry slots
         * without a disk round-trip.
         */
        void transfer_all_to( mapbuffer &dest );

    private:
        using submap_map_t = std::map<tripoint, std::unique_ptr<submap>>;

    public:
        submap_map_t::iterator begin() {
            return submaps.begin();
        }
        submap_map_t::iterator end() {
            return submaps.end();
        }

        bool is_submap_loaded( const tripoint &p ) const {
            return submaps.contains( p );
        }

        /** Return true if no submaps are currently held in this buffer. */
        bool is_empty() const {
            return submaps.empty();
        }

    private:
        // There's a very good reason this is private,
        // if not handled carefully, this can erase in-use submaps and crash the game.
        void remove_submap( tripoint addr );
        submap *unserialize_submaps( const tripoint &p );
        void deserialize( JsonIn &jsin );
        void save_quad( const tripoint &om_addr, std::list<tripoint> &submaps_to_delete,
                        bool delete_after_save );
        submap_map_t submaps;
};

// mapbuffer_registry.h provides the MAPBUFFER backward-compatibility macro and the
// MAPBUFFER_REGISTRY global.  It is included at the end of this header (after the
// full mapbuffer class definition) so that mapbuffer_registry.h can forward-declare
// mapbuffer without a circular-header dependency.
// Side-effect: any translation unit that includes mapbuffer.h also gets the MAPBUFFER
// macro and MAPBUFFER_REGISTRY without explicitly including mapbuffer_registry.h.
// This is intentional — the macro is needed wherever mapbuffer objects are used — but
// be aware of it when auditing include chains.
#include "mapbuffer_registry.h"
