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
 * A global ceiling of FIRE_SPREAD_CAP (25) fire-spread-loaded submaps is
 * enforced across all dimensions combined.
 *
 * The connectivity invariant: a fire-spread-loaded submap is released during
 * prune_disconnected() if it has no cardinal neighbor covered by a
 * non-fire_spread (properly-loaded) request.
 */
class fire_spread_loader
{
    public:
        fire_spread_loader() = default;
        ~fire_spread_loader() = default;

        fire_spread_loader( const fire_spread_loader & ) = delete;
        fire_spread_loader &operator=( const fire_spread_loader & ) = delete;

        /**
         * Called from game::tick_submap() when fire is present on a loaded
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
         *   - Has no cardinal neighbor covered by a non-fire_spread request
         *     (the connectivity invariant).
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

        // Maximum fire-spread-loaded submaps across all dimensions.
        static constexpr int FIRE_SPREAD_CAP = 25;
};

extern fire_spread_loader fire_loader;
