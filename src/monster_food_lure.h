#pragma once

#include <optional>
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

/// Collects comestible tiles around a single monster.
auto collect_lure_tiles_near( map &here, const monster &mon ) -> lure_snapshot;

/// Whether `mon` eats `food`. Pets only eat pet food lying on a `PET_FEEDER`.
auto wants_food( const monster &mon, const item &food, bool on_feeder ) -> bool;

/// Whether `who` wields pet food that draws the domestic `mon` toward them.
auto is_lured_by_held_food( const monster &mon, const Character &who ) -> bool;

struct lure_goal_query {
    const monster &mon;
    std::span<const lure_tile> tiles;
    /// Result of is_lured_by_held_food() for the avatar.
    bool lured_by_player = false;
    tripoint_bub_ms player_pos;
};

/// Where food draws the monster: its own food tile first, then a holder of pet food, then the
/// nearest visible food tile.
auto pick_lure_goal( const lure_goal_query &query ) -> std::optional<tripoint_bub_ms>;

/// Whether the monster should stay put instead of wandering, e.g. next to someone holding food.
auto holds_position( const monster &mon ) -> bool;

} // namespace food_lure
