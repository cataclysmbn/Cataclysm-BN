#pragma once

#if defined(CATA_MCP)

#    include "coordinates.h"
#    include "overmap/omdata.h"
#    include "overmap/overmapbuffer.h"
#    include "type_id.h"

#    include <array>
#    include <cstddef>

namespace overmap_helpers {

/// Sets every square the window can show, so that the picture comes from the code alone: terrain
/// that cycles through the kinds the cell logic tells apart, every fifth square unseen and some
/// explored.
inline void lay_terrain(const tripoint_abs_omt& center) {
    constexpr auto kinds = std::array{
        "forest", "field", "road_ns", "road_ew", "forest_trail_ns", "lake_surface", "forest_thick"};
    for (auto y = -25; y <= 25; ++y) {
        for (auto x = -40; x <= 40; ++x) {
            const auto at = center + point(x, y);
            ACTIVE_OVERMAP_BUFFER
                .ter_set(at, oter_id(kinds[static_cast<std::size_t>((x * 7 + y * 3 + 1000) % 7)]));
            ACTIVE_OVERMAP_BUFFER.set_seen(at, (x + 2 * y + 1001) % 5 != 0);
            if ((x * 3 + y + 1000) % 11 == 0 && ACTIVE_OVERMAP_BUFFER.seen(at)) {
                ACTIVE_OVERMAP_BUFFER.toggle_explored(at);
            }
        }
    }
}

} // namespace overmap_helpers

#endif
