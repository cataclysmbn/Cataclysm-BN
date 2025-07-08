#pragma once

#include "item_stack.h"
#include "map.h"
#include "point.h"

class item;
class monster;
class vehicle;

enum liquid_dest : int {
    LD_NULL,
    LD_CONSUME,
    LD_ITEM,
    LD_VEH,
    LD_KEG,
    LD_GROUND
};

struct liquid_dest_opt {
    liquid_dest dest_opt = LD_NULL;
    tripoint pos;
    item *it;
    vehicle *veh = nullptr;
};

// Contains functions that handle liquid
namespace liquid_handler
{
/**
 * Consume / handle all of the liquid. The function can be used when the liquid needs
 * to be handled and can not be put back to where it came from (e.g. when it's a newly
 * created item from crafting).
 * The player is forced to handle all of it, which may required them to pour it onto
 * the ground (if they don't have enough container space available) and essentially
 * loose the item.
 * @return Whether any of the liquid has been consumed. `false` indicates the player has
 * declined all options to handle the liquid (essentially canceled the action) and no
 * charges of the liquid have been transferred.
 * `true` indicates some charges have been transferred (but not necessarily all of them).
 */
void handle_all_liquid( detached_ptr<item> &&liquid, int radius );

/**
 * Consume / handle as much of the liquid as possible in varying ways. This function can
 * be used when the action can be canceled, which implies the liquid can be put back
 * to wherever it came from and is *not* lost if the player cancels the action.
 * It returns when all liquid has been handled or if the player has explicitly canceled
 * the action (use the charges count to distinguish).
 * @return unconsumed liquid, if any
 */
bool consume_liquid( item &liquid, int radius = 0 );
bool consume_liquid( detached_ptr<item> &&liquid, int radius = 0 );
bool consume_liquid( tripoint pos, int radius = 0 );
bool consume_liquid( vehicle *veh, itype_id liquid, int radius = 0 );

/**
* The function consumes moves of the player as needed.
* @param liquid The actual liquid, any liquid remaining will be left in this parameter.
* If no liquid is remaining it will be null.
* @return Whether the user has handled the liquid (at least part of it). `false` indicates
* the user has rejected all possible actions. But note that `true` does *not* indicate any
* liquid was actually consumed, the user may have chosen an option that turned out to be
* invalid (chose to fill into a full/unsuitable container).
* Basically `false` indicates the user does not *want* to handle the liquid, `true`
* indicates they want to handle it.
*/
bool handle_liquid( detached_ptr<item> &&liquid, int radius = 0 );
bool handle_liquid( item &liquid, int radius = 0 );
/**
 * Each of these variants may start a player activity.
 */
bool handle_liquid( tripoint pos, int radius = 0 );
bool handle_liquid( vehicle *veh, int part_id, int radius = 0 );
} // namespace liquid_handler


