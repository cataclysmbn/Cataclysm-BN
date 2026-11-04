#include "auto_pickup.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <functional>
#include <memory>
#include <ranges>
#include <string_view>
#include <utility>

#include "avatar.h"
#include "color.h"
#include "cursesdef.h"
#include "debug.h"
#include "engine_client_component.h"
#include "filesystem.h"
#include "fstream_utils.h"
#include "game.h"
#include "input.h"
#include "item.h"
#include "item_factory.h"
#include "itype.h"
#include "json.h"
#include "material.h"
#include "options.h"
#include "output.h"
#include "path_info.h"
#include "point.h"
#include "string_formatter.h"
#include "string_input_popup.h"
#include "string_utils.h"
#include "translations.h"
#include "type_id.h"
#include "ui_manager.h"
#include "world.h"
#include "item_search.h"

using namespace auto_pickup;

bool auto_pickup::test_pattern_function( const itype &type, std::string filter )
{
    auto func = filter_from_string<itype>( filter, wildcard_itype_filter );
    return func( type );
}

bool auto_pickup::autopickup_item_function( const item &object, std::string filter )
{
    auto func = filter_from_string<item>( filter, wildcard_item_filter );
    return func( object );
}

auto_pickup::player_settings &get_auto_pickup()
{
    static auto_pickup::player_settings single_instance;
    return single_instance;
}

namespace
{
namespace ec = engine_client;

/// The components and affordances of the auto pickup manager and of the list of the items a rule matches.
constexpr auto pickup_tabs = "autopickup:tabs";
constexpr auto pickup_rules = "autopickup:rules";
constexpr auto pickup_details = "autopickup:details";
constexpr auto pickup_help = "autopickup:help";
constexpr auto pickup_tab = "autopickup:tab";
constexpr auto pickup_select = "autopickup:select";
constexpr auto pickup_edit = "autopickup:edit";
constexpr auto pickup_mode = "autopickup:mode";
constexpr auto test_matches = "autopickup_test:matches";
constexpr auto test_note = "autopickup_test:note";
constexpr auto test_select = "autopickup_test:select";
/// The actions of the manager that act on the rule under the cursor.
constexpr auto actions_on_a_rule = std::array<std::string_view, 8>
{ "REMOVE_RULE", "COPY_RULE", "SWAP_RULE_GLOBAL_CHAR",
    "ENABLE_RULE", "DISABLE_RULE", "MOVE_RULE_UP", "MOVE_RULE_DOWN", "TEST_RULE"
};

/// A rule's row: the number the session gave it, which no language or position changes.
auto row_id_of( const rule &value ) -> std::string { return "rule:" + std::to_string( value.serial ); }

/// What the text entry of a rule explains about the patterns it takes.
auto pattern_help() -> std::string
{
    return _(
               "* is used as a Wildcard.  A few Examples:\n"
               "\n"
               "wooden arrow    matches the itemname exactly\n"
               "wooden ar*      matches items beginning with wood ar\n"
               "*rrow           matches items ending with rrow\n"
               "*avy fle*fi*arrow     multiple * are allowed\n"
               "heAVY*woOD*arrOW      case insensitive search\n"
               "\n"
               "Pickup using:\n"
               "c:food          matches item in category food\n"
               "m:kevlar        matches items made of Kevlar\n"
               "M:copper        matches items made purely of copper\n"
               "q:drilling      matches items with drilling qualites\n"
               "k:fabrication   matches books that teach fabrication\n"
               "b:c:food;*meat* matches conditions c:food and *meat*" );
}

/// An item a rule matches: the row id its item type gives it, and its name.
struct item_match {
    std::string id;
    std::string name;
};

/// A tab title without the brackets native draws around it to mark its shortcut.
auto without_shortcut_brackets( std::string title ) -> std::string
{
    std::erase_if( title, []( const char c ) { return c == '[' || c == ']' || c == '<' || c == '>'; } );
    return title;
}
} // namespace

void user_interface::show()
{
    if( tabs.empty() ) {
        return;
    }

    const int iHeaderHeight = 4;
    int iContentHeight = 0;
    const int iTotalCols = 2;

    catacurses::window w_border;
    catacurses::window w_header;
    catacurses::window w;

    ui_adaptor ui;

    const auto init_windows = [&]( ui_adaptor & ui ) {
        iContentHeight = FULL_SCREEN_HEIGHT - 2 - iHeaderHeight;
        const point iOffset( TERMX > FULL_SCREEN_WIDTH ? ( TERMX - FULL_SCREEN_WIDTH ) / 2 : 0,
                             TERMY > FULL_SCREEN_HEIGHT ? ( TERMY - FULL_SCREEN_HEIGHT ) / 2 : 0 );

        w_border = catacurses::newwin( FULL_SCREEN_HEIGHT, FULL_SCREEN_WIDTH,
                                       iOffset );
        w_header = catacurses::newwin( iHeaderHeight, FULL_SCREEN_WIDTH - 2,
                                       iOffset + point_south_east );
        w = catacurses::newwin( iContentHeight, FULL_SCREEN_WIDTH - 2,
                                iOffset + point( 1, iHeaderHeight + 1 ) );

        ui.position_from_window( w_border );
    };
    init_windows( ui );
    ui.on_screen_resize( init_windows );

    size_t iTab = 0;
    int iLine = 0;
    int iColumn = 1;
    int iStartPos = 0;

    ui.on_redraw( [&]( const ui_adaptor & ) {
        // Redraw the border
        draw_border( w_border, BORDER_COLOR, title );
        // |-
        mvwputch( w_border, point( 0, 3 ), c_light_gray, LINE_XXXO );
        // -|
        mvwputch( w_border, point( 79, 3 ), c_light_gray, LINE_XOXX );
        // _|_
        mvwputch( w_border, point( 5, FULL_SCREEN_HEIGHT - 1 ), c_light_gray, LINE_XXOX );
        mvwputch( w_border, point( 51, FULL_SCREEN_HEIGHT - 1 ), c_light_gray, LINE_XXOX );
        mvwputch( w_border, point( 61, FULL_SCREEN_HEIGHT - 1 ), c_light_gray, LINE_XXOX );
        wnoutrefresh( w_border );

        // Redraw the header
        int tmpx = 0;
        tmpx += shortcut_print( w_header, point( tmpx, 0 ), c_white, c_light_green, _( "<A>dd" ) ) + 2;
        tmpx += shortcut_print( w_header, point( tmpx, 0 ), c_white, c_light_green, _( "<R>emove" ) ) + 2;
        tmpx += shortcut_print( w_header, point( tmpx, 0 ), c_white, c_light_green, _( "<C>opy" ) ) + 2;
        tmpx += shortcut_print( w_header, point( tmpx, 0 ), c_white, c_light_green, _( "<M>ove" ) ) + 2;
        tmpx += shortcut_print( w_header, point( tmpx, 0 ), c_white, c_light_green, _( "<E>nable" ) ) + 2;
        tmpx += shortcut_print( w_header, point( tmpx, 0 ), c_white, c_light_green, _( "<D>isable" ) ) + 2;
        if( !g->u.name.empty() ) {
            shortcut_print( w_header, point( tmpx, 0 ), c_white, c_light_green, _( "<T>est" ) );
        }
        tmpx = 0;
        tmpx += shortcut_print( w_header, point( tmpx, 1 ), c_white, c_light_green,
                                _( "<+-> Move up/down" ) ) + 2;
        tmpx += shortcut_print( w_header, point( tmpx, 1 ), c_white, c_light_green,
                                _( "<Enter>-Edit" ) ) + 2;
        shortcut_print( w_header, point( tmpx, 1 ), c_white, c_light_green, _( "<Tab>-Switch Page" ) );

        for( int i = 0; i < 78; i++ ) {
            if( i == 4 || i == 50 || i == 60 ) {
                mvwputch( w_header, point( i, 2 ), c_light_gray, LINE_OXXX );
                mvwputch( w_header, point( i, 3 ), c_light_gray, LINE_XOXO );
            } else {
                // Draw line under header
                mvwputch( w_header, point( i, 2 ), c_light_gray, LINE_OXOX );
            }
        }
        mvwprintz( w_header, point( 1, 3 ), c_white, "#" );
        mvwprintz( w_header, point( 8, 3 ), c_white, _( "Rules" ) );
        mvwprintz( w_header, point( 52, 3 ), c_white, _( "I/E" ) );

        rule_list &cur_rules = tabs[iTab].new_rules;
        int locx = 17;
        for( size_t i = 0; i < tabs.size(); i++ ) {
            const auto color = iTab == i ? hilite( c_white ) : c_white;
            locx += shortcut_print( w_header, point( locx, 2 ), c_white, color, tabs[i].title ) + 1;
        }

        locx = 55;
        mvwprintz( w_header, point( locx, 0 ), c_white, _( "Auto pickup enabled:" ) );
        locx += shortcut_print( w_header, point( locx, 1 ),
                                get_option<bool>( "AUTO_PICKUP" ) ? c_light_green : c_light_red, c_white,
                                get_option<bool>( "AUTO_PICKUP" ) ? _( "True" ) : _( "False" ) );
        locx += shortcut_print( w_header, point( locx, 1 ), c_white, c_light_green, "  " );
        locx += shortcut_print( w_header, point( locx, 1 ), c_white, c_light_green, _( "<S>witch" ) );
        shortcut_print( w_header, point( locx, 1 ), c_white, c_light_green, "  " );

        wnoutrefresh( w_header );

        // Clear the lines
        for( int i = 0; i < iContentHeight; i++ ) {
            for( int j = 0; j < 79; j++ ) {
                if( j == 4 || j == 50 || j == 60 ) {
                    mvwputch( w, point( j, i ), c_light_gray, LINE_XOXO );
                } else {
                    mvwputch( w, point( j, i ), c_black, ' ' );
                }
            }
        }

        draw_scrollbar( w_border, iLine, iContentHeight, cur_rules.size(), point( 0, 5 ) );
        wnoutrefresh( w_border );

        calcStartPos( iStartPos, iLine, iContentHeight, cur_rules.size() );

        // display auto pickup
        for( int i = iStartPos; i < static_cast<int>( cur_rules.size() ); i++ ) {
            if( i >= iStartPos &&
                i < iStartPos + ( iContentHeight > static_cast<int>( cur_rules.size() ) ?
                                  static_cast<int>( cur_rules.size() ) : iContentHeight ) ) {
                nc_color cLineColor = cur_rules[i].bActive ? c_white : c_light_gray;

                mvwprintz( w, point( 1, i - iStartPos ), cLineColor, "%d", i + 1 );
                mvwprintz( w, point( 5, i - iStartPos ), cLineColor, "" );

                if( iLine == i ) {
                    wprintz( w, c_yellow, ">> " );
                } else {
                    wprintz( w, c_yellow, "   " );
                }

                wprintz( w, iLine == i && iColumn == 1 ? hilite( cLineColor ) : cLineColor, "%s",
                         cur_rules[i].sRule.empty() ? _( "<empty rule>" ) : cur_rules[i].sRule );

                mvwprintz( w, point( 52, i - iStartPos ), iLine == i && iColumn == 2 ?
                           hilite( cLineColor ) : cLineColor, "%s",
                           cur_rules[i].bExclude ? _( "Exclude" ) :  _( "Include" ) );
            }
        }

        wnoutrefresh( w );
    } );

    bStuffChanged = false;
    input_context ctxt( "AUTO_PICKUP" );
    ctxt.register_cardinal();
    ctxt.register_action( "CONFIRM" );
    ctxt.register_action( "QUIT" );
    if( tabs.size() > 1 ) {
        ctxt.register_action( "NEXT_TAB" );
        ctxt.register_action( "PREV_TAB" );
    }
    ctxt.register_action( "ADD_RULE" );
    ctxt.register_action( "REMOVE_RULE" );
    ctxt.register_action( "COPY_RULE" );
    ctxt.register_action( "ENABLE_RULE" );
    ctxt.register_action( "DISABLE_RULE" );
    ctxt.register_action( "MOVE_RULE_UP" );
    ctxt.register_action( "MOVE_RULE_DOWN" );
    ctxt.register_action( "TEST_RULE" );
    ctxt.register_action( "HELP_KEYBINDINGS" );

    const bool allow_swapping = tabs.size() == 2;
    if( allow_swapping ) {
        ctxt.register_action( "SWAP_RULE_GLOBAL_CHAR" );
    }

    if( is_autopickup ) {
        ctxt.register_action( "SWITCH_AUTO_PICKUP_OPTION" );
    }

    // The tabs, one row per rule, and the help of the rule patterns beside them.
    const auto rules_view = [&]() -> ec::screen_view {
        using namespace std::views;
        const auto &rules = tabs[iTab].new_rules;
        const auto enabled = get_option<bool>( "AUTO_PICKUP" );
        auto rows = rules | enumerate | transform( [&]( const auto & entry )
        {
            const auto &[index, value] = entry;
            return ec::row{
.id = row_id_of( value ), .label = value.sRule.empty() ? std::string{ _( "<empty rule>" ) } :
                value.sRule,
                .selected = index == iLine, .columns = {
                    { .key = "i_e", .label = _( "I/E" ), .value = value.bExclude ? _( "Exclude" ) : _( "Include" ) },
                    { .key = "active", .label = _( "Active" ), .value = value.bActive ? _( "True" ) : _( "False" ) }
                },
                .color = string_from_color( value.bActive ? c_white : c_light_gray ) };
        } ) | std::ranges::to<std::vector>();
        const auto row_arg = ec::arg_shape{ .row = ec::row_shape{ .component_id = pickup_rules } };
        auto items = ec::list( { .id = pickup_rules, .rows = std::move( rows ), .selection = "single", .activate = pickup_edit } );
        items.affordances.push_back( pickup_select );
        items.affordances.push_back( pickup_mode );
        auto details = ec::panel( {
            .id = pickup_details,
            .stats = { {
                    .key = "enabled", .label = _( "Auto pickup enabled:" ), .value = enabled ? _( "True" ) : _( "False" ),
                    .tone = enabled ? "good" : "bad"
                }
            },
            .child = ec::component{ .id = pickup_help, .body = ec::text_body{ .markup = pattern_help() } } } );
        // Without a rule there is nothing to act on; testing a rule also needs a character.
        auto denials = actions_on_a_rule | filter( [&]( const std::string_view id )
        {
            return rules.empty() || ( id == "TEST_RULE" && g->u.name.empty() );
        } ) | transform( []( const std::string_view id )
        {
            return ec::denial{ .id = std::string{ id } };
        } ) | std::ranges::to<std::vector>();
        return {
            .title = title,
            .root = ec::tabs( {
                .id = pickup_tabs,
                .tabs = tabs | enumerate | transform( [&]( const auto & entry )
                {
                    const auto &[index, value] = entry;
                    return ec::tab_entry{ .id = value.id, .label = without_shortcut_brackets( value.title ), .selected = static_cast<size_t>( index ) == iTab };
                } ) | std::ranges::to<std::vector>(),
                .select = pickup_tab,
                .child = ec::split( {
                    .id = "autopickup:body", .relation = "detail", .active = pickup_rules,
                    .children = ec::children_of( std::move( items ), std::move( details ) ) } ) } ),
            .affordances = {
                ec::invoke_affordance( { .id = pickup_select, .label = _( "Select" ), .args = row_arg, .role = "select" } ),
                ec::invoke_affordance( { .id = pickup_edit, .label = _( "Rules" ), .args = row_arg } ),
                ec::invoke_affordance( { .id = pickup_mode, .label = _( "I/E" ), .args = row_arg } ),
                ec::invoke_affordance( { .id = pickup_tab, .label = _( "Select" ), .args = { .row = ec::row_shape{ .component_id = pickup_tabs } } } ),
            },
            .denials = std::move( denials ),
.focus_component = pickup_rules, .focus_row = rules.empty() ? std::string{} :
            row_id_of( rules[iLine] ) };
    };
    const auto screen = ec::screen_scope( ctxt, "auto_pickup", { .role = "screen", .view = [&]( const auto & /*state*/ ) { return rules_view(); } } );

    while( true ) {
        rule_list &cur_rules = tabs[iTab].new_rules;

        const bool currentPageNonEmpty = !cur_rules.empty();

        ui_manager::redraw();

        auto action = std::string{ ctxt.handle_input() };
        // A client's affordance acts on the native cursor, like the keypress that does the same.
        if( const auto invoked = ec::invoked( ctxt.get_raw_input() ) ) {
            const auto &id = invoked->affordance_id;
            const auto &row = invoked->args.row;
            action.clear();
            if( id == pickup_tab ) {
                if( const auto tab_found = row ? std::ranges::find( tabs, *row, &tab::id ) : tabs.end();
                    tab_found != tabs.end() ) {
                    iTab = static_cast<size_t>( tab_found - tabs.begin() );
                    iLine = 0;
                }
            } else if( const auto rule_found = row ? std::ranges::find( cur_rules, *row,
                                               row_id_of ) : cur_rules.end();
                       rule_found != cur_rules.end() ) {
                iLine = static_cast<int>( rule_found - cur_rules.begin() );
                // Editing the rule or its list kind is the confirmation of the column that holds it.
                if( id == pickup_edit || id == pickup_mode ) {
                    iColumn = id == pickup_edit ? 1 : 2;
                    action = "CONFIRM";
                }
            }
        }

        if( action == "NEXT_TAB" ) {
            iTab++;
            if( iTab >= tabs.size() ) {
                iTab = 0;
            }
            iLine = 0;
        } else if( action == "PREV_TAB" ) {
            if( iTab > 0 ) {
                iTab--;
            } else {
                iTab = tabs.size() - 1;
            }
            iLine = 0;
        } else if( action == "QUIT" ) {
            break;
        } else if( action == "DOWN" ) {
            iLine++;
            iColumn = 1;
            if( iLine >= static_cast<int>( cur_rules.size() ) ) {
                iLine = 0;
            }
        } else if( action == "UP" ) {
            iLine--;
            iColumn = 1;
            if( iLine < 0 ) {
                iLine = cur_rules.size() - 1;
            }
        } else if( action == "REMOVE_RULE" && currentPageNonEmpty ) {
            bStuffChanged = true;
            cur_rules.erase( cur_rules.begin() + iLine );
            if( iLine > static_cast<int>( cur_rules.size() ) - 1 ) {
                iLine--;
            }
            if( iLine < 0 ) {
                iLine = 0;
            }
        } else if( action == "COPY_RULE" && currentPageNonEmpty ) {
            bStuffChanged = true;
            cur_rules.push_back( cur_rules[iLine] );
            cur_rules.back().serial = next_rule_serial();
            iLine = cur_rules.size() - 1;
        } else if( allow_swapping && action == "SWAP_RULE_GLOBAL_CHAR" && currentPageNonEmpty ) {
            const size_t other_iTab = ( iTab + 1 ) % 2;
            rule_list &other_rules = tabs[other_iTab].new_rules;
            bStuffChanged = true;
            //copy over
            other_rules.push_back( cur_rules[iLine] );
            other_rules.back().serial = next_rule_serial();
            //remove old
            cur_rules.erase( cur_rules.begin() + iLine );
            iTab = other_iTab;
            iLine = other_rules.size() - 1;
        } else if( action == "ADD_RULE" || ( action == "CONFIRM" && currentPageNonEmpty ) ) {
            const int old_iLine = iLine;
            if( action == "ADD_RULE" ) {
                cur_rules.push_back( rule( "", true, false ) );
                iLine = cur_rules.size() - 1;
            }
            ui_manager::redraw();

            if( iColumn == 1 || action == "ADD_RULE" ) {
                ui_adaptor help_ui;
                catacurses::window w_help;
                const auto init_help_window = [&]( ui_adaptor & help_ui ) {
                    const point iOffset( TERMX > FULL_SCREEN_WIDTH ? ( TERMX - FULL_SCREEN_WIDTH ) / 2 : 0,
                                         TERMY > FULL_SCREEN_HEIGHT ? ( TERMY - FULL_SCREEN_HEIGHT ) / 2 : 0 );
                    w_help = catacurses::newwin( FULL_SCREEN_HEIGHT * ( 2.0 / 3 ) + 2,
                                                 FULL_SCREEN_WIDTH * 3 / 4,
                                                 iOffset + point( 19 / 2, 7 + FULL_SCREEN_HEIGHT / 2 / 2 ) );
                    help_ui.position_from_window( w_help );
                };
                init_help_window( help_ui );
                help_ui.on_screen_resize( init_help_window );

                help_ui.on_redraw( [&]( const ui_adaptor & ) {
                    // NOLINTNEXTLINE(cata-use-named-point-constants)
                    fold_and_print( w_help, point( 1, 1 ), 999, c_white, pattern_help() );

                    draw_border( w_help );
                    wnoutrefresh( w_help );
                } );
                const std::string r = string_input_popup()
                                      .title( _( "Pickup Rule:" ) )
                                      .width( 30 )
                                      .text( cur_rules[iLine].sRule )
                                      .query_string();
                // If r is empty, then either (1) The player ESC'ed from the window (changed their mind), or
                // (2) Explicitly entered an empty rule- which isn't allowed since "*" should be used
                // to include/exclude everything
                if( !r.empty() ) {
                    cur_rules[iLine].sRule = wildcard_trim_rule( r );
                    bStuffChanged = true;
                } else if( action == "ADD_RULE" ) {
                    cur_rules.pop_back();
                    iLine = old_iLine;
                }
            } else if( iColumn == 2 ) {
                bStuffChanged = true;
                cur_rules[iLine].bExclude = !cur_rules[iLine].bExclude;
            }
        } else if( action == "ENABLE_RULE" && currentPageNonEmpty ) {
            bStuffChanged = true;
            cur_rules[iLine].bActive = true;
        } else if( action == "DISABLE_RULE" && currentPageNonEmpty ) {
            bStuffChanged = true;
            cur_rules[iLine].bActive = false;
        } else if( action == "LEFT" ) {
            iColumn--;
            if( iColumn < 1 ) {
                iColumn = iTotalCols;
            }
        } else if( action == "RIGHT" ) {
            iColumn++;
            if( iColumn > iTotalCols ) {
                iColumn = 1;
            }
        } else if( action == "MOVE_RULE_UP" && currentPageNonEmpty ) {
            bStuffChanged = true;
            if( iLine < static_cast<int>( cur_rules.size() ) - 1 ) {
                std::swap( cur_rules[iLine], cur_rules[iLine + 1] );
                iLine++;
                iColumn = 1;
            }
        } else if( action == "MOVE_RULE_DOWN" && currentPageNonEmpty ) {
            bStuffChanged = true;
            if( iLine > 0 ) {
                std::swap( cur_rules[iLine], cur_rules[iLine - 1] );
                iLine--;
                iColumn = 1;
            }
        } else if( action == "TEST_RULE" && currentPageNonEmpty && !g->u.name.empty() ) {
            cur_rules[iLine].test_pattern();
        } else if( action == "SWITCH_AUTO_PICKUP_OPTION" ) {
            // TODO: Now that NPCs use this function, it could be used for them too
            get_options().get_option( "AUTO_PICKUP" ).setNext();
            get_options().save();
        }
    }

    if( !bStuffChanged ) {
        return;
    }

    if( !query_yn( _( "Save changes?" ) ) ) {
        return;
    }

    for( tab &t : tabs ) {
        t.rules.get() = t.new_rules;
    }
}

void player_settings::show()
{
    user_interface ui;

    ui.title = _( " AUTO PICKUP MANAGER " );
    ui.tabs.emplace_back( "global", _( "[<Global>]" ), global_rules );
    if( !g->u.name.empty() ) {
        ui.tabs.emplace_back( "character", _( "[<Character>]" ), character_rules );
    }
    ui.is_autopickup = true;

    ui.show();

    if( !ui.bStuffChanged ) {
        return;
    }

    save_global();
    if( !g->u.name.empty() ) {
        save_character();
    }
    invalidate();
}

void rule::test_pattern() const
{
    std::vector<item_match> vMatchingItems;

    if( sRule.empty() ) {
        return;
    }

    //Loop through all itemfactory items
    //APU now ignores prefixes, bottled items and suffix combinations still not generated
    for( const itype *e : item_controller->all() ) {
        const std::string sItemName = e->nname( 1 );
        if( !test_pattern_function( *e, sRule ) && !wildcard_match( sItemName, sRule ) ) {
            continue;
        }

        vMatchingItems.push_back( { .id = "item:" + e->get_id().str(), .name = sItemName } );
    }

    int iStartPos = 0;
    int iContentHeight = 0;
    int iContentWidth = 0;

    catacurses::window w_test_rule_border;
    catacurses::window w_test_rule_content;

    ui_adaptor ui;

    const auto init_windows = [&]( ui_adaptor & ui ) {
        const point iOffset( 15 + ( TERMX > FULL_SCREEN_WIDTH ? ( TERMX - FULL_SCREEN_WIDTH ) / 2 : 0 ),
                             5 + ( TERMY > FULL_SCREEN_HEIGHT ? ( TERMY - FULL_SCREEN_HEIGHT ) / 2 :
                                   0 ) );
        iContentHeight = FULL_SCREEN_HEIGHT - 8;
        iContentWidth = FULL_SCREEN_WIDTH - 30;

        w_test_rule_border = catacurses::newwin( iContentHeight + 2, iContentWidth,
                             iOffset );
        w_test_rule_content = catacurses::newwin( iContentHeight,
                              iContentWidth - 2,
                              iOffset + point_south_east );

        ui.position_from_window( w_test_rule_border );
    };
    init_windows( ui );
    ui.on_screen_resize( init_windows );

    int nmatch = vMatchingItems.size();
    const std::string buf = string_format( vgettext( "%1$d item matches: %2$s",
                                           "%1$d items match: %2$s",
                                           nmatch ), nmatch, sRule );

    int iLine = 0;

    input_context ctxt( "AUTO_PICKUP_TEST" );
    ctxt.register_updown();
    ctxt.register_action( "QUIT" );
    ctxt.register_action( "HELP_KEYBINDINGS" );

    ui.on_redraw( [&]( const ui_adaptor & ) {
        draw_border( w_test_rule_border, BORDER_COLOR, buf, hilite( c_white ) );
        center_print( w_test_rule_border, iContentHeight + 1, red_background( c_white ),
                      _( "Won't display content or suffix matches" ) );
        wnoutrefresh( w_test_rule_border );

        // Clear the lines
        for( int i = 0; i < iContentHeight; i++ ) {
            for( int j = 0; j < 79; j++ ) {
                mvwputch( w_test_rule_content, point( j, i ), c_black, ' ' );
            }
        }

        calcStartPos( iStartPos, iLine, iContentHeight, vMatchingItems.size() );

        // display auto pickup
        for( int i = iStartPos; i < static_cast<int>( vMatchingItems.size() ); i++ ) {
            if( i >= iStartPos &&
                i < iStartPos + ( iContentHeight > static_cast<int>( vMatchingItems.size() ) ?
                                  static_cast<int>( vMatchingItems.size() ) : iContentHeight ) ) {
                nc_color cLineColor = c_white;

                mvwprintz( w_test_rule_content, point( 0, i - iStartPos ), cLineColor, "%d", i + 1 );
                mvwprintz( w_test_rule_content, point( 4, i - iStartPos ), cLineColor, "" );

                if( iLine == i ) {
                    wprintz( w_test_rule_content, c_yellow, ">> " );
                } else {
                    wprintz( w_test_rule_content, c_yellow, "   " );
                }

                wprintz( w_test_rule_content, iLine == i ? hilite( cLineColor ) : cLineColor,
                         vMatchingItems[i].name );
            }
        }

        wnoutrefresh( w_test_rule_content );
    } );

    // One row per matching item, and under them the note on what the list leaves out.
    const auto matches_view = [&]() -> ec::screen_view {
        using namespace std::views;
        auto rows = vMatchingItems | enumerate | transform( [&]( const auto & entry )
        {
            const auto &[index, match] = entry;
            return ec::row{ .id = match.id, .label = match.name, .selected = index == iLine };
        } ) | std::ranges::to<std::vector>();
        return {
            .title = buf,
            .root = ec::split( {
                .id = "autopickup_test:body", .relation = "detail", .active = test_matches,
                .children = ec::children_of(
                ec::list( { .id = test_matches, .rows = std::move( rows ), .selection = "single" } ),
                ec::component{ .id = test_note, .body = ec::text_body{ .markup = _( "Won't display content or suffix matches" ), .tone = "bad" } } ) } ),
            .affordances = {
                ec::invoke_affordance( { .id = test_select, .label = _( "Select" ), .args = { .row = ec::row_shape{ .component_id = test_matches } }, .role = "select" } ),
            },
.focus_component = test_matches, .focus_row = vMatchingItems.empty() ? std::string{} :
            vMatchingItems[iLine].id };
    };
    const auto screen = ec::screen_scope( ctxt, "auto_pickup_test", { .role = "dialog", .view = [&]( const auto & /*state*/ ) { return matches_view(); } } );

    while( true ) {
        ui_manager::redraw();

        auto action = std::string{ ctxt.handle_input() };
        // A client's affordance moves the cursor to its row, like the keypress that does the same.
        if( const auto invoked = ec::invoked( ctxt.get_raw_input() ) ) {
            const auto found = invoked->args.row ? std::ranges::find( vMatchingItems, *invoked->args.row,
                               &item_match::id ) : vMatchingItems.end();
            action.clear();
            if( found != vMatchingItems.end() ) {
                iLine = static_cast<int>( found - vMatchingItems.begin() );
            }
        }
        if( action == "DOWN" ) {
            iLine++;
            if( iLine >= static_cast<int>( vMatchingItems.size() ) ) {
                iLine = 0;
            }
        } else if( action == "UP" ) {
            iLine--;
            if( iLine < 0 ) {
                iLine = vMatchingItems.size() - 1;
            }
        } else if( action == "QUIT" ) {
            break;
        }
    }
}

bool player_settings::has_rule( const item *it )
{
    const std::string &name = it->tname( 1 );
    for( auto &elem : character_rules ) {
        if( name.length() == elem.sRule.length() && ci_find_substr( name, elem.sRule ) != -1 ) {
            return true;
        }
    }
    return false;
}

void player_settings::add_rule( const item *it )
{
    character_rules.push_back( rule( it->tname( 1, false ), true, false ) );
    create_rule( it );

    if( !get_option<bool>( "AUTO_PICKUP" ) &&
        query_yn( _( "Autopickup is not enabled in the options.  Enable it now?" ) ) ) {
        get_options().get_option( "AUTO_PICKUP" ).setNext();
        get_options().save();
    }
}

void player_settings::remove_rule( const item *it )
{
    const std::string sRule = it->tname( 1, false );
    for( rule_list::iterator candidate = character_rules.begin();
         candidate != character_rules.end(); ++candidate ) {
        if( sRule.length() == candidate->sRule.length() &&
            ci_find_substr( sRule, candidate->sRule ) != -1 ) {
            character_rules.erase( candidate );
            invalidate();
            break;
        }
    }
}

bool player_settings::empty() const
{
    return global_rules.empty() && character_rules.empty();
}

void npc_settings::create_rule( const std::string &to_match )
{
    rules.create_rule( map_items, to_match );
}

void rule_list::create_rule( cache &map_items, const std::string &to_match )
{
    for( const rule &elem : *this ) {
        if( !elem.bActive || !wildcard_match( to_match, elem.sRule ) ) {
            continue;
        }

        map_items[ to_match ] = elem.bExclude ? RULE_BLACKLISTED : RULE_WHITELISTED;
    }
}

void player_settings::create_rule( const item *it )
{
    // TODO: change it to be a reference
    global_rules.create_rule( map_items, *it );
    character_rules.create_rule( map_items, *it );
}

void rule_list::create_rule( cache &map_items, const item &it )
{
    const std::string to_match = it.tname( 1, false );

    for( const rule &elem : *this ) {
        if( !elem.bActive ) {
            continue;
        } else if( !autopickup_item_function( it, elem.sRule ) &&
                   !wildcard_match( to_match, elem.sRule ) ) {
            continue;
        }

        map_items[ to_match ] = elem.bExclude ? RULE_BLACKLISTED : RULE_WHITELISTED;
    }
}

void player_settings::refresh_map_items( cache &map_items ) const
{
    //process include/exclude in order of rules, global first, then character specific
    //if a specific item is being added, all the rules need to be checked now
    //may have some performance issues since exclusion needs to check all items also
    global_rules.refresh_map_items( map_items );
    character_rules.refresh_map_items( map_items );
}

void rule_list::refresh_map_items( cache &map_items ) const
{
    for( const rule &elem : *this ) {
        if( elem.sRule.empty() || !elem.bActive ) {
            continue;
        }

        if( !elem.bExclude ) {
            //Check include patterns against all itemfactory items
            for( const itype *e : item_controller->all() ) {
                const std::string &cur_item = e->nname( 1 );

                if( !test_pattern_function( *e, elem.sRule ) && !wildcard_match( cur_item, elem.sRule ) ) {
                    continue;
                }

                map_items[ cur_item ] = RULE_WHITELISTED;
                map_items.temp_items[ cur_item ] = e;
            }
        } else {
            //only re-exclude items from the existing mapping for now
            //new exclusions will process during pickup attempts
            for( auto &map_item : map_items ) {
                if( !test_pattern_function( *( map_items.temp_items[ map_item.first ] ), elem.sRule ) &&
                    !wildcard_match( map_item.first, elem.sRule ) ) {
                    continue;
                }

                map_items[ map_item.first ] = RULE_BLACKLISTED;
            }
        }
    }
}

rule_state base_settings::check_item( const std::string &sItemName ) const
{
    if( !map_items.ready ) {
        recreate();
    }

    const auto iter = map_items.find( sItemName );
    if( iter != map_items.end() ) {
        return iter->second;
    }

    return RULE_NONE;
}

void player_settings::clear_character_rules()
{
    character_rules.clear();
    invalidate();
}

bool player_settings::save_character()
{
    return save( true );
}

bool player_settings::save_global()
{
    return save( false );
}

bool player_settings::save( const bool bCharacter )
{
    if( bCharacter ) {
        //Character not saved yet.
        if( !g->get_active_world()->player_file_exist( ".sav" ) ) {
            return true;
        }

        return g->get_active_world()->write_to_player_file( ".apu.json", [&]( std::ostream & fout ) {
            JsonOut jout( fout, true );
            ( bCharacter ? character_rules : global_rules ).serialize( jout );
        }, _( "autopickup configuration" ) );
    } else {
        return write_to_file( PATH_INFO::autopickup(), [&]( std::ostream & fout ) {
            JsonOut jout( fout, true );
            ( bCharacter ? character_rules : global_rules ).serialize( jout );
        }, _( "autopickup configuration" ) );
    }
}

void player_settings::load_character()
{
    load( true );
}

void player_settings::load_global()
{
    load( false );
}

void player_settings::load( const bool bCharacter )
{
    if( bCharacter ) {
        g->get_active_world()->read_from_player_file_json( ".apu.json", [&]( JsonIn & jsin ) {
            ( bCharacter ? character_rules : global_rules ).deserialize( jsin );
        }, true );
    } else {
        read_from_file_json( PATH_INFO::autopickup(), [&]( JsonIn & jsin ) {
            ( bCharacter ? character_rules : global_rules ).deserialize( jsin );
        }, true );
    }
    invalidate();
}

void rule::serialize( JsonOut &jsout ) const
{
    jsout.start_object();
    jsout.member( "rule", sRule );
    jsout.member( "active", bActive );
    jsout.member( "exclude", bExclude );
    jsout.end_object();
}

void rule_list::serialize( JsonOut &jsout ) const
{
    jsout.start_array();
    for( const rule &elem : *this ) {
        elem.serialize( jsout );
    }
    jsout.end_array();
}

void rule::deserialize( JsonIn &jsin )
{
    JsonObject jo = jsin.get_object();
    sRule = jo.get_string( "rule" );
    bActive = jo.get_bool( "active" );
    bExclude = jo.get_bool( "exclude" );
}

void rule_list::deserialize( JsonIn &jsin )
{
    clear();

    jsin.start_array();
    while( !jsin.end_array() ) {
        rule tmp;
        tmp.deserialize( jsin );
        push_back( tmp );
    }
}

void npc_settings::show( const std::string &name )
{
    user_interface ui;
    ui.title = string_format( _( "Pickup rules for %s" ), name );
    ui.tabs.emplace_back( "npc", name, rules );
    ui.show();
    // Don't need to save the rules here, it will be save along with the NPC object itself.
    if( !ui.bStuffChanged ) {
        return;
    }
    invalidate();
}

void npc_settings::serialize( JsonOut &jsout ) const
{
    rules.serialize( jsout );
}

void npc_settings::deserialize( JsonIn &jsin )
{
    rules.deserialize( jsin );
}

void npc_settings::refresh_map_items( cache &map_items ) const
{
    rules.refresh_map_items( map_items );
}

bool npc_settings::empty() const
{
    return rules.empty();
}

void base_settings::recreate() const
{
    map_items.clear();
    map_items.temp_items.clear();
    refresh_map_items( map_items );
    map_items.ready = true;
    map_items.temp_items.clear();
}

void base_settings::invalidate()
{
    map_items.ready = false;
}
