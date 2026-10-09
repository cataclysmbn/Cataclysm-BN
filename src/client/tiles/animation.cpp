#if defined(TILES)

#    include "avatar.h"
#    include "cached_options.h"
#    include "cata_tiles.h"
#    include "character.h"
#    include "client_animation.h"
#    include "game.h"
#    include "game_constants.h"
#    include "monster.h"
#    include "mtype.h"
#    include "options.h"
#    include "sdltiles.h"
#    include "travel/travel_destination.h"
#    include "ui_manager.h"

#    include <algorithm>
#    include <array>
#    include <ranges>

namespace game_client {
namespace {
auto get_bullet_dir(const std::vector<tripoint_bub_ms>& trajectory, const size_t index)
    -> direction {
    return index == 0 && trajectory.size() > 1
             ? direction_from(trajectory[index], trajectory[index + 1])
         : index >= 1 && index < trajectory.size()
             ? direction_from(trajectory[index - 1], trajectory[index])
             : direction::NORTH;
}

auto get_bullet_rotation(const direction dir) -> int {
    switch (dir) {
        case direction::NORTH:
            return 0;
        case direction::NORTHEAST:
            return 5;
        case direction::EAST:
            return 3;
        case direction::SOUTHEAST:
            return 8;
        case direction::SOUTH:
            return 2;
        case direction::SOUTHWEST:
            return 7;
        case direction::WEST:
            return 1;
        case direction::NORTHWEST:
            return 6;
        default:
            return 0;
    }
}

auto point_visible(const tripoint_bub_ms& position) -> bool {
    return g->is_in_viewport(position) && g->u.sees(position);
}

class tiles_animation_service final: public animation_service {
public:
    auto draw_explosion(const explosion_animation_options& options) -> bool override {
        if (test_mode) { return true; }
        if (!use_tiles || !tilecontext) { return false; }
        if (!g->is_in_viewport(options.position, -options.radius) || !g->u.sees(options.position)) {
            return true;
        }
        auto radius = 1;
        auto callback = make_shared_fast<game::draw_callback_t>([&options, &radius] {
            tilecontext->init_explosion(options.position, radius, options.name);
        });
        g->add_draw_callback(callback);
        for (radius = 1; radius <= options.radius; ++radius) {
            progress_animation({.multiplier = EXPLOSION_MULTIPLIER});
        }
        if (options.radius > 0) { tilecontext->void_explosion(); }
        return true;
    }

    auto draw_custom_explosion(const custom_explosion_animation_options& options) -> bool override {
        if (!use_tiles || !tilecontext || options.layers == nullptr) { return false; }
        if (test_mode) { return true; }
        auto combined_layer = std::map<tripoint_bub_ms, explosion_tile>{};
        auto callback = make_shared_fast<game::draw_callback_t>([&combined_layer, &options] {
            tilecontext->init_custom_explosion_layer(combined_layer, options.name);
        });
        g->add_draw_callback(callback);
        for (const auto& layer : *options.layers) {
            combined_layer.insert(layer.begin(), layer.end());
            if (std::ranges::any_of(layer, [](const auto& entry) {
                    return g->is_in_viewport(entry.first) && g->u.sees(entry.first);
                })) {
                progress_animation({.multiplier = EXPLOSION_MULTIPLIER});
            }
        }
        tilecontext->void_custom_explosion();
        return true;
    }
    auto draw_bullet(const bullet_animation_options& options) -> bool override {
        if (!use_tiles || !tilecontext || options.trajectory == nullptr) { return false; }
        if (!g->is_in_viewport(options.position) || !g->u.sees(options.position)) { return true; }
        const auto sprite =
            options.custom_sprite.empty()
                ? options.bullet == '*' ? "animation_bullet_normal_0deg"
                : options.bullet == '#' ? "animation_bullet_flame"
                : options.bullet == '`'
                    ? "animation_bullet_shrapnel"
                    : ""
                : options.custom_sprite;
        const auto rotation = get_bullet_rotation(
            get_bullet_dir(*options.trajectory, static_cast<size_t>(options.index)));
        auto callback = make_shared_fast<game::draw_callback_t>([&options, sprite, rotation] {
            tilecontext->init_draw_bullet(options.position, sprite, rotation);
        });
        g->add_draw_callback(callback);
        progress_animation();
        tilecontext->void_bullet();
        return true;
    }

    auto draw_bullet_trajectories(const bullet_trajectories_animation_options& request)
        -> bool override {
        if (!use_tiles || !tilecontext || request.trajectories == nullptr) { return false; }
        const auto& options = *request.trajectories;
        if (options.trajectories.empty()) { return true; }
        const auto sprite =
            options.custom_sprite.empty()
                ? options.bullet == '*' ? "animation_bullet_normal_0deg"
                : options.bullet == '#' ? "animation_bullet_flame"
                : options.bullet == '`'
                    ? "animation_bullet_shrapnel"
                    : ""
                : options.custom_sprite;
        if (options.draw_as_line) {
            auto points = std::vector<tripoint_bub_ms>{};
            auto sprites = std::vector<std::string>{};
            auto rotations = std::vector<int>{};
            for (const auto& trajectory : options.trajectories) {
                if (trajectory.size() < 2) { continue; }
                auto line_points =
                    std::vector<tripoint_bub_ms>(trajectory.begin() + 1, trajectory.end());
                for (size_t point_index = 0; point_index < line_points.size(); ++point_index) {
                    if (!point_visible(line_points[point_index])) { continue; }
                    points.push_back(line_points[point_index]);
                    sprites.push_back(sprite);
                    rotations.push_back(
                        get_bullet_rotation(get_bullet_dir(line_points, point_index)));
                }
            }
            if (points.empty()) { return true; }
            auto callback = make_shared_fast<
                game::draw_callback_t>([&points, &sprites, &rotations] {
                tilecontext->init_draw_bullets(points, sprites, rotations);
            });
            g->add_draw_callback(callback);
            progress_animation({.draw_popup = false});
            tilecontext->void_bullet();
            return true;
        }
        const auto longest = std::ranges::max(
            options.trajectories
            | std::views::transform([](const auto& trajectory) { return trajectory.size(); }));
        for (size_t step = 1; step < longest; ++step) {
            auto points = std::vector<tripoint_bub_ms>{};
            auto sprites = std::vector<std::string>{};
            auto rotations = std::vector<int>{};
            for (const auto& trajectory : options.trajectories) {
                if (step >= trajectory.size() || !g->is_in_viewport(trajectory[step])
                    || !g->u.sees(trajectory[step])) {
                    continue;
                }
                points.push_back(trajectory[step]);
                sprites.push_back(sprite);
                rotations.push_back(get_bullet_rotation(get_bullet_dir(trajectory, step)));
            }
            if (points.empty()) { continue; }
            auto callback = make_shared_fast<
                game::draw_callback_t>([&points, &sprites, &rotations] {
                tilecontext->init_draw_bullets(points, sprites, rotations);
            });
            g->add_draw_callback(callback);
            progress_animation();
            tilecontext->void_bullet();
        }
        return true;
    }
    auto draw_cursor(const cursor_animation_options& options) -> bool override {
        if (!use_tiles || !tilecontext) { return false; }
        tilecontext->init_draw_cursor(options.position);
        return true;
    }

    auto draw_highlight(const cursor_animation_options& options) -> bool override {
        if (!use_tiles || !tilecontext) { return false; }
        tilecontext->init_draw_highlight(options.position);
        return true;
    }

    auto draw_weather(const weather_animation_options& options) -> bool override {
        if (!use_tiles || !tilecontext || options.weather == nullptr) { return false; }
        tilecontext->init_draw_weather(*options.weather, options.weather->wtype->animation.tile);
        return true;
    }

    auto draw_sct() -> bool override {
        if (!use_tiles || !tilecontext) { return false; }
        tilecontext->init_draw_sct();
        return true;
    }

    auto draw_zones(const zones_animation_options& options) -> bool override {
        if (!use_tiles || !tilecontext || options.zones == nullptr) { return false; }
        tilecontext->init_draw_zones(*options.zones);
        return true;
    }

    auto draw_hit_mon(const hit_mon_animation_options& options) -> bool override {
        if (test_mode) { return true; }
        if (!use_tiles || !tilecontext || options.target == nullptr) { return false; }
        auto callback = make_shared_fast<game::draw_callback_t>([&options] {
            tilecontext->init_draw_hit(options.position, options.target->type->id.str());
        });
        g->add_draw_callback(callback);
        progress_animation();
        return true;
    }

    auto draw_hit_player(const hit_player_animation_options& options) -> bool override {
        if (test_mode) { return true; }
        if (!use_tiles || !tilecontext || options.target == nullptr) { return false; }
        const auto type =
            options.target->is_player()
                ? (options.target->male ? "player_male" : "player_female")
                : (options.target->male ? "npc_male" : "npc_female");
        auto callback = make_shared_fast<game::draw_callback_t>([&options, type] {
            tilecontext->init_draw_hit(options.position, type);
        });
        g->add_draw_callback(callback);
        progress_animation();
        return true;
    }

    auto draw_line(const line_animation_options& options) -> bool override {
        if (!use_tiles || !tilecontext || options.points == nullptr) { return false; }
        if (!options.no_reveal && !avatar_knows_travel_destination(g->u, options.position)) {
            return true;
        }
        tilecontext->init_draw_line(options.position, *options.points, "line_target", true);
        return true;
    }

    auto draw_trail_line(const line_animation_options& options) -> bool override {
        if (test_mode) { return true; }
        if (!use_tiles || !tilecontext || options.points == nullptr) { return false; }
        tilecontext->init_draw_line(options.position, *options.points, "line_trail", false);
        return true;
    }

    auto draw_line_of(const draw_sprite_line_options& options) -> bool override {
        if (!use_tiles) { return false; }
        g->refresh_player_visibility_cache_if_needed();
        if (test_mode || !tilecontext) { return true; }
        auto points = std::vector<tripoint_bub_ms>{};
        auto sprites = std::vector<std::string>{};
        auto rotations = std::vector<int>{};
        for (size_t i = 0; i < options.points.size(); ++i) {
            if (!point_visible(options.points[i])) { continue; }
            const auto step = static_cast<int>(i % 8);
            constexpr auto cardinal_rotations = std::array{0, 1, 2, 3};
            constexpr auto diagonal_rotations = std::array{5, 6, 7, 8};
            const auto rotation =
                options.rotate
                    ? (step % 2 == 0 ? cardinal_rotations[step / 2] : diagonal_rotations[step / 2])
                    : get_bullet_rotation(get_bullet_dir(options.points, i));
            points.push_back(options.points[i]);
            sprites.push_back(options.sprite);
            rotations.push_back(rotation);
        }
        if (points.empty()) { return true; }
        auto callback = make_shared_fast<game::draw_callback_t>([&points, &sprites, &rotations] {
            tilecontext->init_draw_bullets(points, sprites, rotations);
        });
        g->add_draw_callback(callback);
        progress_animation({.draw_popup = false});
        tilecontext->void_bullet();
        return true;
    }

    auto draw_radiation_override(const radiation_override_options& options) -> bool override {
        if (!use_tiles || !tilecontext) { return false; }
        tilecontext->init_draw_radiation_override(options.position, options.radiation);
        return true;
    }

    auto draw_terrain_override(const terrain_override_options& options) -> bool override {
        if (!use_tiles || !tilecontext) { return false; }
        tilecontext->init_draw_terrain_override(options.position, options.terrain);
        return true;
    }

    auto draw_furniture_override(const furniture_override_options& options) -> bool override {
        if (!use_tiles || !tilecontext) { return false; }
        tilecontext->init_draw_furniture_override(options.position, options.furniture);
        return true;
    }

    auto draw_graffiti_override(const graffiti_override_options& options) -> bool override {
        if (!use_tiles || !tilecontext) { return false; }
        tilecontext->init_draw_graffiti_override(options.position, options.has_graffiti);
        return true;
    }

    auto draw_trap_override(const trap_override_options& options) -> bool override {
        if (!use_tiles || !tilecontext) { return false; }
        tilecontext->init_draw_trap_override(options.position, options.trap);
        return true;
    }

    auto draw_field_override(const field_override_options& options) -> bool override {
        if (!use_tiles || !tilecontext) { return false; }
        tilecontext->init_draw_field_override(options.position, options.field);
        return true;
    }

    auto draw_item_override(const item_override_options& options) -> bool override {
        if (!use_tiles || !tilecontext) { return false; }
        tilecontext->init_draw_item_override(
            options.position, options.item, options.monster, options.highlight);
        return true;
    }

    auto draw_vehicle_part_override(const vehicle_part_override_options& options) -> bool override {
        if (!use_tiles || !tilecontext) { return false; }
        tilecontext->init_draw_vpart_override(
            options.position, options.part, options.part_mod, options.direction, options.highlight,
            options.mount.xy().raw());
        return true;
    }

    auto draw_below_override(const below_override_options& options) -> bool override {
        if (!use_tiles || !tilecontext) { return false; }
        tilecontext->init_draw_below_override(options.position, options.draw);
        return true;
    }

    auto draw_monster_override(const monster_override_options& options) -> bool override {
        if (!use_tiles || !tilecontext) { return false; }
        tilecontext->init_draw_monster_override(
            options.position, options.monster, options.count, options.more, options.attitude);
        return true;
    }

    auto draw_cone_aoe(const cone_aoe_animation_options& options) -> bool override {
        if (!use_tiles || !tilecontext || options.coverage == nullptr) { return false; }
        if (test_mode) { return true; }
        const auto buckets = bucket_by_distance(options.origin, *options.coverage);
        const auto max_bucket_count = std::min<size_t>(10, options.coverage->size());
        if (max_bucket_count == 0) { return true; }
        const auto waves = optimal_bucketing(buckets, max_bucket_count);
        auto combined_layer = one_bucket{};
        combined_layer.reserve(options.coverage->size());
        auto callback = make_shared_fast<game::draw_callback_t>([&combined_layer, &options] {
            tilecontext->init_draw_cone_aoe(options.origin, combined_layer);
        });
        g->add_draw_callback(callback);
        for (const auto& layer : waves) {
            for (auto& point : combined_layer) { point.val *= 1.0 - (2.0 / max_bucket_count); }
            combined_layer.insert(combined_layer.end(), layer.begin(), layer.end());
            if (std::ranges::any_of(combined_layer, [](const auto& point) {
                    const auto position = tripoint_bub_ms(point.pt);
                    return g->is_in_viewport(position) && g->u.sees(position);
                })) {
                progress_animation();
            }
        }
        tilecontext->void_cone_aoe();
        return true;
    }

    auto minimap_requires_animation() const -> bool override {
        return use_tiles && tilecontext && tilecontext->minimap_requires_animation();
    }

    auto terrain_requires_animation() const -> bool override {
        return use_tiles && tilecontext && tilecontext->terrain_requires_animation();
    }
};
} // namespace

auto install_tiles_animation() -> void {
    set_animation(std::make_unique<tiles_animation_service>());
}
} // namespace game_client

#endif // TILES
