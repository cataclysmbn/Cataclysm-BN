#pragma once

#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "coordinates.h"
#include "point.h"

// ---------------------------------------------------------------------------
// submap_load_listener
// ---------------------------------------------------------------------------

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

// ---------------------------------------------------------------------------
// load_request_source / handle / request
// ---------------------------------------------------------------------------

/** Identifies the system that created a load request. */
enum class load_request_source : int {
    reality_bubble,  ///< Player's active reality bubble
    player_base,     ///< A persistent player base that should stay loaded
    script,          ///< Lua/scripted event that needs a region loaded
    fire_spread,     ///< Fire-spread loader keeping adjacent submaps resident
};

/** Opaque handle returned by request_load(); used to update or release. */
using load_request_handle = uint64_t;

/** A single outstanding load request. */
struct submap_load_request {
    load_request_source source = load_request_source::reality_bubble;
    std::string dimension_id;
    tripoint_abs_sm center;
    int radius = 0;  ///< Half-width in submaps; the loaded square is (2*radius+1)^2 per z-level
    int z_min = 0;   ///< Lowest z-level to include (inclusive).  Set to center.z for single-level.
    int z_max = 0;   ///< Highest z-level to include (inclusive).  Set to center.z for single-level.
};

// ---------------------------------------------------------------------------
// submap_load_manager
// ---------------------------------------------------------------------------

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
         * This computes the new desired submap set, diffs it against the
         * previous set, calls on_submap_loaded() for entries that are newly
         * desired and on_submap_unloaded() for entries that are no longer
         * desired.
         *
         * **PRECONDITION (Phase 6):** This function must NOT be called until
         * map::loadn() is updated to use
         *   MAPBUFFER_REGISTRY.get(bound_dimension_)
         * instead of the MAPBUFFER macro for all submap I/O.  Until that
         * change lands, the active dimension's submaps live in the primary
         * registry slot ("") regardless of what get_dimension_prefix()
         * returns, so on_submap_loaded() would look in the wrong registry
         * slot and write nullptr into the grid, crashing the game.
         *
         * Call site: game::do_turn() — add after Phase 6 integration is
         * complete.
         */
        void update();

        /**
         * Return true if the submap at @p pos in @p dim_id is covered by any
         * active load request.
         */
        bool is_requested( const std::string &dim_id, const tripoint_abs_sm &pos ) const;

        /**
         * Return true if @p pos in @p dim_id is covered by a reality_bubble
         * request (i.e. is inside the player's active view).
         */
        bool is_properly_requested( const std::string &dim_id,
                                    const tripoint_abs_sm &pos ) const;

        /**
         * Return the set of dimension IDs that have at least one active request.
         */
        std::vector<std::string> active_dimensions() const;

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

        /** Register a listener to receive load/unload notifications. */
        void add_listener( submap_load_listener *listener );

        /** Unregister a listener.  No-op if not registered. */
        void remove_listener( submap_load_listener *listener );

    private:
        using desired_key = std::pair<std::string, tripoint>;

        load_request_handle next_handle_ = 1;
        std::map<load_request_handle, submap_load_request> requests_;

        /** Desired set from the previous update() call. */
        std::set<desired_key> prev_desired_;

        std::vector<submap_load_listener *> listeners_;

        /** Compute the full desired set from all active requests. */
        std::set<desired_key> compute_desired_set() const;
};

extern submap_load_manager submap_loader;
