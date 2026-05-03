#pragma once

#include <optional>

#include "coordinates.h"

class map;

namespace map_funcs
{

/**
 * Checks both the neighborhoods of from and to for climbable surfaces,
 * returns move cost of climbing from `from` to `to`.
 * Return value can depend on the orientation of the terrain.
 */
auto climbing_cost( const map &m, const tripoint_bub_ms &from,
                    const tripoint_bub_ms &to ) -> std::optional<int>;

void migo_nerve_cage_removal( map &m, const tripoint_bub_ms &p, bool spawn_damaged );

} // namespace map_funcs


