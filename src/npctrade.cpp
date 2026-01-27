#include "npctrade.h"

#include <algorithm>
#include <memory>
#include <ranges>
#include <string>
#include <vector>

#include "avatar.h"
#include "faction.h"
#include "game.h"
#include "item.h"
#include "item_category.h"
#include "map_selector.h"
#include "npc.h"
#include "player.h"
#include "skill.h"
#include "string_utils.h"
#include "trade_win.h"
#include "type_id.h"
#include "vehicle_selector.h"
#include "visitable.h"

static const skill_id skill_barter( "barter" );
static const flag_id json_flag_NO_UNWIELD( "NO_UNWIELD" );

void npc_trading::transfer_items( std::vector<item_pricing> &stuff, Character &,
                                  Character &receiver, bool npc_gives )
{
    std::ranges::for_each( stuff, [&]( auto & ip ) {
        if( !ip.selected ) {
            return;
        }

        auto &gift = *ip.locs.front();
        const auto charges = npc_gives ? ip.u_charges : ip.npc_charges;

        if( ip.charges ) {
            auto to_give = gift.split( charges );
            to_give->set_owner( receiver );

            receiver.i_add( std::move( to_give ) );
        } else {
            gift.set_owner( receiver );
            std::ranges::for_each( ip.locs, [&]( auto * it ) {
                receiver.i_add( it->detach() );
            } );
        }
    } );
}

std::vector<item_pricing> npc_trading::init_selling( npc &np )
{
    auto result = std::vector<item_pricing> {};
    const auto slice = np.inv_const_slice();
    std::ranges::for_each( slice, [&]( const auto & i ) {
        auto &it = *i->front();

        const auto price = it.price( true );
        auto val = np.value( it );
        if( np.wants_to_sell( it, val, price ) ) {
            result.emplace_back( *i, val, static_cast<int>( i->size() ) );
        }
    } );

    if( np.will_exchange_items_freely() ) {
        std::ranges::for_each( np.wielded_items(), [&]( auto * weapon ) {
            if( !weapon->has_flag( json_flag_NO_UNWIELD ) ) {
                result.emplace_back( std::vector<item *> { weapon }, np.value( *weapon ), 0 );
            }
        } );
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
    std::ranges::for_each( slice, [&]( const auto & i ) {
        check_item( *i, i->size() );
    } );

    if( !seller.primary_weapon().has_flag( json_flag_NO_UNWIELD ) ) {
        check_item( {&seller.primary_weapon()}, 1 );
    }

    //nearby items owned by the NPC will only show up in
    //the trade window if the NPC is also a shopkeeper
    if( np.is_shopkeeper() ) {
        std::ranges::for_each( map_selector( seller.pos(), PICKUP_RANGE ),
        [&]( auto & cursor ) {
            cursor.visit_items( [&check_item]( item * node ) {
                check_item( {node}, 1 );
                return VisitResponse::SKIP;
            } );
        } );
    }

    std::ranges::for_each( vehicle_selector( seller.pos(), 1 ), [&]( auto & cursor ) {
        cursor.visit_items( [&check_item]( item * node ) {
            check_item( {node}, 1 );
            return VisitResponse::SKIP;
        } );
    } );

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

auto npc_trading::setup_trade_state( trade_state &state, int cost, npc &np ) -> void
{
    // Populate the list of what the NPC is willing to buy, and the prices they pay
    // Note that the NPC's barter skill is factored into these prices.
    // TODO: Recalc item values every time a new item is selected
    // Trading is not linear - starving NPC may pay $100 for 3 jerky, but not $100000 for 300 jerky
    state.theirs = npc_trading::init_buying( g->u, np, true );
    state.yours = npc_trading::init_buying( np, g->u, false );

    if( np.will_exchange_items_freely() ) {
        state.your_balance = 0;
    } else {
        state.your_balance = np.op_of_u.owed - cost;
    }
}

auto npc_trading::npc_will_accept_trade( const trade_state &state, const npc &np ) -> bool
{
    return np.will_exchange_items_freely() || state.your_balance + np.max_credit_extended() > 0;
}

auto npc_trading::calc_npc_owes_you( const trade_state &state, const npc &np ) -> int
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
    if( state.your_balance > np.op_of_u.owed && state.your_balance > np.max_willing_to_owe() ) {
        return std::max( np.op_of_u.owed, np.max_willing_to_owe() );
    }

    // Fair's fair. NPC will remember this debt (or credit they've extended)
    return state.your_balance;
}

auto npc_trading::update_npc_owed( const trade_state &state, npc &np ) -> void
{
    np.op_of_u.owed = calc_npc_owes_you( state, np );
}

// Oh my aching head
// op_of_u.owed is the positive when the NPC owes the player, and negative if the player owes the
// NPC
// cost is positive when the player owes the NPC money for a service to be performed
auto npc_trading::trade( npc &np, int cost, const std::string &deal ) -> bool
{
    // Only allow actual shopkeepers to refresh their inventory like this
    if( np.is_shopkeeper() ) {
        np.shop_restock();
    }
    //np.drop_items( np.weight_carried() - np.weight_capacity(),
    //               np.volume_carried() - np.volume_capacity() );
    np.drop_invalid_inventory();

    auto state = trade_state{};
    npc_trading::setup_trade_state( state, cost, np );
    auto trade_win = trading_window( state );

    const auto traded = trade_win.perform_trade( np, deal );
    if( traded ) {
        auto practice = 0;

        npc_trading::transfer_items( state.yours, g->u, np, false );
        npc_trading::transfer_items( state.theirs, np, g->u, true );

        // NPCs will remember debts, to the limit that they'll extend credit or previous debts
        if( !np.will_exchange_items_freely() ) {
            npc_trading::update_npc_owed( state, np );
            g->u.practice( skill_barter, practice / 10000 );
        }
    }
    return traded;
}
