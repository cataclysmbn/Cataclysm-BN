#include "dynamic_atlas.h"
#if defined(TILES)

#include "cata_tiles.h"

#include <cassert>
#include <format>
#include <ranges>
#include <stack>
#include <utility>
#include <vector>

#include "sdl_utils.h"
#include "sdltiles.h"

detail::texture_packer::~texture_packer() = default;

template<typename T>
static T round_up( T n, T m )
{
    if( m == 0 ) {
        return n;
    }
    return ( ( n + m - 1 ) / m ) * m;
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

auto dynamic_atlas::get_staging_area(
    const int width, const int height ) -> std::tuple<SDL_Texture *, SDL_Surface *, SDL_Rect>
{
    const auto r_width = round_up( width, hint_sprite_width );
    const auto r_height = round_up( height, hint_sprite_height );

    if( staging_surf == nullptr || staging_surf->w < r_width ||
        staging_surf->h < r_height ) {
        const auto &r = get_sdl_renderer();
        staging_surf = create_surface_32( r_width, r_height );
        staging_tex = CreateTexture( r, sdl_color_pixel_format,
                                     SDL_TEXTUREACCESS_TARGET, r_width, r_height );
        SDL_SetTextureBlendMode( staging_tex.get(), SDL_BLENDMODE_NONE );
        SDL_SetSurfaceBlendMode( staging_surf.get(), SDL_BLENDMODE_NONE );
    }

    return std::make_tuple( staging_tex.get(), staging_surf.get(),
                            SDL_Rect{0, 0, width, height} );
}

auto dynamic_atlas::id_assign( const size_t id, const atlas_texture &tex ) -> bool
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

auto dynamic_atlas::id_search( const size_t id ) -> std::optional<atlas_texture>
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

    for( auto &it : sheets ) {
        const auto &r = get_sdl_renderer();
        if( it.readback == nullptr ) {
            it.readback = create_surface_32( it.atlas_width, it.atlas_height );
            SDL_SetSurfaceBlendMode( it.readback.get(), SDL_BLENDMODE_NONE );
        }

        if( it.dirty ) {
            const auto prev_rt = SDL_GetRenderTarget( r.get() );
            SDL_SetRenderTarget( r.get(), it.texture.get() );
            SDL_RenderReadPixels( r.get(), nullptr, 0, it.readback->pixels, it.readback->pitch );
            SDL_SetRenderTarget( r.get(), prev_rt );
        }
    }
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
           : std::make_tuple( true, it->readback.get(), tex.srcrect );
}

atlas_texture dynamic_atlas::allocate_sprite( const int w, const int h )
{
    constexpr auto get_texture = []( const SDL_Texture_SharedPtr & tex, const SDL_Rect & r,
    const int actual_w, const int actual_h ) {
        assert( actual_w <= r.w && actual_h <= r.h );
        SDL_Rect r2 = {
            r.x, r.y, actual_w, actual_h
        };
        return atlas_texture{tex, r2};
    };

    for( const auto &s : sheets ) {
        auto p = s.packer->pack( w, h );
        if( p.has_value() ) {
            return get_texture( s.texture, p.value(), w, h );
        }
    }

    const auto &r = get_sdl_renderer();
    SDL_RendererInfo info{};
    SDL_GetRendererInfo( r.get(), &info );
    int tex_width;
    int tex_height;
    int tex_access;

    std::unique_ptr<detail::texture_packer> packer;
    if( info.flags & SDL_RENDERER_SOFTWARE ) {
        tex_width = w;
        tex_height = h;
        tex_access = SDL_TEXTUREACCESS_TARGET;
        packer = std::make_unique<null_texture_packer>(
                     SDL_Rect{0, 0, tex_width, tex_height}
                 );
    } else {
        tex_width = std::min( max_atlas_width, info.max_texture_width );
        tex_height = std::min( max_atlas_height, info.max_texture_height );
        tex_access = SDL_TEXTUREACCESS_TARGET;
        packer = std::make_unique<stripe_texture_packer>(
                     SDL_Rect{0, 0, tex_width, tex_height},
                     hint_sprite_width
                 );
    }

    assert( w <= tex_width && h <= tex_height );

    const auto tex = SDL_CreateTexture( r.get(), sdl_color_pixel_format, tex_access, tex_width,
                                        tex_height );
    SDL_SetTextureBlendMode( tex, SDL_BLENDMODE_BLEND );

    {
        const auto state = sdl_save_render_state( r.get() );

        SDL_SetRenderTarget( r.get(), tex );
        SetRenderDrawColor( r, 255, 255, 255, 0 );
        SDL_RenderClear( r.get() );

        sdl_restore_render_state( r.get(), state );
    }

    auto s = sprite_sheet{
        SDL_Texture_Ptr( tex ),
        std::move( packer ),
        tex_width,
        tex_height,
        nullptr,
        true
    };
    auto &entry = sheets.emplace_back( std::move( s ) );
    entry.dirty = true;

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