#pragma once

#include <vector>

class item;
class Character;
class JsonIn;
class JsonOut;
class map;
struct tripoint;

template<typename T>
class detached_ptr;

namespace pickup
{

struct pick_drop_selection;

/**
 * Returns `false` if the player was presented a prompt and decided to cancel the pickup.
 * `true` in other cases.
 */
bool do_pickup( std::vector<pick_drop_selection> &targets, bool autopickup );
bool query_thief();

enum from_where : int {
    from_cargo = 0,
    from_ground,
    prompt
};

/** Pick up items; 'g' or ',' or via examine() */
void pick_up( const tripoint &p, int min, from_where get_items_from = prompt );
/** Determines the cost of moving an item by a character. */
int cost_to_move_item( const Character &who, const item &it );

/**
 * If character is handling a potentially spillable bucket, gracefully handle what
 * to do with the contents.
 *
 * Returns nullptr if we handled the container and the container if we chose to spill the
 * contents and the container still needs to be put somewhere.
 * @param c Character handling the spillable item
 * @param it item to handle
 * @param m map they are on
 */
detached_ptr<item> handle_spillable_contents( Character &c, detached_ptr<item> &&it, map &m );
} // namespace pickup


