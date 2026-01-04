#include "units_temperature.h"
#if defined(TILES)
#include "cata_tiles.h"

#include <algorithm>
#include <array>
#include <bitset>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iterator>
#include <optional>
#include <set>
#include <stdexcept>
#include <tuple>
#include <unordered_set>
#include <ranges>

#include "action.h"
#include "avatar.h"
#include "calendar.h"
#include "cata_utility.h"
#include "catacharset.h"
#include "character.h"
#include "character_id.h"
#include "clzones.h"
#include "color.h"
#include "coordinate_conversions.h"
#include "cuboid_rectangle.h"
#include "cursesdef.h"
#include "cursesport.h"
#include "debug.h"
#include "dynamic_atlas.h"
#include "field.h"
#include "field_type.h"
#include "flag.h"
#include "fstream_utils.h"
#include "game.h"
#include "game_constants.h"
#include "hsv_color.h"
#include "input.h"
#include "int_id.h"
#include "init.h"
#include "item.h"
#include "item_factory.h"
#include "itype.h"
#include "json.h"
#include "make_static.h"
#include "map.h"
#include "map_memory.h"
#include "mapbuffer.h"
#include "mapdata.h"
#include "mod_tileset.h"
#include "monster.h"
#include "monstergenerator.h"
#include "mtype.h"
#include "npc.h"
#include "omdata.h"
#include "output.h"
#include "overlay_ordering.h"
#include "overmap_location.h"
#include "path_info.h"
#include "pixel_minimap.h"
#include "player.h"
#include "rect_range.h"
#include "scent_map.h"
#include "sdl_utils.h"
#include "sdl_wrappers.h"
#include "sdltiles.h"
#include "sounds.h"
#include "string_formatter.h"
#include "string_id.h"
#include "string_utils.h"
#include "submap.h"
#include "tileray.h"
#include "translations.h"
#include "trap.h"
#include "type_id.h"
#include "veh_type.h"
#include "vehicle.h"
#include "vehicle_part.h"
#include "vpart_position.h"
#include "weather.h"
#include "weighted_list.h"
#include "overmapbuffer.h"

#include "profile.h"

#define dbg(x) DebugLogFL((x),DC::SDL)

static const efftype_id effect_ridden( "ridden" );

static const itype_id itype_corpse( "corpse" );
static const trait_id trait_INATTENTIVE( "INATTENTIVE" );

static const std::string ITEM_HIGHLIGHT( "highlight_item" );
static const std::string ZOMBIE_REVIVAL_INDICATOR( "zombie_revival_indicator" );

static const flag_id flag_TINT_NO_FG( "TINT_NO_FG" );
static const flag_id flag_TINT_NO_BG( "TINT_NO_BG" );
static const flag_id flag_TINT_NONE( "TINT_NONE" );

static const std::array<std::string, 8> multitile_keys = {{
        "center",
        "corner",
        "edge",
        "t_connection",
        "end_piece",
        "unconnected",
        "open",
        "broken"
    }
};

extern int fontwidth;
extern int fontheight;
static const std::string empty_string;
static const std::array<std::string, 13> TILE_CATEGORY_IDS = {{
        "", // C_NONE,
        "vehicle_part", // C_VEHICLE_PART,
        "terrain", // C_TERRAIN,
        "item", // C_ITEM,
        "furniture", // C_FURNITURE,
        "trap", // C_TRAP,
        "field", // C_FIELD,
        "lighting", // C_LIGHTING,
        "monster", // C_MONSTER,
        "bullet", // C_BULLET,
        "hit_entity", // C_HIT_ENTITY,
        "weather", // C_WEATHER,
        "overmap_terrain"
    }
};

namespace
{

std::string get_ascii_tile_id( const uint32_t sym, const int FG, const int BG )
{
    return std::string( { 'A', 'S', 'C', 'I', 'I', '_', static_cast<char>( sym ), static_cast<char>( FG ), static_cast<char>( BG ) } );
}

pixel_minimap_mode pixel_minimap_mode_from_string( const std::string &mode )
{
    if( mode == "solid" ) {
        return pixel_minimap_mode::solid;
    } else if( mode == "squares" ) {
        return pixel_minimap_mode::squares;
    } else if( mode == "dots" ) {
        return pixel_minimap_mode::dots;
    }

    debugmsg( "Unsupported pixel minimap mode \"" + mode + "\"." );
    return pixel_minimap_mode::solid;
}

struct draw_zone_overlay_options {
    const SDL_Renderer_Ptr &renderer;
    SDL_Rect rect;
    SDL_Color color;
    std::multimap<point, formatted_text> &overlay_strings;
    std::string name = empty_string;
    int alpha = 64;
    bool draw_label = true;
};

void draw_zone_overlay( const draw_zone_overlay_options &opt )
{
    SDL_Color color = opt.color;
    color.a = static_cast<Uint8>( opt.alpha );

    constexpr auto flags = sdl_render_state_flags::draw_color | sdl_render_state_flags::blend_mode;
    const auto state = sdl_save_render_state<flags>( opt.renderer.get() );

    SetRenderDrawBlendMode( opt.renderer, SDL_BLENDMODE_BLEND );
    SetRenderDrawColor( opt.renderer, color.r, color.g, color.b, color.a );
    RenderFillRect( opt.renderer, &opt.rect );

    sdl_restore_render_state( opt.renderer.get(), state );

    if( opt.draw_label && !opt.name.empty() ) {
        const point center( opt.rect.x + opt.rect.w / 2, opt.rect.y + opt.rect.h / 2 );
        opt.overlay_strings.emplace( center,
                                     formatted_text( opt.name, catacurses::white, text_alignment::center ) );
    }
}

} // namespace

static int msgtype_to_tilecolor( const game_message_type type, const bool bOldMsg )
{
    const int iBold = bOldMsg ? 0 : 8;

    switch( type ) {
        case m_good:
            return iBold + catacurses::green;
        case m_bad:
            return iBold + catacurses::red;
        case m_mixed:
        case m_headshot:
            return iBold + catacurses::magenta;
        case m_neutral:
            return iBold + catacurses::white;
        case m_warning:
        case m_critical:
            return iBold + catacurses::yellow;
        case m_info:
        case m_grazing:
            return iBold + catacurses::blue;
        default:
            break;
    }

    return -1;
}

formatted_text::formatted_text( const std::string &text, const int color,
                                const direction text_direction )
    : text( text ), color( color )
{
    switch( text_direction ) {
        case direction::NORTHWEST:
        case direction::WEST:
        case direction::SOUTHWEST:
            alignment = text_alignment::right;
            break;
        case direction::NORTH:
        case direction::CENTER:
        case direction::SOUTH:
            alignment = text_alignment::center;
            break;
        default:
            alignment = text_alignment::left;
            break;
    }
}

void idle_animation_manager::prepare_for_redraw()
{
    // Forget about animations from previous frame
    present_ = false;

    if( !enabled_ ) {
        frame = 0;
        return;
    }

    // Use system clock to keep steady frame rate
    auto now = std::chrono::system_clock::now();
    auto now_ms = std::chrono::time_point_cast<std::chrono::milliseconds>( now );
    auto value = now_ms.time_since_epoch();
    // Aiming roughly at the standard 60 frames per second
    frame = value.count() / 17;
}

struct tile_render_info {
    tripoint pos;
    // accumulator for 3d tallness of sprites rendered here so far;
    int height_3d = 0;
    lit_level ll;
    bool invisible[5];
    tile_render_info( const tripoint &pos, const int height_3d, const lit_level ll,
                      const bool( &invisible )[5] )
        : pos( pos ), height_3d( height_3d ), ll( ll ) {
        std::copy( invisible, invisible + 5, this->invisible );
    }
};

cata_tiles::cata_tiles( const SDL_Renderer_Ptr &renderer, const GeometryRenderer_Ptr &geometry ) :
    renderer( renderer ),
    geometry( geometry ),
    minimap( renderer, geometry )
{
    assert( renderer );

    tile_height = 0;
    tile_width = 0;
    tile_ratiox = 0;
    tile_ratioy = 0;

    in_animation = false;
    do_draw_explosion = false;
    do_draw_custom_explosion = false;
    do_draw_bullet = false;
    do_draw_hit = false;
    do_draw_line = false;
    do_draw_cursor = false;
    do_draw_highlight = false;
    do_draw_weather = false;
    do_draw_sct = false;
    do_draw_zones = false;

    nv_goggles_activated = false;

    on_options_changed();
}

cata_tiles::~cata_tiles() = default;

void cata_tiles::on_options_changed()
{
    memory_map_mode = get_option <std::string>( "MEMORY_MAP_MODE" );

    pixel_minimap_settings settings;

    settings.mode = pixel_minimap_mode_from_string( get_option<std::string>( "PIXEL_MINIMAP_MODE" ) );
    settings.brightness = get_option<int>( "PIXEL_MINIMAP_BRIGHTNESS" );
    settings.beacon_size = get_option<int>( "PIXEL_MINIMAP_BEACON_SIZE" );
    settings.beacon_blink_interval = get_option<bool>( "ANIMATIONS" ) ?
                                     get_option<int>( "PIXEL_MINIMAP_BLINK" ) : 0;
    settings.square_pixels = get_option<bool>( "PIXEL_MINIMAP_RATIO" );
    settings.scale_to_fit = get_option<bool>( "PIXEL_MINIMAP_SCALE_TO_FIT" );

    minimap->set_settings( settings );
}

const tile_type *tileset::find_tile_type( const std::string &id ) const
{
    const auto iter = tile_ids.find( id );
    return iter != tile_ids.end() ? &iter->second : nullptr;
}

std::optional<tile_lookup_res>
tileset::find_tile_type_by_season( const std::string &id, season_type season ) const
{
    assert( season < season_type::NUM_SEASONS );
    const auto iter = tile_ids_by_season[season].find( id );

    if( iter == tile_ids_by_season[season].end() ) {
        return std::nullopt;
    }
    auto &res = iter->second;
    if( res.season_tile ) {
        return *res.season_tile;
    } else if( res.default_tile ) { // can skip this check, but just in case
        return tile_lookup_res( iter->first, *res.default_tile );
    }
    debugmsg( "empty record found in `tile_ids_by_season` for key: %s", id );
    return std::nullopt;
}

tile_type &tileset::create_tile_type( const std::string &id, tile_type &&new_tile_type )
{
    // Must overwrite existing tile
    // TODO: c++17 - replace [] + find() with insert_or_assign()
    tile_ids[id] = std::move( new_tile_type );
    auto inserted = tile_ids.find( id );

    const std::string &inserted_id = inserted->first;
    tile_type &inserted_tile = inserted->second;

    // populate cache by season
    constexpr size_t suffix_len = 15;
    constexpr char season_suffix[4][suffix_len] = {
        "_season_spring", "_season_summer", "_season_autumn", "_season_winter"
    };
    bool has_season_suffix = false;
    for( int i = 0; i < 4; i++ ) {
        if( id.ends_with( season_suffix[i] ) ) {
            has_season_suffix = true;
            // key is id without _season suffix
            season_tile_value &value = tile_ids_by_season[i][id.substr( 0,
                                       id.size() - strlen( season_suffix[i] ) )];
            // value stores reference to string id with _season suffix
            value.season_tile = tile_lookup_res( inserted_id, inserted_tile );
            break;
        }
    }
    // tile doesn't have _season suffix, add it as "default" into all four seasons
    if( !has_season_suffix ) {
        for( auto &tile_ids_for_season : tile_ids_by_season ) {
            tile_ids_for_season[id].default_tile = &inserted_tile;
        }
    }

    return inserted_tile;
}

void cata_tiles::load_tileset(
    const std::string &tileset_id,
    const std::vector<mod_id> &mod_list,
    const bool precheck,
    const bool force,
    const bool pump_events
)
{
    if( !force && tileset_ptr &&
        !get_option<bool>( "FORCE_TILESET_RELOAD" ) &&
        tileset_ptr->get_tileset_id() == tileset_id &&
        tileset_mod_list_stamp == mod_list
      ) {
        return;
    }
    // TODO: move into clear or somewhere else.
    // reset the overlay ordering from the previous loaded tileset
    tileset_mutation_overlay_ordering.clear();

    // Load the tileset into a separate instance and only set this->tileset_ptr
    // when the loading has succeeded.
    std::unique_ptr<tileset> new_tileset_ptr = std::make_unique<tileset>();
    tileset_loader loader( *new_tileset_ptr, renderer );
    loader.load( tileset_id, precheck, /*pump_events=*/pump_events );
    tileset_ptr = std::move( new_tileset_ptr );
    tileset_mod_list_stamp = mod_list;

    set_draw_scale( 16 );

    minimap->set_type( tile_iso ? pixel_minimap_type::iso : pixel_minimap_type::ortho );
}

void cata_tiles::reinit()
{
    set_draw_scale( 16 );
    RenderClear( renderer );
}

static void get_tile_information( const std::string &config_path, std::string &json_path,
                                  std::string &tileset_path )
{
    const std::string default_json = PATH_INFO::defaulttilejson();
    const std::string default_tileset = PATH_INFO::defaulttilepng();

    // Get JSON and TILESET vars from config
    const auto reader = [&]( std::istream & fin ) {
        while( !fin.eof() ) {
            std::string sOption;
            fin >> sOption;

            if( sOption.empty() ) {
                getline( fin, sOption );
            } else if( sOption[0] == '#' ) {
                // Skip comment
                getline( fin, sOption );
            } else if( sOption.find( "JSON" ) != std::string::npos ) {
                fin >> json_path;
                dbg( DL::Info ) << "JSON path set to [" << json_path << "].";
            } else if( sOption.find( "TILESET" ) != std::string::npos ) {
                fin >> tileset_path;
                dbg( DL::Info ) << "TILESET path set to [" << tileset_path << "].";
            } else {
                getline( fin, sOption );
            }
        }
    };

    if( !read_from_file( config_path, reader ) ) {
        json_path = default_json;
        tileset_path = default_tileset;
    }

    if( json_path.empty() ) {
        json_path = default_json;
        dbg( DL::Info ) << "JSON set to default [" << json_path << "].";
    }
    if( tileset_path.empty() ) {
        tileset_path = default_tileset;
        dbg( DL::Info ) << "TILESET set to default [" << tileset_path << "].";
    }
}

static size_t get_surface_hash( SDL_Surface *surf, const SDL_Rect *rect )
{

    if( SDL_MUSTLOCK( surf ) ) {
        SDL_LockSurface( surf );
    }

    SDL_Rect rr;
    if( rect == nullptr ) {
        rr = {0, 0, surf->w, surf->h};
        rect = &rr;
    }

    size_t hash = 0;
    cata::hash_combine( hash, rect->w );
    cata::hash_combine( hash, rect->h );

    const int dx = rect->w;
    const int dy = rect->h;

    for( int y = 0; y < dy; ++y ) {
        const auto offset = static_cast<uint32_t>( ( ( y + rect->y ) * surf->w ) + rect->x );
        auto pData = static_cast<uint32_t *>( surf->pixels ) + offset;
        for( int x = 0; x < dx; ++x, ++pData ) {
            cata::hash_combine( hash, *pData );
        }
    }

    if( SDL_MUSTLOCK( surf ) ) {
        SDL_LockSurface( surf );
    }

    return hash;
}

template<bool SkipTransparent = true, typename FilterFn>
static void
apply_color_filter(
    SDL_Surface *dst, const SDL_Rect &dstRect,
    SDL_Surface *src, const SDL_Rect &srcRect,
    FilterFn filter_func )
{
    assert( dst );

    if( SDL_MUSTLOCK( dst ) ) {
        SDL_LockSurface( dst );
    }
    if( SDL_MUSTLOCK( src ) ) {
        SDL_LockSurface( src );
    }

    const int dx = std::min( dstRect.w, srcRect.w );
    const int dy = std::min( dstRect.h, srcRect.h );

    for( int y = 0; y < dy; ++y ) {

        const auto dst_offset = static_cast<uint32_t>( ( ( y + dstRect.y ) * dst->w ) + dstRect.x );
        const auto src_offset = static_cast<uint32_t>( ( ( y + srcRect.y ) * src->w ) + srcRect.x );

        auto pDst = static_cast<SDL_Color *>( dst->pixels ) + dst_offset;
        auto pSrc = static_cast<SDL_Color *>( src->pixels ) + src_offset;

        for( int x = 0; x < dx; ++x, ++pDst, ++pSrc ) {
            if constexpr( SkipTransparent ) {
                if( pSrc->a == 0x00 ) {
                    *pDst = {0, 0, 0, 0};
                }
            }
            *pDst = filter_func( *pSrc );
        }
    }

    if( SDL_MUSTLOCK( dst ) ) {
        SDL_UnlockSurface( dst );
    }
    if( SDL_MUSTLOCK( src ) ) {
        SDL_UnlockSurface( src );
    }
}

template<bool SkipTransparent = true, typename BlendFn>
static void apply_blend_filter(
    SDL_Surface *dst, const SDL_Rect &dstRect,
    SDL_Surface *srcA, const SDL_Rect &srcRectA,
    SDL_Surface *srcB, const SDL_Rect &srcRectB,
    BlendFn blend_func )
{
    assert( dst );
    assert( srcA );
    assert( srcB );

    if( SDL_MUSTLOCK( dst ) ) {
        SDL_LockSurface( dst );
    }
    if( SDL_MUSTLOCK( srcA ) ) {
        SDL_LockSurface( srcA );
    }
    if( SDL_MUSTLOCK( srcB ) ) {
        SDL_LockSurface( srcB );
    }

    const int dx = std::min( dstRect.w, std::min( srcRectA.w, srcRectB.w ) );
    const int dy = std::min( dstRect.h, std::min( srcRectA.h, srcRectB.h ) );

    for( int y = 0; y < dy; ++y ) {

        const auto dst_offset = static_cast<uint32_t>( ( ( y + dstRect.y ) * dst->w ) + dstRect.x );
        const auto srcA_offset = static_cast<uint32_t>( ( ( y + srcRectA.y ) * srcA->w ) + srcRectA.x );
        const auto srcB_offset = static_cast<uint32_t>( ( ( y + srcRectB.y ) * srcB->w ) + srcRectB.x );

        auto pDst = static_cast<SDL_Color *>( dst->pixels ) + dst_offset;
        auto pSrcA = static_cast<SDL_Color *>( srcA->pixels ) + srcA_offset;
        auto pSrcB = static_cast<SDL_Color *>( srcB->pixels ) + srcB_offset;

        for( int x = 0; x < dx; ++x, ++pDst, ++pSrcA, ++pSrcB ) {
            if constexpr( SkipTransparent ) {
                if( pSrcA->a == 0x00 || pSrcB->a == 0x00 ) {
                    *pDst = {0, 0, 0, 0};
                }
            }
            *pDst = blend_func( *pSrcA, *pSrcB );
        }
    }

    if( SDL_MUSTLOCK( dst ) ) {
        SDL_UnlockSurface( dst );
    }
    if( SDL_MUSTLOCK( srcA ) ) {
        SDL_UnlockSurface( srcA );
    }
    if( SDL_MUSTLOCK( srcB ) ) {
        SDL_UnlockSurface( srcB );
    }
}

template<bool SkipTransparent = true, typename FilterFn>
static SDL_Surface_Ptr apply_color_filter_blit_copy(
    const SDL_Surface_Ptr &src, FilterFn filter_func )
{
    assert( src );
    SDL_Surface_Ptr dst = create_surface_32( src->w, src->h );
    assert( dst );
    throwErrorIf(
        SDL_BlitSurface( src.get(), nullptr, dst.get(), nullptr ) != 0,
        "SDL_BlitSurface failed"
    );

    auto pix = static_cast<SDL_Color *>( dst->pixels );

    for( int y = 0, ey = dst->h; y < ey; ++y ) {
        for( int x = 0, ex = dst->w; x < ex; ++x, ++pix ) {
            if constexpr( SkipTransparent ) {
                if( pix->a == 0x00 ) {
                    continue;
                }
            }
            *pix = filter_func( *pix );
        }
    }

    return dst;
}

static bool is_contained( const SDL_Rect &smaller, const SDL_Rect &larger )
{
    return smaller.x >= larger.x &&
           smaller.y >= larger.y &&
           smaller.x + smaller.w <= larger.x + larger.w &&
           smaller.y + smaller.h <= larger.y + larger.h;
}

bool tileset_loader::copy_surface_to_texture( const SDL_Surface_Ptr &surf, const point offset,
        std::vector<texture> &target ) const
{
    assert( surf );
    const rect_range<SDL_Rect> input_range(
        sprite_width, sprite_height,
        point( surf->w / sprite_width, surf->h / sprite_height ) );

    const std::shared_ptr<SDL_Texture> texture_ptr =
        CreateTextureFromSurface( renderer, surf );
    if( !texture_ptr ) {
        return false;
    }

    for( const SDL_Rect rect : input_range ) {
        assert( offset.x % sprite_width == 0 );
        assert( offset.y % sprite_height == 0 );
        const point pos( offset + point( rect.x, rect.y ) );
        assert( pos.x % sprite_width == 0 );
        assert( pos.y % sprite_height == 0 );
        const size_t index =
            this->offset + ( pos.x / sprite_width ) +
            ( pos.y / sprite_height ) * ( tile_atlas_width / sprite_width );
        assert( index < target.size() );
        assert( target[index].dimension() == std::make_pair( 0, 0 ) );
        target[index] = texture( texture_ptr, rect );
    }
    return true;
}

bool tileset_loader::copy_surface_to_dynamic_atlas(
    const SDL_Surface_Ptr &surf, const point offset )
{
#if !defined(DYNAMIC_ATLAS)
    return false;
#else
    assert( surf );
    const rect_range<SDL_Rect> input_range(
        sprite_width,
        sprite_height,
        point( surf->w / sprite_width, surf->h / sprite_height )
    );

    auto [st_tex, st_surf, st_sub_rect] =
        ts.texture_atlas()->get_staging_area( sprite_width, sprite_height );

    SDL_SetSurfaceBlendMode( surf.get(), SDL_BLENDMODE_NONE );

    auto state = sdl_save_render_state( renderer.get() );
    for( const SDL_Rect src_rect : input_range ) {
        assert( offset.x % sprite_width == 0 );
        assert( offset.y % sprite_height == 0 );

        const point pos( offset + point( src_rect.x, src_rect.y ) );
        assert( pos.x % sprite_width == 0 );
        assert( pos.y % sprite_height == 0 );

        const int index =
            this->offset + ( pos.x / sprite_width ) +
            ( pos.y / sprite_height ) * ( tile_atlas_width / sprite_width );

        SDL_FillRect( st_surf, nullptr, SDL_MapRGBA( st_surf->format, 255, 255, 255, 0 ) );
        SDL_BlitSurface( surf.get(), &src_rect, st_surf, &st_sub_rect );

        const auto surf_hash = get_surface_hash( st_surf, nullptr );
        const auto existing = ts.tileset_atlas->id_search( surf_hash );

        atlas_texture atl_tex;
        if( existing.has_value() ) {
            atl_tex = existing.value();
        } else {
            atl_tex = ts.tileset_atlas->allocate_sprite( sprite_width, sprite_height );
            ts.tileset_atlas->id_assign( surf_hash, atl_tex );

            SDL_UpdateTexture( st_tex, nullptr, st_surf->pixels, st_surf->pitch );
            SDL_SetRenderTarget( renderer.get(), atl_tex.first.get() );
            SDL_RenderCopy( renderer.get(), st_tex, &st_sub_rect, &atl_tex.second );
        }

        const auto tex_key = tileset_lookup_key{ index, TILESET_NO_MASK, tileset_fx_type::none, TILESET_NO_COLOR };
        auto& [at_tex, at_rect] = atl_tex;
        ts.tile_lookup.emplace( tex_key, texture( std::move( at_tex ), at_rect ) );
    }
    sdl_restore_render_state( renderer.get(), state );

    return true;
#endif
}

static color_pixel_function_pointer get_pixel_function( const tileset_fx_type &type )
{
    switch( type ) {
        case tileset_fx_type::shadow:
            return get_color_pixel_function( "color_pixel_grayscale" );
            break;
        case tileset_fx_type::night:
            return get_color_pixel_function( "color_pixel_nightvision" );
            break;
        case tileset_fx_type::overexposed:
            return get_color_pixel_function( "color_pixel_overexposed" );
            break;
        case tileset_fx_type::underwater:
            return get_color_pixel_function( "color_pixel_underwater" );
            break;
        case tileset_fx_type::underwater_dark:
            return get_color_pixel_function( "color_pixel_underwater_dark" );
            break;
        case tileset_fx_type::memory:
            return get_color_pixel_function( tilecontext->memory_map_mode );
            break;
        case tileset_fx_type::z_overlay:
            return get_color_pixel_function( "color_pixel_zoverlay" );
            break;
        default:
            return get_color_pixel_function( "color_pixel_copy" );
            break;
    }
}

template<typename T, typename U, U max_t = std::numeric_limits<U>::max()>
static T ilerp( const T a, const T b, const U t )
{
    return ( ( b * t ) + ( a * ( max_t - t ) ) ) / max_t;
};

static void apply_surf_blend_effect(
    SDL_Surface *staging, const SDL_Color &color, const bool use_mask,
    const SDL_Rect &dstRect, const SDL_Rect &srcRect, const SDL_Rect &maskRect )
{
    ZoneScoped;

    const HSVColor dest_hsv = rgb2hsv( color );
    constexpr auto overlay = []( const uint8_t lower, const uint8_t upper ) -> uint8_t {
        if( lower > 127 )
        {
            const auto u = ( 255 - lower ) * 255 / 127;
            const auto m = lower - ( 255 - lower );
            const auto o = ( upper * u / 255 ) + m;
            return std::clamp<uint8_t>( o, 0, 255 );
        } else
        {
            const auto u = ( lower * 255 / 127 );
            const auto o = upper * u / 255;
            return std::clamp<uint8_t>( o, 0, 255 );
        }
    };
    if( use_mask ) {
        auto effect_mask = [&]( const SDL_Color & base_rgb, const SDL_Color & mask_rgb )  -> SDL_Color {
            HSVColor base_hsv = rgb2hsv( base_rgb );
            base_hsv.H = dest_hsv.H;
            base_hsv.S = ilerp<uint16_t>( std::min( base_hsv.S, dest_hsv.S ), dest_hsv.S, mask_rgb.g );
            base_hsv.V = ilerp( base_hsv.V, overlay( base_hsv.V, dest_hsv.V ), mask_rgb.b );

            RGBColor res = hsv2rgb( base_hsv );
            res.r = ilerp( base_rgb.r, res.r, mask_rgb.r );
            res.g = ilerp( base_rgb.g, res.g, mask_rgb.r );
            res.b = ilerp( base_rgb.b, res.b, mask_rgb.r );
            return res;
        };
        apply_blend_filter(
            staging, dstRect,
            staging, srcRect,
            staging, maskRect,
            effect_mask
        );
    } else {
        auto effect_no_mask = [&]( const SDL_Color & c )  -> SDL_Color {
            HSVColor base_hsv = rgb2hsv( c );
            base_hsv.H = dest_hsv.H;
            base_hsv.S = ilerp<uint16_t, uint8_t>( std::min( base_hsv.S, dest_hsv.S ), dest_hsv.S, 127 );
            base_hsv.V = ilerp<uint16_t, uint8_t>( base_hsv.V, overlay( base_hsv.V, dest_hsv.V ), 127 );
            return hsv2rgb( base_hsv );
        };
        apply_color_filter(
            staging, dstRect,
            staging, srcRect,
            effect_no_mask
        );
    }
}

const texture *tileset::get_or_default( const int sprite_index,
                                        const int mask_index,
                                        const tileset_fx_type &type,
                                        const SDL_Color &color ) const
{
    ZoneScoped;

#if defined(DYNAMIC_ATLAS)

    const auto base_tex_key = tileset_lookup_key{ sprite_index, TILESET_NO_MASK, tileset_fx_type::none, TILESET_NO_COLOR };
    const auto mask_tex_key = tileset_lookup_key{ mask_index, TILESET_NO_MASK, tileset_fx_type::none, TILESET_NO_COLOR };
    const auto mod_tex_key = tileset_lookup_key{ sprite_index, mask_index, type, color };

    if( g->display_overlay_state( ACTION_DISPLAY_TILES_NO_VFX ) ) {
        const auto base_tex_it = tile_lookup.find( base_tex_key );
        return base_tex_it == tile_lookup.end()
               ? nullptr
               : &base_tex_it->second;
    }

    const auto mod_tex_it = tile_lookup.find( mod_tex_key );
    if( mod_tex_it != tile_lookup.end() ) {
        return &mod_tex_it->second;
    }

    const auto base_tex_it = tile_lookup.find( base_tex_key );
    if( base_tex_it == tile_lookup.end() ) {
        return nullptr;
    }

    const auto mask_tex_it = tile_lookup.find( mask_tex_key );

    const color_pixel_function_pointer vfx_func = get_pixel_function( type );
    if( !vfx_func ) {
        debugmsg( "Error loading visual effect function" );
    }

    {
        ZoneScoped;

        const auto &r = get_sdl_renderer();
        const auto rp = r.get();

        const texture &base_tex = base_tex_it->second;
        const texture *mask_tex = ( mask_tex_it != tile_lookup.end() ) ? &mask_tex_it->second : nullptr;

        const auto [spr_w, spr_h] = base_tex.dimension();
        const auto [st_tex, st_surf, st_sub_rect ] =
            texture_atlas()->get_staging_area( spr_w * 2, spr_h * 2 );

        const auto st_sub_rect_source = SDL_Rect{ st_sub_rect.x + 0, st_sub_rect.y + 0, spr_w, spr_h };
        const auto st_sub_rect_mask = SDL_Rect{ st_sub_rect.x + spr_w, st_sub_rect.y + 0, spr_w, spr_h };
        const auto st_sub_rect_tinted = SDL_Rect{ st_sub_rect.x + 0, st_sub_rect.y + spr_h, spr_w, spr_h };
        const auto st_sub_rect_final = SDL_Rect{ st_sub_rect.x + spr_w, st_sub_rect.y + spr_h, spr_w, spr_h };

        const auto state = sdl_save_render_state( rp );

        SDL_SetRenderTarget( rp, st_tex );
        SetRenderDrawColor( r, 255, 0, 255, 255 );
        SDL_RenderClear( rp );

        base_tex.set_blend_mode( SDL_BLENDMODE_NONE );
        base_tex.render_copy( r, &st_sub_rect_source );
        base_tex.set_blend_mode( SDL_BLENDMODE_BLEND );

        if( mask_tex ) {
            mask_tex->set_blend_mode( SDL_BLENDMODE_NONE );
            mask_tex->render_copy( r, &st_sub_rect_mask );
            mask_tex->set_blend_mode( SDL_BLENDMODE_BLEND );
        }

        SDL_RenderReadPixels( rp, nullptr, st_surf->format->format, st_surf->pixels, st_surf->pitch );

        if( color == TILESET_NO_COLOR ) {
            apply_color_filter( st_surf, st_sub_rect_tinted, st_surf, st_sub_rect_source, color_pixel_copy );
        } else {
            apply_surf_blend_effect( st_surf, color, mask_tex, st_sub_rect_tinted,
                                     st_sub_rect_source, st_sub_rect_mask );
        }

        apply_color_filter( st_surf, st_sub_rect_final,  st_surf, st_sub_rect_tinted, vfx_func );

        auto surf_hash = get_surface_hash( st_surf, &st_sub_rect_final );
        auto existing = tileset_atlas->id_search( surf_hash );

        atlas_texture atl_tex;
        if( existing.has_value() ) {
            atl_tex = std::move( existing.value() );
        } else {
            atl_tex = tileset_atlas->allocate_sprite( spr_w, spr_h );
            tileset_atlas->id_assign( surf_hash, atl_tex );

            SDL_UpdateTexture( st_tex, nullptr, st_surf->pixels, st_surf->pitch );
            SDL_SetRenderTarget( rp, atl_tex.first.get() );
            SDL_RenderCopy( rp, st_tex, &st_sub_rect_final, &atl_tex.second );
        }

        sdl_restore_render_state( rp, state );
        auto& [at_tex, at_rect] = atl_tex;
        auto [entry, ok] = tile_lookup.emplace( mod_tex_key, texture( std::move( at_tex ), at_rect ) );
        return &entry->second;
    }
#else
    if( sprite_index >= tile_values.size() ) {
        return nullptr;
    }

    switch( type ) {
        case tileset_fx_type::shadow:
            return &shadow_tile_values[sprite_index];
            break;
        case tileset_fx_type::night:
            return &night_tile_values[sprite_index];
            break;
        case tileset_fx_type::overexposed:
            return &overexposed_tile_values[sprite_index];
            break;
        case tileset_fx_type::underwater:
            return &underwater_tile_values[sprite_index];
            break;
        case tileset_fx_type::underwater_dark:
            return &underwater_dark_tile_values[sprite_index];
            break;
        case tileset_fx_type::memory:
            return &memory_tile_values[sprite_index];
            break;
        case tileset_fx_type::z_overlay:
            return &z_overlay_values[sprite_index];
            break;
        default:
            return &tile_values[sprite_index];
            break;
    }
#endif
}

bool tileset_loader::create_textures_from_tile_atlas( const SDL_Surface_Ptr &tile_atlas,
        point offset )
{
    assert( tile_atlas );

#if defined(DYNAMIC_ATLAS)
    return copy_surface_to_dynamic_atlas( tile_atlas, offset );
#else

    /** perform color filter conversion here */
    using tiles_pixel_color_entry = std::tuple<std::vector<texture>*, std::string>;
    std::array<tiles_pixel_color_entry, 8> tile_values_data = {{
            { std::make_tuple( &ts.tile_values, "color_pixel_none" ) },
            { std::make_tuple( &ts.shadow_tile_values, "color_pixel_grayscale" ) },
            { std::make_tuple( &ts.night_tile_values, "color_pixel_nightvision" ) },
            { std::make_tuple( &ts.overexposed_tile_values, "color_pixel_overexposed" ) },
            { std::make_tuple( &ts.underwater_tile_values, "color_pixel_underwater" ) },
            { std::make_tuple( &ts.underwater_dark_tile_values, "color_pixel_underwater_dark" ) },
            { std::make_tuple( &ts.z_overlay_values, "color_pixel_zoverlay" ) },
            { std::make_tuple( &ts.memory_tile_values, tilecontext->memory_map_mode ) }
        }
    };
    for( tiles_pixel_color_entry &entry : tile_values_data ) {
        std::vector<texture> *tile_values = std::get<0>( entry );
        color_pixel_function_pointer color_pixel_function = get_color_pixel_function( std::get<1>
                ( entry ) );
        bool success;
        if( !color_pixel_function ) {
            // TODO: Move it inside apply_color_filter.
            success = copy_surface_to_texture( tile_atlas, offset, *tile_values );
        } else {
            success = copy_surface_to_texture( apply_color_filter_blit_copy( tile_atlas, color_pixel_function ),
                                               offset,
                                               *tile_values );
        }
        if( !success ) {
            return false;
        }
    }
    return true;
#endif
}

template<typename T>
static void extend_vector_by( std::vector<T> &vec, const size_t additional_size )
{
    vec.resize( vec.size() + additional_size );
}

void tileset_loader::load_tileset( const std::string &img_path, const bool pump_events )
{
    const SDL_Surface_Ptr tile_atlas = load_image( img_path.c_str() );
    assert( tile_atlas );
    tile_atlas_width = tile_atlas->w;

    if( R >= 0 && R <= 255 && G >= 0 && G <= 255 && B >= 0 && B <= 255 ) {
        const Uint32 key = SDL_MapRGB( tile_atlas->format, 0, 0, 0 );
        throwErrorIf( SDL_SetColorKey( tile_atlas.get(), SDL_TRUE, key ) != 0, "SDL_SetColorKey failed" );
        throwErrorIf( SDL_SetSurfaceRLE( tile_atlas.get(), 1 ), "SDL_SetSurfaceRLE failed" );
    }

    SDL_RendererInfo info;
    throwErrorIf( SDL_GetRendererInfo( renderer.get(), &info ) != 0, "SDL_GetRendererInfo failed" );
    // Software rendering stores textures as surfaces with run-length encoding, which makes extracting a part
    // in the middle of the texture slow. Therefore this "simulates" that the renderer only supports one tile
    // per texture. Each tile will go on its own texture object.
    if( info.flags & SDL_RENDERER_SOFTWARE ) {
        info.max_texture_width = sprite_width;
        info.max_texture_height = sprite_height;
    }
    // for debugging only: force a very small maximal texture size, as to trigger
    // splitting the tile atlas.
#if 0
    // +1 to check correct rounding
    info.max_texture_width = sprite_width * 10 + 1;
    info.max_texture_height = sprite_height * 20 + 1;
#endif

    const int min_tile_xcount = 128;
    const int min_tile_ycount = min_tile_xcount * 2;

    if( info.max_texture_width == 0 ) {
        info.max_texture_width = sprite_width * min_tile_xcount;
        dbg( DL::Info ) <<
                        "SDL_RendererInfo max_texture_width was set to 0.  Changing it to " <<
                        info.max_texture_width;
    } else {
        throwErrorIf( info.max_texture_width < sprite_width,
                      "Maximal texture width is smaller than tile width" );
    }

    if( info.max_texture_height == 0 ) {
        info.max_texture_height = sprite_height * min_tile_ycount;
        dbg( DL::Info ) <<
                        "SDL_RendererInfo max_texture_height was set to 0.  Changing it to "
                        << info.max_texture_height;
    } else {
        throwErrorIf( info.max_texture_height < sprite_height,
                      "Maximal texture height is smaller than tile height" );
    }

    // Number of tiles in each dimension that fits into a (maximal) SDL texture.
    // If the tile atlas contains more than that, we have to split it.
    const int max_tile_xcount = info.max_texture_width / sprite_width;
    const int max_tile_ycount = info.max_texture_height / sprite_height;
    // Range over the tile atlas, wherein each rectangle fits into the maximal
    // SDL texture size. In other words: a range over the parts into which the
    // tile atlas needs to be split.
    const rect_range<SDL_Rect> output_range(
        max_tile_xcount * sprite_width,
        max_tile_ycount * sprite_height,
        point( divide_round_up( tile_atlas->w, info.max_texture_width ), divide_round_up( tile_atlas->h,
                info.max_texture_height ) ) );

    const int expected_tilecount = ( tile_atlas->w / sprite_width ) * ( tile_atlas->h / sprite_height );

#if !defined(DYNAMIC_ATLAS)
    extend_vector_by( ts.tile_values, expected_tilecount );
    extend_vector_by( ts.shadow_tile_values, expected_tilecount );
    extend_vector_by( ts.night_tile_values, expected_tilecount );
    extend_vector_by( ts.overexposed_tile_values, expected_tilecount );
    extend_vector_by( ts.underwater_tile_values, expected_tilecount );
    extend_vector_by( ts.underwater_dark_tile_values, expected_tilecount );
    extend_vector_by( ts.z_overlay_values, expected_tilecount );
    extend_vector_by( ts.memory_tile_values, expected_tilecount );
#endif

    for( const SDL_Rect sub_rect : output_range ) {
        assert( sub_rect.x % sprite_width == 0 );
        assert( sub_rect.y % sprite_height == 0 );
        assert( sub_rect.w % sprite_width == 0 );
        assert( sub_rect.h % sprite_height == 0 );
        SDL_Surface_Ptr smaller_surf;

        if( is_contained( SDL_Rect{ 0, 0, tile_atlas->w, tile_atlas->h }, sub_rect ) ) {
            // can use tile_atlas directly, it is completely contained in the output rectangle
        } else {
            // Need a temporary surface that contains the parts of the tile atlas that fit
            // into sub_rect. But doesn't always need to be as large as sub_rect.
            const int w = std::min( tile_atlas->w - sub_rect.x, sub_rect.w );
            const int h = std::min( tile_atlas->h - sub_rect.y, sub_rect.h );
            smaller_surf = ::create_surface_32( w, h );
            assert( smaller_surf );
            const SDL_Rect inp{ sub_rect.x, sub_rect.y, w, h };
            throwErrorIf( SDL_BlitSurface( tile_atlas.get(), &inp, smaller_surf.get(), nullptr ) != 0,
                          "SDL_BlitSurface failed" );
        }
        const SDL_Surface_Ptr &surf_to_use = smaller_surf ? smaller_surf : tile_atlas;
        assert( surf_to_use );

        if( !create_textures_from_tile_atlas( surf_to_use, point( sub_rect.x, sub_rect.y ) ) ) {
            // May happen on some systems - there's nothing we can do about it
            throw std::runtime_error(
                _(
                    "Failed to create texture atlas, see debug.log for details.  "
                    "This commonly happens if the device is low on memory.  "
                    "Try rebooting device, or using another tileset or a different renderer."
                )
            );
        }

        if( pump_events ) {
            inp_mngr.pump_events();
        }
    }

    size = expected_tilecount;
}

void cata_tiles::set_draw_scale( float scale )
{
    assert( tileset_ptr );
    tile_width = tileset_ptr->get_tile_width() * tileset_ptr->get_tile_pixelscale() * scale / 16;
    tile_height = tileset_ptr->get_tile_height() * tileset_ptr->get_tile_pixelscale() * scale / 16;

    tile_ratiox = ( static_cast<float>( tile_width ) / static_cast<float>( fontwidth ) );
    tile_ratioy = ( static_cast<float>( tile_height ) / static_cast<float>( fontheight ) );
}

std::optional<tile_search_result> cata_tiles::tile_type_search( const tile_search_params &tile )
{
    auto [id, category, subcategory, subtile, rota] = tile;
    std::optional<tile_lookup_res> res = find_tile_looks_like( id, category );
    const tile_type *tt = nullptr;
    if( res ) {
        tt = &( res->tile() );
    }
    const std::string &found_id = res ? ( res->id() ) : id;

    if( !tt ) {
        uint32_t sym = UNKNOWN_UNICODE;
        nc_color col = c_white;
        if( category == C_FURNITURE ) {
            const furn_str_id fid( found_id );
            if( fid.is_valid() ) {
                const furn_t &f = fid.obj();
                sym = f.symbol();
                col = f.color();
            }
        } else if( category == C_TERRAIN ) {
            const ter_str_id tid( found_id );
            if( tid.is_valid() ) {
                const ter_t &t = tid.obj();
                sym = t.symbol();
                col = t.color();
            }
        } else if( category == C_MONSTER ) {
            const mtype_id mid( found_id );
            if( mid.is_valid() ) {
                const mtype &mt = mid.obj();
                sym = UTF8_getch( mt.sym );
                col = mt.color;
            }
        } else if( category == C_VEHICLE_PART ) {
            const vpart_id vpid( found_id.substr( 3 ) );
            if( vpid.is_valid() ) {
                const vpart_info &v = vpid.obj();

                if( subtile == open_ ) {
                    sym = '\'';
                } else if( subtile == broken ) {
                    sym = v.sym_broken;
                } else {
                    sym = v.sym;
                }
                subtile = -1;

                tileray face = tileray( units::from_degrees( rota ) );
                sym = special_symbol( face.dir_symbol( sym ) );
                rota = 0;

                col = v.color;
            }
        } else if( category == C_FIELD ) {
            const field_type_id fid = field_type_id( found_id );
            sym = fid.obj().get_codepoint();
            // TODO: field intensity?
            col = fid.obj().get_color();
        } else if( category == C_TRAP ) {
            const trap_str_id tmp( found_id );
            if( tmp.is_valid() ) {
                const trap &t = tmp.obj();
                sym = t.sym;
                col = t.color;
            }
        } else if( category == C_ITEM ) {
            //TODO!: push this up, it's a bad one
            item *tmp;
            if( found_id.starts_with( "corpse_" ) ) {
                tmp = item::spawn_temporary( itype_corpse, calendar::start_of_cataclysm );
            } else {
                tmp = item::spawn_temporary( found_id, calendar::start_of_cataclysm );
            }
            sym = tmp->symbol().empty() ? ' ' : tmp->symbol().front();
            col = tmp->color();
        } else if( category == C_OVERMAP_TERRAIN ) {
            const oter_type_str_id tmp( id );
            if( tmp.is_valid() ) {
                sym = tmp->symbol;
                col = tmp->color;
            }
        } else if( category == C_OVERMAP_NOTE ) {
            sym = id[5];
            col = color_from_string( id.substr( 7, id.length() - 1 ) );
        } else if( category == C_BULLET ) {
            static const auto default_bullet = std::string{ "animation_bullet_normal_0deg" };
            auto res = find_tile_with_season( default_bullet );
            if( res ) {
                return tile_search_result{ .tt = &res->tile(), .found_id = res->id() };
            }
        }
        // Special cases for walls
        switch( sym ) {
            case LINE_XOXO:
            case LINE_XOXO_UNICODE:
                sym = LINE_XOXO_C;
                break;
            case LINE_OXOX:
            case LINE_OXOX_UNICODE:
                sym = LINE_OXOX_C;
                break;
            case LINE_XXOO:
            case LINE_XXOO_UNICODE:
                sym = LINE_XXOO_C;
                break;
            case LINE_OXXO:
            case LINE_OXXO_UNICODE:
                sym = LINE_OXXO_C;
                break;
            case LINE_OOXX:
            case LINE_OOXX_UNICODE:
                sym = LINE_OOXX_C;
                break;
            case LINE_XOOX:
            case LINE_XOOX_UNICODE:
                sym = LINE_XOOX_C;
                break;
            case LINE_XXXO:
            case LINE_XXXO_UNICODE:
                sym = LINE_XXXO_C;
                break;
            case LINE_XXOX:
            case LINE_XXOX_UNICODE:
                sym = LINE_XXOX_C;
                break;
            case LINE_XOXX:
            case LINE_XOXX_UNICODE:
                sym = LINE_XOXX_C;
                break;
            case LINE_OXXX:
            case LINE_OXXX_UNICODE:
                sym = LINE_OXXX_C;
                break;
            case LINE_XXXX:
            case LINE_XXXX_UNICODE:
                sym = LINE_XXXX_C;
                break;
            default:
                // sym goes unchanged
                break;
        }

        if( sym != 0 && sym < 256 ) {
            // see cursesport.cpp, function wattron
            const int pairNumber = col.to_color_pair_index();
            const cata_cursesport::pairs &colorpair = cata_cursesport::colorpairs[pairNumber];
            // What about isBlink?
            const bool isBold = col.is_bold();
            const int FG = colorpair.FG + ( isBold ? 8 : 0 );
            std::string generic_id = get_ascii_tile_id( sym, FG, -1 );

            // do not rotate fallback tiles!
            if( sym != LINE_XOXO_C && sym != LINE_OXOX_C ) {
                rota = 0;
            }
            if( tileset_ptr->find_tile_type( generic_id ) ) {
                return tile_type_search( {generic_id, C_NONE, subcategory, subtile, rota} );
            }
            // Try again without color this time (using default color).
            generic_id = get_ascii_tile_id( sym, -1, -1 );
            if( tileset_ptr->find_tile_type( generic_id ) ) {
                return tile_type_search( {generic_id, C_NONE, subcategory, subtile, rota} );
            }
        }
    }

    // if id is not found, try to find a tile for the category+subcategory combination
    if( !tt ) {
        const std::string &category_id = TILE_CATEGORY_IDS[category];
        if( !category_id.empty() && !subcategory.empty() ) {
            tt = tileset_ptr->find_tile_type( "unknown_" + category_id + "_" + subcategory );
        }
    }

    // if at this point we have no tile, try just the category
    if( !tt ) {
        const std::string &category_id = TILE_CATEGORY_IDS[category];
        if( !category_id.empty() ) {
            tt = tileset_ptr->find_tile_type( "unknown_" + category_id );
        }
    }

    // if we still have no tile, we're out of luck, fall back to unknown
    if( !tt ) {
        tt = tileset_ptr->find_tile_type( "unknown" );
    }

    //  this really shouldn't happen, but the tileset creator might have forgotten to define an unknown tile
    if( !tt ) {
        return std::nullopt;
    }

    return std::optional{tile_search_result{tt, found_id}};
}

void tileset_loader::load( const std::string &tileset_id, const bool precheck,
                           const bool pump_events )
{
    std::string json_conf;
    std::string tileset_path;
    std::string tileset_root;

    const auto tset_iter = TILESETS.find( tileset_id );
    if( tset_iter != TILESETS.end() ) {
        tileset_root = tset_iter->second;
        dbg( DL::Info ) << '"' << tileset_id << '"' << " tileset: found config file path: " << tileset_root;
        get_tile_information( tileset_root + '/' + PATH_INFO::tileset_conf(),
                              json_conf, tileset_path );
        dbg( DL::Info ) << "Current tileset is: " << tileset_id;
    } else {
        dbg( DL::Error ) << "Tileset \"" << tileset_id << "\" from options is invalid";
        json_conf = PATH_INFO::defaulttilejson();
        tileset_path = PATH_INFO::defaulttilepng();
    }

    std::string json_path = tileset_root + '/' + json_conf;
    std::string img_path = tileset_root + '/' + tileset_path;

    dbg( DL::Info ) << "Attempting to Load JSON file " << json_path;
    std::ifstream config_file( json_path.c_str(), std::ifstream::in | std::ifstream::binary );

    if( !config_file.good() ) {
        throw std::runtime_error( std::string( "Failed to open tile info json: " ) + json_path );
    }

    JsonIn config_json( config_file );
    JsonObject config = config_json.get_object();

    // "tile_info" section must exist.
    if( !config.has_member( "tile_info" ) ) {
        config.throw_error( "\"tile_info\" missing" );
    }

    for( const JsonObject &curr_info : config.get_array( "tile_info" ) ) {
        ts.tile_height = curr_info.get_int( "height" );
        ts.tile_width = curr_info.get_int( "width" );
        tile_iso = curr_info.get_bool( "iso", false );
        ts.tile_pixelscale = curr_info.get_float( "pixelscale", 1.0f );
    }

    if( precheck ) {
        config.allow_omitted_members();
        return;
    }
#if defined(DYNAMIC_ATLAS)
    ts.tileset_atlas = std::make_unique<dynamic_atlas>( 4096, 4096, ts.tile_width, ts.tile_height );
#endif
    // Load tile information if available.
    offset = 0;
    load_internal( config, tileset_root, img_path, pump_events );

    // Load mod tilesets if available
    for( const mod_tileset &mts : all_mod_tilesets ) {
        // Set sprite_id offset to separate from other tilesets.
        sprite_id_offset = offset;
        tileset_root = mts.get_base_path();
        json_path = mts.get_full_path();

        if( !mts.is_compatible( tileset_id ) ) {
            dbg( DL::Info ) << "Mod tileset in \"" << json_path << "\" is not compatible.";
            continue;
        }
        dbg( DL::Info ) << "Attempting to Load JSON file " << json_path;
        std::ifstream mod_config_file( json_path.c_str(), std::ifstream::in | std::ifstream::binary );

        if( !mod_config_file.good() ) {
            throw std::runtime_error( std::string( "Failed to open tile info json: " ) + json_path );
        }

        JsonIn mod_config_json( mod_config_file );

        const auto mark_visited = []( const JsonObject & jobj ) {
            // These fields have been visited in load_mod_tileset
            jobj.get_string_array( "compatibility" );
        };

        int num_in_file = 1;
        if( mod_config_json.test_array() ) {
            for( const JsonObject &mod_config : mod_config_json.get_array() ) {
                if( mod_config.get_string( "type" ) == "mod_tileset" ) {
                    if( num_in_file == mts.num_in_file() ) {
                        mark_visited( mod_config );
                        load_internal( mod_config, tileset_root, img_path, pump_events );
                        break;
                    }
                    num_in_file++;
                }
                mod_config.allow_omitted_members();
            }
        } else {
            JsonObject mod_config = mod_config_json.get_object();
            mark_visited( mod_config );
            load_internal( mod_config, tileset_root, img_path, pump_events );
        }
    }

    // loop through all tile ids and eliminate empty/invalid things
    for( auto it = ts.tile_ids.begin(); it != ts.tile_ids.end(); ) {
        // second is the tile_type describing that id
        auto &td = it->second;
        process_variations_after_loading( td.sprite.fg );
        process_variations_after_loading( td.sprite.bg );
        // All tiles need at least foreground or background data, otherwise they are useless.
        if( td.sprite.bg.empty() && td.sprite.fg.empty() ) {
            dbg( DL::Warn ) << "tile " << it->first << " has no (valid) foreground nor background";
            // remove the id from seasonal variations!
            for( auto &container : ts.tile_ids_by_season ) {
                if( container.contains( it->first ) ) {
                    container.erase( it->first );
                }
            }
            // remove id from full list of ids
            ts.tile_ids.erase( it++ );
        } else {
            ++it;
        }
    }

    if( !ts.find_tile_type( "unknown" ) ) {
        dbg( DL::Warn ) << "The tileset you're using has no 'unknown' tile defined!";
    }
    ensure_default_item_highlight();

    ts.tileset_id = tileset_id;
#if defined(DYNAMIC_ATLAS)
    ts.tileset_atlas->readback_load();
#endif
}

void tileset_loader::load_internal( const JsonObject &config, const std::string &tileset_root,
                                    const std::string &img_path, const bool pump_events )
{
    if( config.has_array( "tiles-new" ) ) {
        // new system, several entries
        // When loading multiple tileset images this defines where
        // the tiles from the most recently loaded image start from.
        for( const JsonObject &tile_part_def : config.get_array( "tiles-new" ) ) {
            const std::string tileset_image_path = tileset_root + '/' + tile_part_def.get_string( "file" );
            R = -1;
            G = -1;
            B = -1;
            if( tile_part_def.has_object( "transparency" ) ) {
                JsonObject tra = tile_part_def.get_object( "transparency" );
                R = tra.get_int( "R" );
                G = tra.get_int( "G" );
                B = tra.get_int( "B" );
            }
            sprite_width = tile_part_def.get_int( "sprite_width", ts.tile_width );
            sprite_height = tile_part_def.get_int( "sprite_height", ts.tile_height );
            // Now load the tile definitions for the loaded tileset image.
            sprite_offset.x = tile_part_def.get_int( "sprite_offset_x", 0 );
            sprite_offset.y = tile_part_def.get_int( "sprite_offset_y", 0 );
            // First load the tileset image to get the number of available tiles.
            dbg( DL::Info ) << "Attempting to Load Tileset file " << tileset_image_path;
            load_tileset( tileset_image_path, pump_events );
            load_tilejson_from_file( tile_part_def );
            if( tile_part_def.has_member( "ascii" ) ) {
                load_ascii( tile_part_def );
            }
            // Make sure the tile definitions of the next tileset image don't
            // override the current ones.
            offset += size;
            if( pump_events ) {
                inp_mngr.pump_events();
            }
        }
    } else {
        sprite_width = ts.tile_width;
        sprite_height = ts.tile_height;
        sprite_offset = point_zero;
        R = -1;
        G = -1;
        B = -1;
        // old system, no tile file path entry, only one array of tiles
        dbg( DL::Info ) << "Attempting to Load Tileset file " << img_path;
        load_tileset( img_path, pump_events );
        load_tilejson_from_file( config );
        offset = size;
    }

    // allows a tileset to override the order of mutation images being applied to a character
    if( config.has_array( "overlay_ordering" ) ) {
        load_overlay_ordering_into_array( config, tileset_mutation_overlay_ordering );
    }

    // offset should be the total number of sprites loaded from every tileset image
    // eliminate any sprite references that are too high to exist
    // also eliminate negative sprite references
}

void tileset_loader::process_variations_after_loading( weighted_int_list<std::vector<int>> &vs )
{
    // loop through all of the variations
    for( auto &v : vs ) {
        // in a given variation, erase any invalid sprite ids
        v.obj.erase(
            std::remove_if(
                v.obj.begin(),
                v.obj.end(),
        [&]( int id ) {
            return id >= offset || id < 0;
        } ),
        v.obj.end()
        );
    }
    // erase any variations with no valid sprite ids left
    vs.erase(
        std::remove_if(
            vs.begin(),
            vs.end(),
    [&]( const weighted_object<int, std::vector<int>> &o ) {
        return o.obj.empty();
    }
        ),
    vs.end()
    );
    // populate the bookkeeping table used for selecting sprite variations
    vs.precalc();
}

void tileset_loader::add_ascii_subtile( tile_type &curr_tile, const std::string &t_id,
                                        int sprite_id,
                                        const std::string &s_id )
{
    const std::string m_id = t_id + "_" + s_id;
    tile_type curr_subtile;
    curr_subtile.sprite.fg.add( std::vector<int>( {sprite_id} ), 1 );
    curr_subtile.masks.tint.fg.add( std::vector<int>( {TILESET_NO_MASK} ), 1 );
    curr_subtile.rotates = true;
    curr_tile.available_subtiles.push_back( s_id );
    ts.create_tile_type( m_id, std::move( curr_subtile ) );
}

void tileset_loader::load_ascii( const JsonObject &config )
{
    if( !config.has_member( "ascii" ) ) {
        config.throw_error( "\"ascii\" section missing" );
    }
    for( const JsonObject &entry : config.get_array( "ascii" ) ) {
        load_ascii_set( entry );
    }
}

void tileset_loader::load_ascii_set( const JsonObject &entry )
{
    // tile for ASCII char 0 is at `in_image_offset`,
    // the other ASCII chars follow from there.
    const int in_image_offset = entry.get_int( "offset" );
    if( in_image_offset >= size ) {
        entry.throw_error( "invalid offset (out of range)", "offset" );
    }
    // color, of the ASCII char. Can be -1 to indicate all/default colors.
    int FG = -1;
    const std::string scolor = entry.get_string( "color", "DEFAULT" );
    if( scolor == "BLACK" ) {
        FG = catacurses::black;
    } else if( scolor == "RED" ) {
        FG = catacurses::red;
    } else if( scolor == "GREEN" ) {
        FG = catacurses::green;
    } else if( scolor == "YELLOW" ) {
        FG = catacurses::yellow;
    } else if( scolor == "BLUE" ) {
        FG = catacurses::blue;
    } else if( scolor == "MAGENTA" ) {
        FG = catacurses::magenta;
    } else if( scolor == "CYAN" ) {
        FG = catacurses::cyan;
    } else if( scolor == "WHITE" ) {
        FG = catacurses::white;
    } else if( scolor == "DEFAULT" ) {
        FG = -1;
    } else {
        entry.throw_error( "invalid color for ASCII", "color" );
    }
    // Add an offset for bold colors (ncurses has this bold attribute,
    // this mimics it). bold does not apply to default color.
    if( FG != -1 && entry.get_bool( "bold", false ) ) {
        FG += 8;
    }
    const int base_offset = offset + in_image_offset;
    // Finally load all 256 ASCII chars (actually extended ASCII)
    for( int ascii_char = 0; ascii_char < 256; ascii_char++ ) {
        const int index_in_image = ascii_char + in_image_offset;
        if( index_in_image < 0 || index_in_image >= size ) {
            // Out of range is ignored for now.
            continue;
        }
        const std::string id = get_ascii_tile_id( ascii_char, FG, -1 );
        tile_type curr_tile;
        curr_tile.offset = sprite_offset;
        curr_tile.masks.tint.fg.add( std::vector<int>( {TILESET_NO_MASK} ), 1 );
        auto &sprites = *( curr_tile.sprite.fg.add( std::vector<int>( {index_in_image + offset} ), 1 ) );
        switch( ascii_char ) {
            // box bottom/top side (horizontal line)
            case LINE_OXOX_C:
                sprites[0] = 205 + base_offset;
                break;
            // box left/right side (vertical line)
            case LINE_XOXO_C:
                sprites[0] = 186 + base_offset;
                break;
            // box top left
            case LINE_OXXO_C:
                sprites[0] = 201 + base_offset;
                break;
            // box top right
            case LINE_OOXX_C:
                sprites[0] = 187 + base_offset;
                break;
            // box bottom right
            case LINE_XOOX_C:
                sprites[0] = 188 + base_offset;
                break;
            // box bottom left
            case LINE_XXOO_C:
                sprites[0] = 200 + base_offset;
                break;
            // box bottom north T (left, right, up)
            case LINE_XXOX_C:
                sprites[0] = 202 + base_offset;
                break;
            // box bottom east T (up, right, down)
            case LINE_XXXO_C:
                sprites[0] = 208 + base_offset;
                break;
            // box bottom south T (left, right, down)
            case LINE_OXXX_C:
                sprites[0] = 203 + base_offset;
                break;
            // box X (left down up right)
            case LINE_XXXX_C:
                sprites[0] = 206 + base_offset;
                break;
            // box bottom east T (left, down, up)
            case LINE_XOXX_C:
                sprites[0] = 184 + base_offset;
                break;
        }
        if( ascii_char == LINE_XOXO_C || ascii_char == LINE_OXOX_C ) {
            curr_tile.rotates = false;
            curr_tile.multitile = true;
            add_ascii_subtile( curr_tile, id, 206 + base_offset, "center" );
            add_ascii_subtile( curr_tile, id, 201 + base_offset, "corner" );
            add_ascii_subtile( curr_tile, id, 186 + base_offset, "edge" );
            add_ascii_subtile( curr_tile, id, 203 + base_offset, "t_connection" );
            add_ascii_subtile( curr_tile, id, 210 + base_offset, "end_piece" );
            add_ascii_subtile( curr_tile, id, 219 + base_offset, "unconnected" );
        }
        ts.create_tile_type( id, std::move( curr_tile ) );
    }
}

void tileset_loader::load_tilejson_from_file( const JsonObject &config )
{
    if( !config.has_member( "tiles" ) ) {
        config.throw_error( "\"tiles\" section missing" );
    }

    for( const JsonObject &entry : config.get_array( "tiles" ) ) {
        std::vector<std::string> ids;
        if( entry.has_string( "id" ) ) {
            ids.push_back( entry.get_string( "id" ) );
        } else if( entry.has_array( "id" ) ) {
            ids = entry.get_string_array( "id" );
        }
        for( const std::string &t_id : ids ) {
            tile_type &curr_tile = load_tile( entry, t_id );
            curr_tile.offset = sprite_offset;
            const bool t_multi = entry.get_bool( "multitile", false );
            const bool t_rota = entry.get_bool( "rotates", t_multi );
            const int t_h3d = entry.get_int( "height_3d", 0 );
            const auto t_flags = entry.get_tags<flag_id>( "flags" );
            std::optional<RGBColor> t_tint;
            entry.read( "default_tint", t_tint );

            if( t_multi ) {
                // fetch additional tiles
                for( const JsonObject &subentry : entry.get_array( "additional_tiles" ) ) {
                    const std::string s_id = subentry.get_string( "id" );
                    const std::string m_id = t_id + "_" + s_id;
                    tile_type &curr_subtile = load_tile( subentry, m_id );
                    curr_subtile.offset = sprite_offset;
                    curr_subtile.rotates = true;
                    curr_subtile.height_3d = t_h3d;
                    curr_subtile.animated = subentry.get_bool( "animated", false );
                    curr_subtile.default_tint = t_tint;
                    curr_subtile.flags = t_flags;
                    curr_tile.available_subtiles.push_back( s_id );
                }
            } else if( entry.has_array( "additional_tiles" ) ) {
                entry.throw_error( "Additional tiles defined, but 'multitile' is not true." );
            }

            // write the information of the base tile to curr_tile
            curr_tile.multitile = t_multi;
            curr_tile.rotates = t_rota;
            curr_tile.height_3d = t_h3d;
            curr_tile.default_tint = t_tint;
            curr_tile.flags = t_flags;
            curr_tile.animated = entry.get_bool( "animated", false );
        }
    }
    dbg( DL::Info ) << "Tile Width: " << ts.tile_width << " Tile Height: " << ts.tile_height <<
                    " Tile Definitions: " << ts.tile_ids.size();
}

/**
 * Load a tile definition and add it to the @ref tileset::tile_ids map.
 * All loaded tiles go into one vector (@ref tileset::tile_values), their index in it is their id.
 * The JSON data (loaded here) contains tile ids relative to the associated image.
 * They are translated into global ids by adding the @p offset, which is the number of
 * previously loaded tiles (excluding the tiles from the associated image).
 * @param id The id of the new tile definition (which is the key in @ref tileset::tile_ids). Any existing
 * definition of the same id is overridden.
 * @return A reference to the loaded tile inside the @ref tileset::tile_ids map.
 */
tile_type &tileset_loader::load_tile( const JsonObject &entry, const std::string &id )
{
    tile_type curr_subtile;

    load_tile_spritelists( entry, curr_subtile.sprite.fg, "fg" );
    load_tile_spritelists( entry, curr_subtile.sprite.bg, "bg" );

    if( entry.has_array( "masks" ) ) {
        for( const JsonObject mask_entry : entry.get_array( "masks" ) ) {
            const auto mask_type = mask_entry.get_string( "type" );
            if( mask_type == "tint" ) {
                load_tile_spritelists( mask_entry, curr_subtile.masks.tint.fg, "fg" );
                load_tile_spritelists( mask_entry, curr_subtile.masks.tint.bg, "bg" );
            } else {
                debugmsg_of( DL::Warn, "Invalid tile mask type: %s", mask_type );
            }
        }
    }

    using vslist = tile_type::sprite_list;
    auto ensure_mask = [&]( vslist & mask, const vslist & sprite ) {
        if( ! mask.empty() ) {
            for( const auto& [a, b] :  std::views::zip( mask, sprite ) ) {
                if( ( a.weight != b.weight ) || ( a.obj.size() != b.obj.size() ) ) {
                    debugmsg( "Tile mask definition must match sprite: %s", id );
                    mask.clear();
                }
            }
        }

        if( mask.empty() ) {
            for( auto &l : sprite ) {
                auto tmp = std::vector<int>( l.obj.size(), TILESET_NO_MASK );
                mask.add( tmp, l.weight );
            }
        }
    };

    ensure_mask( curr_subtile.masks.tint.fg, curr_subtile.sprite.fg );
    ensure_mask( curr_subtile.masks.tint.bg, curr_subtile.sprite.bg );

    curr_subtile.has_om_transparency = entry.get_bool( "has_om_transparency", false );

    return ts.create_tile_type( id, std::move( curr_subtile ) );
}

void tileset_loader::load_tile_spritelists( const JsonObject &entry,
        weighted_int_list<std::vector<int>> &vs,
        const std::string &objname )
{
    // json array indicates rotations or variations
    if( entry.has_array( objname ) ) {
        JsonArray g_array = entry.get_array( objname );
        // int elements of array indicates rotations
        // create one variation, populate sprite_ids with list of ints
        if( g_array.test_int() ) {
            std::vector<int> v;
            for( const int entry : g_array ) {
                const int sprite_id = entry + sprite_id_offset;
                if( sprite_id >= 0 ) {
                    v.push_back( sprite_id );
                }
            }
            vs.add( v, 1 );
        }
        // object elements of array indicates variations
        // create one variation per object
        else if( g_array.test_object() ) {
            for( const JsonObject &vo : g_array ) {
                std::vector<int> v;
                int weight = vo.get_int( "weight" );
                // negative weight is invalid
                if( weight < 0 ) {
                    vo.throw_error( "Invalid weight for sprite variation (<0)", objname );
                }
                // int sprite means one sprite
                if( vo.has_int( "sprite" ) ) {
                    const int sprite_id = vo.get_int( "sprite" ) + sprite_id_offset;
                    if( sprite_id >= 0 ) {
                        v.push_back( sprite_id );
                    }
                }
                // array sprite means rotations
                else if( vo.has_array( "sprite" ) ) {
                    for( const int entry : vo.get_array( "sprite" ) ) {
                        const int sprite_id = entry + sprite_id_offset;
                        if( sprite_id >= 0 ) {
                            v.push_back( sprite_id );
                        }
                    }
                }
                if( v.size() != 1 &&
                    v.size() != 2 &&
                    v.size() != 4 ) {
                    vo.throw_error( "Invalid number of sprites (not 1, 2, or 4)", objname );
                }
                vs.add( v, weight );
            }
        }
    }
    // json int indicates a single sprite id
    else if( entry.has_int( objname ) && entry.get_int( objname ) >= 0 ) {
        vs.add( std::vector<int>( {entry.get_int( objname ) + sprite_id_offset} ), 1 );
    }
}

static int divide_round_down( int a, int b )
{
    if( b < 0 ) {
        a = -a;
        b = -b;
    }
    if( a >= 0 ) {
        return a / b;
    } else {
        return -( ( -a + b - 1 ) / b );
    }
}

void cata_tiles::draw( point dest, const tripoint &center, int width, int height,
                       std::multimap<point, formatted_text> &overlay_strings,
                       color_block_overlay_container &color_blocks )
{
    if( !g ) {
        return;
    }

#if defined(__ANDROID__)
    // Attempted bugfix for Google Play crash - prevent divide-by-zero if no tile width/height specified
    if( tile_width == 0 || tile_height == 0 ) {
        return;
    }
#endif

    {
        //set clipping to prevent drawing over stuff we shouldn't
        SDL_Rect clipRect = {dest.x, dest.y, width, height};
        printErrorIf( SDL_RenderSetClipRect( renderer.get(), &clipRect ) != 0,
                      "SDL_RenderSetClipRect failed" );

        //fill render area with black to prevent artifacts where no new pixels are drawn
        geometry->rect( renderer, clipRect, SDL_Color{ 0, 0, 0, 255 } );
    }

    point s;
    get_window_tile_counts( width, height, s.x, s.y );

    init_light();
    map &here = get_map();
    const visibility_variables &cache = here.get_visibility_variables_cache();

    const bool iso_mode = tile_iso;

    const bool show_zones_overlay = g->show_zone_overlay && !iso_mode;

    struct zone_render_data {
        point min_local;
        point max_local;
        SDL_Color color;
        std::string name;
    };

    std::vector<zone_render_data> zones_to_draw;
    zones_to_draw.reserve( 64 );

    if( show_zones_overlay ) {
        const zone_manager &mgr = zone_manager::get_manager();
        for( const zone_data &zone : mgr.get_zones() ) {
            if( !zone.get_enabled() || zone.get_start_point().z != center.z ) {
                continue;
            }

            const tripoint local_start = here.getlocal( zone.get_start_point() );
            const tripoint local_end = here.getlocal( zone.get_end_point() );
            const point min_pt( std::min( local_start.x, local_end.x ),
                                std::min( local_start.y, local_end.y ) );
            const point max_pt( std::max( local_start.x, local_end.x ),
                                std::max( local_start.y, local_end.y ) );

            zones_to_draw.push_back( { min_pt, max_pt, curses_color_to_SDL( zone.get_type().obj().color() ), zone.get_name() } );
        }
    }

    const bool has_selected_zone = do_draw_zones;
    point selected_min = point_zero;
    point selected_max = point_zero;
    int selected_z = 0;
    if( has_selected_zone ) {
        const tripoint sel_start = zone_start + zone_offset;
        const tripoint sel_end = zone_end + zone_offset;
        selected_min = point( std::min( sel_start.x, sel_end.x ), std::min( sel_start.y, sel_end.y ) );
        selected_max = point( std::max( sel_start.x, sel_end.x ), std::max( sel_start.y, sel_end.y ) );
        selected_z = sel_start.z;
    }

    o = iso_mode ? center.xy() : center.xy() - point( POSX, POSY );

    op = dest;
    // Rounding up to include incomplete tiles at the bottom/right edges
    screentile_width = divide_round_up( width, tile_width );
    screentile_height = divide_round_up( height, tile_height );

    const int min_col = 0;
    const int max_col = s.x;
    const int min_row = 0;
    const int max_row = s.y;

    //limit the render area to maximum view range (121x121 square centered on player)
    const int min_visible_x = g->u.posx() % SEEX;
    const int min_visible_y = g->u.posy() % SEEY;
    const int max_visible_x = ( g->u.posx() % SEEX ) + ( MAPSIZE - 1 ) * SEEX;
    const int max_visible_y = ( g->u.posy() % SEEY ) + ( MAPSIZE - 1 ) * SEEY;

    // Map memory should be at least the size of the view range
    // so that new tiles can be memorized, and at least the size of the display
    // since at farthest zoom displayed area may be bigger than view range.
    const point min_mm_reg = point(
                                 std::min( o.x, min_visible_x ),
                                 std::min( o.y, min_visible_y )
                             );
    const point max_mm_reg = point(
                                 std::max( s.x + o.x, max_visible_x ),
                                 std::max( s.y + o.y, max_visible_y )
                             );
    g->u.prepare_map_memory_region(
        here.getabs( tripoint( min_mm_reg, center.z ) ),
        here.getabs( tripoint( max_mm_reg, center.z ) )
    );

    idle_animations.set_enabled( get_option<bool>( "ANIMATIONS" ) );
    idle_animations.prepare_for_redraw();

    //set up a default tile for the edges outside the render area
    visibility_type offscreen_type = VIS_DARK;
    if( cache.u_is_boomered ) {
        offscreen_type = VIS_BOOMER_DARK;
    }

    //retrieve night vision goggle status once per draw
    auto vision_cache = g->u.get_vision_modes();
    nv_goggles_activated = vision_cache[NV_GOGGLES];

    // check that the creature for which we'll draw the visibility map is still alive at that point
    if( g->display_overlay_state( ACTION_DISPLAY_VISIBILITY ) && g->displaying_visibility_creature ) {
        const Creature *creature = g->displaying_visibility_creature;
        const auto is_same_creature_predicate = [&creature]( const Creature & c ) {
            return creature == &c;
        };
        if( g->get_creature_if( is_same_creature_predicate ) == nullptr )  {
            g->displaying_visibility_creature = nullptr;
        }
    }
    const point half_tile( tile_width / 2, 0 );
    const point quarter_tile( tile_width / 4, tile_height / 4 );
    if( g->display_overlay_state( ACTION_DISPLAY_VEHICLE_AI ) ) {
        for( const wrapped_vehicle &elem : here.get_vehicles() ) {
            const vehicle &veh = *elem.v;
            const point veh_pos = veh.global_pos3().xy();
            for( const auto &overlay_data : veh.get_debug_overlay_data() ) {
                const point pt = veh_pos + std::get<0>( overlay_data );
                const int color = std::get<1>( overlay_data );
                const std::string &text = std::get<2>( overlay_data );
                overlay_strings.emplace( player_to_screen( pt ),
                                         formatted_text( text, color,
                                                 text_alignment::left ) );
            }
        }
    }

    std::vector<tile_render_info> &draw_points = *draw_points_cache;
    int min_z = OVERMAP_HEIGHT;

    for( int row = min_row; row < max_row; row ++ ) {

        draw_points.clear();
        for( int col = min_col; col < max_col; col ++ ) {
            int temp_x;
            int temp_y;
            if( iso_mode ) {
                //in isometric, rows and columns represent a checkerboard screen space, and we place
                //the appropriate tile in valid squares by getting position relative to the screen center.
                if( modulo( row - s.y / 2, 2 ) != modulo( col - s.x / 2, 2 ) ) {
                    continue;
                }
                temp_x = divide_round_down( col - row - s.x / 2 + s.y / 2, 2 ) + o.x;
                temp_y = divide_round_down( row + col - s.y / 2 - s.x / 2, 2 ) + o.y;
            } else {
                temp_x = col + o.x;
                temp_y = row + o.y;
            }

            bool invis = ( temp_y < min_visible_y || temp_y > max_visible_y || temp_x < min_visible_x ||
                           temp_x > max_visible_x ) &&
                         ( has_memory_at( {temp_x, temp_y, center.z} ) || has_draw_override( {temp_x, temp_y, center.z} ) );



            // Add scent value to the overlay_strings list for every visible tile when displaying scent
            if( g->display_overlay_state( ACTION_DISPLAY_SCENT ) && !invis ) {
                const int scent_value = g->scent.get( {temp_x, temp_y, center.z} );
                if( scent_value > 0 ) {
                    overlay_strings.emplace( player_to_screen( point( temp_x, temp_y ) ) + point( tile_width / 2, 0 ),
                                             formatted_text( std::to_string( scent_value ), 8 + catacurses::yellow,
                                                     direction::NORTH ) );
                }
            }

            // Add scent type to the overlay_strings list for every visible tile when displaying scent
            if( g->display_overlay_state( ACTION_DISPLAY_SCENT_TYPE ) && !invis ) {
                const scenttype_id scent_type = g->scent.get_type( {temp_x, temp_y, center.z} );
                if( !scent_type.is_empty() ) {
                    overlay_strings.emplace( player_to_screen( point( temp_x, temp_y ) ) + point( tile_width / 2, 0 ),
                                             formatted_text( scent_type.c_str(), 8 + catacurses::yellow,
                                                     direction::NORTH ) );
                }
            }

            if( g->display_overlay_state( ACTION_DISPLAY_RADIATION ) ) {
                const auto rad_override = radiation_override.find( {temp_x, temp_y, center.z} );
                const bool rad_overridden = rad_override != radiation_override.end();
                if( rad_overridden || !invis ) {
                    const int rad_value = rad_overridden ? rad_override->second : here.get_radiation( {temp_x, temp_y, center.z} );
                    catacurses::base_color col;
                    if( rad_value > 0 ) {
                        col = catacurses::green;
                    } else {
                        col = catacurses::cyan;
                    }
                    overlay_strings.emplace( player_to_screen( point( temp_x, temp_y ) ) + point( tile_width / 2, 0 ),
                                             formatted_text( std::to_string( rad_value ), 8 + col, direction::NORTH ) );
                }
            }

            // Add temperature value to the overlay_strings list for every visible tile when displaying temperature
            if( g->display_overlay_state( ACTION_DISPLAY_TEMPERATURE ) && !invis ) {
                const auto temp = get_weather().get_temperature( tripoint_abs_omt{temp_x, temp_y, center.z} );
                short color;
                const short bold = 8;
                if( temp > 40_c ) {
                    color = catacurses::red;
                } else if( temp > 25_c ) {
                    color = catacurses::yellow + bold;
                } else if( temp > 10_c ) {
                    color = catacurses::green + bold;
                } else if( temp > 0_c ) {
                    color = catacurses::white + bold;
                } else if( temp > -10_c ) {
                    color = catacurses::cyan + bold;
                } else {
                    color = catacurses::blue + bold;
                }
                const auto display_option = get_option<std::string>( "USE_CELSIUS" );
                const int temp_value = display_option == "kelvin" ? units::to_kelvins( temp )
                                       : display_option == "fahrenheit" ? units::to_fahrenheit( temp )
                                       : units::to_celsius( temp );

                overlay_strings.emplace( player_to_screen( point( temp_x, temp_y ) ) + point( tile_width / 2, 0 ),
                                         formatted_text( std::to_string( temp_value ), color,
                                                 direction::NORTH ) );
            }

            if( g->display_overlay_state( ACTION_DISPLAY_VISIBILITY ) &&
                g->displaying_visibility_creature && !invis ) {
                const bool visibility = g->displaying_visibility_creature->sees( {temp_x, temp_y, center.z} );

                // color overlay.
                auto block_color = visibility ? windowsPalette[catacurses::green] : SDL_Color{ 192, 192, 192, 255 };
                block_color.a = 100;
                color_blocks.first = SDL_BLENDMODE_BLEND;
                color_blocks.second.emplace( player_to_screen( point( temp_x, temp_y ) ), block_color );

                // overlay string
                std::string visibility_str = visibility ? "+" : "-";
                overlay_strings.emplace(
                    player_to_screen( point( temp_x, temp_y ) ) + point( tile_width / 4, tile_height / 4 ),
                    formatted_text( visibility_str, catacurses::black, direction::NORTH ) );
            }

            static std::vector<SDL_Color> lighting_colors;
            // color hue in the range of [0..10], 0 being white,  10 being blue
            auto draw_debug_tile = [&]( const int color_hue, const std::string & text ) {
                if( lighting_colors.empty() ) {
                    SDL_Color white = { 255, 255, 255, 255 };
                    SDL_Color blue = { 0, 0, 255, 255 };
                    lighting_colors = color_linear_interpolate( white, blue, 9 );
                }
                point tile_pos = player_to_screen( point( temp_x, temp_y ) );

                // color overlay
                SDL_Color color = lighting_colors[std::min( std::max( 0, color_hue ), 10 )];
                color.a = 100;
                color_blocks.first = SDL_BLENDMODE_BLEND;
                color_blocks.second.emplace( tile_pos, color );

                // string overlay
                overlay_strings.emplace( tile_pos + quarter_tile, formatted_text( text, catacurses::black,
                                         direction::NORTH ) );
            };

            if( g->display_overlay_state( ACTION_DISPLAY_LIGHTING ) ) {
                if( g->displaying_lighting_condition == 0 ) {
                    const float light = here.ambient_light_at( {temp_x, temp_y, center.z} );
                    // note: lighting will be constrained in the [1.0, 11.0] range.
                    int intensity = static_cast<int>( std::max( 1.0, LIGHT_AMBIENT_LIT - light + 1.0 ) ) - 1;
                    draw_debug_tile( intensity, string_format( "%.1f", light ) );
                }
            }

            if( g->display_overlay_state( ACTION_DISPLAY_TRANSPARENCY ) ) {
                const float tr = here.light_transparency( {temp_x, temp_y, center.z} );
                int intensity =  tr <= LIGHT_TRANSPARENCY_SOLID ? 10 :  static_cast<int>
                                 ( ( tr - LIGHT_TRANSPARENCY_OPEN_AIR ) * 8 );
                draw_debug_tile( intensity, string_format( "%.2f", tr ) );
            }

            lit_level ll = lit_level::BLANK;
            int last_vis = center.z + 1;
            for( int z = center.z; z >= -OVERMAP_DEPTH; z-- ) {
                const auto &ch = here.access_cache( z );

                const tripoint pos( temp_x, temp_y, z );
                const int &x = pos.x;
                const int &y = pos.y;

                bool in_vis_bounds = ( y >= min_visible_y && y <= max_visible_y && x >= min_visible_x &&
                                       x <= max_visible_x );

                bool in_map_bounds = here.inbounds( pos );

                const bool stop_on_memory = z != center.z && has_memory_at( pos ) &&
                                            ( !in_map_bounds || here.ter( pos ) != t_open_air );

                if( ( fov_3d || z == center.z ) && in_map_bounds ) {
                    ll = ch.visibility_cache[x][y];
                    if( !would_apply_vision_effects( here.get_visibility( ch.visibility_cache[x][y], cache ) ) ) {
                        last_vis = z;
                    }
                }

                const auto low_override = draw_below_override.find( pos );
                const bool low_overridden = low_override != draw_below_override.end();
                if( low_overridden ? !low_override->second :
                    ( in_map_bounds && ( here.dont_draw_lower_floor( pos ) || stop_on_memory ) )
                    || ( !in_map_bounds && ( has_memory_at( pos ) || pos.z <= 0 ) ) ) {
                    // invisible to normal eyes
                    bool invisible[5];
                    invisible[0] = false;

                    if( !in_vis_bounds ) {
                        if( has_memory_at( pos ) ) {
                            ll = lit_level::MEMORIZED;
                            invisible[0] = true;
                        } else if( has_draw_override( pos ) ) {
                            ll = lit_level::DARK;
                            invisible[0] = true;
                        } else {
                            apply_vision_effects( pos, offscreen_type );
                            break;
                        }
                    }

                    int height_3d = 0;

                    for( int i = 0; i < 4; i++ ) {
                        const tripoint np = pos + neighborhood[i];
                        invisible[1 + i] = np.y < min_visible_y || np.y > max_visible_y ||
                                           np.x < min_visible_x || np.x > max_visible_x ||
                                           would_apply_vision_effects( here.get_visibility( ch.visibility_cache[np.x][np.y], cache ) );
                    }

                    if( !invisible[0] && apply_vision_effects( pos, here.get_visibility( ll, cache ) ) ) {
                        if( has_draw_override( pos ) || has_memory_at( pos ) ) {
                            invisible[0] = true;
                        }
                        for( int cz = pos.z; !invisible[0] && cz <= -center.z; cz++ ) {
                            const Creature *critter = g->critter_at( {pos.xy(), cz}, true );
                            if( critter && ( g->u.sees_with_infrared( *critter ) ||
                                             g->u.sees_with_specials( *critter ) ) ) {
                                invisible[0] = true;
                            }
                        }
                        if( invisible[0] ) {
                            min_z = std::min( pos.z, min_z );
                            draw_points.emplace_back( pos, height_3d, ll, invisible );
                        } else if( last_vis != center.z + 1 ) {
                            min_z = std::min( last_vis, min_z );
                            draw_points.emplace_back( tripoint( pos.xy(), last_vis ), height_3d, ll, invisible );
                        }

                    } else {
                        min_z = std::min( pos.z, min_z );
                        draw_points.emplace_back( pos, height_3d, ll, invisible );
                    }
                    break;
                }
            }
        }

        auto compare_z = [&]( tile_render_info a, tile_render_info b ) -> bool {
            return ( a.pos.z < b.pos.z );
        };

        const std::array<decltype( &cata_tiles::draw_furniture ), 3> base_drawing_layers = {{
                &cata_tiles::draw_furniture, &cata_tiles::draw_graffiti, &cata_tiles::draw_trap
            }
        };
        struct zlevel_layer {
            bool hide_unseen;
            decltype( &cata_tiles::draw_furniture ) function;
        };
        const std::array < zlevel_layer, 3 > zlevel_drawing_layers = {{
                {true, &cata_tiles::draw_field_or_item}, {false, &cata_tiles::draw_vpart}, {true, &cata_tiles::draw_critter_at}
            }
        };

        const std::array<decltype( &cata_tiles::draw_furniture ), 2> final_drawing_layers = {{
                &cata_tiles::draw_zone_mark, &cata_tiles::draw_zombie_revival_indicators
            }
        };

        std::ranges::stable_sort( draw_points, compare_z );
        for( tile_render_info &p : draw_points ) {
            draw_terrain( p.pos, p.ll, p.height_3d, p.invisible, center.z - p.pos.z );

            if( p.pos.z == center.z ) {
                const point screen_tl = player_to_screen( p.pos.xy() );
                const SDL_Rect tile_rect{ screen_tl.x, screen_tl.y, tile_width, tile_height };

                const bool in_selected_zone = has_selected_zone && p.pos.z == selected_z &&
                                              p.pos.x >= selected_min.x && p.pos.x <= selected_max.x &&
                                              p.pos.y >= selected_min.y && p.pos.y <= selected_max.y;

                bool selected_drawn = false;

                if( show_zones_overlay ) {
                    for( const zone_render_data &zone : zones_to_draw ) {
                        if( p.pos.x < zone.min_local.x || p.pos.x > zone.max_local.x ||
                            p.pos.y < zone.min_local.y || p.pos.y > zone.max_local.y ) {
                            continue;
                        }
                        draw_zone_overlay( {
                            .renderer = renderer,
                            .rect = tile_rect,
                            .color = zone.color,
                            .overlay_strings = overlay_strings,
                            .alpha = in_selected_zone ? 128 : 64,
                            .draw_label = false
                        } );
                        selected_drawn = selected_drawn || in_selected_zone;
                    }
                }
                if( in_selected_zone && !selected_drawn ) {
                    draw_zone_overlay( {
                        .renderer = renderer,
                        .rect = tile_rect,
                        .color = curses_color_to_SDL( c_light_green ),
                        .overlay_strings = overlay_strings,
                        .alpha = 128,
                        .draw_label = false
                    } );
                }
            }
        }

        for( int z = min_z; z <= center.z; z++ ) {
            for( tile_render_info &p : draw_points ) {
                if( p.pos.z > z ) {
                    break;
                }
                if( p.pos.z == z ) {
                    for( decltype( &cata_tiles::draw_furniture ) f : base_drawing_layers ) {
                        ( this->*f )( p.pos, p.ll, p.height_3d, p.invisible, center.z - p.pos.z );
                    }
                }
                const auto &ch = here.access_cache( z );

                for( const zlevel_layer &f : zlevel_drawing_layers ) {
                    if( here.inbounds( p.pos ) && z != p.pos.z ) {
                        if( !f.hide_unseen || ch.visibility_cache[p.pos.x][p.pos.y] != lit_level::BLANK ) {
                            const bool ( invis )[5] = {false, false, false, false, false};
                            ( this->*( f.function ) )( {p.pos.xy(), z}, p.ll, p.height_3d, invis, center.z - z );
                        }
                    } else {
                        ( this->*( f.function ) )( {p.pos.xy(), z}, p.ll, p.height_3d, p.invisible, center.z - z );
                    }
                }
            }
        }

        for( tile_render_info &p : draw_points ) {
            for( decltype( &cata_tiles::draw_furniture ) f : final_drawing_layers ) {
                ( this->*f )( p.pos, p.ll, p.height_3d, p.invisible, 0 );
            }
        }
    }

    // display number of monsters to spawn in mapgen preview
    for( const tile_render_info &p : draw_points ) {
        const auto mon_override = monster_override.find( p.pos );
        if( mon_override != monster_override.end() ) {
            const int count = std::get<1>( mon_override->second );
            const bool more = std::get<2>( mon_override->second );
            if( count > 1 || more ) {
                std::string text = "x" + std::to_string( count );
                if( more ) {
                    text += "+";
                }
                overlay_strings.emplace( player_to_screen( p.pos.xy() ) + point( tile_width / 2, 0 ),
                                         formatted_text( text, catacurses::red, direction::NORTH ) );
            }
        }
    }
    // tile overrides are already drawn in the previous code
    void_radiation_override();
    void_terrain_override();
    void_furniture_override();
    void_graffiti_override();
    void_trap_override();
    void_field_override();
    void_item_override();
    void_vpart_override();
    void_draw_below_override();
    void_monster_override();

    //Memorize everything the character just saw even if it wasn't displayed.
    for( int mem_y = min_visible_y; mem_y <= max_visible_y; mem_y++ ) {
        for( int mem_x = min_visible_x; mem_x <= max_visible_x; mem_x++ ) {
            half_open_rectangle<point> already_drawn(
                point( min_col, min_row ), point( max_col, max_row ) );
            if( iso_mode ) {
                // calculate the screen position according to the drawing code above (division rounded down):

                // mem_x = ( col - row - sx / 2 + sy / 2 ) / 2 + o.x;
                // mem_y = ( row + col - sy / 2 - sx / 2 ) / 2 + o.y;
                // ( col - sx / 2 ) % 2 = ( row - sy / 2 ) % 2
                // ||
                // \/
                const int col = mem_y + mem_x + s.x / 2 - o.y - o.x;
                const int row = mem_y - mem_x + s.y / 2 - o.y + o.x;
                if( already_drawn.contains( point( col, row ) ) ) {
                    continue;
                }
            } else {
                // calculate the screen position according to the drawing code above:

                // mem_x = col + o.x
                // mem_y = row + o.y
                // ||
                // \/
                // col = mem_x - o.x
                // row = mem_y - o.y
                if( already_drawn.contains( point( mem_x, mem_y ) - o ) ) {
                    continue;
                }
            }

            lit_level lighting = here.access_cache( center.z ).visibility_cache[mem_x][mem_y];

            int z = center.z;
            for( ;  z > -OVERMAP_DEPTH; z-- ) {
                const auto low_override = draw_below_override.find( {mem_x, mem_y, z} );
                const bool low_overridden = low_override != draw_below_override.end();
                if( low_overridden ? !low_override->second : ( here.dont_draw_lower_floor( {mem_x, mem_y, z} )
                        || ( fov_3d && lighting != lit_level::BLANK &&
                             here.access_cache( z - 1 ).visibility_cache[mem_x][mem_y] == lit_level::BLANK ) ) ) {
                    if( fov_3d ) {
                        lighting = here.access_cache( z ).visibility_cache[mem_x][mem_y];
                    }
                    break;
                }
            }


            const auto &ch = here.access_cache( z );
            const tripoint p( mem_x, mem_y, z );

            if( apply_vision_effects( p, here.get_visibility( lighting, cache ) ) ) {
                continue;
            }
            int height_3d = 0;
            bool invisible[5];
            invisible[0] = false;
            for( int i = 0; i < 4; i++ ) {
                const tripoint np = p + neighborhood[i];
                invisible[1 + i] = np.y < min_visible_y || np.y > max_visible_y ||
                                   np.x < min_visible_x || np.x > max_visible_x ||
                                   would_apply_vision_effects( here.get_visibility( ch.visibility_cache[np.x][np.y], cache ) );
            }
            //calling draw to memorize everything.
            //bypass cache check in case we learn something new about the terrain's connections
            draw_terrain( p, lighting, height_3d, invisible, 0 );
            if( here.check_seen_cache( p ) ) {
                draw_furniture( p, lighting, height_3d, invisible, 0 );
                draw_trap( p, lighting, height_3d, invisible, 0 );
                draw_vpart( p, lighting, height_3d, invisible, 0 );
                here.check_and_set_seen_cache( p );
            }
        }
    }

    in_animation = do_draw_explosion || do_draw_custom_explosion ||
                   do_draw_bullet || do_draw_hit || do_draw_line ||
                   do_draw_cursor || do_draw_highlight || do_draw_weather ||
                   do_draw_sct || do_draw_zones || do_draw_cone_aoe;

    draw_footsteps_frame( center );
    if( in_animation ) {
        if( do_draw_explosion ) {
            draw_explosion_frame();
        }
        if( do_draw_custom_explosion ) {
            draw_custom_explosion_frame();
        }
        if( do_draw_bullet ) {
            draw_bullet_frame();
        }
        if( do_draw_hit ) {
            draw_hit_frame();
            void_hit();
        }
        if( do_draw_line ) {
            draw_line();
            void_line();
        }
        if( do_draw_weather ) {
            draw_weather_frame();
            void_weather();
        }
        if( do_draw_sct ) {
            draw_sct_frame( overlay_strings );
            void_sct();
        }
        if( do_draw_zones ) {
            void_zones();
        }
        if( do_draw_cursor ) {
            draw_cursor();
            void_cursor();
        }
        if( do_draw_highlight ) {
            draw_highlight();
            void_highlight();
        }
        if( do_draw_cone_aoe ) {
            draw_cone_aoe_frame();
        }
    } else if( g->u.view_offset != tripoint_zero && !g->u.in_vehicle ) {
        // check to see if player is located at ter
        const tile_search_params tile { "cursor", C_NONE, empty_string, 0, 0 };
        const auto pos = tripoint( g->ter_view_p.xy(), center.z );
        draw_from_id_string(
            tile, pos, std::nullopt, std::nullopt,
            lit_level::LIT, false, 0, false );
    }
    if( g->u.controlling_vehicle ) {
        if( std::optional<tripoint> indicator_offset = g->get_veh_dir_indicator_location( true ) ) {
            const tile_search_params tile { "cursor", C_NONE, empty_string, 0, 0 };
            const auto pos = indicator_offset->xy() + tripoint( g->u.posx(), g->u.posy(), center.z );
            draw_from_id_string(
                tile, pos, std::nullopt, std::nullopt,
                lit_level::LIT, false, 0, false );
        }
    }

    if( g->debug_submap_grid_overlay && !iso_mode ) {
        point sm_start = ms_to_sm_copy( here.getabs( point( min_col, min_row ) + o ) );
        point sm_end = ms_to_sm_copy( here.getabs( point( max_col, max_row ) + o ) );

        bool zlevs = here.has_zlevels();
        int mapsize = here.getmapsize();
        tripoint mappos = here.get_abs_sub();
        half_open_rectangle<point> maprect( mappos.xy(), mappos.xy() + point( mapsize, mapsize ) );

        const auto is_map = [mappos, zlevs, maprect]( const tripoint & p ) {
            if( !maprect.contains( p.xy() ) ) {
                return false;
            }
            if( zlevs ) {
                return true;
            } else {
                return p.z == mappos.z;
            }
        };

        const auto is_mapbuffer = []( const tripoint & p ) {
            return MAPBUFFER.is_submap_loaded( p );
        };

        constexpr int THICC = 1; // line thickness
        for( int sm_x = sm_start.x; sm_x <= sm_end.x; sm_x++ ) {
            for( int sm_y = sm_start.y; sm_y <= sm_end.y; sm_y++ ) {
                point sm_p = point( sm_x, sm_y );
                tripoint sm_tp = tripoint( sm_x, sm_y, center.z );
                point p1 = player_to_screen( here.getlocal( sm_to_ms_copy( sm_p ) ) );
                point p3 = player_to_screen( here.getlocal( sm_to_ms_copy( sm_p + point_south_east ) ) );
                p3 -= point( THICC, THICC ); // Don't draw over other lines

                // Leave a small gap to indicate omt boundaries
                point tmp = omt_to_sm_copy( sm_to_omt_copy( sm_tp ) ).xy();
                if( tmp.x == sm_tp.x ) {
                    p1.x += 2;
                }
                if( tmp.y == sm_tp.y ) {
                    p1.y += 2;
                }

                SDL_Color col;
                if( is_map( sm_tp ) ) {
                    col = {0, 220, 0, 255};
                } else if( is_mapbuffer( sm_tp ) ) {
                    col = {0, 180, 180, 255};
                } else {
                    col = {0, 0, 220, 255};
                }

                geometry->vertical_line( renderer, p1, p3.y, THICC, col );
                geometry->vertical_line( renderer, point( p3.x, p1.y ), p3.y, THICC, col );
                geometry->horizontal_line( renderer, p1, p3.x, THICC, col );
                geometry->horizontal_line( renderer, point( p1.x, p3.y ), p3.x, THICC, col );
            }
        }
    }

    if( show_zones_overlay ) {
        for( const zone_render_data &zone : zones_to_draw ) {
            if( zone.name.empty() ) {
                continue;
            }

            const point screen_tl = player_to_screen( zone.min_local );
            const point screen_br = player_to_screen( zone.max_local ) + point( tile_width, tile_height );
            const point center_pt( screen_tl.x + ( screen_br.x - screen_tl.x ) / 2,
                                   screen_tl.y + ( screen_br.y - screen_tl.y ) / 2 );

            overlay_strings.emplace( center_pt,
                                     formatted_text( zone.name, catacurses::white, text_alignment::center ) );
        }
    }

    printErrorIf( SDL_RenderSetClipRect( renderer.get(), nullptr ) != 0,
                  "SDL_RenderSetClipRect failed" );
}

bool cata_tiles::terrain_requires_animation() const
{
    return idle_animations.enabled() && idle_animations.present();
}

void cata_tiles::display_character( const Character &ch, const point &p )
{
    int height_3d = 0;
    draw_entity_with_overlays( ch, tripoint( p, 0 ), lit_level::BRIGHT, height_3d, true );
}

void cata_tiles::draw_minimap( point dest, const tripoint &center, int width, int height )
{
    minimap->draw( SDL_Rect{ dest.x, dest.y, width, height }, center );
}

bool cata_tiles::minimap_requires_animation() const
{
    return minimap->has_animated_elements();
}

void cata_tiles::get_window_tile_counts( const int width, const int height, int &columns,
        int &rows ) const
{
    if( tile_iso ) {
        columns = std::ceil( static_cast<double>( width ) / tile_width ) * 2 + 4;
        rows = std::ceil( static_cast<double>( height ) / ( tile_width / 2.0 - 1 ) ) * 2 + 4;
    } else {
        columns = std::ceil( static_cast<double>( width ) / tile_width );
        rows = std::ceil( static_cast<double>( height ) / tile_height );
    }
}

std::optional<tile_lookup_res>
cata_tiles::find_tile_with_season( const std::string &id ) const
{
    const season_type season = season_of_year( calendar::turn );
    return tileset_ptr->find_tile_type_by_season( id, season );
}

template<typename T>
std::optional<tile_lookup_res>
cata_tiles::find_tile_looks_like_by_string_id( const std::string &id, TILE_CATEGORY category,
        const int looks_like_jumps_limit ) const
{
    const string_id<T> s_id( id );
    if( !s_id.is_valid() ) {
        return std::nullopt;
    }
    const T &obj = s_id.obj();
    return find_tile_looks_like( obj.looks_like, category, looks_like_jumps_limit - 1 );
}

auto cata_tiles::find_tile_looks_like( const std::string &id, TILE_CATEGORY category,
                                       const int looks_like_jumps_limit ) const -> std::optional<tile_lookup_res>
{
    if( id.empty() || looks_like_jumps_limit <= 0 ) {
        return std::nullopt;
    }

    // Note on memory management:
    // This method must returns pointers to the objects (std::string *id  and tile_type * tile)
    // that are valid when this metod returns. Ideally they should have the lifetime
    // that is equal or exceeds lifetime of `this` or `this::tileset_ptr`.
    // For example, `id` argument may have shorter lifetime and thus should not be returned!
    // The result of `find_tile_with_season` is OK to be returned, because it's guaranteed to
    // return pointers to the keys and values that are stored inside the `tileset_ptr`.
    const auto tile_with_season = find_tile_with_season( id );
    if( tile_with_season ) {
        return tile_with_season;
    }

    switch( category ) {
        case C_FURNITURE:
            return find_tile_looks_like_by_string_id<furn_t>( id, category, looks_like_jumps_limit );
        case C_TERRAIN:
            return find_tile_looks_like_by_string_id<ter_t>( id, category, looks_like_jumps_limit );
        case C_TRAP:
            return find_tile_looks_like_by_string_id<trap>( id, category, looks_like_jumps_limit );
        case C_FIELD:
            return find_tile_looks_like_by_string_id<field_type>( id, category, looks_like_jumps_limit );
        case C_MONSTER:
            return find_tile_looks_like_by_string_id<mtype>( id, category, looks_like_jumps_limit );
        case C_OVERMAP_TERRAIN: {
            std::optional<tile_lookup_res> ret;
            const oter_type_str_id type_tmp( id );
            if( !type_tmp.is_valid() ) {
                return ret;
            }

            int jump_limit = looks_like_jumps_limit;
            for( const std::string &looks_like : type_tmp.obj().looks_like ) {

                ret = find_tile_looks_like( looks_like, category, jump_limit - 1 );
                if( ret.has_value() ) {
                    return ret;
                }

                jump_limit--;
                if( jump_limit <= 0 ) {
                    return ret;
                }
            }

            return ret;
        }

        case C_VEHICLE_PART: {
            // vehicle parts start with vp_ for their tiles, but not their IDs
            const vpart_id base_vpid( id.substr( 3 ) );
            if( !base_vpid.is_valid() ) {
                return std::nullopt;
            }
            return find_tile_looks_like( "vp_" + base_vpid.obj().looks_like, category,
                                         looks_like_jumps_limit - 1 );
        }
        case C_ITEM: {
            itype_id iid = itype_id( id );
            if( !iid.is_valid() ) {
                if( id.starts_with( "corpse_" ) ) {
                    return find_tile_looks_like(
                               itype_corpse.str(), category, looks_like_jumps_limit - 1
                           );
                }
                return std::nullopt;
            }
            return find_tile_looks_like( iid->looks_like.str(), category, looks_like_jumps_limit - 1 );
        }

        case C_BULLET: {
            auto ammo_name = id;
            replace_first( ammo_name, "animation_bullet_", "" );
            auto iid = itype_id( ammo_name );
            if( !iid.is_valid() ) {
                return std::nullopt;
            }
            if( !iid->looks_like.is_empty() ) {
                return find_tile_looks_like( "animation_bullet_" + iid->looks_like.str(), category,
                                             looks_like_jumps_limit - 1 );
            }
            return std::nullopt;
        }

        default:
            return std::nullopt;
    }
}

bool cata_tiles::find_overlay_looks_like( const bool male, const std::string &overlay,
        std::string &draw_id )
{
    bool exists = false;

    std::string looks_like;
    std::string over_type;

    if( overlay.starts_with( "worn_" ) ) {
        looks_like = overlay.substr( 5 );
        over_type = "worn_";
    } else if( overlay.starts_with( "wielded_" ) ) {
        looks_like = overlay.substr( 8 );
        over_type = "wielded_";
    } else {
        looks_like = overlay;
    }

    for( int cnt = 0; cnt < 10 && !looks_like.empty(); cnt++ ) {
        draw_id = ( male ? "overlay_male_" : "overlay_female_" ) + over_type + looks_like;
        if( tileset_ptr->find_tile_type( draw_id ) ) {
            exists = true;
            break;
        }
        draw_id = "overlay_" + over_type + looks_like;
        if( tileset_ptr->find_tile_type( draw_id ) ) {
            exists = true;
            break;
        }
        if( looks_like.starts_with( "mutation_active_" ) ) {
            looks_like = "mutation_" + looks_like.substr( 16 );
            continue;
        }
        itype_id iid = itype_id( looks_like );
        if( !iid.is_valid() ) {
            break;
        }
        looks_like = iid->looks_like.str();
    }
    return exists;
}

bool cata_tiles::draw_from_id_string(
    const tile_search_params &tile, const tripoint &pos,
    std::optional<SDL_Color> bg_color, std::optional<SDL_Color> fg_color,
    lit_level ll, bool apply_visual_effects, int overlay_count,
    const bool as_independent_entity, int &height_3d )
{
    // If the ID string does not produce a drawable tile
    // it will revert to the "unknown" tile.
    // The "unknown" tile is one that is highly visible so you kinda can't miss it :D

    // check to make sure that we are drawing within a valid area
    // [0->width|height / tile_width|height]

    half_open_rectangle<point> screen_bounds( o, o + point( screentile_width, screentile_height ) );
    if( !as_independent_entity &&
        !tile_iso &&
        !screen_bounds.contains( pos.xy() ) ) {
        return false;
    }

    // Trying to search for tile type
    std::optional<tile_search_result> search_result;
    search_result = tile_type_search( tile );
    if( search_result == std::nullopt ) {
        return false;
    }

    const tile_type *tt = search_result.value().tt;
    std::string found_id = search_result.value().found_id;

    const tile_type &display_tile = *tt;
    // check to see if the display_tile is multitile, and if so if it has the key related to subtile
    if( tile.subtile != -1 && display_tile.multitile ) {
        const auto &display_subtiles = display_tile.available_subtiles;
        const auto end = std::end( display_subtiles );
        if( std::find( begin( display_subtiles ), end, multitile_keys[tile.subtile] ) != end ) {
            // append subtile name to tile and re-find display_tile
            const tile_search_params multi_tile = {
                found_id + "_" + multitile_keys[tile.subtile],
                tile.category, tile.subcategory, -1, tile.rota
            };
            return draw_from_id_string(
                       multi_tile, pos, bg_color, fg_color,
                       ll, apply_visual_effects, overlay_count, as_independent_entity, height_3d );
        }
    }

    // translate from player-relative to screen relative tile position
    const point screen_pos = as_independent_entity ? pos.xy() : player_to_screen( pos.xy() );

    auto simple_point_hash = []( const auto & p ) {
        return p.x + p.y * 65536;
    };

    bool has_variations = display_tile.sprite.fg.size() > 1 || display_tile.sprite.bg.size() > 1;
    bool variations_enabled = !display_tile.animated || idle_animations.enabled();
    // with animated tiles, seed is used for stagger
    bool seed_for_animation = has_variations && variations_enabled && display_tile.animated;
    bool seed_from_map_coords = false;
    int true_rota = tile.rota;

    // seed the PRNG to get a reproducible random int
    // TODO: faster solution here
    unsigned int seed = 0;
    map &here = get_map();
    // TODO: determine ways other than category to differentiate more types of sprites
    switch( tile.category ) {
        case C_TERRAIN:
        case C_FIELD:
        case C_LIGHTING:
            // stationary map tiles, seed based on map coordinates
            seed_from_map_coords = true;
            break;
        case C_VEHICLE_PART:
            // vehicle parts, seed based on coordinates within the vehicle
            // TODO: also use some vehicle id, for less predictability
        {
            // new scope for variable declarations
            const auto vp_override = vpart_override.find( pos );
            const bool vp_overridden = vp_override != vpart_override.end();
            if( vp_overridden ) {
                const vpart_id &vp_id = std::get<0>( vp_override->second );
                if( vp_id ) {
                    point mount = std::get<4>( vp_override->second );
                    seed = simple_point_hash( mount );
                }
            } else {
                const optional_vpart_position vp = here.veh_at( pos );
                if( vp ) {
                    seed = simple_point_hash( vp->mount() );
                }
            }

            // convert vehicle 360-degree direction (0=E,45=SE, etc) to 4-way tile
            // rotation (0=N,1=W,etc)
            tileray face = tileray( units::from_degrees( true_rota ) );
            true_rota = 3 - face.dir4();

        }
        break;
        case C_FURNITURE: {
            // If the furniture is not movable, we'll allow seeding by the position
            // since we won't get the behavior that occurs where the tile constantly
            // changes when the player grabs the furniture and drags it, causing the
            // seed to change.
            const furn_str_id fid( found_id );
            if( fid.is_valid() ) {
                const furn_t &f = fid.obj();
                if( !f.is_movable() ) {
                    seed = simple_point_hash( here.getabs( pos ) );
                }
            }
        }
        break;
        case C_ITEM:
        case C_TRAP:
            if( seed_for_animation ) {
                seed_from_map_coords = true;
            }
            // TODO: come up with ways to make random sprites consistent for these types
            break;
        case C_OVERMAP_TERRAIN:
            seed = simple_point_hash( pos );
            break;
        case C_NONE:
        case C_BULLET:
        case C_HIT_ENTITY:
        case C_WEATHER:
            // TODO: come up with ways to make random sprites consistent for these types
            break;
        case C_MONSTER:
            // FIXME: add persistent id to Creature type, instead of using monster pointer address
            if( !monster_override.contains( pos ) ) {
                seed = reinterpret_cast<uintptr_t>( g->critter_at<monster>( pos ) );
            }
            break;
        default:
            // player
            if( found_id.starts_with( "player_" ) ) {
                seed = g->u.name[0];
                break;
            }
            // NPC
            if( found_id.starts_with( "npc_" ) ) {
                if( npc *const guy = g->critter_at<npc>( pos ) ) {
                    seed = guy->getID().get_value();
                    break;
                }
            }
    }

    // make sure we aren't going to rotate the tile if it shouldn't be rotated
    if( !display_tile.rotates
        && tile.category != C_NONE
        && tile.category != C_MONSTER
        && tile.category != C_BULLET ) {
        true_rota = 0;
    }

    unsigned int loc_rand = 0;
    // only bother mixing up a hash/random value if the tile has some sprites to randomly pick between
    // or has an idle animation and idle animations are enabled
    if( has_variations && variations_enabled ) {
        if( seed_from_map_coords ) {
            seed = simple_point_hash( g->m.getabs( pos ) );
        }
        static const auto rot32 = []( const unsigned int x, const int k ) {
            return ( x << k ) | ( x >> ( 32 - k ) );
        };
        // use a fair mix function to turn the "random" seed into a random int
        // taken from public domain code at http://burtleburtle.net/bob/c/lookup3.c 2015/12/11
        unsigned int a = seed, b = -seed, c = seed * seed;
        c ^= b;
        c -= rot32( b, 14 );
        a ^= c;
        a -= rot32( c, 11 );
        b ^= a;
        b -= rot32( a, 25 );
        c ^= b;
        c -= rot32( b, 16 );
        a ^= c;
        a -= rot32( c, 4 );
        b ^= a;
        b -= rot32( a, 14 );
        c ^= b;
        c -= rot32( b, 24 );
        loc_rand = c;

        // idle tile animations:
        if( display_tile.animated ) {
            idle_animations.mark_present();
            // offset by loc_rand so that everything does not blink at the same time:
            int frame = idle_animations.current_frame() + loc_rand;
            int frames_in_loop = display_tile.sprite.fg.get_weight();
            if( frames_in_loop == 1 ) {
                frames_in_loop = display_tile.sprite.bg.get_weight();
            }
            // loc_rand is actually the weighed index of the selected tile, and
            // for animations the "weight" is the number of frames to show the tile for:
            loc_rand = frame % frames_in_loop;
        }
    }

    //To make first layer of overlays more opaque and easy to distinguish
    overlay_count = overlay_count + ( overlay_count > 0 );
    //Overmap overlays usually have higher counts, so make them less opaque
    const int base_overlay_alpha = tile.category == C_OVERMAP_TERRAIN ? 12 : 24;

    //Let's branch transparent overmaps early if tranparency overlays are enabled
    //Because if tranparency is enabled then backgrounds should not be drawn
    if( tile.category == C_OVERMAP_TERRAIN
        && display_tile.has_om_transparency
        && overmap_transparency ) {
        draw_sprite_at( display_tile, screen_pos, loc_rand, /*fg:*/ true,
                        true_rota, fg_color, ll, apply_visual_effects,
                        base_overlay_alpha * overlay_count, height_3d );
        return true;
    }

    //draw it!
    draw_tile_at( display_tile, screen_pos, loc_rand, true_rota,
                  bg_color, fg_color, ll, apply_visual_effects, height_3d,
                  base_overlay_alpha * overlay_count );

    return true;
}

void cata_tiles::draw_om_tile_recursively( const tripoint_abs_omt omp, const std::string &id,
        int rotation, int subtile, int base_z_offset )
{
    const tile_search_params tile { id, C_OVERMAP_TERRAIN, "overmap_terrain", subtile, rotation };
    const auto tt = tile_type_search( tile );
    if( tt == std::nullopt ) {
        return;
    }

    if( tt->tt->has_om_transparency ) {
        //So current tile has transparent pixels, so we need to render below one first
        const tripoint_abs_omt new_pos = omp + tripoint( 0, 0, -1 );
        int new_rotation = 0, new_subtile = 0;
        const std::string new_id = get_omt_id_rotation_and_subtile( new_pos, new_rotation, new_subtile );
        draw_om_tile_recursively( new_pos, new_id, new_rotation, new_subtile, base_z_offset + 1 );
    }

    auto [bgCol, fgCol] = get_overmap_color( overmap_buffer, omp );

    const lit_level ll = overmap_buffer.is_explored( omp ) ? lit_level::LOW : lit_level::LIT;
    draw_from_id_string(
        tile, omp.raw(), bgCol, fgCol,
        ll, false, base_z_offset, false );
}

bool cata_tiles::draw_sprite_at( const tile_type &tile, point p,
                                 unsigned int loc_rand, bool is_fg, int rota,
                                 std::optional<SDL_Color> color, lit_level ll,
                                 bool apply_visual_effects, int overlay_count,
                                 int &height_3d )
{


    const auto &sv_list_sprite = is_fg ? tile.sprite.fg : tile.sprite.bg;
    const auto picked_sprite_list = sv_list_sprite.pick( loc_rand );
    if( !picked_sprite_list ) {
        return true;
    }
    const auto &sprite_list = *picked_sprite_list;
    if( sprite_list.empty() ) {
        return true;
    }

    const auto &sv_list_tint_mask = is_fg ? tile.masks.tint.fg : tile.masks.tint.bg;
    const auto picked_tint_mask_list = sv_list_tint_mask.pick( loc_rand );
    if( !picked_tint_mask_list ) {
        debugmsg( "Failed to load tint mask" );
        return true;
    }
    const auto &tint_mask_list = *picked_tint_mask_list;
    if( tint_mask_list.size() != sprite_list.size() ) {
        debugmsg( "Sprite and mask lists size mismatch" );
        return true;
    }

    /*
     * If single tile:
     *   Don't rotate a background tile
     *   Rotate foreground
     * If multiple tiles:
     *   Don't apply sprite rotation in SDL
     *   2: tiles, tile 0 is N/S, tile 1 is E/W
     *   4: tiles, 0=N, 1=E, 2=S, 3=W
     *   5+ tiles: Animation, should not be used for rotation
     */
    const auto num_sprites = sprite_list.size();
    const auto is_single_sprite = num_sprites == 1;
    const auto rotate_sprite = is_fg && is_single_sprite;
    const auto sprite_num = is_single_sprite
                            ? 0
                            : ( rota % num_sprites );

    tileset_fx_type fx_type;
    if( ll == lit_level::MEMORIZED ) {
        fx_type = tileset_fx_type::memory;
    } else if( apply_visual_effects && nv_goggles_activated ) {
        fx_type = ll == lit_level::LOW
                  ? tileset_fx_type::night
                  : tileset_fx_type::overexposed;
    } else if( overlay_count > 0 && static_z_effect ) {
        fx_type = tileset_fx_type::z_overlay;
        color = std::nullopt;
    } else if( apply_visual_effects && g->u.is_underwater() ) {
        fx_type = ll == lit_level::LOW
                  ? tileset_fx_type::underwater_dark
                  : tileset_fx_type::underwater;
    } else if( ll == lit_level::LOW ) {
        fx_type = tileset_fx_type::shadow;
    } else {
        fx_type = tileset_fx_type::none;
    }

    const int tile_idx = sprite_list[sprite_num];
    const int mask_idx = tint_mask_list[sprite_num];

    const auto default_color = tile.default_tint.value_or( TILESET_NO_COLOR );

    SDL_Color tint_color;
    if( tile.flags.contains( flag_TINT_NONE ) ) {
        tint_color = TILESET_NO_COLOR;
    } else if( is_fg && tile.flags.contains( flag_TINT_NO_FG ) ) {
        tint_color = TILESET_NO_COLOR;
    } else if( !is_fg && tile.flags.contains( flag_TINT_NO_BG ) ) {
        tint_color = TILESET_NO_COLOR;
    } else {
        tint_color = color.value_or( default_color );
    }

    const texture *sprite_tex = tileset_ptr->get_or_default( tile_idx, mask_idx, fx_type, tint_color );

    int width = 0;
    int height = 0;
    std::tie( width, height ) = sprite_tex->dimension();

    SDL_Rect destination;
    destination.x = p.x + tile.offset.x * tile_width / tileset_ptr->get_tile_width();
    destination.y = p.y + ( tile.offset.y - height_3d ) * tile_width / tileset_ptr->get_tile_width();
    destination.w = width * tile_width / tileset_ptr->get_tile_width();
    destination.h = height * tile_height / tileset_ptr->get_tile_height();

    auto render = [&]( const int rotation, const SDL_RendererFlip flip ) {
        sprite_tex->set_alpha_mod( 255 );
        const int ret = sprite_tex->render_copy_ex( renderer, &destination, rotation, nullptr, flip );
        if( !static_z_effect && overlay_count > 0 ) {
            const auto overlay =
                tileset_ptr->get_or_default(
                    tile_idx, TILESET_NO_MASK, tileset_fx_type::z_overlay, TILESET_NO_COLOR );
            if( overlay ) {
                overlay->set_alpha_mod( std::min( 192, overlay_count ) );
                overlay->render_copy_ex( renderer, &destination, rotation, nullptr, flip );
            }
        }
        return ret;
    };

    int ret = 0;
    if( rotate_sprite ) {
        switch( rota ) {
            default:
            case 0:
                // unrotated (and 180, with just two sprites)
                ret = render( 0, SDL_FLIP_NONE );
                break;
            case 1:
                // 90 degrees (and 270, with just two sprites)
                if( !tile_iso ) {
                    // never rotate isometric tiles
                    ret = render( -90, SDL_FLIP_NONE );
                } else {
                    ret = render( 0, SDL_FLIP_NONE );
                }
                break;
            case 2:
                // 180 degrees, implemented with flips instead of rotation
                if( !tile_iso ) {
                    // never flip isometric tiles vertically
                    ret = render( 0, static_cast<SDL_RendererFlip>( SDL_FLIP_HORIZONTAL | SDL_FLIP_VERTICAL ) );
                } else {
                    ret = render( 0, SDL_FLIP_NONE );
                }
                break;
            case 3:
                // 270 degrees
                if( !tile_iso ) {
                    // never rotate isometric tiles
                    ret = render( 90, SDL_FLIP_NONE );
                } else {
                    ret = render( 0, SDL_FLIP_NONE );
                }
                break;
            case 4:
                // flip horizontally
                ret = render( 0, SDL_FLIP_HORIZONTAL );
                break;
            case 5:
                // 45 degrees
                if( !tile_iso ) {
                    // never rotate isometric tiles
                    ret = render( 45, SDL_FLIP_NONE );
                } else {
                    ret = render( 0, SDL_FLIP_NONE );
                }
                break;
            case 6:
                // 315 degrees
                if( !tile_iso ) {
                    // never rotate isometric tiles
                    ret = render( -45, SDL_FLIP_NONE );
                } else {
                    ret = render( 0, SDL_FLIP_NONE );
                }
                break;
            case 7:
                // 225 degrees
                if( !tile_iso ) {
                    // never rotate isometric tiles
                    ret = render( -135, SDL_FLIP_NONE );
                } else {
                    ret = render( 0, SDL_FLIP_NONE );
                }
                break;
            case 8:
                // 135 degrees
                if( !tile_iso ) {
                    // never rotate isometric tiles
                    ret = render( 135, SDL_FLIP_NONE );
                } else {
                    ret = render( 0, SDL_FLIP_NONE );
                }
                break;
        }
    } else {
        // don't rotate, same as case 0 above
        ret = render( 0, SDL_FLIP_NONE );
    }

    printErrorIf( ret != 0, "SDL_RenderCopyEx() failed" );
    // this reference passes all the way back up the call chain back to
    // cata_tiles::draw() std::vector<tile_render_info> draw_points[].height_3d
    // where we are accumulating the height of every sprite stacked up in a tile
    height_3d += tile.height_3d;
    return true;
}

bool cata_tiles::draw_tile_at( const tile_type &tile, point p,
                               unsigned int loc_rand, int rota,
                               std::optional<SDL_Color> bg_color,
                               std::optional<SDL_Color> fg_color, lit_level ll,
                               bool apply_visual_effects, int &height_3d,
                               int overlay_count )
{
    draw_sprite_at( tile, p, loc_rand, /*fg:*/ false, rota, bg_color, ll,
                    apply_visual_effects, overlay_count );
    draw_sprite_at( tile, p, loc_rand, /*fg:*/ true, rota, fg_color, ll,
                    apply_visual_effects, overlay_count, height_3d );
    return true;
}

bool cata_tiles::draw_color_at( const SDL_Color &color, point pos, SDL_BlendMode blend_mode )
{
    SDL_Rect rect{
        pos.x,
        pos.y,
        tile_width,
        tile_height
    };

    SDL_BlendMode old_blend_mode;
    GetRenderDrawBlendMode( renderer, old_blend_mode );
    SetRenderDrawBlendMode( renderer, blend_mode );
    SetRenderDrawColor( renderer, color.r, color.g, color.b, color.a );
    RenderFillRect( renderer, &rect );
    SetRenderDrawBlendMode( renderer, old_blend_mode );
    return true;
}

bool cata_tiles::would_apply_vision_effects( const visibility_type visibility ) const
{
    return visibility != VIS_CLEAR;
}

bool cata_tiles::apply_vision_effects( const tripoint &pos,
                                       const visibility_type visibility )
{
    if( !would_apply_vision_effects( visibility ) ) {
        return false;
    }
    const std::string *light_name = nullptr;
    switch( visibility ) {
        case VIS_HIDDEN: {
            light_name = &STATIC( std::string( "lighting_hidden" ) );
            break;
        }
        case VIS_LIT: {
            light_name = &STATIC( std::string( "lighting_lowlight_light" ) );
            break;
        }
        case VIS_BOOMER: {
            light_name = &STATIC( std::string( "lighting_boomered_light" ) );
            break;
        }
        case VIS_BOOMER_DARK: {
            light_name = &STATIC( std::string( "lighting_boomered_dark" ) );
            break;
        }
        case VIS_DARK: {
            light_name = &STATIC( std::string( "lighting_lowlight_dark" ) );
            break;
        }
        case VIS_CLEAR:
            // should never happen
            break;
    }

    // lighting is never rotated, though, could possibly add in random rotation?
    const tile_search_params tile { *light_name, C_LIGHTING, empty_string, 0, 0 };
    draw_from_id_string(
        tile, pos, std::nullopt, std::nullopt,
        lit_level::LIT, false, 0, false );

    return true;
}

template<typename T>
auto get_map_memory_of_at( const tripoint &p ) -> std::optional<memorized_terrain_tile>
{
    if( !g->u.should_show_map_memory() ) {
        return std::nullopt;
    }

    const memorized_terrain_tile t = g->u.get_memorized_tile( get_map().getabs( p ) );
    if( !string_id<T>( t.tile ).is_valid() ) {
        return std::nullopt;
    }

    return t;
}

template<>
auto get_map_memory_of_at<vpart_info>( const tripoint &p ) -> std::optional<memorized_terrain_tile>
{
    if( !g->u.should_show_map_memory() ) {
        return std::nullopt;
    }

    const memorized_terrain_tile t = g->u.get_memorized_tile( get_map().getabs( p ) );
    if( !t.tile.starts_with( "vp_" ) ) {
        return std::nullopt;
    }

    const auto actual_part = t.tile.substr( 3 );
    if( !string_id<vpart_info>( actual_part ).is_valid() ) {
        return std::nullopt;
    }

    return t;
}

bool cata_tiles::has_memory_at( const tripoint &p )
{
    if( !g->u.should_show_map_memory() ) {
        return false;
    }

    const memorized_terrain_tile t = g->u.get_memorized_tile( get_map().getabs( p ) );
    return !t.tile.empty();
}

auto cata_tiles::get_ter_memory_at( const tripoint &p ) -> std::optional<memorized_terrain_tile>
{
    return get_map_memory_of_at<ter_t>( p );
}

auto cata_tiles::get_furn_memory_at( const tripoint &p ) -> std::optional<memorized_terrain_tile>
{
    return get_map_memory_of_at<furn_t>( p );
}

auto cata_tiles::get_trap_memory_at( const tripoint &p ) -> std::optional<memorized_terrain_tile>
{
    return get_map_memory_of_at<trap>( p );
}

auto cata_tiles::get_vpart_memory_at( const tripoint &p ) -> std::optional<memorized_terrain_tile>
{
    return get_map_memory_of_at<vpart_info>( p );
}

bool cata_tiles::draw_block( const tripoint &p, SDL_Color color, int scale )
{
    SDL_Rect rect;
    rect.h = tile_width / scale;
    rect.w = tile_height / scale;

    if( tile_iso ) {
        rect.h = ( rect.h * 2 ) / 3;
        rect.w = ( rect.w * 3 ) / 4;
    }
    // translate from player-relative to screen relative tile position
    point screen;
    if( tile_iso ) {
        screen.x = ( ( p.x - o.x ) - ( o.y - p.y ) + screentile_width - 2 ) * tile_width / 2 +
                   op.x;
        // y uses tile_width because width is definitive for iso tiles
        // tile footprints are half as tall as wide, arbitrarily tall
        screen.y = ( ( p.y - o.y ) - ( p.x - o.x ) - 4 ) * tile_width / 4 +
                   screentile_height * tile_height / 2 + // TODO: more obvious centering math
                   op.y;
    } else {
        screen.x = ( p.x - o.x ) * tile_width + op.x;
        screen.y = ( p.y - o.y ) * tile_height + op.y;
    }
    rect.x = screen.x + ( tile_width - rect.w ) / 2;
    rect.y = screen.y + ( tile_height - rect.h ) / 2;
    if( tile_iso ) {
        rect.y += tile_height / 8;
    }

    geometry->rect( renderer, rect,  color );
    return true;
}

bool cata_tiles::draw_terrain( const tripoint &p, const lit_level ll, int &height_3d,
                               const bool ( &invisible )[5], int z_drop )
{
    map &here = get_map();
    const auto override = terrain_override.find( p );
    const bool overridden = override != terrain_override.end();
    bool neighborhood_overridden = overridden;
    if( !neighborhood_overridden ) {
        for( point dir : neighborhood ) {
            if( terrain_override.contains( p + dir ) ) {
                neighborhood_overridden = true;
                break;
            }
        }
    }

    const ter_id &t = here.ter( p );
    const auto [bgCol, fgCol] = get_terrain_color( t.obj(), here, p );

    // first memorize the actual terrain
    if( t && !invisible[0] ) {
        int subtile = 0;
        int rotation = 0;
        int connect_group = 0;
        if( t.obj().connects( connect_group ) ) {
            get_connect_values( p, subtile, rotation, connect_group, {} );
            // re-memorize previously seen terrain in case new connections have been seen
            here.set_memory_seen_cache_dirty( p );
        } else {
            get_terrain_orientation( p, rotation, subtile, {}, invisible );
            // do something to get other terrain orientation values
        }
        const std::string &tname = t.id().str();
        if( here.check_seen_cache( p ) ) {
            if( !t->has_flag( TFLAG_NO_MEMORY ) && !t->has_flag( TFLAG_Z_TRANSPARENT ) ) {
                g->u.memorize_tile( here.getabs( p ), tname, subtile, rotation );
            } else {
                g->u.clear_memorized_tile( here.getabs( p ) );
            }
        }
        // draw the actual terrain if there's no override
        if( !neighborhood_overridden ) {
            // Open air is used for holes / sky. Drawing a cyan marker here can bleed through
            // semi-transparent sprites (e.g. explosion smoke) and look like stuck artifacts.
            // If a tileset provides an explicit tile for it, use that; otherwise draw nothing.
            if( t == t_open_air ) {
                if( tileset_ptr && tileset_ptr->find_tile_type( tname ) ) {
                    const auto tile = tile_search_params{ tname, C_TERRAIN, empty_string, 0, 0 };
                    return draw_from_id_string(
                               tile, p, bgCol, fgCol,
                               ll, true, z_drop, false, height_3d );
                }
                return true;
            }

            const auto tile = tile_search_params{ .id = tname, .category = C_TERRAIN, .subcategory = empty_string, .subtile = subtile, .rota = rotation };
            return draw_from_id_string(
                       tile, p, bgCol, fgCol,
                       ll, true, z_drop, false, height_3d );
        }
    }
    if( invisible[0] ? overridden : neighborhood_overridden ) {
        // and then draw the override terrain
        const ter_id &t2 = overridden ? override->second : t;
        if( t2 ) {
            // both the current and neighboring overrides may change the appearance
            // of the tile, so always re-calculate it.
            int subtile = 0;
            int rotation = 0;
            int connect_group = 0;
            if( t2.obj().connects( connect_group ) ) {
                get_connect_values( p, subtile, rotation, connect_group, terrain_override );
            } else {
                get_terrain_orientation( p, rotation, subtile, terrain_override, invisible );
            }
            const std::string &tname = t2.id().str();
            // tile overrides are never memorized
            // tile overrides are always shown with full visibility
            const lit_level lit = overridden ? lit_level::LIT : ll;
            const bool nv = !overridden;
            const tile_search_params tile { tname, C_TERRAIN, empty_string, subtile, rotation };
            return draw_from_id_string(
                       tile, p, bgCol, fgCol,
                       lit, nv, z_drop, false, height_3d );
        }
    } else if( invisible[0] ) {
        // try drawing memory if invisible and not overridden
        const auto ret = get_ter_memory_at( p );
        if( ret.has_value() ) {
            const auto& [tile_id, subtile, rotation] = ret.value();
            const tile_search_params tile { tile_id, C_TERRAIN, empty_string, subtile, rotation };
            return draw_from_id_string(
                       tile, p, bgCol, fgCol,
                       lit_level::MEMORIZED, true, z_drop, false, height_3d );
        }
    }
    return false;
}

bool cata_tiles::draw_furniture( const tripoint &p, const lit_level ll, int &height_3d,
                                 const bool ( &invisible )[5], int z_drop )
{
    const auto override = furniture_override.find( p );
    const bool overridden = override != furniture_override.end();
    bool neighborhood_overridden = overridden;
    if( !neighborhood_overridden ) {
        for( point dir : neighborhood ) {
            if( furniture_override.contains( p + dir ) ) {
                neighborhood_overridden = true;
                break;
            }
        }
    }

    const map &here = get_map();
    const furn_id &f = here.furn( p );
    const auto [bgCol, fgCol] = get_furniture_color( f.obj(), here, p );

    // first memorize the actual furniture
    if( f && !invisible[0] ) {
        const int neighborhood[4] = {
            static_cast<int>( here.furn( p + point_south ) ),
            static_cast<int>( here.furn( p + point_east ) ),
            static_cast<int>( here.furn( p + point_west ) ),
            static_cast<int>( here.furn( p + point_north ) )
        };
        int subtile = 0;
        int rotation = 0;

        int connect_group = 0;
        if( f.obj().connects( connect_group ) ) {
            get_furn_connect_values( p, subtile, rotation, connect_group, {} );
        } else {
            get_tile_values_with_ter( p, f.to_i(), neighborhood, subtile, rotation );
        }

        const std::string &fname = f.id().str();
        if( here.check_seen_cache( p ) ) {
            g->u.memorize_tile( here.getabs( p ), fname, subtile, rotation );
        }
        // draw the actual furniture if there's no override
        if( !neighborhood_overridden ) {
            const tile_search_params tile { fname, C_FURNITURE, empty_string, subtile, rotation};
            return draw_from_id_string(
                       tile, p, bgCol, fgCol,
                       ll, true, z_drop, false, height_3d );
        }
    }
    if( invisible[0] ? overridden : neighborhood_overridden ) {
        // and then draw the override furniture
        const furn_id &f2 = overridden ? override->second : f;
        if( f2 ) {
            // both the current and neighboring overrides may change the appearance
            // of the tile, so always re-calculate it.
            const auto furn = [&]( const tripoint & q, const bool invis ) -> furn_id {
                const auto it = furniture_override.find( q );
                return it != furniture_override.end() ? it->second :
                ( !overridden || !invis ) ? here.furn( q ) : f_null;
            };
            const int neighborhood[4] = {
                static_cast<int>( furn( p + point_south, invisible[1] ) ),
                static_cast<int>( furn( p + point_east, invisible[2] ) ),
                static_cast<int>( furn( p + point_west, invisible[3] ) ),
                static_cast<int>( furn( p + point_north, invisible[4] ) )
            };
            int subtile = 0;
            int rotation = 0;

            int connect_group = 0;
            if( f.obj().connects( connect_group ) ) {
                get_furn_connect_values( p, subtile, rotation, connect_group, {} );
            } else {
                get_tile_values_with_ter( p, f.to_i(), neighborhood, subtile, rotation );
            }

            get_tile_values( f2.to_i(), neighborhood, subtile, rotation );
            const std::string &fname = f2.id().str();
            // tile overrides are never memorized
            // tile overrides are always shown with full visibility
            const lit_level lit = overridden ? lit_level::LIT : ll;
            const bool nv = !overridden;
            const tile_search_params tile { fname, C_FURNITURE, empty_string, subtile, rotation };
            return draw_from_id_string(
                       tile, p, bgCol, fgCol,
                       lit, nv, z_drop, false, height_3d );
        }
    } else if( invisible[0] ) {
        // try drawing memory if invisible and not overridden
        const auto ret = get_furn_memory_at( p );
        if( ret.has_value() ) {
            const auto& [tile_id, subtile, rotation] = ret.value();
            const tile_search_params tile { tile_id, C_FURNITURE, empty_string, subtile, rotation };
            return draw_from_id_string(
                       tile, p, bgCol, fgCol,
                       lit_level::MEMORIZED, true, z_drop, false, height_3d );
        }
    }
    return false;
}

bool cata_tiles::draw_trap( const tripoint &p, const lit_level ll, int &height_3d,
                            const bool ( &invisible )[5], int z_drop )
{
    const auto override = trap_override.find( p );
    const bool overridden = override != trap_override.end();
    bool neighborhood_overridden = overridden;
    if( !neighborhood_overridden ) {
        for( point dir : neighborhood ) {
            if( trap_override.contains( p + dir ) ) {
                neighborhood_overridden = true;
                break;
            }
        }
    }

    map &here = get_map();
    const trap &tr = here.tr_at( p );
    const trap_id &tr_id = tr.loadid;
    auto [bgCol, fgCol] = get_trap_color( tr, here, p );

    // first memorize the actual trap
    if( tr_id && !invisible[0] && tr_id.obj().can_see( p, g->u ) ) {
        const int neighborhood[4] = {
            static_cast<int>( here.tr_at( p + point_south ).loadid ),
            static_cast<int>( here.tr_at( p + point_east ).loadid ),
            static_cast<int>( here.tr_at( p + point_west ).loadid ),
            static_cast<int>( here.tr_at( p + point_north ).loadid )
        };
        int subtile = 0;
        int rotation = 0;
        get_tile_values( tr_id.to_i(), neighborhood, subtile, rotation );
        const std::string trname = tr_id.id().str();
        if( here.check_seen_cache( p ) && tr_id != tr_ledge ) {
            g->u.memorize_tile( here.getabs( p ), trname, subtile, rotation );
        }
        // draw the actual trap if there's no override
        if( !neighborhood_overridden ) {
            const tile_search_params tile { trname, C_TRAP, empty_string, subtile, rotation };
            return draw_from_id_string(
                       tile, p, bgCol, fgCol,
                       ll, true, z_drop, false, height_3d );
        }
    }
    if( overridden || ( !invisible[0] && neighborhood_overridden && tr_id.obj().can_see( p, g->u ) ) ) {
        // and then draw the override trap
        const trap_id &tr2 = overridden ? override->second : tr_id;
        if( tr2 ) {
            // both the current and neighboring overrides may change the appearance
            // of the tile, so always re-calculate it.
            const auto tr_at = [&]( const tripoint & q, const bool invis ) -> trap_id {
                const auto it = trap_override.find( q );
                return it != trap_override.end() ? it->second :
                ( !overridden || !invis ) ? here.tr_at( q ).loadid : tr_null;
            };
            const int neighborhood[4] = {
                static_cast<int>( tr_at( p + point_south, invisible[1] ) ),
                static_cast<int>( tr_at( p + point_east, invisible[2] ) ),
                static_cast<int>( tr_at( p + point_west, invisible[3] ) ),
                static_cast<int>( tr_at( p + point_north, invisible[4] ) )
            };
            int subtile = 0;
            int rotation = 0;
            get_tile_values( tr2.to_i(), neighborhood, subtile, rotation );
            const std::string &trname = tr2.id().str();
            // tile overrides are never memorized
            // tile overrides are always shown with full visibility
            const lit_level lit = overridden ? lit_level::LIT : ll;
            const bool nv = !overridden;
            const tile_search_params tile { trname, C_TRAP, empty_string, subtile, rotation };
            return draw_from_id_string(
                       tile, p, bgCol, fgCol,
                       lit, nv, z_drop, false, height_3d );
        }
    } else if( invisible[0] ) {
        // try drawing memory if invisible and not overridden
        const auto ret = get_trap_memory_at( p );
        if( ret.has_value() ) {
            const auto& [tile_id, subtile, rotation] = ret.value();
            const tile_search_params tile2 { tile_id, C_TRAP, empty_string, subtile, rotation };
            return draw_from_id_string(
                       tile2, p, bgCol, fgCol,
                       lit_level::MEMORIZED, true, z_drop, false, height_3d );
        }
    }
    return false;
}

bool cata_tiles::draw_graffiti( const tripoint &p, const lit_level ll, int &height_3d,
                                const bool ( &invisible )[5], int z_drop )
{
    const auto override = graffiti_override.find( p );
    const bool overridden = override != graffiti_override.end();
    if( overridden ? !override->second : ( invisible[0] || !get_map().has_graffiti_at( p ) ) ) {
        return false;
    }
    const lit_level lit = overridden ? lit_level::LIT : ll;

    const auto [bgCol, fgCol] = get_graffiti_color( get_map(), p );

    const tile_search_params tile { "graffiti", C_NONE, empty_string, 0, 0 };
    return draw_from_id_string(
               tile, p,  bgCol, fgCol,
               lit, false, z_drop, false, height_3d );
}

bool cata_tiles::draw_field_or_item( const tripoint &p, const lit_level ll, int &height_3d,
                                     const bool ( &invisible )[5], int z_drop )
{
    if( !fov_3d && z_drop > 0 ) {
        return false;
    }
    const auto fld_override = field_override.find( p );
    const bool fld_overridden = fld_override != field_override.end();
    map &here = get_map();
    const field_type_id &fld = fld_overridden ?
                               fld_override->second : here.field_at( p ).displayed_field_type();

    bool ret_draw_field = false;
    bool ret_draw_items = false;
    if( ( fld_overridden || !invisible[0] ) && fld.obj().display_field ) {
        const lit_level lit = fld_overridden ? lit_level::LIT : ll;
        const bool nv = !fld_overridden;

        auto field_at = [&]( const tripoint & q, const bool invis ) -> field_type_id {
            const auto it = field_override.find( q );
            return it != field_override.end() ? it->second :
            ( !fld_overridden || !invis ) ? here.field_at( q ).displayed_field_type() : fd_null;
        };
        // for rotation information
        const int neighborhood[4] = {
            static_cast<int>( field_at( p + point_south, invisible[1] ) ),
            static_cast<int>( field_at( p + point_east, invisible[2] ) ),
            static_cast<int>( field_at( p + point_west, invisible[3] ) ),
            static_cast<int>( field_at( p + point_north, invisible[4] ) )
        };

        int subtile = 0;
        int rotation = 0;
        get_tile_values( fld.to_i(), neighborhood, subtile, rotation );

        const auto [bgCol, fgCol] = get_field_color( here.field_at( p ), here, p );

        const tile_search_params tile { fld.id().str(), C_FIELD, empty_string, subtile, rotation };
        ret_draw_field = draw_from_id_string(
                             tile, p, bgCol, fgCol,
                             lit, nv, z_drop, false );
    }
    if( fld.obj().display_items ) {
        const auto it_override = item_override.find( p );
        const bool it_overridden = it_override != item_override.end();

        std::optional<SDL_Color> bgCol = std::nullopt;
        std::optional<SDL_Color> fgCol = std::nullopt;

        itype_id it_id;
        mtype_id mon_id;
        bool hilite;
        const itype *it_type;
        if( it_overridden ) {
            it_id = std::get<0>( it_override->second );
            mon_id = std::get<1>( it_override->second );
            hilite = std::get<2>( it_override->second );
            it_type = &*it_id;
        } else if( !invisible[0] && here.sees_some_items( p, g->u ) ) {
            const maptile &tile = here.maptile_at( p );
            const item &itm = tile.get_uppermost_item();
            const mtype *const mon = itm.get_mtype();
            it_id = itm.typeId();
            mon_id = mon ? mon->id : mtype_id::NULL_ID();
            hilite = tile.get_item_count() > 1;
            it_type = itm.type;

            const auto [b, f] = get_item_color( itm, here, p );
            fgCol = b;
            bgCol = f;
        } else {
            it_type = nullptr;
            hilite = false;
        }

        if( it_type && !it_id.is_null() ) {
            const std::string disp_id = it_id == itype_corpse && mon_id
                                        ? "corpse_" + mon_id.str()
                                        : it_id.str();
            const std::string it_category = it_type->get_item_type_string();
            const lit_level lit = it_overridden ? lit_level::LIT : ll;
            const bool nv = !it_overridden;

            const tile_search_params tile { disp_id, C_ITEM, it_category, 0, 0 };
            ret_draw_items = draw_from_id_string(
                                 tile, p, bgCol, fgCol,
                                 lit, nv, z_drop, false, height_3d );
            if( ret_draw_items && hilite ) {
                draw_item_highlight( p );
            }
        }
    }
    return ret_draw_field && ret_draw_items;
}

bool cata_tiles::draw_vpart( const tripoint &p, lit_level ll, int &height_3d,
                             const bool ( &invisible )[5], int z_drop )
{
    const auto override = vpart_override.find( p );
    const bool overridden = override != vpart_override.end();
    map &here = get_map();
    // first memorize the actual vpart
    const optional_vpart_position vp = here.veh_at( p );

    const auto [bgCol, fgCol] = get_vpart_color( vp, here, p );

    if( vp && !invisible[0] ) {
        const vehicle &veh = vp->vehicle();
        int veh_part = vp->part_index();

        // Gets the visible part, should work fine once tileset vp_ids are updated to work with the vehicle part json ids
        // get the vpart_id
        char part_mod = 0;
        const Creature *critter = g->critter_at( p, true );
        const vpart_id &vp_id = veh.part_id_string( veh_part, z_drop > 0 && critter == nullptr, part_mod );
        const int subtile = part_mod == 1 ? open_ : part_mod == 2 ? broken : 0;
        const int rotation = std::round( to_degrees( veh.face.dir() ) );
        const std::string vpname = "vp_" + vp_id.str();
        avatar &you = get_avatar();
        if( !veh.forward_velocity() && !veh.player_in_control( you ) &&
            here.check_seen_cache( p ) ) {
            you.memorize_tile( here.getabs( p ), vpname, subtile, rotation );
        }
        if( !overridden ) {
            const std::optional<vpart_reference> cargopart = vp.part_with_feature( "CARGO", true );
            const bool draw_highlight =
                cargopart && !veh.get_items( cargopart->part_index() ).empty();
            const tile_search_params tile = {vpname, C_VEHICLE_PART, empty_string, subtile, rotation};
            const bool ret = draw_from_id_string(
                                 tile, p, bgCol, fgCol,
                                 ll, true, z_drop, false, height_3d );
            if( ret && draw_highlight ) {
                draw_item_highlight( p );
            }
            return ret;
        }
    }

    if( overridden ) {
        // and then draw the override vpart
        const vpart_id &vp2 = std::get<0>( override->second );
        if( vp2 ) {
            const char part_mod = std::get<1>( override->second );
            const int subtile = part_mod == 1 ? open_ : part_mod == 2 ? broken : 0;
            const units::angle rotation = std::get<2>( override->second );
            const int draw_highlight = std::get<3>( override->second );
            const std::string vpname = "vp_" + vp2.str();
            // tile overrides are never memorized
            // tile overrides are always shown with full visibility
            const tile_search_params tile = {
                vpname, C_VEHICLE_PART, empty_string, subtile,
                static_cast<int>( std::round( to_degrees( rotation ) ) )
            };

            const bool ret = draw_from_id_string(
                                 tile, p, bgCol, fgCol,
                                 lit_level::LIT, false, z_drop, false, height_3d );
            if( ret && draw_highlight ) {
                draw_item_highlight( p );
            }
            return ret;
        }
    } else if( invisible[0] ) {
        // try drawing memory if invisible and not overridden
        const auto ret = get_vpart_memory_at( p );
        if( ret.has_value() ) {
            const auto [tile_id, subtile, rotation] = ret.value();
            const tile_search_params tile { tile_id, C_VEHICLE_PART, empty_string, subtile, rotation };
            return draw_from_id_string(
                       tile, p, bgCol, fgCol,
                       lit_level::MEMORIZED, true, z_drop, false, height_3d );
        }
    }
    return false;
}

bool cata_tiles::draw_critter_at( const tripoint &p, lit_level ll, int &height_3d,
                                  const bool ( &invisible )[5], int z_drop )
{
    if( !fov_3d && z_drop > 0 ) {
        return false;
    }
    bool result;
    bool is_player;
    bool sees_player;
    Attitude attitude;
    const auto override = monster_override.find( p );
    if( override != monster_override.end() ) {
        const mtype_id id = std::get<0>( override->second );
        if( !id ) {
            return false;
        }
        is_player = false;
        sees_player = false;
        attitude = std::get<3>( override->second );
        const std::string &chosen_id = id.str();
        const std::string &ent_subcategory = id.obj().species.empty() ?
                                             empty_string : id.obj().species.begin()->str();
        const tile_search_params tile = { chosen_id, C_MONSTER, ent_subcategory, corner, 0 };
        result = draw_from_id_string(
                     tile, p, std::nullopt, std::nullopt,
                     lit_level::LIT, false, z_drop, false, height_3d );
    } else if( !invisible[0] ) {
        const Creature *pcritter = g->critter_at( p, true );
        if( pcritter == nullptr ) {
            return false;
        }
        const Creature &critter = *pcritter;

        if( !g->u.sees( critter ) ) {
            if( g->u.sees_with_infrared( critter ) || g->u.sees_with_specials( critter ) ) {
                const tile_search_params tile { "infrared_creature", C_NONE, empty_string, 0, 0 };
                return draw_from_id_string(
                           tile, p, std::nullopt, std::nullopt,
                           lit_level::LIT, false, z_drop, false, height_3d );
            }
            return false;
        }
        result = false;
        sees_player = false;
        is_player = false;
        attitude = Attitude::A_ANY;
        const monster *m = dynamic_cast<const monster *>( &critter );
        if( m != nullptr ) {
            constexpr auto ent_category = C_MONSTER;
            std::string ent_subcategory = empty_string;
            if( !m->type->species.empty() ) {
                ent_subcategory = m->type->species.begin()->str();
            }
            constexpr int subtile = corner;
            // depending on the toggle flip sprite left or right
            int rot_facing = -1;
            if( m->facing == FD_RIGHT ) {
                rot_facing = 0;
            } else if( m->facing == FD_LEFT ) {
                rot_facing = 4;
            }
            if( rot_facing >= 0 ) {
                const auto ent_name = m->type->id;
                std::string chosen_id = ent_name.str();
                if( m->has_effect( effect_ridden ) ) {
                    int pl_under_height = 6;
                    if( m->mounted_player ) {
                        draw_entity_with_overlays( *m->mounted_player, p, ll, pl_under_height );
                    }
                    const std::string prefix = "rid_";
                    std::string copy_id = chosen_id;
                    const std::string ridden_id = copy_id.insert( 0, prefix );
                    const tile_type *tt = tileset_ptr->find_tile_type( ridden_id );
                    if( tt ) {
                        chosen_id = ridden_id;
                    }
                }

                const auto [bgCol, fgCol] = get_monster_color( *m, get_map(), p );

                const tile_search_params tile { chosen_id, ent_category, ent_subcategory, subtile, rot_facing };
                result = draw_from_id_string(
                             tile, p, bgCol, fgCol,
                             ll, false, z_drop, false, height_3d );
                sees_player = m->sees( g->u );
                attitude = m->attitude_to( g-> u );
            }
        }
        const player *pl = dynamic_cast<const player *>( &critter );
        if( pl != nullptr ) {
            draw_entity_with_overlays( *pl, p, ll, height_3d );
            result = true;
            if( pl->is_player() ) {
                is_player = true;
            } else {
                sees_player = pl->sees( g-> u );
                attitude = pl->attitude_to( g-> u );
            }
        }
    } else {
        // invisible
        const Creature *critter = g->critter_at( p, true );
        if( critter && ( g->u.sees_with_infrared( *critter ) || g->u.sees_with_specials( *critter ) ) ) {
            // try drawing infrared creature if invisible and not overridden
            // return directly without drawing overlay
            const tile_search_params tile { "infrared_creature", C_NONE, empty_string, 0, 0 };
            return draw_from_id_string(
                       tile, p, std::nullopt, std::nullopt,
                       lit_level::LIT, false, z_drop, false, height_3d );
        }
        return false;
    }

    if( result && !is_player ) {
        std::string draw_id = "overlay_" + Creature::attitude_raw_string( attitude );
        if( sees_player && !g->u.has_trait( trait_INATTENTIVE ) ) {
            draw_id += "_sees_player";
        }
        if( tileset_ptr->find_tile_type( draw_id ) ) {
            const tile_search_params tile { draw_id, C_NONE, empty_string, 0, 0 };
            draw_from_id_string(
                tile, p, std::nullopt, std::nullopt,
                lit_level::LIT, false, z_drop, false, height_3d );
        }
    }
    return result;

}

bool cata_tiles::draw_zone_mark( const tripoint &p, lit_level ll, int &height_3d,
                                 const bool ( &invisible )[5], int z_drop )
{
    if( invisible[0] ) {
        return false;
    }

    if( !g->is_zones_manager_open() ) {
        return false;
    }

    const zone_manager &mgr = zone_manager::get_manager();
    const tripoint &abs = get_map().getabs( p );
    const auto zone = mgr.get_bottom_zone( abs );

    if( zone && zone->has_options() ) {
        auto option = dynamic_cast<const mark_option *>( &zone->get_options() );

        if( option && !option->get_mark().empty() ) {
            const tile_search_params tile { option->get_mark(), C_NONE, empty_string, 0, 0 };
            return draw_from_id_string(
                       tile, p, std::nullopt, std::nullopt,
                       ll, true, z_drop, false, height_3d );
        }
    }

    return false;
}

bool cata_tiles::draw_zombie_revival_indicators( const tripoint &pos, const lit_level /*ll*/,
        int &/*height_3d*/, const bool ( &invisible )[5], int z_drop )
{
    map &here = get_map();
    if( tileset_ptr->find_tile_type( ZOMBIE_REVIVAL_INDICATOR ) && !invisible[0] &&
        !item_override.contains( pos ) && here.could_see_items( pos, g->u ) ) {
        for( auto &i : here.i_at( pos ) ) {
            if( i->is_corpse() ) {
                if( i->can_revive() || ( i->get_mtype()->zombify_into && !i->has_flag( flag_PULPED ) ) ) {
                    const tile_search_params tile { ZOMBIE_REVIVAL_INDICATOR, C_NONE, empty_string, 0, 0 };
                    return draw_from_id_string(
                               tile, pos, std::nullopt, std::nullopt,
                               lit_level::LIT, false, z_drop, false );
                }
            }
        }
    }
    return false;
}

void cata_tiles::draw_entity_with_overlays( const Character &ch, const tripoint &p, lit_level ll,
        int &height_3d, const bool as_independent_entity )
{
    std::string ent_name;

    if( ch.is_npc() ) {
        ent_name = ch.male ? "npc_male" : "npc_female";
    } else {
        ent_name = ch.male ? "player_male" : "player_female";
    }
    // first draw the character itself(i guess this means a tileset that
    // takes this seriously needs a naked sprite)
    int prev_height_3d = height_3d;


    if( ch.facing == FD_RIGHT || ch.facing == FD_LEFT ) {
        // depending on the toggle flip sprite left or right
        const int rota = ch.facing == FD_RIGHT ? 0 : 4;

        const auto [entity_bgCol, entity_fgCol] = get_character_color( ch, get_map(), p );
        const tile_search_params tile { ent_name, C_NONE, "", corner, rota };
        draw_from_id_string(
            tile, p, entity_bgCol, entity_fgCol,
            ll, false, 0, as_independent_entity, height_3d );
    }

    const auto &m = get_map();
    auto get_overlay_color = [&]<typename T>( T && arg ) {
        using Decayed = std::remove_reference_t<T>;
        using PtrBase = std::remove_const_t<std::remove_pointer_t<Decayed>>;
        if constexpr( std::is_same_v<PtrBase, item> ) {
            return get_item_color( *arg, m, p );
        } else if constexpr( std::is_same_v<PtrBase, effect> ) {
            return get_effect_color( *arg, ch, m, p );
        } else if constexpr( std::is_same_v<PtrBase, bionic> ) {
            return get_bionic_color( *arg, ch, m, p );
        } else if constexpr( std::is_same_v<PtrBase, mutation> ) {
            return get_mutation_color( *arg, ch, m, p );
        } else {
            return color_tint_pair{std::nullopt, std::nullopt};
        }
    };

    // next up, draw all the overlays
    auto overlays = ch.get_overlay_ids();
    for( const auto& [overlay_id, data] : overlays ) {

        const auto [overlay_bgCol, overlay_fgCol] = std::visit( get_overlay_color, data );

        std::string draw_id = overlay_id;
        if( find_overlay_looks_like( ch.male, overlay_id, draw_id ) ) {
            int overlay_height_3d = prev_height_3d;
            if( ch.facing == FD_RIGHT ) {
                const tile_search_params tile {draw_id, C_NONE, "", corner, /*rota:*/ 0};
                draw_from_id_string(
                    tile, p, overlay_bgCol, overlay_fgCol,
                    ll, false, 0, as_independent_entity, overlay_height_3d );
            } else if( ch.facing == FD_LEFT ) {
                const tile_search_params tile = {draw_id, C_NONE, "", corner, /*rota:*/ 4};
                draw_from_id_string(
                    tile, p, overlay_bgCol, overlay_fgCol,
                    ll, false, 0, as_independent_entity, overlay_height_3d );
            }
            // the tallest height-having overlay is the one that counts
            height_3d = std::max( height_3d, overlay_height_3d );
        }
    }
}

bool cata_tiles::draw_item_highlight( const tripoint &pos )
{
    const tile_search_params tile { ITEM_HIGHLIGHT, C_NONE, empty_string, 0, 0 };
    return draw_from_id_string(
               tile, pos, std::nullopt, std::nullopt,
               lit_level::LIT, false, 0, false );
}

void tileset_loader::ensure_default_item_highlight()
{
    if( ts.find_tile_type( ITEM_HIGHLIGHT ) ) {
        return;
    }
#if defined(DYNAMIC_ATLAS)
    const Uint8 highlight_alpha = 127;

    int index = offset;

    const SDL_Surface_Ptr surface = create_surface_32( ts.tile_width, ts.tile_height );
    assert( surface );
    throwErrorIf( SDL_FillRect( surface.get(), nullptr, SDL_MapRGBA( surface->format, 0, 0, 127,
                                highlight_alpha ) ) != 0, "SDL_FillRect failed" );

    auto [tex, rect] = ts.tileset_atlas->allocate_sprite( ts.tile_width, ts.tile_height );
    SDL_UpdateTexture( tex.get(), &rect, surface->pixels, surface->pitch );

    ts.tile_ids[ITEM_HIGHLIGHT].sprite.fg.add( std::vector<int>( {index} ), 1 );
    ts.tile_lookup.emplace( tileset_lookup_key{
        index,
        TILESET_NO_MASK,
        tileset_fx_type::none,
        TILESET_NO_COLOR
    }, texture( tex, rect ) );
#else
    const Uint8 highlight_alpha = 127;

    int index = ts.tile_values.size();

    const SDL_Surface_Ptr surface = create_surface_32( ts.tile_width, ts.tile_height );
    assert( surface );
    throwErrorIf( SDL_FillRect( surface.get(), nullptr, SDL_MapRGBA( surface->format, 0, 0, 127,
                                highlight_alpha ) ) != 0, "SDL_FillRect failed" );
    ts.tile_values.emplace_back( CreateTextureFromSurface( renderer, surface ), SDL_Rect{ 0, 0, ts.tile_width, ts.tile_height } );
    ts.tile_ids[ITEM_HIGHLIGHT].sprite.fg.add( std::vector<int>( {index} ), 1 );
#endif

}

/* Animation Functions */
/* -- Inits */
void cata_tiles::init_explosion( const tripoint &p, int radius, const std::string &name )
{
    do_draw_explosion = true;
    exp_pos = p;
    exp_rad = radius;
    exp_name = name;
}
void cata_tiles::init_custom_explosion_layer( const std::map<tripoint, explosion_tile> &layer,
        const std::string &name )
{
    do_draw_custom_explosion = true;
    custom_explosion_layer = layer;
    exp_name = name;
}
void cata_tiles::init_draw_cone_aoe( const tripoint &origin, const one_bucket &layer )
{
    do_draw_cone_aoe = true;
    cone_aoe_origin = origin;
    cone_aoe_layer = layer;
}
void cata_tiles::init_draw_bullet( const tripoint &p, std::string name, int rotation )
{
    do_draw_bullet = true;
    bul_pos.push_back( p );
    bul_id.push_back( std::move( name ) );
    bul_rotation.push_back( rotation );
}
void cata_tiles::init_draw_bullets( const std::vector<tripoint> &ps,
                                    const std::vector<std::string> &names, const std::vector<int> &rotations )
{
    do_draw_bullet = true;
    bul_pos.insert( bul_pos.end(), ps.begin(), ps.end() );
    bul_id.insert( bul_id.end(), names.begin(), names.end() );
    bul_rotation.insert( bul_rotation.end(), rotations.begin(), rotations.end() );
}
void cata_tiles::init_draw_hit( const tripoint &p, std::string name )
{
    do_draw_hit = true;
    hit_pos = p;
    hit_entity_id = std::move( name );
}
void cata_tiles::init_draw_line( const tripoint &p, std::vector<tripoint> trajectory,
                                 std::string name, bool target_line )
{
    do_draw_line = true;
    is_target_line = target_line;
    line_pos = p;
    line_endpoint_id = std::move( name );
    line_trajectory = std::move( trajectory );
}
void cata_tiles::init_draw_cursor( const tripoint &p )
{
    do_draw_cursor = true;
    cursors.emplace_back( p );
}
void cata_tiles::init_draw_highlight( const tripoint &p )
{
    do_draw_highlight = true;
    highlights.emplace_back( p );
}
void cata_tiles::init_draw_weather( weather_printable weather, std::string name )
{
    do_draw_weather = true;
    weather_name = std::move( name );
    anim_weather = std::move( weather );
}
void cata_tiles::init_draw_sct()
{
    do_draw_sct = true;
}
void cata_tiles::init_draw_zones( const tripoint &_start, const tripoint &_end,
                                  const tripoint &_offset )
{
    do_draw_zones = true;
    zone_start = _start;
    zone_end = _end;
    zone_offset = _offset;
}
void cata_tiles::init_draw_radiation_override( const tripoint &p, const int rad )
{
    radiation_override.emplace( p, rad );
}
void cata_tiles::init_draw_terrain_override( const tripoint &p, const ter_id &id )
{
    terrain_override.emplace( p, id );
}
void cata_tiles::init_draw_furniture_override( const tripoint &p, const furn_id &id )
{
    furniture_override.emplace( p, id );
}
void cata_tiles::init_draw_graffiti_override( const tripoint &p, const bool has )
{
    graffiti_override.emplace( p, has );
}
void cata_tiles::init_draw_trap_override( const tripoint &p, const trap_id &id )
{
    trap_override.emplace( p, id );
}
void cata_tiles::init_draw_field_override( const tripoint &p, const field_type_id &id )
{
    field_override.emplace( p, id );
}
void cata_tiles::init_draw_item_override( const tripoint &p, const itype_id &id,
        const mtype_id &mid, const bool hilite )
{
    item_override.emplace( p, std::make_tuple( id, mid, hilite ) );
}
void cata_tiles::init_draw_vpart_override( const tripoint &p, const vpart_id &id,
        const int part_mod, const units::angle veh_dir, const bool hilite, point mount )
{
    vpart_override.emplace( p, std::make_tuple( id, part_mod, veh_dir, hilite, mount ) );
}
void cata_tiles::init_draw_below_override( const tripoint &p, const bool draw )
{
    draw_below_override.emplace( p, draw );
}
void cata_tiles::init_draw_monster_override( const tripoint &p, const mtype_id &id, const int count,
        const bool more, const Attitude att )
{
    monster_override.emplace( p, std::make_tuple( id, count, more, att ) );
}
/* -- Void Animators */
void cata_tiles::void_explosion()
{
    do_draw_explosion = false;
    exp_pos = {-1, -1, -1};
    exp_rad = -1;
}
void cata_tiles::void_custom_explosion()
{
    do_draw_custom_explosion = false;
    custom_explosion_layer.clear();
}
void cata_tiles::void_bullet()
{
    do_draw_bullet = false;
    bul_pos.clear();
    bul_id.clear();
    bul_rotation.clear();
}
void cata_tiles::void_hit()
{
    do_draw_hit = false;
    hit_pos = { -1, -1, -1 };
    hit_entity_id.clear();
}
void cata_tiles::void_line()
{
    do_draw_line = false;
    is_target_line = false;
    line_pos = { -1, -1, -1 };
    line_endpoint_id.clear();
    line_trajectory.clear();
}
void cata_tiles::void_cursor()
{
    do_draw_cursor = false;
    cursors.clear();
}
void cata_tiles::void_highlight()
{
    do_draw_highlight = false;
    highlights.clear();
}
void cata_tiles::void_weather()
{
    do_draw_weather = false;
    weather_name.clear();
    anim_weather.vdrops.clear();
}
void cata_tiles::void_sct()
{
    do_draw_sct = false;
}
void cata_tiles::void_zones()
{
    do_draw_zones = false;
}
void cata_tiles::void_radiation_override()
{
    radiation_override.clear();
}
void cata_tiles::void_terrain_override()
{
    terrain_override.clear();
}
void cata_tiles::void_furniture_override()
{
    furniture_override.clear();
}
void cata_tiles::void_graffiti_override()
{
    graffiti_override.clear();
}
void cata_tiles::void_trap_override()
{
    trap_override.clear();
}
void cata_tiles::void_field_override()
{
    field_override.clear();
}
void cata_tiles::void_item_override()
{
    item_override.clear();
}
void cata_tiles::void_vpart_override()
{
    vpart_override.clear();
}
void cata_tiles::void_draw_below_override()
{
    draw_below_override.clear();
}
void cata_tiles::void_monster_override()
{
    monster_override.clear();
}
bool cata_tiles::has_draw_override( const tripoint &p ) const
{
    return radiation_override.contains( p ) ||
           terrain_override.contains( p ) ||
           furniture_override.contains( p ) ||
           graffiti_override.contains( p ) ||
           trap_override.contains( p ) ||
           field_override.contains( p ) ||
           item_override.contains( p ) ||
           vpart_override.contains( p ) ||
           draw_below_override.contains( p ) ||
           monster_override.contains( p );
}
/* -- Animation Renders */
void cata_tiles::draw_explosion_frame()
{
    for( int i = 1; i < exp_rad; ++i ) {
        draw_from_id_string(
        {exp_name, C_NONE, empty_string, corner, 0},
        exp_pos + point( -i, -i ), std::nullopt, std::nullopt,
        lit_level::LIT, true, 0, false );
        draw_from_id_string(
        {exp_name, C_NONE, empty_string, corner, 1},
        exp_pos + point( -i, i ), std::nullopt, std::nullopt,
        lit_level::LIT, true, 0, false );
        draw_from_id_string(
        {exp_name, C_NONE, empty_string, corner, 2},
        exp_pos + point( i, i ), std::nullopt, std::nullopt,
        lit_level::LIT, true, 0, false );
        draw_from_id_string(
        {exp_name, C_NONE, empty_string, corner, 3},
        exp_pos + point( i, -i ), std::nullopt, std::nullopt,
        lit_level::LIT, true, 0, false );

        for( int j = 1 - i; j < 0 + i; j++ ) {
            draw_from_id_string(
            {exp_name, C_NONE, empty_string, edge, 0},
            exp_pos + point( j, -i ), std::nullopt, std::nullopt,
            lit_level::LIT, true, 0, false );
            draw_from_id_string(
            {exp_name, C_NONE, empty_string, edge, 0},
            exp_pos + point( j, i ), std::nullopt, std::nullopt,
            lit_level::LIT, true, 0, false );

            draw_from_id_string(
            {exp_name, C_NONE, empty_string, edge, 1},
            exp_pos + point( -i, j ), std::nullopt, std::nullopt,
            lit_level::LIT, true, 0, false );
            draw_from_id_string(
            {exp_name, C_NONE, empty_string, edge, 1},
            exp_pos + point( i, j ), std::nullopt, std::nullopt,
            lit_level::LIT, true, 0, false );
        }
    }
}

void cata_tiles::draw_custom_explosion_frame()
{
    // TODO: Make the drawing code handle all the missing tiles: <^>v and *
    // TODO: Add more explosion tiles, like "strong explosion", so that it displays more info

    // explosion_weak/explosion_medium/explosion removed from tiles in favor of allowing custom explosion sprites.

    int subtile = 0;
    int rotation = 0;

    for( const auto &pr : custom_explosion_layer ) {
        const explosion_neighbors ngh = pr.second.neighborhood;

        switch( ngh ) {
            case N_NORTH:
            case N_SOUTH:
                subtile = edge;
                rotation = 1;
                break;
            case N_WEST:
            case N_EAST:
                subtile = edge;
                rotation = 0;
                break;
            case N_NORTH | N_SOUTH:
            case N_NORTH | N_SOUTH | N_WEST:
            case N_NORTH | N_SOUTH | N_EAST:
                subtile = edge;
                rotation = 1;
                break;
            case N_WEST | N_EAST:
            case N_WEST | N_EAST | N_NORTH:
            case N_WEST | N_EAST | N_SOUTH:
                subtile = edge;
                rotation = 0;
                break;
            case N_SOUTH | N_EAST:
                subtile = corner;
                rotation = 0;
                break;
            case N_NORTH | N_EAST:
                subtile = corner;
                rotation = 1;
                break;
            case N_NORTH | N_WEST:
                subtile = corner;
                rotation = 2;
                break;
            case N_SOUTH | N_WEST:
                subtile = corner;
                rotation = 3;
                break;
            case N_NO_NEIGHBORS:
                subtile = edge;
                break;
            case N_WEST | N_EAST | N_NORTH | N_SOUTH:
                // Needs some special tile
                subtile = edge;
                break;
        }

        const tripoint &p = pr.first;
        const tile_search_params tile { exp_name, C_NONE, empty_string, subtile, rotation };
        draw_from_id_string(
            tile, p, std::nullopt, std::nullopt,
            lit_level::LIT, true, 0, false );
        // Used to be divided into explosion_weak/explosion_medium/explosion.
    }
}

void cata_tiles::draw_cone_aoe_frame()
{
    // Should probably jsonize for flamethrower, dragon breath etc.
    static const std::array<std::string, 3> sprite_ids = {
        "shot_cone_weak",
        "shot_cone_medium",
        "shot_cone_strong"
    };

    for( const point_with_value &pv : cone_aoe_layer ) {
        const tripoint diff = pv.pt - cone_aoe_origin;
        int rotation = ( sgn( diff.x ) == sgn( diff.y ) ? 1 : 0 );

        size_t intensity = ( pv.val >= 1.0 ) + ( pv.val >= 0.5 );
        const tile_search_params tile { sprite_ids[intensity], C_NONE, empty_string, 0, rotation };
        draw_from_id_string(
            tile, pv.pt, std::nullopt, std::nullopt,
            lit_level::LIT, false, 0, false );
    }
}
void cata_tiles::void_cone_aoe()
{
    do_draw_cone_aoe = true;
    cone_aoe_origin = {-1, -1, -1};
    cone_aoe_layer.clear();
}

void cata_tiles::draw_bullet_frame()
{
    for( size_t i = 0; i < bul_pos.size(); ++i ) {
        const auto tile = tile_search_params{
            .id = bul_id[i],
            .category = C_BULLET,
            .subcategory = empty_string,
            .subtile = 0,
            .rota = bul_rotation[i]
        };
        draw_from_id_string(
            tile, bul_pos[i], std::nullopt, std::nullopt,
            lit_level::LIT, false, 0, false );
    }
}
void cata_tiles::draw_hit_frame()
{
    std::string hit_overlay = "animation_hit";

    draw_from_id_string(
    {hit_entity_id, C_HIT_ENTITY, empty_string, 0, 0},
    hit_pos, std::nullopt, std::nullopt, lit_level::LIT,
    false, 0, false );
    draw_from_id_string(
    {hit_overlay, C_NONE, empty_string, 0, 0}, hit_pos,
    std::nullopt, std::nullopt, lit_level::LIT, false, 0,
    false );
}
void cata_tiles::draw_line()
{
    if( line_trajectory.empty() ) {
        return;
    }
    static std::string line_overlay = "animation_line";
    if( !is_target_line || g->u.sees( line_pos ) ) {
        for( auto it = line_trajectory.begin(); it != line_trajectory.end() - 1; ++it ) {
            draw_from_id_string(
            {line_overlay, C_NONE, empty_string, 0, 0},
            *it, std::nullopt, std::nullopt,
            lit_level::LIT, false, 0, false
            );
        }
    }

    draw_from_id_string(
    {line_endpoint_id, C_NONE, empty_string, 0, 0},
    line_trajectory.back(), std::nullopt, std::nullopt,
    lit_level::LIT, false, 0, false
    );
}
void cata_tiles::draw_cursor()
{
    for( const tripoint &p : cursors ) {
        draw_from_id_string(
        {"cursor", C_NONE, empty_string, 0, 0},
        p, std::nullopt, std::nullopt,
        lit_level::LIT, false, 0, false
        );
    }
}
void cata_tiles::draw_highlight()
{
    for( const tripoint &p : highlights ) {
        draw_from_id_string(
        {"highlight", C_NONE, empty_string, 0, 0},
        p,  std::nullopt, std::nullopt,
        lit_level::LIT, false, 0, false
        );
    }
}
void cata_tiles::draw_weather_frame()
{

    for( auto &vdrop : anim_weather.vdrops ) {
        // TODO: Z-level awareness if weather ever happens on anything but z-level 0.
        tripoint p( vdrop.first, vdrop.second, 0 );
        if( !tile_iso ) {
            // currently in ASCII screen coordinates
            p += o;
        }
        draw_from_id_string(
        {weather_name, C_WEATHER, empty_string, 0, 0},
        p, std::nullopt, std::nullopt,
        lit_level::LIT, true, 0, false
        );
    }
}

void cata_tiles::draw_sct_frame( std::multimap<point, formatted_text> &overlay_strings )
{
    const bool use_font = get_option<bool>( "ANIMATION_SCT_USE_FONT" );

    for( auto iter = SCT.vSCT.begin(); iter != SCT.vSCT.end(); ++iter ) {
        const point iD( iter->getPosX(), iter->getPosY() );
        const int full_text_length = utf8_width( iter->getText() );

        int iOffsetX = 0;
        int iOffsetY = 0;

        for( int j = 0; j < 2; ++j ) {
            std::string sText = iter->getText( ( j == 0 ) ? "first" : "second" );
            int FG = msgtype_to_tilecolor( iter->getMsgType( ( j == 0 ) ? "first" : "second" ),
                                           iter->getStep() >= scrollingcombattext::iMaxSteps / 2 );

            if( use_font ) {
                const auto direction = iter->getDirecton();
                // Compensate for string length offset added at SCT creation
                // (it will be readded using font size and proper encoding later).
                const int direction_offset = ( -displace_XY( direction ).x + 1 ) * full_text_length / 2;

                overlay_strings.emplace(
                    player_to_screen( iD + point( direction_offset, 0 ) ),
                    formatted_text( sText, FG, direction ) );
            } else {
                for( auto &it : sText ) {
                    const std::string generic_id = get_ascii_tile_id( it, FG, -1 );

                    if( tileset_ptr->find_tile_type( generic_id ) ) {
                        draw_from_id_string(
                        {generic_id, C_NONE, empty_string, 0, 0},
                        iD + tripoint( iOffsetX, iOffsetY, g->u.pos().z ), std::nullopt, std::nullopt,
                        lit_level::LIT, false, 0, false
                        );
                    }

                    if( tile_iso ) {
                        iOffsetY++;
                    }
                    iOffsetX++;
                }
            }
        }
    }
}

void cata_tiles::draw_zones_frame( std::multimap<point, formatted_text> &overlay_strings )
{
    const point min_local = zone_offset.xy() + zone_start.xy();
    const point max_local = zone_offset.xy() + zone_end.xy();
    const tripoint center_local( ( min_local.x + max_local.x ) / 2,
                                 ( min_local.y + max_local.y ) / 2, get_avatar().pos().z );

    // get_zone_at expects absolute coordinates
    const zone_data *zone = zone_manager::get_manager().get_zone_at(
                                get_map().getabs( center_local ) );

    const point screen_tl = player_to_screen( min_local );
    const point screen_br = player_to_screen( max_local ) + point( tile_width, tile_height );

    draw_zone_overlay( {
        .renderer = renderer,
        .rect = { screen_tl.x, screen_tl.y, screen_br.x - screen_tl.x, screen_br.y - screen_tl.y },
        .color = zone
        ? curses_color_to_SDL( zone->get_type().obj().color() )
        : curses_color_to_SDL( c_light_green ),
        .overlay_strings = overlay_strings,
        .name = zone ? zone->get_name() : "",
    } );
}

void cata_tiles::draw_footsteps_frame( const tripoint &center )
{
    static const std::string id_footstep = "footstep";
    static const std::string id_footstep_above = "footstep_above";
    static const std::string id_footstep_below = "footstep_below";

    for( const tripoint &pos : sounds::get_footstep_markers() ) {
        if( pos.z == center.z ) {
            draw_from_id_string(
            {id_footstep, C_NONE, empty_string, 0, 0},
            pos, std::nullopt, std::nullopt,
            lit_level::LIT, false, 0, false
            );
        } else if( pos.z > center.z ) {
            draw_from_id_string(
            {id_footstep_above, C_NONE, empty_string, 0, 0},
            pos, std::nullopt, std::nullopt,
            lit_level::LIT, false, 0, false
            );
        } else {
            draw_from_id_string(
            {id_footstep_below, C_NONE, empty_string, 0, 0},
            pos, std::nullopt, std::nullopt,
            lit_level::LIT, false, 0, false
            );
        }
    }
}
/* END OF ANIMATION FUNCTIONS */

void cata_tiles::init_light()
{
    g->reset_light_level();
}

void cata_tiles::get_terrain_orientation( const tripoint &p, int &rota, int &subtile,
        const std::map<tripoint, ter_id> &ter_override, const bool ( &invisible )[5] )
{
    map &here = get_map();
    const bool overridden = ter_override.contains( p );
    const auto ter = [&]( const tripoint & q, const bool invis ) -> ter_id {
        const auto override = ter_override.find( q );
        return override != ter_override.end() ? override->second :
        ( !overridden || !invis ) ? here.ter( q ) : t_null;
    };

    // get terrain at x,y
    const ter_id tid = ter( p, invisible[0] );
    if( tid == t_null ) {
        subtile = 0;
        rota = 0;
        return;
    }

    // get terrain neighborhood
    const ter_id neighborhood[4] = {
        ter( p + point_south, invisible[1] ),
        ter( p + point_east, invisible[2] ),
        ter( p + point_west, invisible[3] ),
        ter( p + point_north, invisible[4] )
    };

    char val = 0;

    // populate connection information
    for( int i = 0; i < 4; ++i ) {
        if( neighborhood[i] == tid ) {
            val += 1 << i;
        }
    }

    get_rotation_and_subtile( val, rota, subtile );
}

void cata_tiles::get_rotation_and_subtile( const char val, int &rotation, int &subtile )
{
    switch( val ) {
        // no connections
        case 0:
            subtile = unconnected;
            rotation = 0;
            break;
        // all connections
        case 15:
            subtile = center;
            rotation = 0;
            break;
        // end pieces
        case 8:
            subtile = end_piece;
            rotation = 2;
            break;
        case 4:
            subtile = end_piece;
            rotation = 3;
            break;
        case 2:
            subtile = end_piece;
            rotation = 1;
            break;
        case 1:
            subtile = end_piece;
            rotation = 0;
            break;
        // edges
        case 9:
            subtile = edge;
            rotation = 0;
            break;
        case 6:
            subtile = edge;
            rotation = 1;
            break;
        // corners
        case 12:
            subtile = corner;
            rotation = 2;
            break;
        case 10:
            subtile = corner;
            rotation = 1;
            break;
        case 3:
            subtile = corner;
            rotation = 0;
            break;
        case 5:
            subtile = corner;
            rotation = 3;
            break;
        // all t_connections
        case 14:
            subtile = t_connection;
            rotation = 2;
            break;
        case 11:
            subtile = t_connection;
            rotation = 1;
            break;
        case 7:
            subtile = t_connection;
            rotation = 0;
            break;
        case 13:
            subtile = t_connection;
            rotation = 3;
            break;
    }
}

void cata_tiles::get_connect_values( const tripoint &p, int &subtile, int &rotation,
                                     const int connect_group, const std::map<tripoint, ter_id> &ter_override )
{
    uint8_t connections = get_map().get_known_connections( p, connect_group, ter_override );
    get_rotation_and_subtile( connections, rotation, subtile );
}

void cata_tiles::get_furn_connect_values( const tripoint &p, int &subtile, int &rotation,
        const int connect_group, const std::map<tripoint,
        furn_id> &furn_override )
{
    uint8_t connections = get_map().get_known_connections_f( p, connect_group, furn_override );
    get_rotation_and_subtile( connections, rotation, subtile );
}

void cata_tiles::get_tile_values( const int t, const int *tn, int &subtile, int &rotation )
{
    bool connects[4];
    char val = 0;
    for( int i = 0; i < 4; ++i ) {
        connects[i] = ( tn[i] == t );
        if( connects[i] ) {
            val += 1 << i;
        }
    }
    get_rotation_and_subtile( val, rotation, subtile );
}

void cata_tiles::get_tile_values_with_ter( const tripoint &p, const int t, const int *tn,
        int &subtile, int &rotation )
{
    get_tile_values( t, tn, subtile, rotation );
    // calculate rotation for unconnected tiles based on surrounding walls
    if( subtile == unconnected ) {
        int val = 0;
        bool use_furniture = false;
        map &here = get_map();

        if( here.has_flag( "ALIGN_WORKBENCH", p ) ) {
            for( int i = 0; i < 4; ++i ) {
                // align to furniture that has the workbench quality
                const tripoint &pt = p + four_adjacent_offsets[i];
                if( here.has_furn( pt ) && here.furn( pt ).obj().workbench ) {
                    val += 1 << i;
                    use_furniture = true;
                }
            }
        }
        // if still unaligned, try aligning to walls
        if( val == 0 ) {
            for( int i = 0; i < 4; ++i ) {
                const tripoint &pt = p + four_adjacent_offsets[i];
                if( here.has_flag( "WALL", pt ) || here.has_flag( "WINDOW", pt ) ||
                    here.has_flag( "DOOR", pt ) ) {
                    val += 1 << i;
                }
            }
        }

        switch( val ) {
            case 4:    // south wall
            case 14:   // north opening T
                rotation = 2;
                break;
            case 2:    // east wall
            case 6:    // southeast corner
            case 5:    // E/W corridor
            case 7:    // east opening T
                rotation = 1;
                break;
            case 8:    // west wall
            case 12:   // southwest corner
            case 13:   // west opening T
                rotation = 3;
                break;
            case 0:    // no walls
            case 1:    // north wall
            case 3:    // northeast corner
            case 9:    // northwest corner
            case 10:   // N/S corridor
            case 11:   // south opening T
            case 15:   // surrounded
            default:   // just in case
                rotation = 0;
                break;
        };

        //
        if( use_furniture ) {
            rotation = ( rotation + 2 ) % 4;
        }
    }
}

void cata_tiles::do_tile_loading_report( const std::function<void( std::string )> &out )
{
    out( "Loaded tileset: " + get_option<std::string>( "TILES" ) );

    if( !init::is_data_loaded() ) {
        // There's nothing to do anymore without the core data.
        return;
    }

    tile_loading_report<ter_t>( ter_t::count(), C_TERRAIN, out, "" );
    tile_loading_report<furn_t>( furn_t::count(), C_FURNITURE, out, "" );

    std::map<itype_id, const itype *> items;
    for( const itype *e : item_controller->all() ) {
        items.emplace( e->get_id(), e );
    }
    tile_loading_report( items, C_ITEM, out, "" );

    auto mtypes = MonsterGenerator::generator().get_all_mtypes();
    lr_generic( mtypes.begin(), mtypes.end(), []( const std::vector<mtype>::iterator & m ) {
        return ( *m ).id.str();
    }, C_MONSTER, out, "" );
    tile_loading_report( vpart_info::all(), C_VEHICLE_PART, out, "vp_" );
    tile_loading_report<trap>( trap::count(), C_TRAP, out, "" );
    tile_loading_report<field_type>( field_type::count(), C_FIELD, out, "" );
}

point cata_tiles::player_to_screen( point p ) const
{
    point screen;
    if( tile_iso ) {
        screen.x = ( ( p.x - o.x ) - ( o.y - p.y ) + screentile_width - 2 ) * tile_width / 2 +
                   op.x;
        // y uses tile_width because width is definitive for iso tiles
        // tile footprints are half as tall as wide, arbitrarily tall
        screen.y = ( ( p.y - o.y ) - ( p.x - o.x ) - 4 ) * tile_width / 4 +
                   screentile_height * tile_height / 2 + // TODO: more obvious centering math
                   op.y;
    } else {
        screen.x = ( p.x - o.x ) * tile_width + op.x;
        screen.y = ( p.y - o.y ) * tile_height + op.y;
    }
    return {screen};
}

template<typename Iter, typename Func>
void cata_tiles::lr_generic( Iter begin, Iter end, Func id_func, TILE_CATEGORY category,
                             std::function<void( std::string )> out, const std::string &prefix )
{
    std::string missing_list;
    std::string missing_with_looks_like_list;
    for( ; begin != end; ++begin ) {
        const std::string id_string = id_func( begin );

        if( !tileset_ptr->find_tile_type( prefix + id_string ) &&
            !find_tile_looks_like( id_string, category ) ) {
            missing_list.append( id_string + " " );
        } else if( !tileset_ptr->find_tile_type( prefix + id_string ) ) {
            missing_with_looks_like_list.append( id_string + " " );
        }
    }
    out( "Missing " + TILE_CATEGORY_IDS[category] + ": " + missing_list );
    out( "Missing " + TILE_CATEGORY_IDS[category] + " (but looks_like tile exists): " +
         missing_with_looks_like_list );
}

template <typename maptype>
void cata_tiles::tile_loading_report( const maptype &tiletypemap, TILE_CATEGORY category,
                                      std::function<void( std::string )> out, const std::string &prefix )
{
    lr_generic( tiletypemap.begin(), tiletypemap.end(),
    []( const decltype( tiletypemap.begin() ) & v ) {
        // c_str works for std::string and for string_id!
        return v->first.c_str();
    }, category, out, prefix );
}

template <typename base_type>
void cata_tiles::tile_loading_report( const size_t count, TILE_CATEGORY category,
                                      std::function<void( std::string )> out, const std::string &prefix )
{
    lr_generic( static_cast<size_t>( 0 ), count,
    []( const size_t i ) {
        return int_id<base_type>( i ).id().str();
    }, category, out, prefix );
}

template <typename arraytype>
void cata_tiles::tile_loading_report( const arraytype &array, int array_length,
                                      TILE_CATEGORY category, std::function<void( std::string )> out, const std::string &prefix )
{
    const auto begin = &( array[0] );
    lr_generic( begin, begin + array_length,
    []( decltype( begin ) const v ) {
        return v->id;
    }, category, out, prefix );
}

std::vector<options_manager::id_and_option> cata_tiles::build_renderer_list()
{
    std::vector<options_manager::id_and_option> renderer_names;
    std::vector<options_manager::id_and_option> default_renderer_names = {
#   if defined(_WIN32)
        { "direct3d", translate_marker( "direct3d" ) },
#   endif
        { "opengl", translate_marker( "opengl" ) },
        { "opengles2", translate_marker( "opengles2" ) },
        { "software", translate_marker( "software" ) },
    };
    int numRenderDrivers = SDL_GetNumRenderDrivers();
    DebugLog( DL::Info, DC::Main ) << "Number of render drivers on your system: " << numRenderDrivers;
    for( int ii = 0; ii < numRenderDrivers; ii++ ) {
        SDL_RendererInfo ri;
        SDL_GetRenderDriverInfo( ii, &ri );
        DebugLog( DL::Info, DC::Main ) << "Render driver: " << ii << "/" << ri.name;
        // First default renderer name we will put first on the list. We can use it later as default value.
        if( ri.name == default_renderer_names.front().first ) {
            renderer_names.emplace( renderer_names.begin(), default_renderer_names.front() );
        } else {
            renderer_names.emplace_back( ri.name, ri.name );
        }

    }

    return renderer_names.empty() ? default_renderer_names : renderer_names;
}

std::vector<options_manager::id_and_option> cata_tiles::build_display_list()
{
    std::vector<options_manager::id_and_option> display_names;
    std::vector<options_manager::id_and_option> default_display_names = {
        { "0", translate_marker( "Display 0" ) }
    };

    int numdisplays = SDL_GetNumVideoDisplays();
    display_names.reserve( numdisplays );
    for( int i = 0 ; i < numdisplays ; i++ ) {
        display_names.emplace_back( std::to_string( i ), std::string( SDL_GetDisplayName( i ) ) );
    }

    return display_names.empty() ? default_display_names : display_names;
}

#endif // SDL_TILES
