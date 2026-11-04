#include "cached_options.h"
#include "color.h"
#include "debug.h"
#include "filesystem.h"
#include "input.h"
#include "loading_ui.h"
#include "loading_ui_client.h"
#include "mod_manager.h"
#include "output.h"
#include "point.h"
#include "sdl_wrappers.h"
#include "sdltiles.h"
#include "string_utils.h"
#include "translations.h"
#include "ui.h"
#include "ui_manager.h"
#include "utils/algo.h"
#include "worldfactory.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <memory>
#include <optional>
#include <random>
#include <ranges>
#include <unordered_set>
#include <vector>

namespace {
struct loading_image_cache {
    std::string path;
    SDL_Texture_Ptr texture;
    point image_size = point_zero;
    bool attempted = false;
};

auto get_loading_image_cache(loading_image_cache& cache, const std::string& loading_image_path)
    -> const loading_image_cache* {
    if (loading_image_path.empty()) {
        cache = {};
        return nullptr;
    }

    if (cache.path != loading_image_path) {
        cache = {};
        cache.path = loading_image_path;
    }

    if (cache.attempted) { return cache.texture ? &cache : nullptr; }

    cache.attempted = true;

    try {
        auto surface = load_image(loading_image_path.c_str());
        cache.image_size = point(surface->w, surface->h);
        cache.texture = CreateTextureFromSurface(get_sdl_renderer(), surface);
    } catch (const std::exception& err) {
        game_client::log_loading_image(
            string_format("failed to load image '%s': %s", loading_image_path, err.what()));
        cache.path = loading_image_path;
        cache.image_size = point_zero;
        cache.attempted = true;
        return nullptr;
    }

    if (!cache.texture) {
        game_client::log_loading_image(
            string_format("failed to create texture for '%s'", loading_image_path));
        cache.path = loading_image_path;
        cache.attempted = true;
        return nullptr;
    }

    return &cache;
}

auto get_loading_image_rect(const point& image_size) -> std::optional<SDL_Rect> {
    const auto window_size = get_sdl_window_size();
    const auto buffer_size = get_sdl_display_buffer_size();
    if (window_size.x <= 0 || window_size.y <= 0 || buffer_size.x <= 0 || buffer_size.y <= 0) {
        return std::nullopt;
    }

    return get_scaled_loading_image_size({.image_size = image_size, .screen_size = window_size})
        .transform([&window_size, &buffer_size](const point& scaled_size) {
            const auto output_rect =
                SDL_Rect{(window_size.x - scaled_size.x) / 2, (window_size.y - scaled_size.y) / 2,
                         scaled_size.x, scaled_size.y};
            return SDL_Rect{
                static_cast<int>(std::lround(
                    static_cast<double>(output_rect.x) * buffer_size.x / window_size.x)),
                static_cast<int>(std::lround(
                    static_cast<double>(output_rect.y) * buffer_size.y / window_size.y)),
                static_cast<int>(std::lround(
                    static_cast<double>(output_rect.w) * buffer_size.x / window_size.x)),
                static_cast<int>(std::lround(
                    static_cast<double>(output_rect.h) * buffer_size.y / window_size.y))};
        });
}

auto get_loading_image_author_pos(const std::string& text) -> std::optional<point> {
    const auto screen_dimensions = get_sdl_display_buffer_size();
    const auto font_size = get_sdl_font_size();
    if (screen_dimensions.x <= 0 || screen_dimensions.y <= 0 || font_size.x <= 0
        || font_size.y <= 0) {
        return std::nullopt;
    }

    const auto text_width = utf8_width(text, true);
    if (text_width <= 0) { return std::nullopt; }

    return point(std::max(0, screen_dimensions.x - (text_width + 1) * font_size.x),
                 std::max(0, screen_dimensions.y - font_size.y));
}

auto draw_loading_image_author(const std::string& author) -> bool {
    if (author.empty()) { return false; }

    const auto text = string_format(_("by %s"), author);
    const auto text_pos = get_loading_image_author_pos(text);
    if (!text_pos) { return false; }

    draw_sdl_text_outlined(
        {.text = text,
         .pos_pixel = *text_pos,
         .text_color = catacurses::white,
         .outline_color = catacurses::black,
         .outline_thickness = 2});
    return true;
}

auto draw_loading_image_author_if_present(const std::optional<std::string>& author) -> bool {
    return author.transform(draw_loading_image_author).value_or(false);
}

struct sdl_render_state_guard {
    const SDL_Renderer_Ptr& renderer;
    point logical_size = point_zero;
    float scale_x = 1.0f;
    float scale_y = 1.0f;
    SDL_Rect viewport = {};
    std::optional<SDL_Rect> clip_rect;
    SDL_RendererLogicalPresentation present;

    explicit sdl_render_state_guard(const SDL_Renderer_Ptr& renderer): renderer(renderer) {
        SDL_GetRenderLogicalPresentation(renderer.get(), &logical_size.x, &logical_size.y, &present);
        SDL_GetRenderScale(renderer.get(), &scale_x, &scale_y);
        SDL_GetRenderViewport(renderer.get(), &viewport);
        if (SDL_RenderClipEnabled(renderer.get())) {
            clip_rect.emplace();
            SDL_GetRenderClipRect(renderer.get(), &*clip_rect);
        }
        SDL_SetRenderClipRect(renderer.get(), nullptr);
        SDL_SetRenderLogicalPresentation(renderer.get(), 0, 0, present);
        SDL_SetRenderScale(renderer.get(), 1.0f, 1.0f);
        SDL_SetRenderViewport(renderer.get(), nullptr);
    }

    ~sdl_render_state_guard() {
        if (logical_size.x > 0 && logical_size.y > 0) {
            SDL_SetRenderLogicalPresentation(
                renderer.get(), logical_size.x, logical_size.y, present);
        } else {
            SDL_SetRenderLogicalPresentation(renderer.get(), 0, 0, present);
            SDL_SetRenderScale(renderer.get(), scale_x, scale_y);
            SDL_SetRenderViewport(renderer.get(), &viewport);
        }
        SDL_SetRenderClipRect(renderer.get(), clip_rect ? &*clip_rect : nullptr);
    }
};

class tiles_loading_image_renderer final: public game_client::selecting_loading_image_renderer {
    loading_image_cache image_cache;
    auto draw_current(loading_image_selection_state& state) -> bool {
        // Advancement wraps around, so one traversal is the most that can find a loadable image.
        for (auto attempts = state.paths.size(); attempts > 0 && !state.current_path.empty();
             --attempts) {
            const auto* const cache = get_loading_image_cache(image_cache, state.current_path);
            if (cache != nullptr) {
                const auto rect = get_loading_image_rect(cache->image_size);
                if (!rect) {
                    game_client::log_loading_image(
                        string_format("failed to calculate rect for '%s'", state.current_path));
                    return false;
                }
                const auto& renderer = get_sdl_renderer();
                const auto render_state_guard = sdl_render_state_guard(renderer);
                clear_sdl_display_buffer();
                SDL_FRect fRect{};
                SDL_RectToFRect(&*rect, &fRect);
                RenderCopy(renderer, cache->texture, nullptr, &fRect);
                draw_loading_image_author_if_present(state.current_author);
                return true;
            }
            if (!game_client::advance_loading_image(state)) { break; }
        }

        return false;
    }

public:
    auto draw(loading_image_selection_state& state) -> void override {
        select(state);
        draw_current(state);
    }
};
} // namespace

namespace game_client {
auto install_tiles_loading_images() -> void {
    set_loading_image_factory([]() -> std::unique_ptr<loading_image_renderer> {
        return std::make_unique<tiles_loading_image_renderer>();
    });
}
} // namespace game_client
