#include "game.h" // IWYU pragma: associated

#include <algorithm>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "color.h"
#include "cursesdef.h"
#include "debug.h"
#include "client_choice.h"
#include "diary.h"
#include "input.h"
#include "options.h"
#include "output.h"
#include "popup.h"
#include "string_editor_window.h"
#include "string_formatter.h"
#include "string_input_popup.h"
#include "translations.h"
#include "ui.h"
#include "ui_manager.h"
#include "wcwidth.h"

namespace
{
// maximum limit of the UIs width. After that, we center them, with an even space left and right
const int MAX_DAIRY_UI_WIDTH = 150;

/**print a scrollable list, printed std::vector<std::string> as list with scrollbar*/
void print_list_scrollable( catacurses::window *win, std::vector<std::string> list, int *selection,
                            int entries_per_page, int xoffset, int width, bool active, bool border,
                            const report_color_error color_error )
{
    if( *selection < 0 && !list.empty() ) {
        *selection = static_cast<int>( list.size() ) - 1;
    } else if( *selection >= static_cast<int>( list.size() ) ) {
        *selection = 0;
    }
    const int borderspace = border ? 1 : 0;
    entries_per_page = entries_per_page - borderspace * 2;


    const int top_of_page = entries_per_page * ( *selection / entries_per_page );

    const int bottom_of_page = std::min<int>( top_of_page + entries_per_page, list.size() );

    const int line_width = width - 1 - borderspace;

    for( int i = top_of_page; i < bottom_of_page; i++ ) {

        const nc_color col = c_white;
        const int y = i - top_of_page;
        const bool highlight = *selection == i && active;
        const nc_color line_color = highlight ? hilite( col ) : col;
        std::string line_str = list[i];
        if( highlight ) {
            line_str = left_justify( remove_color_tags( line_str ), line_width );
        }
        trim_and_print( *win, point( xoffset + 1, y + borderspace ), line_width,
                        line_color, line_str, color_error );
    }
    if( border ) {
        draw_border( *win );
    }
    if( active && entries_per_page < static_cast<int>( list.size() ) ) {
        draw_scrollbar( *win, *selection, entries_per_page, list.size(), point( xoffset,
                        0 + borderspace ) );
    }

}

void print_list_scrollable( catacurses::window *win, std::vector<std::string> list, int *selection,
                            bool active, bool border, const report_color_error color_error )
{
    print_list_scrollable( win, std::move( list ), selection, getmaxy( *win ), 0, getmaxx( *win ),
                           active, border,
                           color_error );
}

void print_list_scrollable( catacurses::window *win, const std::string &text, int *selection,
                            int entries_per_page, int xoffset, int width, bool active, bool border,
                            const report_color_error color_error )
{
    int border_space = border ? 1 : 0;
    // -1 on the left for scroll bar and another -1 on the right reserved for cursor
    std::vector<std::string> list = foldstring( text, width - 2 - border_space * 2 );
    print_list_scrollable( win, list, selection, entries_per_page, xoffset, width, active, border,
                           color_error );
}

void print_list_scrollable( catacurses::window *win, const std::string &text, int *selection,
                            bool active,
                            bool border, const report_color_error color_error )
{
    print_list_scrollable( win, text, selection, getmaxy( *win ), 0, getmaxx( *win ), active, border,
                           color_error );
}

void draw_diary_border( catacurses::window *win, const nc_color &color = c_white )
{
    wattron( *win, color );

    const point max( getmaxx( *win ) - 1, getmaxy( *win ) - 1 );
    const int midx = max.x / 2;
    for( int i = 4; i <= max.y - 4; i++ ) {
        mvwprintw( *win, point( 0, i ), "||||" );
        mvwprintw( *win, point( max.x - 3, i ), "||||" );
        mvwprintw( *win, point( midx, i ), " | " );
    }
    for( int i = 4; i <= max.x - 4; i++ ) {
        if( !( i >= midx && i <= midx + 2 ) ) {
            mvwprintw( *win, point( i, 0 ), "____" );
            mvwprintw( *win, point( i, max.y - 2 ), "____" );
            mvwprintw( *win, point( i, max.y - 1 ), "====" );
            mvwprintw( *win, point( i, max.y ), "----" );
        }
    }
    // top left corner
    mvwprintw( *win, point_zero, "    " );
    mvwprintw( *win, point_south, ".-/|" );
    mvwprintw( *win, point( 0, 2 ), "||||" );
    mvwprintw( *win, point( 0, 3 ), "||||" );
    // bottom left corner
    mvwprintw( *win, point( 0, max.y - 3 ), "||||" );
    mvwprintw( *win, point( 0, max.y - 2 ), "||||" );
    mvwprintw( *win, point( 0, max.y - 1 ), "||/=" );
    mvwprintw( *win, point( 0, max.y - 0 ), "`'--" );
    // top right corner
    mvwprintw( *win, point( max.x - 3, 0 ), "    " );
    mvwprintw( *win, point( max.x - 3, 1 ), "|\\-." );
    mvwprintw( *win, point( max.x - 3, 2 ), "||||" );
    mvwprintw( *win, point( max.x - 3, 3 ), "||||" );
    // bottom right corner
    mvwprintw( *win, max + point( -3, -3 ), "||||" );
    mvwprintw( *win, max + point( -3, -2 ), "||||" );
    mvwprintw( *win, max + point( -3, -1 ), "=\\||" );
    mvwprintw( *win, max + point( -3, 0 ), "--''" );
    // mid top
    mvwprintw( *win, point( midx, 0 ), "   " );
    mvwprintw( *win, point( midx, 1 ), "\\ /" );
    mvwprintw( *win, point( midx, 2 ), " | " );
    mvwprintw( *win, point( midx, 3 ), " | " );
    // mid bottom
    mvwprintw( *win, point( midx, max.y - 3 ), " | " );
    mvwprintw( *win, point( midx, max.y - 2 ), " | " );
    mvwprintw( *win, point( midx, max.y - 1 ), "\\|/" );
    mvwprintw( *win, point( midx, max.y - 0 ), "___" );
    wattroff( *win, color );
}
} // namespace

static std::pair<point, point> diary_window_position()
{
    return {
        point( TERMX / 4, TERMY / 4 ),
        point( TERMX / 2, TERMY / 2 )
    };
}

static int uis_padding()
{
    const int padding = TERMX - MAX_DAIRY_UI_WIDTH;
    return ( padding >= 0 ) ? padding / 2 : 0;
}

void diary::show_diary_ui( diary *c_diary )
{
    catacurses::window w_diary;
    catacurses::window w_pages; // pages window, left of diary
    catacurses::window w_text; // right part of diary
    catacurses::window w_changes; // left part of diary
    catacurses::window w_border; // borders of diary
    catacurses::window w_desc; // keybindings window up
    catacurses::window w_info; // bottom window

    enum class window_mode : int { PAGE_WIN = 0, CHANGE_WIN, TEXT_WIN, NUM_WIN, FIRST_WIN = 0, LAST_WIN = NUM_WIN - 1 };
    window_mode currwin = window_mode::PAGE_WIN;


    std::map<window_mode, int> selected = { {window_mode::PAGE_WIN, 0}, {window_mode::CHANGE_WIN, 0}, {window_mode::TEXT_WIN, 0} };


    input_context ctxt( "DIARY" );
    ctxt.register_cardinal();
    ctxt.register_action( "CONFIRM" );
    ctxt.register_action( "QUIT" );
    ctxt.register_action( "NEW_PAGE" );
    ctxt.register_action( "DELETE PAGE" );
    ctxt.register_action( "EXPORT_DIARY" );
    ctxt.register_action( "HELP_KEYBINDINGS" );

    ui_adaptor ui_diary;
    ui_diary.on_screen_resize( [&]( ui_adaptor & ui ) {
        const std::pair<point, point> beg_and_max = diary_window_position();
        point beg = ( uis_padding() != 0 ) ? point( uis_padding() + MAX_DAIRY_UI_WIDTH / 4 - 3,
                    beg_and_max.first.y - 1 ) : beg_and_max.first + point( uis_padding() - 3, -1 );
        point max = point( TERMX - beg.x - 5,
                           beg_and_max.second.y ) - point( uis_padding(), 0 );
        const int midx = max.x / 2;

        w_changes = catacurses::newwin( max.y - 3, midx - 1, beg + point_south );
        w_text = catacurses::newwin( max.y - 3, max.x - midx - 1, beg + point( 2 + midx, 3 ) );
        w_border = catacurses::newwin( max.y + 5, max.x + 9, beg + point( -4, -2 ) );

        ui.position_from_window( w_border );
    } );
    ui_diary.mark_resize();
    ui_diary.on_redraw( [&]( const ui_adaptor & ) {
        werase( w_changes );
        werase( w_text );
        werase( w_border );

        draw_diary_border( &w_border );

        // first get head text, then add the change list to it
        std::vector<std::string> left_diary_text = c_diary->get_head_text();
        std::vector<std::string> change_list = c_diary->get_change_list();
        left_diary_text.insert( left_diary_text.end(), change_list.begin(), change_list.end() );

        print_list_scrollable( &w_changes, left_diary_text, &selected[window_mode::CHANGE_WIN],
                               currwin == window_mode::CHANGE_WIN, false, report_color_error::yes );
        print_list_scrollable( &w_text, c_diary->get_page_text(), &selected[window_mode::TEXT_WIN],
                               currwin == window_mode::TEXT_WIN, false, report_color_error::no );

        wnoutrefresh( w_border );
        wnoutrefresh( w_changes );
        wnoutrefresh( w_text );
    } );

    ui_adaptor ui_pages;
    ui_pages.on_screen_resize( [&]( ui_adaptor & ui ) {
        const std::pair<point, point> beg_and_max = diary_window_position();
        point beg = beg_and_max.first;
        point max = beg_and_max.second;

        w_pages = catacurses::newwin( max.y + 5,
                                      ( uis_padding() != 0 ) ? MAX_DAIRY_UI_WIDTH / 4 - 7 : beg.x - 7, point( uis_padding(),
                                              beg.y - 3 ) );

        ui.position_from_window( w_pages );
    } );
    ui_pages.mark_resize();
    ui_pages.on_redraw( [&]( const ui_adaptor & ) {
        werase( w_pages );

        print_list_scrollable( &w_pages, c_diary->get_pages_list(), &selected[window_mode::PAGE_WIN],
                               currwin == window_mode::PAGE_WIN, true, report_color_error::yes );
        center_print( w_pages, 0, c_light_gray, string_format( _( "pages: %d" ),
                      c_diary->get_pages_list().size() ) );

        wnoutrefresh( w_pages );
    } );

    ui_adaptor ui_desc;
    ui_desc.on_screen_resize( [&]( ui_adaptor & ui ) {
        const std::pair<point, point> beg_and_max = diary_window_position();
        point beg = beg_and_max.first;

        w_desc = catacurses::newwin( 3, TERMX - uis_padding() * 2, point( uis_padding(), beg.y - 6 ) );

        ui.position_from_window( w_desc );
    } );
    ui_desc.mark_resize();
    ui_desc.on_redraw( [&]( const ui_adaptor & ) {
        werase( w_desc );

        draw_border( w_desc );
        center_print( w_desc, 0, c_light_gray, string_format( _( "%s's Diary" ), c_diary->owner ) );
        std::string desc = string_format( _( "%s, %s, %s, %s" ),
                                          ctxt.get_desc( "NEW_PAGE", _( "New page" ), input_context::allow_all_keys ),
                                          ctxt.get_desc( "CONFIRM", _( "Edit text" ), input_context::allow_all_keys ),
                                          ctxt.get_desc( "DELETE PAGE", _( "Delete page" ), input_context::allow_all_keys ),
                                          ctxt.get_desc( "EXPORT_DIARY", _( "Export diary" ), input_context::allow_all_keys )
                                        );
        center_print( w_desc, 1, c_white, desc );

        wnoutrefresh( w_desc );
    } );

    ui_adaptor ui_info;
    ui_info.on_screen_resize( [&]( ui_adaptor & ui ) {
        const std::pair<point, point> beg_and_max = diary_window_position();
        point beg = beg_and_max.first;
        point max = beg_and_max.second;

        w_info = catacurses::newwin( ( TERMY - beg.y - max.y - 2 ) > 7 ? 7 : TERMY - beg.y - max.y - 2,
                                     TERMX - uis_padding() * 2, point( uis_padding(), beg.y + max.y + 2 ) );

        ui.position_from_window( w_info );
    } );
    ui_info.mark_resize();
    ui_info.on_redraw( [&]( const ui_adaptor & ) {
        werase( w_info );

        draw_border( w_info );
        center_print( w_info, 0, c_light_gray, string_format( _( "Info" ) ) );
        if( currwin == window_mode::CHANGE_WIN || currwin == window_mode::TEXT_WIN ) {
            fold_and_print( w_info, point_south_east, getmaxx( w_info ) - 2, c_white, string_format( "%s",
                            c_diary->get_desc_map()[selected[window_mode::CHANGE_WIN]] ) );
        }

        wnoutrefresh( w_info );
    } );

    const auto interaction = game_client::interaction_scope( ctxt, [&]() {
        auto snapshot = game_client::interaction_snapshot{
            .kind = game_client::interaction_kind::choices,
            .title = string_format( _( "%s's Diary" ), c_diary->owner ),
            .message = remove_color_tags( c_diary->get_page_text() ),
            .allow_cancel = true,
        };
        for( const auto &line : c_diary->get_head_text() ) {
            snapshot.message += "\n" + remove_color_tags( line );
        }
        for( const auto &line : c_diary->get_change_list() ) {
            snapshot.message += "\n" + remove_color_tags( line );
        }
        const auto pages = c_diary->get_pages_list();
        for( auto i = std::size_t{ 0 }; i < pages.size(); ++i ) {
            snapshot.choices.push_back( {
                .id = "page:" + std::to_string( i ), .label = remove_color_tags( pages[i] ),
                .highlighted = static_cast<int>( i ) == selected[window_mode::PAGE_WIN],
            } );
        }
        for( const auto *id : { "NEW_PAGE", "CONFIRM", "DELETE PAGE", "EXPORT_DIARY" } ) {
            snapshot.choices.push_back( { .id = std::string( "action:" ) + id,
                                          .label = remove_color_tags( ctxt.get_action_name( id ) ) } );
        }
        return snapshot;
    } );

    while( true ) {

        if( ( !c_diary->pages.empty() &&
              selected[window_mode::PAGE_WIN] >= static_cast<int>( c_diary->pages.size() ) ) ||
            ( c_diary->pages.empty() && selected[window_mode::PAGE_WIN] != 0 ) ) {
            selected[window_mode::PAGE_WIN] = 0;
        }
        selected[window_mode::PAGE_WIN] = c_diary->set_opened_page( selected[window_mode::PAGE_WIN] );
        ui_diary.invalidate_ui();
        ui_pages.invalidate_ui();
        ui_desc.invalidate_ui();
        ui_info.invalidate_ui();
        ui_manager::redraw_invalidated();
        const auto action = game_client::action_of( ctxt,
        ctxt.handle_input(), [&]( const std::string & id ) {
            if( id.starts_with( "page:" ) ) {
                currwin = window_mode::PAGE_WIN;
                selected[window_mode::PAGE_WIN] = std::stoi( id.substr( 5 ) );
            }
            return std::string();
        } );
        if( action == "RIGHT" ) {
            currwin = static_cast<window_mode>( static_cast<int>( currwin ) + 1 );
            if( currwin >= window_mode::NUM_WIN ) {
                currwin = window_mode::FIRST_WIN;
            }
            selected[window_mode::TEXT_WIN] = 0;
        } else if( action == "LEFT" ) {
            currwin = static_cast<window_mode>( static_cast<int>( currwin ) - 1 );
            if( currwin < window_mode::FIRST_WIN ) {
                currwin = window_mode::LAST_WIN;
            }
            selected[window_mode::TEXT_WIN] = 0;
        } else if( action == "DOWN" ) {
            selected[currwin]++;
            if( selected[window_mode::PAGE_WIN] >= static_cast<int>( c_diary->pages.size() ) ) {
                selected[window_mode::PAGE_WIN] = 0;
            }
        } else if( action == "UP" ) {
            selected[currwin]--;
            if( selected[window_mode::PAGE_WIN] < 0 ) {
                selected[window_mode::PAGE_WIN] = c_diary->pages.empty() ? 0 : c_diary->pages.size() - 1;
            }

        } else if( action == "CONFIRM" ) {
            if( !c_diary->pages.empty() ) {
                c_diary->edit_page_ui( [&]() {
                    return w_text;
                } );
            }
        } else if( action == "NEW_PAGE" ) {
            c_diary->new_page();
            selected[window_mode::PAGE_WIN] = c_diary->pages.size() - 1;

        } else if( action == "DELETE PAGE" ) {
            if( !c_diary->pages.empty() ) {
                if( query_yn( _( "Delete this page from the diary?" ) ) ) {
                    c_diary->delete_page();
                    if( selected[window_mode::PAGE_WIN] >= static_cast<int>( c_diary->pages.size() ) ) {
                        selected[window_mode::PAGE_WIN] --;
                    }
                }
            }
        } else if( action == "EXPORT_DIARY" ) {
            if( query_yn( _( "Export the diary as .md?" ) ) ) {
                c_diary->export_to_md();
            }

        } else if( action == "QUIT" ) {
            break;
        }

    }
}

void diary::edit_page_ui( const std::function<catacurses::window()> &create_window )
{
    // modify the stored text, so the new text is displayed after exiting from
    // the editor window, and before confirming or canceling the y/n query.
    std::string &new_text = get_page_ptr()->m_text;
    const std::string old_text = new_text;

    string_editor_window ed( create_window, new_text );

    do {
        const std::pair<bool, std::string> result = ed.query_string();
        new_text = result.second;

        // confirmed or unchanged
        if( result.first || old_text == new_text ) {
            break;
        }

        const bool force_uc = get_option<bool>( "FORCE_CAPITAL_YN" );
        const auto &allow_key = force_uc ? input_context::disallow_lower_case
                                : input_context::allow_all_keys;
        const std::string action = query_popup()
                                   .context( "YESNOQUIT" )
                                   .message( "%s", _( "Save entry?" ) )
                                   .option( "YES", allow_key )
                                   .option( "NO", allow_key )
                                   .allow_cancel( true )
                                   .default_color( c_light_red )
                                   .query()
                                   .action;
        if( action == "YES" ) {
            break;
        } else if( action == "NO" ) {
            new_text = old_text;
            break;
        }
    } while( true );
}
