#pragma once

#include "editor/document.h"
#include "point.h"

#include <cstddef>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <vector>

namespace editor {

/// The overmap terrains an entry's `om_terrain` makes, row by row: one for a name or a list of
/// names, one per overmap square for rows of names; none for an entry of no overmap terrain.
auto entry_terrains(const nlohmann::ordered_json& value) -> std::vector<std::vector<std::string>>;

/// The overmap terrain `dz` levels above `om_terrain` in the first building or overmap special
/// that stacks it, by its name without a direction.
auto terrain_above(const std::string& om_terrain, int dz) -> std::optional<std::string>;

/// The level `om_terrain` stands on in the first building or overmap special that holds it.
auto terrain_level(const std::string& om_terrain) -> std::optional<int>;

/// An overmap square of an entry: which entry and where its squares start in the rows.
struct entry_square {
    std::size_t entry = 0;
    point origin;
};

/// The overmap square of the entries that makes `om_terrain`.
auto square_making(const std::vector<document_object>& entries, const std::string& om_terrain)
    -> std::optional<entry_square>;

/// An overmap square of one entry with the square `dz` levels above it in another.
struct stacked_square {
    point origin;
    entry_square other;
};

/// The overmap squares of entry `entry` whose square `dz` levels above some entry of `entries`
/// makes.
auto stacked_squares(const std::vector<document_object>& entries, std::size_t entry, int dz)
    -> std::vector<stacked_square>;

} // namespace editor
