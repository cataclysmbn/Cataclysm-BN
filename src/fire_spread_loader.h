#pragma once

#include <map>
#include <string>
#include <utility>

#include "coordinates.h"
#include "submap_load_manager.h"

/**
 * Manages fire-spread load requests for out-of-bubble simulation.
 *
 * When fire on a properly-loaded submap would spread to an adjacent unloaded
 * submap, fire_spread_loader requests that submap via the load manager using
 * the fire_spread source.  This preserves physically correct fire spread
 * behavior at the boundary of the loaded set.
 *
 * A global ceiling of fire_spread_submap_cap (default 25, player-configurable
 * via FIRE_SPREAD_SUBMAP_CAP option) fire-spread-loaded submaps is enforced
 * across all dimensions combined.
 *
 * The connectivity invariant: a fire-spread-loaded submap is released during
 * prune_disconnected() if it is not reachable via cardinal fire-loaded
 * neighbors from a non-fire_spread (properly-loaded) submap.
 */
class fire_spread_loader
{
    public:
        fire_spread_loader() = default;
        ~fire_spread_loader() = default;

        fire_spread_loader( const fire_spread_loader & ) = delete;
        fire_spread_loader &operator=( const fire_spread_loader & ) = delete;

        /**
         * Called from game::world_tick() when fire is present on a loaded
         * submap and an adjacent submap position is not properly loaded.
         *
         * Does nothing if:
         *   - the global FIRE_SPREAD_CAP is already reached, OR
         *   - the position is already requested (fire or otherwise), OR
         *   - @p pos is not adjacent to any properly-loaded submap.
         */
        void request_for_fire( const std::string &dim, tripoint_abs_sm pos );

        /**
         * Called once per game::world_tick().
         *
         * Releases any fire-spread request whose submap either:
         *   - No longer has any fire fields, OR
         *   - Is not reachable via cardinal fire-loaded neighbors from a
         *     properly-loaded (non-fire_spread) submap (the connectivity
         *     invariant, enforced via BFS flood-fill).
         */
        void prune_disconnected( submap_load_manager &loader );

        /** Total number of currently active fire-spread load requests. */
        int loaded_count() const {
            return static_cast<int>( fire_handles_.size() );
        }

    private:
        using dim_pos_key = std::pair<std::string, tripoint_abs_sm>;

        // Handles for each fire-spread load request, keyed by (dimension_id, submap_pos).
        std::map<dim_pos_key, load_request_handle> fire_handles_;

        // (cap is now the cached global fire_spread_submap_cap — see cached_options.h)
};

extern fire_spread_loader fire_loader;
