#pragma once

#include "coordinates.h"

#include <expected>
#include <nlohmann/json.hpp>
#include <string>

namespace editor {

/// The mapgen entry of the `size` overmap squares from `at` east and south that generates the
/// live map's terrain, furniture, traps and fields there, each kind of square under a symbol of
/// its own, or the symbol of `palette` (none when empty) that maps that kind alone; the error says
/// why not.
auto export_region(
    const tripoint_abs_omt& at, const point_rel_omt& size, const std::string& palette)
    -> std::expected<nlohmann::ordered_json, std::string>;

/// The nested chunk `id` of the live map's squares from `corner`, `size` squares east and south,
/// made as `export_region` makes an overmap square's mapgen; the error says why not.
auto export_chunk(
    const tripoint_abs_ms& corner, const point_rel_ms& size, const std::string& palette,
    const std::string& id) -> std::expected<nlohmann::ordered_json, std::string>;

} // namespace editor
