#pragma once

#include "coordinates.h"

class map;

/// Returns true when a vehicle rope ladder hangs above `p` with only open air between them.
auto is_hanging_rope_visible(const map& here, const tripoint_bub_ms& p) -> bool;
