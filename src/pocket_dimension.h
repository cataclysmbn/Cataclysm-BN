#pragma once

#include <map>
#include <optional>
#include <string>

#include "boundary_section.h"
#include "coordinates.h"
#include "layer.h"
#include "point.h"
#include "safe_reference.h"
#include "type_id.h"

class item;
class JsonIn;
class JsonOut;
class overmap_special;

/**
 * Unique identifier for pocket dimensions.
 *
 * This is separate from boundary_section_id because pocket dimensions
 * have additional metadata beyond just their bounds (owner, mapgen, etc.)
 */
struct pocket_dimension_id {
    int value = -1;

    bool is_valid() const {
        return value >= 0;
    }

    bool operator==( const pocket_dimension_id &other ) const {
        return value == other.value;
    }
    bool operator!=( const pocket_dimension_id &other ) const {
        return value != other.value;
    }
    bool operator<( const pocket_dimension_id &other ) const {
        return value < other.value;
    }

    void serialize( JsonOut &json ) const;
    void deserialize( JsonIn &jsin );
};

/**
 * Ownership via item reference.
 *
 * Pocket dimensions are typically owned by items (bags of holding, etc.)
 * When the item is destroyed, the pocket dimension collapses.
 */
struct pocket_owner_item {
    safe_reference<item> ref;

    bool is_valid() const;

    void serialize( JsonOut &json ) const;
    void deserialize( JsonIn &jsin );
};

/**
 * Represents a single pocket dimension instance.
 *
 * A pocket dimension is a bounded region in the POCKET_DIMENSION layer
 * that is owned by an item. The actual bounds and mapgen configuration
 * are stored in the associated boundary_section.
 */
class pocket_dimension
{
    public:
        pocket_dimension_id id;
        boundary_section_id section_id;  // Associated boundary section (stores bounds, mapgen, etc.)
        pocket_owner_item owner;
        tripoint_abs_omt return_location;  // Where to return when exiting

        // Check if the owner is still valid
        bool is_owner_valid() const;

        // Get the full bounds including border (via boundary_section_manager)
        const boundary_bounds *get_bounds() const;

        // Get the entry point within the pocket dimension
        // Uses entry_offset if set, otherwise defaults to center of interior at ground level
        tripoint_abs_ms get_entry_point() const;

        // Entry offset from interior origin (in OMT units for omt_offset, tile units for local_offset)
        // These are optional overrides - if not set, defaults to center of interior
        std::optional<tripoint> entry_omt_offset;   // Offset in OMT coordinates within interior
        std::optional<point> entry_local_offset;    // Offset in tiles within the target OMT

        ter_str_id border_terrain;

        // Get usable interior bounds (excludes border)
        boundary_bounds get_interior() const;

        // Check if terrain has been generated
        bool is_generated() const;

        void serialize( JsonOut &json ) const;
        void deserialize( JsonIn &jsin );
};

/**
 * Singleton manager for all pocket dimensions.
 *
 * Handles creation, destruction, and access to pocket dimensions.
 * Delegates spatial queries and map generation to boundary_section_manager.
 */
class pocket_dimension_manager
{
    public:
        static pocket_dimension_manager &instance();

        // Create a new pocket dimension from an overmap_special.
        // The size is calculated from the special's terrain layout.
        // Any tiles not covered by the special's terrain are filled with border_terrain
        // (defaults to t_pd_border if not specified).
        // If special_id is not provided, defaults to "Cave".
        // Returns invalid ID on failure.
        pocket_dimension_id create( item &owner_item,
                                    const overmap_special_id &special_id = overmap_special_id( "Cave" ),
                                    const ter_str_id &border_terrain = ter_str_id() );

        // Get pocket dimension by ID (nullptr if not found)
        pocket_dimension *get( pocket_dimension_id id );
        const pocket_dimension *get( pocket_dimension_id id ) const;

        // Destroy a pocket dimension and clean up its submaps
        void destroy( pocket_dimension_id id );

        // Teleport player (and companions) into a pocket dimension
        // Entry position is determined by:
        // 1. entry_omt_offset and entry_local_offset in the pocket_dimension (from item vars)
        // 2. Falls back to center of interior if not specified
        // 3. If the target tile is not safe/passable, searches for the closest safe tile
        void enter( pocket_dimension_id id, const tripoint_abs_omt &return_loc );

        // Exit the current pocket dimension and return to the real world
        void exit_current();

        // Query current state
        bool player_in_pocket_dimension() const;
        std::optional<pocket_dimension_id> current_pocket_id() const;

        // Check if a position is in any pocket dimension's border zone
        // Delegates to boundary_section_manager
        bool is_in_pocket_border( const tripoint_abs_ms &pos ) const;
        bool is_in_pocket_border( const tripoint_abs_omt &pos ) const;

        // Get the pocket dimension containing this position (if any)
        pocket_dimension *get_at_position( const tripoint_abs_omt &pos );
        pocket_dimension *get_at_position( const tripoint_abs_ms &pos );

        // Process all pocket dimensions - destroy orphaned ones, handle player stranding
        void process();

        // Clear all pocket dimensions (for new game)
        void clear();

        // Serialization
        void serialize( JsonOut &json ) const;
        void deserialize( JsonIn &jsin );

    private:
        pocket_dimension_manager() = default;

        std::map<pocket_dimension_id, pocket_dimension> dimensions;
        int next_id = 1;
        std::optional<pocket_dimension_id> current_dimension;
};

// Legacy compatibility - use layer.h functions instead
// These are kept for existing code that hasn't been updated yet

// Z-level where pocket dimensions start
// Prefer using get_layer_base_z(world_layer::POCKET_DIMENSION)
constexpr int POCKET_DIMENSION_Z_BASE = get_layer_base_z( world_layer::POCKET_DIMENSION );

// Border width in OMT units
// Prefer using boundary_bounds::DEFAULT_BORDER_WIDTH_OMT
constexpr int POCKET_BORDER_WIDTH_OMT = boundary_bounds::DEFAULT_BORDER_WIDTH_OMT;

// Helper to check if a Z-level is in pocket dimension range
// Prefer using is_pocket_dimension_z() from layer.h or get_layer() == world_layer::POCKET_DIMENSION
// Note: This is now defined in layer.h, so we just use that version
