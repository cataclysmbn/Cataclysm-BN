#if defined(TILES)
#include "sdl_geometry.h"
#include "sdl_utils.h"
#include "debug.h"

#define dbg(x) DebugLogFL((x),DC::SDL)

void GeometryRenderer::horizontal_line( const SDL_Renderer_Ptr &renderer, point pos, int x2,
                                        int thickness, const SDL_Color &color ) const
{
    SDL_FRect rect { static_cast<float>( pos.x ), static_cast<float>( pos.y ),
                     static_cast<float>( x2 - pos.x ), static_cast<float>( thickness ) };
    this->rect( renderer, rect, color );
}

void GeometryRenderer::vertical_line( const SDL_Renderer_Ptr &renderer, point pos, int y2,
                                      int thickness, const SDL_Color &color ) const
{
    SDL_FRect rect { static_cast<float>( pos.x ), static_cast<float>( pos.y ),
                     static_cast<float>( thickness ), static_cast<float>( y2 - pos.y ) };
    this->rect( renderer, rect, color );
}

void GeometryRenderer::rect( const SDL_Renderer_Ptr &renderer, point pos, int width,
                             int height, const SDL_Color &color ) const
{
    SDL_FRect rect { static_cast<float>( pos.x ), static_cast<float>( pos.y ),
                     static_cast<float>( width ), static_cast<float>( height ) };
    this->rect( renderer, rect, color );
}


void DefaultGeometryRenderer::rect( const SDL_Renderer_Ptr &renderer, const SDL_FRect &rect,
                                    const SDL_Color &color ) const
{
    SetRenderDrawColor( renderer, color.r, color.g, color.b, color.a );
    RenderFillRect( renderer, &rect );
}

ColorModulatedGeometryRenderer::ColorModulatedGeometryRenderer( const SDL_Renderer_Ptr &renderer )
{
    SDL_Surface_Ptr alt_surf = CreateSurface( sdl_color_pixel_format, 1, 1 );
    if( alt_surf ) {
        FillSurfaceRect( alt_surf, nullptr,
                         SDL_MapRGB( SDL_GetPixelFormatDetails( alt_surf->format ), nullptr,
                                     255, 255, 255 ) );

        tex.reset( SDL_CreateTextureFromSurface( renderer.get(), alt_surf.get() ) );
        alt_surf.reset();

        // Test to make sure color modulation is supported by renderer
        bool tex_enable = !SetTextureColorMod( tex, 0, 0, 0 );
        if( !tex_enable ) {
            tex.reset();
        }
        dbg( DL::Info ) << "ColorModulatedGeometryRenderer constructor() = " <<
                        ( tex_enable ? "FAIL" : "SUCCESS" ) << ". tex_enable = " << tex_enable;
    } else {
        dbg( DL::Error ) << "CreateRGBSurface failed: " << SDL_GetError();
    }
}

void ColorModulatedGeometryRenderer::rect( const SDL_Renderer_Ptr &renderer, const SDL_FRect &rect,
        const SDL_Color &color ) const
{
    if( tex ) {
        SetTextureColorMod( tex, color.r, color.g, color.b );
        RenderCopy( renderer, tex, nullptr, &rect );
    } else {
        DefaultGeometryRenderer::rect( renderer, rect, color );
    }
}

#endif // TILES
