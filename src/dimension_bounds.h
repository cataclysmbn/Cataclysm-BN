#pragma once

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
    tripoint_abs_sm min_bound;
    tripoint_abs_sm max_bound;

    // What to display for out-of-bounds tiles
    ter_str_id boundary_terrain;

    // What to display for out-of-bounds overmap tiles
    oter_str_id boundary_overmap_terrain;

    /**
     * Check if a point in absolute submap coordinates is within bounds.
     */
    bool contains( const tripoint_abs_sm &p ) const;

    /**
     * Check if a point in absolute overmap terrain coordinates is within bounds.
     * Each OMT spans 2 submaps in x and y.
     */
    bool contains_omt( const tripoint_abs_omt &p ) const;

    /**
     * Check if a point in absolute map square coordinates is within bounds.
     */
    bool contains_ms( const tripoint_abs_ms &p ) const;

    /**
     * Check if a local map tripoint (relative to current map) is within bounds.
     * Requires the map's absolute position for conversion.
     * @param p local tripoint
     * @param map_origin the map's absolute submap origin
     */
    bool contains_local( const tripoint &p, const tripoint_abs_sm &map_origin ) const;
};
