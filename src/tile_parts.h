#pragma once

#include "coordinates.h"

#include <nlohmann/json_fwd.hpp>
#include <string>
#include <vector>

namespace engine_client
{

/// One fact the look view prints of a square, in the order it prints them.
struct tile_part {
    /// `visibility`, `location`, `move_cost`, `terrain`, `light`, `furniture`, `description`, `concealment`,
    /// `cover`, `features`, `sign`, `below`, `field`, `trap`, `creature`, `vehicle`, `vehicle_part`, `item`,
    /// `inscription` or `sound`.
    std::string kind = {};
    /// The fact, localized, with color tags.
    std::string text = {};
    /// What more the view says of it when it has room, such as a creature's description; empty for none.
    std::string detail = {};
};

/// What the native look view prints of bubble square `at`, composed from the map and the creatures on it.
auto tile_parts( const tripoint_bub_ms &at ) -> std::vector<tile_part>;

/// The wire value of a part: `detail` only when it has one.
auto to_json( const tile_part &part ) -> nlohmann::ordered_json;

} // namespace engine_client
