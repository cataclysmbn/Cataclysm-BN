#include "mutation_ui.h"

#include <algorithm> //std::min
#include <cstddef>
#include <memory>
#include <unordered_map>

#include "character.h"
#include "enums.h"
#include "input.h"
#include "inventory.h"
#include "mutation.h"
#include "output.h"
#include "popup.h"
#include "string_formatter.h"
#include "string_id.h"
#include "translations.h"
#include "ui_manager.h"
#include "value_ptr.h"

// '!' and '=' are uses as default bindings in the menu
const invlet_wrapper
mutation_chars( "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ\"#&()*+./:;@[\\]^_{|}" );

static void draw_exam_window( const catacurses::window &win, const int border_y )
{
    const int width = getmaxx( win );
    mvwputch( win, point( 0, border_y ), BORDER_COLOR, LINE_XXXO );
    mvwhline( win, point( 1, border_y ), LINE_OXOX, width - 2 );
    mvwputch( win, point( width - 1, border_y ), BORDER_COLOR, LINE_XOXX );
}

const auto shortcut_desc = []( const std::string &comment, const std::string &keys )
{
    return string_format( comment, string_format( "[<color_yellow>%s</color>]", keys ) );
};

enum class mutation_menu_mode {
    activating,
    examining,
    reassigning,
    hiding,
};
enum class mutation_tab_mode {
    active,
    passive,
    none
};
// needs extensive improvement

static trait_id GetTrait( std::vector<trait_id> active, std::vector<trait_id> passive, int cursor,
                          mutation_tab_mode tab_mode )
{
    trait_id mut_id;
    if( tab_mode == mutation_tab_mode::active ) {
        mut_id = active[cursor];
    } else {
        mut_id = passive[cursor];
    }
    return mut_id;
}

static void show_mutations_titlebar( const catacurses::window &window,
                                     const mutation_menu_mode menu_mode, const input_context &ctxt )
{
    werase( window );
    std::string desc;
    if( menu_mode == mutation_menu_mode::reassigning ) {
        desc += std::string( _( "Reassigning." ) ) + "  " +
                _( "Select a mutation to reassign or press [<color_yellow>SPACE</color>] to cancel. " );
    }
    if( menu_mode == mutation_menu_mode::activating ) {
        desc += colorize( _( "Activating" ),
                          c_green ) + "  " + shortcut_desc( _( "%s to examine mutation, " ),
                                  ctxt.get_desc( "TOGGLE_EXAMINE" ) );
    }
    if( menu_mode == mutation_menu_mode::examining ) {
        desc += colorize( _( "Examining" ),
                          c_light_blue ) + "  " + shortcut_desc( _( "%s to activate mutation, " ),
                                  ctxt.get_desc( "TOGGLE_EXAMINE" ) );
    }
    if( menu_mode == mutation_menu_mode::hiding ) {
        desc += colorize( _( "Hiding" ), c_cyan ) + "  " + shortcut_desc( _( "%s to activate mutation, " ),
                ctxt.get_desc( "TOGGLE_EXAMINE" ) );
    }
    if( menu_mode != mutation_menu_mode::reassigning ) {
        desc += shortcut_desc( _( "%s to reassign invlet, " ), ctxt.get_desc( "REASSIGN" ) );
    }
    desc += shortcut_desc( _( "%s to toggle sprite visibility, " ), ctxt.get_desc( "TOGGLE_SPRITE" ) );
    desc += shortcut_desc( _( "%s to change keybindings." ), ctxt.get_desc( "HELP_KEYBINDINGS" ) );
    // NOLINTNEXTLINE(cata-use-named-point-constants)
    fold_and_print( window, point( 1, 0 ), getmaxx( window ) - 1, c_white, desc );
    wnoutrefresh( window );
}

static std::optional<trait_id> trait_by_invlet( const mutation_collection &mutations, int ch )
{
    for( const std::pair<const trait_id, char_trait_data> &mut : mutations ) {
        if( mut.second.key == ch ) {
            return mut.first;
        }
    }
    return std::nullopt;
}

void show_mutations_ui( Character &who )
{
    if( !who.is_avatar() ) {
        // TODO: Implement NPCs activating mutations
        return;
    }

    // Mutation selection UI obscures the terrain,
    // but some mutations may ask to select a tile when activated.
    // As such, we must (de-)activate only after destroying the UI.
    detail::mutations_ui_result res = detail::show_mutations_ui_internal( who );

    switch( res.cmd ) {
        case detail::mutations_ui_cmd::exit:
            break;
        case detail::mutations_ui_cmd::deactivate:
            who.deactivate_mutation( res.mut );
            break;
        case detail::mutations_ui_cmd::activate:
            who.activate_mutation( res.mut );
            break;
    }
}

detail::mutations_ui_result detail::show_mutations_ui_internal( Character &who )
{
    std::vector<trait_id> passive;
    std::vector<trait_id> active;
    for( std::pair<const trait_id, char_trait_data> &mut : who.my_mutations ) {
        if( !mut.first->activated && ! mut.first->transform ) {
            passive.push_back( mut.first );
        } else {
            active.push_back( mut.first );
        }
        // New mutations are initialized with no key at all, so we have to do this here.
        if( mut.second.key == ' ' ) {
            for( const auto &letter : mutation_chars ) {
                if( !trait_by_invlet( who.my_mutations, letter ) ) {
                    mut.second.key = letter;
                    break;
                }
            }
        }
    }

    // maximal number of rows in both columns
    const int mutations_count = std::max( passive.size(), active.size() );

    const int TITLE_HEIGHT = 2;
    const int DESCRIPTION_HEIGHT = 5;
    // + lines with text in titlebar, local
    const int HEADER_LINE_Y = TITLE_HEIGHT + 1;
    const int list_start_y = HEADER_LINE_Y + 2;

    // Main window
    /** Total required height is:
    * top frame line:                                         + 1
    * height of title window:                                 + TITLE_HEIGHT
    * line after the title:                                   + 1
    * line with active/passive mutation captions:               + 1
    * height of the biggest list of active/passive mutations:   + mutations_count
    * line before mutation description:                         + 1
    * height of description window:                           + DESCRIPTION_HEIGHT
    * bottom frame line:                                      + 1
    * TOTAL: TITLE_HEIGHT + mutations_count + DESCRIPTION_HEIGHT + 5
    */

    int HEIGHT = 0;
    int WIDTH = 0;
    catacurses::window wBio;

    int DESCRIPTION_LINE_Y = 0;
    catacurses::window w_description;

    catacurses::window w_title;

    int second_column = 0;

    int scroll_position = 0;
    int cursor = 0;
    int max_scroll_position = 0;
    int list_height = 0;
    int half_list_view_location = 0;
    mutation_menu_mode menu_mode = mutation_menu_mode::activating;
    mutation_tab_mode tab_mode;
    if( !passive.empty() ) {
        tab_mode = mutation_tab_mode::passive;
    } else if( !active.empty() ) {
        tab_mode = mutation_tab_mode::active;
    } else {
        tab_mode = mutation_tab_mode::none;
    }

    const auto recalc_max_scroll_position = [&]() {
        list_height = ( menu_mode == mutation_menu_mode::examining ?
                        DESCRIPTION_LINE_Y : HEIGHT - 1 ) - list_start_y;
        max_scroll_position = mutations_count - list_height;
        half_list_view_location = list_height / 2;
        if( max_scroll_position < 0 ) {
            scroll_position = 0;
        } else if( scroll_position > max_scroll_position ) {
            scroll_position = max_scroll_position;
        }
    };

    ui_adaptor ui;
    ui.on_screen_resize( [&]( ui_adaptor & ui ) {
        HEIGHT = std::min( TERMY, std::max( FULL_SCREEN_HEIGHT,
                                            TITLE_HEIGHT + mutations_count + DESCRIPTION_HEIGHT + 5 ) );
        WIDTH = FULL_SCREEN_WIDTH + ( TERMX - FULL_SCREEN_WIDTH ) / 2;
        const point START( ( TERMX - WIDTH ) / 2, ( TERMY - HEIGHT ) / 2 );
        wBio = catacurses::newwin( HEIGHT, WIDTH, START );

        // Description window @ the bottom of the bionic window
        const int DESCRIPTION_START_Y = START.y + HEIGHT - DESCRIPTION_HEIGHT - 1;
        DESCRIPTION_LINE_Y = DESCRIPTION_START_Y - START.y - 1;
        w_description = catacurses::newwin( DESCRIPTION_HEIGHT, WIDTH - 2,
                                            point( START.x + 1, DESCRIPTION_START_Y ) );

        // Title window
        const int TITLE_START_Y = START.y + 1;
        w_title = catacurses::newwin( TITLE_HEIGHT, WIDTH - 2,
                                      point( START.x + 1, TITLE_START_Y ) );

        recalc_max_scroll_position();

        // X-coordinate of the list of active mutations
        second_column = 32 + ( TERMX - FULL_SCREEN_WIDTH ) / 4;

        ui.position_from_window( wBio );
    } );
    ui.mark_resize();

    input_context ctxt( "MUTATIONS" );
    ctxt.register_updown();
    ctxt.register_action( "ANY_INPUT" );
    ctxt.register_action( "TOGGLE_EXAMINE" );
    ctxt.register_action( "TOGGLE_SPRITE" );
    ctxt.register_action( "REASSIGN" );
    ctxt.register_action( "NEXT_TAB" );
    ctxt.register_action( "PREV_TAB" );
    ctxt.register_action( "CONFIRM" );
    ctxt.register_action( "HELP_KEYBINDINGS" );
    ctxt.register_action( "QUIT" );
#if defined(__ANDROID__)
    for( const auto &p : passive ) {
        ctxt.register_manual_key( who.my_mutations[p].key, p.obj().name() );
    }
    for( const auto &a : active ) {
        ctxt.register_manual_key( who.my_mutations[a].key, a.obj().name() );
    }
#endif

    std::optional<trait_id> examine_id;

    ui.on_redraw( [&]( const ui_adaptor & ) {
        werase( wBio );
        draw_border( wBio, BORDER_COLOR, _( " MUTATIONS " ) );
        // Draw line under title
        mvwhline( wBio, point( 1, HEADER_LINE_Y ), LINE_OXOX, WIDTH - 2 );
        // Draw symbols to connect additional lines to border
        mvwputch( wBio, point( 0, HEADER_LINE_Y ), BORDER_COLOR, LINE_XXXO ); // |-
        mvwputch( wBio, point( WIDTH - 1, HEADER_LINE_Y ), BORDER_COLOR, LINE_XOXX ); // -|

        // Captions
        mvwprintz( wBio, point( 2, HEADER_LINE_Y + 1 ), c_light_blue, _( "Passive:" ) );
        mvwprintz( wBio, point( second_column, HEADER_LINE_Y + 1 ), c_light_blue, _( "Active:" ) );

        if( menu_mode == mutation_menu_mode::examining ) {
            draw_exam_window( wBio, DESCRIPTION_LINE_Y );
        }
        nc_color type;
        if( passive.empty() ) {
            mvwprintz( wBio, point( 2, list_start_y ), c_light_gray, _( "None" ) );
        } else {
            for( int i = scroll_position; static_cast<size_t>( i ) < passive.size(); i++ ) {
                const mutation_branch &md = passive[i].obj();
                const char_trait_data &td = who.my_mutations[passive[i]];
                const bool is_highlighted = cursor == i;
                if( i - scroll_position == list_height ) {
                    break;
                }
                if( is_highlighted && tab_mode == mutation_tab_mode::passive ) {
                    type = who.has_base_trait( passive[i] ) ? c_cyan_yellow : c_light_cyan_yellow;
                } else {
                    type = who.has_base_trait( passive[i] ) ? c_cyan : c_light_cyan;
                }
                mvwprintz( wBio, point( 2, list_start_y + i - scroll_position ),
                           type, "%c %s", td.key, md.name() );
                if( !td.show_sprite ) {
                    //~ Hint: Letter to show which mutation is Hidden in the mutation menu
                    wprintz( wBio, c_cyan, _( " H" ) );
                }
            }
        }

        if( active.empty() ) {
            mvwprintz( wBio, point( second_column, list_start_y ), c_light_gray, _( "None" ) );
        } else {
            for( int i = scroll_position; static_cast<size_t>( i ) < active.size(); i++ ) {
                const mutation_branch &md = active[i].obj();
                const char_trait_data &td = who.my_mutations[active[i]];
                const bool is_highlighted = cursor == i;
                if( i - scroll_position == list_height ) {
                    break;
                }
                if( td.powered ) {
                    if( is_highlighted && tab_mode == mutation_tab_mode::active ) {
                        type = who.has_base_trait( active[i] ) ? c_green_yellow : c_light_green_yellow;
                    } else {
                        type = who.has_base_trait( active[i] ) ? c_green : c_light_green;
                    }
                } else {
                    if( is_highlighted && tab_mode == mutation_tab_mode::active ) {
                        type = who.has_base_trait( active[i] ) ? c_red_yellow : c_light_red_yellow;
                    } else {
                        type = who.has_base_trait( active[i] ) ? c_red : c_light_red;
                    }
                }
                // TODO: track resource(s) used and specify
                mvwputch( wBio, point( second_column, list_start_y + i - scroll_position ),
                          type, td.key );
                std::string mut_desc;
                mut_desc += md.name();
                if( md.cost > 0 && md.cooldown > 0 ) {
                    //~ RU means Resource Units
                    mut_desc += string_format( _( " - %d RU / %d turns" ),
                                               md.cost, md.cooldown );
                } else if( md.cost > 0 ) {
                    //~ RU means Resource Units
                    mut_desc += string_format( _( " - %d RU" ), md.cost );
                } else if( md.cooldown > 0 ) {
                    mut_desc += string_format( _( " - %d turns" ), md.cooldown );
                }
                if( td.powered ) {
                    mut_desc += _( " - Active" );
                }
                mvwprintz( wBio, point( second_column + 2, list_start_y + i - scroll_position ),
                           type, mut_desc );
            }
        }

        draw_scrollbar( wBio, scroll_position, list_height, mutations_count,
                        point( 0, list_start_y ), c_white, true );
        wnoutrefresh( wBio );
        show_mutations_titlebar( w_title, menu_mode, ctxt );

        if( menu_mode == mutation_menu_mode::examining && examine_id.has_value() ) {
            werase( w_description );
            fold_and_print( w_description, point_zero, WIDTH - 2, c_light_blue, examine_id.value()->desc() );
            wnoutrefresh( w_description );
        }
    } );

    mutations_ui_result ret;
    bool exit = false;
    while( !exit ) {
        recalc_max_scroll_position();
        ui_manager::redraw();
        bool handled = false;
        const std::string action = ctxt.handle_input();
        const input_event evt = ctxt.get_raw_input();
        if( evt.type == input_event_t::keyboard && !evt.sequence.empty() ) {
            const int ch = evt.get_first_input();
            if( ch == ' ' ) { //skip if space is pressed (space is used as an empty hotkey)
                continue;
            }
            std::optional<trait_id> mut_id = trait_by_invlet( who.my_mutations, ch );
            if( mut_id ) {
                const mutation_branch &mut_data = mut_id->obj();
                switch( menu_mode ) {
                    case mutation_menu_mode::reassigning: {
                        query_popup pop;
                        pop.message( _( "%s; enter new letter." ),
                                     mutation_branch::get_name( *mut_id ) )
                        .context( "POPUP_WAIT" )
                        .allow_cancel( true )
                        .allow_anykey( true );

                        bool pop_exit = false;
                        while( !pop_exit ) {
                            const query_popup::result ret = pop.query();
                            bool pop_handled = false;
                            if( ret.evt.type == input_event_t::keyboard && !ret.evt.sequence.empty() ) {
                                const int newch = ret.evt.get_first_input();
                                if( mutation_chars.valid( newch ) ) {
                                    const std::optional<trait_id> other_mut_id = trait_by_invlet( who.my_mutations, newch );
                                    if( other_mut_id ) {
                                        std::swap( who.my_mutations[*mut_id].key, who.my_mutations[*other_mut_id].key );
                                    } else {
                                        who.my_mutations[*mut_id].key = newch;
                                    }
                                    pop_exit = true;
                                    pop_handled = true;
                                }
                            }
                            if( !pop_handled ) {
                                if( ret.action == "QUIT" ) {
                                    pop_exit = true;
                                } else if( ret.action != "HELP_KEYBINDINGS" &&
                                           ret.evt.type == input_event_t::keyboard ) {
                                    popup( _( "Invalid mutation letter.  Only those characters are valid:\n\n%s" ),
                                           mutation_chars.get_allowed_chars() );
                                }
                            }
                        }

                        menu_mode = mutation_menu_mode::activating;
                        examine_id = std::nullopt;
                        // TODO: show a message like when reassigning a key to an item?
                        break;
                    }
                    case mutation_menu_mode::activating: {
                        const cata::value_ptr<mut_transform> &trans = mut_data.transform;
                        if( mut_data.activated || trans ) {
                            if( who.my_mutations[*mut_id].powered ) {
                                if( trans && !trans->msg_transform.empty() ) {
                                    who.add_msg_if_player( m_neutral, trans->msg_transform );
                                } else {
                                    who.add_msg_if_player( m_neutral, _( "You stop using your %s." ), mut_data.name() );
                                }
                                ret.cmd = mutations_ui_cmd::deactivate;
                                ret.mut = *mut_id;
                                exit = true;
                            } else if( can_use_mutation_warn( *mut_id, who ) ) {
                                if( trans && !trans->msg_transform.empty() ) {
                                    who.add_msg_if_player( m_neutral, trans->msg_transform );
                                } else {
                                    who.add_msg_if_player( m_neutral, _( "You activate your %s." ), mut_data.name() );
                                }
                                ret.cmd = mutations_ui_cmd::activate;
                                ret.mut = *mut_id;
                                exit = true;
                            } else {
                                popup( _( "You feel like using your %s would kill you!" ),
                                       mut_data.name() );
                            }
                        } else {
                            popup( _( "You cannot activate %s!  To read a description of "
                                      "%s, press '!', then '%c'." ),
                                   mut_data.name(), mut_data.name(), who.my_mutations[*mut_id].key );
                        }
                        break;
                    }
                    case mutation_menu_mode::examining:
                        // Describing mutations, not activating them!
                        examine_id = mut_id;
                        break;
                    case mutation_menu_mode::hiding:
                        who.my_mutations[*mut_id].show_sprite = !who.my_mutations[*mut_id].show_sprite;
                        break;
                }
                handled = true;
            } else if( mutation_chars.valid( ch ) ) {
                handled = true;
            }
        }
        if( !handled ) {

            // Essentially, up-down navigation adapted from the bionics_ui.cpp, with a bunch of extra workarounds to keep functionality

            if( action == "DOWN" ) {

                int lowerlim;

                if( tab_mode == mutation_tab_mode::passive ) {
                    lowerlim = static_cast<int>( passive.size() ) - 1;
                } else if( tab_mode == mutation_tab_mode::active ) {
                    lowerlim = static_cast<int>( active.size() ) - 1;
                } else {
                    continue;
                }

                if( cursor < lowerlim ) {
                    cursor++;
                } else {
                    cursor = 0;
                }
                if( scroll_position < max_scroll_position &&
                    cursor - scroll_position > list_height - half_list_view_location ) {
                    scroll_position++;
                }
                if( scroll_position > 0 && cursor - scroll_position < half_list_view_location ) {
                    scroll_position = std::max( cursor - half_list_view_location, 0 );
                }

                // Draw the description, shabby workaround
                examine_id = GetTrait( active, passive, cursor, tab_mode );

            } else if( action == "UP" ) {

                int lim;
                if( tab_mode == mutation_tab_mode::passive ) {
                    lim = passive.size() - 1;
                } else if( tab_mode == mutation_tab_mode::active ) {
                    lim = active.size() - 1;
                } else {
                    continue;
                }
                if( cursor > 0 ) {
                    cursor--;
                } else {
                    cursor = lim;
                }
                if( scroll_position > 0 && cursor - scroll_position < half_list_view_location ) {
                    scroll_position--;
                }
                if( scroll_position < max_scroll_position &&
                    cursor - scroll_position > list_height - half_list_view_location ) {
                    scroll_position =
                        std::max( std::min<int>( lim + 1 - list_height,
                                                 cursor - half_list_view_location ), 0 );
                }

                examine_id = GetTrait( active, passive, cursor, tab_mode );
            } else if( action == "NEXT_TAB" || action == "PREV_TAB" ) {
                if( tab_mode == mutation_tab_mode::active && !passive.empty() ) {
                    tab_mode = mutation_tab_mode::passive;
                } else if( tab_mode == mutation_tab_mode::passive && !active.empty() ) {
                    tab_mode = mutation_tab_mode::active;
                } else {
                    continue;
                }
                examine_id = GetTrait( active, passive, cursor, tab_mode );
                scroll_position = 0;
                cursor = 0;
            } else if( action == "CONFIRM" ) {
                trait_id mut_id;
                if( tab_mode == mutation_tab_mode::active ) {
                    mut_id = active[cursor];
                } else if( tab_mode == mutation_tab_mode::passive ) {
                    mut_id = passive[cursor];
                } else {
                    continue;
                }
                if( !mut_id.is_null() ) {
                    const mutation_branch &mut_data = mut_id.obj();
                    switch( menu_mode ) {
                        case mutation_menu_mode::reassigning: {
                            query_popup pop;
                            pop.message( _( "%s; enter new letter." ),
                                         mutation_branch::get_name( mut_id ) )
                            .context( "POPUP_WAIT" )
                            .allow_cancel( true )
                            .allow_anykey( true );

                            bool pop_exit = false;
                            while( !pop_exit ) {
                                const query_popup::result ret = pop.query();
                                bool pop_handled = false;
                                if( ret.evt.type == input_event_t::keyboard && !ret.evt.sequence.empty() ) {
                                    const int newch = ret.evt.get_first_input();
                                    if( mutation_chars.valid( newch ) ) {
                                        const std::optional<trait_id> other_mut_id = trait_by_invlet( who.my_mutations, newch );
                                        if( other_mut_id ) {
                                            std::swap( who.my_mutations[mut_id].key, who.my_mutations[*other_mut_id].key );
                                        } else {
                                            who.my_mutations[mut_id].key = newch;
                                        }
                                        pop_exit = true;
                                        pop_handled = true;
                                    } else if( newch == ' ' ) {
                                        who.my_mutations[mut_id].key = newch;
                                        pop_exit = true;
                                        pop_handled = true;
                                    }
                                }
                                if( !pop_handled ) {
                                    if( ret.action == "QUIT" ) {
                                        pop_exit = true;
                                    } else if( ret.action != "HELP_KEYBINDINGS" &&
                                               ret.evt.type == input_event_t::keyboard ) {
                                        popup( _( "Invalid mutation letter.  Only those characters are valid:\n\n%s" ),
                                               mutation_chars.get_allowed_chars() );
                                    }
                                }
                            }

                            menu_mode = mutation_menu_mode::activating;
                            examine_id = std::nullopt;
                            // TODO: show a message like when reassigning a key to an item?
                            break;
                        }
                        case mutation_menu_mode::activating: {
                            if( mut_data.activated ) {
                                if( who.my_mutations[mut_id].powered ) {
                                    who.add_msg_if_player( m_neutral, _( "You stop using your %s." ), mut_data.name() );

                                    who.deactivate_mutation( mut_id );
                                    // Action done, leave screen
                                    exit = true;
                                } else if( ( !mut_data.hunger || who.get_kcal_percent() >= 0.8f ) &&
                                           ( !mut_data.thirst || who.get_thirst() <= 400 ) &&
                                           ( !mut_data.fatigue || who.get_fatigue() <= 400 ) ) {
                                    who.add_msg_if_player( m_neutral, _( "You activate your %s." ), mut_data.name() );

                                    who.activate_mutation( mut_id );
                                    // Action done, leave screen
                                    exit = true;
                                } else {
                                    popup( _( "You don't have enough in you to activate your %s!" ), mut_data.name() );
                                }
                            } else {
                                popup( _( "You cannot activate %s!  To read a description of "
                                          "%s, press '!', then '%c'." ),
                                       mut_data.name(), mut_data.name(), who.my_mutations[mut_id].key );
                            }
                            break;
                        }
                        case mutation_menu_mode::examining:
                            // Describing mutations, not activating them!
                            examine_id = mut_id;
                            break;
                        case mutation_menu_mode::hiding:
                            who.my_mutations[mut_id].show_sprite = !who.my_mutations[mut_id].show_sprite;
                            break;
                    }
                }
            } else if( action == "REASSIGN" ) {
                menu_mode = mutation_menu_mode::reassigning;
                examine_id = std::nullopt;
            } else if( action == "TOGGLE_EXAMINE" ) {
                // switches between activation and examination
                menu_mode = menu_mode == mutation_menu_mode::activating ?
                            mutation_menu_mode::examining : mutation_menu_mode::activating;
                examine_id = std::nullopt;
            } else if( action == "TOGGLE_SPRITE" ) {
                menu_mode = mutation_menu_mode::hiding;
                examine_id = std::nullopt;
            } else if( action == "QUIT" ) {
                ret.cmd = mutations_ui_cmd::exit;
                exit = true;
            }
        }
    }

    return ret;
}
