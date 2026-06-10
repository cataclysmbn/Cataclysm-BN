#pragma once

#include "coordinates.h"
#include "type_id.h"

template<typename T>
class detached_ptr;
class item;

namespace map_mutation_hooks
{

struct furniture_changed_options {
    const dimension_id &dim_id;
    const tripoint_abs_ms &p;
    const furn_id &old_furniture;
    const furn_id &new_furniture;
};

auto on_furniture_changed( const furniture_changed_options &options ) -> void;

struct item_placement_options {
    const dimension_id &dim_id;
    const tripoint_abs_ms &p;
    detached_ptr<item> &item_to_place;
};

auto prepare_item_for_placement( const item_placement_options &options ) -> bool;

} // namespace map_mutation_hooks
