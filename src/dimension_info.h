#pragma once

#include <optional>
#include <string>

#include "coordinates.h"
#include "dimension_bounds.h"
#include "type_id.h"

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

    /// Optional spatial bounds for bounded (pocket) dimensions.  nullopt means the
    /// dimension extends infinitely in all directions.
    std::optional<dimension_bounds> bounds;

    /// Absolute submap coordinate of the portal that leads back to the parent dimension.
    /// This is the player's position in the parent immediately before entering this
    /// dimension.  For the overworld (dimension_id == "") this is meaningless (zeroed).
    tripoint_abs_sm origin_pos;

    /// The dimension_id of the dimension from which this one was entered.
    /// Empty string = entered from the overworld.
    std::string parent_dimension_id;
};
