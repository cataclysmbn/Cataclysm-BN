#pragma once

#include "coordinates.h"

#include <expected>
#include <nlohmann/json.hpp>
#include <string>

namespace editor {

/// The mapgen entry of the `size` overmap squares from `at` east and south that generates the
/// live map's terrain, furniture, traps and fields there, each kind of square under a symbol of
/// its own; the error says why not.
auto export_region(const tripoint_abs_omt& at, const point_rel_omt& size)
    -> std::expected<nlohmann::ordered_json, std::string>;

} // namespace editor
