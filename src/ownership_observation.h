#pragma once

#include "type_id.h"

class Character;
class item;

/// Native nearby-component ownership/attitude gating without repairing items or factions.
auto item_available_for_crafting_observation( const item &value, const Character &actor ) -> bool;

/// Owner as native validation would report it, without removing a missing owner from the item.
auto item_owner_for_observation( const item &value ) -> faction_id;
