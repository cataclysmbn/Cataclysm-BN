#pragma once

#include <functional>

class avatar;
class item;

namespace examine_item_menu
{

enum class menu_pos_t {
    left,
    right
};

bool run(
    item &loc,
    const std::function<int()> &func_pos_x,
    const std::function<int()> &func_width,
    menu_pos_t menu_pos
);

/** Hint value used to decide action text color. */
enum class hint_rating : int {
    /** Item should display as gray (action impossible) */
    cant,
    /** Item should display as red (action impossible at the moment) */
    iffy,
    /** Item should display as green (action possible at the moment) */
    good,
    /** Item should display as bright red (kill list) */
    blood
};

hint_rating rate_action_use( const avatar &you, const item &it );
hint_rating rate_action_read( const avatar &you, const item &it );
hint_rating rate_action_eat( const avatar &you, const item &it );
hint_rating rate_action_wear( const avatar &you, const item &it );
hint_rating rate_action_change_side( const avatar &you, const item &it );
hint_rating rate_action_takeoff( const avatar &you, const item &it );
hint_rating rate_action_unload( const avatar &you, const item &it );
hint_rating rate_action_reload( const avatar &you, const item &it );
hint_rating rate_action_mend( const avatar &you, const item &it );
// Needs non-const reference to validate crafting inventory
hint_rating rate_action_disassemble( avatar &you, const item &it );
hint_rating rate_action_salvage( avatar &you, const item &it );

} // namespace examine_item_menu


