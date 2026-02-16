#pragma once

#include <list>
#include <set>
#include <vector>

#include "point.h"
#include "ret_val.h"
#include "type_id.h"
#include "activity_speed_adapters.h"

class avatar;
class Character;
class inventory;
class item;
class recipe;
struct iuse_location;
struct tool_comp;

enum class cost_adjustment : int;

struct bench_location {
    explicit bench_location( bench_type type, tripoint position )
        : type( type ), position( position )
    {}
    bench_type type;
    tripoint position;
};
template<typename Type>
struct comp_selection;

/**
 * @brief Removes any (removable) ammo and stores it in character's inventory.
 */
void remove_ammo( item &dis_item, Character &who );
/**
 * @brief Removes any (removable) ammo from each item and stores it in character's inventory.
 */
void remove_ammo( std::vector<item *> &dis_items, Character &who );

bench_location find_best_bench( const Character &who, const item &craft );

float workbench_crafting_speed_multiplier( const item &craft, const bench_location &bench );
float morale_crafting_speed_multiplier( const Character &who, const recipe &rec );
float lighting_crafting_speed_multiplier( const Character &who, const recipe &rec );
float crafting_speed_multiplier( const Character &who, const recipe &rec, bool );
float crafting_speed_multiplier( const Character &who, const item &craft,
                                 const bench_location &bench );
void complete_craft( Character &who, item &craft );

namespace crafting
{
std::pair<bench_type, float> best_bench_here( const item &craft, const tripoint &loc,
        bool can_lift );
/**
* Returns the set of book types in crafting_inv that provide the
* given recipe.
* @param c Character whose skills are used to limit the available recipes
* @param crafting_inv Current available items that may contain readable books
* @param r Recipe to search for in the available books
*/
std::set<itype_id> get_books_for_recipe( const Character &c, const inventory &crafting_inv,
        const recipe *r );

/**
 * Returns the set of book types that provide the given recipe.
 */
std::set<itype_id> get_books_for_recipe( const recipe *r );

int charges_for_complete( int full_charges );
int charges_for_starting( int full_charges );
int charges_for_continuing( int full_charges );

/**
 * Returns selected tool component that matches one of the expected ones.
 * @param tools tools to match
 * @param batch size of batch to craft, multiplier on expected charges
 * @param map_inv map inventory to select from
 * @param player_with_inv if not null, character who provides additional inventory
 * @param hotkeys hotkeys available to the menu
 * @param can_cancel can the selection be aborted with no result
 * @param adjustment affects required charges, see @ref cost_adjustment
 */
comp_selection<tool_comp>
select_tool_component( const std::vector<tool_comp> &tools, int batch, const inventory &map_inv,
                       const Character *player_with_inv,
                       bool can_cancel,
                       const std::string &hotkeys,
                       cost_adjustment adjustment );

comp_selection<tool_comp>
select_tool_component( const std::vector<tool_comp> &tools, int batch, const inventory &map_inv,
                       const Character *player_with_inv,
                       bool can_cancel = false );

/** Check if character can disassemble an item using the given crafting inventory. */
ret_val<bool> can_disassemble( const Character &who, const item &obj, const inventory &inv );

/**
 * Prompt for an item to disassemble, then start activity.
 */
bool disassemble( avatar &you );

/**
 * Prompt to disassemble given item, then start activity.
 */
bool disassemble( avatar &you, item &target );

/**
 * Start an activity to disassemble all items in avatar's square.
 */
bool disassemble_all( avatar &you, bool recursively );

/**
 * Complete disassembly of target item.
 */
void complete_disassemble( Character &who, const iuse_location &target, const tripoint &pos );

} // namespace crafting
