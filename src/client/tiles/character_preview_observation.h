#pragma once

#include "character.h"

#include <functional>
#include <span>

class avatar;

namespace game_client::tiles {

/// Synchronous diagnostic access to the real preview result, before drawing.
/// References belong to the current display call and must not be retained.
using character_preview_observer =
    std::function<auto(const avatar&, std::span<const Character::overlay_entry>)->void>;

/// Disabled by default. Returns the previous observer for scoped test cleanup.
auto set_character_preview_observer(character_preview_observer observer)
    -> character_preview_observer;

} // namespace game_client::tiles
