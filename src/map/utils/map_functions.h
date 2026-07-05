#pragma once

#include "coordinates.h"

#include <optional>

class map;
class mapbuffer;

namespace map_funcs {

struct physical_clear_path_opts {
    const map& m;
    tripoint_bub_ms from;
    tripoint_bub_ms to;
    int range = -1;
    int cost_min = 1;
    int cost_max = 100;
    bool require_clear_path = true;
};

/// Checks whether a physical path is clear, including terrain floors and vehicle floors.
auto physical_clear_path(const physical_clear_path_opts& opts) -> bool;

/**
 * Checks both the neighborhoods of from and to for climbable surfaces,
 * returns move cost of climbing from `from` to `to`.
 * Return value can depend on the orientation of the terrain.
 */
auto climbing_cost(mapbuffer& buffer, const tripoint_abs_ms& from, const tripoint_abs_ms& to)
    -> std::optional<int>;
auto climbing_cost(const map& m, const tripoint_bub_ms& from, const tripoint_bub_ms& to)
    -> std::optional<int>;

auto migo_nerve_cage_removal(mapbuffer& buffer, const tripoint_abs_ms& p, bool spawn_damaged)
    -> void;
void migo_nerve_cage_removal(map& m, const tripoint_bub_ms& p, bool spawn_damaged);

} // namespace map_funcs
