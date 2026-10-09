#if !defined(_WIN32) && (defined(CURSES) || defined(CATA_CURSES_CLIENT))

#    include "input.h"

#    define NCURSES_NOMACROS
#    if !defined(__APPLE__)
#        define NCURSES_WIDECHAR 1
#    endif
#    if defined(__CYGWIN__)
#        include <ncurses/curses.h>
#    else
#        include <curses.h>
#    endif

#    include "catacharset.h"
#    include "client_backend.h"
#    include "color.h"
#    include "color_loader.h"
#    include "cursesdef.h"
#    include "cursesport.h"
#    include "debug.h"
#    include "game_ui.h"
#    include "hsv_color.h"
#    include "output.h"

#    include <algorithm>
#    include <array>
#    include <cstdint>
#    include <cstring>
#    include <langinfo.h>
#    include <ranges>
#    include <stdexcept>

namespace
{

auto native_stdscr() -> ::WINDOW* { return ::stdscr; }

int native_timeout = -1;

auto native_pair_index( const int foreground, const int background ) -> short
{
    // Pair zero is the terminal's white-on-black default. Reserve the other 63 combinations.
    const auto encoded = ( foreground & 7 ) * 8 + ( background & 7 );
    return static_cast<short>( encoded == 56 ? 0 : encoded < 56 ? encoded + 1 : encoded );
}
auto native_pair( const cata_cursesport::cursecell &cell ) -> short
{
    return COLOR_PAIRS >= 64 ? native_pair_index( cell.FG, cell.BG ) : 0;
}
auto ensure_term_size() -> void;
auto check_encoding() -> void;

} // namespace

auto game_client::curses::draw_window_native( const catacurses::window &window ) -> void
{
    auto *const source = window.get<cata_cursesport::WINDOW>();
    auto *const target = native_stdscr();
    if( source == nullptr || target == nullptr ) { return; }
    for( int y = 0; y < source->height; ++y ) {
        const auto target_y = source->pos.y + y;
        if( target_y < 0 || target_y >= ::getmaxy( target ) ) { continue; }
        for( int x = 0; x < source->width; ++x ) {
            const auto target_x = source->pos.x + x;
            if( target_x < 0 || target_x >= ::getmaxx( target ) ) { continue; }
            const auto &cell = source->line[y].chars[x];
            ::wmove( target, target_y, target_x );
            ::wattrset( target,
                        COLOR_PAIR( native_pair( cell ) ) | ( static_cast<int>( cell.FG ) >= 8 ? A_BOLD : 0 )
                        | ( static_cast<int>( cell.BG ) >= 8 ? A_BLINK : 0 ) );
            if( !cell.ch.empty() ) { ::waddstr( target, cell.ch.c_str() ); }
        }
        source->line[y].touched = false;
    }
    source->draw = false;
}

auto game_client::curses::clear_window_native( const catacurses::window & /*window*/ ) -> void {}

auto game_client::curses::present_native() -> void
{
    if( native_stdscr() != nullptr ) {
        const auto *const screen = catacurses::newscr.get<cata_cursesport::WINDOW>();
        if( screen != nullptr ) {
            const auto cursor = screen->cursor;
            if( cursor.x >= 0 && cursor.y >= 0 && cursor.x < ::getmaxx( native_stdscr() )
                && cursor.y < ::getmaxy( native_stdscr() ) ) {
                ::wmove( native_stdscr(), cursor.y, cursor.x );
            }
        }
        ::wrefresh( native_stdscr() );
    }
}

auto game_client::curses::initialize_native() -> void
{
    if( native_stdscr() == nullptr ) {
        if( ::initscr() == nullptr ) { throw std::runtime_error( "initscr failed" ); }
        ::noecho();
        ::cbreak();
        ::keypad( native_stdscr(), true );
        ::set_escdelay( 10 );
        ::start_color();
        if( COLOR_PAIRS >= 64 && COLORS >= 8 ) {
            for( const auto foreground : std::views::iota( 0, 8 ) ) {
                for( const auto background : std::views::iota( 0, 8 ) ) {
                    const auto pair = native_pair_index( foreground, background );
                    if( pair != 0 ) {
                        ::init_pair( pair, static_cast<short>( foreground ),
                                     static_cast<short>( background ) );
                    }
                }
            }
        }
#    if !defined(__CYGWIN__)
        ::mousemask( BUTTON1_CLICKED | BUTTON3_CLICKED | REPORT_MOUSE_POSITION, nullptr );
#    endif
    }
    auto palette = std::array<RGBColor, color_loader<RGBColor>::COLOR_NAMES_COUNT> {};
    color_loader<RGBColor>().load( palette );
    init_colors();
    const auto height = ::getmaxy( native_stdscr() );
    const auto width = ::getmaxx( native_stdscr() );
    catacurses::stdscr = catacurses::newwin( height, width, point_zero );
    catacurses::newscr = catacurses::newwin( height, width, point_zero );
    check_encoding();
    ensure_term_size();
}

auto game_client::curses::shutdown_native() -> void
{
    catacurses::stdscr = {};
    catacurses::newscr = {};
    if( native_stdscr() != nullptr ) { ::endwin(); }
}

auto game_client::curses::set_cursor_native( const int visibility ) -> void
{
    ::curs_set( visibility );
}

auto game_client::curses::pump_events_native() -> void
{
    if( test_mode ) { return; }
    const auto previous_timeout = native_timeout;
    game_client::curses::set_timeout_native( 0 );
    auto key = ::getch();
    auto resize = false;
    while( key != ERR ) {
        resize = resize || key == KEY_RESIZE;
        key = ::getch();
    }
    game_client::curses::set_timeout_native( previous_timeout );
    if( resize ) {
        game_client::curses::resize_native(
            game_client::curses::terminal_width_native(),
            game_client::curses::terminal_height_native() );
        catacurses::resizeterm();
    }
}

auto game_client::curses::read_input_native() -> input_event
{
    catacurses::doupdate();
    auto key = ERR;
    input_event result;
    do {
        key = ::getch();
        if( key != ERR ) {
            auto newch = ERR;
            const auto previous_timeout = native_timeout;
            game_client::curses::set_timeout_native( 0 );
            do { newch = ::getch(); }
            while( newch != ERR && newch == key );
            game_client::curses::set_timeout_native( previous_timeout );
            if( newch != ERR && newch != key ) { ::ungetch( newch ); }
        }
        result = input_event();
        if( key == ERR ) {
            result.type = native_timeout > 0 ? input_event_t::timeout : input_event_t::error;
        } else if( key == KEY_RESIZE ) {
            game_client::curses::resize_native(
                game_client::curses::terminal_width_native(),
                game_client::curses::terminal_height_native() );
            catacurses::resizeterm();
        } else if( key == KEY_MOUSE ) {
            MEVENT event;
            if( ::getmouse( &event ) == OK ) {
                result.type = input_event_t::mouse;
                result.mouse_pos = point( event.x, event.y );
                if( event.bstate & BUTTON1_CLICKED ) {
                    result.add_input( MOUSE_BUTTON_LEFT );
                } else if( event.bstate & BUTTON3_CLICKED ) {
                    result.add_input( MOUSE_BUTTON_RIGHT );
                } else if( event.bstate & REPORT_MOUSE_POSITION ) {
                    result.add_input( MOUSE_MOVE );
                } else {
                    result.type = input_event_t::error;
                }
            } else {
                result.type = input_event_t::error;
            }
        } else {
            if( key == 127 ) { return input_event( KEY_BACKSPACE, input_event_t::keyboard ); }
            result.type = input_event_t::keyboard;
            result.text.append( 1, static_cast<char>( key ) );
            if( key >= 194 && key <= 223 ) {
                result.text.append( 1, static_cast<char>( ::getch() ) );
            } else if( key >= 224 && key <= 239 ) {
                result.text.append( 1, static_cast<char>( ::getch() ) );
                result.text.append( 1, static_cast<char>( ::getch() ) );
            } else if( key >= 240 && key <= 244 ) {
                result.text.append( 1, static_cast<char>( ::getch() ) );
                result.text.append( 1, static_cast<char>( ::getch() ) );
                result.text.append( 1, static_cast<char>( ::getch() ) );
            } else if( key >= 127 ) {
                return input_event( key, input_event_t::keyboard );
            }
            const auto codepoint = UTF8_getch( result.text );
            if( codepoint == UNKNOWN_UNICODE ) { return input_event( key, input_event_t::keyboard ); }
            result.add_input( key );
        }
    } while( key == KEY_RESIZE );
    return result;
}

auto game_client::curses::set_timeout_native( const int timeout ) -> void
{
    native_timeout = timeout;
    ::timeout( timeout < 0 ? -1 : timeout );
}

auto game_client::curses::terminal_width_native() -> int
{
    return native_stdscr() == nullptr ? 0 : ::getmaxx( native_stdscr() );
}

auto game_client::curses::terminal_height_native() -> int
{
    return native_stdscr() == nullptr ? 0 : ::getmaxy( native_stdscr() );
}

namespace
{
auto ensure_term_size() -> void
{
    // do not use ui_adaptor here to avoid re-entry
    const auto minHeight = FULL_SCREEN_HEIGHT;
    const auto minWidth = FULL_SCREEN_WIDTH;
    auto maxy = getmaxy( catacurses::stdscr );
    auto maxx = getmaxx( catacurses::stdscr );

    while( maxy < minHeight || maxx < minWidth ) {
        catacurses::erase();
        if( maxy < minHeight && maxx < minWidth ) {
            fold_and_print(
                catacurses::stdscr, point_zero, maxx, c_white,
                _( "Whoa!  Your terminal is tiny!  This game requires a minimum terminal size of "
                   "%dx%d to work properly.  %dx%d just won't do.  Maybe a smaller font would help?" ),
                minWidth, minHeight, maxx, maxy );
        } else if( maxx < minWidth ) {
            fold_and_print(
                catacurses::stdscr, point_zero, maxx, c_white,
                _( "Oh!  Hey, look at that.  Your terminal is just a little too narrow.  This game "
                   "requires a minimum terminal size of %dx%d to function.  It just won't work "
                   "with only %dx%d.  Can you stretch it out sideways a bit?" ),
                minWidth, minHeight, maxx, maxy );
        } else {
            fold_and_print(
                catacurses::stdscr, point_zero, maxx, c_white,
                _( "Woah, woah, we're just a little short on space here.  The game requires a "
                   "minimum terminal size of %dx%d to run.  %dx%d isn't quite enough!  Can you "
                   "make the terminal just a smidgen taller?" ),
                minWidth, minHeight, maxx, maxy );
        }
        catacurses::refresh();
        // do not use input_manager or input_context here to avoid re-entry
        ::getch();
        game_client::curses::resize_native( ::getmaxx( native_stdscr() ), ::getmaxy( native_stdscr() ) );
        maxy = getmaxy( catacurses::stdscr );
        maxx = getmaxx( catacurses::stdscr );
    }
}

auto check_encoding() -> void
{
    // Check whether LC_CTYPE supports the UTF-8 encoding
    // and show a warning if it doesn't
    if( std::strcmp( nl_langinfo( CODESET ), "UTF-8" ) != 0 ) {
        // do not use ui_adaptor here to avoid re-entry
        auto key = ERR;
        do {
            const auto unicode_error_msg = _(
                                               "You don't seem to have a valid Unicode locale.  You may see some weird "
                                               "characters (e.g. empty boxes or question marks). You have been warned." );
            catacurses::erase();
            const auto maxx = getmaxx( catacurses::stdscr );
            fold_and_print( catacurses::stdscr, point_zero, maxx, c_white, unicode_error_msg );
            catacurses::refresh();
            // do not use input_manager or input_context here to avoid re-entry
            key = ::getch();
            game_client::curses::
            resize_native( ::getmaxx( native_stdscr() ), ::getmaxy( native_stdscr() ) );
        } while( key == KEY_RESIZE || key == KEY_MOUSE );
    }
}

} // namespace

auto game_client::curses::resize_native( const int cell_w, const int cell_h ) -> void
{
    // resizeterm() queues another KEY_RESIZE; the shared UI already handles this event.
    ::resize_term( cell_h, cell_w );
    if( native_stdscr() != nullptr ) {
        catacurses::stdscr = catacurses::newwin( cell_h, cell_w, point_zero );
        catacurses::newscr = catacurses::newwin( cell_h, cell_w, point_zero );
    }
}

namespace game_client
{

namespace
{

class curses_backend final: public backend
{
    public:
        auto initialize() -> void override { game_client::curses::initialize_native(); }
        auto shutdown() -> void override { game_client::curses::shutdown_native(); }
        auto present() -> void override { game_client::curses::present_native(); }
        auto draw_window( const catacurses::window &window ) -> void override {
            game_client::curses::draw_window_native( window );
        }
        auto clear_window( const catacurses::window &window ) -> void override {
            game_client::curses::clear_window_native( window );
        }
        auto set_cursor( const int visibility ) -> void override {
            game_client::curses::set_cursor_native( visibility );
        }
        auto set_timeout( const int timeout_ms ) -> void override {
            game_client::curses::set_timeout_native( timeout_ms );
        }
        auto read_input( const int timeout_ms ) -> input_event override {
            const auto previous_timeout = inp_mngr.get_timeout();
            game_client::curses::set_timeout_native( timeout_ms );
            auto result = game_client::curses::read_input_native();
            game_client::curses::set_timeout_native( previous_timeout );
            return result;
        }
        auto pump_events() -> void override { game_client::curses::pump_events_native(); }
        auto resize( const point cell_size ) -> void override {
            game_client::curses::resize_native( cell_size.x, cell_size.y );
        }
        auto projected_size() const -> point override {
            return point( game_client::curses::terminal_width_native(),
                          game_client::curses::terminal_height_native() );
        }
        auto capabilities() const -> client_capabilities override {
            return {.tiles = false, .mouse = true, .gamepad = false};
        }
};

} // namespace

auto make_curses_backend() -> backend_ptr { return std::make_unique<curses_backend>(); }

} // namespace game_client

#endif // CATA_CURSES_CLIENT
