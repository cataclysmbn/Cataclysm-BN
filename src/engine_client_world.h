#pragma once

#include "engine_client_state.h"

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

} // namespace engine_client::world
