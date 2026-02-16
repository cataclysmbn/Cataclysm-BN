#pragma once
#if !(defined(TILES) || defined(_WIN32))

#include "hsv_color.h"
#include "color_loader.h"

namespace ncurses
{

auto color_to_RGB( const nc_color &color ) -> RGBColor;

} // namespace ncurses

#endif