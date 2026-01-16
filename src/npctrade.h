#pragma once

#include <cstddef>
#include <vector>
#include <string>
#include <utility>
#include <algorithm>
#include <list>

#include "inventory.h"
#include "output.h"
#include "units.h"
#include "cursesdef.h"
#include "translations.h"

class Character;
class faction;
class item;
class npc;
class player;
class ui_adaptor;

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

class trading_window
{
    public:
        trading_window() = default;
        std::vector<item_pricing> theirs;
        std::vector<item_pricing> yours;
        int your_balance = 0;

        void setup_trade( int cost, npc &np );
        bool perform_trade( npc &np, const std::string &deal );
        void update_npc_owed( npc &np );

    private:
        void setup_win( ui_adaptor &ui );
        void update_win( npc &np, const std::string &deal );
        auto show_item_data( size_t index, bool target_is_theirs ) -> void;

        catacurses::window w_head;
        catacurses::window w_them;
        catacurses::window w_you;
        size_t entries_per_page = 0;
        bool focus_them = true; // Is the focus on them?
        size_t them_off = 0, you_off = 0; // Offset from the start of the list
        size_t them_cursor = 0;
        size_t you_cursor = 0;

        inventory temp;
        units::volume volume_left;
        units::mass weight_left;

        int get_var_trade( const item &it, int total_count, int amount_hint );
        bool npc_will_accept_trade( const npc &np ) const;
        int calc_npc_owes_you( const npc &np ) const;
};

namespace npc_trading
{

bool pay_npc( npc &np, int cost );

int cash_to_favor( const npc &, int cash );

void transfer_items( std::vector<item_pricing> &stuff, Character &giver, Character &receiver,
                     bool npc_gives );
double net_price_adjustment( const Character &buyer, const Character &seller );
bool trade( npc &p, int cost, const std::string &deal );
std::vector<item_pricing> init_selling( npc &p );
std::vector<item_pricing> init_buying( Character &buyer, Character &seller, bool is_npc );
} // namespace npc_trading
