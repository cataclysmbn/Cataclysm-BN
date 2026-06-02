#pragma once

#include <optional>
#include <string>
#include <vector>

#include "translations.h"
#include "type_id.h"

template<typename T> class generic_factory;
class JsonObject;

/**
 * Defines a dimension/world type for multi-dimension support.
 *
 * World types specify how dimensions are generated, bounded, saved, and simulated.
 * They replace hardcoded dimension strings with JSON-loadable types for modding support.
 */
struct world_type {
    private:
        friend class generic_factory<world_type>;
        bool was_loaded = false;

    public:
        world_type_id id;

        // Display information
        translation name;
        translation description;

        // Generation settings
        std::string region_settings_id = "default";  // Links to regional_settings
        bool generate_overmap = true;                // False for bounded pocket dims
        bool infinite_bounds = true;                 // False for pocket dimensions

        // Boundaries (for bounded dimensions)
        std::optional<ter_str_id> boundary_terrain;  // Visual tile for out-of-bounds
        // Note: bash message comes from the terrain definition itself

        // Save structure
        std::string save_prefix;  // Folder/file naming prefix (empty for default dimension)

        // Transport (future)
        bool allow_npc_travel = false;
        bool allow_vehicle_travel = false;

        // Coordinate scale relative to the parent dimension.
        // scale_num parent units = scale_den local units.
        // e.g. scale_num=8, scale_den=1 → 1 tile here = 8 tiles in parent (Nether-like).
        int scale_num = 1;
        int scale_den = 1;

        // Per-dimension sunrise/sunset hour overrides (0–23). -1 means use world option default.
        // See calendar::dim_time_config for details.
        int sunrise_summer  = -1;
        int sunrise_winter  = -1;
        int sunrise_equinox = -1;
        int sunset_summer   = -1;
        int sunset_winter   = -1;
        int sunset_equinox  = -1;

        // Permanent day/night modes. These override is_day()/is_night() checks entirely.
        // NOTE: A full custom-ticks-per-day system would require refactoring calendar
        // conversion functions throughout the codebase — deferred.
        bool permanent_daylight = false;
        bool permanent_night    = false;

        void load( const JsonObject &jo, const std::string &src );
        void check() const;
};

namespace world_types
{
/** Get all currently loaded world types */
const std::vector<world_type> &get_all();
/** Finalize all loaded world types */
void finalize_all();
/** Clear all loaded world types (invalidating any pointers) */
void reset();
/** Load world type from JSON definition */
void load( const JsonObject &jo, const std::string &src );
/** Checks all loaded from JSON are valid */
void check_consistency();

/** Get the default world type (base reality) */
const world_type_id &get_default();
} // namespace world_types
