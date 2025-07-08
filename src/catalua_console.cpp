#include "catalua_console.h"

#include "catalua_log.h"
#include "catalua_impl.h"
#include "cursesdef.h"
#include "game.h"
#include "init.h"
#include "input.h"
#include "output.h"
#include "string_editor_window.h"
#include "string_utils.h"
#include "ui_manager.h"
#include "uistate.h"

namespace cata
{

struct folded_log_msg {
    bool is_head = false;
    LuaLogLevel level;
    std::string text;
};

static std::vector<folded_log_msg> build_folded_log( int width )
{
    std::vector<folded_log_msg> ret;
    for( const lua_log_msg &msg : get_lua_log_instance().get_entries() ) {
        std::vector<std::string> lines = foldstring( msg.text, width );
        for( int i = static_cast<int>( lines.size() ) - 1; i >= 0; i-- ) {
            ret.push_back( folded_log_msg{ i == 0, msg.level, std::move( lines[i] ) } );
        }
    }
    return ret;
}

static nc_color get_log_level_color( LuaLogLevel level )
{
    switch( level ) {
        case LuaLogLevel::Input:
            return c_white;
        case LuaLogLevel::DebugMsg:
            return c_magenta;
        case LuaLogLevel::Error:
            return c_red;
        case LuaLogLevel::Warn:
            return c_yellow;
        case LuaLogLevel::Info:
            return c_light_gray;
        default:
            debugmsg( "Log level color not defined!" );
            return c_white;
    }
}

static std::vector<std::string> &get_input_history()
{
    return uistate.gethistory( "LUA_CONSOLE" );
}

static int num_history_entries()
{
    return static_cast<int>( get_input_history().size() );
}

static void add_to_input_history( const std::string &s )
{
    std::vector<std::string> &hist = get_input_history();
    for( auto it = hist.begin(); it != hist.end(); it++ ) {
        if( *it == s ) {
            // Refresh existing history entry
            std::string msg = std::move( *it );
            hist.erase( it );
            hist.push_back( std::move( msg ) );
            return;
        }
    }
    // Add new history entry
    hist.push_back( s );
}

void show_lua_console_impl()
{
    input_context ctxt( "LUA_CONSOLE" );
    ctxt.register_action( "HELP_KEYBINDINGS" );
    ctxt.register_action( "EDIT" );
    ctxt.register_action( "QUIT" );
    ctxt.register_action( "LUA_RELOAD" );
    ctxt.register_action( "HISTORY_UP" );
    ctxt.register_action( "HISTORY_DOWN" );
    ctxt.register_action( "SCROLL_UP" );
    ctxt.register_action( "SCROLL_DOWN" );
    ctxt.register_action( "SCROLL_TOP" );
    ctxt.register_action( "SCROLL_BOTTOM" );

    ui_adaptor ui;

    constexpr int CURRENT_INPUT = -1;
    constexpr int scroll_speed = 5;
    constexpr int input_area_size = 5;

    point win_pos;
    point win_size;
    point log_pos;
    point log_size;
    point prompt_pos;
    point prompt_size;

    int log_scroll_pos = 0;
    std::vector<folded_log_msg> log_folded;

    const auto create_string_editor = [&]() {
        // Offset by one for the scrollbar
        return catacurses::newwin( prompt_size.y, prompt_size.x + 1, prompt_pos - point_east );
    };

    catacurses::window w_console;
    catacurses::window w_log;
    catacurses::window w_prompt;

    std::string current_input;
    int history_cursor = CURRENT_INPUT;

    bool is_editing = false;

    ui.on_screen_resize( [&]( ui_adaptor & ui ) {
        win_size = point( TERMX, TERMY );
        win_pos = point( ( TERMX - win_size.x ) / 2, ( TERMY - win_size.y ) / 2 );
        prompt_size = point( win_size.x - 3, input_area_size );
        prompt_pos = win_pos + point( 2, win_size.y - input_area_size - 1 );
        log_pos = win_pos + point_south_east;
        log_size = win_size + point( -2, -7 - input_area_size );
        w_console = catacurses::newwin( win_size.y, win_size.x, win_pos );
        w_log = catacurses::newwin( log_size.y, log_size.x, log_pos );
        w_prompt = catacurses::newwin( prompt_size.y, prompt_size.x, prompt_pos );
        ui.position_from_window( w_console );
    } );
    ui.mark_resize();

    ui.on_redraw( [&]( const ui_adaptor & ) {
        werase( w_console );
        draw_border( w_console );
        std::string separator;
        int separator_y = win_size.y - prompt_size.y - 2;
        for( int i = 0; i < win_size.x - 2; i++ ) {
            separator += LINE_OXOX_S;
        }
        mvwprintz( w_console, point( 1, separator_y ), c_light_gray, separator );
        mvwprintz( w_console, point( 1, separator_y - 4 ), c_light_gray, separator );
        if( is_editing ) {
            mvwprintz( w_console, point( 1, separator_y - 3 ), c_light_gray,
                       _( "Press Ctrl+S to run script, press Esc to cancel" )
                     );
            mvwprintz( w_console, point( 1, separator_y - 2 ), c_light_gray,
                       _( "Press arrow keys to navigate text input" )
                     );
            mvwprintz( w_console, point( 1, separator_y - 1 ), c_light_gray,
                       _( "Press Enter to add new line" )
                     );
        } else {
            mvwprintz( w_console, point( 1, separator_y - 3 ), c_light_gray,
                       _( "Press Enter to enter/edit command, Esc to quit" )
                     );
            mvwprintz( w_console, point( 1, separator_y - 2 ), c_light_gray,
                       _( "Press Up/Down arrows to select from history" )
                     );
            mvwprintz( w_console, point( 1, separator_y - 1 ), c_light_gray,
                       _( "Press PgUp/PgDn/Home/End to scroll output window" )
                     );
        }

        scrollbar()
        .offset_x( 0 )
        .offset_y( 1 )
        .content_size( log_folded.size() )
        .viewport_size( log_size.y )
        .viewport_pos( static_cast<int>( log_folded.size() ) - log_scroll_pos - log_size.y )
        .scroll_to_last( false )
        .apply( w_console );

        wnoutrefresh( w_console );

        werase( w_log );

        int y_pos = log_size.y - 1;
        int start_line = log_scroll_pos;
        int end_line = std::min( static_cast<int>( log_folded.size() ), start_line + log_size.y );
        for( int log_line = start_line; log_line < end_line; log_line++ ) {
            const folded_log_msg &msg = log_folded[log_line];
            nc_color col = get_log_level_color( msg.level );
            int x_pos = 1;
            if( msg.level == LuaLogLevel::Input ) {
                // User input is indented, and first line is highlighted
                x_pos = 2;
                if( msg.is_head ) {
                    mvwprintz( w_log, point( 1, y_pos ), col, ">" );
                }
            }
            mvwprintz( w_log, point( x_pos, y_pos ), col, msg.text );
            y_pos--;
        }

        wnoutrefresh( w_log );

        werase( w_prompt );

        print_scrollable(
            w_prompt,
            0,
            history_cursor == CURRENT_INPUT ? current_input : get_input_history()[history_cursor],
            c_light_gray,
            ""
        );

        wnoutrefresh( w_prompt );
    } );

    bool log_invalidated = true;
    while( true ) {
        if( log_invalidated ) {
            log_invalidated = false;
            log_scroll_pos = 0;
            log_folded = build_folded_log( 76 );
        }
        ui_manager::redraw_invalidated();

        const std::string act = ctxt.handle_input();

        if( act == "QUIT" ) {
            // Close
            return;
        } else if( act == "HISTORY_UP" ) {
            int sz = num_history_entries();
            if( sz != 0 && history_cursor != 0 ) {
                if( history_cursor == CURRENT_INPUT ) {
                    history_cursor = sz - 1;
                } else {
                    history_cursor = std::max( 0, history_cursor - 1 );
                }
            }
            // Update input preview
            ui.invalidate_ui();
        } else if( act == "HISTORY_DOWN" ) {
            int sz = num_history_entries();
            if( sz != 0 && history_cursor != CURRENT_INPUT ) {
                if( history_cursor == sz - 1 ) {
                    history_cursor = CURRENT_INPUT;
                } else {
                    history_cursor = std::min( history_cursor + 1, sz - 1 );
                }
            }
            // Update input preview
            ui.invalidate_ui();
        } else if( act == "SCROLL_UP" || act == "SCROLL_TOP" ) {
            int limit = std::max( 0, static_cast<int>( log_folded.size() ) - log_size.y );
            if( act == "SCROLL_TOP" ) {
                log_scroll_pos = limit;
            } else {
                log_scroll_pos = std::min( limit, log_scroll_pos + scroll_speed );
            }
            ui.invalidate_ui();
        } else if( act == "SCROLL_DOWN" ) {
            log_scroll_pos = std::max( 0, log_scroll_pos - scroll_speed );
            ui.invalidate_ui();
        } else if( act == "SCROLL_BOTTOM" ) {
            log_scroll_pos = 0;
            ui.invalidate_ui();
        } else if( act == "EDIT" ) {
            // Edit
            is_editing = true;
            ui.invalidate_ui();
            string_editor_window ew(
                create_string_editor,
                history_cursor == CURRENT_INPUT ? current_input : get_input_history()[history_cursor]
            );
            std::pair<bool, std::string> res = ew.query_string();
            is_editing = false;
            history_cursor = CURRENT_INPUT;
            if( res.first ) {
                // Confirmed
                current_input.clear();
                add_to_input_history( res.second );
                log_invalidated = true;
                run_console_input( DynamicDataLoader::get_instance().lua->lua, res.second );
            } else {
                // Canceled, save input for later use
                current_input = res.second;
            }
        } else if( act == "LUA_RELOAD" ) {
            ui.invalidate_ui();
            log_invalidated = true;
            reload_lua_code();
        }
    }
}

} // namespace cata
