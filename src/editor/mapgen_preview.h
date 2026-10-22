#pragma once

#include "coordinates.h"
#include "type_id.h"

#include <cstddef>
#include <expected>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

class mapgen_function;

namespace editor {

/// The mapgens of one `"type": "mapgen"` entry, loaded as the game loads them but registered
/// nowhere.
struct detached_mapgens {
    /// How many overmap squares the entry covers.
    point_rel_omt grid;
    /// The overmap terrain and the mapgen of each overmap square, row by row.
    std::vector<std::string> om_terrains;
    std::vector<std::shared_ptr<mapgen_function>> functions;
};

/// Loads the entry in `text`; the error is what the loader threw. Nested and update mapgens load no
/// function.
auto load_detached_mapgens(std::string_view text) -> std::expected<detached_mapgens, std::string>;

/// Registers the entry in `text` as the mapgen of its overmap terrains, in place of the ones the
/// game loaded for them, so that the game generates it from then on; the error is what the loader
/// threw. Nested and update mapgens are not applied.
auto apply_mapgen(std::string_view text) -> std::expected<void, std::string>;

/// The overmap terrain an entry's `om_terrain` names: itself, or its turn to `direction` when it
/// rotates.
auto overmap_terrain(const std::string& name, const std::string& direction)
    -> std::optional<oter_id>;

/// A parameter a preview can pin, with every value it can take.
struct mapgen_parameter_choice {
    std::string name;
    std::vector<std::string> values;
};

/// The parameters of a function `load_detached_mapgens` loaded, by name.
auto parameters_of(const mapgen_function& function) -> std::vector<mapgen_parameter_choice>;

/// Fixes the named parameters of such a function to the given values.
auto pin_parameters(mapgen_function& function, const std::map<std::string, std::string>& pins)
    -> std::expected<void, std::string>;

struct preview_options {
    /// The overmap square of the entry's first square; the others follow east and south of it.
    tripoint_abs_omt at;
    unsigned int seed = 1;
    /// The direction an overmap terrain that rotates is generated for.
    std::string direction = "north";
    /// Parameters fixed to a value, by name; the others are picked as the game picks them.
    std::map<std::string, std::string> pins = {};
};

/// What one square of a preview holds.
struct preview_square {
    std::string ter;
    std::string furn;
    std::string trap;
    /// The ids of the items on it, ordered.
    std::vector<std::string> items;
    /// Whether a vehicle covers it.
    bool vehicle = false;
};

struct mapgen_preview {
    point_rel_ms size;
    /// Row by row.
    std::vector<preview_square> squares;
    /// What the loader and the generator said.
    std::vector<std::string> messages;
};

struct loot_options {
    preview_options preview;
    /// The squares whose items are counted, both corners included.
    point_rel_ms from;
    point_rel_ms to;
    int runs = 100;
};

/// How often an item lies in the squares over the runs.
struct loot_share {
    std::string item;
    /// The runs it lies there in, out of all.
    double chance = 0.0;
    /// How many lie there on average.
    double mean = 0.0;
};

/// Previews the entry with seeds `seed` to `seed + runs - 1` and counts the items in the squares;
/// the most common first.
auto roll_loot(std::string_view text, const loot_options& options)
    -> std::expected<std::vector<loot_share>, std::string>;

/// Generates the entry in `text` as the game generates its overmap squares, into a scratch buffer:
/// the world and its overmap are left as they were. The same options give the same squares.
auto preview_mapgen(std::string_view text, const preview_options& options)
    -> std::expected<mapgen_preview, std::string>;

} // namespace editor
