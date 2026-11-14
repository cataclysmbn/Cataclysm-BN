#pragma once

#include "point.h"

#include <string>
#include <vector>

namespace game_client
{

/// A single composed terminal cell independent of any display protocol.
struct screen_cell {
    std::string text = " ";
    int foreground = 7;
    int background = 0;
};

/// A complete composed terminal screen shared by display clients.
struct screen_snapshot {
    int width = 0;
    int height = 0;
    point cursor;
    bool cursor_visible = false;
    std::vector<screen_cell> cells;
    std::string text;
};

} // namespace game_client
