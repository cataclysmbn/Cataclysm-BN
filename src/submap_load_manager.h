#pragma once

#include <cstdint>
#include <list>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include <future>

#include "coordinates.h"
#include "mapgen_functions.h"
#include "point.h"
#include "type_id.h"

class mapbuffer;



/** Identifies the system that created a load request. */
enum class load_request_source : int {
    reality_bubble,  ///< Player's active reality bubble
    player_base,     ///< A persistent player base that should stay loaded
    script,          ///< Lua/scripted event that needs a region loaded
    fire_spread,     ///< Fire-spread loader keeping adjacent submaps resident
    lazy_border,     ///< Kept in memory around the bubble but not simulated
    portal_preload,  ///< portal_tile keeping its target area resident
};

/** Opaque handle returned by request_load(); used to update or release. */
using load_request_handle = uint64_t;

/** A single outstanding load request. */
struct submap_load_request {
    load_request_source source = load_request_source::reality_bubble;
    dimension_id dim_id;
    point_abs_sm begin;
    point_abs_sm end;

    auto operator==( const submap_load_request &rhs ) const -> bool = default;
};

/**
 * Tracks which submaps should be resident in memory across all dimensions.
 *
 * Callers create requests via request_load() and receive a handle.  They
 * call update_request() as the player moves and release_load() when the
 * region is no longer needed.  update() must be called once per turn; it
 * loads and evicts submaps, pushing the simulated set to mapbuffer each frame.
 */
class submap_load_manager
{
    public:
        submap_load_manager() = default;
        ~submap_load_manager() = default;

        // Non-copyable
        submap_load_manager( const submap_load_manager & ) = delete;
        submap_load_manager &operator=( const submap_load_manager & ) = delete;

        /**
         * Register a new load request.  The request always covers all z-levels
         * (-OVERMAP_DEPTH to OVERMAP_HEIGHT); omts are full vertical pillars.
         *
         * @return A handle that identifies this request for future updates/releases.
         */
        auto request_load( load_request_source source,
                           const dimension_id &dim_id,
                           const point_abs_sm &begin,
                           const point_abs_sm &end ) -> load_request_handle;

        /**
         * Move the bounds of an existing request (e.g. on player movement).
         * No-op if the handle is not found.
         */
        auto update_request( load_request_handle handle,
                             const point_abs_sm &begin,
                             const point_abs_sm &end ) -> void;

        /**
         * Release a load request.  The submaps it was keeping loaded may be
         * evicted on the next update() call.
         */
        void release_load( load_request_handle handle );

        /**
         * Process all active requests, load newly-simulated submaps, and
         * evict departed OMT columns.
         *
         * Simulated positions (reality_bubble, fire_spread, player_base,
         * script) are loaded synchronously, then pushed to mapbuffer via
         * set_simulated_submaps().  Lazy-border positions, when enabled,
         * are kept resident but do NOT enter the simulated set.  OMTs that
         * leave the desired set are retained briefly in memory, then evicted
         * over a small per-turn budget.
         *
         * Call site: game::do_turn(), game::update_map()
         */
        auto update( bool defer_lazy_border_work = false ) -> void;

        /**
         * Process lazy-border preload/retention work that update() intentionally
         * deferred.  Safe to call from render-time CPU/GPU overlap windows because
         * lazy-border submaps are resident-only and do not enter simulation.
         */
        auto process_deferred_lazy_border_work() -> void;
        /** True when a later GPU in-flight window has lazy-border resident work to drain. */
        auto has_deferred_lazy_border_work() const noexcept -> bool;

        /** Update the player position used to budget lazy-border preloading. */
        auto update_lazy_border_focus( const dimension_id &dim_id,
                                       const tripoint_abs_ms &pos ) -> void;

        /**
         * Block until all in-flight background lazy-load tasks complete.
         *
         * Must be called before saving the game, switching dimensions, or
         * shutting down the thread pool so that no worker holds raw submap
         * pointers across those operations.
         */
        void drain_lazy_loads();

        /**
         * Clear all cached state so the next update() does not evict submaps
         * based on stale dimension entries.
         *
         * Must be called after draining lazy loads, when switching dimensions.
         * Without this, the eviction pass in update() would call unload_omt()
         * on the old dimension's positions — which now hold freshly-generated
         * submaps for the new dimension in the primary slot — freeing them
         * while m.grid still holds raw pointers to them (use-after-free crash).
         */
        void flush_prev_desired();

        /**
         * Return true if the submap at @p pos in @p dim_id is covered by any
         * active load request.
         */
        auto is_requested( const dimension_id &dim_id, const point_abs_sm &pos ) const -> bool;
        auto is_requested( const dimension_id &dim_id, const tripoint_abs_sm &pos ) const -> bool {
            return is_requested( dim_id, pos.xy() );
        }

        /**
         * Return true if @p pos in @p dim_id is covered by a reality_bubble
         * request (i.e. is inside the player's loaded square grid).
         */
        auto is_properly_requested( const dimension_id &dim_id,
                                    const tripoint_abs_sm &pos ) const -> bool;
        /**
        * Return true if submap at @p pos in @p dim_id is loaded in memory.
        */
        auto is_loaded( const dimension_id &dim_id,
                        const point_abs_sm &pos ) const -> bool;
        auto is_loaded( const dimension_id &dim_id,
                        const tripoint_abs_sm &pos ) const -> bool {
            return is_loaded( dim_id, pos.xy() );
        }

        /**
         * Return true if @p pos in @p dim_id is covered by any active load
         * request whose source is NOT lazy_border.
         *
         * Positions that are only in the desired set via a lazy_border request
         * are kept resident in memory but are not actively simulated (fields,
         * fire, NPCs, etc.).  Use this to gate per-turn processing in
         * world_tick() and similar loops.
         */
        auto is_simulated( const dimension_id &dim_id,
                           const point_abs_sm &pos ) const -> bool;
        auto is_simulated( const dimension_id &dim_id,
                           const tripoint_abs_sm &pos ) const -> bool {
            return is_simulated( dim_id, pos.xy() );
        }



        /**
         * Return horizontal submap positions currently in the simulated set for
         * @p dim_id. Load requests cover all z-levels, so callers that need
         * concrete submap objects should expand these positions over the full
         * z-level range.
         */
        auto simulated_submaps( const dimension_id &dim_id ) const -> std::span<const point_abs_sm>;

        /**
         * Return the set of dimension IDs that have at least one active request.
         */
        auto active_dimensions() const -> std::vector<dimension_id>;

        /**
         * Return all active load requests whose source is not reality_bubble.
         *
         * Used by game-logic processing loops (load_npcs, monmove, etc.) to
         * find loaded regions that need entity processing outside the player's
         * reality bubble.  Each returned request describes a region that is
         * fully resident in its mapbuffer and should receive the same per-turn
         * game logic as the reality bubble.
         */
        auto non_bubble_requests() const -> std::vector<submap_load_request>;



    private:
        using desired_key = std::pair<dimension_id, point_abs_sm>;
        using omt_key    = std::pair<dimension_id, tripoint_abs_omt>;
        using omt_column_key    = std::pair<dimension_id, point_abs_omt>;

        /** Hash for pair<dimension_id, CoordType> used by unordered containers.
         *  CoordType must be hashable via std::hash (all coord_point specializations are). */
        template<typename CoordType>
        struct coord_pair_hash {
            auto operator()( const std::pair<dimension_id, CoordType> &k ) const noexcept
            -> std::size_t {
                auto h = std::hash<dimension_id> {}( k.first );
                h ^= std::hash<CoordType> {}( k.second ) + 0x9e3779b9 + ( h << 6 ) + ( h >> 2 );
                return h;
            }
        };

        using key_set = std::unordered_set<desired_key, coord_pair_hash<point_abs_sm>>;
        using horizontal_omt_set = std::unordered_set<omt_column_key,
              coord_pair_hash<point_abs_omt>>;
        using retained_omt_list = std::list<omt_column_key>;
        using lazy_omt_job_list = std::list<omt_key>;
        struct lazy_omt_focus {
            dimension_id dim_id;
            tripoint_abs_ms pos;
        };
        struct lazy_omt_load_result {
            bool dirty = false;
            mapgen_result generation;

            auto generated() const -> bool {
                return generation.is_generated();
            }
        };
        struct lazy_omt_start_result {
            bool started = false;
            bool generated = false;
        };
        struct lazy_omt_load_options {
            bool defer_postprocess_hooks = false;
            bool worker_safe = false;
            bool use_selected_mapgen = false;
            std::shared_ptr<mapgen_function> selected_mapgen;
        };

        load_request_handle next_handle_ = 1;
        std::map<load_request_handle, submap_load_request> requests_;

        /** Previous all_desired set for departed-omt detection in update(). */
        key_set previous_all_desired_;

        /** Non-simulated OMT columns kept resident for short-term backtracking. */
        retained_omt_list retained_omts_;
        std::unordered_map<omt_column_key, retained_omt_list::iterator,
            coord_pair_hash<point_abs_omt>> retained_omt_index_;

        /** OMT z-levels waiting for amortized lazy-border preload. */
        lazy_omt_job_list lazy_omt_jobs_;
        std::unordered_map<omt_key, lazy_omt_job_list::iterator,
            coord_pair_hash<tripoint_abs_omt>> lazy_omt_job_index_;
        std::map<omt_key, std::future<lazy_omt_load_result>> lazy_omt_futures_;

        /** Compute the simulated desired set (excludes lazy_border). */
        key_set compute_desired_set() const;
        auto rebuild_simulated_submaps_by_dimension( const key_set &simulated ) -> void;

        /** Compute OMT-space lazy-border columns. */
        auto compute_lazy_border_omts() const -> horizontal_omt_set;

        /** Add lazy_border positions into @p target. */
        auto add_lazy_border_into( key_set &target,
                                   const horizontal_omt_set &border_omts ) const -> void;

        auto current_lazy_border_omt_count() const -> std::size_t;
        auto retained_omt_soft_cap() const -> std::size_t;
        auto retained_omt_hard_cap() const -> std::size_t;
        auto retained_omt_panic_cap() const -> std::size_t;
        auto retained_omt_base_budget() const -> std::size_t;
        auto retain_omt( const omt_column_key &key ) -> void;
        auto erase_retained_omt( const omt_column_key &key ) -> void;
        auto erase_desired_retained_omts( const key_set &desired ) -> void;
        auto evict_omt_column( const omt_column_key &key ) -> void;
        auto evict_oldest_retained_omts( std::size_t count ) -> void;
        auto process_retained_omt_eviction() -> void;
        auto run_deferred_mapgen_hooks_and_omt_post_passes(
            const horizontal_omt_set &generated_omt_columns ) -> void;
        static auto load_lazy_omt_zlevel_data( mapbuffer &mb,
                                               const tripoint_abs_omt &omt_addr,
                                               const lazy_omt_load_options &options )
        -> lazy_omt_load_result;
        auto complete_lazy_omt_result_on_main_thread( const omt_key &key,
                lazy_omt_load_result result ) -> lazy_omt_load_result;
        auto erase_lazy_omt_job( const omt_key &key ) -> void;
        auto apply_lazy_omt_result( const omt_key &key,
                                    const lazy_omt_load_result &result ) -> bool;
        auto finish_lazy_omt_job( const omt_key &key ) -> bool;
        auto reap_lazy_omt_jobs() -> void;
        auto start_lazy_omt_job( const omt_key &key ) -> lazy_omt_start_result;
        auto lazy_omt_priority( const omt_column_key &key ) const -> int;
        auto queue_lazy_border_omts( const horizontal_omt_set &border_omts ) -> void;
        auto has_lazy_border_work_pending() const -> bool;
        auto process_or_defer_lazy_border_work( bool defer_lazy_border_work ) -> void;
        auto process_lazy_border_work() -> void;
        auto process_lazy_border_preload() -> void;

        // dirty_omts_ removed in Phase 1 — mapbuffer owns dirty tracking
        // via mapbuffer::dirty_columns_.  See set_simulated_submaps().

        /** Snapshot of all request bounds from the previous update().
         *  Used to detect steady-state and skip expensive recomputation. */
        std::vector<std::pair<load_request_handle, submap_load_request>> prev_requests_;

        std::map<dimension_id, std::vector<point_abs_sm>> simulated_submaps_by_dimension_;

        point lazy_omt_preload_direction_ = point_zero;
        std::optional<lazy_omt_focus> lazy_omt_focus_;
        double lazy_omt_budget_credit_ = 0.0;
        int lazy_omt_last_credit_turn_ = -1;
        bool lazy_border_work_deferred_ = false;
};

extern submap_load_manager submap_loader;
