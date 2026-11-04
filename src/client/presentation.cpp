#include "client_presentation.h"
#include "client_presentation_scope.h"
#include "json.h"

namespace game_client {
namespace {
auto service = std::unique_ptr<render_service>{};
}

auto render_service::window_bounds(const catacurses::window& win) -> rectangle<point> {
    return cell_bounds({getbegx(win), getbegy(win)}, {getmaxx(win), getmaxy(win)});
}

auto render_service::cell_bounds(const point origin, const point size) -> rectangle<point> {
    return rectangle<point>(origin, origin + size);
}

auto render_service::load_mod_tileset(const mod_tileset_source& source) -> void {
    source.object.allow_omitted_members();
}

auto presentation() -> render_service& {
    static auto fallback = render_service{};
    return service ? *service : fallback;
}

presentation_scope::presentation_scope(std::unique_ptr<render_service> replacement)
    : previous_(std::move(service)) {
    service = std::move(replacement);
}

presentation_scope::~presentation_scope() noexcept { service.swap(previous_); }

auto set_presentation(std::unique_ptr<render_service> replacement) -> void {
    service = std::move(replacement);
}
} // namespace game_client
