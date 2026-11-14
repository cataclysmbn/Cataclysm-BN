#pragma once

#include <optional>
#include <string>
#include <vector>

namespace game_client
{
/// One loading screen state as every client sees it. `image.path` is relative to the base path.
struct loading_image_info {
    std::string path;
    std::optional<std::string> author;
};
struct loading_progress {
    std::string title;
    std::vector<std::string> entries;
    std::size_t index = 0;
    std::optional<loading_image_info> image;
    bool done = false;
};
/// Receives each loading_ui step on the game thread, and `done` when a loading_ui that showed
/// anything ends. Unset means nobody but the native screen observes loading.
using loading_observer = auto( * )( const loading_progress & ) -> void;
auto set_loading_observer( loading_observer observer ) -> void;
} // namespace game_client
