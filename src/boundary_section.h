#pragma once

#include <optional>
#include <string>

#include "coordinates.h"
#include "layer.h"
#include "point.h"
#include "type_id.h"
#include "map.h"

class JsonIn;
class JsonOut;
class overmap_special;

/**
 * Unique identifier for boundary sections.
 *
 * Each bounded region (like a pocket dimension) gets a unique ID
 * that persists across save/load cycles.
 */
struct boundary_section_id {
    int value = -1;

    bool is_valid() const {
        return value >= 0;
    }

    bool operator==( const boundary_section_id &other ) const {
        return value == other.value;
    }
    bool operator!=( const boundary_section_id &other ) const {
        return value != other.value;
    }
    bool operator<( const boundary_section_id &other ) const {
        return value < other.value;
    }

    void serialize( JsonOut &json ) const;
    void deserialize( JsonIn &jsin );
};

/**
 * Axis-aligned bounding box for boundary sections.
 *
 * Represents the full bounds of a bounded region including its border.
 * Coordinates are in absolute OMT (overmap terrain) units.
 *
 * The border is a zone around the edges where terrain is impassable
 * and pathfinding/raycasting is blocked.
 */
struct boundary_bounds {
    tripoint_abs_omt min;  // Minimum corner (inclusive)
    tripoint_abs_omt max;  // Maximum corner (exclusive)

    // Default border width in OMT units (2 OMT = 48 tiles of border on each side)
    static constexpr int DEFAULT_BORDER_WIDTH_OMT = 2;

    // Border width for this specific bounds (can be customized per section)
    int border_width_omt = DEFAULT_BORDER_WIDTH_OMT;

    // Check if a point is within the full bounds (including border)
    bool contains( const tripoint_abs_omt &p ) const;
    bool contains( const tripoint_abs_sm &p ) const;
    bool contains( const tripoint_abs_ms &p ) const;

    // Check if two bounds intersect
    bool intersects( const boundary_bounds &other ) const;

    // Get the usable interior bounds (excluding border)
    boundary_bounds interior() const;

    // Check if a point is in the border zone (within border_width_omt of any edge)
    bool is_in_border( const tripoint_abs_omt &p ) const;
    bool is_in_border( const tripoint_abs_sm &p ) const;
    bool is_in_border( const tripoint_abs_ms &p ) const;

    // Get dimensions of the full bounds
    tripoint size() const;

    // Get dimensions of the interior (usable area)
    tripoint interior_size() const;

    void serialize( JsonOut &json ) const;
    void deserialize( JsonIn &jsin );
};

/**
 * Configuration for how a boundary section's terrain should be generated.
 *
 * Supports two modes:
 * 1. Legacy: comma-separated mapgen IDs applied in tile order
 * 2. Special: overmap_special with terrain layout and offset
 */
struct boundary_section_mapgen {
    // Legacy mode: raw mapgen IDs (comma-separated for tiled generation)
    std::string mapgen_id;

    // Special mode: use overmap_special for structured generation
    std::optional<overmap_special_id> special_id;
    tripoint special_offset;  // Offset to apply when placing special within bounds

    bool has_mapgen() const {
        return !mapgen_id.empty() || special_id.has_value();
    }

    bool uses_special() const {
        return special_id.has_value();
    }

    void serialize( JsonOut &json ) const;
    void deserialize( JsonIn &jsin );
};

/**
 * A boundary section represents a discrete bounded region within a bounded layer.
 *
 * Examples include individual pocket dimensions, spirit realm areas, etc.
 * Each section has its own bounds, and pathfinding/raycasting cannot cross
 * the section's boundary edges.
 */
struct boundary_section {
    boundary_section_id id;
    world_layer layer = world_layer::POCKET_DIMENSION;
    boundary_bounds bounds;
    boundary_section_mapgen mapgen;  // How to generate terrain for this section
    bool generated = false;          // Has the terrain been generated?

    // Check if a point is within this section (including border)
    bool contains( const tripoint_abs_omt &p ) const;
    bool contains( const tripoint_abs_ms &p ) const;

    // Check if a point is in this section's border zone
    bool is_in_border( const tripoint_abs_omt &p ) const;
    bool is_in_border( const tripoint_abs_ms &p ) const;

    // Get the interior bounds (usable area, excluding border)
    boundary_bounds get_interior() const;

    void serialize( JsonOut &json ) const;
    void deserialize( JsonIn &jsin );
};

/**
 * Singleton manager for all boundary sections across all bounded layers.
 *
 * This provides a central registry for querying which boundary section
 * contains a given position, which is needed for:
 * - Pathfinding cache lookup (different caches per section)
 * - Border terrain rendering
 * - Preventing movement/LOS across section boundaries
 * - Map generation for bounded sections
 */
class boundary_section_manager
{
    public:
        static boundary_section_manager &instance();

        // Register a new boundary section with optional mapgen configuration
        // Returns the assigned section ID
        boundary_section_id register_section( world_layer layer, const boundary_bounds &bounds,
                                              const boundary_section_mapgen &mapgen = {} );

        // Unregister a boundary section (e.g., when pocket dimension is destroyed)
        void unregister_section( boundary_section_id id );

        // Get a section by ID (nullptr if not found)
        boundary_section *get( boundary_section_id id );
        const boundary_section *get( boundary_section_id id ) const;

        // Get the section containing a position (nullptr if not in any section)
        boundary_section *get_at( const tripoint_abs_omt &pos );
        boundary_section *get_at( const tripoint_abs_ms &pos );
        const boundary_section *get_at( const tripoint_abs_omt &pos ) const;
        const boundary_section *get_at( const tripoint_abs_ms &pos ) const;

        // Check if a position is in any section's border zone
        bool is_in_any_border( const tripoint_abs_omt &pos ) const;
        bool is_in_any_border( const tripoint_abs_ms &pos ) const;

        // Check if bounds would collide with any existing section
        bool would_collide( const boundary_bounds &bounds ) const;

        // Get the z-level range for a position within a boundary section
        // If the position is in a boundary section, returns (min_z, max_z) of that section
        // If not in any section, returns (0, 0) to indicate no constraint
        // The returned range is inclusive on both ends
        std::pair<int, int> get_z_bounds_at( const tripoint_abs_sm &pos ) const;

        // Get the overmap terrain ID for a position within a boundary section
        // Returns the appropriate oter_id for display on the overmap
        // Returns ot_null if position is not in any section
        oter_id get_oter_at( const tripoint_abs_omt &pos ) const;

        // Generate terrain for a boundary section (if not already generated)
        // Creates submaps with border terrain and applies mapgen to interior
        void generate_section_terrain( boundary_section_id id );

        // Remove all submaps associated with a boundary section
        void clear_section_submaps( boundary_section_id id );

        // Clear all sections (for new game)
        void clear();

        // Serialization
        void serialize( JsonOut &json ) const;
        void deserialize( JsonIn &jsin );

        // Calculate required interior size from an overmap_special's terrain layout
        // Returns (min_corner, max_corner_exclusive) in relative coordinates
        static std::pair<tripoint, tripoint> calculate_special_bounds( const overmap_special &special );

        // Find a non-colliding location for a new section in the given layer
        tripoint_abs_omt allocate_space( world_layer layer, const tripoint &size_omt );

    private:
        boundary_section_manager() = default;

        std::map<boundary_section_id, boundary_section> sections;
        int next_id = 1;

        // Internal generation helpers
        void create_base_submaps( const boundary_section &section );
        void apply_legacy_mapgen( boundary_section &section );
        void apply_special_mapgen( boundary_section &section, const overmap_special &special );
};
