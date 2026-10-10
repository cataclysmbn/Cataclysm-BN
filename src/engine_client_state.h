#pragma once

#include <compare>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace engine_client
{

/// Absolute map square. `dim` is the game dimension_id; "" is the primary dimension.
struct position {
    std::string dim = {};
    int x = 0;
    int y = 0;
    int z = 0;
    auto operator<=>( const position & ) const = default; // *NOPAD*
};
struct bounds {
    position min;
    position max;
    auto operator<=>( const bounds & ) const = default; // *NOPAD*
};
/// Appearance from game data, so text clients need no tileset.
struct look {
    std::string kind = {};
    std::optional<std::string> id = std::nullopt;
    std::string glyph = {};
    std::string color = {};
    auto operator<=>( const look & ) const = default; // *NOPAD*
};
struct field_entry {
    look appearance;
    int intensity = 0;
    auto operator<=>( const field_entry & ) const = default; // *NOPAD*
};
enum class knowledge { remembered, visible, sensed };
/// The two layers the avatar memorized for a cell.
struct memory_layers {
    look terrain;
    std::optional<look> overlay = std::nullopt;
    auto operator<=>( const memory_layers & ) const = default; // *NOPAD*
};
struct cell {
    position at;
    knowledge known = knowledge::visible;
    std::optional<look> terrain = std::nullopt;
    std::optional<look> furniture = std::nullopt;
    std::vector<field_entry> fields = {};
    std::vector<look> traps = {};
    std::vector<look> items = {};
    std::optional<look> vehicle = std::nullopt;
    std::optional<int> light = std::nullopt;
    std::optional<memory_layers> memory = std::nullopt;
    auto operator<=>( const cell & ) const = default; // *NOPAD*
};
/// `sensed` entities carry only id, position and sense.
struct entity {
    std::string id = {};
    position at;
    knowledge known = knowledge::visible;
    std::optional<look> appearance = std::nullopt;
    std::optional<std::string> name = std::nullopt;
    std::vector<std::string> statuses = {};
    std::optional<std::string> sense = std::nullopt;
    auto operator<=>( const entity & ) const = default; // *NOPAD*
};
struct avatar_stat {
    std::string id = {};
    std::string label = {};
    std::string value = {};
    auto operator<=>( const avatar_stat & ) const = default; // *NOPAD*
};
struct inventory_entry {
    look appearance;
    std::string name = {};
    std::optional<std::uint64_t> count = std::nullopt;
    auto operator<=>( const inventory_entry & ) const = default; // *NOPAD*
};
struct avatar_value {
    std::string id = {};
    position at;
    std::string name = {};
    std::vector<avatar_stat> stats = {};
    /// Carried items, in the native inventory order. Items have no instance IDs.
    std::vector<inventory_entry> inventory = {};
    auto operator<=>( const avatar_value & ) const = default; // *NOPAD*
};
struct environment_value {
    std::string turn = {};
    std::string time = {};
    std::string weather = {};
    auto operator<=>( const environment_value & ) const = default; // *NOPAD*
};

/// What the avatar knows. Cells are sparse: absent inside the coverage means unknown.
struct world_state {
    std::optional<bounds> coverage = std::nullopt;
    std::map<position, cell> cells = {};
    std::map<std::string, entity> entities = {};
    std::optional<avatar_value> avatar = std::nullopt;
    std::optional<environment_value> environment = std::nullopt;
    /// The native auto-move route a map click planned and not yet confirmed, nearest square first.
    std::vector<position> route = {};
    auto operator==( const world_state & ) const -> bool = default;
};

} // namespace engine_client
