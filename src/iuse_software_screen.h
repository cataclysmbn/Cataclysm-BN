#pragma once

#include "cursesdef.h"
#include "engine_client_component.h"

#include <string>
#include <vector>

namespace iuse_software
{

struct canvas_screen_options {
    std::string title;
    /// The windows the game draws into: the first frames the others, which are drawn over it in order.
    std::vector<catacurses::window> layers;
    /// Nonzero where the game moves on its own every this many milliseconds.
    int refresh_ms = 0;
};

/// A software game's drawn windows as the screen it publishes: the games keep their board in the window they
/// draw, so the window as a whole is the one canvas component.
auto canvas_screen( const canvas_screen_options &opts ) -> engine_client::screen_view;

} // namespace iuse_software
