#pragma once

#include <map>
#include <memory>
#include <optional>
#include <string>

#include "coordinates.h"
#include "dimension_bounds.h"
#include "type_id.h"

class submap;

/**
 * Lightweight metadata holder for a secondary dimension that is kept loaded
 * in the mapbuffer_registry.
 *
 * In the old design, this class owned the submaps directly (submaps_ field).
 * In the new design, submaps live in MAPBUFFER_REGISTRY.get(get_dimension_id()).
 * The submaps_ field is retained only for backwards-compatible deserialization
 * of old save files; it is migrated to the registry via
 * migrate_submaps_to_registry() at load time and is otherwise unused.
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
         * Transfer buffers from the primary world into this secondary world's
         * registry slot.
         *
         * Called when leaving a dimension that should be kept loaded.
         * Submaps are moved from MAPBUFFER_REGISTRY.primary() into
         * MAPBUFFER_REGISTRY.get(get_dimension_id()).
         *
         * @param bounds The dimension bounds (stored for compatibility; unused now)
         * @param simulation_center Last player position (unused; kept for compat)
         */
        void capture_from_primary( const std::optional<dimension_bounds> &bounds,
                                   const tripoint_abs_sm &simulation_center );

        /**
         * Restore buffers from this secondary world's registry slot to the
         * primary world.
         *
         * Called when returning to this kept-loaded dimension.  Submaps are
         * moved from MAPBUFFER_REGISTRY.get(get_dimension_id()) back into
         * MAPBUFFER_REGISTRY.primary().  After this call the secondary_world
         * should be destroyed.
         */
        void restore_to_primary();

        /**
         * Save the secondary world state to disk via the registry.
         */
        void save_state();

        /**
         * Free all memory held by this secondary world (including any submaps
         * still in the registry slot).  After calling this, is_loaded() is false.
         */
        void unload();

        /**
         * Migrate any submaps that were deserialized from an old save file
         * (stored in the legacy submaps_ field) into the mapbuffer_registry.
         * Should be called once during game::load() for each secondary world
         * that was present in the save.
         */
        void migrate_submaps_to_registry();

        bool is_loaded() const {
            return loaded_;
        }

        const world_type_id &get_world_type() const {
            return world_type_;
        }

        const std::string &get_instance_id() const {
            return instance_id_;
        }

        /**
         * Compute the dimension ID (registry key) for this secondary world.
         * This matches the save-file prefix for the dimension.
         */
        std::string get_dimension_id() const;

    private:
        world_type_id world_type_;
        std::string instance_id_;
        bool loaded_ = false;

        // Legacy field: submaps read from old save files before registry migration.
        // Populated only during old-save deserialization; empty in normal operation.
        std::map<tripoint, std::unique_ptr<submap>> submaps_;
};
