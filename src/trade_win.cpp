#include "trade_win.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <memory>
#include <optional>
#include <ranges>
#include <string>
#include <unordered_map>
#include <vector>

#include "avatar.h"
#include "catacharset.h"
#include "color.h"
#include "game.h"
#include "input.h"
#include "ime.h"
#include "item.h"
#include "item_category.h"
#include "item_contents.h"
#include "item_search.h"
#include "itype.h"
#include "npc.h"
#include "output.h"
#include "player.h"
#include "point.h"
#include "string_formatter.h"
#include "string_input_popup.h"
#include "string_utils.h"
#include "translations.h"
#include "ui_manager.h"
#include "units_utility.h"

namespace
{
constexpr auto trade_head_height = 4;
constexpr auto trade_info_height = 4;
constexpr auto trade_header_rows = 4;
constexpr auto trade_header_separator_rows = 0;
constexpr auto trade_total_header_rows = trade_header_rows + trade_header_separator_rows;

struct category_range {
    item_category_id id;
    size_t start = 0;
    size_t end = 0;
};

auto build_category_ranges( const std::vector<item_pricing> &list,
                            const std::vector<size_t> &filtered ) -> std::vector<category_range>
{
    auto ranges = std::vector<category_range> {};
    std::ranges::for_each( std::views::iota( size_t{0}, filtered.size() ),
    [&]( size_t idx ) {
        const auto list_index = filtered[idx];
        const auto &ip = list[list_index];
        const auto category_id = ip.locs.front()->get_category().get_id();
        if( ranges.empty() || ranges.back().id != category_id ) {
            if( !ranges.empty() ) {
                ranges.back().end = idx;
            }
            ranges.push_back( category_range{ .id = category_id, .start = idx, .end = idx + 1 } );
        } else {
            ranges.back().end = idx + 1;
        }
    } );
    return ranges;
}

auto register_trade_actions( input_context &ctxt, bool include_any_input ) -> void
{
    ctxt.register_action( "SWITCH_LISTS" );
    ctxt.register_action( "UP" );
    ctxt.register_action( "DOWN" );
    ctxt.register_action( "LEFT" );
    ctxt.register_action( "RIGHT" );
    ctxt.register_action( "FILTER" );
    ctxt.register_action( "RESET_FILTER" );
    ctxt.register_action( "CATEGORY_SELECTION" );
    ctxt.register_action( "PAGE_UP" );
    ctxt.register_action( "PAGE_DOWN" );
    ctxt.register_action( "EXAMINE" );
    ctxt.register_action( "AUTOBALANCE" );
    ctxt.register_action( "TOGGLE_ITEM_INFO" );
    ctxt.register_action( "CONFIRM" );
    ctxt.register_action( "QUIT" );
    ctxt.register_action( "HELP_KEYBINDINGS" );
    if( include_any_input ) {
        ctxt.register_action( "ANY_INPUT" );
    }
}
} // namespace

trading_window::trading_window( npc_trading::trade_state &state ) : state( state )
{
}

trading_window::~trading_window() = default;

auto trading_window::setup_win( ui_adaptor &ui ) -> void
{
    const auto win_they_w = TERMX / 2;
    const auto info_height = show_item_info ? trade_info_height : 0;
    entries_per_page = std::min( TERMY - trade_head_height - info_height - 3 -
                                 trade_total_header_rows,
                                 2 + ( 'z' - 'a' ) + ( 'Z' - 'A' ) );
    w_head = catacurses::newwin( trade_head_height, TERMX, point_zero );
    const auto list_height = TERMY - trade_head_height - info_height;
    w_them = catacurses::newwin( list_height, win_they_w,
                                 point( 0, trade_head_height ) );
    w_you = catacurses::newwin( list_height, TERMX - win_they_w,
                                point( win_they_w, trade_head_height ) );
    w_info = catacurses::newwin( info_height, TERMX,
                                 point( 0, trade_head_height + list_height ) );
    ui.position( point_zero, point( TERMX, TERMY ) );
}

auto trading_window::update_win( npc &np, const std::string &deal ) -> void
{
    // Draw borders, one of which is highlighted
    werase( w_them );
    werase( w_you );

    // Colors for hinting if the trade will be accepted or not.
    const auto trade_color = npc_trading::npc_will_accept_trade( state, np ) ? c_green : c_red;
    struct selection_totals {
        units::volume volume = 0_ml;
        units::mass weight = 0_gram;
    };

    auto ctxt = input_context( "NPC_TRADE" );
    register_trade_actions( ctxt, false );
    ctxt.register_action( "SWITCH_LISTS" );
    ctxt.register_action( "UP" );
    ctxt.register_action( "DOWN" );
    ctxt.register_action( "LEFT" );
    ctxt.register_action( "RIGHT" );
    ctxt.register_action( "FILTER" );
    ctxt.register_action( "RESET_FILTER" );
    ctxt.register_action( "PAGE_UP" );
    ctxt.register_action( "PAGE_DOWN" );
    ctxt.register_action( "EXAMINE" );
    ctxt.register_action( "CONFIRM" );
    ctxt.register_action( "QUIT" );
    ctxt.register_action( "HELP_KEYBINDINGS" );

    werase( w_head );
    draw_border( w_head );
    const auto head_inner_w = getmaxx( w_head ) - 2;
    const auto head_title_y = 1;
    const auto head_keybinds_label_prefix = _( "< [" );
    const auto head_keybinds_label_middle = _( "] keybindings >" );
    const auto head_keybinds_label_width = utf8_width( head_keybinds_label_prefix ) +
                                           1 + utf8_width( head_keybinds_label_middle );
    const auto title_label = [&deal]() -> std::string {
        if( deal == _( "Pay:" ) )
        {
            return _( "Paying" );
        }
        if( deal == _( "Reward" ) )
        {
            return _( "Accepting a reward from" );
        }
        return _( "Trading with" );
    }();
    mvwprintz( w_head, point( 1, head_title_y ), c_white, title_label );
    mvwprintz( w_head, point( 1 + utf8_width( title_label ) + 1, head_title_y ),
               c_light_green, np.disp_name() );
    const auto examine_key = to_lower_case( ctxt.get_desc( "EXAMINE", 1 ) );
    const auto switch_key = ctxt.get_desc( "SWITCH_LISTS", 1 );
    const auto confirm_key = ctxt.get_desc( "CONFIRM", 1 );
    const auto autobalance_key = ctxt.get_desc( "AUTOBALANCE", 1 );
    const auto confirm_label = _( "confirm trade" );
    const auto autobalance_label = _( "autobalance" );
    const auto examine_label = _( "examine item" );
    const auto switch_label = _( "switch panes" );
    const auto category_state_on = _( "ON" );
    const auto category_state_off = _( "OFF" );
    const auto state_on_color = category_mode ? c_light_green : c_dark_gray;
    const auto state_off_color = category_mode ? c_dark_gray : c_light_green;
    const auto state_sep = _( "|" );
    const auto hint_sep = "  ";
    const auto hint_width = [&]( const std::string & key, const std::string & label ) -> int {
        return 2 + utf8_width( key ) + 2 + utf8_width( label );
    };
    const auto col0_w = std::max( hint_width( examine_key, examine_label ),
                                  hint_width( confirm_key, confirm_label ) );
    const auto col1_w = std::max( hint_width( switch_key, switch_label ),
                                  hint_width( autobalance_key, autobalance_label ) );
    const auto grid_width = col0_w + utf8_width( hint_sep ) + col1_w;
    const auto grid_x = 1 + std::max( head_inner_w - grid_width, 0 );
    const auto hint_y1 = head_title_y;
    const auto hint_y2 = head_title_y + 1;
    const auto draw_hint = [&]( int x, int y, const std::string & key,
    const std::string & label ) -> void {
        mvwprintz( w_head, point( x, y ), c_light_gray, "[" );
        x += 1;
        mvwprintz( w_head, point( x, y ), c_light_gray, key );
        x += utf8_width( key );
        mvwprintz( w_head, point( x, y ), c_light_gray, "] " );
        x += 2;
        mvwprintz( w_head, point( x, y ), c_light_gray, label );
    };
    draw_hint( grid_x, hint_y1, examine_key, examine_label );
    draw_hint( grid_x + col0_w + utf8_width( hint_sep ), hint_y1, switch_key, switch_label );
    draw_hint( grid_x, hint_y2, confirm_key, confirm_label );
    draw_hint( grid_x + col0_w + utf8_width( hint_sep ), hint_y2, autobalance_key,
               autobalance_label );

    auto cost_str = std::string{ _( "Exchange" ) };
    if( !np.will_exchange_items_freely() ) {
        cost_str = string_format( state.your_balance >= 0 ? _( "Credit %s" ) : _( "Debt %s" ),
                                  format_money( std::abs( state.your_balance ) ) );
    }

    const auto head_bottom_y = getmaxy( w_head ) - 1;
    const auto cost_tag = string_format( "< %s >", cost_str );
    const auto cost_w = utf8_width( cost_tag );
    const auto cost_x = 1 + ( head_inner_w - cost_w ) / 2;
    mvwprintz( w_head, point( cost_x, head_bottom_y ), trade_color, cost_tag );

    const auto category_key = ctxt.get_desc( "CATEGORY_SELECTION", 1 );
    const auto category_label_prefix = _( "< [" );
    const auto category_label_middle = _( "] category select " );
    const auto category_label_suffix = _( " >" );
    const auto category_label_width = utf8_width( category_label_prefix ) +
                                      utf8_width( category_key ) +
                                      utf8_width( category_label_middle ) +
                                      1 + utf8_width( category_state_on ) +
                                      utf8_width( state_sep ) +
                                      utf8_width( category_state_off ) + 1 +
                                      utf8_width( category_label_suffix );
    auto category_x = 1 + std::max( head_inner_w - category_label_width, 0 );
    mvwprintz( w_head, point( category_x, head_bottom_y ), c_white, category_label_prefix );
    category_x += utf8_width( category_label_prefix );
    mvwprintz( w_head, point( category_x, head_bottom_y ), c_yellow, category_key );
    category_x += utf8_width( category_key );
    mvwprintz( w_head, point( category_x, head_bottom_y ), c_white, category_label_middle );
    category_x += utf8_width( category_label_middle );
    mvwprintz( w_head, point( category_x, head_bottom_y ), c_white, "[" );
    category_x += 1;
    mvwprintz( w_head, point( category_x, head_bottom_y ), state_on_color, category_state_on );
    category_x += utf8_width( category_state_on );
    mvwprintz( w_head, point( category_x, head_bottom_y ), c_white, state_sep );
    category_x += utf8_width( state_sep );
    mvwprintz( w_head, point( category_x, head_bottom_y ), state_off_color, category_state_off );
    category_x += utf8_width( category_state_off );
    mvwprintz( w_head, point( category_x, head_bottom_y ), c_white, "]" );
    category_x += 1;
    mvwprintz( w_head, point( category_x, head_bottom_y ), c_white, category_label_suffix );

    auto keybinds_x = 1 + head_inner_w - head_keybinds_label_width;
    mvwprintz( w_head, point( keybinds_x, 0 ), c_white, head_keybinds_label_prefix );
    keybinds_x += utf8_width( head_keybinds_label_prefix );
    mvwprintz( w_head, point( keybinds_x, 0 ), c_yellow, "?" );
    keybinds_x += 1;
    mvwprintz( w_head, point( keybinds_x, 0 ), c_white, head_keybinds_label_middle );
    draw_border( w_them, ( focus_them ? c_yellow : BORDER_COLOR ) );
    draw_border( w_you, ( !focus_them ? c_yellow : BORDER_COLOR ) );
    draw_border( w_info, BORDER_COLOR );

    mvwprintz( w_them, point( 2, 1 ), c_white, _( "Inventory:" ) );
    mvwprintz( w_them, point( 2 + utf8_width( _( "Inventory:" ) ) + 1, 1 ), c_light_green,
               np.name );
    mvwprintz( w_you,  point( 2, 1 ), c_white, _( "Inventory:" ) );
    mvwprintz( w_you,  point( 2 + utf8_width( _( "Inventory:" ) ) + 1, 1 ), c_light_green,
               _( "You" ) );

    const auto selected_amount = []( const item_pricing & ip, bool is_theirs ) -> int {
        if( ip.charges > 0 )
        {
            return is_theirs ? ip.u_charges : ip.npc_charges;
        }
        return is_theirs ? ip.u_has : ip.npc_has;
    };
    const auto sum_selected = [&]( const std::vector<item_pricing> &list,
    bool is_theirs ) -> selection_totals {
        return std::ranges::fold_left( list | std::views::transform( [&]( const item_pricing & ip )
        {
            const auto amount = selected_amount( ip, is_theirs );
            return selection_totals{ .volume = ip.vol * amount, .weight = ip.weight * amount };
        } ), selection_totals{},
        []( const selection_totals & acc, const selection_totals & value ) -> selection_totals {
            return selection_totals{
                .volume = acc.volume + value.volume,
                .weight = acc.weight + value.weight };
        } );
    };
    const auto your_selected = sum_selected( state.yours, false );
    const auto their_selected = sum_selected( state.theirs, true );
    const auto player_free_volume = g->u.volume_capacity() - g->u.volume_carried() +
                                    your_selected.volume - their_selected.volume;
    const auto player_free_weight = g->u.weight_capacity() - g->u.weight_carried() +
                                    your_selected.weight - their_selected.weight;

    them_filtered = build_filtered_indices( state.theirs, them_filter );
    you_filtered = build_filtered_indices( state.yours, you_filter );

    const auto show_filter_help = filter_edit;
    const auto help_on_theirs = !filter_edit_theirs;
    // Draw lists of items, starting from offset
    for( size_t whose = 0; whose <= 1; whose++ ) {
        const auto they = whose == 0;
        const auto &list = they ? state.theirs : state.yours;
        const auto &filtered = they ? them_filtered : you_filtered;
        const auto &offset = they ? them_off : you_off;
        const auto &person = they ? static_cast<player &>( np ) :
                             static_cast<player &>( g->u );
        auto &w_whose = they ? w_them : w_you;
        auto win_w = getmaxx( w_whose );
        // Borders
        win_w -= 2;
        const auto end = std::min( filtered.size(), offset + entries_per_page );
        const auto visible = std::views::iota( offset, end );
        const auto max_width = [&]( auto make_width ) -> int {
            return std::ranges::fold_left( visible | std::views::transform( make_width ), 0,
            []( int acc, int value ) { return std::max( acc, value ); } );
        };
        const auto qty_label = _( "amt" );
        const auto weight_label = _( "weight" );
        const auto vol_label = _( "vol" );
        const auto price_label = _( "unit price" );
        auto qty_w = max_width( [&]( size_t idx ) -> int {
            const auto &ip = list[filtered[idx]];
            const auto available_amount = ip.charges > 0 ? ip.charges : ip.count;
            return available_amount > 1 ? utf8_width( string_format( "%d", available_amount ) ) : 0;
        } );
        auto weight_w = max_width( [&]( size_t idx ) -> int {
            const auto &ip = list[filtered[idx]];
            const auto available_amount = ip.charges > 0 ? ip.charges : std::max( ip.count, 1 );
            const auto weight_str = string_format( "%.2f",
                                                   convert_weight( ip.weight * available_amount ) );
            return utf8_width( weight_str );
        } );
        auto vol_w = max_width( [&]( size_t idx ) -> int {
            const auto &ip = list[filtered[idx]];
            const auto available_amount = ip.charges > 0 ? ip.charges : std::max( ip.count, 1 );
            const auto vol_str = string_format(
                "%.2f",
                convert_volume( to_milliliter( ip.vol * available_amount ) ) );
            return utf8_width( vol_str );
        } );
        auto price_w = max_width( [&]( size_t idx ) -> int {
            const auto &ip = list[filtered[idx]];
            return utf8_width( format_money( ip.price ) );
        } );
        qty_w = std::max( qty_w, utf8_width( qty_label ) );
        vol_w = std::max( vol_w, utf8_width( vol_label ) );
        weight_w = std::max( weight_w, utf8_width( weight_label ) );
        price_w = std::max( price_w, utf8_width( price_label ) );
        const auto align_left = [&]( const std::string & text, int width ) -> std::string {
            const auto pad = std::max( width - utf8_width( text ), 0 );
            return text + std::string( pad, ' ' );
        };
        const auto align_right = [&]( const std::string & text, int width ) -> std::string {
            const auto pad = std::max( width - utf8_width( text ), 0 );
            return std::string( pad, ' ' ) + text;
        };
        const auto price_x = win_w - price_w;
        const auto vol_x = price_x - 1 - vol_w;
        const auto weight_x = vol_x - 1 - weight_w;
        const auto qty_x = weight_x - 1 - qty_w;
        const auto name_indent = 2;
        const auto name_x = 1 + name_indent;
        const auto name_w = std::max( qty_x - 2 - name_indent, 1 );
        const auto item_hotkeys = ctxt.get_available_single_char_hotkeys(
                                      "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ" );
        const auto stats_y = 2;
        const auto separator_y = 3;
        const auto header_y = 4;
        const auto header_color = c_light_gray;
        const auto pane_free_volume = they ? state.volume_left : player_free_volume;
        const auto pane_free_weight = they ? state.weight_left : player_free_weight;
        const auto pane_max_volume = they ? np.volume_capacity() : g->u.volume_capacity();
        const auto pane_max_weight = they ? np.weight_capacity() : g->u.weight_capacity();
        const auto pane_used_volume = pane_max_volume - pane_free_volume;
        const auto pane_used_weight = pane_max_weight - pane_free_weight;
        const auto weight_used_str = string_format( "%.2f",
                                     convert_weight( pane_used_weight ) );
        const auto weight_max_str = string_format( "%.2f",
                                    convert_weight( pane_max_weight ) );
        const auto weight_str = string_format( _( "/%s %s" ), weight_max_str, weight_units() );
        const auto vol_used_str = string_format( "%.2f",
                                  convert_volume( to_milliliter( pane_used_volume ) ) );
        const auto vol_max_str = string_format( "%.2f",
                                                convert_volume( to_milliliter( pane_max_volume ) ) );
        const auto vol_str = string_format( _( "/%s %s" ), vol_max_str, volume_units_abbr() );
        const auto weight_color = pane_used_weight > pane_max_weight ? c_light_red : c_light_green;
        const auto vol_color = pane_used_volume > pane_max_volume ? c_light_red : c_light_green;
        mvwprintz( w_whose, point( 1, stats_y ), header_color, std::string( win_w, ' ' ) );
        const auto stats_width = utf8_width( weight_used_str ) +
                                 utf8_width( weight_str ) + 2 +
                                 utf8_width( vol_used_str ) +
                                 utf8_width( vol_str );
        auto x = std::max( win_w - stats_width, 1 );
        mvwprintz( w_whose, point( x, stats_y ), weight_color, weight_used_str );
        x += utf8_width( weight_used_str );
        mvwprintz( w_whose, point( x, stats_y ), header_color, weight_str );
        x += utf8_width( weight_str ) + 2;
        mvwprintz( w_whose, point( x, stats_y ), vol_color, vol_used_str );
        x += utf8_width( vol_used_str );
        mvwprintz( w_whose, point( x, stats_y ), header_color, vol_str );
        x += utf8_width( vol_str ) + 2;
        mvwhline( w_whose, point( 1, separator_y ), LINE_OXOX, win_w );
        mvwprintz( w_whose, point( name_x + 3, header_y ), header_color,
                   trim_by_length( _( "Name (charges)" ), name_w ) );
        mvwprintz( w_whose, point( qty_x, header_y ), header_color,
                   align_left( qty_label, qty_w ) );
        mvwprintz( w_whose, point( weight_x, header_y ), header_color,
                   align_left( weight_label, weight_w ) );
        mvwprintz( w_whose, point( vol_x, header_y ), header_color,
                   align_left( vol_label, vol_w ) );
        mvwprintz( w_whose, point( price_x, header_y ), header_color,
                   align_left( price_label, price_w ) );
        const auto filter_prefix = _( "< [" );
        const auto filter_middle = _( "] filter" );
        const auto filter_suffix = _( " >" );
        const auto filter_input_sep = _( ": " );
        const auto filter_label_width = utf8_width( filter_prefix ) + 1 +
                                        utf8_width( filter_middle ) +
                                        utf8_width( filter_suffix );
        const auto filter_y = getmaxy( w_whose ) - 1;
        auto filter_x = 1;
        mvwprintz( w_whose, point( filter_x, filter_y ), c_white, filter_prefix );
        filter_x += utf8_width( filter_prefix );
        mvwprintz( w_whose, point( filter_x, filter_y ), c_yellow, "/" );
        filter_x += 1;
        mvwprintz( w_whose, point( filter_x, filter_y ), c_white, filter_middle );
        filter_x += utf8_width( filter_middle );
        const auto is_editing_here = filter_edit && ( filter_edit_theirs == they );
        if( is_editing_here || !( they ? them_filter : you_filter ).empty() ) {
            const auto filter_label_free = std::max( win_w - filter_label_width -
                                           utf8_width( filter_input_sep ), 0 );
            const auto &pane_filter = they ? them_filter : you_filter;
            const auto &active_text = is_editing_here && filter_popup ? filter_popup->text() :
                                      pane_filter;
            const auto filter_text = trim_by_length( active_text, filter_label_free );
            mvwprintz( w_whose, point( filter_x, filter_y ), c_white, filter_input_sep );
            filter_x += utf8_width( filter_input_sep );
            const auto filter_color = is_editing_here ? c_white : c_magenta;
            mvwprintz( w_whose, point( filter_x, filter_y ), filter_color, filter_text );
            filter_x += utf8_width( filter_text );
        }
        mvwprintz( w_whose, point( filter_x, filter_y ), c_white, filter_suffix );
        const auto draw_filter_help = show_filter_help && ( they == help_on_theirs );
        if( draw_filter_help ) {
            const auto help_start_y = 1;
            const auto help_height = std::max( getmaxy( w_whose ) - 2, 0 );
            if( help_height > 0 ) {
                const auto clear_width = std::string( win_w, ' ' );
                std::ranges::for_each( std::views::iota( help_start_y,
                help_start_y + help_height ), [&]( int y ) {
                    mvwprintz( w_whose, point( 1, y ), c_white, clear_width );
                } );
                draw_item_filter_rules( w_whose, help_start_y, help_height,
                                        item_filter_type::FILTER );
            }
            continue;
        }
        auto last_category = std::optional<item_category_id> {};
        const auto is_focused_pane = ( they && focus_them ) || ( !they && !focus_them );
        const auto category_ranges = build_category_ranges( list, filtered );
        auto active_category_id = std::optional<item_category_id> {};
        if( category_mode && is_focused_pane && !category_ranges.empty() ) {
            const auto &category_cursor = they ? them_category_cursor : you_category_cursor;
            if( category_cursor < category_ranges.size() ) {
                active_category_id = category_ranges[category_cursor].id;
            }
        }
        auto row = size_t{0};
        for( size_t i = offset; i < filtered.size() && row < entries_per_page; i++ ) {
            const auto list_index = filtered[i];
            const auto &ip = list[list_index];
            const auto *it = ip.locs.front();
            const auto category_id = it->get_category().get_id();
            if( !last_category || *last_category != category_id ) {
                const auto category_label = to_upper_case( it->get_category().name() );
                const auto category_y = static_cast<int>( row + 1 + trade_total_header_rows );
                mvwprintz( w_whose, point( 2, category_y ), c_magenta,
                           trim_by_length( category_label, win_w - 1 ) );
                row++;
                if( row >= entries_per_page ) {
                    break;
                }
            }
            auto color = it == &person.primary_weapon() ? c_yellow : c_light_gray;
            const auto is_cursor = ( they && focus_them && i == them_cursor ) ||
                                   ( !they && !focus_them && i == you_cursor );
            const auto row_y = static_cast<int>( row + 1 + trade_total_header_rows );
            const auto &owner_sells = they ? ip.u_has : ip.npc_has;
            const auto &owner_sells_charge = they ? ip.u_charges : ip.npc_charges;
            auto itname = it->display_name();

            if( np.will_exchange_items_freely() &&
                ip.locs.front()->where() != item_location_type::character ) {
                itname = itname + " (" + ip.locs.front()->describe_location( &g->u ) + ")";
                color = c_light_blue;
            }

            if( ip.selected ) {
                color = c_white;
            }
            const auto is_category_selected = active_category_id &&
                                              *active_category_id == category_id;
            const auto should_hilite = is_cursor || is_category_selected;
            auto line_color = should_hilite ? hilite( c_white ) : color;
            if( should_hilite ) {
                const auto fill = std::string( win_w, ' ' );
                mvwprintz( w_whose, point( 1, row_y ), line_color, fill );
            }

            const auto hotkey_index = i - offset;
            const auto keychar = hotkey_index < item_hotkeys.size() ?
                                 item_hotkeys[hotkey_index] : ' ';
            const auto total_amount = ip.charges > 0 ? ip.charges : std::max( ip.count, 1 );
            const auto selected_amount = ip.charges > 0 ? owner_sells_charge : owner_sells;
            auto selection_mark = '-';
            if( selected_amount >= total_amount && total_amount > 0 ) {
                selection_mark = '+';
            } else if( selected_amount > 0 ) {
                selection_mark = '#';
            }
            trim_and_print( w_whose, point( name_x, row_y ), name_w, line_color, "%c %c %s",
                            keychar, selection_mark, itname );
#if defined(__ANDROID__)
            if( keychar != ' ' ) {
                ctxt.register_manual_key( keychar, itname );
            }
#endif

            auto price_str = format_money( ip.price );
            const auto available_amount = ip.charges > 0 ? ip.charges : ip.count;
            const auto qty_str = available_amount > 1 ? string_format( "%d", available_amount ) :
                                 std::string{};
            const auto weight_str = string_format(
                                        "%.2f",
                                        convert_weight( ip.weight * available_amount ) );
            const auto vol_str = string_format(
                                     "%.2f",
                                     convert_volume( to_milliliter( ip.vol * available_amount ) ) );
            mvwprintz( w_whose, point( qty_x, row_y ), line_color,
                       align_left( qty_str, qty_w ) );
            mvwprintz( w_whose, point( weight_x, row_y ), line_color,
                       align_left( weight_str, weight_w ) );
            mvwprintz( w_whose, point( vol_x, row_y ), line_color,
                       align_left( vol_str, vol_w ) );
            auto price_color = c_light_gray;
            if( !np.will_exchange_items_freely() ) {
                const auto base_price = it->price( true );
                if( base_price > 0 ) {
                    const auto ratio = ip.price / base_price;
                    const auto neutral_low = 0.95;
                    const auto neutral_high = 1.05;
                    if( ratio < neutral_low ) {
                        price_color = they ? c_light_green : c_light_red;
                    } else if( ratio > neutral_high ) {
                        price_color = they ? c_light_red : c_light_green;
                    } else {
                        price_color = c_light_gray;
                    }
                }
            } else {
                price_color = c_dark_gray;
                price_str.clear();
            }
            if( should_hilite ) {
                price_color = hilite( price_color );
            }
            mvwprintz( w_whose, point( price_x, row_y ), price_color,
                       align_right( price_str, price_w ) );
            last_category = category_id;
            row++;
        }
        const auto paging_y = getmaxy( w_whose ) - 1;
        const auto total_pages = entries_per_page > 0 ?
                                 std::max( ( filtered.size() + entries_per_page - 1 ) /
                                           entries_per_page, size_t{1} ) :
                                 size_t{1};
        const auto raw_page = entries_per_page > 0 ? ( offset / entries_per_page ) + 1 : size_t{1};
        const auto current_page = std::min( raw_page, total_pages );
        const auto page_label = string_format( _( "< Page %d/%d >" ),
                                               static_cast<int>( current_page ),
                                               static_cast<int>( total_pages ) );
        const auto page_x = 1 + std::max( win_w - utf8_width( page_label ), 0 );
        mvwprintw( w_whose, point( page_x, paging_y ), page_label );
    }
    const auto &info_list = focus_them ? state.theirs : state.yours;
    const auto &info_filtered = focus_them ? them_filtered : you_filtered;
    const auto info_cursor = focus_them ? them_cursor : you_cursor;
    const auto info_inner_w = getmaxx( w_info ) - 2;
    werase( w_info );
    if( show_item_info ) {
        draw_border( w_info, BORDER_COLOR );
        mvwprintz( w_info, point( 2, 0 ), c_white, _( "< item description >" ) );
        if( !category_mode && !info_filtered.empty() && info_cursor < info_filtered.size() ) {
            const auto &info_item = *info_list[info_filtered[info_cursor]].locs.front();
            const auto info_desc = info_item.type->description.translated();
            fold_and_print( w_info, point( 1, 1 ), info_inner_w, c_light_gray, info_desc );
        } else {
            trim_and_print( w_info, point( 1, 1 ), info_inner_w, c_dark_gray,
                            _( "No item selected." ) );
        }
    }
    wnoutrefresh( w_head );
    wnoutrefresh( w_them );
    wnoutrefresh( w_you );
    wnoutrefresh( w_info );
}

auto trading_window::show_item_data( size_t index, bool target_is_theirs ) -> info_popup_result
{
    auto &target_list = target_is_theirs ? state.theirs : state.yours;
    if( index >= target_list.size() ) {
        return info_popup_result::none;
    }

    const auto &info_win = target_is_theirs ? w_you : w_them;
    auto ui = ui_adaptor{};
    auto w_popup = catacurses::window{};
    auto scroll_pos = size_t{0};
    ui.on_screen_resize( [&]( ui_adaptor & ui ) {
        const auto width = std::max( getmaxx( info_win ), 1 );
        const auto height = std::max( getmaxy( info_win ), 1 );
        const auto pos = point( getbegx( info_win ), getbegy( info_win ) );
        w_popup = catacurses::newwin( height, width, pos );
        ui.position_from_window( w_popup );
    } );
    ui.mark_resize();

    const auto &itm = *target_list[index].locs.front();
    const auto info_text = itm.info_string();

    ui.on_redraw( [&]( const ui_adaptor & ) {
        werase( w_popup );
        draw_border( w_popup );
        const auto inner_w = getmaxx( w_popup ) - 2;
        const auto inner_h = getmaxy( w_popup ) - 2;
        const auto folded = foldstring( info_text, inner_w );
        const auto max_scroll = folded.size() > static_cast<size_t>( inner_h ) ?
                                folded.size() - static_cast<size_t>( inner_h ) : 0;
        scroll_pos = std::min( scroll_pos, max_scroll );
        auto cur_color = c_white;
        for( size_t i = 0; i < static_cast<size_t>( inner_h ) && i + scroll_pos < folded.size(); i++ ) {
            print_colored_text( w_popup, point( 1, 1 + static_cast<int>( i ) ), cur_color,
                                c_white, folded[i + scroll_pos] );
        }
        if( folded.size() > static_cast<size_t>( inner_h ) ) {
            scrollbar()
            .offset_x( getmaxx( w_popup ) - 1 )
            .offset_y( 1 )
            .content_size( static_cast<int>( folded.size() ) )
            .viewport_pos( static_cast<int>( scroll_pos ) )
            .viewport_size( inner_h )
            .scroll_to_last( false )
            .apply( w_popup );
        }
        wnoutrefresh( w_popup );
    } );

    auto ctxt = input_context( "NPC_TRADE" );
    ctxt.register_action( "UP" );
    ctxt.register_action( "DOWN" );
    ctxt.register_action( "PAGE_UP" );
    ctxt.register_action( "PAGE_DOWN" );
    ctxt.register_action( "CONFIRM" );
    ctxt.register_action( "QUIT" );
    ctxt.register_action( "HELP_KEYBINDINGS" );

    auto result = info_popup_result::none;
    auto exit = false;
    while( !exit ) {
        ui_manager::redraw();
        const auto action = ctxt.handle_input();
        if( action == "UP" ) {
            result = info_popup_result::move_up;
            exit = true;
        } else if( action == "DOWN" ) {
            result = info_popup_result::move_down;
            exit = true;
        } else if( action == "PAGE_UP" || action == "PAGE_DOWN" ) {
            const auto inner_h = std::max( getmaxy( w_popup ) - 2, 1 );
            const auto folded = foldstring( info_text, std::max( getmaxx( w_popup ) - 2, 1 ) );
            const auto max_scroll = folded.size() > static_cast<size_t>( inner_h ) ?
                                    folded.size() - static_cast<size_t>( inner_h ) : 0;
            const auto page_rem = static_cast<size_t>( inner_h );
            if( action == "PAGE_UP" ) {
                scroll_pos = scroll_pos > page_rem ? scroll_pos - page_rem : 0;
            } else {
                scroll_pos = std::min( scroll_pos + page_rem, max_scroll );
            }
        } else if( action == "CONFIRM" || action == "QUIT" ) {
            exit = true;
        }
    }
    return result;
}

auto trading_window::build_filtered_indices( const std::vector<item_pricing> &list,
        const std::string &filter ) const -> std::vector<size_t>
{
    if( filter.empty() ) {
        return std::views::iota( size_t{0}, list.size() ) | std::ranges::to<std::vector>();
    }
    const auto filter_fn = item_filter_from_string( filter );
    return std::views::iota( size_t{0}, list.size() )
    | std::views::filter( [&]( size_t idx ) {
        return filter_fn( *list[idx].locs.front() );
    } )
    | std::ranges::to<std::vector>();
}

auto trading_window::get_var_trade( const item &it, int total_count, int amount_hint ) -> int
{
    auto popup_input = string_input_popup{};
    auto how_many = total_count;
    const auto contained = it.is_container() && !it.contents.empty();

    const auto title = contained ?
                       string_format( _( "Trade how many containers with %s [MAX: %d]: " ),
                                      it.get_contained().type_name( how_many ), total_count ) :
                       string_format( _( "Trade how many %s [MAX: %d]: " ), it.type_name( how_many ),
                                      total_count );
    if( amount_hint > 0 ) {
        popup_input.description( string_format(
                                     _( "Hint: You can buy up to %d with your current balance." ),
                                     std::min( amount_hint, total_count ) ) );
    } else if( amount_hint < 0 ) {
        popup_input.description( string_format(
                                     _( "Hint: You'll need to offer %d to even out the deal." ),
                                     -amount_hint ) );
    }
    popup_input.title( title ).edit( how_many );
    if( popup_input.canceled() || how_many <= 0 ) {
        return -1;
    }
    return std::min( total_count, how_many );
}

auto trading_window::perform_trade( npc &np, const std::string &deal ) -> bool
{
    state.volume_left = np.volume_capacity() - np.volume_carried();
    state.weight_left = np.weight_capacity() - np.weight_carried();

    // Shopkeeps are happy to have large inventories.
    if( np.is_shopkeeper() ) {
        state.volume_left = 5000_liter;
        state.weight_left = 5000_kilogram;
    }

    auto ctxt = input_context( "NPC_TRADE" );
    register_trade_actions( ctxt, true );

    auto ui = ui_adaptor{};
    ui.on_screen_resize( [this]( ui_adaptor & ui ) {
        setup_win( ui );
    } );
    ui.mark_resize();

    ui.on_redraw( [&]( const ui_adaptor & ) {
        update_win( np, deal );
    } );

    auto confirm = false;
    auto exit = false;
    auto pending_count = std::optional<int> {};
    category_mode = false;
    them_category_cursor = 0;
    you_category_cursor = 0;

    const auto clamp_cursor_to_list = [&]( size_t list_size, size_t &cursor,
    size_t &offset ) -> void {
        if( entries_per_page == 0 )
        {
            cursor = list_size == 0 ? 0 : std::min( cursor, list_size - 1 );
            offset = 0;
            return;
        }
        if( list_size == 0 )
        {
            cursor = 0;
            offset = 0;
            return;
        }
        cursor = std::min( cursor, list_size - 1 );
        if( list_size <= entries_per_page )
        {
            offset = 0;
            return;
        }
        if( cursor < offset )
        {
            offset = cursor;
        } else if( cursor >= offset + entries_per_page )
        {
            offset = cursor - entries_per_page + 1;
        }
        if( offset + entries_per_page > list_size )
        {
            offset = list_size - entries_per_page;
        }
    };

    const auto apply_trade_change = [&]( item_pricing & ip, int new_amount ) -> void {
        auto &owner_sells = focus_them ? ip.u_has : ip.npc_has;
        auto &owner_sells_charge = focus_them ? ip.u_charges : ip.npc_charges;
        const auto has_charges = ip.charges > 0;
        auto *current_amount = has_charges ? &owner_sells_charge :&owner_sells;
        const auto max_amount = has_charges ? ip.charges : std::max( ip.count, 1 );
        const auto clamped_amount = std::clamp( new_amount, 0, max_amount );
        if( clamped_amount == *current_amount )
        {
            return;
        }
        const auto delta_amount = clamped_amount - *current_amount;
        *current_amount = clamped_amount;
        ip.selected = clamped_amount > 0;

        const auto signed_amount = focus_them ? delta_amount : -delta_amount;
        const auto delta_price = static_cast<int>( ip.price * signed_amount );
        if( !np.will_exchange_items_freely() )
        {
            state.your_balance -= delta_price;
        }
        if( ip.locs.front()->where() == item_location_type::character )
        {
            state.volume_left += ip.vol * signed_amount;
            state.weight_left += ip.weight * signed_amount;
        }
    };
    const auto get_current_amount = [&]( const item_pricing & ip ) -> int {
        if( ip.charges > 0 )
        {
            return focus_them ? ip.u_charges : ip.npc_charges;
        }
        return focus_them ? ip.u_has : ip.npc_has;
    };
    const auto get_max_amount = [&]( const item_pricing & ip ) -> int {
        return ip.charges > 0 ? ip.charges : std::max( ip.count, 1 );
    };
    struct balance_item_entry {
        size_t list_index = 0;
        int current_amount = 0;
        int max_amount = 0;
        int unit_balance_delta = 0;
    };
    struct balance_choice {
        int prev_balance = 0;
        int amount = 0;
    };
    using balance_map = std::unordered_map<int, balance_choice>;
    const auto calc_category_autobalance_plan = [&]( const std::vector<item_pricing> &list,
            const std::vector<size_t> &filtered_indices,
    const category_range & range ) -> std::unordered_map<size_t, int> {
        auto plan = std::unordered_map<size_t, int> {};
        auto entries = std::vector<balance_item_entry> {};
        std::ranges::for_each( std::views::iota( range.start, range.end ),
                               [&]( size_t idx )
        {
            const auto list_index = filtered_indices[idx];
            const auto &ip = list[list_index];
            const auto current_amount = get_current_amount( ip );
            plan.emplace( list_index, current_amount );
            const auto unit_balance_delta = ( focus_them ? -1 : 1 ) *
                                            static_cast<int>( ip.price );
            const auto max_amount = get_max_amount( ip );
            if( unit_balance_delta == 0 || max_amount == 0 ) {
                return;
            }
            entries.push_back( balance_item_entry{
                .list_index = list_index,
                .current_amount = current_amount,
                .max_amount = max_amount,
                .unit_balance_delta = unit_balance_delta
            } );
        } );
        if( entries.empty() )
        {
            return plan;
        }

        auto min_balance = state.your_balance;
        auto max_balance = state.your_balance;
        std::ranges::for_each( entries, [&]( const balance_item_entry & entry )
        {
            const auto delta_at_min = entry.unit_balance_delta * ( 0 - entry.current_amount );
            const auto delta_at_max = entry.unit_balance_delta *
                                      ( entry.max_amount - entry.current_amount );
            min_balance += std::min( delta_at_min, delta_at_max );
            max_balance += std::max( delta_at_min, delta_at_max );
        } );

        auto dp = balance_map{ {
                state.your_balance, balance_choice{
                    .prev_balance = state.your_balance,
                    .amount = 0 }
            } };
        auto decisions = std::vector<balance_map> {};
        decisions.reserve( entries.size() );

        std::ranges::for_each( entries, [&]( const balance_item_entry & entry )
        {
            auto next = balance_map {};
            auto item_decisions = balance_map {};
            std::ranges::for_each( dp, [&]( const auto & entry_pair ) {
                const auto prev_balance = entry_pair.first;
                const auto unit = entry.unit_balance_delta;
                const auto current_amount = entry.current_amount;
                const auto max_amount = entry.max_amount;
                const auto lower = ( static_cast<double>( min_balance - prev_balance ) /
                                     static_cast<double>( unit ) ) + current_amount;
                const auto upper = ( static_cast<double>( max_balance - prev_balance ) /
                                     static_cast<double>( unit ) ) + current_amount;
                const auto min_amount = std::clamp(
                                            static_cast<int>( std::ceil( std::min( lower, upper ) ) ),
                                            0, max_amount );
                const auto max_amount_bound = std::clamp(
                                                  static_cast<int>( std::floor( std::max( lower, upper ) ) ),
                                                  0, max_amount );
                if( min_amount > max_amount_bound ) {
                    return;
                }
                std::ranges::for_each( std::views::iota( min_amount, max_amount_bound + 1 ),
                [&]( int amount ) {
                    const auto new_balance = prev_balance +
                                             unit * ( amount - current_amount );
                    const auto inserted = next.emplace( new_balance, balance_choice{
                        .prev_balance = prev_balance,
                        .amount = amount } ).second;
                    if( inserted ) {
                        item_decisions.emplace( new_balance, balance_choice{
                            .prev_balance = prev_balance,
                            .amount = amount } );
                    }
                } );
            } );
            dp = std::move( next );
            decisions.push_back( std::move( item_decisions ) );
        } );

        if( dp.empty() )
        {
            return plan;
        }

        auto best_balance = std::optional<int> {};
        std::ranges::for_each( dp, [&]( const auto & entry_pair )
        {
            const auto balance = entry_pair.first;
            if( balance >= 0 ) {
                if( !best_balance || balance < *best_balance ) {
                    best_balance = balance;
                }
            }
        } );
        if( !best_balance )
        {
            std::ranges::for_each( dp, [&]( const auto & entry_pair ) {
                const auto balance = entry_pair.first;
                if( !best_balance || balance > *best_balance ) {
                    best_balance = balance;
                }
            } );
        }
        if( !best_balance )
        {
            return plan;
        }

        auto balance = *best_balance;
        for( auto i = static_cast<int>( entries.size() ); i-- > 0; )
        {
            const auto index = static_cast<size_t>( i );
            const auto &entry = entries[index];
            auto &item_decisions = decisions[index];
            const auto found = item_decisions.find( balance );
            if( found == item_decisions.end() ) {
                break;
            }
            plan[entry.list_index] = found->second.amount;
            balance = found->second.prev_balance;
        }
        return plan;
    };
    const auto calc_autobalance_amount = [&]( const item_pricing & ip ) -> int {
        const auto unit_balance_delta = ( focus_them ? -1 : 1 ) * static_cast<int>( ip.price );
        if( unit_balance_delta == 0 )
        {
            return get_current_amount( ip );
        }
        const auto current_amount = get_current_amount( ip );
        const auto max_amount = get_max_amount( ip );
        const auto ideal = static_cast<double>( current_amount ) -
        static_cast<double>( state.your_balance ) /
        static_cast<double>( unit_balance_delta );
        const auto clamp_amount = [&]( int amount ) -> int {
            return std::clamp( amount, 0, max_amount );
        };
        const auto candidates = std::vector<int> {
            clamp_amount( 0 ),
            clamp_amount( max_amount ),
            clamp_amount( static_cast<int>( std::floor( ideal ) ) ),
            clamp_amount( static_cast<int>( std::ceil( ideal ) ) )
        };
        const auto result_balance = [&]( int amount ) -> int {
            return state.your_balance + unit_balance_delta * ( amount - current_amount );
        };
        const auto non_debt_candidates = candidates
        | std::views::filter( [&]( int amount ) { return result_balance( amount ) >= 0; } )
        | std::ranges::to<std::vector>();
        if( !non_debt_candidates.empty() )
        {
            return *std::ranges::min_element(
            non_debt_candidates, [&]( int lhs, int rhs ) {
                const auto lhs_balance = result_balance( lhs );
                const auto rhs_balance = result_balance( rhs );
                const auto lhs_abs = std::abs( lhs_balance );
                const auto rhs_abs = std::abs( rhs_balance );
                if( lhs_abs != rhs_abs ) {
                    return lhs_abs < rhs_abs;
                }
                return std::abs( lhs - current_amount ) < std::abs( rhs - current_amount );
            } );
        }
        return *std::ranges::max_element( candidates, [&]( int lhs, int rhs )
        {
            const auto lhs_balance = result_balance( lhs );
            const auto rhs_balance = result_balance( rhs );
            if( lhs_balance != rhs_balance ) {
                return lhs_balance < rhs_balance;
            }
            return std::abs( lhs - current_amount ) > std::abs( rhs - current_amount );
        } );
    };
    while( !exit ) {
        auto &target_list = focus_them ? state.theirs : state.yours;
        auto &filtered = focus_them ? them_filtered : you_filtered;
        auto &offset = focus_them ? them_off : you_off;
        auto &cursor = focus_them ? them_cursor : you_cursor;
        auto &category_cursor = focus_them ? them_category_cursor : you_category_cursor;
        const auto item_hotkeys = ctxt.get_available_single_char_hotkeys(
                                      "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ" );
        ui_manager::redraw();
        clamp_cursor_to_list( filtered.size(), cursor, offset );
        const auto category_ranges = build_category_ranges( target_list, filtered );
        if( category_cursor >= category_ranges.size() ) {
            category_cursor = category_ranges.empty() ? 0 : category_ranges.size() - 1;
        }

        const auto action = ctxt.handle_input();
        if( action == "SWITCH_LISTS" ) {
            focus_them = !focus_them;
            if( category_mode ) {
                auto &new_target_list = focus_them ? state.theirs : state.yours;
                auto &new_filtered = focus_them ? them_filtered : you_filtered;
                auto &new_offset = focus_them ? them_off : you_off;
                auto &new_cursor = focus_them ? them_cursor : you_cursor;
                auto &new_category_cursor = focus_them ? them_category_cursor : you_category_cursor;
                const auto new_category_ranges = build_category_ranges( new_target_list,
                                                 new_filtered );
                if( !new_category_ranges.empty() ) {
                    if( new_category_cursor >= new_category_ranges.size() ) {
                        new_category_cursor = new_category_ranges.size() - 1;
                    }
                    new_cursor = new_category_ranges[new_category_cursor].start;
                    clamp_cursor_to_list( new_filtered.size(), new_cursor, new_offset );
                }
            }
        } else if( action == "UP" ) {
            if( category_mode ) {
                if( !category_ranges.empty() ) {
                    category_cursor = category_cursor > 0 ? category_cursor - 1 :
                                      category_ranges.size() - 1;
                    cursor = category_ranges[category_cursor].start;
                }
            } else if( !filtered.empty() ) {
                cursor = cursor > 0 ? cursor - 1 : filtered.size() - 1;
            }
        } else if( action == "DOWN" ) {
            if( category_mode ) {
                if( !category_ranges.empty() ) {
                    category_cursor = ( category_cursor + 1 ) < category_ranges.size() ?
                                      category_cursor + 1 : 0;
                    cursor = category_ranges[category_cursor].start;
                }
            } else if( !filtered.empty() ) {
                cursor = ( cursor + 1 ) < filtered.size() ? cursor + 1 : 0;
            }
        } else if( action == "RIGHT" || action == "LEFT" ) {
            if( category_mode ) {
                if( !category_ranges.empty() ) {
                    const auto &range = category_ranges[category_cursor];
                    const auto apply_amount = [&]( item_pricing & ip ) -> void {
                        if( action == "RIGHT" )
                        {
                            const auto max_amount = ip.charges > 0 ? ip.charges :
                            std::max( ip.count, 1 );
                            apply_trade_change( ip, max_amount );
                        } else
                        {
                            apply_trade_change( ip, 0 );
                        }
                    };
                    std::ranges::for_each( std::views::iota( range.start, range.end ),
                    [&]( size_t idx ) {
                        apply_amount( target_list[filtered[idx]] );
                    } );
                }
            } else if( !filtered.empty() ) {
                auto &ip = target_list[filtered[cursor]];
                if( action == "RIGHT" ) {
                    const auto max_amount = ip.charges > 0 ? ip.charges : std::max( ip.count, 1 );
                    const auto requested_amount = pending_count.value_or( max_amount );
                    apply_trade_change( ip, requested_amount );
                } else {
                    apply_trade_change( ip, 0 );
                }
            }
            pending_count.reset();
        } else if( action == "AUTOBALANCE" ) {
            if( filtered.empty() ) {
                continue;
            }
            if( category_mode ) {
                if( category_ranges.empty() ) {
                    continue;
                }
                const auto &range = category_ranges[category_cursor];
                const auto plan = calc_category_autobalance_plan( target_list, filtered, range );
                std::ranges::for_each( std::views::iota( range.start, range.end ),
                [&]( size_t idx ) {
                    const auto list_index = filtered[idx];
                    auto &ip = target_list[list_index];
                    const auto plan_it = plan.find( list_index );
                    if( plan_it != plan.end() ) {
                        apply_trade_change( ip, plan_it->second );
                    }
                } );
            } else {
                auto &ip = target_list[filtered[cursor]];
                const auto best_amount = calc_autobalance_amount( ip );
                apply_trade_change( ip, best_amount );
            }
            pending_count.reset();
        } else if( action == "TOGGLE_ITEM_INFO" ) {
            show_item_info = !show_item_info;
            ui.mark_resize();
        } else if( action == "CATEGORY_SELECTION" ) {
            category_mode = !category_mode;
            if( category_mode && !category_ranges.empty() && !filtered.empty() ) {
                const auto cursor_category = target_list[filtered[cursor]]
                                             .locs.front()->get_category().get_id();
                const auto match = std::ranges::find_if( category_ranges,
                [&]( const category_range & entry ) {
                    return entry.id == cursor_category;
                } );
                if( match != category_ranges.end() ) {
                    category_cursor = static_cast<size_t>(
                                          std::distance( category_ranges.begin(), match ) );
                }
                cursor = category_ranges[category_cursor].start;
                clamp_cursor_to_list( filtered.size(), cursor, offset );
            }
        } else if( action == "FILTER" ) {
            auto &active_filter = focus_them ? them_filter : you_filter;
            const auto original_filter = active_filter;
            filter_edit = true;
            filter_edit_theirs = focus_them;
            const auto &filter_win = focus_them ? w_them : w_you;
            filter_popup = std::make_unique<string_input_popup>();
            const auto filter_prefix = _( "< [" );
            const auto filter_middle = _( "] filter" );
            const auto filter_suffix = _( " >" );
            const auto filter_input_sep = _( ": " );
            const auto filter_input_x = 1 + utf8_width( filter_prefix ) + 1 +
                                        utf8_width( filter_middle ) +
                                        utf8_width( filter_input_sep );
            const auto filter_input_end = std::max(
                                              getmaxx( filter_win ) - 2 -
                                              utf8_width( filter_suffix ),
                                              filter_input_x );
            const auto filter_input_y = getmaxy( filter_win ) - 1;
            filter_popup->max_length( 256 )
            .text( active_filter )
            .identifier( "npc_trade" )
            .window( filter_win, point( filter_input_x, filter_input_y ), filter_input_end );
            auto sentry = ime_sentry{};
            do {
                ui_manager::redraw();
                filter_popup->query_string( false );
            } while( !filter_popup->canceled() && !filter_popup->confirmed() );
            const auto filter_confirmed = filter_popup->confirmed();
            const auto filter_text = std::string( filter_popup->text() );
            filter_edit = false;
            filter_popup = nullptr;
            if( filter_confirmed ) {
                active_filter = filter_text;
                auto &active_list = focus_them ? state.theirs : state.yours;
                auto &active_filtered = focus_them ? them_filtered : you_filtered;
                active_filtered = build_filtered_indices( active_list, active_filter );
                clamp_cursor_to_list( active_filtered.size(), cursor, offset );
            } else {
                active_filter = original_filter;
            }
        } else if( action == "RESET_FILTER" ) {
            auto &active_filter = focus_them ? them_filter : you_filter;
            active_filter.clear();
            auto &active_list = focus_them ? state.theirs : state.yours;
            auto &active_filtered = focus_them ? them_filtered : you_filtered;
            active_filtered = build_filtered_indices( active_list, active_filter );
            clamp_cursor_to_list( active_filtered.size(), cursor, offset );
        } else if( action == "PAGE_UP" ) {
            if( offset > entries_per_page ) {
                offset -= entries_per_page;
            } else {
                offset = 0;
            }
            if( !target_list.empty() ) {
                cursor = offset;
            }
        } else if( action == "PAGE_DOWN" ) {
            if( offset + entries_per_page < filtered.size() ) {
                offset += entries_per_page;
            }
            if( !filtered.empty() ) {
                cursor = offset;
            }
        } else if( action == "EXAMINE" ) {
            if( category_mode ) {
                continue;
            }
            const auto result = show_item_data( filtered.empty() ? 0 : filtered[cursor], focus_them );
            if( !filtered.empty() ) {
                if( result == info_popup_result::move_up ) {
                    cursor = cursor > 0 ? cursor - 1 : filtered.size() - 1;
                } else if( result == info_popup_result::move_down ) {
                    cursor = ( cursor + 1 ) < filtered.size() ? cursor + 1 : 0;
                }
            }
        } else if( action == "CONFIRM" ) {
            if( !npc_trading::npc_will_accept_trade( state, np ) ) {

                if( np.max_credit_extended() == 0 ) {
                    popup( _( "You'll need to offer me more than that." ) );
                } else {
                    popup(
                        _( "Sorry, I'm only willing to extend you %s in credit." ),
                        format_money( np.max_credit_extended() )
                    );
                }
            } else if( state.volume_left < 0_ml || state.weight_left < 0_gram ) {
                // Make sure NPC doesn't go over allowed volume
                popup( _( "%s can't carry all that." ), np.name );
            } else if( npc_trading::calc_npc_owes_you( state, np ) < state.your_balance ) {
                // NPC is happy with the trade, but isn't willing to remember the whole debt.
                const auto trade_ok = query_yn(
                                          _( "I'm never going to be able to pay you back for all that.  The most I'm willing to owe you is %s.\n\nContinue with trade?" ),
                                          format_money( np.max_willing_to_owe() )
                                      );

                if( trade_ok ) {
                    exit = true;
                    confirm = true;
                }
            } else {
                if( query_yn( _( "Looks like a deal!  Accept this trade?" ) ) ) {
                    exit = true;
                    confirm = true;
                }
            }
        } else if( action == "QUIT" ) {
            exit = true;
            confirm = false;
        } else if( action == "ANY_INPUT" ) {
            const auto evt = ctxt.get_raw_input();
            if( evt.type != input_event_t::keyboard || evt.sequence.empty() ) {
                continue;
            }
            auto ch = evt.get_first_input();
            if( ch >= '0' && ch <= '9' ) {
                const auto digit = static_cast<int>( ch - '0' );
                if( !pending_count ) {
                    pending_count.emplace( 0 );
                }
                *pending_count = *pending_count * 10 + digit;
                if( *pending_count <= 0 ) {
                    pending_count.reset();
                }
                continue;
            }
            const auto hotkey_pos = item_hotkeys.find( static_cast<char>( ch ) );
            if( hotkey_pos == std::string::npos ) {
                continue;
            }
            auto ch_index = static_cast<size_t>( hotkey_pos );
            ch_index += offset;
            if( ch_index < filtered.size() ) {
                cursor = ch_index;
                clamp_cursor_to_list( filtered.size(), cursor, offset );
                if( category_mode && !category_ranges.empty() ) {
                    const auto cursor_category = target_list[filtered[cursor]]
                                                 .locs.front()->get_category().get_id();
                    const auto match = std::ranges::find_if( category_ranges,
                    [&]( const category_range & entry ) {
                        return entry.id == cursor_category;
                    } );
                    if( match != category_ranges.end() ) {
                        category_cursor = static_cast<size_t>(
                                              std::distance( category_ranges.begin(), match ) );
                    }
                }
                auto &ip = target_list[filtered[ch_index]];
                auto change_amount = 1;
                auto &owner_sells = focus_them ? ip.u_has : ip.npc_has;
                auto &owner_sells_charge = focus_them ? ip.u_charges : ip.npc_charges;

                const auto calc_amount_hint = [&]() -> int {
                    if( ip.price > 0 )
                    {
                        if( focus_them && state.your_balance > 0 ) {
                            return state.your_balance / ip.price;
                        } else if( !focus_them && state.your_balance < 0 ) {
                            const auto amt = state.your_balance / ip.price;
                            const auto rem = ( std::fmod( state.your_balance, ip.price ) == 0 ) ? 0 : 1;
                            return amt - rem;
                        }
                    }
                    return 0;
                };

                if( ip.selected ) {
                    if( owner_sells_charge > 0 ) {
                        change_amount = owner_sells_charge;
                        owner_sells_charge = 0;
                    } else if( owner_sells > 0 ) {
                        change_amount = owner_sells;
                        owner_sells = 0;
                    }
                } else if( ip.charges > 0 ) {
                    const auto hint = calc_amount_hint();
                    change_amount = get_var_trade( *ip.locs.front(), ip.charges, hint );
                    if( change_amount < 1 ) {
                        continue;
                    }
                    owner_sells_charge = change_amount;
                } else {
                    if( ip.count > 1 ) {
                        const auto hint = calc_amount_hint();
                        change_amount = get_var_trade( *ip.locs.front(), ip.count, hint );
                        if( change_amount < 1 ) {
                            continue;
                        }
                    }
                    owner_sells = change_amount;
                }
                ip.selected = !ip.selected;
                if( ip.selected != focus_them ) {
                    change_amount *= -1;
                }
                const auto delta_price = static_cast<int>( ip.price * change_amount );
                if( !np.will_exchange_items_freely() ) {
                    state.your_balance -= delta_price;
                }
                if( ip.locs.front()->where() == item_location_type::character ) {
                    state.volume_left += ip.vol * change_amount;
                    state.weight_left += ip.weight * change_amount;
                }
            }
        }
    }

    return confirm;
}
