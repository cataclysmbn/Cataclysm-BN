#if defined(TILES)

#include "sdl_utils.h"

#include <array>
#include <utility>

#include "color.h"
#include "color_loader.h"
#include "cursesport.h"
#include "debug.h"
#include "sdltiles.h"

color_pixel_function_map builtin_color_pixel_functions = {
    { "color_pixel_none", nullptr },
    { "color_pixel_copy", color_pixel_copy },
    { "color_pixel_darken", color_pixel_darken },
    { "color_pixel_sepia", color_pixel_sepia },
    { "color_pixel_grayscale", color_pixel_grayscale },
    { "color_pixel_nightvision", color_pixel_nightvision },
    { "color_pixel_overexposed", color_pixel_overexposed },
    { "color_pixel_underwater", color_pixel_underwater },
    { "color_pixel_underwater_dark", color_pixel_underwater_dark },
    { "color_pixel_zoverlay", color_pixel_z_overlay },
};

color_pixel_function_pointer get_color_pixel_function( const std::string &name )
{
    const auto iter = builtin_color_pixel_functions.find( name );
    if( iter == builtin_color_pixel_functions.end() ) {
        debugmsg( "no color pixel function with name %s", name );
        return nullptr;
    }
    return iter->second;
}

SDL_Color adjust_color_brightness( const SDL_Color &color, int percent )
{
    if( percent <= 0 ) {
        return { 0x00, 0x00, 0x00, color.a };
    }

    if( percent == 100 ) {
        return color;
    }

    return SDL_Color{
        static_cast<Uint8>( std::min( color.r *percent / 100, 0xFF ) ),
        static_cast<Uint8>( std::min( color.g *percent / 100, 0xFF ) ),
        static_cast<Uint8>( std::min( color.b *percent / 100, 0xFF ) ),
        color.a
    };
}

SDL_Color mix_colors( const SDL_Color &first, const SDL_Color &second, int second_percent )
{
    if( second_percent <= 0 ) {
        return first;
    }

    if( second_percent >= 100 ) {
        return second;
    }

    const int first_percent = 100 - second_percent;

    return SDL_Color{
        static_cast<Uint8>( std::min( ( first.r * first_percent + second.r * second_percent ) / 100, 0xFF ) ),
        static_cast<Uint8>( std::min( ( first.g * first_percent + second.g * second_percent ) / 100, 0xFF ) ),
        static_cast<Uint8>( std::min( ( first.b * first_percent + second.b * second_percent ) / 100, 0xFF ) ),
        static_cast<Uint8>( std::min( ( first.a * first_percent + second.a * second_percent ) / 100, 0xFF ) )
    };
}

SDL_Color color_pixel_grayscale( const SDL_Color &color )
{
    if( is_black( color ) ) {
        return color;
    }

    const Uint8 av = average_pixel_color( color );
    const Uint8 result = std::max( av * 5 >> 3, 0x01 );

    return { result, result, result, color.a };
}

SDL_Color color_pixel_nightvision( const SDL_Color &color )
{
    const Uint8 av = average_pixel_color( color );
    const Uint8 result = std::min( ( av * ( ( av * 3 >> 2 ) + 64 ) >> 8 ) + 16, 0xFF );

    return {
        static_cast<Uint8>( result >> 2 ),
        static_cast<Uint8>( result ),
        static_cast<Uint8>( result >> 3 ),
        color.a
    };
}

SDL_Color color_pixel_overexposed( const SDL_Color &color )
{
    const Uint8 av = average_pixel_color( color );
    const Uint8 result = std::min( 64 + ( av * ( ( av >> 2 ) + 0xC0 ) >> 8 ), 0xFF );

    return {
        static_cast<Uint8>( result >> 2 ),
        static_cast<Uint8>( result ),
        static_cast<Uint8>( result >> 3 ),
        color.a
    };
}

SDL_Color color_pixel_underwater( const SDL_Color &color )
{
    if( is_black( color ) ) {
        return color;
    }

    return {
        std::max<Uint8>( 175 * color.r >> 8, 0x01 ),
        std::max<Uint8>( 225 * color.g >> 8, 0x01 ),
        std::max<Uint8>( 250 * color.b >> 8, 0x01 ),
        color.a
    };
}

SDL_Color color_pixel_underwater_dark( const SDL_Color &color )
{
    if( is_black( color ) ) {
        return color;
    }

    // Half the levels of bright version, except slightly more red to keep it from getting too dark.
    return {
        std::max<Uint8>( 100 * color.r >> 8, 0x01 ),
        std::max<Uint8>( 113 * color.g >> 8, 0x01 ),
        std::max<Uint8>( 125 * color.b >> 8, 0x01 ),
        color.a
    };
}

SDL_Color color_pixel_copy( const SDL_Color &color ) {
    return color;
}

SDL_Color color_pixel_darken( const SDL_Color &color )
{
    if( is_black( color ) ) {
        return color;
    }

    // 85/256 ~ 1/3
    return {
        std::max<Uint8>( 85 * color.r >> 8, 0x01 ),
        std::max<Uint8>( 85 * color.g >> 8, 0x01 ),
        std::max<Uint8>( 85 * color.b >> 8, 0x01 ),
        color.a
    };

}

SDL_Color color_pixel_sepia( const SDL_Color &color )
{
    if( is_black( color ) ) {
        return color;
    }

    /*
     *  Objective is to provide a gradient between two color points
     *  (sepia_dark and sepia_light) based on the grayscale value.
     *  This presents an effect intended to mimic a faded sepia photograph.
     */

    const SDL_Color sepia_dark = { 39, 23, 19, color.a};
    const SDL_Color sepia_light = { 241, 220, 163, color.a};

    const Uint8 av = average_pixel_color( color );
    const float gammav = 1.6;
    const float pv = av / 255.0;
    const Uint8 finalv =
        std::min( static_cast<int>( std::round( std::pow( pv, gammav ) * 150 ) ), 100 );

    return mix_colors( sepia_dark, sepia_light, finalv );
}

SDL_Color color_pixel_z_overlay( const SDL_Color &color )
{
    if( static_z_effect ) {
        return mix_colors( color, {128, 255, 255, color.a}, color.a / 8 );
    }
    return { 128, 255, 255, color.a };
}

SDL_Color curses_color_to_SDL( const nc_color &color )
{
    const int pair_id = color.to_color_pair_index();
    const auto pair = cata_cursesport::colorpairs[pair_id];

    int palette_index = pair.FG != 0 ? pair.FG : pair.BG;

    if( color.is_bold() ) {
        palette_index += color_loader<SDL_Color>::COLOR_NAMES_COUNT / 2;
    }

    return windowsPalette[palette_index];
}

std::vector<SDL_Color> color_linear_interpolate( const SDL_Color &start_color,
        const SDL_Color &end_color, const unsigned additional_steps )
{
    const unsigned steps = additional_steps + 2;
    std::vector<SDL_Color> colors;
    colors.reserve( steps );
    colors.push_back( start_color );

    const float step = 1.0f / ( steps - 1 );
    for( unsigned i = 0; i < steps - 2; i++ ) {
        const float percent = ( i + 1 ) * step;
        const Uint8 r = static_cast<Uint8>( start_color.r + static_cast<Uint8>( percent *
                                            ( end_color.r - start_color.r ) ) );
        const Uint8 g = static_cast<Uint8>( start_color.g + static_cast<Uint8>( percent *
                                            ( end_color.g - start_color.g ) ) );
        const Uint8 b = static_cast<Uint8>( start_color.b + static_cast<Uint8>( percent *
                                            ( end_color.b - start_color.b ) ) );
        colors.push_back( { r, g, b, 255 } );
    }
    colors.push_back( end_color );
    return colors;
}

SDL_Surface_Ptr create_surface_32( int w, int h )
{
#if SDL_BYTEORDER == SDL_BIG_ENDIAN
    return CreateRGBSurface( 0, w, h, 32, 0xFF000000, 0x00FF0000, 0x0000FF00, 0x000000FF );
#else
    return CreateRGBSurface( 0, w, h, 32, 0x000000FF, 0x0000FF00, 0x00FF0000, 0xFF000000 );
#endif
}

SDL_Rect fit_rect_inside( const SDL_Rect &inner, const SDL_Rect &outer )
{
    const float inner_ratio = static_cast<float>( inner.w ) / inner.h;
    const float outer_ratio = static_cast<float>( outer.w ) / outer.h;
    const float factor = inner_ratio > outer_ratio
                         ? static_cast<float>( outer.w ) / inner.w
                         : static_cast<float>( outer.h ) / inner.h;

    const int w = factor * inner.w;
    const int h = factor * inner.h;
    const point p( outer.x + ( outer.w - w ) / 2, outer.y + ( outer.h - h ) / 2 );

    return SDL_Rect{ p.x, p.y, w, h };
}

void get_pixel_rgba( const SDL_Surface *surface, int x, int y,
                     Uint8 &r, Uint8 &g, Uint8 &b, Uint8 &a )
{
    auto *pixels = static_cast<Uint8 *>( surface->pixels );
    Uint8 *p = pixels + y * surface->pitch + x * surface->format->BytesPerPixel;

    Uint32 pixel;
    switch( surface->format->BytesPerPixel ) {
    case 4:
        pixel = *reinterpret_cast<Uint32 *>( p );
        break;
    case 3:
        // 24-bit surfaces (rare)
        if constexpr ( SDL_BYTEORDER == SDL_BIG_ENDIAN ) {
            pixel = p[0] << 16 | p[1] << 8 | p[2];
        } else {
            pixel = p[0] | p[1] << 8 | p[2] << 16;
        }
        break;
    default:
        pixel = 0;
        break;
    }

    SDL_GetRGBA( pixel, surface->format, &r, &g, &b, &a );
}

void set_pixel_rgba( const SDL_Surface *surface, int x, int y,
                     Uint8 r, Uint8 g, Uint8 b, Uint8 a )
{
    Uint32 pixel = SDL_MapRGBA( surface->format, r, g, b, a );

    auto *pixels = static_cast<Uint8 *>( surface->pixels );
    Uint8 *p = pixels + y * surface->pitch + x * surface->format->BytesPerPixel;

    switch( surface->format->BytesPerPixel ) {
    case 4:
        *reinterpret_cast<Uint32 *>( p ) = pixel;
        break;
    case 3:
        // 24-bit surfaces (rare)
        if constexpr ( SDL_BYTEORDER == SDL_BIG_ENDIAN ) {
            p[0] = ( pixel >> 16 ) & 0xff;
            p[1] = ( pixel >> 8 ) & 0xff;
            p[2] = pixel & 0xff;
        } else {
            p[0] = pixel & 0xff;
            p[1] = ( pixel >> 8 ) & 0xff;
            p[2] = ( pixel >> 16 ) & 0xff;
        }
        break;
    default:
        break;
    }
}

#endif // SDL_TILES
