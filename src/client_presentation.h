#pragma once

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "cursesdef.h"
#include "coordinates.h"
#include "point.h"
#include "cuboid_rectangle.h"
#include "client_render_hooks.h"
#include "translations.h"

class game;

namespace game_client
{

enum class font_space { terminal, map, overmap };

struct mod_tileset_source {
    const JsonObject &object;
    const std::string &source;
    const std::string &base_path;
    const std::string &full_path;
};

struct graphics_option {
    std::string id;
    translation name;
};

struct tileset_refresh_options {
    bool used_tiles_changed = false;
    bool pixel_minimap_height_changed = false;
    bool ingame = false;
    bool force_tile_change = false;
};

/// Optional graphical operations. Shared interaction code never owns a native renderer.
/// The default service supplies cell geometry and no graphical capabilities.
class render_service
{
    public:
        virtual ~render_service() = default;
        virtual auto clipboard_available() const -> bool { return false; }
        virtual auto clipboard_text() -> std::string { return {}; }
        virtual auto set_clipboard_text( const std::string &/*text*/ ) -> bool { return false; }
        virtual auto start_text_input() -> void {}
        virtual auto stop_text_input() -> void {}
        virtual auto window_bounds( const catacurses::window &win ) -> rectangle<point>;
        virtual auto cell_bounds( point origin, point size ) -> rectangle<point>;
        virtual auto suspend_clip() -> std::optional<rectangle<point>> { return std::nullopt; }
        virtual auto restore_clip( const std::optional<rectangle<point>> &/*clip*/ ) -> void {}
        virtual auto invalidate_framebuffer( bool /*force*/ ) -> void {}
        virtual auto font_dimensions( point size, font_space /*from*/, font_space /*to*/ ) -> point { return size; }
        virtual auto clear_window( const catacurses::window &/*win*/ ) -> void {}
        virtual auto clear_display() -> void {}
        virtual auto toggle_fullscreen() -> void {}
        virtual auto scaling_factor() const -> int { return 1; }
        virtual auto set_zoom( float /*zoom*/, bool /*overmap*/ ) -> void {}
        virtual auto screenshot( const std::string &/*path*/ ) -> bool { return false; }
        virtual auto show_error( const std::string &/*message*/ ) -> void {}
        virtual auto reset_minimap() -> void {}
        virtual auto options_changed() -> void {}
        virtual auto reset_mod_tileset() -> void {}
        virtual auto load_mod_tileset( const mod_tileset_source &source ) -> void;
        virtual auto load_tileset() -> void {}
        virtual auto reload_tileset( const std::function < auto( std::string ) -> void > &/*out*/ ) ->
        void {}
        virtual auto projectile_sprite( const std::string &/*id*/, tile_category /*category*/ )
        -> std::optional<std::string> { return std::nullopt; }
        virtual auto dynamic_atlas_available() const -> bool { return false; }
        virtual auto dump_atlas( const std::string &/*path*/ ) -> void {}
        virtual auto draw_target_tile( ::game &/*game*/, const tripoint_bub_ms &/*tile*/ ) -> bool { return false; }
        virtual auto display_options() const -> std::vector<graphics_option> { return {}; }
        virtual auto renderer_options() const -> std::vector<graphics_option> { return {}; }
        virtual auto refresh_tileset( const tileset_refresh_options &/*options*/ ) -> void {}
};

auto presentation() -> render_service &; // *NOPAD*
auto set_presentation( std::unique_ptr<render_service> service ) -> void;
/// Called only by the tiles backend; implemented in its private translation unit.
auto install_tiles_presentation() -> void;

} // namespace game_client
