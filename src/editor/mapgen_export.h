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

} // namespace editor
