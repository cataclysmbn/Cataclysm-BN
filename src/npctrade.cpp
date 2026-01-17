#include "npctrade.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <memory>
#include <optional>
#include <ranges>
#include <set>
#include <string>
#include <vector>

#include "avatar.h"
#include "catacharset.h"
#include "color.h"
#include "cursesdef.h"
#include "faction.h"
#include "game.h"
#include "input.h"
#include "ime.h"
#include "item.h"
#include "item_category.h"
#include "item_contents.h"
#include "item_search.h"
#include "itype.h"
#include "map_selector.h"
#include "npc.h"
#include "output.h"
#include "player.h"
#include "point.h"
#include "skill.h"
#include "string_formatter.h"
#include "string_input_popup.h"
#include "string_utils.h"
#include "translations.h"
#include "type_id.h"
#include "ui_manager.h"
#include "units_utility.h"
#include "vehicle_selector.h"
#include "visitable.h"

static const skill_id skill_barter( "barter" );
static const flag_id json_flag_NO_UNWIELD( "NO_UNWIELD" );

namespace
{
constexpr auto trade_head_height = 6;
constexpr auto trade_info_height = 4;
constexpr auto trade_header_rows = 4;
constexpr auto trade_header_separator_rows = 0;
constexpr auto trade_total_header_rows = trade_header_rows + trade_header_separator_rows;
} // namespace

void npc_trading::transfer_items( std::vector<item_pricing> &stuff, Character &,
                                  Character &receiver, bool npc_gives )
{
    for( item_pricing &ip : stuff ) {
        if( !ip.selected ) {
            continue;
        }

        item &gift = *ip.locs.front();
        const int charges = npc_gives ? ip.u_charges : ip.npc_charges;

        if( ip.charges ) {
            auto to_give = gift.split( charges );
            to_give->set_owner( receiver );

            receiver.i_add( std::move( to_give ) );
        } else {
            gift.set_owner( receiver );
            for( item *&it : ip.locs ) {
                receiver.i_add( it->detach() );
            }
        }
    }
}

std::vector<item_pricing> npc_trading::init_selling( npc &np )
{
    std::vector<item_pricing> result;
    const_invslice slice = np.inv_const_slice();
    for( auto const &i : slice ) {
        item &it = *i->front();

        const int price = it.price( true );
        int val = np.value( it );
        if( np.wants_to_sell( it, val, price ) ) {
            result.emplace_back( *i, val, static_cast<int>( i->size() ) );
        }
    }

    if( np.will_exchange_items_freely() ) {
        for( item *weapon : np.wielded_items() ) {
            if( !weapon->has_flag( json_flag_NO_UNWIELD ) ) {
                result.emplace_back( std::vector<item *> {weapon}, np.value( *weapon ), 0 );
            }
        }
    }

    return result;
}

double npc_trading::net_price_adjustment( const Character &buyer, const Character &seller )
{
    // Adjust the prices based on your barter skill.
    // cap adjustment so nothing is ever sold below value
    ///\EFFECT_INT_NPC slightly increases bartering price changes, relative to your INT

    ///\EFFECT_BARTER_NPC increases bartering price changes, relative to your BARTER

    ///\EFFECT_INT slightly increases bartering price changes, relative to NPC INT

    ///\EFFECT_BARTER increases bartering price changes, relative to NPC BARTER
    double adjust = 0.05 * ( seller.int_cur - buyer.int_cur ) +
                    price_adjustment( seller.get_skill_level( skill_barter ) -
                                      buyer.get_skill_level( skill_barter ) );
    return( std::max( adjust, 1.0 ) );
}

template <typename T, typename Callback>
void buy_helper( T &src, Callback cb )
{
    src.visit_items( [&cb]( item * node ) {
        cb( node, 1 );

        return VisitResponse::SKIP;
    } );
}

std::vector<item_pricing> npc_trading::init_buying( Character &buyer, Character &seller,
        bool is_npc )
{
    std::vector<item_pricing> result;
    npc *np_p = dynamic_cast<npc *>( &buyer );
    if( is_npc ) {
        np_p = dynamic_cast<npc *>( &seller );
    }
    npc &np = *np_p;
    faction *fac = np.get_faction();

    double adjust = net_price_adjustment( buyer, seller );

    const auto check_item = [fac, adjust, is_npc, &np, &result,
                                  &seller]( const std::vector<item *> &locs,
    int count = 1 ) {
        item *it_ptr = locs.front();
        if( it_ptr == nullptr || it_ptr->is_null() ) {
            return;
        }
        item &it = *it_ptr;

        // Don't sell items we don't own.
        if( !it.is_owned_by( seller ) ) {
            return;
        }

        const int market_price = it.price( true );
        int val = np.value( it, market_price );
        if( ( is_npc && np.wants_to_sell( it, val, market_price ) ) ||
            np.wants_to_buy( it, val, market_price ) ) {
            result.emplace_back( locs, val, count );
            result.back().adjust_values( adjust, fac );
        }
    };

    const_invslice slice = seller.inv_const_slice();
    for( const auto &i : slice ) {
        check_item( *i, i->size() );
    }

    if( !seller.primary_weapon().has_flag( json_flag_NO_UNWIELD ) ) {
        check_item( {&seller.primary_weapon()}, 1 );
    }

    //nearby items owned by the NPC will only show up in
    //the trade window if the NPC is also a shopkeeper
    if( np.is_shopkeeper() ) {
        for( map_cursor &cursor : map_selector( seller.pos(), PICKUP_RANGE ) ) {
            cursor.visit_items( [&check_item]( item * node ) {
                check_item( {node}, 1 );
                return VisitResponse::SKIP;
            } );
        }
    }

    for( vehicle_cursor &cursor : vehicle_selector( seller.pos(), 1 ) ) {
        cursor.visit_items( [&check_item]( item * node ) {
            check_item( {node}, 1 );
            return VisitResponse::SKIP;
        } );
    }

    const auto cmp = []( const item_pricing & a, const item_pricing & b ) {
        // Sort items by category first, then name.
        return localized_compare( std::make_pair( a.locs.front()->get_category(),
                                  a.locs.front()->display_name() ),
                                  std::make_pair( b.locs.front()->get_category(), b.locs.front()->display_name() ) );
    };

    std::ranges::sort( result, cmp );

    return result;
}

void item_pricing::set_values( int ip_count )
{
    const item *i_p = locs.front();
    is_container = i_p->is_container() || i_p->is_ammo_container();
    vol = i_p->volume();
    weight = i_p->weight();
    if( is_container || i_p->count() == 1 ) {
        count = ip_count;
    } else {
        charges = i_p->count();
        price /= charges;
        vol /= charges;
        weight /= charges;
    }
}

// Adjusts the pricing of an item, *unless* it is the currency of the
// faction we're trading with, as that should always be worth face value.
void item_pricing::adjust_values( const double adjust, const faction *fac )
{
    if( !fac || fac->currency != locs.front()->typeId() ) {
        price *= adjust;
    }
}

void trading_window::setup_win( ui_adaptor &ui )
{
    const int win_they_w = TERMX / 2;
    entries_per_page = std::min( TERMY - trade_head_height - trade_info_height - 3 -
                                 trade_total_header_rows,
                                 2 + ( 'z' - 'a' ) + ( 'Z' - 'A' ) );
    w_head = catacurses::newwin( trade_head_height, TERMX, point_zero );
    const auto list_height = TERMY - trade_head_height - trade_info_height;
    w_them = catacurses::newwin( list_height, win_they_w,
                                 point( 0, trade_head_height ) );
    w_you = catacurses::newwin( list_height, TERMX - win_they_w,
                                point( win_they_w, trade_head_height ) );
    w_info = catacurses::newwin( trade_info_height, TERMX,
                                 point( 0, trade_head_height + list_height ) );
    ui.position( point_zero, point( TERMX, TERMY ) );
}

// 'cost' is the cost of a service the NPC may be rendering, if any.
void trading_window::setup_trade( int cost, npc &np )
{
    // Populate the list of what the NPC is willing to buy, and the prices they pay
    // Note that the NPC's barter skill is factored into these prices.
    // TODO: Recalc item values every time a new item is selected
    // Trading is not linear - starving NPC may pay $100 for 3 jerky, but not $100000 for 300 jerky
    theirs = npc_trading::init_buying( g->u, np, true );
    yours = npc_trading::init_buying( np, g->u, false );

    if( np.will_exchange_items_freely() ) {
        your_balance = 0;
    } else {
        your_balance = np.op_of_u.owed - cost;
    }
}

void trading_window::update_win( npc &np, const std::string &deal )
{
    // Draw borders, one of which is highlighted
    werase( w_them );
    werase( w_you );

    std::set<item *> without;
    std::vector<item *> added;

    for( item_pricing &pricing : yours ) {
        if( pricing.selected ) {
            added.push_back( pricing.locs.front() );
        }
    }

    for( item_pricing &pricing : theirs ) {
        if( pricing.selected ) {
            without.insert( pricing.locs.front() );
        }
    }

    // Colors for hinting if the trade will be accepted or not.
    const nc_color trade_color       = npc_will_accept_trade( np ) ? c_green : c_red;
    const nc_color trade_color_light = npc_will_accept_trade( np ) ? c_light_green : c_light_red;
    struct selection_totals {
        units::volume volume = 0_ml;
        units::mass weight = 0_gram;
    };

    input_context ctxt( "NPC_TRADE" );

    werase( w_head );
    draw_border( w_head );
    const auto head_inner_w = getmaxx( w_head ) - 2;
    const auto head_title_y = 1;
    const auto head_keybinds_label_prefix = _( "< [" );
    const auto head_keybinds_label_middle = _( "] keybindings >" );
    const auto head_keybinds_label_width = utf8_width( head_keybinds_label_prefix ) +
                                           1 + utf8_width( head_keybinds_label_middle );
    const auto title_label = _( "Trading:" );
    mvwprintz( w_head, point( 1, head_title_y ), c_white, title_label );
    mvwprintz( w_head, point( 1 + utf8_width( title_label ) + 1, head_title_y ),
               c_light_green, np.disp_name() );

    std::string cost_str = _( "Exchange" );
    if( !np.will_exchange_items_freely() ) {
        cost_str = string_format( your_balance >= 0 ? _( "Credit %s" ) : _( "Debt %s" ),
                                  format_money( std::abs( your_balance ) ) );
    }

    const auto head_bottom_y = getmaxy( w_head ) - 1;
    const auto cost_tag = string_format( "< %s >", cost_str );
    const auto cost_w = utf8_width( cost_tag );
    const auto cost_x = 1 + ( head_inner_w - cost_w ) / 2;
    mvwprintz( w_head, point( cost_x, head_bottom_y ), trade_color, cost_tag );

    if( !deal.empty() ) {
        const auto deal_w = utf8_width( deal );
        const auto deal_x = 1 + ( head_inner_w - deal_w ) / 2;
        mvwprintz( w_head, point( deal_x, head_title_y ),
                   trade_color_light, deal );
    }
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

    const auto selected_amount = []( const item_pricing &ip, bool is_theirs ) -> int {
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
    const auto your_selected = sum_selected( yours, false );
    const auto their_selected = sum_selected( theirs, true );
    const auto player_free_volume = g->u.volume_capacity() - g->u.volume_carried() +
                                    your_selected.volume - their_selected.volume;
    const auto player_free_weight = g->u.weight_capacity() - g->u.weight_carried() +
                                    your_selected.weight - their_selected.weight;

    them_filtered = build_filtered_indices( theirs, them_filter );
    you_filtered = build_filtered_indices( yours, you_filter );

    const auto show_filter_help = filter_edit;
    const auto help_on_theirs = !filter_edit_theirs;
    // Draw lists of items, starting from offset
    for( size_t whose = 0; whose <= 1; whose++ ) {
        const bool they = whose == 0;
        const std::vector<item_pricing> &list = they ? theirs : yours;
        const std::vector<size_t> &filtered = they ? them_filtered : you_filtered;
        const size_t &offset = they ? them_off : you_off;
        const player &person = they ? static_cast<player &>( np ) :
                               static_cast<player &>( g->u );
        catacurses::window &w_whose = they ? w_them : w_you;
        int win_w = getmaxx( w_whose );
        // Borders
        win_w -= 2;
        const auto end = std::min( filtered.size(), offset + entries_per_page );
        const auto visible = std::views::iota( offset, end );
        const auto calc_amount = [&]( const item_pricing & ip, int owner_sells,
        int owner_sells_charge ) -> int {
            auto amount = ip.charges > 0 ? ip.charges : 1;
            if( ip.charges > 0 && owner_sells_charge > 0 )
            {
                amount = owner_sells_charge;
            } else if( owner_sells > 0 )
            {
                amount = owner_sells;
            }
            return amount;
        };
        const auto max_width = [&]( auto make_width ) -> int {
            return std::ranges::fold_left( visible | std::views::transform( make_width ), 0,
            []( int acc, int value ) { return std::max( acc, value ); } );
        };
        const auto qty_label = _( "amt" );
        const auto weight_label = _( "weight" );
        const auto vol_label = _( "vol" );
        const auto price_label = _( "price" );
        auto qty_w = max_width( [&]( size_t idx ) -> int {
            const auto &ip = list[filtered[idx]];
            const auto owner_sells = they ? ip.u_has : ip.npc_has;
            const auto owner_sells_charge = they ? ip.u_charges : ip.npc_charges;
            const auto amount = calc_amount( ip, owner_sells, owner_sells_charge );
            return amount > 1 ? utf8_width( string_format( "%d", amount ) ) : 0;
        } );
        auto weight_w = max_width( [&]( size_t idx ) -> int {
            const auto &ip = list[filtered[idx]];
            const auto owner_sells = they ? ip.u_has : ip.npc_has;
            const auto owner_sells_charge = they ? ip.u_charges : ip.npc_charges;
            const auto amount = calc_amount( ip, owner_sells, owner_sells_charge );
            const auto weight_str = string_format( "%.2f", convert_weight( ip.weight * amount ) );
            return utf8_width( weight_str );
        } );
        auto vol_w = max_width( [&]( size_t idx ) -> int {
            const auto &ip = list[filtered[idx]];
            const auto owner_sells = they ? ip.u_has : ip.npc_has;
            const auto owner_sells_charge = they ? ip.u_charges : ip.npc_charges;
            const auto amount = calc_amount( ip, owner_sells, owner_sells_charge );
            const auto vol_str = string_format( "%.2f",
                                                convert_volume( to_milliliter( ip.vol * amount ) ) );
            return utf8_width( vol_str );
        } );
        auto price_w = max_width( [&]( size_t idx ) -> int {
            const auto &ip = list[filtered[idx]];
            const auto owner_sells = they ? ip.u_has : ip.npc_has;
            const auto owner_sells_charge = they ? ip.u_charges : ip.npc_charges;
            const auto amount = calc_amount( ip, owner_sells, owner_sells_charge );
            return utf8_width( format_money( ip.price * amount ) );
        } );
        qty_w = std::max( qty_w, utf8_width( qty_label ) );
        vol_w = std::max( vol_w, utf8_width( vol_label ) );
        weight_w = std::max( weight_w, utf8_width( weight_label ) );
        price_w = std::max( price_w, utf8_width( price_label ) );
        const auto align_left = [&]( const std::string & text, int width ) -> std::string {
            const auto pad = std::max( width - utf8_width( text ), 0 );
            return text + std::string( pad, ' ' );
        };
        const auto price_x = win_w - price_w;
        const auto vol_x = price_x - 1 - vol_w;
        const auto weight_x = vol_x - 1 - weight_w;
        const auto qty_x = weight_x - 1 - qty_w;
        const auto name_indent = 2;
        const auto name_x = 1 + name_indent;
        const auto name_w = std::max( qty_x - 2 - name_indent, 1 );
        const auto stats_y = 2;
        const auto separator_y = 3;
        const auto header_y = 4;
        const auto header_color = c_light_gray;
        const auto pane_free_volume = they ? volume_left : player_free_volume;
        const auto pane_free_weight = they ? weight_left : player_free_weight;
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
        const auto draw_filter_help = show_filter_help && ( they == help_on_theirs );
        if( draw_filter_help ) {
            const auto help_start_y = trade_total_header_rows + 1;
            const auto help_height = std::max( getmaxy( w_whose ) - help_start_y - 1, 0 );
            if( help_height > 0 ) {
                draw_item_filter_rules( w_whose, help_start_y, help_height,
                                        item_filter_type::FILTER );
            }
            continue;
        }
        std::optional<item_category_id> last_category;
        size_t row = 0;
        for( size_t i = offset; i < filtered.size() && row < entries_per_page; i++ ) {
            const auto list_index = filtered[i];
            const item_pricing &ip = list[list_index];
            const item *it = ip.locs.front();
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
            const int &owner_sells = they ? ip.u_has : ip.npc_has;
            const int &owner_sells_charge = they ? ip.u_charges : ip.npc_charges;
            int amount = ip.charges > 0 ? ip.charges : 1;
            std::string itname = it->display_name();

            if( np.will_exchange_items_freely() && ip.locs.front()->where() != item_location_type::character ) {
                itname = itname + " (" + ip.locs.front()->describe_location( &g->u ) + ")";
                color = c_light_blue;
            }

            if( ip.charges > 0 && owner_sells_charge > 0 ) {
                amount = owner_sells_charge;
            } else {
                if( ip.count > 1 ) {
                    itname += string_format( _( " (%d)" ), ip.count );
                }
                if( owner_sells ) {
                    amount = owner_sells;
                }
            }

            if( ip.selected ) {
                color = c_white;
            }
            auto line_color = color;
            if( is_cursor ) {
                line_color = hilite( c_white );
            }
            if( is_cursor ) {
                const auto fill = std::string( win_w, ' ' );
                mvwprintz( w_whose, point( 1, row_y ), line_color, fill );
            }

            int keychar = i - offset + 'a';
            if( keychar > 'z' ) {
                keychar = keychar - 'z' - 1 + 'A';
            }
            const auto total_amount = ip.charges > 0 ? ip.charges : std::max( ip.count, 1 );
            const auto selected_amount = ip.charges > 0 ? owner_sells_charge : owner_sells;
            auto selection_mark = '-';
            if( selected_amount >= total_amount && total_amount > 0 ) {
                selection_mark = '+';
            } else if( selected_amount > 0 ) {
                selection_mark = '#';
            }
            trim_and_print( w_whose, point( name_x, row_y ), name_w, line_color, "%c %c %s",
                            static_cast<char>( keychar ), selection_mark, itname );
#if defined(__ANDROID__)
            ctxt.register_manual_key( keychar, itname );
#endif

            std::string price_str = format_money( ip.price * amount );
            const auto qty_str = amount > 1 ? string_format( "%d", amount ) : std::string();
            const auto weight_str = string_format( "%.2f",
                                                   convert_weight( ip.weight * amount ) );
            const auto vol_str = string_format( "%.2f",
                                                convert_volume( to_milliliter( ip.vol * amount ) ) );
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
            if( is_cursor ) {
                price_color = hilite( price_color );
            }
            mvwprintz( w_whose, point( price_x, row_y ), price_color,
                       align_left( price_str, price_w ) );
            last_category = category_id;
            row++;
        }
        const auto paging_y = static_cast<int>( trade_total_header_rows + entries_per_page + 1 );
        if( offset > 0 ) {
            mvwprintw( w_whose, point( 1, paging_y ), _( "< Back" ) );
        }
        if( offset + entries_per_page < filtered.size() ) {
            mvwprintw( w_whose, point( 9, paging_y ), _( "More >" ) );
        }
    }
    const auto &info_list = focus_them ? theirs : yours;
    const auto &info_filtered = focus_them ? them_filtered : you_filtered;
    const auto info_cursor = focus_them ? them_cursor : you_cursor;
    const auto info_inner_w = getmaxx( w_info ) - 2;
    werase( w_info );
    draw_border( w_info, BORDER_COLOR );
    mvwprintz( w_info, point( 2, 0 ), c_white, _( "< item description >" ) );
    if( !info_filtered.empty() && info_cursor < info_filtered.size() ) {
        const auto &info_item = *info_list[info_filtered[info_cursor]].locs.front();
        const auto info_desc = info_item.type->description.translated();
        fold_and_print( w_info, point( 1, 1 ), info_inner_w, c_light_gray, info_desc );
    } else {
        trim_and_print( w_info, point( 1, 1 ), info_inner_w, c_dark_gray,
                        _( "No item selected." ) );
    }
    wnoutrefresh( w_head );
    wnoutrefresh( w_them );
    wnoutrefresh( w_you );
    wnoutrefresh( w_info );
}

auto trading_window::show_item_data( size_t index, bool target_is_theirs ) -> info_popup_result
{
    auto &target_list = target_is_theirs ? theirs : yours;
    if( index >= target_list.size() ) {
        return info_popup_result::none;
    }

    const auto &info_win = target_is_theirs ? w_you : w_them;
    ui_adaptor ui;
    catacurses::window w_popup;
    ui.on_screen_resize( [&]( ui_adaptor & ui ) {
        const auto width = getmaxx( info_win );
        const auto height = getmaxy( info_win );
        const auto pos = point( getbegx( info_win ), getbegy( info_win ) );
        w_popup = catacurses::newwin( height, width, pos );
        ui.position_from_window( w_popup );
    } );
    ui.mark_resize();

    const item &itm = *target_list[index].locs.front();
    const auto info_text = itm.info_string();

    ui.on_redraw( [&]( const ui_adaptor & ) {
        werase( w_popup );
        draw_border( w_popup );
        const auto inner_w = getmaxx( w_popup ) - 2;
        fold_and_print( w_popup, point( 1, 1 ), inner_w, c_white, info_text );
        wnoutrefresh( w_popup );
    } );

    input_context ctxt( "NPC_TRADE" );
    ctxt.register_action( "UP" );
    ctxt.register_action( "DOWN" );
    ctxt.register_action( "CONFIRM" );
    ctxt.register_action( "QUIT" );
    ctxt.register_action( "HELP_KEYBINDINGS" );

    auto result = info_popup_result::none;
    bool exit = false;
    while( !exit ) {
        ui_manager::redraw();
        const auto action = ctxt.handle_input();
        if( action == "UP" ) {
            result = info_popup_result::move_up;
            exit = true;
        } else if( action == "DOWN" ) {
            result = info_popup_result::move_down;
            exit = true;
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

int trading_window::get_var_trade( const item &it, int total_count, int amount_hint )
{
    string_input_popup popup_input;
    int how_many = total_count;
    const bool contained = it.is_container() && !it.contents.empty();

    const std::string title = contained ?
                              string_format( _( "Trade how many containers with %s [MAX: %d]: " ),
                                      it.get_contained().type_name( how_many ), total_count ) :
                              string_format( _( "Trade how many %s [MAX: %d]: " ), it.type_name( how_many ), total_count );
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

bool trading_window::perform_trade( npc &np, const std::string &deal )
{
    volume_left = np.volume_capacity() - np.volume_carried();
    weight_left = np.weight_capacity() - np.weight_carried();

    // Shopkeeps are happy to have large inventories.
    if( np.is_shopkeeper() ) {
        volume_left = 5000_liter;
        weight_left = 5000_kilogram;
    }

    input_context ctxt( "NPC_TRADE" );
    ctxt.register_action( "SWITCH_LISTS" );
    ctxt.register_action( "UP" );
    ctxt.register_action( "DOWN" );
    ctxt.register_action( "LEFT" );
    ctxt.register_action( "RIGHT" );
    ctxt.register_action( "FILTER" );
    ctxt.register_action( "PAGE_UP" );
    ctxt.register_action( "PAGE_DOWN" );
    ctxt.register_action( "EXAMINE" );
    ctxt.register_action( "CONFIRM" );
    ctxt.register_action( "QUIT" );
    ctxt.register_action( "HELP_KEYBINDINGS" );
    ctxt.register_action( "ANY_INPUT" );

    ui_adaptor ui;
    ui.on_screen_resize( [this]( ui_adaptor & ui ) {
        setup_win( ui );
    } );
    ui.mark_resize();

    ui.on_redraw( [&]( const ui_adaptor & ) {
        update_win( np, deal );
    } );

    bool confirm = false;
    bool exit = false;
    std::optional<int> pending_count;

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
            your_balance -= delta_price;
        }
        if( ip.locs.front()->where() == item_location_type::character )
        {
            volume_left += ip.vol * signed_amount;
            weight_left += ip.weight * signed_amount;
        }
    };
    while( !exit ) {
        auto &target_list = focus_them ? theirs : yours;
        auto &filtered = focus_them ? them_filtered : you_filtered;
        auto &offset = focus_them ? them_off : you_off;
        auto &cursor = focus_them ? them_cursor : you_cursor;
        clamp_cursor_to_list( filtered.size(), cursor, offset );
        ui_manager::redraw();

        const std::string action = ctxt.handle_input();
        if( action == "SWITCH_LISTS" ) {
            focus_them = !focus_them;
        } else if( action == "UP" ) {
            if( !filtered.empty() ) {
                cursor = cursor > 0 ? cursor - 1 : filtered.size() - 1;
            }
        } else if( action == "DOWN" ) {
            if( !filtered.empty() ) {
                cursor = ( cursor + 1 ) < filtered.size() ? cursor + 1 : 0;
            }
        } else if( action == "RIGHT" || action == "LEFT" ) {
            if( !filtered.empty() ) {
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
        } else if( action == "FILTER" ) {
            auto &active_filter = focus_them ? them_filter : you_filter;
            const auto original_filter = active_filter;
            filter_edit = true;
            filter_edit_theirs = focus_them;
            filter_popup = std::make_unique<string_input_popup>();
            filter_popup->max_length( 256 ).text( active_filter ).identifier( "npc_trade" );
            ime_sentry sentry;
            do {
                ui_manager::redraw();
                const auto new_filter = filter_popup->query_string( false );
                if( filter_popup->canceled() ) {
                    active_filter = original_filter;
                } else {
                    active_filter = new_filter;
                }
            } while( !filter_popup->canceled() && !filter_popup->confirmed() );
            filter_edit = false;
            filter_popup = nullptr;
            auto &active_list = focus_them ? theirs : yours;
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
            const auto result = show_item_data( filtered.empty() ? 0 : filtered[cursor], focus_them );
            if( !filtered.empty() ) {
                if( result == info_popup_result::move_up ) {
                    cursor = cursor > 0 ? cursor - 1 : filtered.size() - 1;
                } else if( result == info_popup_result::move_down ) {
                    cursor = ( cursor + 1 ) < filtered.size() ? cursor + 1 : 0;
                }
            }
        } else if( action == "CONFIRM" ) {
            if( !npc_will_accept_trade( np ) ) {

                if( np.max_credit_extended() == 0 ) {
                    popup( _( "You'll need to offer me more than that." ) );
                } else {
                    popup(
                        _( "Sorry, I'm only willing to extend you %s in credit." ),
                        format_money( np.max_credit_extended() )
                    );
                }
            } else if( volume_left < 0_ml || weight_left < 0_gram ) {
                // Make sure NPC doesn't go over allowed volume
                popup( _( "%s can't carry all that." ), np.name );
            } else if( calc_npc_owes_you( np ) < your_balance ) {
                // NPC is happy with the trade, but isn't willing to remember the whole debt.
                const bool trade_ok = query_yn(
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
            const input_event evt = ctxt.get_raw_input();
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
            // Letters & such
            if( ch >= 'a' && ch <= 'z' ) {
                ch -= 'a';
            } else if( ch >= 'A' && ch <= 'Z' ) {
                ch = ch - 'A' + ( 'z' - 'a' ) + 1;
            } else {
                continue;
            }

            auto ch_index = static_cast<size_t>( ch );
            ch_index += offset;
            if( ch_index < filtered.size() ) {
                item_pricing &ip = target_list[filtered[ch_index]];
                int change_amount = 1;
                int &owner_sells = focus_them ? ip.u_has : ip.npc_has;
                int &owner_sells_charge = focus_them ? ip.u_charges : ip.npc_charges;

                const auto calc_amount_hint = [&]() -> int {
                    if( ip.price > 0 )
                    {
                        if( focus_them && your_balance > 0 ) {
                            return your_balance / ip.price;
                        } else if( !focus_them && your_balance < 0 ) {
                            int amt = your_balance / ip.price;
                            int rem = ( std::fmod( your_balance, ip.price ) == 0 ) ? 0 : 1;
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
                    int hint = calc_amount_hint();
                    change_amount = get_var_trade( *ip.locs.front(), ip.charges, hint );
                    if( change_amount < 1 ) {
                        continue;
                    }
                    owner_sells_charge = change_amount;
                } else {
                    if( ip.count > 1 ) {
                        int hint = calc_amount_hint();
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
                int delta_price = ip.price * change_amount;
                if( !np.will_exchange_items_freely() ) {
                    your_balance -= delta_price;
                }
                if( ip.locs.front()->where() == item_location_type::character ) {
                    volume_left += ip.vol * change_amount;
                    weight_left += ip.weight * change_amount;
                }
            }
        }
    }

    return confirm;
}

// Returns how much the NPC will owe you after this transaction.
// You must also check if they will accept the trade.
int trading_window::calc_npc_owes_you( const npc &np ) const
{
    // Friends don't hold debts against friends.
    if( np.will_exchange_items_freely() ) {
        return 0;
    }

    // If they're going to owe you more than before, and it's more than they're willing
    // to owe, then cap the amount owed at the present level or their willingness to owe
    // (whichever is bigger).
    //
    // When could they owe you more than max_willing_to_owe? It could be from quest rewards,
    // when they were less angry, or from when you were better friends.
    if( your_balance > np.op_of_u.owed && your_balance > np.max_willing_to_owe() ) {
        return std::max( np.op_of_u.owed, np.max_willing_to_owe() );
    }

    // Fair's fair. NPC will remember this debt (or credit they've extended)
    return your_balance;
}

void trading_window::update_npc_owed( npc &np )
{
    np.op_of_u.owed = calc_npc_owes_you( np );
}

// Oh my aching head
// op_of_u.owed is the positive when the NPC owes the player, and negative if the player owes the
// NPC
// cost is positive when the player owes the NPC money for a service to be performed
bool npc_trading::trade( npc &np, int cost, const std::string &deal )
{
    // Only allow actual shopkeepers to refresh their inventory like this
    if( np.is_shopkeeper() ) {
        np.shop_restock();
    }
    //np.drop_items( np.weight_carried() - np.weight_capacity(),
    //               np.volume_carried() - np.volume_capacity() );
    np.drop_invalid_inventory();

    trading_window trade_win;
    trade_win.setup_trade( cost, np );

    bool traded = trade_win.perform_trade( np, deal );
    if( traded ) {
        int practice = 0;

        npc_trading::transfer_items( trade_win.yours, g->u, np, false );
        npc_trading::transfer_items( trade_win.theirs, np, g->u, true );

        // NPCs will remember debts, to the limit that they'll extend credit or previous debts
        if( !np.will_exchange_items_freely() ) {
            trade_win.update_npc_owed( np );
            g->u.practice( skill_barter, practice / 10000 );
        }
    }
    return traded;
}

// Will the NPC accept the trade that's currently on offer?
bool trading_window::npc_will_accept_trade( const npc &np ) const
{
    return np.will_exchange_items_freely() || your_balance + np.max_credit_extended() > 0;
}
