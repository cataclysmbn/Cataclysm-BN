#include "scores_ui.h"

#include <algorithm>
#include <cassert>
#include <iterator>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "achievement.h"
#include "catalua.h"
#include "color.h"
#include "cursesdef.h"
#include "event_statistics.h"
#include "input.h"
#include "kill_tracker.h"
#include "output.h"
#include "point.h"
#include "stats_tracker.h"
#include "translations.h"
#include "ui.h"
#include "ui_manager.h"

static std::string get_achievements_text( const achievements_tracker &achievements )
{
    ( void )achievements;
    return cata::get_lua_achievements_text();
}

static std::string get_scores_text( stats_tracker &stats )
{
    std::string os;
    std::vector<const score *> valid_scores = stats.valid_scores();
    for( const score *scr : valid_scores ) {
        os += scr->description( stats ) + "\n";
    }
    if( valid_scores.empty() ) {
        os += _( "This game has no valid scores.\n" );
    }
    os += _( "\nNote that only scores that existed when you started this game and still exist now "
             "will appear here." );
    return os;
}

void show_scores_ui( const achievements_tracker &achievements, stats_tracker &stats,
                     const kill_tracker &kills )
{
    catacurses::window w;

    enum class tab_mode {
        achievements,
        scores,
        kills,
        num_tabs,
        first_tab = achievements,
    };

    tab_mode tab = static_cast<tab_mode>( 0 );
    input_context ctxt( "SCORES" );
    ctxt.register_cardinal();
    ctxt.register_action( "PAGE_UP" );
    ctxt.register_action( "PAGE_DOWN" );
    ctxt.register_action( "QUIT" );
    ctxt.register_action( "PREV_TAB" );
    ctxt.register_action( "NEXT_TAB" );
    ctxt.register_action( "HELP_KEYBINDINGS" );

    catacurses::window w_view;
    scrolling_text_view view( w_view );
    bool new_tab = true;

    ui_adaptor ui;
    const auto &init_windows = [&]( ui_adaptor & ui ) {
        w = new_centered_win( TERMY - 2, FULL_SCREEN_WIDTH );
        w_view = catacurses::newwin( getmaxy( w ) - 4, getmaxx( w ) - 1,
                                     point( getbegx( w ), getbegy( w ) + 3 ) );
        ui.position_from_window( w );
    };
    ui.on_screen_resize( init_windows );
    // initialize explicitly here since w_view is used before first redraw
    init_windows( ui );

    const std::vector<std::pair<tab_mode, std::string>> tabs = {
        { tab_mode::achievements, _( "ACHIEVEMENTS" ) },
        { tab_mode::scores, _( "SCORES" ) },
        { tab_mode::kills, _( "KILLS" ) },
    };

    ui.on_redraw( [&]( const ui_adaptor & ) {
        werase( w );
        draw_tabs( w, tabs, tab );
        draw_border_below_tabs( w );
        wnoutrefresh( w );

        view.draw( c_white );
    } );

    while( true ) {
        if( new_tab ) {
            switch( tab ) {
                case tab_mode::achievements:
                    view.set_text( get_achievements_text( achievements ) );
                    break;
                case tab_mode::scores:
                    view.set_text( get_scores_text( stats ) );
                    break;
                case tab_mode::kills:
                    view.set_text( kills.get_kills_text() );
                    break;
                case tab_mode::num_tabs:
                    assert( false );
                    break;
            }
        }

        ui_manager::redraw();
        const std::string action = ctxt.handle_input();
        new_tab = false;
        if( action == "RIGHT" || action == "NEXT_TAB" ) {
            tab = static_cast<tab_mode>( static_cast<int>( tab ) + 1 );
            if( tab >= tab_mode::num_tabs ) {
                tab = tab_mode::first_tab;
            }
            new_tab = true;
        } else if( action == "LEFT" || action == "PREV_TAB" ) {
            tab = static_cast<tab_mode>( static_cast<int>( tab ) - 1 );
            if( tab < tab_mode::first_tab ) {
                tab = static_cast<tab_mode>( static_cast<int>( tab_mode::num_tabs ) - 1 );
            }
            new_tab = true;
        } else if( action == "DOWN" ) {
            view.scroll_down();
        } else if( action == "UP" ) {
            view.scroll_up();
        } else if( action == "PAGE_DOWN" ) {
            view.page_down();
        } else if( action == "PAGE_UP" ) {
            view.page_up();
        } else if( action == "CONFIRM" || action == "QUIT" ) {
            break;
        }
    }
}

void show_kills( kill_tracker &kills )
{
    catacurses::window w;

    input_context ctxt( "SCORES" );
    ctxt.register_cardinal();
    ctxt.register_action( "PAGE_UP" );
    ctxt.register_action( "PAGE_DOWN" );
    ctxt.register_action( "QUIT" );
    ctxt.register_action( "HELP_KEYBINDINGS" );

    catacurses::window w_view;
    scrolling_text_view view( w_view );

    ui_adaptor ui;
    const auto &init_windows = [&]( ui_adaptor & ui ) {
        w = new_centered_win( TERMY - 2, FULL_SCREEN_WIDTH );
        w_view = catacurses::newwin( getmaxy( w ) - 4, getmaxx( w ) - 1,
                                     point( getbegx( w ), getbegy( w ) + 3 ) );
        ui.position_from_window( w );
        view.set_text( kills.get_kills_text() );
    };
    ui.on_screen_resize( init_windows );
    // initialize explicitly here since w_view is used before first redraw
    init_windows( ui );

    ui.on_redraw( [&]( const ui_adaptor & ) {
        werase( w );
        draw_border( w );
        wnoutrefresh( w );
        view.draw( c_white );
    } );

    while( true ) {
        ui_manager::redraw();
        const std::string action = ctxt.handle_input();
        if( action == "DOWN" ) {
            view.scroll_down();
        } else if( action == "UP" ) {
            view.scroll_up();
        } else if( action == "PAGE_DOWN" ) {
            view.page_down();
        } else if( action == "PAGE_UP" ) {
            view.page_up();
        } else if( action == "CONFIRM" || action == "QUIT" ) {
            break;
        }
    }
}
