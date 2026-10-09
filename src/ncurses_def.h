#pragma once
#if !defined(TILES) || defined(CATA_CURSES_CLIENT)

#include "hsv_color.h"
#include "color_loader.h"

namespace game_client::curses
{

auto color_to_RGB_native( const nc_color &color ) -> RGBColor;

} // namespace game_client::curses

#endif
