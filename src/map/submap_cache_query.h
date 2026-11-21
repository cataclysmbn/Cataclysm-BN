#pragma once

#include "coordinates.h"
#include "game_constants.h"
#include "map/map.h"
#include "map/submap.h"
#include "point.h"

#include <algorithm>
#include <vector>

/// Inclusive tile rectangle on one z-level and the submap position cache to read.
struct submap_cache_rect {
    tripoint_bub_ms min;
    tripoint_bub_ms max;
    /// `&submap::field_cache` or `&submap::trap_cache`.
    /// The cache must list every tile that holds its data. Stale entries are allowed.
    std::vector<point_sm_ms> submap::* cache;
};

/// Returns true when a tile listed in `rect.cache` lies in the rectangle and `pred( sm, local )`
/// returns true. Reads one submap pointer per submap and only the listed tiles.
/// Use it to skip a full tile scan that can find nothing.
template <typename Pred>
auto any_cached_tile_in(const map& here, const submap_cache_rect& rect, Pred pred) -> bool {
    const auto z = rect.min.z();
    const auto sm_min =
        point(divide_round_to_minus_infinity(rect.min.x(), SEEX),
              divide_round_to_minus_infinity(rect.min.y(), SEEY));
    const auto sm_max =
        point(divide_round_to_minus_infinity(rect.max.x(), SEEX),
              divide_round_to_minus_infinity(rect.max.y(), SEEY));
    for (auto smx = sm_min.x; smx <= sm_max.x; ++smx) {
        for (auto smy = sm_min.y; smy <= sm_max.y; ++smy) {
            const auto origin = point(smx * SEEX, smy * SEEY);
            const submap* sm = here.get_submap_at(tripoint_bub_ms(origin.x, origin.y, z));
            if (sm == nullptr) { continue; }
            const auto in_rect = [&](const point_sm_ms& local) {
                const auto x = origin.x + local.x();
                const auto y = origin.y + local.y();
                return x >= rect.min.x() && x <= rect.max.x() && y >= rect.min.y()
                    && y <= rect.max.y();
            };
            if (std::ranges::any_of(sm->*rect.cache, [&](const point_sm_ms& local) {
                    return in_rect(local) && pred(*sm, local);
                })) {
                return true;
            }
        }
    }
    return false;
}
