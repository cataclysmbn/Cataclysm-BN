#pragma once

#include "debug.h"
#include "map.h"

namespace test_cache_validation {

auto rebuild_defensively(map& here, int zlev) -> void;

template <typename Query>
auto matches_after_defensive_rebuild(map& here, const int zlev, Query query) -> bool {
    const auto selective_result = query();
    rebuild_defensively(here, zlev);
    const bool matches = selective_result == query();
    if (!matches) {
        const auto origin = here.get_abs_sub();
        debugmsg("defensive cache validation mismatch: map origin (%d,%d), z-level %d", origin.x(),
                 origin.y(), zlev);
    }
    return matches;
}

} // namespace test_cache_validation
