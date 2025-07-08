#pragma once

class Character;
class item;

/// Determines whether an item is compact enough to wear multiple of the same type
/// without encumbrance penalty.
auto is_compact( const item &it, const Character &c ) -> bool;


