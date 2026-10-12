#pragma once

#include "coordinates.h"
#include "engine_client_state.h"
#include "color.h"
#include "type_id.h"

#include <set>
#include <string>

class Creature;
class item;

/// What the avatar currently sees and remembers, as the owned `world_state` of the 1.0 contract.
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
/// The ids of the tints the client's tileset has. Looks carry the keys among them that apply to them.
auto set_tint_ids( std::set<std::string> ids ) -> void;
/// The absolute square of a reality-bubble square, in the loaded map's dimension.
auto position_at( const tripoint_bub_ms &p ) -> position;
/// The native color name of `color`.
auto color_name( const nc_color &color ) -> std::string;
/// The opaque ID of a creature the avatar sees; minted on the first disclosure, like the entities of the snapshot.
auto entity_id( const Creature &critter ) -> std::string;
/// Appearance from game data, as a cell layer publishes it without a shape.
auto item_appearance( const item &thing ) -> look;
auto terrain_appearance( const ter_id &id ) -> look;
auto furniture_appearance( const furn_id &id ) -> look;

} // namespace engine_client::world
