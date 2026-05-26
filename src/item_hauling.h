#pragma once

#include "point.h"
#include "item.h"
#include "map.h"

// Checks if items at position are haulable
bool has_haulable_items( const tripoint_bub_ms &pos );

// Checks and returns if an item is haulable
// (e.g returns false if checked item has tag Liquid)
bool is_haulable( const item &item );


