#pragma once

#include <vector>

#include "point.h"

/// Options for rendering zone previews and overlays.
struct zone_draw_options {
    tripoint start;
    tripoint end;
    tripoint offset;
    std::vector<tripoint> points;
};
