#pragma once

#include <string>

class game;

namespace game_observation
{

/// A structured view containing only player-visible and player-known game data.
struct snapshot {
    std::string json;
    std::string text;
};

/// Capture the current game state on the game thread.
auto capture() -> snapshot;
auto capture( const ::game *current ) -> snapshot;

} // namespace game_observation
