#pragma once

#include <functional>
#include <list>
#include <string>
#include <utility>
#include <optional>

#include "item_handling_util.h"

struct tripoint;

class avatar;
class item;
class player;
class repair_item_actor;
class Character;

using item_filter = std::function<bool( const item & )>;

namespace game_menus
{

namespace inv
{
// item selector for all items in @you's inventory.
item *titled_menu( avatar &you, const std::string &title,
                   const std::string &none_message = "" );
// item selector for items in @you's inventory with a filter
item *titled_filter_menu( const item_filter &filter, avatar &you,
                          const std::string &title, const std::string &none_message = "" );

/**
* @name Customized inventory menus
*
* The functions here execute customized inventory menus for specific game situations.
* Each menu displays only related inventory (or nearby) items along with context dependent information.
* More functions will follow. TODO: update all 'inv_for...()' functions to return @ref item_location instead of
* plain int and move them here.
* @return Either location of the selected item or null location if none was selected.
*/
/*@{*/

void common( avatar &you );
void compare( player &p, const std::optional<tripoint> &offset );
void compare( const item &left, const item &right );
/** Assign (or reassign from existing) letter to item in character's inventory. */
void reassign_letter( Character &who, item &it, int invlet );
/** Prompt to assign (or clear) letter to item in character's inventory. */
void prompt_reassign_letter( Character &who, item &it );
void swap_letters( player &p );

/**
 * Select items to drop.
 * @return A list of pairs of item_location, quantity.
 */
drop_locations multidrop( player &p );


/** Consuming an item. */
item *consume( player &p );
/** Consuming a food item via a custom menu. */
item *consume_food( player &p );
/** Consuming a drink item via a custom menu. */
item *consume_drink( player &p );
/** Consuming a medication item via a custom menu. */
item *consume_meds( player &p );
/** Choosing a container for liquid. */
item *container_for( avatar &you, const item &liquid, int radius = 0 );
/** Item disassembling menu. */
item *disassemble( player &p );
/** Gunmod installation menu. */
item *gun_to_modify( player &p, const item &gunmod );
/** Book reading menu. */
item *read( player &pl );
/** Menu for stealing stuff. */
item *steal( avatar &you, player &victim );
/** Item activation menu. */
item *use( avatar &you );
/** Item wielding/unwielding menu. */
item *wield( avatar &you );
/** Item wielding/unwielding menu. */
item *holster( player &p, item &holster );
/** Choosing a gun to saw down it's barrel. */
item *saw_barrel( player &p, item &tool );
/** Choosing a gun to saw down its stock. */
item *saw_stock( player &p, item &tool );
/** Choose item to wear. */
item *wear( player &p );
/** Choose item to take off. */
item *take_off( avatar &you );
/** Item cut up menu. */
item *salvage( player &p );
/** Repair menu. */
item *repair( player &p, const repair_item_actor *actor, const item *main_tool );
/** Bionic install menu. */
item *install_bionic( player &p, player &patient, bool surgeon = false );
/** Bionic uninstall menu. */
item *uninstall_bionic( player &p, player &patient );
/**Autoclave sterilize menu*/
item *sterilize_cbm( player &p );
/*@}*/

} // namespace inv

} // namespace game_menus


