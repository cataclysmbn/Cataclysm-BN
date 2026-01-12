#pragma once

#include "json.h"
#include "color.h"

#if defined(TILES)
#include "sdl_wrappers.h"
#else
#include <cstdint>
#endif

struct RGBColor {
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint8_t a;

#if defined(TILES)
    RGBColor() = default;
    RGBColor( const uint8_t r, const uint8_t g, const uint8_t b, const uint8_t a ) : r{r}, g{g}, b{b}, a{a} {}
    RGBColor( const SDL_Color &c ) : r( c.r ), g( c.g ), b( c.b ), a( c.a ) {}
    operator SDL_Color() const {
        return SDL_Color{ r, g, b, a };
    }
#endif
    void serialize( JsonOut &jsout ) const;
    void deserialize( JsonIn &jsin );
};

struct HSVColor {
    /// Hue: 0~393210 ( ( 1 << 16 ) - 1) * 6)
    uint32_t H;
    // Saturation: 0~65535
    uint16_t S;
    // Value: 0~255
    uint8_t V;
    // Alpha: 0~255
    uint8_t A;
};

auto curses_color_to_RGB( const nc_color &color ) -> RGBColor;
auto hsv2rgb( HSVColor color ) -> RGBColor;
auto rgb2hsv( RGBColor color ) -> HSVColor;