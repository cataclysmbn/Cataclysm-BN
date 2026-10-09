#pragma once

class Character;
class item;

/// Native nearby-component ownership/attitude gating without repairing items or factions.
auto item_available_for_crafting_observation( const item &value, const Character &actor ) -> bool;
