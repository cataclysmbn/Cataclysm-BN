#pragma once

#include <vector>

#include "point.h"

/// Options for rendering zone previews and overlays.
struct zone_draw_options {
    tripoint_bub_ms start;
    tripoint_bub_ms end;
    tripoint_rel_ms offset;
    std::vector<tripoint_bub_ms> points;
};
