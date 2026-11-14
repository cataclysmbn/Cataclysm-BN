#pragma once

#include "coordinates.h"

class map;

/// True if a rope ladder hangs at `p` from a vehicle above, with only open air between them.
auto is_hanging_rope_visible(const map& here, const tripoint_bub_ms& p) -> bool;
