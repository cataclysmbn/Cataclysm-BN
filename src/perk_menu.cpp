#include "color.h"
#include "game_constants.h"
#include "mutation.h"
#include "perk.h"

#include "character.h"
#include "cursesdef.h"
#include "engine_client_component.h"
#include "flag.h"
#include "game.h"
#include "game_inventory.h"
#include "generic_factory.h"
#include "generic_readers.h"
#include "iexamine.h"
#include "input.h"
#include "inventory.h"
#include "item.h"
#include "itype.h"
#include "json.h"
#include "map/mapdata.h"
#include "messages.h"
#include "output.h"
#include "player.h"
#include "player_activity.h"
#include "point.h"
#include "relic.h"
#include "requirements.h"
#include "skill.h"
#include "string_formatter.h"
#include "string_utils.h"
#include "translations.h"
#include "type_id.h"
#include "ui.h"
#include "ui_manager.h"
#include "uistate.h"

#include <algorithm>
#include <numeric>
#include <ranges>
#include <string>
#include <type_traits>
#include <vector>

namespace
{
void draw_cat_tabs( const catacurses::window &w, const std::vector<std::string> &all_tabs,
                    const int tab )
{
    werase( w );

    int width_for_tabs = getmaxx( w );
    draw_tabs( w, all_tabs, all_tabs[tab], width_for_tabs );

    wnoutrefresh( w );
}
} // namespace

void draw_perk_menu( Character &player )
{
    // Define the UI
    const int head_height = 4;
    int current_category = 0;
    int num_categories = 0;

    int ui_width = 0;

    int list_width = 0;
    int list_height = 0;
    int list_position = 0;
    int list_scroll_min = 0;
    int list_scroll_max = 0;
    int list_lines = 0;

    int info_width = 0;
    int info_height = 0;
    int info_position = 0;
    int info_scroll_min = 0;
    int info_scroll_max = 0;
    int info_lines = 0;

    catacurses::window w_head;
    catacurses::window w_list;
    catacurses::window w_info;

    ui_adaptor ui;

    ui.on_screen_resize( [&]( ui_adaptor & ui ) {
        ui_width = TERMX / 3 * 2;
        list_width = ui_width / 2;
        info_width = ui_width / 2;

        const int ui_start_x = ( TERMX - ui_width ) / 2;
        const int ui_start_y = TERMY / 8;

        // -2 is for the border
        list_height = ( TERMY / 4 * 3 ) - head_height - 2;
        info_height = ( TERMY / 4 * 3 ) - head_height - 2;

        w_head = catacurses::newwin( head_height, ui_width, point( ui_start_x, ui_start_y + 1 ) );
        w_list = catacurses::newwin( list_height, list_width, point( ui_start_x,
                                     ui_start_y + head_height ) );
        w_info = catacurses::newwin( info_height, info_width, point( ui_start_x + list_width,
                                     ui_start_y + head_height ) );
        ui.position( point( ui_start_x, ui_start_y ), point( ui_width, TERMY ) );
    } );
    ui.mark_resize();

    // Define all relevant variables
    std::vector<std::string> all_categories;
    std::map<std::string, std::vector<perk_id>> perks_by_category;
    for( const auto &perk : player.get_perks() ) {
        if( perk->is_hidden() ) {
            continue;
        }
        std::string perk_cat = perk->get_category();
        if( perks_by_category.contains( perk_cat ) ) {
            perks_by_category[perk_cat].push_back( perk );
        } else {
            all_categories.push_back( perk_cat );
            perks_by_category[perk_cat] = { perk };
        }
    }
    num_categories = all_categories.size();
    if( num_categories == 0 ) {
        popup( "You have no perks to examine" );
        return;
    }

    ui.on_redraw( [&]( ui_adaptor & ui ) {
        std::vector<perk_id> &cat_perks = perks_by_category[all_categories[current_category]];

        // Borders & Tabs
        draw_cat_tabs( w_head, all_categories, current_category );

        // List of perks
        werase( w_list );
        draw_border( w_list );

        list_lines = cat_perks.size();
        calcStartPos( list_scroll_min, list_position, list_height, list_lines );
        list_scroll_max = std::min( list_lines, list_scroll_min + list_height - 2 );

        for( int i = list_scroll_min; i < list_scroll_max; i++ ) {
            const bool highlight = i == list_position;
            const point print_from( 2, i - list_scroll_min + 1 );
            if( highlight ) {
                ui.set_cursor( w_list, print_from );
            }
            trim_and_print( w_list, print_from, list_width - 4, highlight ? c_white : c_light_gray,
                            cat_perks[i]->get_name() );
        }
        draw_scrollbar( w_list, list_position, list_height, list_lines, point( list_width, 0 ) );

        wnoutrefresh( w_list );

        // Info of currently examined perk

        werase( w_info );
        draw_border( w_info );

        std::string description = replace_colors( cat_perks[list_position]->get_description() );
        std::vector<std::string> description_lines = foldstring( description, info_width - 4 );
        info_lines = description_lines.size();
        calcStartPos( info_scroll_min, info_position, info_height, info_lines );
        info_scroll_max = std::min( info_lines, info_scroll_min + info_height );
        for( int i = info_scroll_min; i < info_scroll_max; i++ ) {
            const point print_from( 2, i - info_scroll_min + 1 );
            trim_and_print( w_info, print_from, info_width - 4, c_white, description_lines[i] );
        }
        draw_scrollbar( w_info, info_position, info_height, info_lines, point( info_width, 0 ) );

        wnoutrefresh( w_info );
    } );

    input_context ctxt( "PERKS" );
    ctxt.register_action( "QUIT" );
    ctxt.register_action( "UP" );
    ctxt.register_action( "DOWN" );
    ctxt.register_action( "PAGE_UP", to_translation( "Scroll Description Up" ) );
    ctxt.register_action( "PAGE_DOWN", to_translation( "Scroll Description Down" ) );
    ctxt.register_action( "PREV_TAB" );
    ctxt.register_action( "NEXT_TAB" );
    ctxt.register_action( "HELP_KEYBINDINGS" );

    namespace ec = engine_client;
    const auto perk_row_id = []( const perk_id & perk ) { return "perk:" + perk.str(); };
    const auto screen = ec::screen_scope( ctxt, "perks", { .role = "screen", .view = [&]( const ec::boundary_state & )
    {
        const auto &cat_perks = perks_by_category[all_categories[current_category]];
        const auto tab_of = [&]( const std::string & category ) { return ec::tab_entry{ .id = category, .label = category, .selected = category == all_categories[current_category] }; };
        const auto row_of = [&]( const perk_id & perk ) { return ec::row{ .id = perk_row_id( perk ), .label = perk->get_name() }; };
        return ec::screen_view{
            .title = _( "Perks" ),
            .root = ec::tabs( {
                .id = "perks:tabs",
                .tabs = all_categories | std::views::transform( tab_of ) | std::ranges::to<std::vector>(),
                .select = "perks:tab",
                .child = ec::split( {
                    .id = "perks",
                    .relation = "detail",
                    .active = "perks:list",
                    .children = ec::children_of(
                    ec::list( { .id = "perks:list", .rows = cat_perks | std::views::transform( row_of ) | std::ranges::to<std::vector>(), .selection = "single", .activate = "perks:pick" } ),
                    ec::panel( { .id = "perks:info", .child = ec::component{ .id = "perks:details", .body = ec::text_body{ .markup = cat_perks[list_position]->get_description() } } } ) ),
                } ),
            } ),
            .affordances = {
                ec::invoke_affordance( { .id = "perks:tab", .label = _( "Category" ), .args = { .row = ec::row_shape{ .component_id = "perks:tabs" } }, .role = "select" } ),
                ec::invoke_affordance( { .id = "perks:pick", .label = _( "Select" ), .args = { .row = ec::row_shape{ .component_id = "perks:list" } }, .role = "select" } ),
            },
            .focus_component = "perks:list",
            .focus_row = perk_row_id( cat_perks[list_position] ),
        };
    }
                                                         } );

    while( true ) {
        ui_manager::redraw();
        const std::string action = ctxt.handle_input();
        if( const auto call = ec::invoked( ctxt.get_raw_input() ); call && call->args.row ) {
            if( call->affordance_id == "perks:tab" ) {
                const auto found = std::ranges::find( all_categories, *call->args.row );
                if( found != all_categories.end() ) {
                    current_category = static_cast<int>( found - all_categories.begin() );
                    list_position = 0;
                    info_position = 0;
                }
            } else {
                const auto &cat_perks = perks_by_category[all_categories[current_category]];
                const auto found = std::ranges::find( cat_perks, *call->args.row, perk_row_id );
                if( found != cat_perks.end() ) {
                    list_position = static_cast<int>( found - cat_perks.begin() );
                    info_position = 0;
                }
            }
            continue;
        }
        if( action == "PAGE_UP" ) {
            info_position -= 1;
            if( info_position < 0 ) { info_position = info_lines - 1; }
        } else if( action == "PAGE_DOWN" ) {
            info_position += 1;
            if( info_position > info_lines ) { info_position = 0; }
        } else if( action == "UP" ) {
            info_position = 0;
            list_position -= 1;
            if( list_position < 0 ) { list_position = list_lines - 1; }
        } else if( action == "DOWN" ) {
            info_position = 0;
            list_position += 1;
            if( list_position > list_lines - 1 ) { list_position = 0; }
        } else if( action == "NEXT_TAB" ) {
            info_position = 0;
            list_position = 0;
            current_category += 1;
            if( current_category > num_categories - 1 ) { current_category = 0; }
        } else if( action == "PREV_TAB" ) {
            info_position = 0;
            list_position = 0;
            current_category -= 1;
            if( current_category < 0 ) { current_category = num_categories - 1; }
        } else if( action == "QUIT" ) {
            return;
        }
    }
}
