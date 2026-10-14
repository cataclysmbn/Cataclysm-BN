#include "game.h" // IWYU pragma: associated

#include <algorithm>
#include <array>
#include <map>
#include <ranges>
#include <string>
#include <vector>

#include "avatar.h"
#include "calendar.h"
#include "color.h"
#include "debug.h"
#include "engine_client_component.h"
#include "engine_client_requirements.h"
#include "input.h"
#include "mission.h"
#include "npc.h"
#include "output.h"
#include "string_formatter.h"
#include "string_utils.h"
#include "translations.h"
#include "ui.h"
#include "ui_manager.h"

namespace
{
/// The description of a mission with the reward counts filled in.
auto format_tokenized_description( const std::string &description,
                                   const std::vector<std::pair<int, itype_id>> &rewards ) -> std::string
{
    auto formatted_description = description;
    for( const auto &reward : rewards ) {
        const auto token = "<reward_count:" + reward.second.str() + ">";
        formatted_description = replace_all( formatted_description, token, string_format( "%d",
                                             reward.first ) );
    }
    return formatted_description;
}

/// How long is left to a mission with a deadline, as the player can tell it.
auto remaining_time( const mission &miss, const avatar &you ) -> std::string
{
    const auto remaining = miss.get_deadline() - calendar::turn;
    return remaining <= 0_turns ? _( "None!" ) : you.has_watch() ? to_string(
               remaining ) : to_string_approx( remaining );
}
} // namespace

void game::list_missions()
{
    catacurses::window w_missions;

    enum class tab_mode : int {
        TAB_ACTIVE = 0,
        TAB_COMPLETED,
        TAB_FAILED,
        NUM_TABS,
        FIRST_TAB = 0,
        LAST_TAB = NUM_TABS - 1
    };
    tab_mode tab = tab_mode::FIRST_TAB;
    size_t selection = 0;
    int entries_per_page = 0;
    input_context ctxt( "MISSIONS" );
    ctxt.register_cardinal();
    ctxt.register_action( "CONFIRM" );
    ctxt.register_action( "QUIT" );
    ctxt.register_action( "HELP_KEYBINDINGS" );

    ui_adaptor ui;
    ui.on_screen_resize( [&]( ui_adaptor & ui ) {
        w_missions = new_centered_win( FULL_SCREEN_HEIGHT, FULL_SCREEN_WIDTH );

        // content ranges from y=3 to FULL_SCREEN_HEIGHT - 2
        entries_per_page = FULL_SCREEN_HEIGHT - 4;

        ui.position_from_window( w_missions );
    } );
    ui.mark_resize();

    std::vector<mission *> umissions;

    ui.on_redraw( [&]( const ui_adaptor & ) {
        werase( w_missions );
        // entries_per_page * page number
        const int top_of_page = entries_per_page * ( selection / entries_per_page );
        const int bottom_of_page =
            std::min( top_of_page + entries_per_page - 1, static_cast<int>( umissions.size() ) - 1 );

        for( int i = 3; i < FULL_SCREEN_HEIGHT - 1; i++ ) {
            mvwputch( w_missions, point( 30, i ), BORDER_COLOR, LINE_XOXO );
        }

        const std::vector<std::pair<tab_mode, std::string>> tabs = {
            { tab_mode::TAB_ACTIVE, _( "ACTIVE MISSIONS" ) },
            { tab_mode::TAB_COMPLETED, _( "COMPLETED MISSIONS" ) },
            { tab_mode::TAB_FAILED, _( "FAILED MISSIONS" ) },
        };
        draw_tabs( w_missions, tabs, tab );
        draw_border_below_tabs( w_missions );

        mvwputch( w_missions, point( 30, 2 ), BORDER_COLOR,
                  tab == tab_mode::TAB_COMPLETED ? ' ' : LINE_OXXX ); // ^|^
        mvwputch( w_missions, point( 30, FULL_SCREEN_HEIGHT - 1 ), BORDER_COLOR, LINE_XXOX ); // _|_

        draw_scrollbar( w_missions, selection, entries_per_page, umissions.size(), point( 0, 3 ) );

        for( int i = top_of_page; i <= bottom_of_page; i++ ) {
            const auto miss = umissions[i];
            const nc_color col = u.get_active_mission() == miss ? c_light_green : c_white;
            const int y = i - top_of_page + 3;
            trim_and_print( w_missions, point( 1, y ), 28,
                            static_cast<int>( selection ) == i ? hilite( col ) : col,
                            miss->name() );
        }

        if( selection < umissions.size() ) {
            const auto miss = umissions[selection];
            const nc_color col = u.get_active_mission() == miss ? c_light_green : c_white;
            std::string for_npc;
            if( miss->get_npc_id().is_valid() ) {
                npc *guy = g->find_npc( miss->get_npc_id() );
                if( guy ) {
                    for_npc = string_format( _( " for %s" ), guy->disp_name() );
                }
            }

            int y = 3;
            y += fold_and_print( w_missions, point( 31, y ), getmaxx( w_missions ) - 33, col,
                                 miss->name() + for_npc );

            y++;
            if( !miss->get_description().empty() ) {
                y += fold_and_print( w_missions, point( 31, y ), getmaxx( w_missions ) - 33, c_white,
                                     format_tokenized_description( miss->get_description(), miss->get_likely_rewards() ) );
            }
            if( miss->has_deadline() ) {
                const time_point deadline = miss->get_deadline();
                mvwprintz( w_missions, point( 31, ++y ), c_white, _( "Deadline: %s" ), to_string( deadline ) );

                if( tab != tab_mode::TAB_COMPLETED ) {
                    // There's no point in displaying this for a completed mission.
                    // @ TODO: But displaying when you completed it would be useful.
                    mvwprintz( w_missions, point( 31, ++y ), c_white, _( "Time remaining: %s" ), remaining_time( *miss,
                               u ) );
                }
            }
            if( miss->has_target() ) {
                const tripoint_abs_omt pos = u.abs_omt_pos();
                // TODO: target does not contain a z-component, targets are assumed to be on z=0
                mvwprintz( w_missions, point( 31, ++y ), c_white, _( "Target: %s   You: %s" ),
                           miss->get_target().to_string(), pos.to_string() );
            }
        } else {
            static const std::map< tab_mode, std::string > nope = {
                { tab_mode::TAB_ACTIVE, translate_marker( "You have no active missions!" ) },
                { tab_mode::TAB_COMPLETED, translate_marker( "You haven't completed any missions!" ) },
                { tab_mode::TAB_FAILED, translate_marker( "You haven't failed any missions!" ) }
            };
            mvwprintz( w_missions, point( 31, 4 ), c_light_red, _( nope.at( tab ) ) );
        }

        wnoutrefresh( w_missions );
    } );

    namespace ec = engine_client;
    constexpr auto tabs_id = "missions:tabs";
    constexpr auto select_tab_id = "missions:tab";
    constexpr auto list_id = "missions:list";
    constexpr auto select_mission_id = "missions:select";
    const auto tab_ids = std::array<std::string, 3> { "active", "completed", "failed" };
    const auto mission_row_id = []( const mission & miss ) { return "mission:" + std::to_string( miss.get_id() ); };
    const auto screen = ec::screen_scope( ctxt, { .view = [&]( const ec::boundary_state & )
    {
        const auto tab_names = std::array<std::string, 3> { _( "ACTIVE MISSIONS" ),
                   _( "COMPLETED MISSIONS" ), _( "FAILED MISSIONS" )
                                                          };
        const auto current = static_cast<std::size_t>( tab );
        auto entries = std::vector<ec::tab_entry> {};
        for( auto i = std::size_t{ 0 }; i < tab_ids.size(); ++i ) {
            entries.push_back( { .id = tab_ids[i], .label = tab_names[i], .selected = i == current } );
        }
        auto content = ec::component {};
        if( selection < umissions.size() ) {
            auto rows = umissions | std::views::transform( [&]( mission * miss ) {
                return ec::row{ .id = mission_row_id( *miss ), .label = miss->name(),
                                .color = string_from_color( u.get_active_mission() == miss ? c_light_green : c_white ) };
            } ) | std::ranges::to<std::vector>();
            auto *const miss = umissions[selection];
            auto title = miss->name();
            if( miss->get_npc_id().is_valid() ) {
                if( const auto *guy = g->find_npc( miss->get_npc_id() ) ) {
                    title += string_format( _( " for %s" ), guy->disp_name() );
                }
            }
            auto stats = std::vector<ec::panel_stat> {};
            const auto add_stat = [&]( const std::string & key, const std::string & format,
            std::string value ) {
                stats.push_back( { .key = key, .label = ec::caption( format.substr( 0, format.find( '%' ) ) ), .value = std::move( value ) } );
            };
            if( miss->has_deadline() ) {
                add_stat( "deadline", _( "Deadline: %s" ), to_string( miss->get_deadline() ) );
                if( tab != tab_mode::TAB_COMPLETED ) {
                    add_stat( "remaining", _( "Time remaining: %s" ), remaining_time( *miss, u ) );
                }
            }
            if( miss->has_target() ) {
                add_stat( "target", _( "Target: %s   You: %s" ), miss->get_target().to_string() );
                stats.push_back( { .key = "you", .label = ec::caption( _( "You" ) ), .value = u.abs_omt_pos().to_string() } );
            }
            auto detail = ec::panel( { .id = "missions:detail", .title = title, .stats = std::move( stats ) } );
            if( !miss->get_description().empty() ) {
                detail.children.push_back( { .id = "missions:description", .body = ec::text_body{
                        .markup = format_tokenized_description( miss->get_description(), miss->get_likely_rewards() ) } } );
            }
            content = ec::split( { .id = "missions:root", .relation = "detail", .children = ec::children_of(
                                       ec::list( { .id = list_id, .rows = std::move( rows ), .selection = "single", .activate = select_mission_id } ),
                                       std::move( detail ) ) } );
        } else {
            const auto nope = std::array<std::string, 3> { _( "You have no active missions!" ),
                       _( "You haven't completed any missions!" ), _( "You haven't failed any missions!" )
                                                         };
            content = { .id = "missions:none", .body = ec::text_body{ .markup = nope[current] } };
        }
        return ec::screen_view{
            .title = _( "Missions" ),
            .root = ec::tabs( { .id = tabs_id, .tabs = std::move( entries ), .select = select_tab_id, .child = std::move( content ) } ),
            .affordances = {
                ec::invoke_affordance( { .id = select_tab_id, .label = _( "Category" ), .args = { .row = ec::row_shape{ .component_id = tabs_id } } } ),
                ec::invoke_affordance( { .id = select_mission_id, .label = ctxt.get_action_name( "CONFIRM" ), .args = { .row = ec::row_shape{ .component_id = list_id } } } ),
            },
            .focus_component = selection < umissions.size() ? list_id : "",
            .focus_row = selection < umissions.size() ? mission_row_id( *umissions[selection] ) : "",
        };
    }
                                                } );

    while( true ) {
        umissions.clear();
        if( tab < tab_mode::FIRST_TAB || tab >= tab_mode::NUM_TABS ) {
            debugmsg( "The sanity check failed because tab=%d", static_cast<int>( tab ) );
            tab = tab_mode::FIRST_TAB;
        }
        switch( tab ) {
            case tab_mode::TAB_ACTIVE:
                umissions = u.get_active_missions();
                break;
            case tab_mode::TAB_COMPLETED:
                umissions = u.get_completed_missions();
                break;
            case tab_mode::TAB_FAILED:
                umissions = u.get_failed_missions();
                break;
            default:
                break;
        }
        if( ( !umissions.empty() && selection >= umissions.size() ) ||
            ( umissions.empty() && selection != 0 ) ) {
            debugmsg( "Sanity check failed: selection=%d, size=%d", static_cast<int>( selection ),
                      static_cast<int>( umissions.size() ) );
            selection = 0;
        }
        ui_manager::redraw();
        auto action = ctxt.handle_input();
        // A client picked a tab or a mission: a mission picked on the active tab is confirmed.
        if( const auto picked = ec::invoked( ctxt.get_raw_input() ); picked && picked->args.row ) {
            if( picked->affordance_id == select_tab_id ) {
                if( const auto found = std::ranges::find( tab_ids, *picked->args.row ); found != tab_ids.end() ) {
                    tab = static_cast<tab_mode>( found - tab_ids.begin() );
                    selection = 0;
                }
            } else if( picked->affordance_id == select_mission_id ) {
                const auto found = std::ranges::find_if( umissions, [&]( const mission * miss ) { return mission_row_id( *miss ) == *picked->args.row; } );
                if( found != umissions.end() ) {
                    selection = found - umissions.begin();
                    action = tab == tab_mode::TAB_ACTIVE ? "CONFIRM" : "";
                }
            }
        }
        if( action == "RIGHT" ) {
            tab = static_cast<tab_mode>( static_cast<int>( tab ) + 1 );
            if( tab >= tab_mode::NUM_TABS ) {
                tab = tab_mode::FIRST_TAB;
            }
            selection = 0;
        } else if( action == "LEFT" ) {
            tab = static_cast<tab_mode>( static_cast<int>( tab ) - 1 );
            if( tab < tab_mode::FIRST_TAB ) {
                tab = tab_mode::LAST_TAB;
            }
            selection = 0;
        } else if( action == "DOWN" ) {
            selection++;
            if( selection >= umissions.size() ) {
                selection = 0;
            }
        } else if( action == "UP" ) {
            if( selection == 0 ) {
                selection = umissions.empty() ? 0 : umissions.size() - 1;
            } else {
                selection--;
            }
        } else if( action == "CONFIRM" ) {
            if( tab == tab_mode::TAB_ACTIVE && selection < umissions.size() ) {
                u.set_active_mission( *umissions[selection] );
            }
            break;
        } else if( action == "QUIT" ) {
            break;
        }
    }
}
