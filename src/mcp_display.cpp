#include "avatar.h"
#include "client_memory.h"
#include "client_render_hooks.h"
#include "color_loader.h"
#include "cuboid_rectangle.h"
#include "cursesdef.h"
#include "cursesport.h"
#include "engine_client_session.h"
#include "engine_client_world.h"
#include "game.h"
#include "game_constants.h"
#include "game_ui.h"
#include "hsv_color.h"
#include "input.h"
#include "json.h"
#include "map/map.h"
#include "mcp_session.h"
#include "ncurses_def.h"
#include "options.h"
#include "output.h"
#include "panels.h"
#include "ui_manager.h"
#include "vehicle/vehicle.h"
#include "vehicle/vpart_position.h"

#if defined(_WIN32)
#   include "platform_win.h"
#endif

#include <algorithm>
#include <array>
#include <ranges>
#include <vector>

/// The display of the protocol engine: the curses windows land in a memory screen, and input comes from the client.
namespace memory = game_client::memory;

catacurses::window catacurses::newscr;

auto catacurses::init_interface() -> void
{
    const auto width = std::max( { FULL_SCREEN_WIDTH, memory::minimum_terminal.x, get_option<int>( "TERMINAL_X" ) } );
    const auto height = std::max( { FULL_SCREEN_HEIGHT, memory::minimum_terminal.y, get_option<int>( "TERMINAL_Y" ) } );
    memory::initialize( width, height );
    engine_client::process_session().set_world_capture( &engine_client::world::capture_world,
            &engine_client::world::capture_patch );
}

auto catacurses::endwin() -> void
{
    bn::mcp::finish_session();
    memory::shutdown();
}

auto catacurses::curs_set( const int visibility ) -> void { memory::set_cursor( visibility ); }

auto cata_cursesport::curses_drawwindow( const catacurses::window &win ) -> void { memory::draw_window( win ); }
auto cata_cursesport::handle_additional_window_clear( WINDOW * /*win*/ ) -> void {}

auto clear_window_area( const catacurses::window &win ) -> void { memory::clear_window( win ); }
auto refresh_display() -> void { memory::present(); }
auto projected_window_width() -> int { return memory::screen_size().x; }
auto projected_window_height() -> int { return memory::screen_size().y; }
auto get_terminal_width() -> int { return memory::screen_size().x; }
auto get_terminal_height() -> int { return memory::screen_size().y; }
auto get_scaling_factor() -> int { return 1; }
auto handle_resize( const int /*w*/, const int /*h*/ ) -> bool { return false; }
auto resize_term( const int cell_w, const int cell_h ) -> void { memory::resize( cell_w, cell_h ); }

auto game_ui::resize_terrain_window( const point cells ) -> void
{
    auto &panels = panel_manager::get_manager();
    resize_term( std::max( { FULL_SCREEN_WIDTH, memory::minimum_terminal.x,
                             cells.x + panels.get_width_left() + panels.get_width_right() } ),
                 std::max( { FULL_SCREEN_HEIGHT, memory::minimum_terminal.y, cells.y } ) );
    init_ui();
    ui_manager::screen_resized();
}

auto gamepad_available() -> bool { return false; }

auto input_manager::read_native_input_event() -> input_event
{
    memory::set_timeout( get_timeout() );
    const auto result = engine_client::process_session().delivered( memory::read_input() );
    previously_pressed_key = result.type == input_event_t::keyboard ? result.get_first_input() : 0;
    return result;
}

auto input_manager::pump_events() -> void { previously_pressed_key = 0; }

auto input_manager::set_timeout( const int timeout ) -> void
{
    input_timeout = timeout;
    memory::set_timeout( timeout );
}

auto input_context::get_coordinates( const catacurses::window &capture_win ) ->
std::optional<tripoint_bub_ms>
{
    if( !coordinate_input_received || g == nullptr ) { return std::nullopt; }
    const auto &capture_window = capture_win ? capture_win : g->w_terrain;
    if( !capture_window ) { return std::nullopt; }
    const auto view_size = point( getmaxx( capture_window ), getmaxy( capture_window ) );
    const auto win_min = point( getbegx( capture_window ), getbegy( capture_window ) );
    const auto win_bounds = half_open_rectangle<point>( win_min, win_min + view_size );
    if( !win_bounds.contains( coordinate ) ) { return std::nullopt; }
    auto view_offset = point_bub_ms{};
    if( capture_window == g->w_terrain ) { view_offset = g->terrain_view_center(); }
    const auto p = view_offset + ( coordinate - win_min ) - view_size / 2;
    return tripoint_bub_ms( p, g->get_levz() );
}

template<>
auto color_loader<RGBColor>::from_rgb( const int r, const int g, const int b ) -> RGBColor
{
    return RGBColor{ static_cast<uint8_t>( r ), static_cast<uint8_t>( g ), static_cast<uint8_t>( b ), 255 };
}

auto ncurses::color_to_RGB( const nc_color &color ) -> RGBColor
{
    static const auto palette = []() {
        auto colors = std::array<RGBColor, color_loader<RGBColor>::COLOR_NAMES_COUNT> {};
        color_loader<RGBColor> {}.load( colors );
        return colors;
    }
    ();
    const auto pair_index = std::clamp( color.to_color_pair_index(), 0,
                                        static_cast<int>( cata_cursesport::colorpairs.size() ) - 1 );
    const auto &pair = cata_cursesport::colorpairs[pair_index];
    auto index = static_cast<int>( pair.FG != catacurses::black ? pair.FG : pair.BG );
    if( color.is_bold() ) { index += palette.size() / 2; }
    return palette[std::clamp( index, 0, static_cast<int>( palette.size() ) - 1 )];
}

namespace
{
auto minimap_color_at( const map &here, const tripoint_bub_ms &p ) -> nc_color
{
    if( const auto vp = here.veh_at( p ) ) { return vp->vehicle().part_color( vp->part_index() ); }
    if( const auto furn = here.furn( p ) ) { return furn->color(); }
    return here.ter( p )->color();
}
} // namespace

auto game::draw_pixel_minimap( const catacurses::window &w ) -> void
{
    w_pixel_minimap = w;
    // A protocol client has no pixels: each cell shows the colour of the lit map tile it covers.
    const auto &here = get_map();
    const auto &cache = here.access_cache( u.bub_pos().z() );
    const auto size = point( getmaxx( w ), getmaxy( w ) );
    for( const auto y : std::views::iota( 0, size.y ) ) {
        for( const auto x : std::views::iota( 0, size.x ) ) {
            const auto at = tripoint_bub_ms( u.bub_pos().xy() + point( x, y ) - size / 2, u.bub_pos().z() );
            if( !here.inbounds( at ) ) { continue; }
            const auto lighting = cache.visibility_cache[cache.idx( at.x(), at.y() )];
            if( lighting == lit_level::DARK || lighting == lit_level::BLANK ) { continue; }
            mvwputch_inv( w, point( x, y ), minimap_color_at( here, at ), " " );
        }
    }
}

namespace
{
auto &declared()
{
    static auto list = std::vector<game_client::mod_tileset_declaration> {};
    return list;
}
} // namespace

auto game_client::declared_mod_tilesets() -> const std::vector<mod_tileset_declaration> & { return declared(); } // *NOPAD*

auto game_client::load_mod_tileset( const JsonObject &object, const std::string & /*source*/,
                                    const std::string &base_path, const std::string &full_path ) -> void
{
    const auto index = 1 + std::ranges::count( declared(), full_path,
                       &mod_tileset_declaration::full_path );
    declared().push_back( {
        .base_path = base_path, .full_path = full_path, .index = static_cast<int>( index ),
        .compatibility = object.get_string_array( "compatibility" )
    } );
    object.allow_omitted_members();
}

auto game_client::reset_mod_tileset() -> void { declared().clear(); }

#if defined(_WIN32)
auto getWindowHandle() -> HWND { return nullptr; }
#endif
