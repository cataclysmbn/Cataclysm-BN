#include "../src/map/map.h"
#include "activity_skip_wait_helpers.h"
#include "cata_utility.h"
#include "catch/catch.hpp"
#include "game.h"
#include "options.h"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <string>
#include <vector>

namespace {

auto env_or(const char* name, const std::string& fallback) -> std::string {
    const auto* value = std::getenv(name);
    return value ? value : fallback;
}

/// Parses "NAME=value,NAME=value" into option overrides.
auto override_options(const std::string& list) -> std::vector<std::unique_ptr<override_option>> {
    auto overrides = std::vector<std::unique_ptr<override_option>>{};
    for (auto pos = std::size_t{0}; pos < list.size();) {
        const auto end = std::min(list.find(',', pos), list.size());
        const auto pair = list.substr(pos, end - pos);
        const auto eq = pair.find('=');
        REQUIRE(eq != std::string::npos);
        overrides.push_back(
            std::make_unique<override_option>(pair.substr(0, eq), pair.substr(eq + 1)));
        pos = end + 1;
    }
    return overrides;
}

} // namespace

/// Waits PROFILE_WAIT_MINUTES (default 60) in the field and town scenes through the wait activity
/// on a map of production size. Prints one line per scene with milliseconds per calendar turn.
/// PROFILE_WAIT_SCENE=field|town runs one scene. PROFILE_OPTS="NAME=value,..." overrides options.
/// A scene where the avatar dies or the wait ends early fails and prints no result line.
TEST_CASE("activity skip wait timing", "[.][benchmark][activity]") {
    const auto minutes = std::stoi(env_or("PROFILE_WAIT_MINUTES", "60"));
    const auto only_scene = env_or("PROFILE_WAIT_SCENE", "");
    const auto skip_options = skip_window_options{};
    // Restore the globals cached from options and the map size after the overrides below are
    // undone.
    const auto recache = on_out_of_scope([] {
        get_options().cache_to_globals();
        g->resize_reality_bubble_to(get_option<int>("REALITY_BUBBLE_SIZE"));
    });
    const auto extra_options = override_options(env_or("PROFILE_OPTS", ""));
    get_options().cache_to_globals();
    // Test processes start with a map of MAPSIZE; the game sizes the map to the bubble.
    g->resize_reality_bubble_to(get_option<int>("REALITY_BUBBLE_SIZE"));

    for (const auto& [scene, name] :
         {std::pair{wait_scene::field, "field"}, std::pair{wait_scene::town, "town"}}) {
        if (!only_scene.empty() && only_scene != name) { continue; }
        setup_wait_scene(scene);
        REQUIRE(get_map().getmapsize() == g_mapsize);
        const auto start_turn = calendar::turn;

        const auto start = std::chrono::steady_clock::now();
        const auto state = wait_through_activity(time_duration::from_minutes(minutes));
        const auto ms =
            std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start)
                .count();
        if (!state) {
            FAIL_CHECK(name << ": " << state.error());
            continue;
        }

        const auto turns = to_turns<int>(state->turn - start_turn);
        std::printf("PROFILE wait scene=%s turns=%d calls=%d total_ms=%.1f per_turn_ms=%.4f\n",
                    name, turns, state->do_turn_calls, ms, ms / turns);
    }
}
