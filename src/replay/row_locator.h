#pragma once

#include "client_interaction_data.h"

namespace replay {

/// Row ids of items and diary pages (`item:<n>`, `stack:<n>`, `page:<n>`) are session serials that
/// depend on what was published first, so a recording must not hold them. `locate_row_ids` rewrites
/// each into where the thing is in the simulation (`item:@u:3` is the fourth item the avatar
/// carries), and `resolve_row_ids` turns that back into the ids of the session it is played in. An
/// id that names nothing alive is left as it is.
auto locate_row_ids(game_client::interaction_event event) -> game_client::interaction_event;
auto resolve_row_ids(game_client::interaction_event event) -> game_client::interaction_event;

} // namespace replay
