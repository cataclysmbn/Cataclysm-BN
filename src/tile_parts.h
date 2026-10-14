#pragma once

#include "color.h"
#include "coordinates.h"

#include <nlohmann/json_fwd.hpp>
#include <optional>
#include <string>
#include <vector>

class Creature;

namespace engine_client
{

/// A number a fact reports, such as a move cost or a count.
struct tile_value {
    std::string key = {};
    int value = 0;
};

/// A fact as the native views print it: the text in the color it is printed in. The native `print_*` code and the
/// parts of the wire both read the facts, so what they say cannot differ.
struct fact {
    /// See `tile_part::kind`.
    std::string kind = {};
    /// What introduces the text, such as "Wielding: ", printed before it.
    std::string label = {};
    std::string text = {};
    std::optional<nc_color> color = std::nullopt;
    std::string detail = {};
    /// What the fact is about, such as a terrain id; empty for none.
    std::string id = {};
    std::vector<tile_value> values = {};
};

/// One fact the look and target views tell of a square, in the order the native look view prints them.
struct tile_part {
    /// `visibility`, `location`, `move_cost`, `terrain`, `light`, `furniture`, `description`, `dispenser`,
    /// `concealment`, `cover`, `features`, `sign`, `below`, `field`, `trap`, `creature`, `hp`, `effects`,
    /// `attitude`, `senses`, `speed`, `readiness`, `difficulty`, `origin`, `object_id`, `rider`, `size`, `goal`,
    /// `wielding`, `wearing`, `traits`, `vehicle`, `vehicle_part`, `item`, `more_items`, `inscription`, `sound`,
    /// `turret` or `spell`.
    std::string kind = {};
    /// The fact, localized, with color tags.
    std::string text = {};
    /// What more the view says of it when it has room, such as a creature's description; empty for none.
    std::string detail = {};
    /// The identity of what the fact is about, such as a terrain id; empty for none.
    std::string id = {};
    std::vector<tile_value> values = {};
};

/// The part that tells `value`: its text colored, a bare whitespace edge trimmed.
auto to_part( const fact &value ) -> tile_part;
/// `to_part` of each fact.
auto to_parts( const std::vector<fact> &facts ) -> std::vector<tile_part>;

/// How the avatar perceives a creature it does not see: by its usual info, or only by these words.
struct sensed_creature {
    bool normal = false;
    std::vector<std::string> lines = {};
};
auto sense_of( const Creature &who ) -> sensed_creature;

/// What the native creature info prints of `who`, a fact per line; none for a creature that has no such info.
auto creature_facts( const Creature &who ) -> std::vector<fact>;

/// What the native look view prints of bubble square `at`, composed from the map and the creatures on it.
auto tile_parts( const tripoint_bub_ms &at ) -> std::vector<tile_part>;

/// The wire value of a part: `detail`, `id` and `values` only when it has them.
auto to_json( const tile_part &part ) -> nlohmann::ordered_json;

} // namespace engine_client
