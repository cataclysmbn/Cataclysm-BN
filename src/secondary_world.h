#pragma once

#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>

#include "calendar.h"
#include "coordinates.h"
#include "dimension_bounds.h"
#include "type_id.h"

class submap;
class overmap;

/**
 * Holds the state of a secondary dimension that is kept loaded in memory
 * for fast travel and optional background simulation.
 *
 * This class maintains separate map and overmap buffers from the primary world,
 * allowing the game to keep a pocket dimension loaded while the player is in
 * another dimension.
 */
class secondary_world
{
    public:
        /**
         * Construct a secondary world for the given world type and instance.
         * @param type The world type of this dimension
         * @param instance_id The unique instance ID (for pocket dimensions)
         */
        explicit secondary_world( const world_type_id &type, const std::string &instance_id );

        // Non-copyable, movable
        secondary_world( const secondary_world & ) = delete;
        secondary_world &operator=( const secondary_world & ) = delete;
        secondary_world( secondary_world && ) = default;
        secondary_world &operator=( secondary_world && ) = default;

        ~secondary_world();

        /**
         * Transfer buffers from the primary world into this secondary world.
         * Called when leaving a dimension that should be kept loaded.
         * @param bounds The dimension bounds (for bounded pocket dimensions)
         * @param simulation_center The last player position for simulation purposes
         */
        void capture_from_primary( const std::optional<dimension_bounds> &bounds,
                                   const tripoint_abs_sm &simulation_center );

        /**
         * Restore buffers back to the primary world.
         * Called when returning to this kept-loaded dimension.
         * After calling this, the secondary_world is in an invalid state
         * and should be destroyed.
         */
        void restore_to_primary();

        /**
         * Save the secondary world state to disk.
         */
        void save_state();

        /**
         * Simulate one tick of time passing in this dimension.
         * What gets simulated depends on the simulation level setting.
         * @param level The simulation level ("minimal", "moderate", "full")
         * @param delta Time duration to simulate (defaults to 1 turn, for future batch processing)
         */
        void simulate_tick( const std::string &level, time_duration delta = 1_turns );

        /**
         * Free all memory held by this secondary world.
         * After calling this, is_loaded() will return false.
         */
        void unload();

        bool is_loaded() const {
            return loaded_;
        }

        const world_type_id &get_world_type() const {
            return world_type_;
        }

        const std::string &get_instance_id() const {
            return instance_id_;
        }

    private:
        world_type_id world_type_;
        std::string instance_id_;
        bool loaded_ = false;

        // Separate buffers for this dimension
        // Using raw map instead of unique_ptr to mapbuffer to avoid circular dependencies
        std::map<tripoint, std::unique_ptr<submap>> submaps_;
        std::unordered_map<point_abs_om, std::unique_ptr<overmap>> overmaps_;

        // Cached bounds for simulation
        std::optional<dimension_bounds> bounds_;

        // Simulation state
        tripoint_abs_sm simulation_center_;

        // Simulation methods for each level
        void simulate_minimal( time_duration delta );
        void simulate_moderate( time_duration delta );
        void simulate_full( time_duration delta );

        // Helper to check if a position is within bounds
        bool is_in_bounds( const tripoint &sm_pos ) const;
};
