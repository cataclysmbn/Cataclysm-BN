#pragma once

#include "editor/document.h"
#include "point.h"

#include <cstddef>
#include <map>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <vector>

namespace editor {

/// The overmap terrains an entry's `om_terrain` makes, row by row: one for a name or a list of
/// names, one per overmap square for rows of names; none for an entry of no overmap terrain.
auto entry_terrains(const nlohmann::ordered_json& value) -> std::vector<std::vector<std::string>>;

/// An overmap terrain a building or overmap special places, by name without a direction.
struct placed_terrain {
    std::string name;
    /// Whether it is placed turned from how its mapgen draws it.
    bool turned = false;
};

/// The overmap terrains one building or overmap special places, by square.
using special_layout = std::map<tripoint, placed_terrain>;

/// Where the fixed buildings and overmap specials place overmap terrains.
struct level_index {
    std::vector<special_layout> layouts;
};

/// The layouts of every fixed building and overmap special of the game.
auto index_levels() -> level_index;

/// The overmap terrain `dz` levels above `om_terrain` where a layout stacks one other than itself.
auto terrain_above(const level_index& index, const std::string& om_terrain, int dz)
    -> std::optional<std::string>;

/// The level `om_terrain` stands on in the first layout that holds it.
auto terrain_level(const level_index& index, const std::string& om_terrain) -> std::optional<int>;

/// An overmap square of an entry: which entry and where its squares start in the rows.
struct entry_square {
    std::size_t entry = 0;
    point origin;
};

/// The overmap square of the entries, other than `skipped`, that makes `om_terrain`.
auto square_making(
    const std::vector<document_object>& entries, const std::string& om_terrain,
    std::optional<std::size_t> skipped = std::nullopt) -> std::optional<entry_square>;

/// An overmap square of one entry with the square `dz` levels above it in another.
struct stacked_square {
    point origin;
    entry_square other;
};

/// The overmap squares of entry `entry` whose square `dz` levels above some entry of `entries`
/// makes.
auto stacked_squares(
    const level_index& index, const std::vector<document_object>& entries, std::size_t entry,
    int dz) -> std::vector<stacked_square>;

} // namespace editor
