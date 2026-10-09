#include "client_backend.h"
#include "client_command.h"
#include "client_display.h"
#include "client_input.h"
#include "color_loader.h"
#include "cuboid_rectangle.h"
#include "cursesdef.h"
#include "game.h"
#include "hsv_color.h"
#include "replay/replay.h"
#include "runtime_handlers.h"

template <>
auto color_loader<RGBColor>::from_rgb(const int r, const int g, const int b) -> RGBColor {
    return RGBColor{static_cast<uint8_t>(r), static_cast<uint8_t>(g), static_cast<uint8_t>(b), 255};
}

namespace catacurses {

auto init_interface() -> void { game_client::active_backend().initialize(); }

auto endwin() -> void {
    if (game_client::backend_selected()) { game_client::active_backend().shutdown(); }
}

auto curs_set(const int visibility) -> void {
    if (game_client::backend_selected()) { game_client::active_backend().set_cursor(visibility); }
}

} // namespace catacurses

auto is_mouse_enabled() -> bool {
    return game_client::backend_selected() && game_client::active_backend().capabilities().mouse;
}

auto gamepad_available() -> bool {
    return game_client::backend_selected() && game_client::active_backend().capabilities().gamepad;
}

auto get_terminal_width() -> int {
    return game_client::backend_selected() ? game_client::active_backend().terminal_size().x : 80;
}

auto get_terminal_height() -> int {
    return game_client::backend_selected() ? game_client::active_backend().terminal_size().y : 24;
}

auto projected_window_width() -> int {
    return game_client::backend_selected() ? game_client::active_backend().projected_size().x : 80;
}
auto projected_window_height() -> int {
    return game_client::backend_selected() ? game_client::active_backend().projected_size().y : 24;
}

auto get_scaling_factor() -> int {
    return game_client::backend_selected() ? game_client::active_backend().scaling_factor() : 1;
}

auto handle_resize(const int width, const int height) -> bool {
    if (game_client::backend_selected()) {
        game_client::active_backend().resize(point(width, height));
    }
    return true;
}

auto resize_client_term(const int width, const int height) -> void {
    if (game_client::backend_selected()) {
        game_client::active_backend().resize(point(width, height));
    }
}

auto input_context::get_coordinates(const catacurses::window& capture_win)
    -> std::optional<tripoint_bub_ms> {
    if (!coordinate_input_received || g == nullptr) { return std::nullopt; }
    if (game_client::has_tiles()) {
        return game_client::active_backend().project_input_coordinates(coordinate, capture_win);
    }
    const auto& capture_window = capture_win ? capture_win : g->w_terrain;
    if (!capture_window) { return std::nullopt; }
    const auto view_size = point(getmaxx(capture_window), getmaxy(capture_window));
    const auto win_min = point(getbegx(capture_window), getbegy(capture_window));
    const auto win_bounds = half_open_rectangle<point>(win_min, win_min + view_size);
    if (!win_bounds.contains(coordinate)) { return std::nullopt; }
    auto view_offset = point_bub_ms{};
    if (capture_window == g->w_terrain) { view_offset = g->ter_view_p.xy(); }
    const auto screen_position = coordinate - win_min;
    const auto p = view_offset + screen_position - view_size / 2;
    return tripoint_bub_ms(p, g->get_levz());
}

auto refresh_display() -> void {
    if (game_client::backend_selected()) { game_client::active_backend().present(); }
}

auto input_manager::get_input_event() -> input_event {
    game_client::begin_input_boundary();
    auto result = input_event{};
    if (replay::is_enabled()) {
        const auto active = game_client::active_input_context();
        const auto boundary = replay::input_boundary_metadata{
            .context = std::string(active.category),
            .actions = active.context ? active.context->get_registered_actions_copy()
                                      : std::vector<std::string>{},
            .timeout_ms = get_timeout()};
        try {
            if (const auto recorded = replay::next_input_event(boundary)) {
                game_client::active_backend().pump_events();
                result = *recorded;
            } else {
                result = game_client::active_backend().read_input(get_timeout());
                replay::record_input_event(result, boundary);
            }
        } catch (const replay::completed& /*end*/) { exit_handler(0); }
    } else {
        result = game_client::active_backend().read_input(get_timeout());
    }
    previously_pressed_key = result.type == input_event_t::keyboard ? result.get_first_input() : 0;
    return result;
}

auto input_manager::pump_events() -> void {
    if (game_client::backend_selected()) { game_client::active_backend().pump_events(); }
    previously_pressed_key = 0;
}

auto input_manager::set_timeout(const int timeout) -> void {
    input_timeout = timeout;
    if (game_client::backend_selected()) { game_client::active_backend().set_timeout(timeout); }
}
