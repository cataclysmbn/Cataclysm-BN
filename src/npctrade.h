#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "units.h"

class Character;
class faction;
class item;
class npc;

class item_pricing
{
    public:

        item_pricing( std::vector<item *>l, float v, int count ) : locs( l ), price( v ) {
            set_values( count );
        }
        void set_values( int ip_count );
        void adjust_values( double adjust, const faction *fac );

        std::vector<item *>locs;
        float price;
        // Whether this is selected for trading
        bool selected = false;
        bool is_container;
        int count = 0;
        int charges = 0;
        int u_has = 0;
        int npc_has = 0;
        int u_charges = 0;
        int npc_charges = 0;
        units::mass weight = 0_gram;
        units::volume vol = 0_ml;
};

namespace npc_trading
{

struct trade_state {
    std::vector<item_pricing> theirs;
    std::vector<item_pricing> yours;
    int your_balance = 0;
    units::volume volume_left = 0_ml;
    units::mass weight_left = 0_gram;
};

auto setup_trade_state( trade_state &state, int cost, npc &np ) -> void;
auto npc_will_accept_trade( const trade_state &state, const npc &np ) -> bool;
auto calc_npc_owes_you( const trade_state &state, const npc &np ) -> int;
auto update_npc_owed( const trade_state &state, npc &np ) -> void;

bool pay_npc( npc &np, int cost );

int cash_to_favor( const npc &, int cash );

void transfer_items( std::vector<item_pricing> &stuff, Character &giver, Character &receiver,
                     bool npc_gives );
double net_price_adjustment( const Character &buyer, const Character &seller );
auto trade( npc &p, int cost, const std::string &deal ) -> bool;
std::vector<item_pricing> init_selling( npc &p );
std::vector<item_pricing> init_buying( Character &buyer, Character &seller, bool is_npc );
} // namespace npc_trading
