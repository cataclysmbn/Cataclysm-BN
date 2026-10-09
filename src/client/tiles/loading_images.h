#pragma once

#include <string>
#include <vector>

namespace game_client::tiles {

/// Randomizes loading images without advancing the simulation RNG.
auto shuffle_loading_image_paths(std::vector<std::string>& paths) -> void;

} // namespace game_client::tiles
