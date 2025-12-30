#pragma once

#include <functional>
#include <memory>

#include "point.h"

enum class mouse_button : int {
    none = 0,
    left = 1,
    right = 2,
    middle = 3,
    scroll_up = 4,
    scroll_down = 5,
    back = 6,
    forward = 7,
};

enum class mouse_event_type : int {
    click_down,
    click_up,
    hover_enter,
    hover_exit,
    hover_move,
};

struct mouse_event {
    point pos;
    point local_pos;
    mouse_button button = mouse_button::none;
    mouse_event_type type = mouse_event_type::click_down;
    bool shift_held = false;
    bool ctrl_held = false;
    bool alt_held = false;
    bool left_held = false;
    bool right_held = false;
    bool middle_held = false;
};

using mouse_callback_t = std::function<void( const mouse_event & )>;
using mouse_callback_ptr = std::shared_ptr<mouse_callback_t>;
