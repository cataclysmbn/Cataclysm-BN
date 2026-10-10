#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "coordinates.h"
#include "map_memory.h"

class map;
class vehicle;

namespace map_perception
{

struct orientation {
    int subtile = 0;
    int rotation = 0;
};

/// The native tile selection of a cell's furniture and trap, and of its terrain with the connection mask.
struct cell_orientations {
    uint8_t terrain_mask = 0;
    orientation terrain;
    orientation furniture;
    orientation trap;
};

/// What decides which squares the avatar sees, read once when many squares are looked at.
struct sight {
    bool blind = false;
    int clairvoyance = 0;
};
auto current_sight() -> sight;

/// Connections to the neighbours the avatar knows and, for unconnected furniture, the alignment to walls
/// or workbenches, for a currently visible cell.
auto visible_orientations( const map &here, const tripoint_bub_ms &p,
                           const sight &view ) -> cell_orientations;

/// The multitile key `tile_config.json` names a native subtile index by; empty for an unknown index.
auto multitile_key( int subtile ) -> std::string;

/// Pure SEWN connection mask conversion, shared by knowledge and presentation.
auto orient( uint8_t connections ) -> orientation;

/// Wall line glyph of a memorized terrain tile, `fallback` when the tile carries no connections.
auto remembered_wall_symbol( const memorized_terrain_tile &tile, int fallback ) -> int;

/// Terrain symbol of a visible cell; auto-wall terrain draws the lines it connects to known neighbours.
auto connected_wall_symbol( const map &here, const tripoint_bub_ms &p ) -> int;

/// Live randomized glyphs use a separate presentation-only generator.
auto cosmetic_variant() -> int;

/// Detailed ordinary sight from the completed visibility cache, without refreshing it.
auto visible_at( const map &here, const tripoint_bub_ms &p ) -> bool;

/// Every position ordinary sight shows, found by one pass over the completed visibility caches.
auto visible_cells( const map &here ) -> std::vector<tripoint_bub_ms>;

/// Read completed engine visibility, including exposed vehicle roofs and actor self-awareness.
auto detailed_at( const map &here, const tripoint_bub_ms &p ) -> bool;
auto vehicle_known( const map &here, const vehicle &veh ) -> bool;

struct acquisition_counts {
    /// Actual acquisition work, including checks performed on the unchanged fast path.
    /// cache_checks counts native build calls, not work inside those calls.
    size_t cache_checks = 0;
    size_t visibility_updates = 0;
    size_t submaps_checked = 0;
    size_t fields_checked = 0;
    size_t traps_checked = 0;
    size_t vehicle_points_checked = 0;
    size_t candidates = 0;
    size_t acquired = 0;
    size_t full_rescans = 0;
    size_t memory_writes = 0;
};

/// Native cache/knowledge producers invalidate acquisition, never acquire it.
auto invalidate_visibility( const map &here ) -> void;
auto invalidate_cell( const map &here, const tripoint_bub_ms &p ) -> void;
auto memory_changed( const tripoint_abs_ms &p ) -> void;
/// Clear/load/session replacement must not reuse the previous knowledge baseline.
auto reset() -> void;

/// Intermediate redraws of a simulation step may show transient states but never write knowledge.
/// Knowledge is acquired only at engine boundaries, so animation speed cannot change it.
class presentation_scope
{
    public:
        presentation_scope();
        ~presentation_scope();
        presentation_scope( const presentation_scope & ) = delete;
        auto operator=( const presentation_scope & ) -> presentation_scope & = delete; // *NOPAD*
};
auto presenting() -> bool;

/// Game-thread boundary only. Complete lighting/visibility, then acquire both memory channels.
/// Never call from a renderer or a passive observation.
auto acquire() -> acquisition_counts;

/// Explicit blind obstacle contact. Does not inspect neighbors or disclose the ground under it.
auto contact( const tripoint_bub_ms &p ) -> void;

} // namespace map_perception
