#pragma once

#include "coordinates.h"
#include "type_id.h"

namespace map_mutation_hooks
{

struct furniture_changed_options {
    const dimension_id &dim_id;
    const tripoint_abs_ms &p;
    const furn_id &old_furniture;
    const furn_id &new_furniture;
};

auto on_furniture_changed( const furniture_changed_options &options ) -> void;

} // namespace map_mutation_hooks
