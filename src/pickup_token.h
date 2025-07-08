#pragma once

#include <list>
#include <optional>
#include <vector>
#include "item_handling_util.h"
#include "item_stack.h"
#include "safe_reference.h"

class JsonIn;
class JsonOut;

namespace pickup
{

/** Activity-associated item */
struct act_item {
    /// inventory item
    safe_reference<item> loc;
    /// How many items need to be processed
    int count = 0;
    /// Amount of moves that processing will consume
    int consumed_moves = 0;

    act_item() = default;
    act_item( item &loc, int count, int consumed_moves )
        : loc( &loc ),
          count( count ),
          consumed_moves( consumed_moves ) {}

    void serialize( JsonOut &jsout ) const;
    void deserialize( JsonIn &jsin );
};

struct pick_drop_selection {
    safe_reference<item> target;
    std::optional<int> quantity;
    std::vector<safe_reference<item>> children;

    void serialize( JsonOut &jsout ) const;
    void deserialize( JsonIn &jin );
};

struct stacked_items {
    std::optional<item_stack::iterator> parent;
    std::vector<std::list<item_stack::iterator>> stacked_children;
};

// TODO: This should get information on whether children are consecutive
/** Finds possible parent-child relations in picked up items to save moves */
std::vector<pick_drop_selection> optimize_pickup( const std::vector<item *> &targets,
        const std::vector<int> &quantities );
std::list<act_item> reorder_for_dropping( Character &p, const drop_locations &drop );
std::vector<detached_ptr<item>> obtain_and_tokenize_items( player &p, std::list<act_item> &items );
std::vector<stacked_items> stack_for_pickup_ui( const
        std::vector<item_stack::iterator> &unstacked );
// TODO: This probably shouldn't return raw iterators
std::vector<std::list<item_stack::iterator>> flatten( const std::vector<stacked_items> &stacked );

} // namespace pickup


