#include "cache_validation.h"

#include "game_constants.h"
#include "mapbuffer.h"

#include <ranges>

namespace test_cache_validation {

auto rebuild_defensively(map& here, const int zlev) -> void {
    // Mark every bubble-cache dependency dirty.  The normal mutation path is
    // deliberately not bypassed; this is the broad comparison oracle for
    // tests that have already performed a mutation.
    const auto begin = here.get_abs_sub();
    const auto end = begin + point_rel_sm(here.getmapsize(), here.getmapsize());
    for (const int z : std::views::iota(-OVERMAP_DEPTH, OVERMAP_HEIGHT + 1)) {
        here.get_mapbuffer().mark_submap_caches_dirty({
            .begin = begin,
            .end = end,
            .zlev = z,
            .transparency = true,
            .floor = true,
            .absorption = true,
            .pathfinding = true,
        });
        here.set_transparency_cache_dirty(z);
        here.set_floor_cache_dirty(z);
        here.set_outside_cache_dirty(z);
        here.set_absorption_cache_dirty(z);
        here.set_seen_cache_dirty(z);
        here.set_pathfinding_cache_dirty(z);
        here.invalidate_map_cache(z);
    }

    here.reset_vehicle_cache();
    here.build_map_cache(zlev);
    here.update_visibility_cache(zlev);
}

} // namespace test_cache_validation
