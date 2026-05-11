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
enum base_color : short;
class window;
} // namespace catacurses

extern std::shared_ptr<cata_tiles> tilecontext;
extern std::shared_ptr<cata_tiles> overmap_tilecontext;
extern std::array<SDL_Color, color_loader<SDL_Color>::COLOR_NAMES_COUNT> windowsPalette;

// This function may refresh the screen, so it should not be used where tiles
// may be displayed. Actually, this is supposed to be called from init.cpp,
// and only from there.
void load_tileset();
void rescale_tileset( float size );
bool save_screenshot( const std::string &file_path );
void toggle_fullscreen_window();
extern void repoint_overmap_tilecontext();
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
auto get_sdl_display_buffer_size() -> point;
auto get_sdl_window_size() -> point;
auto get_sdl_font_size() -> point;
void clear_sdl_display_buffer();
void clear_sdl_display_buffer_before_redraw();
struct sdl_text_outline_options {
    std::string text;
    point pos_pixel = point_zero;
    catacurses::base_color text_color = {};
    catacurses::base_color outline_color = {};
    int outline_thickness = 1;
};
void draw_sdl_text_outlined( const sdl_text_outline_options &opts );

const SDL_Renderer_Ptr &get_sdl_renderer();
const SDL_Window_Ptr &get_sdl_window();

#endif // TILES


