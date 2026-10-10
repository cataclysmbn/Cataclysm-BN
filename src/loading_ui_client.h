#pragma once
#include <memory>
#include <optional>
#include <string>
#include <vector>
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
auto has_loading_observer() -> bool;
auto notify_loading( const loading_progress &progress ) -> void;

/// Selects the image of a loading context from the active world's mods, once per renderer.
/// Native renderers draw the result, observers only report it.
class selecting_loading_image_renderer : public loading_image_renderer
{
    private:
        bool selected = false;
    protected:
        auto select( loading_image_selection_state &state ) -> void;
};
/// A renderer that selects the image of each loading context without drawing it.
auto make_selecting_loading_image_renderer() -> std::unique_ptr<loading_image_renderer>;
/// Logs a loading image problem once per message.
auto log_loading_image( const std::string &message ) -> void;
/// Randomizes loading images without advancing the simulation RNG.
auto shuffle_loading_image_paths( std::vector<std::string> &paths ) -> void;
/// Moves to the next image of the state, wrapping around. False when there is none.
auto advance_loading_image( loading_image_selection_state &state ) -> bool;

using loading_image_factory = auto( * )() -> std::unique_ptr<loading_image_renderer>;
auto set_loading_image_factory( loading_image_factory factory ) -> void;
auto make_loading_image_renderer() -> std::unique_ptr<loading_image_renderer>;
auto install_tiles_loading_images() -> void;
}
