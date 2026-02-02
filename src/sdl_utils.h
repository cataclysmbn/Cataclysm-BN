#pragma once

#include <algorithm>
#include <cmath>
#include <string>
#include <unordered_map>
#include <vector>

#include "cached_options.h"
#include "color.h"
#include "debug.h"
#include "sdl_wrappers.h"

using color_pixel_function_pointer = SDL_Color( * )( const SDL_Color &color );
using color_pixel_function_map = std::unordered_map<std::string, color_pixel_function_pointer>;

color_pixel_function_pointer get_color_pixel_function( const std::string &name );

SDL_Color adjust_color_brightness( const SDL_Color &color, int percent );

SDL_Color mix_colors( const SDL_Color &first, const SDL_Color &second, int second_percent );

inline bool is_black( const SDL_Color &color )
{
    return
        color.r == 0x00 &&
        color.g == 0x00 &&
        color.b == 0x00;
}

enum class sdl_render_state_flags : uint32_t {
    none = 0,
    all = static_cast<uint32_t>( -1 ),

    render_target = 1 << 0,
    clip_rect = 1 << 1,
    viewport = 1 << 2,
    draw_color = 1 << 3,
    blend_mode = 1 << 4,
};

constexpr sdl_render_state_flags operator|( sdl_render_state_flags lhs,
        sdl_render_state_flags rhs )
{
    return static_cast<sdl_render_state_flags>(
               static_cast<uint32_t>( lhs ) | static_cast<uint32_t>( rhs )
           );
}

constexpr sdl_render_state_flags operator&( sdl_render_state_flags lhs,
        sdl_render_state_flags rhs )
{
    return static_cast<sdl_render_state_flags>(
               static_cast<uint32_t>( lhs ) & static_cast<uint32_t>( rhs )
           );
}

template<sdl_render_state_flags Flags>
struct sdl_render_state {

    constexpr static bool has_render_target =
        ( Flags & sdl_render_state_flags::render_target ) == sdl_render_state_flags::render_target;
    constexpr static bool has_clip_rect =
        ( Flags & sdl_render_state_flags::clip_rect ) == sdl_render_state_flags::clip_rect;
    constexpr static bool has_viewport =
        ( Flags & sdl_render_state_flags::viewport ) == sdl_render_state_flags::viewport;
    constexpr static bool has_draw_color =
        ( Flags & sdl_render_state_flags::draw_color ) == sdl_render_state_flags::draw_color;
    constexpr static bool has_blend_mode =
        ( Flags & sdl_render_state_flags::blend_mode ) == sdl_render_state_flags::blend_mode;

    using tRT = std::conditional_t<has_render_target, std::tuple<SDL_Texture *>, std::tuple<>>;
    using tCR = std::conditional_t<has_clip_rect, std::tuple<SDL_Rect>, std::tuple<>>;
    using tVP = std::conditional_t<has_viewport, std::tuple<SDL_Rect>, std::tuple<>>;
    using tDC = std::conditional_t<has_draw_color, std::tuple<SDL_Color>, std::tuple<>>;
    using tBM = std::conditional_t<has_blend_mode, std::tuple<SDL_BlendMode>, std::tuple<>>;

    constexpr static ptrdiff_t render_target_idx =
        has_render_target
        ? 0
        : -1;
    constexpr static ptrdiff_t clip_rect_idx =
        has_clip_rect
        ? ( render_target_idx + 1 )
        : render_target_idx;
    constexpr static ptrdiff_t viewport_idx =
        has_viewport
        ? ( clip_rect_idx + 1 )
        : clip_rect_idx;
    constexpr static ptrdiff_t draw_color_idx =
        has_draw_color
        ? ( viewport_idx + 1 )
        : viewport_idx;
    constexpr static ptrdiff_t blend_mode_idx =
        has_blend_mode
        ? ( draw_color_idx + 1 )
        : draw_color_idx;

    using tuple_type = decltype( std::tuple_cat( tRT{}, tCR{}, tVP{}, tDC{}, tBM{} ) );

    tuple_type value;
};

template < sdl_render_state_flags Flags =
           sdl_render_state_flags::render_target |  sdl_render_state_flags::clip_rect >
auto sdl_save_render_state( SDL_Renderer *r ) -> sdl_render_state<Flags>
{

    using type = sdl_render_state<Flags>;

    typename type::tuple_type res;
    if constexpr( type::has_render_target ) {
        std::get<type::render_target_idx>( res ) = SDL_GetRenderTarget( r );
    }
    if constexpr( type::has_clip_rect ) {
        SDL_Rect &v = std::get<type::clip_rect_idx>( res );
        SDL_RenderGetClipRect( r, &v );
    }
    if constexpr( type::has_viewport ) {
        SDL_Rect &v = std::get<type::viewport_idx>( res );
        SDL_RenderGetViewport( r, &v );
    }
    if constexpr( type::has_draw_color ) {
        SDL_Color &v = std::get<type::draw_color_idx>( res );
        SDL_GetRenderDrawColor( r, &v.r, &v.g, &v.b, &v.a );
    }
    if constexpr( type::has_blend_mode ) {
        SDL_BlendMode &v = std::get<type::blend_mode_idx>( res );
        SDL_GetRenderDrawBlendMode( r, &v );
    }

    return type{res};
}

template<sdl_render_state_flags Flags>
auto sdl_restore_render_state( SDL_Renderer *r, const sdl_render_state<Flags> &state )
{
    auto &t = state.value;
    using type = sdl_render_state<Flags>;

    if constexpr( type::has_render_target ) {
        SDL_Texture *v = std::get<type::render_target_idx>( t );
        SDL_SetRenderTarget( r, v );
    }
    if constexpr( type::has_clip_rect ) {
        const SDL_Rect &v = std::get<type::clip_rect_idx>( t );
        SDL_RenderSetClipRect( r, &v );
    }
    if constexpr( type::has_viewport ) {
        const SDL_Rect &v = std::get<type::viewport_idx>( t );
        SDL_RenderSetViewport( r, &v );
    }
    if constexpr( type::has_draw_color ) {
        const SDL_Color &v = std::get<type::draw_color_idx>( t );
        SDL_SetRenderDrawColor( r, v.r, v.g, v.b, v.a );
    }
    if constexpr( type::has_blend_mode ) {
        const SDL_BlendMode &v = std::get<type::blend_mode_idx>( t );
        SDL_SetRenderDrawBlendMode( r, v );
    }
}

inline Uint8 average_pixel_color( const SDL_Color &color )
{
    return 85 * ( color.r + color.g + color.b ) >> 8; // 85/256 ~ 1/3
}

SDL_Color color_pixel_copy( const SDL_Color &color );

SDL_Color color_pixel_grayscale( const SDL_Color &color );

SDL_Color color_pixel_nightvision( const SDL_Color &color );

SDL_Color color_pixel_overexposed( const SDL_Color &color );

SDL_Color color_pixel_underwater( const SDL_Color &color );

SDL_Color color_pixel_underwater_dark( const SDL_Color &color );

SDL_Color color_pixel_darken( const SDL_Color &color );

SDL_Color color_pixel_sepia( const SDL_Color &color );

SDL_Color color_pixel_z_overlay( const SDL_Color &color );

// Use curses_color_to_RGB if on a Curses+SDL shared context
SDL_Color curses_color_to_SDL( const nc_color &color );

#if SDL_BYTEORDER == SDL_BIG_ENDIAN
constexpr static int sdl_color_format = SDL_PIXELFORMAT_RGBA8888;
#else
constexpr static int sdl_color_pixel_format = SDL_PIXELFORMAT_ABGR8888;
#endif

///@throws std::exception upon errors.
///@returns Always a valid pointer.
SDL_Surface_Ptr create_surface_32( int w, int h );

SDL_Rect fit_rect_inside( const SDL_Rect &inner, const SDL_Rect &outer );

/** Linearly interpolate intermediate colors between two given colors.
 * @param start_color: The color to start with.
 * @param end_color: The color that ends the interpolation.
 * @param additional_steps: Number of steps between the start and stop colors.
 * @return A vector of colors containing: the start color, all intermediate colors and finally the end color.
 * @note start with white (r=255, g=255, b=255) and end with blue (r=0, g=0, b=255) and use 2 additional steps:
 *     - The first intermediate color is: (r=170, g=170, b=255)
 *     - The second intermediate color is: (r=85, g=85, b=255)
 * Obviously the more intermediate steps there are, the harder it is to differentiate the intermediate colors.
 */
std::vector<SDL_Color> color_linear_interpolate( const SDL_Color &start_color,
        const SDL_Color &end_color, unsigned additional_steps );


