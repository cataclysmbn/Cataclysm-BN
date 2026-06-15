#pragma once

#include <cstdint>
#include <functional>
#include <limits>
#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "calendar.h"
#include "coordinates.h"
#include "game_constants.h"
#include "item_stack.h"
#include "mapgen_functions.h"
#include "point.h"
#include "type_id.h"
#include "vpart_position.h"

class submap;
class computer;
class Creature;
class field;
class field_entry;
class item;
class JsonIn;
enum ter_bitflags : int;
struct partial_con;
template<typename T>
class location_vector;
template<typename T>
class detached_ptr;
namespace data_vars
{
class data_set;
} // namespace data_vars

struct mapbuffer_generate_omt_options {
    bool defer_postprocess_hooks = false;
    bool worker_safe = false;
    bool use_selected_mapgen = false;
    std::shared_ptr<mapgen_function> selected_mapgen;
};

enum class mapbuffer_lookup_mode : int {
    simulated_only,
    resident_only,
    load_from_disk,
    load_or_generate,
};

struct mapbuffer_lookup_options {
    mapbuffer_lookup_mode mode = mapbuffer_lookup_mode::simulated_only;
};

struct mapbuffer_valid_move_options {
    bool bash = false;
    bool flying = false;
    bool via_ramp = false;
    bool zlevels = true;
    mapbuffer_lookup_options lookup;
};

struct mapbuffer_field_age_options {
    field_type_id type;
    time_duration age = 0_turns;
    bool isoffset = false;
    mapbuffer_lookup_options lookup;
};

struct mapbuffer_field_intensity_options {
    field_type_id type;
    int intensity = 0;
    bool isoffset = false;
    mapbuffer_lookup_options lookup;
};

struct mapbuffer_add_field_options {
    field_type_id type;
    int intensity = std::numeric_limits<int>::max();
    time_duration age = 0_turns;
    mapbuffer_lookup_options lookup;
};

struct mapbuffer_add_computer_options {
    std::string name;
    int security = 0;
    mapbuffer_lookup_options lookup;
};

struct mapbuffer_item_lum_options {
    bool add_luminance = false;
    mapbuffer_lookup_options lookup;
};

class mapbuffer;

struct mapbuffer_add_item_or_charges_options {
    bool overflow = true;
    mapbuffer_lookup_options lookup;
};

struct mapbuffer_erase_item_options {
    location_vector<item>::const_iterator it;
    detached_ptr<item> *out = nullptr;
    mapbuffer_lookup_options lookup;
};

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
        bool add_submap( const tripoint_abs_sm &p, std::unique_ptr<submap> &sm );

        /**
         * Absolute submap lookup with explicit residency/loading policy.
         * Defaults to simulated_only so ordinary callers only see active
         * simulation data unless they explicitly request broader residency.
         *
         * simulated_only: return only if already resident and currently simulated.
         * The simulation set is owned by submap_load_manager and may include
         * non-player-bubble load requests.
         * resident_only: return only if already resident in memory.
         * load_from_disk: load saved/pending data if needed; never generate.
         * load_or_generate: load saved/pending data first, then generate the
         * containing OMT on miss.
         */
        auto get_submap( const tripoint_abs_sm &p,
        mapbuffer_lookup_options options = {} ) -> submap *;

        auto get_ter( const tripoint_abs_ms &p,
        mapbuffer_lookup_options options = {} ) -> std::optional<ter_id>;
        auto set_ter( const tripoint_abs_ms &p, ter_id terrain,
        mapbuffer_lookup_options options = {} ) -> bool;

        auto get_furn( const tripoint_abs_ms &p,
        mapbuffer_lookup_options options = {} ) -> std::optional<furn_id>;
        auto set_furn( const tripoint_abs_ms &p, furn_id furn,
        mapbuffer_lookup_options options = {} ) -> bool;
        auto veh_at( const tripoint_abs_ms &p,
        mapbuffer_lookup_options options = {} ) -> optional_vpart_position;
        auto move_cost( const tripoint_abs_ms &p,
        mapbuffer_lookup_options options = {} ) -> std::optional<int>;
        auto passable( const tripoint_abs_ms &p,
        mapbuffer_lookup_options options = {} ) -> std::optional<bool>;
        auto valid_move( const tripoint_abs_ms &from, const tripoint_abs_ms &to,
        mapbuffer_valid_move_options options = {} ) -> bool;
        auto climb_difficulty( const tripoint_abs_ms &p,
        mapbuffer_lookup_options options = {} ) -> std::optional<int>;
        auto has_flag( const std::string &flag, const tripoint_abs_ms &p,
        mapbuffer_lookup_options options = {} ) -> bool;
        auto has_flag_ter( const std::string &flag, const tripoint_abs_ms &p,
        mapbuffer_lookup_options options = {} ) -> bool;
        auto has_flag_furn( const std::string &flag, const tripoint_abs_ms &p,
        mapbuffer_lookup_options options = {} ) -> bool;
        auto has_flag_vpart( const std::string &flag, const tripoint_abs_ms &p,
        mapbuffer_lookup_options options = {} ) -> bool;
        auto has_flag_furn_or_vpart( const std::string &flag, const tripoint_abs_ms &p,
        mapbuffer_lookup_options options = {} ) -> bool;
        auto has_flag_ter_or_furn( const std::string &flag, const tripoint_abs_ms &p,
        mapbuffer_lookup_options options = {} ) -> bool;
        auto has_flag( ter_bitflags flag, const tripoint_abs_ms &p,
        mapbuffer_lookup_options options = {} ) -> bool;
        auto has_flag_ter( ter_bitflags flag, const tripoint_abs_ms &p,
        mapbuffer_lookup_options options = {} ) -> bool;
        auto has_flag_furn( ter_bitflags flag, const tripoint_abs_ms &p,
        mapbuffer_lookup_options options = {} ) -> bool;
        auto has_flag_ter_or_furn( ter_bitflags flag, const tripoint_abs_ms &p,
        mapbuffer_lookup_options options = {} ) -> bool;
        auto ter_vars( const tripoint_abs_ms &p,
        mapbuffer_lookup_options options = {} ) -> data_vars::data_set *;
        auto furn_vars( const tripoint_abs_ms &p,
        mapbuffer_lookup_options options = {} ) -> data_vars::data_set *;

        auto get_trap( const tripoint_abs_ms &p,
        mapbuffer_lookup_options options = {} ) -> std::optional<trap_id>;
        auto set_trap( const tripoint_abs_ms &p, trap_id trap,
        mapbuffer_lookup_options options = {} ) -> bool;
        auto creature_on_trap( Creature &critter, bool may_avoid = true ) -> void;

        auto get_radiation( const tripoint_abs_ms &p,
        mapbuffer_lookup_options options = {} ) -> std::optional<int>;
        auto set_radiation( const tripoint_abs_ms &p, int radiation,
        mapbuffer_lookup_options options = {} ) -> bool;
        auto adjust_radiation( const tripoint_abs_ms &p, int delta,
        mapbuffer_lookup_options options = {} ) -> std::optional<int>;

        auto get_lum( const tripoint_abs_ms &p,
        mapbuffer_lookup_options options = {} ) -> std::optional<std::uint8_t>;
        auto set_lum( const tripoint_abs_ms &p, std::uint8_t luminance,
        mapbuffer_lookup_options options = {} ) -> bool;

        auto get_temperature( const tripoint_abs_ms &p,
        mapbuffer_lookup_options options = {} ) -> std::optional<int>;
        auto set_temperature( const tripoint_abs_ms &p, int temperature,
        mapbuffer_lookup_options options = {} ) -> bool;

        auto get_field( const tripoint_abs_ms &p,
        mapbuffer_lookup_options options = {} ) -> field *;
        auto has_field_at( const tripoint_abs_ms &p,
        mapbuffer_lookup_options options = {} ) -> bool;
        auto get_field_entry( const tripoint_abs_ms &p, const field_type_id &type,
        mapbuffer_lookup_options options = {} ) -> field_entry *;
        auto get_field_age( const tripoint_abs_ms &p, const field_type_id &type,
        mapbuffer_lookup_options options = {} ) -> std::optional<time_duration>;
        auto get_field_intensity( const tripoint_abs_ms &p, const field_type_id &type,
        mapbuffer_lookup_options options = {} ) -> std::optional<int>;
        auto mod_field_age( const tripoint_abs_ms &p,
                            const mapbuffer_field_age_options &options ) -> std::optional<time_duration>;
        auto mod_field_intensity( const tripoint_abs_ms &p,
                                  const mapbuffer_field_intensity_options &options ) -> std::optional<int>;
        auto set_field_age( const tripoint_abs_ms &p,
                            const mapbuffer_field_age_options &options ) -> std::optional<time_duration>;
        auto set_field_intensity( const tripoint_abs_ms &p,
                                  const mapbuffer_field_intensity_options &options ) -> std::optional<int>;
        auto add_field( const tripoint_abs_ms &p,
                        const mapbuffer_add_field_options &options ) -> bool;
        auto remove_field( const tripoint_abs_ms &p, const field_type_id &type,
        mapbuffer_lookup_options options = {} ) -> bool;
        auto get_items( const tripoint_abs_ms &p,
        mapbuffer_lookup_options options = {} ) -> location_vector<item> *;
        auto add_item_or_charges( const tripoint_abs_ms &p, detached_ptr<item> &&new_item,
        const mapbuffer_add_item_or_charges_options &options = {} ) -> detached_ptr<item>;
        auto add_item( const tripoint_abs_ms &p, detached_ptr<item> &&new_item,
        mapbuffer_lookup_options options = {} ) -> detached_ptr<item>;
        auto erase_item( const tripoint_abs_ms &p,
                         const mapbuffer_erase_item_options &options ) -> location_vector<item>::iterator;
        auto remove_item( const tripoint_abs_ms &p, item *to_remove,
        mapbuffer_lookup_options options = {} ) -> detached_ptr<item>;
        auto clear_items( const tripoint_abs_ms &p,
        mapbuffer_lookup_options options = {} ) -> std::vector<detached_ptr<item>>;
        auto make_item_active( const tripoint_abs_ms &p, item &target,
        mapbuffer_lookup_options options = {} ) -> bool;
        auto make_item_inactive( const tripoint_abs_ms &p, item &target,
        mapbuffer_lookup_options options = {} ) -> bool;
        auto update_item_lum( const tripoint_abs_ms &p, item &target,
                              const mapbuffer_item_lum_options &options ) -> bool;

        auto has_graffiti_at( const tripoint_abs_ms &p,
        mapbuffer_lookup_options options = {} ) -> bool;
        auto graffiti_at( const tripoint_abs_ms &p,
        mapbuffer_lookup_options options = {} ) -> std::optional<std::string>;
        auto set_graffiti( const tripoint_abs_ms &p, const std::string &contents,
        mapbuffer_lookup_options options = {} ) -> bool;
        auto delete_graffiti( const tripoint_abs_ms &p,
        mapbuffer_lookup_options options = {} ) -> bool;

        auto has_signage( const tripoint_abs_ms &p,
        mapbuffer_lookup_options options = {} ) -> bool;
        auto get_signage( const tripoint_abs_ms &p,
        mapbuffer_lookup_options options = {} ) -> std::optional<std::string>;
        auto set_signage( const tripoint_abs_ms &p, const std::string &message,
        mapbuffer_lookup_options options = {} ) -> bool;
        auto delete_signage( const tripoint_abs_ms &p,
        mapbuffer_lookup_options options = {} ) -> bool;

        auto has_computer( const tripoint_abs_ms &p,
        mapbuffer_lookup_options options = {} ) -> bool;
        auto get_computer( const tripoint_abs_ms &p,
        mapbuffer_lookup_options options = {} ) -> computer *;
        auto set_computer( const tripoint_abs_ms &p, const computer &terminal,
        mapbuffer_lookup_options options = {} ) -> bool;
        auto add_computer( const tripoint_abs_ms &p,
                           const mapbuffer_add_computer_options &options ) -> computer *;
        auto delete_computer( const tripoint_abs_ms &p,
        mapbuffer_lookup_options options = {} ) -> bool;

        auto partial_con_at( const tripoint_abs_ms &p,
        mapbuffer_lookup_options options = {} ) -> partial_con *;
        auto partial_con_set( const tripoint_abs_ms &p, std::unique_ptr<partial_con> con,
        mapbuffer_lookup_options options = {} ) -> bool;
        auto partial_con_remove( const tripoint_abs_ms &p,
        mapbuffer_lookup_options options = {} ) -> bool;

        /** Get a submap stored in this buffer.
         *
         * @param x, y, z The absolute world position in submap coordinates.
         * Same as the ones in @ref add_submap.
         * @return NULL if the submap is not in the mapbuffer
         * and could not be loaded. The mapbuffer takes care of the returned
         * submap object, don't delete it on your own.
         */
        submap *lookup_submap( const tripoint_abs_sm &p );

        /** Get a submap only if it's already loaded in memory.
         * Unlike lookup_submap(), this does NOT query the database for missing submaps.
         * Use this for out-of-bounds positions where we know there's no DB entry,
         * to avoid ~2400 wasted SQLite queries per pocket dimension map load.
         *
         * Thread-safe: may be called from background worker threads (under gen_mutex).
         */
        submap *lookup_submap_in_memory( const tripoint_abs_sm &p ) {
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
         * Parallel-safe omt prefetch: reads all submaps in the OMT at
         * @p omt_addr from disk and adds them to the in-memory buffer.
         *
         * May be called concurrently from worker threads for different omt
         * addresses.  The disk I/O phase runs outside @c submaps_mutex_; the
         * add phase acquires the mutex briefly per submap.
         *
         * If the omt file does not exist (submaps need generation), this is a
         * no-op; the caller must fall back to the synchronous generation path in
         * map::loadn().
         *
         * Thread-safety note: the dim-aware @c world::read_map_omt overload is
         * used, so no global (g_active_dimension_id) is read at worker-thread
         * execution time.  For SQLite-backed saves, the connection must be opened
         * with SQLITE_OPEN_FULLMUTEX so concurrent worker-thread reads use the
         * same handle in SQLite's serialized mode.
         */
        /**
         * Returns true if data was loaded from the in-memory write-back cache
         * (pending_writes_) rather than from disk.  A cache-loaded omt has not yet
         * been flushed to actual disk files and must be re-saved before eviction.
         */
        bool preload_omt( const tripoint_abs_omt &omt_addr );

        /**
         * Generate all submaps in the OMT at @p omt_addr if any are not yet
         * resident in memory.
         *
         * When @p options.worker_safe is true, Lua mapgen is reported via
         * mapgen_result_status::needs_main_thread instead of running on the worker.
         * Lua postprocess hooks can be deferred for batched main-thread dispatch.
         *
         * Returns whether generation ran, was skipped, or must be retried on the
         * main thread with the selected Lua generator.
         */
        auto generate_omt( const tripoint_abs_omt &omt_addr,
        const mapbuffer_generate_omt_options &options = {} ) -> mapgen_result;

        /**
         * Destroy submaps that were discarded by preload_omt() because the in-memory
         * version already existed.  Must be called on the main thread after all
         * preload_omt() futures have been joined.
         *
         * safe_reference<T> relies on unsynchronised global statics; destructing
         * submaps (and their items) on worker threads would race on those statics.
         * preload_omt() defers such
         * destruction here instead of letting it happen on the worker.
         */
        auto drain_pending_submap_destroy() -> void;

        /**
         * Evict all submaps in the OMT at @p omt_addr.
         *
         * If @p save is true (default), the omt is serialised into the in-memory
         * write-back cache (@c pending_writes_) before the submap objects are freed.
         * The cache is flushed to disk only on an explicit save.
         * Pass @p save = false only for border-preloaded omts that were never
         * simulated — their in-memory content is identical to what is already on
         * disk, so no write is needed.
         *
         * Does nothing for omts that are fully uniform (they regenerate on demand).
         */
        void unload_omt( const tripoint_abs_omt &omt_addr, bool save = true );

        /**
         * Move all submaps from this buffer into @p dest, leaving this buffer empty.
         * Used by the dimension-transition system to migrate submaps between registry slots
         * without a disk round-trip.
         */
        void transfer_all_to( mapbuffer &dest );

    private:
        using submap_map_t = std::unordered_map<tripoint_abs_sm, std::unique_ptr<submap>>;

        auto active_reality_bubble_local( const tripoint_abs_ms &p ) const
        -> std::optional<tripoint_bub_ms>;
        auto invalidate_active_terrain_set_caches( const tripoint_abs_ms &p, const ter_id &old_id,
                const ter_id &new_id ) const -> void;
        auto sync_furniture_change_side_tables( const tripoint_abs_ms &p, submap &sm,
                                                const point_sm_ms &local, const furn_id &old_id,
                                                const furn_id &new_id ) const -> void;
        auto invalidate_active_furniture_set_caches( const tripoint_abs_ms &p, const furn_id &old_id,
                const furn_id &new_id ) const -> void;
        auto sync_active_trap_change_side_tables( const tripoint_abs_ms &p, const point_sm_ms &local,
                const trap_id &old_id, const trap_id &new_id ) const -> void;
        auto invalidate_active_field_add_caches( const tripoint_abs_ms &p,
                const field_type_id &type ) const -> void;
        auto invalidate_active_field_remove_caches( const tripoint_abs_ms &p,
                const field_type_id &type ) const -> void;
        auto sync_active_item_submap_index( const tripoint_abs_ms &p, const submap &sm ) const -> void;
        auto invalidate_active_item_luminance_cache( const tripoint_abs_ms &p ) const -> void;

        /// Guards all accesses to `submaps` that may overlap with background
        /// worker threads calling add_submap().  std::recursive_mutex allows
        /// mapgen code (running under a held lock) to call lookup_submap_in_memory()
        /// or add_submap() without deadlocking.
        mutable std::recursive_mutex submaps_mutex_;

        /// Submaps that preload_omt() could not add (duplicate already in memory).
        /// Their destruction is deferred here and drained on the main thread via
        /// drain_pending_submap_destroy() to avoid racing on safe_reference<T>
        /// global statics.
        mutable std::mutex pending_destroy_mutex_;
        std::vector<std::unique_ptr<submap>> pending_destroy_submaps_;

        /// Serialised omts awaiting disk flush.  Written by the save=true branch of
        /// unload_omt() (main thread); read back
        /// by preload_omt() (worker threads) before falling through to disk.  Flushed
        /// to disk by save() and discarded by clear(), leaving disk files untouched so
        /// the player can revert to the pre-session state by quitting without saving.
        mutable std::mutex pending_writes_mutex_;
        std::map<tripoint_abs_omt, std::string> pending_writes_;

    public:
        submap_map_t::iterator begin() {
            return submaps.begin();
        }
        submap_map_t::iterator end() {
            return submaps.end();
        }

        /**
         * Iterate all submaps under @c submaps_mutex_, allowing background
         * preload_omt() workers to run concurrently without UB.
         *
         * Use this instead of begin()/end() whenever the caller cannot
         * guarantee that no worker threads are inserting into the buffer.
         */
        template<typename Fn>
        void for_each_submap( Fn &&fn ) {
            std::lock_guard<std::recursive_mutex> lk( submaps_mutex_ );
            for( std::pair<const tripoint_abs_sm, std::unique_ptr<submap>> &entry : submaps ) {
                fn( entry );
            }
        }

        bool is_submap_loaded( const tripoint_abs_sm &p ) const {
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
        auto get_dimension_id() const -> const dimension_id & { // *NOPAD*
            return dimension_id_;
        }

        /** Set the dimension ID — called only by mapbuffer_registry. */
        auto set_dimension_id( const dimension_id &id ) -> void {
            dimension_id_ = id;
        }

    private:
        // There's a very good reason this is private,
        // if not handled carefully, this can erase in-use submaps and crash the game.
        void remove_submap( tripoint_abs_sm addr );
        /**
         * Parse the omt JSON stream into @p out without acquiring @c submaps_mutex_
         * or touching the in-memory map.  Called by both @c deserialize() (which then
         * adds under the lock) and @c preload_omt() (which runs on a worker thread).
         */
        void deserialize_into_vec(
            JsonIn &jsin,
            std::vector<std::pair<tripoint_abs_sm, std::unique_ptr<submap>>> &out,
            const std::function<bool( const tripoint_abs_sm & )> &skip_if = nullptr );
        void save_omt( const tripoint_abs_omt &omt_addr, std::list<tripoint_abs_sm> &submaps_to_delete,
                       bool delete_after_save );
        submap_map_t submaps;

        /// The dimension this buffer belongs to (set by mapbuffer_registry::get()).
        /// Used to construct the correct save/load path without querying global state.
        dimension_id dimension_id_;
};

// Included after the full mapbuffer definition to avoid circular dependencies.
// Provides the MAPBUFFER macro and MAPBUFFER_REGISTRY global.
#include "mapbuffer_registry.h"
