#include "cached_options.h"
#include "cata_tiles.h"
#include "character_preview.h"
#include "client_animation.h"
#include "client_presentation.h"
#include "cursesport.h"
#include "debug.h"
#include "dynamic_atlas.h"
#include "game.h"
#include "game_ui.h"
#include "loading_ui_client.h"
#include "mod_tileset.h"
#include "options.h"
#include "output.h"
#include "sdltiles.h"
#include "ui_manager.h"
#include "vehicle/vehicle_preview.h"
#include "world.h"
#include "worldfactory.h"

namespace game_client {
namespace {
class tiles_render_service final: public render_service {
public:
    auto clipboard_available() const -> bool override { return true; }
    auto display_options() const -> std::vector<graphics_option> override {
        auto result = std::vector<graphics_option>{};
        for (const auto& option : cata_tiles::build_display_list()) {
            result.push_back({.id = option.first, .name = option.second});
        }
        return result;
    }
    auto renderer_options() const -> std::vector<graphics_option> override {
        auto result = std::vector<graphics_option>{};
        for (const auto& option : cata_tiles::build_renderer_list()) {
            result.push_back({.id = option.first, .name = option.second});
        }
        return result;
    }
    auto draw_target_tile(game& game, const tripoint_bub_ms& tile) -> bool override {
        if (!use_tiles) { return false; }
        game.draw_highlight(tile);
        return true;
    }
    auto clipboard_text() -> std::string override {
        auto* text = SDL_GetClipboardText();
        auto result = text ? std::string(text) : std::string{};
        SDL_free(text);
        return result;
    }
    auto set_clipboard_text(const std::string& text) -> bool override {
        return SDL_SetClipboardText(text.c_str());
    }
    auto start_text_input() -> void override {
#if defined(__ANDROID__)
        if (get_option<bool>("ANDROID_AUTO_KEYBOARD")) {
            SDL_StartTextInput(get_sdl_window().get());
        }
#endif
    }
    auto stop_text_input() -> void override {
#if defined(__ANDROID__)
        if (get_option<bool>("ANDROID_AUTO_KEYBOARD")) {
            SDL_StopTextInput(get_sdl_window().get());
        }
#endif
    }
    auto window_bounds(const catacurses::window& win) -> rectangle<point> override {
        const auto dimensions = get_window_dimensions(win);
        return rectangle<point>(
            dimensions.window_pos_pixel,
            dimensions.window_pos_pixel + dimensions.window_size_pixel);
    }
    auto cell_bounds(const point origin, const point size) -> rectangle<point> override {
        const auto dimensions = get_window_dimensions(origin, size);
        return rectangle<point>(
            dimensions.window_pos_pixel,
            dimensions.window_pos_pixel + dimensions.window_size_pixel);
    }
    auto suspend_clip() -> std::optional<rectangle<point>> override {
        const auto& renderer = get_sdl_renderer();
        if (!renderer || !SDL_RenderClipEnabled(renderer.get())) { return std::nullopt; }
        auto rect = SDL_Rect{};
        SDL_GetRenderClipRect(renderer.get(), &rect);
        SDL_SetRenderClipRect(renderer.get(), nullptr);
        return rectangle<point>({rect.x, rect.y}, {rect.x + rect.w, rect.y + rect.h});
    }
    auto restore_clip(const std::optional<rectangle<point>>& clip) -> void override {
        const auto& renderer = get_sdl_renderer();
        if (!renderer) { return; }
        if (clip) {
            const auto rect = SDL_Rect{
                .x = clip->p_min.x,
                .y = clip->p_min.y,
                .w = clip->p_max.x - clip->p_min.x,
                .h = clip->p_max.y - clip->p_min.y};
            SDL_SetRenderClipRect(renderer.get(), &rect);
        } else {
            SDL_SetRenderClipRect(renderer.get(), nullptr);
        }
    }
    auto invalidate_framebuffer(const bool force) -> void override {
        game_client::tiles::reinitialize_framebuffer(force);
    }
    auto font_dimensions(point size, const font_space from, const font_space to) -> point override {
        if (from == font_space::map) {
            game_client::tiles::from_map_font_dimension(size.x, size.y);
        }
        if (to == font_space::map) { game_client::tiles::to_map_font_dimension(size.x, size.y); }
        if (to == font_space::overmap) {
            game_client::tiles::to_overmap_font_dimension(size.x, size.y);
        }
        return size;
    }
    auto clear_window(const catacurses::window& win) -> void override {
        game_client::tiles::clear_window_area_native(win);
    }
    auto clear_display() -> void override { clear_sdl_display_buffer_before_redraw(); }
    auto toggle_fullscreen() -> void override { ::toggle_fullscreen_window(); }
    auto scaling_factor() const -> int override { return ::get_scaling_factor(); }
    auto set_zoom(const float zoom, const bool overmap) -> void override {
        const auto& context = overmap ? overmap_tilecontext : tilecontext;
        if (context && context->current_tileset()) {
            if (overmap) {
                context->set_draw_scale(zoom);
            } else {
                ::rescale_tileset(zoom);
            }
        }
    }
    auto screenshot(const std::string& path) -> bool override { return ::save_screenshot(path); }
    auto show_error(const std::string& message) -> void override {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Cataclysm BN", message.c_str(), nullptr);
    }
    auto reset_minimap() -> void override {
        if (tilecontext) { tilecontext->reset_minimap(); }
    }
    auto options_changed() -> void override {
        if (tilecontext) { tilecontext->on_options_changed(); }
    }
    auto reset_mod_tileset() -> void override { ::reset_mod_tileset(); }
    auto load_mod_tileset(const mod_tileset_source& source) -> void override {
        ::load_mod_tileset(source.object, source.source, source.base_path, source.full_path);
    }
    auto load_tileset() -> void override { ::load_tileset(); }
    auto reload_tileset(const std::function<auto(std::string)->void>& out) -> void override {
        reload(
            {.ingame = world_generator && world_generator->active_world,
             .force = true,
             .reset_zoom = false,
             .disable_on_failure = false,
             .report = out});
    }
    auto refresh_tileset(const tileset_refresh_options& options) -> void override {
        if (options.used_tiles_changed) {
            reload(
                {.ingame = options.ingame,
                 .force = options.force_tile_change,
                 .reset_zoom = true,
                 .disable_on_failure = true,
                 .report = [](const std::string& message) {
                     DebugLog(DL::Info, DC::Main) << message;
                 }});
        } else if (options.ingame && pixel_minimap_option && options.pixel_minimap_height_changed) {
            g->mark_main_ui_adaptor_resize();
        }
    }
    auto projectile_sprite(const std::string& id, const tile_category category)
        -> std::optional<std::string> override {
        if (!tilecontext) { return std::nullopt; }
        const auto category_id = category == tile_category::bullet ? C_BULLET : C_ITEM;
        auto lookup = tilecontext->find_tile_looks_like(id, category_id);
        return lookup ? std::optional<std::string>(lookup->id()) : std::nullopt;
    }
    auto dynamic_atlas_available() const -> bool override {
#if defined(DYNAMIC_ATLAS)
        return true;
#else
        return false;
#endif
    }
    auto dump_atlas(const std::string& path) -> void override {
#if defined(DYNAMIC_ATLAS)
        if (tilecontext && tilecontext->current_tileset()) {
            tilecontext->current_tileset()->texture_atlas()->readback_dump(path);
        }
#else
        (void)path;
#endif
    }

private:
    struct reload_request {
        bool ingame;
        bool force;
        bool reset_zoom;
        bool disable_on_failure;
        std::function<auto(std::string)->void> report;
    };
    auto reload(const reload_request& request) -> void {
        if (!tilecontext) { return; }
        auto ui = ui_adaptor(ui_adaptor::disable_uis_below{});
        const auto terrain_name = get_option<std::string>("TILES");
        const auto overmap_name = get_option<std::string>("OVERMAP_TILES");
        const auto mods =
            request.ingame && world_generator && world_generator->active_world
                ? world_generator->active_world->info->active_mod_order
                : std::vector<mod_id>{};
        const auto load = [&](const bool overmap) {
            try {
                if (overmap) {
                    repoint_overmap_tilecontext();
                } else {
                    tilecontext->reinit();
                }
                const auto& context = overmap ? overmap_tilecontext : tilecontext;
                context->load_tileset(
                    overmap ? overmap_name : terrain_name, mods,
                    /*precheck=*/false, /*force=*/request.force, /*pump_events=*/true);
                if (request.reset_zoom) {
                    g->reset_zoom();
                    g->mark_main_ui_adaptor_resize();
                }
                context->do_tile_loading_report(request.report);
            } catch (const std::exception& error) {
                if (overmap) {
                    popup(_("Loading the overmap tileset failed: %s"), error.what());
                } else {
                    popup(_("Loading the tileset failed: %s"), error.what());
                }
                if (request.disable_on_failure) {
                    use_tiles = false;
                    use_tiles_overmap = false;
                }
            }
        };
        load(false);
        if (terrain_name == overmap_name) {
            overmap_tilecontext = tilecontext;
        } else {
            load(true);
        }
    }
};
} // namespace

auto install_tiles_presentation() -> void {
    set_presentation(std::make_unique<tiles_render_service>());
    install_tiles_loading_images();
    install_tiles_vehicle_preview();
    install_tiles_character_preview();
    install_tiles_animation();
}
} // namespace game_client
