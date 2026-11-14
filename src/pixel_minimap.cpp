#if defined(TILES)

#    include "pixel_minimap.h"

#    include "avatar.h"
#    include "cata_utility.h"
#    include "character.h"
#    include "color.h"
#    include "creature.h"
#    include "debug.h"
#    include "game.h"
#    include "game_constants.h"
#    include "int_id.h"
#    include "map/lightmap.h"
#    include "map/map.h"
#    include "map/mapdata.h"
#    include "math_defines.h"
#    include "monster.h"
#    include "sdl/pixel_minimap_projectors.h"
#    include "sdl_utils.h"
#    include "vehicle/vehicle.h"
#    include "vehicle/vehicle_part.h"
#    include "vehicle/vpart_position.h"

#    include <algorithm>
#    include <array>
#    include <bitset>
#    include <cassert>
#    include <cmath>
#    include <cstdlib>
#    include <functional>
#    include <iterator>
#    include <limits>
#    include <memory>
#    include <optional>
#    include <ranges>
#    include <utility>
#    include <vector>

extern void set_displaybuffer_rendertarget();

namespace
{

// Updated each frame in draw() to reflect the runtime g_mapsize.
static point total_tiles_count = { ( 11 - 2 ) *SEEX, ( 11 - 2 ) *SEEY }; // default size=2

// The actual tile grid rendered by the projector. May be smaller than total_tiles_count
// (clipped to keep tile_size >= 2 so dots always have a 1-pixel gap) or larger
// (minimum 60-tile radius = 120 tiles, padding with black beyond the bubble edge).
static auto view_tiles_count = total_tiles_count;
// The view_tiles_count that the current projector / main_tex were built from.
// Updated whenever set_screen_rect actually rebuilds; used to detect stale projectors
// whose tile_size happens to match even though view_tiles_count changed.
static auto built_view_tiles_count = point {0, 0};

point get_pixel_size( point tile_size, pixel_minimap_mode mode )
{
    switch( mode ) {
        case pixel_minimap_mode::solid:
            return tile_size;

        case pixel_minimap_mode::squares:
            return { std::max( tile_size.x - 1, 1 ), std::max( tile_size.y - 1, 1 ) };

        case pixel_minimap_mode::dots:
            return { point_south_east };
    }

    return {};
}

/// Returns a number in range [0..1]. The range lasts for @param phase_length_ms (milliseconds).
float get_animation_phase( int phase_length_ms )
{
    if( phase_length_ms == 0 ) {
        return 0.0f;
    }

    return std::fmod<float>( SDL_GetTicks(), phase_length_ms ) / phase_length_ms;
}

//creates the texture that individual minimap updates are drawn to
//later, the main texture is drawn to the display buffer
//the surface is needed to determine the color format needed by the texture
SDL_Texture_Ptr create_cache_texture( const SDL_Renderer_Ptr &renderer, int tile_width,
                                      int tile_height )
{
    return CreateTexture( renderer,
                          SDL_PIXELFORMAT_ARGB8888,
                          SDL_TEXTUREACCESS_TARGET,
                          tile_width,
                          tile_height );
}

SDL_Color get_map_color_at( const tripoint_bub_ms &p )
{
    const auto &here = get_map();
    if( const auto vp = here.veh_at( p ) ) {
        return curses_color_to_SDL( vp->vehicle().part_color( vp->part_index() ) );
    }

    if( const auto furn_id = here.furn( p ) ) {
        return curses_color_to_SDL( furn_id->color() );
    }

    return curses_color_to_SDL( here.ter( p )->color() );
}

bool is_critter_animated( Creature *critter )
{
    if( const monster *m = dynamic_cast<monster *>( critter ) ) {
        //faction status (attacking or tracking) determines if red highlights get applied to creature
        const monster_attitude matt = m->attitude( &get_avatar() );

        if( MATT_ATTACK == matt || MATT_FOLLOW == matt ) {
            return true;
        }
    }
    return false;
}

SDL_Color get_critter_color( Creature *critter, int flicker, int mixture )
{
    SDL_Color result = curses_color_to_SDL( critter->symbol_color() );

    if( is_critter_animated( critter ) ) {
        const SDL_Color red_pixel = SDL_Color{ 0xFF, 0x0, 0x0, 0xFF };
        result = adjust_color_brightness( mix_colors( result, red_pixel, mixture ), flicker );
    }

    return result;
}

} // namespace

// a texture atlas with one slot per submap chunk, so updating any number of chunks
//  needs a single render target switch (each switch between chunk textures stalled the renderer)
// at most 142 out of 144 slots can be in use due to regular player movement
//  (moving from submap corner to new corner) with MAPSIZE = 11
// slots are dumped when the player moves more than one submap in one update
//  (teleporting, z-level change) to prevent running out of the remaining pool
class pixel_minimap::shared_texture_pool
{
    public:
        shared_texture_pool( const SDL_Renderer_Ptr &renderer, point chunk_size, const int mapsize ) :
            chunk_size( chunk_size ), columns( mapsize + 1 ) {
            const size_t pool_size = static_cast<size_t>( columns ) * columns;

            atlas = create_cache_texture( renderer, columns * chunk_size.x, columns * chunk_size.y );
            SetTextureBlendMode( atlas, SDL_BLENDMODE_BLEND );
            inactive_index.reserve( pool_size );

            for( size_t i = 0; i < pool_size; ++i ) {
                inactive_index.push_back( i );
            }
        }

        //reserves a slot from the inactive group
        auto request_slot() -> size_t {
            if( !atlas ) {
                return invalid_slot;
            }
            if( inactive_index.empty() ) {
                debugmsg( "Ran out of available textures in the pool." );
                //shouldn't be happening, but minimap will just be default color instead of crashing
                return invalid_slot;
            }
            const auto index = inactive_index.back();
            inactive_index.pop_back();
            return index;
        }

        //releases the provided slot back into the inactive pool to be used again
        //called automatically in the submap cache destructor
        auto release_slot( size_t index ) -> void {
            if( index != invalid_slot ) {
                inactive_index.push_back( index );
            }
        }

        auto slot_rect( size_t index ) const -> SDL_FRect {
            const auto x = static_cast<int>( index % columns ) * chunk_size.x;
            const auto y = static_cast<int>( index / columns ) * chunk_size.y;
            return SDL_FRect{ float( x ), float( y ), float( chunk_size.x ), float( chunk_size.y ) };
        }

        static constexpr auto invalid_slot = std::numeric_limits<size_t>::max();
        SDL_Texture_Ptr atlas;

    private:
        point chunk_size;
        int columns;
        std::vector<size_t> inactive_index;
};

struct pixel_minimap::submap_cache {
    //the color stored for each submap tile
    std::array<SDL_Color, SEEX *SEEY> minimap_colors = {};
    //checks if the submap has been looked at by the minimap routine
    bool touched;
    //the atlas slot updates are drawn to
    size_t texture_index;
    //the list of updates to apply to the slot
    //reduces render target switching to once per frame
    std::vector<point_sm_ms> update_list;
    //flag used to indicate that the slot needs to be cleared before first use
    bool ready;
    shared_texture_pool &pool;

    //reserve the SEEX * SEEY submap tiles
    submap_cache( shared_texture_pool &pool ) :
        touched( false ),
        texture_index( pool.request_slot() ),
        ready( false ),
        pool( pool ) {
    }

    //handle the release of the borrowed slot
    ~submap_cache() {
        pool.release_slot( texture_index );
    }

    // a moved-from cache would release its slot twice
    submap_cache( submap_cache && ) = delete;

    SDL_Color &color_at( point_sm_ms p ) {
        return minimap_colors[p.y() * SEEX + p.x()];
    }
};

pixel_minimap::pixel_minimap( const SDL_Renderer_Ptr &renderer,
                              const GeometryRenderer_Ptr &geometry ) :
    renderer( renderer ),
    geometry( geometry ),
    type( pixel_minimap_type::ortho ),
    screen_rect{ 0, 0, 0, 0 }
{
}

pixel_minimap::~pixel_minimap() = default;

void pixel_minimap::set_type( pixel_minimap_type type )
{
    this->type = type;
    reset();
}

void pixel_minimap::set_settings( const pixel_minimap_settings &settings )
{
    this->settings = settings;
    reset();
}

void pixel_minimap::prepare_cache_for_updates( const tripoint_bub_ms &center )
{
    const auto &here = get_map();
    const auto new_center_sm = project_to<coords::sm>( bub_to_abs( center ) );
    const auto &new_dimension_id = here.get_bound_dimension();
    const auto center_sm_diff = cached_center_sm - new_center_sm;

    //invalidate the cache if the game shifted more than one submap in the last update, or if z-level changed.
    if( cached_dimension_id != new_dimension_id ||
        std::abs( center_sm_diff.x() ) > 1 ||
        std::abs( center_sm_diff.y() ) > 1 ||
        std::abs( center_sm_diff.z() ) > 0 ) {
        cache.clear();
    } else {
        std::ranges::for_each( cache, []( auto & mcp ) {
            mcp.second.touched = false;
        } );
    }

    cached_center_sm = new_center_sm;
    cached_dimension_id = new_dimension_id;
}

//deletes the mapping of unused submap caches from the main map
//the touched flag prevents deletion
void pixel_minimap::clear_unused_cache()
{
    for( auto it = cache.begin(); it != cache.end(); ) {
        it = it->second.touched ? std::next( it ) : cache.erase( it );
    }
}

//draws individual updates to the submap cache texture
//the render target will be set back to display_buffer after all submaps are updated
void pixel_minimap::flush_cache_updates()
{
    auto target_set = false;
    std::ranges::for_each( cache, [&]( auto & mcp ) {
        if( mcp.second.update_list.empty() ) {
            return;
        }
        if( mcp.second.texture_index == shared_texture_pool::invalid_slot ) {
            mcp.second.update_list.clear();
            return;
        }

        if( !target_set ) {
            target_set = true;
            SetRenderTarget( renderer, tex_pool->atlas );
        }
        const auto slot = tex_pool->slot_rect( mcp.second.texture_index );
        const auto slot_pos = point( static_cast<int>( slot.x ), static_cast<int>( slot.y ) );

        if( !mcp.second.ready ) {
            mcp.second.ready = true;

            // clear the slot like RenderClear cleared a whole chunk texture
            auto blend_mode = SDL_BLENDMODE_NONE;
            GetRenderDrawBlendMode( renderer, blend_mode );
            SetRenderDrawBlendMode( renderer, SDL_BLENDMODE_NONE );
            SetRenderDrawColor( renderer, 0x00, 0x00, 0x00, 0x00 );
            RenderFillRect( renderer, &slot );
            SetRenderDrawBlendMode( renderer, blend_mode );

            for( const auto p : submap_tiles() ) {
                const auto tile_pos = slot_pos + projector->get_tile_pos( p.raw(), { SEEX, SEEY } );
                const auto tile_size = projector->get_tile_size();

                geometry->rect( renderer, tile_pos, tile_size.x, tile_size.y, SDL_Color() );
            }
        }

        std::ranges::for_each( mcp.second.update_list, [&]( const auto p ) {
            const auto tile_pos = slot_pos + projector->get_tile_pos( p.raw(), { SEEX, SEEY } );
            const auto tile_color = mcp.second.color_at( p );

            if( pixel_size.x == 1 && pixel_size.y == 1 ) {
                SetRenderDrawColor( renderer, tile_color.r, tile_color.g, tile_color.b, tile_color.a );
                RenderDrawPoint( renderer, tile_pos );
            } else {
                geometry->rect( renderer, tile_pos, pixel_size.x, pixel_size.y, tile_color );
            }
        } );

        mcp.second.update_list.clear();
    } );
}

void pixel_minimap::update_cache_at( const tripoint_bub_sm &pos )
{
    const map &here = get_map();
    const level_cache &access_cache = here.access_cache( pos.z() );
    const bool nv_goggle = get_avatar().get_vision_modes()[NV_GOGGLES];
    const bool env_goggle = get_avatar().get_vision_modes()[ENV_GOGGLES];

    auto &cache_item = get_cache_at( bub_to_abs( pos ) );

    cache_item.touched = true;
    for( const auto sm_ms : submap_tiles() ) {
        const auto ms_pos = project_combine( pos, sm_ms );
        const auto lighting = access_cache.visibility_cache[access_cache.idx( ms_pos.x(), ms_pos.y() )];

        SDL_Color color;

        if( lighting == lit_level::BLANK || lighting == lit_level::DARK ) {
            // TODO: Map memory?
            color = { 0x00, 0x00, 0x00, 0xFF };
        } else {
            color = get_map_color_at( ms_pos );

            //color terrain according to lighting conditions
            if( nv_goggle || env_goggle ) {
                if( lighting == lit_level::LOW ) {
                    color = color_pixel_nightvision( color );
                } else if( lighting != lit_level::DARK && lighting != lit_level::BLANK ) {
                    color = color_pixel_overexposed( color );
                }
            } else if( lighting == lit_level::LOW ) {
                color = color_pixel_grayscale( color );
            }

            color = adjust_color_brightness( color, settings.brightness );
        }

        auto &current_color = cache_item.color_at( sm_ms );

        if( current_color != color ) {
            current_color = color;
            cache_item.update_list.emplace_back( sm_ms );
        }
    }
}

pixel_minimap::submap_cache &pixel_minimap::get_cache_at( const tripoint_abs_sm &abs_sm_pos )
{
    auto it = cache.find( abs_sm_pos );

    if( it == cache.end() ) {
        it = cache.emplace( abs_sm_pos, *tex_pool ).first;
    }

    return it->second;
}

void pixel_minimap::process_cache( const tripoint_bub_ms &center )
{
    const auto current_mapsize = get_map().getmapsize();
    // Refresh the tile count to match the current runtime map size.
    total_tiles_count = { ( current_mapsize - 2 ) *SEEX, ( current_mapsize - 2 ) *SEEY };

    prepare_cache_for_updates( center );

    for( int y = 0; y < current_mapsize; ++y ) {
        for( int x = 0; x < current_mapsize; ++x ) {
            update_cache_at( tripoint_bub_sm( x, y, center.z() ) );
        }
    }

    flush_cache_updates();
    clear_unused_cache();
}

void pixel_minimap::set_screen_rect( const SDL_Rect &screen_rect )
{
    const auto current_mapsize = get_map().getmapsize();
    if( this->screen_rect == screen_rect && main_tex && tex_pool && projector
        && built_view_tiles_count == view_tiles_count && built_mapsize == current_mapsize ) {
        return;
    }

    this->screen_rect = screen_rect;

    projector = create_projector( screen_rect );
    pixel_size = get_pixel_size( projector->get_tile_size(), settings.mode );

    const auto size_on_screen = projector->get_tiles_size( view_tiles_count );

    if( settings.scale_to_fit ) {
        main_tex_clip_rect = SDL_Rect{ 0, 0, size_on_screen.x, size_on_screen.y };
        screen_clip_rect = fit_rect_inside( main_tex_clip_rect, screen_rect );

        main_tex = create_cache_texture( renderer, size_on_screen.x, size_on_screen.y );
        SDL_SetTextureScaleMode( main_tex.get(), SDL_SCALEMODE_LINEAR );

    } else {
        const point d( ( size_on_screen.x - screen_rect.w ) / 2, ( size_on_screen.y - screen_rect.h ) / 2 );

        main_tex_clip_rect = SDL_Rect{
            std::max( d.x, 0 ),
            std::max( d.y, 0 ),
            size_on_screen.x - 2 * std::max( d.x, 0 ),
            size_on_screen.y - 2 * std::max( d.y, 0 )
        };

        screen_clip_rect = SDL_Rect{
            screen_rect.x - std::min( d.x, 0 ),
            screen_rect.y - std::min( d.y, 0 ),
            main_tex_clip_rect.w,
            main_tex_clip_rect.h
        };

        main_tex = create_cache_texture( renderer, size_on_screen.x, size_on_screen.y );
    }

    cache.clear();

    const point chunk_size = projector->get_tiles_size( { SEEX, SEEY } );

    tex_pool = std::make_unique<shared_texture_pool>( renderer, chunk_size, current_mapsize );
    built_view_tiles_count = view_tiles_count;
    built_mapsize = current_mapsize;
}

void pixel_minimap::reset()
{
    projector.reset();
    cache.clear();
    main_tex.reset();
    tex_pool.reset();
    built_mapsize = 0;
}

void pixel_minimap::render( const tripoint_bub_ms &center )
{
    SetRenderTarget( renderer, main_tex );

    SetRenderDrawColor( renderer, 0x00, 0x00, 0x00, 0x00 );
    RenderClear( renderer );

    render_cache( center );
    render_critters( center );

    //set display buffer to main screen
    set_displaybuffer_rendertarget();
    //paint intermediate texture to screen
    const SDL_FRect fsrc{ float( main_tex_clip_rect.x ), float( main_tex_clip_rect.y ),
                          float( main_tex_clip_rect.w ), float( main_tex_clip_rect.h ) };
    const SDL_FRect fdst{ float( screen_clip_rect.x ), float( screen_clip_rect.y ),
                          float( screen_clip_rect.w ), float( screen_clip_rect.h ) };
    RenderCopy( renderer, main_tex, &fsrc, &fdst );
}

void pixel_minimap::render_cache( const tripoint_bub_ms &center )
{
    const auto sm_center = project_to<coords::sm>( bub_to_abs( center ) );

    const auto sm_offset = tripoint_rel_sm( view_tiles_count.x / SEEX / 2,
                                            view_tiles_count.y / SEEY / 2, 0 );

    const auto center_remainder = project_remain<coords::sm>( center ).remainder;
    const auto ms_offset = point_rel_ms( view_tiles_count.x / 2 - sm_offset.x() * SEEX -
                                         center_remainder.x(),
                                         view_tiles_count.y / 2 - sm_offset.y() * SEEY -
                                         center_remainder.y() );

    std::ranges::for_each( cache, [&]( const auto & elem ) {
        if( !elem.second.touched ) {
            return;
        }

        if( elem.second.texture_index == shared_texture_pool::invalid_slot ) {
            return;
        }

        const tripoint_rel_sm rel_pos = elem.first - sm_center;

        if( std::abs( rel_pos.x() ) > sm_offset.x() + 1 ||
            std::abs( rel_pos.y() ) > sm_offset.y() + 1 ||
            rel_pos.z() != 0 ) {
            return;
        }

        const auto sm_pos = rel_pos + sm_offset;
        const auto ms_pos = project_to<coords::ms>( sm_pos ) + ms_offset;

        const auto chunk_rect = projector->get_chunk_rect( ms_pos.xy().raw(), { SEEX, SEEY } );

        const SDL_FRect fchunk{ float( chunk_rect.x ), float( chunk_rect.y ),
                                float( chunk_rect.w ), float( chunk_rect.h ) };
        const auto slot = tex_pool->slot_rect( elem.second.texture_index );
        RenderCopy( renderer, tex_pool->atlas, &slot, &fchunk );
    } );
}

void pixel_minimap::render_critters( const tripoint_bub_ms &center )
{
    //handles the enemy faction red highlights
    //this value should be divisible by 200
    const int indicator_length = settings.beacon_blink_interval * 200; //default is 2000 ms, 2 seconds

    int flicker = 100;
    int mixture = 0;

    if( indicator_length > 0 ) {
        const float t = get_animation_phase( 2 * indicator_length );
        const float s = std::sin( 2 * M_PI * t );

        flicker = lerp_clamped( 25, 100, std::abs( s ) );
        mixture = lerp_clamped( 0, 100, std::max( s, 0.0f ) );
    }

    const level_cache &access_cache = get_map().access_cache( center.z() );

    const auto start_x = center.x() - view_tiles_count.x / 2;
    const auto start_y = center.y() - view_tiles_count.y / 2;
    const point beacon_size = {
        std::max<int>( projector->get_tile_size().x *settings.beacon_size / 2, 2 ),
        std::max<int>( projector->get_tile_size().y *settings.beacon_size / 2, 2 )
    };

    cached_has_animated_beacons = false;
    // Loop over view_tiles_count (the clipped view, not total_tiles_count) so that
    // start_{x,y} + loop index always stays within the loaded cache for large bubbles.
    // An inbounds() guard handles the opposite case (small bubble, wide screen) where
    // view_tiles_count can exceed the cache dimensions.
    for( int y = 0; y < view_tiles_count.y; y++ ) {
        for( int x = 0; x < view_tiles_count.x; x++ ) {
            const tripoint p = tripoint{ start_x + x, start_y + y, center.z() };
            if( !access_cache.inbounds( tripoint_bub_ms( p ).xy() ) ) {
                continue;
            }
            const lit_level lighting = access_cache.visibility_cache[access_cache.idx( p.x, p.y )];

            if( lighting == lit_level::DARK || lighting == lit_level::BLANK ) {
                continue;
            }

            const auto critter = g->critter_at( tripoint_bub_ms( p ), true );

            if( critter == nullptr || !get_avatar().sees( *critter ) ) {
                continue;
            }

            const auto critter_pos = projector->get_tile_pos( { x, y }, view_tiles_count );
            const SDL_Rect critter_rect = SDL_Rect{ critter_pos.x, critter_pos.y, beacon_size.x, beacon_size.y };
            const SDL_Color critter_color = get_critter_color( critter, flicker, mixture );
            cached_has_animated_beacons = cached_has_animated_beacons || is_critter_animated( critter );

            draw_beacon( critter_rect, critter_color );
        }
    }
}

//the main call for drawing the pixel minimap to the screen
void pixel_minimap::draw( const SDL_Rect &screen_rect, const tripoint_bub_ms &center )
{
    if( !g ) {
        return;
    }

    if( screen_rect.w <= 0 || screen_rect.h <= 0 ) {
        return;
    }

    // Update the tile count from the current runtime bubble size BEFORE set_screen_rect
    // so the projector is always built with the correct grid dimensions.
    const auto current_mapsize = get_map().getmapsize();
    total_tiles_count = { ( current_mapsize - 2 ) *SEEX, ( current_mapsize - 2 ) *SEEY };

    // view_tiles_count is the geographic tile grid the projector and renderer use.
    // Large bubble: clip to screen/2 (tile_size >= 2); small bubble: screen/3 (tile_size >= 3).
    // Out-of-bubble area is rendered as black.
    constexpr auto min_view_radius = 60;
    constexpr auto min_view_diameter = 2 * min_view_radius;
    view_tiles_count = {
        total_tiles_count.x > min_view_diameter
        ? std::min( total_tiles_count.x, screen_rect.w / 2 )
        : screen_rect.w / 3,
        total_tiles_count.y > min_view_diameter
        ? std::min( total_tiles_count.y, screen_rect.h / 2 )
        : screen_rect.h / 3
    };

    set_screen_rect( screen_rect );
    process_cache( center );
    render( center );
}

bool pixel_minimap::has_animated_elements() const
{
    return settings.beacon_blink_interval && cached_has_animated_beacons;
}

void pixel_minimap::draw_beacon( const SDL_Rect &rect, const SDL_Color &color )
{
    for( int x = -rect.w, x_max = rect.w; x <= x_max; ++x ) {
        for( int y = -rect.h + std::abs( x ), y_max = rect.h - std::abs( x ); y <= y_max; ++y ) {
            const int divisor = 2 * ( std::abs( y ) == rect.h - std::abs( x ) ? 1 : 0 ) + 1;

            SetRenderDrawColor( renderer, color.r / divisor, color.g / divisor, color.b / divisor, 0xFF );
            RenderDrawPoint( renderer, point( rect.x + x, rect.y + y ) );
        }
    }
}

std::unique_ptr<pixel_minimap_projector> pixel_minimap::create_projector(
    const SDL_Rect &max_screen_rect )
const
{
    switch( type ) {
        case pixel_minimap_type::ortho:
            return std::unique_ptr<pixel_minimap_projector> {
                new pixel_minimap_ortho_projector( view_tiles_count, max_screen_rect, settings.square_pixels )
            };

        case pixel_minimap_type::iso:
            return std::unique_ptr<pixel_minimap_projector> {
                new pixel_minimap_iso_projector( view_tiles_count, max_screen_rect, settings.square_pixels )
            };
    }

    return nullptr;
}

#endif // SDL_TILES
