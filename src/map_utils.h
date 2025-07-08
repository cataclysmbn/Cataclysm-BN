#pragma once

#include "location_vector.h"
#include "point.h"

class item;

using location_subrange =
    std::ranges::subrange<location_vector<item>::iterator, location_vector<item>::iterator>;

/// @brief Get all items at a given position. If the position is inside a vehicle, it will
///        return the items in the vehicle's cargo.
/// @return An item range at the given position.
auto get_items_at( const tripoint &loc ) -> location_subrange;
