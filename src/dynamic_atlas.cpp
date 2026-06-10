#include "dynamic_atlas.h"
#if defined(TILES)

#include "cata_tiles.h"

#include <cassert>
#include <format>
#include <ranges>
#include <stack>
#include <string_view>
#include <utility>
#include <vector>

#include "preload_config.h"
#include "sdl_utils.h"
#include "sdltiles.h"

#define dbg(x) DebugLogFL((x),DC::SDL)

detail::texture_packer::~texture_packer() = default;

template<typename T>
static T round_up( T n, T m )
{
    if( m == 0 ) {
        return n;
    }
    return ( ( n + m - 1 ) / m ) * m;
}

static bool use_texture_streaming()
{
    static std::unordered_map<std::string, bool> auto_values = {
        {SDL_SOFTWARE_RENDERER, false},
        {SDL_GPU_RENDERER, true},
    };
    const auto &r = get_sdl_renderer();
    const auto renderer = SDL_GetRendererName( r.get() );
    switch( preload_config::get_texture_streaming() ) {
        default:
        case preload_config::tristate::auto_select: {
            const auto it = auto_values.find( renderer );
            if( it != auto_values.end() ) {
                return it->second;
            }
            return false;
        }
        case preload_config::tristate::enable: {
            return true;
        }
        case preload_config::tristate::disable: {
            return false;
        }
    }
}

struct stripe_texture_packer final : detail::texture_packer {

    struct stripe {
        uint32_t height;
        uint32_t y_offset;
        uint32_t x_remainder;
    };

    std::vector<stripe> stripes;
    uint32_t y_remainder;
    uint32_t min_size;

    stripe_texture_packer( const SDL_Rect &bounds, const uint32_t min_size )
        : texture_packer( bounds ), y_remainder( bounds.h ), min_size( min_size ) {}

    std::optional<SDL_Rect> pack( const uint32_t width,
                                  const uint32_t height ) override {

        if( std::cmp_greater( width, bounds.w ) || std::cmp_greater( height, bounds.h ) ) {
            return std::nullopt;
        }

        const auto r_height = round_up( height, min_size );

        auto it = std::ranges::find_if( stripes, [&]( const stripe & s ) {
            return s.x_remainder >= width && s.height == r_height;
        } );

        if( it == stripes.end() ) {
            if( r_height > y_remainder || y_remainder < min_size ) {
                return std::nullopt;
            }

            const auto line_height = r_height;
            const auto y_offset = bounds.h - y_remainder;
            const auto x_remainder = static_cast<uint32_t>( bounds.w );

            it = stripes.emplace( stripes.end(), stripe{
                line_height,
                y_offset,
                x_remainder,
            } );
            y_remainder -= line_height;
        }

        auto &s = *it;

        const auto x_offset = bounds.w - s.x_remainder;
        SDL_Rect rect{
            static_cast<int>( bounds.x + x_offset ),
            static_cast<int>( bounds.y + s.y_offset ),
            static_cast<int>( width ),
            static_cast<int>( height )
        };

        s.x_remainder -= width;
        if( s.x_remainder < min_size ) {
            s.x_remainder = 0;
        }

        return rect;
    }
};

struct null_texture_packer final : detail::texture_packer {

    bool has_contents;

    explicit null_texture_packer( const SDL_Rect &bounds )
        : texture_packer( bounds )
        , has_contents( false ) {
    }

    std::optional<SDL_Rect> pack( const uint32_t width, const uint32_t height ) override {
        if( has_contents
            || std::cmp_greater( width, bounds.w )
            || std::cmp_greater( height, bounds.h ) ) {
            return std::nullopt;
        }
        has_contents = true;
        return bounds;
    };
};

auto dynamic_atlas::update_staging_area(
    staging_area &staging, const int width, const int height )
const -> std::tuple<SDL_Texture *, SDL_Surface *, SDL_Rect>
{
    const auto r_width = round_up( width, hint_sprite_width );
    const auto r_height = round_up( height, hint_sprite_height );

    if( staging.surf == nullptr || staging.surf->w < r_width || staging.surf->h < r_height ) {
        const auto &r = get_sdl_renderer();
        staging.surf = create_surface_32( r_width, r_height );
        staging.tex =
            CreateTexture( r, sdl_color_pixel_format, SDL_TEXTUREACCESS_TARGET, r_width, r_height );
        SDL_SetTextureBlendMode( staging.tex.get(), SDL_BLENDMODE_NONE );
        SDL_SetSurfaceBlendMode( staging.surf.get(), SDL_BLENDMODE_NONE );
    }

    return std::make_tuple( staging.tex.get(), staging.surf.get(), SDL_Rect( 0, 0, width, height ) );
}

auto dynamic_atlas::get_staging_area(
    const int width, const int height ) -> std::tuple<SDL_Texture *, SDL_Surface *, SDL_Rect>
{
    return update_staging_area( user_staging, width, height );
}

auto dynamic_atlas::assign_id_internal( const size_t id, const atlas_texture &tex ) -> bool
{
    const auto it = std::ranges::find_if( sheets, [&]( const sprite_sheet & s ) {
        return s.texture.get() == std::get<0>( tex ).get();
    } );
    if( it == sheets.end() ) {
        return false;
    }
    int sheet_index = std::distance( sheets.begin(), it );

    auto [iter, ok] = sprite_ids.emplace( id, std::make_pair( sheet_index, std::get<1>( tex ) ) );
    return ok;
}

auto dynamic_atlas::find_sprite( const size_t id ) -> std::optional<atlas_texture>
{
    const auto it = sprite_ids.find( id );
    if( it == sprite_ids.end() ) {
        return std::nullopt;
    }

    auto [sheet_idx, rect] = it->second;

    return atlas_texture{sheets[sheet_idx].texture, rect};
}

void dynamic_atlas::readback_load()
{
    const auto &r = get_sdl_renderer();
    const auto state = sdl_save_render_state( r.get() );
    for( auto &it : sheets ) {
        if( it.dirty ) {
            auto tmpTex = CreateTexture( r, sdl_color_pixel_format, SDL_TEXTUREACCESS_TARGET, it.atlas_width,
                                         it.atlas_height );
            SDL_SetRenderTarget( r.get(), tmpTex.get() );
            SDL_RenderTexture( r.get(), it.texture.get(), nullptr, nullptr );
            // SDL3: SDL_RenderReadPixels returns a new surface owned by us.
            it.readback.reset( SDL_RenderReadPixels( r.get(), nullptr ) );
            it.dirty = false;
        }
    }
    sdl_restore_render_state( r.get(), state );
}

void dynamic_atlas::readback_clear()
{
    for( auto &it : sheets ) {
        it.readback.reset();
        it.dirty = true;
    }
}

auto dynamic_atlas::readback_find( const texture &tex ) -> std::tuple<bool, SDL_Surface *, SDL_Rect>
{
    const auto it = std::ranges::find_if( sheets, [&]( const sprite_sheet & s ) {
        return s.texture == tex.sdl_texture_ptr;
    } );

    return ( it == sheets.end() )
           ? std::make_tuple( false, nullptr, SDL_Rect{} )
    : std::make_tuple( true, it->readback.get(), SDL_Rect{
        static_cast<int>( tex.srcrect.x ), static_cast<int>( tex.srcrect.y ),
        static_cast<int>( tex.srcrect.w ), static_cast<int>( tex.srcrect.h )
    } );
}

auto dynamic_atlas::get_or_create_sprite(
    const int w, const int h,
    const std::optional<size_t> &id,
    const sprite_callback &cb ) -> atlas_texture
{
    const auto existing = id.has_value() ? find_sprite( id.value() ) : std::nullopt;
    if( existing.has_value() ) {
        return existing.value();
    }
    return create_sprite( w, h, id, cb );
}

auto dynamic_atlas::create_sprite(
    const int w, const int h,
    const std::optional<size_t> &id,
    const sprite_callback &blitFn ) -> atlas_texture
{
    // TODO: Update sprite instead of allocating a new one if ID already exists?
    auto atl_tex = allocate_sprite_internal( w, h );
    if( id.has_value() && !this->assign_id_internal( id.value(), atl_tex ) ) {
        debugmsg( "Duplicate sprite ID in atlas: %x", id.value() );
    }
    auto& [tex, dstRect] = atl_tex;

    const auto tmpRect = SDL_Rect( 0, 0, w, h );
    if( use_texture_streaming() ) {
        SDL_Surface *tmpSurf{};
        if( SDL_LockTextureToSurface( tex.get(), &dstRect, &tmpSurf ) ) {
            blitFn( tmpSurf, &tmpRect );
            SDL_UnlockTexture( tex.get() );
        } else {
            debugmsg( "Failed to lock dynamic atlas texture for writing." );
        }
    } else {
        const auto &r = get_sdl_renderer();

        const auto [stTex, stSurf, stRect] = update_staging_area( local_staging,  w, h );
        blitFn( stSurf, &stRect );
        SDL_UpdateTexture( stTex, nullptr, stSurf->pixels, stSurf->pitch );

        const auto state = sdl_save_render_state( r.get() );

        if( SDL_SetRenderTarget( r.get(), tex.get() ) ) {
            const auto fSrc = SDL_FRect( stRect.x, stRect.y, stRect.w, stRect.h );
            const auto fDst = SDL_FRect( dstRect.x, dstRect.y, dstRect.w, dstRect.h );
            SDL_RenderTexture( r.get(), stTex, &fSrc, &fDst );
        } else {
            debugmsg( "Failed to set dynamic atlas texture as render target." );
        }

        sdl_restore_render_state( r.get(), state );
    }

    return atl_tex;
}

atlas_texture dynamic_atlas::allocate_sprite_internal( const int w, const int h )
{
    constexpr auto get_texture = []( const SDL_Texture_SharedPtr & tex, const SDL_Rect & r,
    const int actual_w, const int actual_h ) {
        assert( actual_w <= r.w && actual_h <= r.h );
        SDL_Rect r2 = {
            r.x, r.y, actual_w, actual_h
        };
        return atlas_texture{tex, r2};
    };

    for( auto &s : sheets ) {
        auto p = s.packer->pack( w, h );
        if( p.has_value() ) {
            s.dirty = true;
            return get_texture( s.texture, p.value(), w, h );
        }
    }

    const auto &r = get_sdl_renderer();
    const bool is_software = ( std::string_view( SDL_GetRendererName( r.get() ) ) == "software" );
    int tex_width;
    int tex_height;

    std::unique_ptr<detail::texture_packer> packer;
    if( is_software ) {
        tex_width = w;
        tex_height = h;
        packer = std::make_unique<null_texture_packer>(
                     SDL_Rect{0, 0, tex_width, tex_height}
                 );
    } else {
        const auto props = SDL_GetRendererProperties( r.get() );
        const int max_tex = static_cast<int>(
                                SDL_GetNumberProperty( props, SDL_PROP_RENDERER_MAX_TEXTURE_SIZE_NUMBER, 4096 ) );
        tex_width = std::min( max_atlas_width, max_tex );
        tex_height = std::min( max_atlas_height, max_tex );
        packer = std::make_unique<stripe_texture_packer>(
                     SDL_Rect{0, 0, tex_width, tex_height},
                     hint_sprite_width
                 );
    }

    assert( w <= tex_width && h <= tex_height );

    const auto access = use_texture_streaming() ? SDL_TEXTUREACCESS_STREAMING :
                        SDL_TEXTUREACCESS_TARGET;

    const auto tex =
        SDL_CreateTexture( r.get(), sdl_color_pixel_format, access, tex_width, tex_height );
    SDL_SetTextureBlendMode( tex, SDL_BLENDMODE_BLEND );
    SDL_SetTextureScaleMode( tex, SDL_SCALEMODE_NEAREST );

    auto s = sprite_sheet{
        SDL_Texture_Ptr( tex ),
        std::move( packer ),
        tex_width,
        tex_height,
        nullptr,
        true
    };
    const auto &entry = sheets.emplace_back( std::move( s ) );

    const auto rect = entry.packer->pack( w, h );

    return get_texture( entry.texture, rect.value(), w, h );
}

void dynamic_atlas::readback_dump( const std::string &s ) const
{
    int i = 0;
    for( auto &q : sheets ) {
        auto name = std::format( "{}/tile_dump_{}.png", s, i++ );
        // TODO: fix windows saving images with swapped red/blue channels (it seems to want ARGB not ABGR)
        IMG_SavePNG( q.readback.get(), name.c_str() );
    }
}


void dynamic_atlas::clear()
{
    sheets.clear();
}

#endif