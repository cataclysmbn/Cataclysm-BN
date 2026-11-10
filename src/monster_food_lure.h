#pragma once

#include <span>
#include <vector>

#include "coordinates.h"

class Character;
class item;
class map;
class monster;
struct mtype;

namespace food_lure
{

/// Maximum distance at which a monster notices lure food.
inline constexpr auto lure_radius = 8;

/// Comestibles lying on one tile, gathered on the main thread for monster planning.
struct lure_tile {
    tripoint_bub_ms pos;
    /// Whether the tile holds furniture flagged `PET_FEEDER`.
    bool feeder = false;
    /// Valid only during the planning pass that receives the snapshot.
    std::vector<const item *> foods;
};
using lure_snapshot = std::vector<lure_tile>;

/// Whether the monster type can be tamed with pet food, which makes it domestic.
auto is_domestic( const mtype &type ) -> bool;

/// Whether `food` carries a pet food tag accepted by `type`.
auto matches_pet_food( const mtype &type, const item &food ) -> bool;

/// Whether any material of the comestible `food` is part of the diet of `type`.
auto matches_diet( const mtype &type, const item &food ) -> bool;

/// Whether `tamer` has one trait of every tamer trait set required by `type`.
auto accepts_tamer( const mtype &type, const Character &tamer ) -> bool;

/// Whether the monster can be drawn to food at all.
auto seeks_food( const monster &mon ) -> bool;

/// Collects comestible tiles around every monster in `critters` that seeks food.
auto collect_lure_tiles( map &here, std::span<monster *const> critters ) -> lure_snapshot;

} // namespace food_lure
