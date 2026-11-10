#pragma once

class Character;
class item;
struct mtype;

namespace food_lure
{

/// Whether the monster type can be tamed with pet food, which makes it domestic.
auto is_domestic( const mtype &type ) -> bool;

/// Whether `food` carries a pet food tag accepted by `type`.
auto matches_pet_food( const mtype &type, const item &food ) -> bool;

/// Whether any material of the comestible `food` is part of the diet of `type`.
auto matches_diet( const mtype &type, const item &food ) -> bool;

/// Whether `tamer` has one trait of every tamer trait set required by `type`.
auto accepts_tamer( const mtype &type, const Character &tamer ) -> bool;

} // namespace food_lure
