#include "avatar.h"
#include "cached_options.h"
#include "calendar.h"
#include "cata_utility.h"
#include "catch/catch.hpp"
#include "client_animation.h"
#include "explosion.h"
#include "explosion_queue.h"
#include "game.h"
#include "item.h"
#include "line.h"
#include "map/map.h"
#include "map_helpers.h"
#include "map_perception.h"
#include "options_helpers.h"
#include "presentation_helpers.h"
#include "presentation_sink.h"
#include "rng.h"
#include "sounds.h"
#include "state_helpers.h"
#include "tile_helpers.h"
#include "weather/weather.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <functional>
#include <iostream>
#include <optional>
#include <ranges>
#include <string>
#include <vector>

namespace {

constexpr auto you_at = tripoint_bub_ms(62, 62, 0);
constexpr auto center = tripoint_bub_ms(70, 62, 0);

auto lit_floor() -> void {
    clear_all_state();
    build_test_map(ter_id("t_floor"));
    calendar::turn = calendar::turn_zero + 12_hours;
    get_weather().weather_id = weather_type_id("clear");
    g->reset_light_level();
    sounds::reset_sounds();
    get_avatar().setpos(you_at);
    get_map().build_map_cache(0);
    map_perception::acquire();
}

auto blast() -> explosion_data { return {.damage = 50, .radius = 4.0f}; }

auto fragments() -> explosion_data {
    auto data = blast();
    data.fragment = explosion_handler::shrapnel_from_legacy(50, data.radius);
    return data;
}

struct timing_scenario {
    std::string name;
    /// Builds the scene; not timed.
    std::function<auto()->void> setup = [] {};
    explosion_data data = blast();
    /// The legacy blast, drawn as a custom explosion.
    bool old_explosions = false;
    /// Replaces the explosion at the center, as a shockwave does.
    std::function<auto()->void> run = {};
};

auto scenarios() -> std::vector<timing_scenario> {
    return {
        {.name = "blast"},
        {.name = "fragments", .data = fragments()},
        {.name = "flying stacks",
         .setup =
             [] {
                 for (const auto offset : {tripoint_east, tripoint_north, tripoint_south}) {
                     for (auto n = 0; n < 20; ++n) {
                         get_map().add_item(center + offset, item::spawn("test_clock_projectile"));
                     }
                 }
                 spawn_test_monster("mon_test_clock_flung", center + tripoint_west);
             }},
        {.name = "terrain destruction",
         .setup =
             [] {
                 auto& here = get_map();
                 for (const auto& p : here.points_in_radius(center, 3)) {
                     if (square_dist(p.raw(), center.raw()) == 3) {
                         here.ter_set(p, ter_id("test_t_clock_wall"));
                     }
                 }
             }},
        {.name = "chain reaction",
         .setup =
             [] {
                 for (auto x = center.x() + 2; x <= center.x() + 22; x += 2) {
                     get_map().ter_set(
                         tripoint_bub_ms(x, center.y(), 0), ter_id("test_t_clock_chain_wall"));
                 }
             }},
        {.name = "custom blast (legacy)", .old_explosions = true},
        {.name = "custom blast (shockwave)",
         .run =
             [] {
                 explosion_handler::shockwave(
                     center, {.radius = 8, .force = 10, .stun = 1, .dam_mult = 1}, "fd_smoke",
                     nullptr);
             }},
    };
}

struct timing_mode {
    std::string name;
    /// Tiles registers a sink, here drawing on a hidden software renderer; a headless process has
    /// none.
    bool tiles = false;
    std::string delay;
};

auto milliseconds_of(const timing_scenario& scenario, const timing_mode& mode) -> double {
    lit_floor();
    scenario.setup();
    get_map().build_map_cache(0);
    const override_option animations("ANIMATIONS", "true");
    const override_option
        old_explosions("OLD_EXPLOSIONS", scenario.old_explosions ? "true" : "false");
    const override_option skip("SKIP_EXPLOSION_ANIMATION_AFTER", "0");
    const override_option pace("ANIMATION_DELAY", mode.delay);
    const auto live = restore_on_out_of_scope(test_mode);
    test_mode = false;
    const auto presented = scoped_presentation{};
    auto display = std::optional<tile_context_fixture>{};
#if defined(TILES)
    if (mode.tiles) {
        display.emplace(false);
        REQUIRE(display->valid());
        game_client::install_tiles_animation();
    }
#endif
    const auto uninstall = on_out_of_scope([] {
        explosion_handler::get_explosion_queue().clear();
    });
    rng_set_engine_seed(424242);
    const auto start = std::chrono::steady_clock::now();
    if (scenario.run) {
        scenario.run();
    } else {
        explosion_handler::explosion(center, scenario.data, nullptr);
    }
    explosion_handler::get_explosion_queue().execute();
    return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start)
        .count();
}

/// The nearest-rank percentile of sorted `values`.
auto percentile(const std::vector<double>& values, const double rank) -> double {
    const auto index = static_cast<std::size_t>(std::ceil(rank * values.size())) - 1;
    return values[std::min(index, values.size() - 1)];
}

} // namespace

TEST_CASE("explosion wall time per presentation mode", "[.][explosion_timing]") {
    constexpr auto runs = 10;
    const auto modes = std::vector<timing_mode>{
        {.name = "headless", .delay = "0"},
        {.name = "headless", .delay = "10"},
#if defined(TILES)
        {.name = "tiles", .tiles = true, .delay = "0"},
        {.name = "tiles", .tiles = true, .delay = "10"},
#endif
    };
    for (const auto& scenario : scenarios()) {
        for (const auto& mode : modes) {
            auto times =
                std::views::iota(0, runs)
                | std::views::transform([&](auto) { return milliseconds_of(scenario, mode); })
                | std::ranges::to<std::vector>();
            std::ranges::sort(times);
            std::cout << "explosion-timing\t" << scenario.name << '\t' << mode.name
                      << "\tdelay=" << mode.delay << "\tp50=" << percentile(times, 0.5)
                      << "\tp95=" << percentile(times, 0.95) << "\tmax=" << times.back() << '\n';
        }
    }
}
