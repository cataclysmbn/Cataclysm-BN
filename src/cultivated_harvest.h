#pragma once

#include <vector>

#include "detached_ptr.h"

class item;

struct cultivated_harvest_options {
    int plant_count = 1;
    int seed_count = 1;
    bool byproducts = true;
};

/// Generate a harvest with the trait inheritance configured in seed_data.
auto get_cultivated_harvest( const item &seed, const cultivated_harvest_options &options ) ->
std::vector<detached_ptr<item>>;
