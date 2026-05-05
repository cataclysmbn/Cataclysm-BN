#pragma once

#include <vector>

#include "detached_ptr.h"
#include "type_id.h"

class item;

using recovered_stored_ammo = std::vector<detached_ptr<item>>;

/// Controls what happens to stored charges that cannot be represented as a physical ammo item.
///
/// Most ammo maps 1 stored charge to 1 item charge.  Plutonium fuel cells are different:
/// vehicle parts and tools store plutonium energy in `PLUTONIUM_CHARGES` units, while the
/// inventory item stores whole fuel cells.  A partial cell must never be spawned as a
/// zero-charge item.  Callers that consume the container should discard the remainder; callers
/// that are only unloading complete units should preserve it.
enum class stored_ammo_remainder_handling {
    preserve,
    discard
};

/// Returns how many physical item charges can be made from stored ammo charges.
///
/// This is the only conversion point from abstract stored ammo charges to item charges.  It
/// intentionally rounds plutonium fuel down to complete cells so partial plutonium cannot become
/// a ghost inventory item.
auto stored_ammo_item_charges( const itype_id &ammo, int stored_charges ) -> int;

/// Returns how many stored charges should be removed for recovered physical item charges.
auto stored_ammo_charges_for_items( const itype_id &ammo, int item_charges ) -> int;

/// Spawns the physical ammo item represented by stored charges, or returns null if none exists.
auto spawn_stored_ammo( const itype_id &ammo, int stored_charges ) -> detached_ptr<item>;

/// Removes recoverable stored ammo from an item and returns physical ammo items.
///
/// This handles guns, tools, and magazines through the same conversion path.  Use `discard` when
/// the source item is being destroyed (crafting/disassembly/salvage).  Use `preserve` when the
/// source remains in the world and only complete recoverable units should be removed.
auto recover_stored_ammo( item &source,
                          stored_ammo_remainder_handling remainder_handling ) -> recovered_stored_ammo;
