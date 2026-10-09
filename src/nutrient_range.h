#pragma once

#include "flat_set.h"
#include "stomach.h"
#include "type_id.h"

class Character;
class item;

struct nutrient_range {
    nutrients minimum;
    nutrients maximum;
};

struct nutrient_range_request {
    const item &food;
    recipe_id recipe;
    cata::flat_set<flag_id> extra_flags = {};
};

/// Native nutrient calculation with local availability, recipe knowledge and detached previews.
/// Recursive ingredient alternatives share this observation context; gameplay caches stay untouched.
auto compute_nutrient_range_for_display( const Character &you,
        const nutrient_range_request &request ) -> nutrient_range;
