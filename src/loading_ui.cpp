#include "loading_ui.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <memory>
#include <optional>
#include <ranges>
#include <unordered_set>
#include <vector>

#include "cata_algo.h"
#include "cached_options.h"
#include "color.h"
#include "debug.h"
#include "filesystem.h"
#include "input.h"
#include "mod_manager.h"
#include "output.h"
#include "point.h"
#include "rng.h"
#include "sdltiles.h"
#include "sdl_wrappers.h"
#include "string_utils.h"
#include "translations.h"
#include "ui.h"
#include "ui_manager.h"
#include "worldfactory.h"

#if defined( TILES )
struct loading_image_cache {
    std::string path;
    SDL_Texture_Ptr texture;
    point image_size = point_zero;
    bool attempted = false;
};
#endif

auto get_scaled_loading_image_size( const loading_image_scaling_options &opts ) ->
std::optional<point>
{
    if( opts.image_size.x <= 0 || opts.image_size.y <= 0 || opts.screen_size.x <= 0 ||
        opts.screen_size.y <= 0 ) {
        return std::nullopt;
    }

    const auto height_scale = static_cast<double>( opts.screen_size.y ) /
                              static_cast<double>( opts.image_size.y );

    return point( std::max( 1, static_cast<int>( std::lround( opts.image_size.x * height_scale ) ) ),
                  opts.screen_size.y );
}

namespace
{

#if defined( TILES )

auto log_loading_image( const std::string &message ) -> void
{
    static auto logged_messages = std::unordered_set<std::string> {};
    if( logged_messages.insert( message ).second ) {
        DebugLog( DL::Info, DC::Main ) << "[loading_images] " << message;
    }
}

auto get_loading_image_search_roots( const MOD_INFORMATION &mod ) -> std::unordered_set<std::string>
{
    using namespace std::views;

    const auto modinfo_root = mod.path_full.empty() ? std::string{} :
                              std::filesystem::path( mod.path_full ).parent_path().generic_string();

    const auto root_paths = std::array<std::string, 2> { mod.path, modinfo_root };
    return root_paths
    | filter( []( const std::string & root_path ) { return !root_path.empty(); } )
    | transform( []( const std::string & root_path ) { return std::filesystem::path( root_path ).lexically_normal().generic_string(); } )
    | std::ranges::to<std::unordered_set>();
}

auto path_is_inside_root( const std::filesystem::path &root_path,
                          const std::filesystem::path &candidate_path ) -> bool
{
    const auto normalized_root = root_path.lexically_normal();
    const auto normalized_candidate = candidate_path.lexically_normal();
    const auto mismatch = std::mismatch( normalized_root.begin(), normalized_root.end(),
                                         normalized_candidate.begin(), normalized_candidate.end() );
    return mismatch.first == normalized_root.end();
}

auto can_choose_loading_image_path() -> bool { return world_generator != nullptr && world_generator->active_world; }

auto get_loading_image_author( const std::string &loading_image_path ) -> std::optional<std::string>
{
    const auto image_stem = std::filesystem::path( loading_image_path ).stem().generic_string();
    const auto author_parts = string_split( image_stem, '_' );

    if( author_parts.size() < 2 || author_parts.front().empty() ) { return {}; }
    return author_parts.front();
}

struct loading_image_match_options {
    const MOD_INFORMATION &mod;
    const std::string &image_name;
};

auto has_loading_image_extension( const std::string &path ) -> bool
{
    static const auto exts = std::unordered_set<std::string> { ".png", ".jpg", ".jpeg", ".bmp", ".gif", ".webp" };
    auto ext = std::filesystem::path( path ).extension().generic_string();
    std::ranges::transform( ext, ext.begin(), []( const unsigned char ch ) { return static_cast<char>( std::tolower( ch ) ); } );

    return exts.contains( ext );
}

auto get_loading_images_from_directory( const std::string &directory_path ) ->
std::vector<std::string>
{
    using namespace std::views;

    return get_files_from_path( "", directory_path, true )
    | filter( []( const std::string & path ) { return file_exist( path ) && has_loading_image_extension( path ); } )
    | std::ranges::to<std::vector>();
}

auto get_loading_image_matches_at_root( const std::string &image_name,
                                        const std::string &root_path ) -> std::vector<std::string>
{
    using namespace std::views;

    const auto normalized_root = std::filesystem::path( root_path ).lexically_normal();
    const auto direct_path = ( normalized_root / image_name ).lexically_normal();
    if( !path_is_inside_root( normalized_root, direct_path ) ) {
        log_loading_image( string_format( "mod loading image '%s' escapes root '%s'",
                                          image_name, root_path ) );
        return {};
    }

    const auto normalized_direct_path = direct_path.generic_string();
    if( file_exist( normalized_direct_path ) &&
        has_loading_image_extension( normalized_direct_path ) ) {
        return { normalized_direct_path };
    }
    if( dir_exist( normalized_direct_path ) ) {
        return get_loading_images_from_directory( normalized_direct_path );
    }

    const auto image_filename = std::filesystem::path( image_name ).filename().generic_string();
    if( image_filename.empty() ) {
        return {};
    }

    const auto author_prefixed_filename = "_" + image_filename;

    return get_files_from_path( image_filename, root_path, true, true )
           | filter( [&normalized_root, &image_filename,
                      &author_prefixed_filename]( const std::string & path ) {
        const auto normalized_path = std::filesystem::path( path ).lexically_normal();
        const auto filename = normalized_path.filename().generic_string();
        return path_is_inside_root( normalized_root, normalized_path )
               && file_exist( path )
               && has_loading_image_extension( path )
               && ( filename == image_filename || filename.ends_with( author_prefixed_filename ) );
    } )
    | std::ranges::to<std::vector>();
}

auto get_loading_image_matches( const loading_image_match_options &opts ) ->
std::vector<std::string>
{
    using namespace cata::ranges;

    return get_loading_image_search_roots( opts.mod )
    | flat_map( [&]( const std::string & root_path ) { return get_loading_image_matches_at_root( opts.image_name, root_path ); } )
    | std::ranges::to<std::vector>();
}

auto get_loading_image_matches_for_mod( const MOD_INFORMATION *mod ) -> std::vector<std::string>
{
    using namespace cata::ranges;

    return mod->loading_images
    | flat_map( [mod]( const std::string & image_name ) { return get_loading_image_matches( loading_image_match_options{ .mod = *mod, .image_name = image_name } ); } )
    | std::ranges::to<std::vector>();
}

auto get_loading_image_paths( const std::vector<mod_id> &mods ) -> std::unordered_set<std::string>
{
    using namespace cata::ranges;
    using namespace std::views;

    const auto paths = mods
    | filter( []( const mod_id & mod ) { return mod.is_valid(); } )
    | transform( []( const mod_id & mod ) { return &*mod; } )
    | filter( []( const MOD_INFORMATION * mod ) { return !mod->loading_images.empty(); } )
    | flat_map( get_loading_image_matches_for_mod )
    | std::ranges::to<std::vector>();
    return std::unordered_set<std::string>( paths.begin(), paths.end() );
}

auto choose_loading_image_paths() -> std::vector<std::string>
{
    if( !can_choose_loading_image_path() ) { return {}; }

    const auto &world_info = *world_generator->active_world->info;
    const auto candidate_set = get_loading_image_paths( world_info.active_mod_order );
    auto candidates = std::vector<std::string>( candidate_set.begin(), candidate_set.end() );
    std::shuffle( candidates.begin(), candidates.end(), rng_get_engine() );
    return candidates;
}

auto get_loading_image_cache( loading_image_cache &cache,
                              const std::string &loading_image_path ) -> const loading_image_cache *
{
    if( loading_image_path.empty() ) {
        cache = {};
        return nullptr;
    }

    if( cache.path != loading_image_path ) {
        cache = {};
        cache.path = loading_image_path;
    }

    if( cache.attempted ) { return cache.texture ? &cache : nullptr; }

    cache.attempted = true;

    try {
        auto surface = load_image( loading_image_path.c_str() );
        cache.image_size = point( surface->w, surface->h );
        cache.texture = CreateTextureFromSurface( get_sdl_renderer(), surface );
    } catch( const std::exception &err ) {
        log_loading_image( string_format( "failed to load image '%s': %s", loading_image_path,
                                          err.what() ) );
        cache.path = loading_image_path;
        cache.image_size = point_zero;
        cache.attempted = true;
        return nullptr;
    }

    if( !cache.texture ) {
        log_loading_image( string_format( "failed to create texture for '%s'", loading_image_path ) );
        cache.path = loading_image_path;
        cache.attempted = true;
        return nullptr;
    }

    return &cache;
}

auto get_loading_image_rect( const point &image_size ) -> std::optional<SDL_Rect>
{
    const auto window_size = get_sdl_window_size();
    const auto buffer_size = get_sdl_display_buffer_size();
    if( window_size.x <= 0 || window_size.y <= 0 || buffer_size.x <= 0 || buffer_size.y <= 0 ) {
        return std::nullopt;
    }

    return get_scaled_loading_image_size( { .image_size = image_size, .screen_size = window_size } )
    .transform( [&window_size, &buffer_size]( const point & scaled_size ) {
        const auto output_rect = SDL_Rect{
            ( window_size.x - scaled_size.x ) / 2,
            ( window_size.y - scaled_size.y ) / 2,
            scaled_size.x,
            scaled_size.y
        };
        return SDL_Rect{
            static_cast<int>( std::lround( static_cast<double>( output_rect.x ) * buffer_size.x /
                                           window_size.x ) ),
            static_cast<int>( std::lround( static_cast<double>( output_rect.y ) * buffer_size.y /
                                           window_size.y ) ),
            static_cast<int>( std::lround( static_cast<double>( output_rect.w ) * buffer_size.x /
                                           window_size.x ) ),
            static_cast<int>( std::lround( static_cast<double>( output_rect.h ) * buffer_size.y /
                                           window_size.y ) )
        };
    } );
}

auto get_loading_image_author_pos( const std::string &text ) -> std::optional<point>
{
    const auto screen_dimensions = get_sdl_display_buffer_size();
    const auto font_size = get_sdl_font_size();
    if( screen_dimensions.x <= 0 || screen_dimensions.y <= 0 || font_size.x <= 0 ||
        font_size.y <= 0 ) {
        return std::nullopt;
    }

    const auto text_width = utf8_width( text, true );
    if( text_width <= 0 ) { return std::nullopt; }

    return point( std::max( 0, screen_dimensions.x - ( text_width + 1 ) * font_size.x ),
                  std::max( 0, screen_dimensions.y - font_size.y ) );
}

auto draw_loading_image_author( const std::string &author ) -> bool
{
    if( author.empty() ) { return false; }

    const auto text = string_format( _( "by %s" ), author );
    const auto text_pos = get_loading_image_author_pos( text );
    if( !text_pos ) { return false; }

    draw_sdl_text_outlined( {
        .text = text,
        .pos_pixel = *text_pos,
        .text_color = catacurses::white,
        .outline_color = catacurses::black,
        .outline_thickness = 2
    } );
    return true;
}

auto draw_loading_image_author_if_present( const std::optional<std::string> &author ) -> bool
{
    return author.transform( draw_loading_image_author ).value_or( false );
}

struct sdl_render_state_guard {
    const SDL_Renderer_Ptr &renderer;
    point logical_size = point_zero;
    float scale_x = 1.0f;
    float scale_y = 1.0f;
    SDL_Rect viewport = {};
    std::optional<SDL_Rect> clip_rect;
    SDL_RendererLogicalPresentation present;

    explicit sdl_render_state_guard( const SDL_Renderer_Ptr &renderer ) : renderer( renderer ) {
        SDL_GetRenderLogicalPresentation( renderer.get(), &logical_size.x, &logical_size.y,  &present );
        SDL_GetRenderScale( renderer.get(), &scale_x, &scale_y );
        SDL_GetRenderViewport( renderer.get(), &viewport );
        if( SDL_RenderClipEnabled( renderer.get() ) ) {
            clip_rect.emplace();
            SDL_GetRenderClipRect( renderer.get(), &*clip_rect );
        }
        SDL_SetRenderClipRect( renderer.get(), nullptr );
        SDL_SetRenderLogicalPresentation( renderer.get(), 0, 0, present );
        SDL_SetRenderScale( renderer.get(), 1.0f, 1.0f );
        SDL_SetRenderViewport( renderer.get(), nullptr );
    }

    ~sdl_render_state_guard() {
        if( logical_size.x > 0 && logical_size.y > 0 ) {
            SDL_SetRenderLogicalPresentation( renderer.get(), logical_size.x, logical_size.y, present );
        } else {
            SDL_SetRenderLogicalPresentation( renderer.get(), 0, 0, present );
            SDL_SetRenderScale( renderer.get(), scale_x, scale_y );
            SDL_SetRenderViewport( renderer.get(), &viewport );
        }
        SDL_SetRenderClipRect( renderer.get(), clip_rect ? &*clip_rect : nullptr );
    }
};

#endif // defined( TILES )

} // namespace

#if defined( TILES )
auto advance_loading_image( loading_image_selection_state &state ) -> bool
{
    if( state.paths.empty() ) {
        state.current_path.clear();
        state.current_author.reset();
        return false;
    }
    if( state.next_path >= state.paths.size() ) {
        state.next_path = 0;
    }

    state.current_path = state.paths[state.next_path++];
    state.current_author = get_loading_image_author( state.current_path );
    return true;
}

auto loading_image_splash::draw_current_loading_image() -> bool
{
    while( !this->selection_state->current_path.empty() ) {
        const auto *const cache = get_loading_image_cache( *loading_image_cache_state,
                                  this->selection_state->current_path );
        if( cache != nullptr ) {
            const auto rect = get_loading_image_rect( cache->image_size );
            if( !rect ) {
                log_loading_image( string_format( "failed to calculate rect for '%s'",
                                                  this->selection_state->current_path ) );
                return false;
            }
            const auto &renderer = get_sdl_renderer();
            const auto render_state_guard = sdl_render_state_guard( renderer );
            clear_sdl_display_buffer();
            SDL_FRect fRect{};
            SDL_RectToFRect( &*rect, &fRect );
            RenderCopy( renderer, cache->texture, nullptr, &fRect );
            draw_loading_image_author_if_present( this->selection_state->current_author );
            return true;
        }
        if( !advance_loading_image( *this->selection_state ) ) {
            break;
        }
    }

    return false;
}
#endif

#if defined( TILES )
loading_image_splash::loading_image_splash() : selection_state( &owned_selection_state )
{
    loading_image_cache_state = std::make_unique<loading_image_cache>();

    ui_background = std::make_unique<background_pane>( [this]() {
        if( !get_option<bool>( "LOADING_SCREEN_IMAGES" ) ) {
            return;
        }
        if( !this->selection_state->lookup_attempted && can_choose_loading_image_path() ) {
            this->selection_state->paths = choose_loading_image_paths();
            this->selection_state->next_path = 0;
            this->selection_state->lookup_attempted = true;
        }
        if( !selected_image_for_this_ui && this->selection_state->lookup_attempted ) {
            selected_image_for_this_ui = true;
            advance_loading_image( *this->selection_state );
        }
        draw_current_loading_image();
    } );
}

loading_image_splash::loading_image_splash( loading_image_selection_state &selection_state ) :
    selection_state( &selection_state )
{
    loading_image_cache_state = std::make_unique<loading_image_cache>();

    ui_background = std::make_unique<background_pane>( [this]() {
        if( !get_option<bool>( "LOADING_SCREEN_IMAGES" ) ) {
            return;
        }
        if( !this->selection_state->lookup_attempted && can_choose_loading_image_path() ) {
            this->selection_state->paths = choose_loading_image_paths();
            this->selection_state->next_path = 0;
            this->selection_state->lookup_attempted = true;
        }
        if( !selected_image_for_this_ui && this->selection_state->lookup_attempted ) {
            selected_image_for_this_ui = true;
            advance_loading_image( *this->selection_state );
        }
        draw_current_loading_image();
    } );
}
#else
loading_image_splash::loading_image_splash()
{
    ui_background = std::make_unique<background_pane>();
}
#endif

loading_image_splash::~loading_image_splash()
{
#if defined( TILES )
    clear_sdl_display_buffer_before_redraw();
#endif
}

loading_ui::loading_ui( bool display )
{
    if( display && !test_mode ) {
        menu = std::make_unique<uilist>();
        menu->settext( _( "Loading" ) );
    }
}

loading_ui::~loading_ui()
{
#if defined( TILES )
    clear_sdl_display_buffer_before_redraw();
#endif
}

void loading_ui::add_entry( const std::string &description )
{
    if( menu != nullptr ) {
        menu->addentry( menu->entries.size(), true, 0, description );
    }
}

void loading_ui::new_context( const std::string &desc )
{
    if( menu != nullptr ) {
        menu->reset();
        menu->settext( desc );
        ui = nullptr;
        ui_splash = nullptr;
    }
}

void loading_ui::init()
{
    if( menu != nullptr && ui == nullptr ) {
#if defined( TILES )
        ui_splash = std::make_unique<loading_image_splash>( loading_image_selection );
#else
        ui_splash = std::make_unique<loading_image_splash>();
#endif

        ui = std::make_unique<ui_adaptor>();
        ui->on_screen_resize( [this]( ui_adaptor & ui ) { menu->reposition( ui ); } );
        menu->reposition( *ui );
        ui->on_redraw( [this]( ui_adaptor & ui ) { menu->show( ui ); } );
    }
}

void loading_ui::proceed()
{
    init();

    if( menu != nullptr && !menu->entries.empty() ) {
        if( menu->selected >= 0 && menu->selected < static_cast<int>( menu->entries.size() ) ) {
            // TODO: Color it red if it errored hard, yellow on warnings
            menu->entries[menu->selected].text_color = c_green;
        }

        if( menu->selected + 1 < static_cast<int>( menu->entries.size() ) ) {
            menu->scrollby( 1 );
        }
    }

    show();
}

void loading_ui::show()
{
    init();

    if( menu != nullptr ) {
        ui_manager::redraw();
        refresh_display();
        inp_mngr.pump_events();
    }
}
