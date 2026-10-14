#pragma once

#include "coordinates.h"
#include "engine_client_state.h"
#include "line.h"
#include "color.h"
#include "type_id.h"

#include <set>
#include <string>
#include <vector>

class avatar;
class Creature;
class item;
class map;

namespace engine_client
{
struct snapshot;
} // namespace engine_client

/// What the avatar currently sees and remembers, as the owned `world_state` of the 0.1 contract.
///
/// Register once with `engine_client::process_session().set_world_capture( &capture_world )`; the
/// session calls it on the game thread at every input boundary and derives diffs and events itself.
///
/// Knowledge comes from map_perception (completed visibility cache and avatar map memory), never from a
/// renderer. Capture neither acquires memory, refreshes caches, draws nor consumes RNG. Cells are sparse:
/// unknown cells are absent, and plain terrain the engine never memorizes (open air) counts as unknown
/// unless something else is on it. Coordinates are absolute map squares with their dimension.
/// Entity IDs are minted on first disclosure and kept while the creature exists.
namespace engine_client::world
{

auto capture_world() -> world_state;
/// The submaps of the reality bubble with a part of a vehicle in them.
auto vehicle_submaps( map &here ) -> std::set<tripoint_bub_sm>;
/// Whether no square of the submap holding `p` can make a cell: it is a uniform block of terrain the engine
/// never memorizes, which `submap::is_uniform` says until any write of terrain, furniture, field, item or
/// trap clears it, and none of `vehicles` is in it. Every square of it is then bare.
auto bare_submap( const map &here, const tripoint_bub_ms &p,
                  const std::set<tripoint_bub_sm> &vehicles ) -> bool;
/// An inventory or ground row with the items it stands for.
struct item_stack {
    inventory_entry row;
    std::vector<item *> items;
};
struct avatar_item_stacks {
    /// The wielded item, the worn items, then the carried stacks.
    std::vector<item_stack> inventory;
    /// The stacks on the avatar's square.
    std::vector<item_stack> ground;
};
auto item_stacks( map &here, const avatar &you ) -> avatar_item_stacks;
/// The sections of `capture_world` whose text the game words (the avatar and the environment) and no other, for a
/// locale the caller has scoped. They equal those of `capture_world` but for their wording.
auto capture_worded() -> world_state;
/// What `capture_world` would capture, as a patch over `previous`, the state the session published last:
/// every section whole, but of the cells only those that can differ from `previous`. `consecutive` says
/// that `previous` is the state the patch before this one captured, which the patch builds on; without it
/// the patch is exhaustive. Same game-state guarantees as `capture_world`.
///
/// What it leaves out is what nothing that changes it can have touched. Sections whose inputs the engine
/// does not announce (what a visible square holds, the creatures, the avatar's body and belongings,
/// the environment) are captured again every time; a square the avatar does not see is named only when its
/// map memory, the vehicle on it, or whether it is seen has changed.
auto capture_patch( const snapshot &previous, bool consecutive ) -> world_patch;
/// The ids of the tints the client's tileset has. Looks carry the keys among them that apply to them.
auto set_tint_ids( std::set<std::string> ids ) -> void;
/// The absolute square of a reality-bubble square, in the loaded map's dimension.
auto position_at( const tripoint_bub_ms &p ) -> position;
/// The native color name of `color`.
auto color_name( const nc_color &color ) -> std::string;
/// A direction as the native game names it. The id is its horizontal part (`north`, `southwest`, ..., `center` for none) followed by
/// `_above` or `_below`; straight up and down are `above` and `below`. The name is localized for the connection.
auto heading_of( direction dir ) -> heading;
/// The opaque ID of a creature the avatar sees; minted on the first disclosure, like the entities of the snapshot.
/// Empty for a creature the game does not track (a fake turret NPC, a monster not yet placed).
auto entity_id( const Creature &critter ) -> std::string;
/// The IDs of the creatures a snapshot would disclose now.
auto disclosed_entity_ids() -> std::set<std::string>;
/// Whether the creature of an ID given out before is still in the game and not dead.
auto entity_alive( const std::string &id ) -> bool;
/// Appearance from game data, as a cell layer publishes it without a shape.
auto item_appearance( const item &thing ) -> look;
auto terrain_appearance( const ter_id &id ) -> look;
auto furniture_appearance( const furn_id &id ) -> look;

} // namespace engine_client::world
