#pragma once
#include <memory>
struct loading_image_selection_state;

/// Native image resources belong to the renderer, independently of loading menus.
class loading_image_renderer
{
    public:
        virtual ~loading_image_renderer() = default;
        virtual auto draw( loading_image_selection_state &state ) -> void = 0;
};
namespace game_client
{
using loading_image_factory = auto( * )() -> std::unique_ptr<loading_image_renderer>;
auto set_loading_image_factory( loading_image_factory factory ) -> void;
auto make_loading_image_renderer() -> std::unique_ptr<loading_image_renderer>;
auto install_tiles_loading_images() -> void;
}
