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

/// Where a special joins an overmap connection, such as a road, to the squares beside it.
struct special_connection {
    tripoint_rel_omt at;
    std::string connection;
};

/// Whether the entry is an overmap special or a building, which place overmap terrains.
auto is_special(const nlohmann::ordered_json& value) -> bool;

/// Whether the entry places squares it lists, rather than growing a mutable special.
auto is_fixed(const nlohmann::ordered_json& value) -> bool;

/// The levels a fixed special or building places squares or connections on, lowest first.
auto special_levels(const nlohmann::ordered_json& value) -> std::vector<int>;

/// The level of `levels` the editor shows first: the lowest from the ground up, else the highest.
auto ground_level(const std::vector<int>& levels) -> int;

/// The squares of a fixed special or a building, as it lists them; none for a mutable special.
auto special_squares(const nlohmann::ordered_json& value) -> std::vector<special_square>;

/// The RFC 6902 patch of a fixed special or building that makes its square `at` place `overmap`:
/// it sets the square's terrain, adds the square, or removes it when `overmap` is empty; empty when
/// nothing changes.
auto square_patch(
    const nlohmann::ordered_json& value, const tripoint_rel_omt& at, const std::string& overmap)
    -> nlohmann::ordered_json;

/// The connections a special or building lists, with a point and a connection id.
auto special_connections(const nlohmann::ordered_json& value) -> std::vector<special_connection>;

/// Whether the special lists a connection at `at`, named or not.
auto has_connection(const nlohmann::ordered_json& value, const tripoint_rel_omt& at) -> bool;

/// The RFC 6902 patch that removes the special's connection at `at`, or else adds one of
/// `connection` there, joined from a square of the special beside it, the first of north, east,
/// south and west.
auto connection_patch(
    const nlohmann::ordered_json& value, const tripoint_rel_omt& at, const std::string& connection)
    -> nlohmann::ordered_json;

/// Loads a special or building entry with the game's loader, finalizes and checks it as the game
/// does after loading data, and registers nothing; what the loader reported, in order.
auto check_special(std::string_view text) -> std::vector<loader_message>;

} // namespace editor
