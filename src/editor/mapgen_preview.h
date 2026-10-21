#pragma once

#include "coordinates.h"

#include <cstddef>
#include <expected>
#include <memory>
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

struct preview_options {
    /// The overmap square of the entry's first square; the others follow east and south of it.
    tripoint_abs_omt at;
    unsigned int seed = 1;
    /// The direction an overmap terrain that rotates is generated for.
    std::string direction = "north";
};

/// What one square of a preview holds.
struct preview_square {
    std::string ter;
    std::string furn;
    std::string trap;
    std::size_t items = 0;
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

/// Generates the entry in `text` as the game generates its overmap squares, into a scratch buffer:
/// the world and its overmap are left as they were. The same options give the same squares.
auto preview_mapgen(std::string_view text, const preview_options& options)
    -> std::expected<mapgen_preview, std::string>;

} // namespace editor
