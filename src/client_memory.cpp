#include "client_memory.h"
#include "client_memory_scope.h"

#include "color_loader.h"
#include "cursesdef.h"
#include "cursesport.h"
#include "hsv_color.h"
#include "output.h"

#include <algorithm>
#include <array>
#include <utility>

namespace
{

struct memory_state {
    std::vector<game_client::screen_cell> cells;
    int width = 0;
    int height = 0;
    bool cursor_visible = false;
    int timeout = -1;
    game_client::memory::input_provider input;
    std::function < auto( const game_client::screen_snapshot & )->void > present;
    /// A window other than stdscr has been drawn since stdscr last was.
    bool overlaid = false;
};

auto state() -> memory_state &
{
    static auto result = memory_state{};
    return result;
}

auto cell_at( const int x, const int y ) -> game_client::screen_cell &
{
    return state().cells[static_cast<size_t>( y * state().width + x )];
}

auto resize_window( const catacurses::window &window, const int width, const int height ) -> void
{
    auto *const native = window.get<cata_cursesport::WINDOW>();
    if( native == nullptr ) { return; }
    native->width = width;
    native->height = height;
    native->line.resize( height );
    for( auto &line : native->line ) {
        line.chars.resize( width );
        line.touched = true;
    }
    native->cursor.x = std::clamp( native->cursor.x, 0, width - 1 );
    native->cursor.y = std::clamp( native->cursor.y, 0, height - 1 );
    native->draw = true;
}

} // namespace

namespace game_client::memory
{

struct scoped_state::saved_state {
    memory_state memory;
    catacurses::window screen;
    catacurses::window new_screen;
};

scoped_state::scoped_state() : saved_( std::make_unique<saved_state>() )
{
    // Allocate first; the swaps cannot throw or modify the borrowed window buffers.
    std::swap( saved_->memory, state() );
    std::swap( saved_->screen, catacurses::stdscr );
    std::swap( saved_->new_screen, catacurses::newscr );
}

scoped_state::~scoped_state() noexcept
{
    std::swap( saved_->memory, state() );
    std::swap( saved_->screen, catacurses::stdscr );
    std::swap( saved_->new_screen, catacurses::newscr );
}

auto initialize( const int width, const int height ) -> void
{
    resize( width, height );
    catacurses::stdscr = catacurses::newwin( height, width, point_zero );
    catacurses::newscr = catacurses::newwin( height, width, point_zero );
    auto palette = std::array<RGBColor, color_loader<RGBColor>::COLOR_NAMES_COUNT> {};
    color_loader<RGBColor>().load( palette );
    init_colors();
}

auto shutdown() -> void
{
    catacurses::stdscr = {};
    catacurses::newscr = {};
    state() = memory_state{};
}

auto set_input_provider( input_provider provider ) -> void
{
    state().input = std::move( provider );
}

auto set_present_callback(
    std::function < auto( const screen_snapshot & )->void > callback ) -> void
{
    state().present = std::move( callback );
}

auto snapshot() -> screen_snapshot
{
    auto &memory = state();
    auto result = screen_snapshot{
        .width = memory.width,
        .height = memory.height,
        .cursor = point_zero,
        .cursor_visible = memory.cursor_visible,
        .cells = memory.cells,
        .text = {},
    };
    const auto *const newscr = catacurses::newscr.get<cata_cursesport::WINDOW>();
    if( newscr != nullptr ) {
        result.cursor = point{
            std::clamp( newscr->cursor.x, 0, std::max( 0, memory.width - 1 ) ),
            std::clamp( newscr->cursor.y, 0, std::max( 0, memory.height - 1 ) )
        };
    }
    for( int y = 0; y < memory.height; ++y ) {
        if( y > 0 ) { result.text += '\n'; }
        for( int x = 0; x < memory.width; ++x ) {
            result.text += memory.cells[static_cast<size_t>( y * memory.width + x )].text;
        }
    }
    return result;
}

auto screen_size() -> point
{
    const auto &memory = state();
    return point{memory.width, memory.height};
}

auto resize( const int width, const int height ) -> void
{
    auto &memory = state();
    const auto old_cells = std::move( memory.cells );
    const auto old_width = memory.width;
    const auto old_height = memory.height;
    memory.width = std::max( 1, width );
    memory.height = std::max( 1, height );
    memory.cells.assign( static_cast<size_t>( memory.width * memory.height ), screen_cell{} );
    for( int y = 0; y < std::min( old_height, memory.height ); ++y ) {
        for( int x = 0; x < std::min( old_width, memory.width ); ++x ) {
            memory.cells[static_cast<size_t>( y * memory.width + x )] =
                old_cells[static_cast<size_t>( y * old_width + x )];
        }
    }
    resize_window( catacurses::stdscr, memory.width, memory.height );
    resize_window( catacurses::newscr, memory.width, memory.height );
}

auto draw_window( const catacurses::window &window ) -> void
{
    auto *const source = window.get<cata_cursesport::WINDOW>();
    if( source == nullptr ) { return; }
    auto &memory = state();
    memory.overlaid = source != catacurses::stdscr.get<cata_cursesport::WINDOW>();
    for( int y = 0; y < source->height; ++y ) {
        const auto target_y = source->pos.y + y;
        if( target_y < 0 || target_y >= memory.height ) { continue; }
        for( int x = 0; x < source->width; ++x ) {
            const auto target_x = source->pos.x + x;
            if( target_x < 0 || target_x >= memory.width ) { continue; }
            const auto &source_cell = source->line[y].chars[x];
            auto &target_cell = cell_at( target_x, target_y );
            target_cell.text = source_cell.ch;
            target_cell.foreground = static_cast<int>( source_cell.FG );
            target_cell.background = static_cast<int>( source_cell.BG );
        }
        source->line[y].touched = false;
    }
    source->draw = false;
}

auto clear_window( const catacurses::window & /*window*/ ) -> void {}

auto set_cursor( const int visibility ) -> void
{
    state().cursor_visible = visibility != 0;
}

auto set_timeout( const int timeout ) -> void
{
    state().timeout = timeout;
}

auto present() -> void
{
    if( const auto callback = state().present ) { callback( snapshot() ); }
}

auto read_input() -> input_event
{
    const auto *const stdscr = catacurses::stdscr.get<cata_cursesport::WINDOW>();
    // The base window drawn after the windows on top of it would blank them.
    if( stdscr != nullptr && stdscr->draw && !state().overlaid ) { draw_window( catacurses::stdscr ); }
    present();
    if( const auto provider = state().input ) { return provider( state().timeout ); }
    auto result = input_event{};
    result.type = state().timeout > 0 ? input_event_t::timeout : input_event_t::error;
    return result;
}

} // namespace game_client::memory
