#if defined(TILES)

#include "sdl_wrappers.h"
#include "sdl_utils.h"

#include <cassert>
#include <ostream>
#include <stdexcept>
#include <string>

#include "debug.h"
#include "point.h"

#define dbg(x) DebugLogFL((x),DC::SDL)

bool printErrorIf( const bool condition, const char *const message )
{
    if( !condition ) {
        return false;
    }
    dbg( DL::Error ) << message << ": " << SDL_GetError();
    return true;
}

void throwErrorIf( const bool condition, const char *const message )
{
    if( !condition ) {
        return;
    }
    throw std::runtime_error( std::string( message ) + ": " + SDL_GetError() );
}

void RenderCopy( const SDL_Renderer_Ptr &renderer, const SDL_Texture_Ptr &texture,
                 const SDL_FRect *srcrect, const SDL_FRect *dstrect )
{
    if( !renderer ) {
        dbg( DL::Error ) << "Tried to render to a null renderer";
        return;
    }
    if( !texture ) {
        dbg( DL::Error ) << "Tried to render a null texture";
        return;
    }
    printErrorIf( !SDL_RenderTexture( renderer.get(), texture.get(), srcrect, dstrect ),
                  "SDL_RenderTexture failed" );
}

SDL_Texture_Ptr CreateTexture( const SDL_Renderer_Ptr &renderer, SDL_PixelFormat format,
                               SDL_TextureAccess access, int w, int h )
{
    if( !renderer ) {
        dbg( DL::Error ) << "Tried to create texture with a null renderer";
        return SDL_Texture_Ptr();
    }
    SDL_Texture_Ptr result( SDL_CreateTexture( renderer.get(), format, access, w, h ) );
    printErrorIf( !result, "SDL_CreateTexture failed" );
    if( result ) {
        SDL_SetTextureScaleMode( result.get(), SDL_SCALEMODE_NEAREST );
    }
    return result;
}

SDL_Texture_Ptr CreateTextureFromSurface( const SDL_Renderer_Ptr &renderer,
        const SDL_Surface_Ptr &surface )
{
    if( !renderer ) {
        dbg( DL::Error ) << "Tried to create texture with a null renderer";
        return SDL_Texture_Ptr();
    }
    if( !surface ) {
        dbg( DL::Error ) << "Tried to create texture from a null surface";
        return SDL_Texture_Ptr();
    }
    SDL_Texture_Ptr result( SDL_CreateTextureFromSurface( renderer.get(), surface.get() ) );
    printErrorIf( !result, "SDL_CreateTextureFromSurface failed" );
    if( result ) {
        SDL_SetTextureScaleMode( result.get(), SDL_SCALEMODE_NEAREST );
    }
    return result;
}

void SetRenderDrawColor( const SDL_Renderer_Ptr &renderer, const Uint8 r, const Uint8 g,
                         const Uint8 b, const Uint8 a )
{
    if( !renderer ) {
        dbg( DL::Error ) << "Tried to use a null renderer";
        return;
    }
    printErrorIf( !SDL_SetRenderDrawColor( renderer.get(), r, g, b, a ),
                  "SDL_SetRenderDrawColor failed" );
}

void RenderDrawPoint( const SDL_Renderer_Ptr &renderer, point p )
{
    printErrorIf( !SDL_RenderPoint( renderer.get(), static_cast<float>( p.x ),
                                    static_cast<float>( p.y ) ), "SDL_RenderPoint failed" );
}

void RenderFillRect( const SDL_Renderer_Ptr &renderer, const SDL_FRect *const rect )
{
    if( !renderer ) {
        dbg( DL::Error ) << "Tried to use a null renderer";
        return;
    }
    printErrorIf( !SDL_RenderFillRect( renderer.get(), rect ), "SDL_RenderFillRect failed" );
}

void FillSurfaceRect( const SDL_Surface_Ptr &surface, const SDL_Rect *const rect, Uint32 color )
{
    if( !surface ) {
        dbg( DL::Error ) << "Tried to use a null surface";
        return;
    }
    printErrorIf( !SDL_FillSurfaceRect( surface.get(), rect, color ), "SDL_FillSurfaceRect failed" );
}

void SetTextureBlendMode( const SDL_Texture_Ptr &texture, SDL_BlendMode blendMode )
{
    if( !texture ) {
        dbg( DL::Error ) << "Tried to use a null texture";
    }

    throwErrorIf( !SDL_SetTextureBlendMode( texture.get(), blendMode ),
                  "SDL_SetTextureBlendMode failed" );
}

bool SetTextureColorMod( const SDL_Texture_Ptr &texture, Uint32 r, Uint32 g, Uint32 b )
{
    if( !texture ) {
        dbg( DL::Error ) << "Tried to use a null texture";
        return true;
    }
    return printErrorIf( !SDL_SetTextureColorMod( texture.get(), r, g, b ),
                         "SDL_SetTextureColorMod failed" );
}

void SetRenderDrawBlendMode( const SDL_Renderer_Ptr &renderer, const SDL_BlendMode blendMode )
{
    if( !renderer ) {
        dbg( DL::Error ) << "Tried to use a null renderer";
        return;
    }
    printErrorIf( !SDL_SetRenderDrawBlendMode( renderer.get(), blendMode ),
                  "SDL_SetRenderDrawBlendMode failed" );
}

void GetRenderDrawBlendMode( const SDL_Renderer_Ptr &renderer, SDL_BlendMode &blend_mode )
{
    if( !renderer ) {
        dbg( DL::Error ) << "Tried to use a null renderer";
        return;
    }
    printErrorIf( !SDL_GetRenderDrawBlendMode( renderer.get(), &blend_mode ),
                  "SDL_GetRenderDrawBlendMode failed" );
}

SDL_Surface_Ptr load_image( const char *const path )
{
    assert( path );
    SDL_Surface_Ptr result( IMG_Load( path ) );
    if( !result ) {
        throw std::runtime_error( "Could not load image \"" + std::string( path ) + "\": " +
                                  SDL_GetError() );
    }
    // Convert Surface to raw SDL_Color format if necessary, as load_image doesn't guarantee any particular format to be loaded
    if( result->format != sdl_color_pixel_format ) {
        result = SDL_Surface_Ptr{ SDL_ConvertSurface( result.get(), sdl_color_pixel_format ) };
    }
    return result;
}

void SetRenderTarget( const SDL_Renderer_Ptr &renderer, const SDL_Texture_Ptr &texture )
{
    if( !renderer ) {
        dbg( DL::Error ) << "Tried to use a null renderer";
        return;
    }
    // a null texture is fine for SDL
    printErrorIf( !SDL_SetRenderTarget( renderer.get(), texture.get() ),
                  "SDL_SetRenderTarget failed" );
}

void RenderClear( const SDL_Renderer_Ptr &renderer )
{
    if( !renderer ) {
        dbg( DL::Error ) << "Tried to use a null renderer";
        return;
    }
    printErrorIf( !SDL_RenderClear( renderer.get() ), "SDL_RenderClear failed" );
}

SDL_Surface_Ptr CreateSurface( const SDL_PixelFormat format, const int width, const int height )
{
    SDL_Surface_Ptr surface( SDL_CreateSurface( width, height, format ) );
    throwErrorIf( !surface, "Failed to create surface" );
    return surface;
}

#endif
