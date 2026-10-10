#pragma once

#include "coordinates.h"
#include "engine_client_state.h"
#include "color.h"

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
/// The absolute square of a reality-bubble square, in the loaded map's dimension.
auto position_at( const tripoint_bub_ms &p ) -> position;
/// The native color name of `color`.
auto color_name( const nc_color &color ) -> std::string;

} // namespace engine_client::world
