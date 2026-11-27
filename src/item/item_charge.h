#pragma once

#include "type_id.h"

class item;

/// Returns the loaded magazine of `it`, or `it` without a magazine.
/// Charge this item instead of `it`, since the type of `it` may list no ammo, such as a mod-defined
/// transform target.
auto charge_target(item& it) -> item&;

/// Adds up to `qty` charges to `charge_target( it )`.
/// Loads `fallback` into an empty target that accepts ammo. Returns the number of charges added.
auto add_charges(item& it, int qty, const itype_id& fallback) -> int;
