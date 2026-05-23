#pragma once

#include <optional>
#include <string>

#include "calendar.h"
#include "coordinates.h"
#include "type_id.h"

/**
 * Defines the boundaries for a bounded dimension (pocket dimension).
 *
 * Boundaries are NOT stored in map data. They are:
 * - Rendered as a specified terrain tile
 * - Completely impassable
 * - Zero storage cost
 * - Skip ALL simulation (temperature, fields, construction, etc.)
 * - Only interaction: bash shows a message
 */
struct dimension_bounds {
    // Bounds in absolute submap coordinates
    tripoint_abs_sm min_bound = { 0, 0, 0 };
    tripoint_abs_sm max_bound = { 1, 1, 0 };

    // What to display for out-of-bounds tiles
    ter_str_id boundary_terrain;

    // What to display for out-of-bounds overmap tiles
    oter_str_id boundary_overmap_terrain;

    bool contains( const tripoint_abs_sm &p ) const {
        return p.x() >= min_bound.x() && p.x() <= max_bound.x() &&
               p.y() >= min_bound.y() && p.y() <= max_bound.y() &&
               p.z() >= min_bound.z() && p.z() <= max_bound.z();
    }

    bool contains( const tripoint_abs_omt &p ) const {
        return contains( project_to<coords::sm>( p ) );
    }

    bool contains( const tripoint_abs_ms &p ) const {
        return contains( project_to<coords::sm>( p ) );
    }

    void serialize( JsonOut &jsout ) const;
    void deserialize( JsonIn &jsin );

    bool operator==( const dimension_bounds &rhs ) const;
};

/**
* Data for items that act as keys to pocket dimensions.
*/
struct pocket_dimension_data {
    pocket_dimension_data() {}
    tripoint_abs_ms entry_point = { SEEX, SEEY, 0 };    // Where player spawns on entry, default to center of OMT
    /**
    * Currently only one pocket dimension can exist in a given map. If this changes, relative coordinates will
    * make this easier. That will only be helpful for pocket dimensions that are truely persistent.
    * Such a system would reduce bloat to some degree for dimensions that are essentially stacks
    * of pocket dimensions.
    **/
    dimension_bounds bounds{};                  // Boundary info struct
    bool is_initialized = false;                // Has the pocket data been set up?
    bool terrain_generated = false;             // Has the terrain been generated?

    // Return tracking - where to go when exiting this pocket
    std::string return_dimension_id;     // Which dimension to return to (empty = overworld)
    world_type_id
    return_world_type;     // World type of the return dimension (may be null for overworld)
    tripoint_abs_ms return_point;        // Where to place player on exit
    const tripoint_abs_sm get_preload_point() const {
        return project_to<coords::sm>( return_point ) - point_rel_sm( g_half_mapsize, g_half_mapsize );
    }

    // Temporary pocket lifetime: set by iuse_pocket_dimension from JSON "lifetime_hours".
    std::optional<time_point> last_player_exit;  // nullopt = player is inside
    std::optional<time_duration> lifetime;       // nullopt = permanent

    void serialize( JsonOut &jsout ) const;
    void deserialize( JsonIn &jsin );

    bool operator==( const pocket_dimension_data &rhs ) const;
};

/**
 * Metadata for a dimension that is active (has at least one submap in the loaded set).
 *
 * Each active dimension gets one entry in `game::loaded_dimensions_`.  The entry is
 * created when the dimension is first entered and removed when the last of its submaps
 * is evicted from the load manager's desired set
 *
 * All fields are plain value types so that `dimension_info` can be stored in
 * `std::unordered_map` without special ownership semantics.
 */
struct dimension_info {
    /// Registry key for this dimension — also the subdirectory name under `dimensions/`
    /// for non-primary dimensions.  Empty string ("") = the overworld (primary).
    std::string dimension_id;

    /// The game world-type associated with this dimension (determines region settings,
    /// generation parameters, etc.).
    world_type_id world_type;

    /// Human-readable name shown in the overmap UI and any "You are in: ..." messages.
    std::string display_name;

    /// Optional info for bounded (pocket) dimensions. nullopt means the
    /// dimension extends infinitely in all directions.
    std::optional<pocket_dimension_data> pocket_info;

    void serialize( JsonOut &jsout ) const;
    void deserialize( JsonIn &jsin );
};
