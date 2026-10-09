#include "loading_ui_client.h"
namespace game_client {
namespace {
auto factory = loading_image_factory{};
}
auto set_loading_image_factory(loading_image_factory value) -> void { factory = value; }
auto make_loading_image_renderer() -> std::unique_ptr<loading_image_renderer> {
    return factory ? factory() : nullptr;
}
} // namespace game_client
