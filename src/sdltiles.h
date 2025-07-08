#pragma once

#include <array>
#if defined(TILES)

#include <string>
#include <memory>

#include "color_loader.h"
#include "point.h"
#include "sdl_wrappers.h"

class cata_tiles;

namespace catacurses
{
class window;
} // namespace catacurses

extern std::unique_ptr<cata_tiles> tilecontext;
extern std::array<SDL_Color, color_loader<SDL_Color>::COLOR_NAMES_COUNT> windowsPalette;

// This function may refresh the screen, so it should not be used where tiles
// may be displayed. Actually, this is supposed to be called from init.cpp,
// and only from there.
void load_tileset();
void rescale_tileset( float size );
bool save_screenshot( const std::string &file_path );
void toggle_fullscreen_window();

struct window_dimensions {
    point scaled_font_size;
    point window_pos_cell;
    point window_size_cell;
    point window_pos_pixel;
    point window_size_pixel;
};
window_dimensions get_window_dimensions( const catacurses::window &win );
// Get dimensional info of an imaginary normal catacurses::window with the given
// position and size. Unlike real catacurses::window, size can be zero.
window_dimensions get_window_dimensions( point pos, point size );

const SDL_Renderer_Ptr &get_sdl_renderer();

#endif // TILES


