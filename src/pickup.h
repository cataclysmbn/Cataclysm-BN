#pragma once

#include <vector>

#include "coordinates.h"
#include "item_stack.h"

class item;
class Character;
class JsonIn;
class JsonOut;
class map;

template<typename T>
class detached_ptr;

namespace pickup
{

struct pick_drop_selection;

struct nearby_pickup_items {
    std::vector<item_stack::iterator> items;
    bool has_ground_items = false;
};

/**
 * Returns `false` if the player was presented a prompt and decided to cancel the pickup.
 * `true` in other cases.
 */
auto do_pickup( std::vector<pick_drop_selection> &targets, bool autopickup ) -> bool;
auto query_thief() -> bool;
auto nearby_items_for_pickup( const tripoint_bub_ms &center ) -> nearby_pickup_items;

enum from_where : int {
    from_cargo = 0,
    from_ground,
    prompt
};

/** Pick up items; 'g' or ',' or via examine() */
auto pick_up( const tripoint_bub_ms &p, int min, from_where get_items_from = prompt ) -> void;
/** Pick up items from the player's tile and every adjacent tile. */
auto pick_up_all_nearby() -> void;
/** Determines the cost of moving an item by a character. */
auto cost_to_move_item( const Character &who, const item &it ) -> int;

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
auto handle_spillable_contents( Character &c, detached_ptr<item> &&it,
                                map &m ) -> detached_ptr<item>;
} // namespace pickup


