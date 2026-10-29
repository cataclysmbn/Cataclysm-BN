#pragma once

#include "coordinates.h"
#include "editor/mapgen_legend.h"

#include <expected>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

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

/// `exported`, the object of an export made without a palette, with each kind of square that a
/// one-character symbol of `legend` maps alone (terrain, at most furniture and a trap, each a plain
/// id) drawn with that symbol, so an export keeps the symbols of the entry open.
auto keep_symbols(nlohmann::ordered_json exported, const std::vector<legend_symbol>& legend)
    -> nlohmann::ordered_json;

} // namespace editor
