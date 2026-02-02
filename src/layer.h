#pragma once

#include "game_constants.h"
#include "type_id.h"

/**
 * Layer System for Multi-Dimensional Map Support
 *
 * The layer system provides a way to have multiple "dimensions" coexisting in the game,
 * each offset by 100 z-levels from each other. Each layer has an internal z-range of
 * -10 to +10 (matching OVERMAP_DEPTH and OVERMAP_HEIGHT).
 *
 * Key concepts:
 * - Each layer is identified by an enum value
 * - Layer z-offset = layer_index * LAYER_Z_OFFSET (100)
 * - Layers can be bounded (like pocket dimensions) or unbounded (like overworld)
 * - Pathfinding and raycasting cannot cross between layers
 */

// Z-level offset between layers (each layer gets 100 z-levels of "space")
// Actual usable range within each layer is -LAYER_Z_MAX to +LAYER_Z_MAX
static constexpr int LAYER_Z_OFFSET = 100;

// Maximum z-level within a layer (matches OVERMAP_HEIGHT/OVERMAP_DEPTH)
static constexpr int LAYER_Z_MAX = OVERMAP_HEIGHT;  // 10

// Minimum z-level within a layer (negative of max)
static constexpr int LAYER_Z_MIN = -OVERMAP_DEPTH;  // -10

// Total number of usable z-levels per layer
static constexpr int LAYER_Z_RANGE = LAYER_Z_MAX - LAYER_Z_MIN + 1;  // 21

/**
 * Enumeration of all map layer types.
 *
 * Each layer type has a base z-offset calculated as:
 *   base_z = static_cast<int>(layer_type) * LAYER_Z_OFFSET
 *
 * The OVERWORLD layer is at index 0, so its base_z is 0, preserving
 * full backward compatibility with existing code.
 */
enum class world_layer : int {
    // The normal game world - z-levels -10 to +10 (base_z = 0)
    OVERWORLD = 0,

    // Pocket dimensions - z-levels 90 to 110 (base_z = 100)
    POCKET_DIMENSION = 1,

    // Future expansion examples:
    // SPIRIT_REALM = 2,      // z-levels 190 to 210
    // NETHER = 3,            // z-levels 290 to 310
    // etc.

    // Sentinel value for iteration/bounds checking
    NUM_LAYERS
};

/**
 * Get the base z-level for a given layer type.
 *
 * The base z-level is the z-value that corresponds to "z=0" within that layer.
 * For OVERWORLD this is 0. For POCKET_DIMENSION this is 100.
 */
constexpr int get_layer_base_z( world_layer layer )
{
    return static_cast<int>( layer ) * LAYER_Z_OFFSET;
}

/**
 * Determine which layer a given absolute z-level belongs to.
 *
 * Returns the layer type, or OVERWORLD for z-levels that don't clearly
 * fall within a defined layer (should not happen in normal gameplay).
 */
world_layer get_layer( int z );

/**
 * Normalize an absolute z-level to the layer-relative z-level.
 *
 * This converts an absolute z-level (like 105) to the equivalent
 * z-level within the layer (like 5).
 *
 * Formula: ((z + LAYER_Z_MAX) % LAYER_Z_OFFSET) - LAYER_Z_MAX
 *
 * Examples:
 *   normalize_z(0)   -> 0   (overworld ground level)
 *   normalize_z(5)   -> 5   (overworld 5 levels up)
 *   normalize_z(100) -> 0   (pocket dimension ground level)
 *   normalize_z(105) -> 5   (pocket dimension 5 levels up)
 *   normalize_z(95)  -> -5  (pocket dimension 5 levels down)
 */
constexpr int normalize_z( int z )
{
    // Add LAYER_Z_MAX to shift range, mod to get position in layer, subtract to normalize
    return ( ( z + LAYER_Z_MAX ) % LAYER_Z_OFFSET ) - LAYER_Z_MAX;
}

/**
 * Convert a normalized (layer-relative) z-level back to absolute z-level.
 *
 * Examples:
 *   denormalize_z(0, OVERWORLD) -> 0
 *   denormalize_z(5, OVERWORLD) -> 5
 *   denormalize_z(0, POCKET_DIMENSION) -> 100
 *   denormalize_z(5, POCKET_DIMENSION) -> 105
 */
constexpr int denormalize_z( int normalized_z, world_layer layer )
{
    return normalized_z + get_layer_base_z( layer );
}

/**
 * Check if a layer type is bounded (has spatial boundaries like pocket dimensions)
 * vs unbounded (infinite like the overworld).
 *
 * Bounded layers have discrete regions (boundary sections) that entities cannot
 * path or see through the edges of.
 */
constexpr bool is_bounded_layer( world_layer layer )
{
    switch( layer ) {
        case world_layer::OVERWORLD:
            return false;
        case world_layer::POCKET_DIMENSION:
            return true;
        default:
            return false;
    }
}

oter_id get_default_terrain( world_layer layer, int relative_z = 0 );

/**
 * Check if an absolute z-level falls within any valid layer's z-range.
 *
 * A valid z-level is one that is within the usable range (-10 to +10)
 * of any defined layer.
 */
bool is_valid_layer_z( int z );

/**
 * Check if an absolute z-level is within the overworld layer.
 * This is the common case and is used for backward compatibility.
 */
constexpr bool is_overworld_z( int z )
{
    return z >= LAYER_Z_MIN && z <= LAYER_Z_MAX;
}

/**
 * Check if an absolute z-level is within the pocket dimension layer.
 * Replaces the old is_pocket_dimension_z() function.
 */
constexpr bool is_pocket_dimension_z( int z )
{
    int base = get_layer_base_z( world_layer::POCKET_DIMENSION );
    return z >= base + LAYER_Z_MIN && z <= base + LAYER_Z_MAX;
}

/**
 * Get the minimum valid z-level for a given layer.
 */
constexpr int get_layer_min_z( world_layer layer )
{
    return get_layer_base_z( layer ) + LAYER_Z_MIN;
}

/**
 * Get the maximum valid z-level for a given layer.
 */
constexpr int get_layer_max_z( world_layer layer )
{
    return get_layer_base_z( layer ) + LAYER_Z_MAX;
}
