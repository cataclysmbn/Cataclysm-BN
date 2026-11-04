#include "iuse_software_screen.h"

namespace iuse_software
{

auto canvas_screen( const canvas_screen_options &opts ) -> engine_client::screen_view
{
    return {
        .title = opts.title,
        .root = engine_client::component{ .id = "canvas:game", .body = game_client::window_canvas( opts.layers ) },
        .refresh_ms = opts.refresh_ms,
    };
}

} // namespace iuse_software
