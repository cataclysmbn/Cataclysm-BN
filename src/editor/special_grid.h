#pragma once

#include "coordinates.h"
#include "editor/mapgen_check.h"

#include <nlohmann/json.hpp>
#include <string>
#include <string_view>
#include <vector>

namespace editor {

/// An overmap square that a fixed overmap special or a building places.
struct special_square {
    tripoint_rel_omt at;
    /// The overmap terrain it places, as the entry names it.
    std::string overmap;
};

/// Whether the entry is an overmap special or a building, which place overmap terrains.
auto is_special(const nlohmann::ordered_json& value) -> bool;

/// The squares of a fixed special or a building, as it lists them; none for a mutable special.
auto special_squares(const nlohmann::ordered_json& value) -> std::vector<special_square>;

/// Loads a special or building entry with the game's loader, finalizes and checks it as the game
/// does after loading data, and registers nothing; what the loader reported, in order.
auto check_special(std::string_view text) -> std::vector<loader_message>;

} // namespace editor
