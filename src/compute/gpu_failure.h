#pragma once

#include "debug.h"

#include <string>
#include <unordered_set>

namespace cata_gpu {
/// Raises the debug popup for a GPU failure once per message: the map cache is rebuilt every frame,
/// and a modal popup per frame would keep the game from ever becoming playable.
inline auto report_failure_once(const std::string& message) -> void {
    static auto reported = std::unordered_set<std::string>{};
    if (reported.insert(message).second) { debugmsg("%s", message); }
}
} // namespace cata_gpu
