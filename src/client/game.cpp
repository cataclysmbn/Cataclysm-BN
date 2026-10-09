#include "game.h"

#include "avatar.h"
#include "cached_options.h"
#include "client_display.h"
#include "client_presentation.h"
#include "cursesdef.h"
#include "cursesport.h"
#include "filesystem.h"
#include "game_ui.h"
#include "input.h"
#include "options.h"
#include "output.h"
#include "ui_manager.h"
#include "world.h"
#include "worldfactory.h"

#include <chrono>
#include <cmath>
#include <ctime>
#include <iomanip>
#include <sstream>

auto game_ui::init_ui() -> void {
    static auto first_init = true;
    if (first_init) {
        pixel_minimap_option = game_client::has_tiles() && get_option<bool>("PIXEL_MINIMAP");
        first_init = false;
    }
    TERMX = get_terminal_width();
    TERMY = get_terminal_height();
    if (game_client::has_tiles()) {
        const auto scale = game_client::presentation().scaling_factor();
        get_options().get_option("TERMINAL_X").setValue(TERMX * scale);
        get_options().get_option("TERMINAL_Y").setValue(TERMY * scale);
        get_options().save();
    } else {
        FULL_SCREEN_HEIGHT = TERMY % 2 ? 25 : 24;
    }
}

auto game::toggle_fullscreen() -> void {
    if (game_client::has_tiles()) {
        game_client::presentation().toggle_fullscreen();
    } else {
        fullscreen = !fullscreen;
        mark_main_ui_adaptor_resize();
    }
}

auto game::toggle_pixel_minimap() -> void {
    if (!game_client::has_tiles()) { return; }
    if (pixel_minimap_option) { game_client::presentation().clear_window(w_pixel_minimap); }
    pixel_minimap_option = !pixel_minimap_option;
    mark_main_ui_adaptor_resize();
}

auto game::reload_tileset(const std::function<auto(std::string)->void>& out) -> void {
    game_client::presentation().reload_tileset(out);
    reapply_zoom();
    mark_main_ui_adaptor_resize();
}

auto game::mouse_edge_scrolling(
    input_context& ctxt, const int speed, const tripoint_rel_ms& last, bool iso)
    -> std::pair<tripoint_rel_ms, tripoint_rel_ms> {
    auto ret = std::make_pair(tripoint_rel_ms::zero(), last);
    if (!game_client::has_tiles()) { return ret; }
    const auto rate = get_option<int>("EDGE_SCROLL");
    if (rate == -1) {
        // Fast return when the option is disabled.
        return ret;
    }
    auto now = std::chrono::steady_clock::now();
    if (now < last_mouse_edge_scroll + std::chrono::milliseconds(rate)) {
        return ret;
    } else {
        last_mouse_edge_scroll = now;
    }
    const auto event = ctxt.get_raw_input();
    if (event.type == input_event_t::mouse) {
        const point threshold(projected_window_width() / 100, projected_window_height() / 100);
        if (event.mouse_pos.x <= threshold.x) {
            ret.first.x() -= speed;
            if (iso) { ret.first.y() -= speed; }
        } else if (event.mouse_pos.x >= projected_window_width() - threshold.x) {
            ret.first.x() += speed;
            if (iso) { ret.first.y() += speed; }
        }
        if (event.mouse_pos.y <= threshold.y) {
            ret.first.y() -= speed;
            if (iso) { ret.first.x() += speed; }
        } else if (event.mouse_pos.y >= projected_window_height() - threshold.y) {
            ret.first.y() += speed;
            if (iso) { ret.first.x() -= speed; }
        }
        ret.second = ret.first;
    } else if (event.type == input_event_t::timeout) {
        ret.first = ret.second;
    }
    return ret;
}

namespace {
constexpr auto MAXIMUM_ZOOM_LEVEL = 4;
constexpr auto MINIMUM_ZOOM_LEVEL = 64;

auto calc_next_zoom(float cur_zoom, int direction) -> float {
    const auto step_count = get_option<int>("ZOOM_STEP_COUNT");
    const auto nth_root_2 = std::pow(2, 1. / step_count);
    // What is our current zoom index:
    // nth_root_2 ** step = cur_zoom
    // log( nth_root_2 ** step ) = log( cur_zoom )
    // step = log(cur_zoom) / log( nth_root_2 )
    const auto expected_cur_ndx = log(cur_zoom) / log(nth_root_2);

    // Round to closest integer
    const auto zoom_level = static_cast<size_t>(std::round(expected_cur_ndx) + direction);

    // calculate next zoom value, and wrap if needed
    auto next_zoom = std::pow(nth_root_2, zoom_level);
    if (next_zoom < MAXIMUM_ZOOM_LEVEL - 0.0001f) {
        next_zoom = MINIMUM_ZOOM_LEVEL;
    } else if (next_zoom > MINIMUM_ZOOM_LEVEL + 0.0001f) {
        next_zoom = MAXIMUM_ZOOM_LEVEL;
    }

    return next_zoom;
}
} // namespace

auto game::zoom_out() -> void {
    if (game_client::has_tiles()) { set_zoom(calc_next_zoom(tileset_zoom, -1)); }
}
auto game::zoom_in() -> void {
    if (game_client::has_tiles()) { set_zoom(calc_next_zoom(tileset_zoom, 1)); }
}
auto game::zoom_out_overmap() -> void {
    if (!game_client::has_tiles()) { return; }
    overmap_tileset_zoom =
        overmap_tileset_zoom > MAXIMUM_ZOOM_LEVEL ? overmap_tileset_zoom / 2 : 64;
    reapply_overmap_zoom();
}
auto game::zoom_in_overmap() -> void {
    if (!game_client::has_tiles()) { return; }
    overmap_tileset_zoom =
        overmap_tileset_zoom == 64 ? MAXIMUM_ZOOM_LEVEL : overmap_tileset_zoom * 2;
    reapply_overmap_zoom();
}
auto game::reapply_overmap_zoom() -> void {
    if (use_tiles && use_tiles_overmap) {
        game_client::presentation().set_zoom(overmap_tileset_zoom, true);
    }
}
auto game::reset_overmap_zoom() -> void {
    overmap_tileset_zoom = DEFAULT_TILESET_ZOOM;
    reapply_overmap_zoom();
}
auto game::reset_zoom() -> void { set_zoom(DEFAULT_TILESET_ZOOM); }
auto game::set_zoom(const float level) -> void {
    if (game_client::has_tiles()) {
        tileset_zoom = level;
        reapply_zoom();
    }
}
auto game::reapply_zoom() -> void {
    if (use_tiles) { game_client::presentation().set_zoom(tileset_zoom, false); }
}
auto game::get_zoom() const -> float { return tileset_zoom; }
auto game::take_screenshot(const std::string& path) const -> bool {
    if (!game_client::has_tiles()) {
        popup(_("Screenshots are unavailable with the active client."));
        return false;
    }
    return game_client::presentation().screenshot(path);
}
auto game::take_screenshot() const -> bool {
    if (!game_client::has_tiles()) {
        popup(_("Screenshots are unavailable with the active client."));
        return false;
    }
    // check that the current '<world>/screenshots' directory exists
    auto map_directory = get_active_world()->info->folder_path() + "/screenshots/";
    assure_dir_exist(map_directory);

    // build file name: <map_dir>/screenshots/[<character_name>]_<date>.png
    // Date format is a somewhat ISO-8601 compliant GMT time date (except for some characters that
    // wouldn't pass on most file systems like ':').
    auto time = std::time(nullptr);
    auto date_buffer = std::stringstream{};
    date_buffer << std::put_time(std::gmtime(&time), "%F_%H-%M-%S_%z");
    const auto tmp_file_name =
        string_format("[%s]_%s.png", get_player_character().get_name(), date_buffer.str());
    const auto file_name = ensure_valid_file_name(tmp_file_name);
    const auto current_file_path = map_directory + file_name;

    // Take a screenshot of the viewport.
    if (take_screenshot(current_file_path)) {
        popup(_("Successfully saved your screenshot to: %s"), map_directory);
        return true;
    } else {
        popup(_("An error occurred while trying to save the screenshot."));
        return false;
    }
}
