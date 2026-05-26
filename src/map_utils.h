#pragma once

#include "location_vector.h"
#include "point.h"

class item;

using location_subrange =
    std::ranges::subrange<location_vector<item>::iterator, location_vector<item>::iterator>;

/// @brief Get all items at a given position. If the position is inside a vehicle, it will
///        return the items in the vehicle's cargo.
/// @return An item range at the given position.
auto get_items_at( const tripoint_abs_ms &loc ) -> location_subrange;

/// Take down deployed furniture and drop its item form.
auto take_down_deployed_furniture( const tripoint_bub_ms &furniture_pos,
                                   const tripoint_bub_ms &drop_pos ) -> void;
