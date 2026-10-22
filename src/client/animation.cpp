#include "client_animation.h"

#include <utility>

namespace game_client {
namespace {
auto service = std::unique_ptr<animation_service>{};
}

auto animation() -> animation_service& // *NOPAD*
{
    static auto fallback = animation_service{};
    return service ? *service : fallback;
}

auto set_animation(std::unique_ptr<animation_service> replacement)
    -> std::unique_ptr<animation_service> {
    return std::exchange(service, std::move(replacement));
}
} // namespace game_client
