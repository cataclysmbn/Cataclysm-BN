#pragma once

#include <optional>
#include <unordered_set>
#include "coordinates.h"
#include "type_id.h"

class item;
class zone_data;

/// Activity queries include corpse, wood and seed categories without changing sorting priority.
auto zone_activity_points( const zone_type_id &type, const tripoint_abs_ms &where,
                           int range = 10 ) -> std::unordered_set<tripoint_abs_ms>;
auto zone_activity_at( const tripoint_abs_ms &where, const zone_type_id &type )
-> const zone_data *; // *NOPAD*

/// Choose one storage tile, preferring narrower category selections and preserving sorted items.
auto zone_sorting_destination( const item &candidate, const tripoint_abs_ms &where,
                               int range ) -> std::optional<tripoint_abs_ms>;
