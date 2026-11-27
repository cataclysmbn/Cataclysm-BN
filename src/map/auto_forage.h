#pragma once

#include <cstdint>
#include <string_view>

enum class forage_class : std::uint8_t;

enum class auto_forage_target : int {
    none,
    terrain,
    furniture,
};

/// @p option is the AUTO_FORAGING value ("off", "bushes", "trees", "flowers"
/// or "both"). Terrain is checked for bushes and trees, furniture for flowers;
/// terrain wins when both match.
auto pick_auto_forage_target(forage_class terrain, forage_class furniture, std::string_view option)
    -> auto_forage_target;
