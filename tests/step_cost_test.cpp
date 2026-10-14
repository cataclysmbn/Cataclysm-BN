#if defined(CATA_MCP)

#    include "avatar.h"
#    include "calendar.h"
#    include "cata_utility.h"
#    include "catch/catch.hpp"
#    include "client_command.h"
#    include "client_input.h"
#    include "compute/compute_backend.h"
#    include "engine_client_session.h"
#    include "engine_client_world.h"
#    include "game.h"
#    include "game_constants.h"
#    include "game_session.h"
#    include "input.h"
#    include "map/map.h"
#    include "map_helpers.h"
#    include "map_perception.h"
#    include "options_helpers.h"
#    include "preload_config.h"
#    include "state_helpers.h"
#    include "weather/weather.h"

#    include <algorithm>
#    include <chrono>
#    include <iostream>
#    include <ranges>
#    include <string>
#    include <vector>

namespace {
namespace ec = engine_client;

enum class ground { open, roofed, underground };

auto ground_name(const ground kind) -> std::string {
    return kind == ground::open ? "open ground" : kind == ground::roofed ? "roofed room" : "cellar";
}

/// The avatar on open grass, in a roofed room on the surface, or in a room cut into the rock below
/// it.
auto make_ground(const ground kind) -> tripoint_bub_ms {
    clear_all_state();
    get_avatar().clear_map_memory();
    clear_map();
    make_open_air_uniform();
    auto& here = get_map();
    const auto start =
        tripoint_bub_ms(g_half_mapsize_x, g_half_mapsize_y, kind == ground::underground ? -1 : 0);
    if (kind != ground::open) {
        for (const auto y : std::views::iota(-8, 9)) {
            for (const auto x : std::views::iota(-8, 9)) {
                const auto p = start + tripoint(x, y, 0);
                const auto edge = std::abs(x) == 8 || std::abs(y) == 8;
                const auto inside = kind == ground::roofed ? "t_floor" : "t_rock_floor";
                here.ter_set(p, ter_id(edge ? "t_wall" : inside));
                if (kind == ground::roofed) {
                    here.ter_set(p + tripoint_above, ter_id("t_flat_roof"));
                }
            }
        }
    }
    get_avatar().setpos(start);
    return start;
}

auto median(std::vector<double> values) -> double {
    std::ranges::sort(values);
    return values[values.size() / 2];
}

using steady = std::chrono::steady_clock;

auto ms(const steady::duration elapsed) -> double {
    return std::chrono::duration<double, std::milli>(elapsed).count();
}

} // namespace

/// Milliseconds the parts of one avatar step cost, by reality bubble, compute backend, ground and
/// time of day: `cata_test-mcp "[.step_cost]"` prints them. The visibility refresh is what the
/// step's acquisition pays beyond acquiring map memory again from completed caches; the GPU runs on
/// the device the test binary initialized (CATA_TEST_COMPUTE_ACCELERATION=gpu for hardware).
TEST_CASE("the cost of an avatar step by its parts", "[.step_cost]") {
    const auto device = preload_config::get_compute_accel();
    const auto map_size = get_map().getmapsize();
    // Cleanup resizes to the option value, so pin it to the bubble this test started with.
    const auto bubble =
        override_option("REALITY_BUBBLE_SIZE", std::to_string(g_reality_bubble_size));
    const auto restore = on_out_of_scope([&] {
        preload_config::set_compute_accel(device);
        g->on_options_changed();
        get_map().resize(map_size);
        clear_all_state();
    });
    const auto steps = 40;
    // The default bubble and a small one.
    for (const auto size : {6, 3}) {
        const auto sized = override_option("REALITY_BUBBLE_SIZE", std::to_string(size));
        g->on_options_changed();
        for (const auto accel : {preload_config::compute_accel::cpu, device}) {
            preload_config::set_compute_accel(accel);
            for (const auto kind : {ground::open, ground::roofed, ground::underground}) {
                for (const auto hour : {12, 0}) {
                    const auto start = make_ground(kind);
                    calendar::turn = calendar::turn_zero + time_duration::from_hours(hour);
                    get_weather().weather_id = weather_type_id("clear");
                    g->reset_light_level();
                    game_session::set_running(true);
                    auto& here = get_map();
                    here.invalidate_map_cache(start.z());
                    auto context = input_context{"YESNO"};
                    auto input = game_client::input_context_scope{context, "YESNO"};
                    context.register_action("YES");
                    game_client::begin_input_boundary();
                    auto authority = ec::session{};
                    authority
                        .set_world_capture(&ec::world::capture_world, &ec::world::capture_patch);
                    map_perception::acquire();
                    REQUIRE(authority.publish_boundary());
                    auto acquisition = std::vector<double>{};
                    auto memory = std::vector<double>{};
                    auto publication = std::vector<double>{};
                    for (const auto step : std::views::iota(0, steps)) {
                        get_avatar().setpos(start + tripoint(step % 2 == 0 ? 1 : 0, 0, 0));
                        calendar::turn += 1_turns;
                        const auto stepped = steady::now();
                        map_perception::acquire();
                        const auto acquired = steady::now();
                        map_perception::invalidate_visibility(here);
                        const auto again = steady::now();
                        map_perception::acquire();
                        const auto remembered = steady::now();
                        REQUIRE(authority.publish_boundary());
                        const auto published = steady::now();
                        acquisition.push_back(ms(acquired - stepped));
                        memory.push_back(ms(remembered - again));
                        publication.push_back(ms(published - remembered));
                    }
                    const auto total = median(acquisition);
                    const auto remembering = median(memory);
                    std::cerr << "bubble " << g_reality_bubble_size << " (" << here.getmapsize()
                              << " submaps), " << cata_compute::selected_backend_name() << ", "
                              << ground_name(kind) << ", " << (hour == 12 ? "noon" : "midnight")
                              << ": visibility " << total - remembering << " ms, map memory "
                              << remembering << " ms, publication " << median(publication)
                              << " ms\n";
                }
            }
        }
    }
}

#endif
