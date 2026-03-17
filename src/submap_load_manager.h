#pragma once

#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

#include <future>

#include "coordinates.h"
#include "point.h"

/**
 * Interface for objects that need to react when submaps become resident or
 * are evicted from memory.
 *
 * Implementors are registered with submap_load_manager::add_listener() and
 * are notified during submap_load_manager::update().
 */
class submap_load_listener
{
    public:
        virtual ~submap_load_listener() = default;

        /**
         * Called when the submap at @p pos in dimension @p dim_id has just
         * been loaded into its mapbuffer and is ready for use.
         */
        virtual void on_submap_loaded( const tripoint_abs_sm &pos,
                                       const std::string &dim_id ) = 0;

        /**
         * Called just before the submap at @p pos in dimension @p dim_id is
         * removed from its mapbuffer.
         */
        virtual void on_submap_unloaded( const tripoint_abs_sm &pos,
                                         const std::string &dim_id ) = 0;
};

/** Identifies the system that created a load request. */
enum class load_request_source : int {
    reality_bubble,  ///< Player's active reality bubble
    player_base,     ///< A persistent player base that should stay loaded
    script,          ///< Lua/scripted event that needs a region loaded
    fire_spread,     ///< Fire-spread loader keeping adjacent submaps resident
    lazy_border,     ///< Kept in memory around the bubble but not simulated
};

/** Opaque handle returned by request_load(); used to update or release. */
using load_request_handle = uint64_t;

/** A single outstanding load request. */
struct submap_load_request {
    load_request_source source = load_request_source::reality_bubble;
    std::string dimension_id;
    tripoint_abs_sm center;
    int radius = 0;  ///< Half-width in submaps.  For reality_bubble this defines the circle
    ///< radius; for other sources a (2*radius+1)^2 square is loaded per z-level.
    int z_min = 0;   ///< Lowest z-level to include (inclusive).  Set to center.z for single-level.
    int z_max = 0;   ///< Highest z-level to include (inclusive).  Set to center.z for single-level.
};

/**
 * Tracks which submaps should be resident in memory across all dimensions.
 *
 * Callers create requests via request_load() and receive a handle.  They
 * call update_request() as the player moves and release_load() when the
 * region is no longer needed.  update() must be called once per turn; it
 * computes the desired-set delta and fires listener notifications.
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
         * Register a new load request.
         *
         * @p z_min and @p z_max control the z-level range covered by the request.
         * Pass the same value for both to cover a single z-level.  For reality-bubble
         * requests in z-level builds, pass @c -OVERMAP_DEPTH and @c OVERMAP_HEIGHT.
         *
         * @return A handle that identifies this request for future updates/releases.
         */
        load_request_handle request_load( load_request_source source,
                                          const std::string &dim_id,
                                          const tripoint_abs_sm &center,
                                          int radius,
                                          int z_min,
                                          int z_max );

        /**
         * Move the center of an existing request (e.g. on player movement).
         * No-op if the handle is not found.
         */
        void update_request( load_request_handle handle,
                             const tripoint_abs_sm &new_center );

        /**
         * Release a load request.  The submaps it was keeping loaded may be
         * evicted on the next update() call.
         */
        void release_load( load_request_handle handle );

        /**
         * Process all active requests, fire load/unload events on listeners.
         *
         * Simulated positions (reality_bubble, fire_spread, player_base,
         * script) are loaded synchronously and trigger listener notifications.
         * Lazy-border positions are submitted to the thread pool for
         * background disk-only loading (preload_quad) and do NOT trigger
         * listener notifications.  Eviction protects the full set
         * (simulated + border).
         *
         * Call site: game::do_turn(), game::update_map()
         */
        void update();

        /**
         * Block until all background lazy-border preload_quad tasks complete.
         *
         * Normal gameplay does NOT need this — update() reaps completed
         * futures non-blockingly, and world_tick() uses for_each_submap()
         * (locked iteration) for thread safety.
         *
         * Use this only when ALL background work must finish before
         * proceeding: saving the game, switching dimensions, or shutting
         * down the thread pool.
         */
        void drain_lazy_loads();

        /**
         * Return true if the submap at @p pos in @p dim_id is covered by any
         * active load request.
         */
        bool is_requested( const std::string &dim_id, const tripoint_abs_sm &pos ) const;

        /**
         * Return true if @p pos in @p dim_id is covered by a reality_bubble
         * request (i.e. is inside the player's loaded square grid).
         */
        bool is_properly_requested( const std::string &dim_id,
                                    const tripoint_abs_sm &pos ) const;
        /**
        * Return true if submap at @p pos in @p dim_id is loaded in memory.
        */
        bool is_loaded( const std::string &dim_id,
                        const tripoint_abs_sm &pos ) const;

        /**
         * Return true if @p pos in @p dim_id is covered by any active load
         * request whose source is NOT lazy_border.
         *
         * Positions that are only in the desired set via a lazy_border request
         * are kept resident in memory but are not actively simulated (fields,
         * fire, NPCs, etc.).  Use this to gate per-turn processing in
         * world_tick() and similar loops.
         */
        bool is_simulated( const std::string &dim_id,
                           const tripoint_abs_sm &pos ) const;

        /**
         * Return the set of dimension IDs that have at least one active request.
         */
        std::vector<std::string> active_dimensions() const;

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

        /**
         * Clear the previous desired set so the next update() call does not
         * evict any submaps based on stale old-dimension entries.
         *
         * Call this when switching dimensions (in game::load_map) after
         * releasing the old reality-bubble handle.  Without this, the
         * eviction pass in update() would call unload_quad() on the old
         * dimension's positions — which now hold freshly-generated submaps
         * for the new dimension in the primary slot — freeing them while
         * m.grid still holds raw pointers to them (use-after-free crash).
         */
        void flush_prev_desired();

        /**
         * Precompute the set of (dx, dy) offsets that form a filled square of
         * the given @p radius and cache them for use in compute_desired_set().
         *
         * Must be called whenever the reality-bubble radius changes (i.e. from
         * map::resize()).  The square footprint means ALL submaps in the full
         * (2*radius+1)×(2*radius+1) grid are tracked and protected from eviction,
         * matching the square grid that map::loadn() loads.
         */
        auto update_load_shape( int radius ) -> void;

        /** Register a listener to receive load/unload notifications. */
        void add_listener( submap_load_listener *listener );

        /** Unregister a listener.  No-op if not registered. */
        void remove_listener( submap_load_listener *listener );

    private:
        using desired_key = std::pair<std::string, tripoint>;
        using quad_key = std::pair<std::string, tripoint>;

        /** Hash for pair<string, tripoint> used by unordered containers. */
        struct pair_hash {
            auto operator()( const desired_key &k ) const noexcept -> std::size_t {
                auto h = std::hash<std::string> {}( k.first );
                h ^= std::hash<tripoint> {}( k.second ) + 0x9e3779b9 + ( h << 6 ) + ( h >> 2 );
                return h;
            }
        };

        using key_set = std::unordered_set<desired_key, pair_hash>;

        load_request_handle next_handle_ = 1;
        std::map<load_request_handle, submap_load_request> requests_;

        /** Full desired set (simulated + border) from the previous update(). */
        key_set prev_desired_;

        /** Simulated-only subset from the previous update().
         *  Used for listener notification diffs. */
        key_set prev_simulated_;

        std::vector<submap_load_listener *> listeners_;

        /** Compute the simulated desired set (excludes lazy_border). */
        key_set compute_desired_set() const;

        /** Add lazy_border positions into @p target. */
        void compute_border_into( key_set &target ) const;

        /** Cached (dx, dy) offsets for the full reality-bubble square footprint. */
        std::vector<point> bubble_offsets_;

        /** In-flight preload_quad futures for lazy border positions.
         *  Each future is paired with the quad key it was submitted for,
         *  so we can remove it from lazy_in_flight_ when reaped. */
        std::vector<std::pair<quad_key, std::future<void>>> lazy_futures_;

        /** Quad keys with in-flight lazy futures — prevents duplicate submissions. */
        std::unordered_set<quad_key, pair_hash> lazy_in_flight_;

        /** Snapshot of all request centers from the previous update().
         *  Used to detect steady-state and skip expensive recomputation. */
        std::vector<std::pair<load_request_handle, tripoint>> prev_centers_;
};

extern submap_load_manager submap_loader;
