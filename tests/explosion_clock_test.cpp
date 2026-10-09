#include "avatar.h"
#include "cached_options.h"
#include "calendar.h"
#include "cata_utility.h"
#include "catch/catch.hpp"
#include "cursesdef.h"
#include "explosion.h"
#include "explosion_queue.h"
#include "explosion_test.h"
#include "game.h"
#include "game_constants.h"
#include "item.h"
#include "json.h"
#include "map/field.h"
#include "map/map.h"
#include "map/mapdata.h"
#include "map_helpers.h"
#include "map_iterator.h"
#include "map_memory.h"
#include "monster.h"
#include "options.h"
#include "options_helpers.h"
#include "rng.h"
#include "sounds.h"
#include "state_helpers.h"
#include "type_id.h"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <locale>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>

namespace {

using explosion_handler::testing::clock_observation;
using explosion_handler::testing::scoped_explosion_clock;

struct pacing_mode {
    std::string name;
    std::string animation_delay;
    std::string skip_after = "0";
    long long redraw_ms = 0;
    bool bypass_test_mode = true;
    bool control_clock = true;
    /// Draw the visible map through the real curses cell renderer on every explosion redraw.
    bool render = false;
    /// Watch from a visible position outside the blast instead of the default out-of-the-way spot.
    bool observer = false;
};

struct fixture_options {
    bool unwind_after_explosion = false;
};

struct fixture_unwind {};

struct fixture_geometry {
    int configured_bubble_size;
    int runtime_bubble_size;
    int runtime_mapsize;
    int actual_mapsize;
    point_abs_sm origin;
    tripoint_abs_ms avatar_position;
    float visible_threshold;
    auto operator<=>(const fixture_geometry&) const -> std::partial_ordering = default; // *NOPAD*
};

auto capture_fixture_geometry() -> fixture_geometry {
    return {
        .configured_bubble_size = get_option<int>("REALITY_BUBBLE_SIZE"),
        .runtime_bubble_size = g_reality_bubble_size,
        .runtime_mapsize = g_mapsize,
        .actual_mapsize = get_map().getmapsize(),
        .origin = get_map().get_abs_sub(),
        .avatar_position = get_avatar().abs_pos(),
        .visible_threshold = g_visible_threshold,
    };
}

auto set_fixture_bubble_size(const int size) -> void {
    const auto target = override_option("REALITY_BUBBLE_SIZE", std::to_string(size));
    if (g_reality_bubble_size == size) {
        // on_options_changed only compares the configured and runtime radii.
        // Force its native rebuild when the test map alone was resized.
        const auto alternate = override_option("REALITY_BUBBLE_SIZE", size == 4 ? "5" : "4");
        g->on_options_changed();
    }
    g->on_options_changed();
}

auto clear_fixture_world() -> void {
    // clear_avatar places the player on this square. Set the base position first
    // so its normal player::setpos does not recenter a temporarily resized map.
    get_avatar().Character::setpos(map_local_to_abs(get_map(), tripoint_bub_ms(60, 60, 0)));
    clear_all_state();
    clear_fields(1);
    clear_items(1);
}

auto restore_fixture_geometry(const fixture_geometry& original) -> void {
    // The general test map can be larger than its runtime bubble. Preserve that
    // incoming geometry, not just the configured radius or the top-left point.
    set_fixture_bubble_size(original.runtime_bubble_size);
    if (get_map().getmapsize() != original.actual_mapsize) {
        get_map().resize(original.actual_mapsize);
    }
    g->load_map(original.origin, false);
    clear_fixture_world();
    get_avatar().Character::setpos(original.avatar_position);
    g_visible_threshold = original.visible_threshold;
}

auto reset_fixture_sounds() -> void {
    // Match the existing sound-test cleanup, including the batch queue and counters.
    sounds::reset_sounds();
    sounds::clear_floodfill_que(true);
    // clear_floodfill_que does not reset this last diagnostic counter.
    get_map().m_sound_cache.sounds_culled_this_turn = 0;
}

auto check_fixture_sounds_are_clear() -> void {
    auto& here = get_map();
    CHECK(here.m_sound_cache.sound_instances.empty());
    // A stale private batch queue would repopulate instances or invalidation counters.
    here.batch_flood_fill_sounds();
    const auto& cache = here.m_sound_cache;
    CHECK(cache.sound_instances.empty());
    CHECK(cache.sound_list_filtered.empty());
    CHECK(sounds::get_footstep_markers().empty());
    const auto counters = std::array{
        cache.sounds_this_turn,
        cache.attempted_monster_sounds,
        cache.attempted_NPC_sounds,
        cache.attempted_movement_sounds,
        cache.attempted_potential_deafening_sounds,
        cache.attempted_non_batch_floodfills,
        cache.batch_flooded_monster_sounds,
        cache.batch_flooded_NPC_sounds,
        cache.invalidated_batch_sounds,
        cache.filtered_sound_lists_made,
        cache.filtered_sound_lists_cleared,
        cache.prior_turn_sound_vector_size,
        cache.sounds_culled_this_turn,
    };
    CHECK(std::ranges::all_of(counters, [](const auto count) { return count == 0; }));
}

auto seed_fixture_sound_state() -> void {
    const auto source = tripoint_bub_ms(60, 60, 0);
    sounds::sound({
        .volume = 50,
        .origin = source,
        .category = sounds::sound_t::movement,
        .description = "cached fixture sound",
        .from_player = true,
    });
    sounds::sound({
        .volume = 140,
        .origin = source,
        .category = sounds::sound_t::combat,
        .description = "queued fixture sound",
        .from_monster = true,
    });
    auto& cache = get_map().m_sound_cache;
    REQUIRE_FALSE(cache.sound_instances.empty());
    REQUIRE(cache.attempted_monster_sounds > 0);
    REQUIRE(cache.attempted_non_batch_floodfills > 0);
    cache.sound_list_filtered[sound_filter_key()] = {0};
    cache.sounds_culled_this_turn = 1;
}

struct fixture_result {
    std::string state;
    std::string rng_state;
    unsigned int next_rng = 0;
    clock_observation clock;
};

// Exact relevant authority state, not a whole-game save or a visibility snapshot.
// All loaded x/y cells on the affected z levels, including all flung items.
auto capture_state() -> std::string {
    auto stream = std::ostringstream{};
    auto json = JsonOut(stream);
    auto& here = get_map();
    json.start_object();
    json.member("map_origin", here.get_abs_sub());
    json.member("map_size", here.getmapsize());
    json.member("runtime_mapsize", g_mapsize);
    json.member("runtime_bubble_size", g_reality_bubble_size);
    json.member("configured_bubble_size", get_option<int>("REALITY_BUBBLE_SIZE"));
    json.member("avatar_position", get_avatar().abs_pos());
    json.member("cells");
    json.start_array();
    for (auto z = -1; z <= 1; ++z) {
        for (const auto& pos : here.points_on_zlevel(z)) {
            json.start_array();
            json.write(pos);
            json.write(here.ter(pos).id().str());
            json.write(here.furn(pos).id().str());
            json.start_array();
            for (const auto& [id, entry] : here.field_at(pos)) {
                json.start_array();
                json.write(id.id().str());
                json.write(entry.get_field_intensity());
                json.write(to_turns<int>(entry.get_field_age()));
                json.write(entry.is_field_alive());
                json.end_array();
            }
            json.end_array();
            json.start_array();
            for (const auto& it : here.i_at(pos)) { it->serialize(json); }
            json.end_array();
            json.end_array();
        }
    }
    json.end_array();
    json.member("monsters");
    json.start_array();
    for (const auto& mon : g->all_monsters()) { mon.serialize(json); }
    json.end_array();
    json.member("remembered");
    json.start_array();
    for (const auto& pos : here.points_on_zlevel(0)) {
        const auto abs = map_local_to_abs(here, pos);
        const auto symbol = get_avatar().get_memorized_symbol(abs);
        const auto& overlay = get_avatar().get_memorized_tile(abs);
        if (symbol != 0 || !overlay.tile.empty()) {
            json.start_array();
            json.write(abs);
            json.write(symbol);
            json.write(overlay.tile);
            json.end_array();
        }
    }
    json.end_array();
    json.member("queue_empty", explosion_handler::get_explosion_queue().empty());
    json.member("queue_count", explosion_handler::get_explosion_queue().get_count());
    json.end_object();
    return stream.str();
}

auto run_fixture(const pacing_mode& mode, const fixture_options& options = {}) -> fixture_result {
    const auto saved_rng = restore_on_out_of_scope(rng_get_engine());
    const auto sound_cleanup = on_out_of_scope(reset_fixture_sounds);
    reset_fixture_sounds();
    const auto original_geometry = capture_fixture_geometry();
    const auto normal_bubble = override_option("REALITY_BUBBLE_SIZE", "4");
    const auto no_mobile_bubble = override_option("ACTIVITY_MOBILE_BUBBLE_SIZE", "0");
    const auto no_idle_bubble = override_option("ACTIVITY_IDLE_BUBBLE_SIZE", "0");
    const auto no_underground_bubble = override_option("UNDERGROUND_BUBBLE_SIZE", "0");
    const auto no_vehicle_bubble = override_option("VEHICLE_BUBBLE_SIZE", "0");
    const auto no_combat_bubble = override_option("COMBAT_BUBBLE_SIZE", "0");
    const auto cleanup = on_out_of_scope([original_geometry]() {
        explosion_handler::get_explosion_queue().clear();
        clear_fixture_world();
        restore_fixture_geometry(original_geometry);
        get_avatar().clear_map_memory();
    });
    clear_fixture_world();
    // Always use the native resize/load paths: options and globals may agree
    // while another test has directly resized the map to a different dimension.
    set_fixture_bubble_size(4);
    g->load_map(point_abs_sm::zero(), false);
    clear_fixture_world();
    move_player_out_of_the_way();
    CHECK(get_map().getmapsize() == g_mapsize);
    CHECK(g_mapsize == 11);
    CHECK(g_reality_bubble_size == get_option<int>("REALITY_BUBBLE_SIZE"));
    CHECK(get_map().get_abs_sub() == point_abs_sm::zero());
    CHECK(get_avatar().abs_pos() == tripoint_abs_ms(71, 71, 0));
    reset_fixture_sounds();
    check_fixture_sounds_are_clear();
    get_avatar().clear_map_memory();
    if (mode.observer) {
        // The observer sees the blast area from outside it, so renderers have knowledge to acquire.
        get_avatar().Character::setpos(map_local_to_abs(get_map(), tripoint_bub_ms(23, 23, 0)));
        // A visible field outside the blast whose glyph is randomized by the curses renderer.
        get_map().add_field(tripoint_bub_ms(24, 24, 0), field_type_id("fd_fatigue"), 1);
    }
    rng_set_engine_seed(424242);
    const auto old_explosions = override_option("OLD_EXPLOSIONS", "false");
    const auto animation = override_option("ANIMATION_DELAY", mode.animation_delay);
    const auto skip = override_option("SKIP_EXPLOSION_ANIMATION_AFTER", mode.skip_after);
    const auto flying_damage = override_option("ITEM_DAMAGE_ON_FLYING_IMPACT", "true");
    const auto distance = override_option("CIRCLEDIST", "true");
    const auto saved_trigdist = restore_on_out_of_scope(trigdist);
    trigdist = true;
    auto& here = get_map();
    const auto center = tripoint_bub_ms(30, 30, 0);
    const auto chain = center + tripoint_rel_ms(2, 0, 0);
    const auto marker = center + tripoint_rel_ms(10, 10, 0);
    for (auto y = 27; y <= 33; ++y) {
        here.ter_set(tripoint_bub_ms(32, y, 0), ter_id("test_t_clock_wall"));
    }
    here.ter_set(chain, ter_id("test_t_clock_chain_wall"));
    for (auto n = 25; n <= 35; ++n) {
        here.ter_set(tripoint_bub_ms(35, n, 0), ter_id("test_t_shrapnel_wall"));
        here.ter_set(tripoint_bub_ms(n, 35, 0), ter_id("test_t_shrapnel_wall"));
    }
    spawn_test_monster("mon_test_clock_flung", center + tripoint_rel_ms(1, 1, 0));
    spawn_test_monster("mon_test_clock_collision", center + tripoint_rel_ms(3, 0, 0));
    for (auto n = 0; n < 4; ++n) {
        here.add_item(center + tripoint_rel_ms(1, 0, 0), item::spawn("test_clock_projectile"));
    }
    here.build_map_cache(0);
    auto& queue = explosion_handler::get_explosion_queue();
    queue.clear();
    auto clock = std::optional<scoped_explosion_clock>{};
    if (mode.control_clock) {
        const auto options = explosion_handler::testing::clock_options{
            .clock_read_ms = 1,
            .redraw_ms = mode.redraw_ms,
            .bypass_test_mode = mode.bypass_test_mode,
            .render = !mode.render ? std::function<auto() -> void>{} : [&here]() {
                // The curses map renderer, as a non-tiles client would run it on each redraw.
                const auto saved_tiles = restore_on_out_of_scope(use_tiles);
                use_tiles = false;
                static const auto window = catacurses::newwin(25, 25, point_zero);
                here.draw(window, get_avatar().bub_pos());
            },
        };
        clock.emplace(options);
    }
    explosion_handler::explosion(center, {.damage = 50, .radius = 4.0f, .fire = true}, nullptr);
    explosion_handler::explosion(marker, {.damage = 10, .radius = 1.0f}, nullptr);
    queue.execute();
    CHECK(queue.empty());
    CHECK(queue.get_count() == 3);
    CHECK(here.ter(chain) == ter_id("test_t_explosion_floor"));
    CHECK(here.get_field(center, fd_fire) != nullptr);
    CHECK(here.getmapsize() == 11);
    CHECK(here.get_abs_sub() == point_abs_sm::zero());
    // Keep the real explosion sounds; both queued loud sounds and immediate cached sounds exist.
    CHECK(here.m_sound_cache.attempted_potential_deafening_sounds > 0);
    CHECK(here.m_sound_cache.attempted_non_batch_floodfills > 0);
    CHECK_FALSE(here.m_sound_cache.sound_instances.empty());
    if (options.unwind_after_explosion) { throw fixture_unwind{}; }
    auto engine_stream = std::ostringstream{};
    engine_stream.imbue(std::locale::classic());
    engine_stream << rng_get_engine();
    // Capture without consuming gameplay RNG; the entire engine and next value are compared.
    auto next_engine = rng_get_engine();
    return {
        .state = capture_state(),
        .rng_state = engine_stream.str(),
        .next_rng = static_cast<unsigned int>(next_engine()),
        .clock = clock ? clock->observation() : clock_observation{},
    };
}

auto write_evidence(const pacing_mode& mode, const fixture_result& result) -> void {
    const auto* directory = std::getenv("CATA_EXPLOSION_CLOCK_ARTIFACT_DIR");
    if (!directory) { return; }
    const auto root = std::filesystem::path(directory);
    std::filesystem::create_directories(root);
    auto state_file = std::ofstream(root / (mode.name + ".state.json"));
    state_file << result.state;
    auto trace_file = std::ofstream(root / (mode.name + ".trace.json"));
    auto json = JsonOut(trace_file, true);
    json.start_object();
    json.member("seed", 424242);
    json.member("animation_delay", mode.animation_delay);
    json.member("skip_after", mode.skip_after);
    json.member("clock_read_ms", 1);
    json.member("redraw_cost_ms", mode.redraw_ms);
    json.member("bypass_test_mode", mode.bypass_test_mode);
    json.member("controlled_clock", mode.control_clock);
    json.member("rng_engine", result.rng_state);
    json.member("next_rng", result.next_rng);
    json.member("simulated_elapsed_ms", result.clock.elapsed_ms);
    json.member("simulated_sleep_ms", result.clock.sleep_ms);
    json.member("simulated_redraw_ms", result.clock.redraw_ms);
    json.member("redraws", result.clock.redraws);
    json.member("trace");
    json.start_array();
    for (const auto& entry : result.clock.trace) {
        json.start_object();
        json.member("kind", entry.kind);
        json.member("position", entry.position);
        json.member("scheduled_time", entry.scheduled_time);
        json.member("relative_time", entry.relative_time);
        json.member("insertion_ordinal", entry.insertion_ordinal);
        json.member("detail", entry.detail);
        json.end_object();
    }
    json.end_array();
    json.end_object();
    REQUIRE(state_file.good());
    REQUIRE(trace_file.good());
}

} // namespace

TEST_CASE("explosion_characterization_clock_scope", "[explosion][explosion_clock]") {
    CHECK_THROWS_AS(scoped_explosion_clock({.clock_read_ms = 0}), std::logic_error);
    CHECK_THROWS_AS(scoped_explosion_clock({.redraw_ms = -1}), std::logic_error);
    {
        const auto clock = scoped_explosion_clock({});
        CHECK_THROWS_AS(scoped_explosion_clock({}), std::logic_error);
        CHECK(clock.observation().trace.empty());
    }
    {
        const auto clock = scoped_explosion_clock({});
        CHECK(clock.observation().elapsed_ms == 0);
    }
    const auto restore_test_mode = restore_on_out_of_scope(test_mode);
    test_mode = false;
    CHECK_THROWS_AS(scoped_explosion_clock({}), std::logic_error);
}

TEST_CASE("explosion_fixture_sound_isolation", "[explosion][explosion_clock][sound]") {
    const auto saved_rng = restore_on_out_of_scope(rng_get_engine());
    const auto cleanup = on_out_of_scope(reset_fixture_sounds);
    reset_fixture_sounds();
    const auto mode = pacing_mode{.name = "sound_isolation", .animation_delay = "0"};
    seed_fixture_sound_state();
    SECTION("normal completion") {
        const auto result = run_fixture(mode);
        CHECK_FALSE(result.state.empty());
        check_fixture_sounds_are_clear();
    }
    SECTION("exception unwinding after real explosions") {
        CHECK_THROWS_AS(run_fixture(mode, {.unwind_after_explosion = true}), fixture_unwind);
        check_fixture_sounds_are_clear();
    }
}

TEST_CASE("explosion_fixture_geometry_isolation", "[explosion][explosion_clock][map]") {
    const auto saved_rng = restore_on_out_of_scope(rng_get_engine());
    const auto original = capture_fixture_geometry();
    const auto cleanup = on_out_of_scope([original]() {
        clear_fixture_world();
        restore_fixture_geometry(original);
        reset_fixture_sounds();
    });
    const auto configured = override_option("REALITY_BUBBLE_SIZE", "4");
    struct incoming_geometry {
        int bubble_size;
        int mapsize;
        point_abs_sm origin;
    };
    const auto contexts = std::array{
        incoming_geometry{.bubble_size = 4, .mapsize = 35, .origin = point_abs_sm::zero()},
        incoming_geometry{.bubble_size = 6, .mapsize = 15, .origin = point_abs_sm(-2, -2)},
        incoming_geometry{.bubble_size = 3, .mapsize = 9, .origin = point_abs_sm(1, 1)},
    };
    const auto mode = pacing_mode{.name = "geometry", .animation_delay = "0"};
    auto reference = std::optional<fixture_result>{};
    for (const auto& context : contexts) {
        CAPTURE(context.bubble_size, context.mapsize, context.origin);
        clear_fixture_world();
        set_fixture_bubble_size(context.bubble_size);
        get_map().resize(context.mapsize);
        g->load_map(context.origin, false);
        clear_fixture_world();
        get_avatar().Character::setpos(tripoint_abs_ms(71, 71, 0));
        const auto before = capture_fixture_geometry();
        CHECK(before.configured_bubble_size == 4);
        CHECK(before.runtime_bubble_size == context.bubble_size);
        CHECK(before.runtime_mapsize == 2 * context.bubble_size + 3);
        CHECK(before.actual_mapsize == context.mapsize);
        CHECK(before.origin == context.origin);
        const auto result = run_fixture(mode);
        CHECK(capture_fixture_geometry() == before);
        check_fixture_sounds_are_clear();
        if (reference) {
            CHECK(result.state == reference->state);
            CHECK(result.rng_state == reference->rng_state);
            CHECK(result.next_rng == reference->next_rng);
            CHECK(result.clock.trace == reference->clock.trace);
        } else {
            reference = result;
        }
        if (&context == &contexts.front()) {
            CHECK_THROWS_AS(run_fixture(mode, {.unwind_after_explosion = true}), fixture_unwind);
            CHECK(capture_fixture_geometry() == before);
            check_fixture_sounds_are_clear();
        }
    }
}

TEST_CASE("curses_map_draw_leaves_gameplay_rng_untouched", "[explosion_clock][map]") {
    clear_all_state();
    clear_fields(1);
    auto& here = get_map();
    const auto spot = get_avatar().bub_pos() + tripoint_rel_ms(2, 0, 0);
    here.add_field(spot, field_type_id("fd_fatigue"), 1);
    here.build_map_cache(0);
    const auto saved_rng = restore_on_out_of_scope(rng_get_engine());
    rng_set_engine_seed(424242);
    const auto before = rng_get_engine();
    const auto saved_tiles = restore_on_out_of_scope(use_tiles);
    use_tiles = false;
    here.draw(catacurses::newwin(25, 25, point_zero), get_avatar().bub_pos());
    CHECK(rng_get_engine() == before);
}

TEST_CASE("explosion_redraw_preserves_gameplay_rng_and_knowledge", "[explosion][explosion_clock]") {
    const auto quiet = run_fixture(
        pacing_mode{.name = "knowledge_quiet", .animation_delay = "0", .observer = true});
    const auto drawn = run_fixture(pacing_mode{
        .name = "knowledge_drawn", .animation_delay = "10", .render = true, .observer = true});
    CHECK(drawn.clock.redraws > 0);
    CHECK(drawn.rng_state == quiet.rng_state);
    CHECK(drawn.next_rng == quiet.next_rng);
    CHECK(drawn.state == quiet.state);
}

TEST_CASE("explosion_logical_time_presentation_invariance", "[explosion][explosion_clock]") {
    namespace ranges = std::ranges;
    const auto modes = std::array{
        pacing_mode{.name = "animated_1", .animation_delay = "1"},
        pacing_mode{.name = "animated_10", .animation_delay = "10"},
        pacing_mode{.name = "nonanimated", .animation_delay = "0"},
        pacing_mode{.name = "skip_after_1", .animation_delay = "10", .skip_after = "1"},
        pacing_mode{.name = "slow_redraw", .animation_delay = "10", .redraw_ms = 100},
        pacing_mode{.name = "test_mode_default", .animation_delay = "10", .bypass_test_mode = false},
    };
    auto results = std::vector<fixture_result>{};
    for (const auto& mode : modes) {
        CAPTURE(mode.name);
        const auto result = run_fixture(mode);
        check_fixture_sounds_are_clear();
        // One repetition is enough to prove repeatability; the other modes are compared to each
        // other.
        const auto repeated = results.empty() ? run_fixture(mode) : result;
        check_fixture_sounds_are_clear();
        CHECK(result.state == repeated.state);
        CHECK(result.rng_state == repeated.rng_state);
        CHECK(result.next_rng == repeated.next_rng);
        CHECK(result.clock.trace == repeated.clock.trace);
        if (!results.empty()) {
            // Authority and causal dispatch/mutation trace are presentation-independent.
            // Clock accounting below is deliberately a separate presentation contract.
            CHECK(result.state == results.front().state);
            CHECK(result.rng_state == results.front().rng_state);
            CHECK(result.next_rng == results.front().next_rng);
            CHECK(result.clock.trace == results.front().clock.trace);
        }
        CHECK(result.clock.elapsed_ms == repeated.clock.elapsed_ms);
        CHECK(result.clock.sleep_ms == repeated.clock.sleep_ms);
        CHECK(result.clock.redraw_ms == repeated.clock.redraw_ms);
        CHECK(result.clock.redraws == repeated.clock.redraws);
        CHECK(result.clock.elapsed_ms >= result.clock.sleep_ms + result.clock.redraw_ms);
        CHECK(result.clock.redraw_ms == result.clock.redraws * mode.redraw_ms);
        const auto& trace = result.clock.trace;
        struct dispatch_key {
            float time;
            std::uint64_t ordinal;
            auto operator<=>(const dispatch_key&) const = default; // *NOPAD*
        };
        auto previous = std::optional<dispatch_key>{};
        auto equal_time_dispatches = 0;
        for (const auto& entry : trace) {
            if (entry.kind == "drain") { previous.reset(); }
            if (!entry.insertion_ordinal) { continue; }
            CHECK(entry.relative_time == entry.scheduled_time);
            const auto key =
                dispatch_key{.time = entry.scheduled_time, .ordinal = *entry.insertion_ordinal};
            if (previous) {
                CHECK(key > *previous);
                equal_time_dispatches += key.time == previous->time;
            }
            previous = key;
        }
        CHECK(equal_time_dispatches > 0);
        CHECK(ranges::any_of(trace, [](const auto& e) { return e.kind == "terrain"; }));
        CHECK(ranges::any_of(trace, [](const auto& e) {
            return e.kind == "movement" && e.detail == "mob";
        }));
        CHECK(ranges::any_of(trace, [](const auto& e) {
            return e.kind == "movement" && e.detail == "item";
        }));
        CHECK(ranges::any_of(trace, [](const auto& e) { return e.kind == "collision"; }));
        CHECK(ranges::any_of(trace, [](const auto& e) { return e.kind == "field_add"; }));
        CHECK(ranges::any_of(trace, [](const auto& e) { return e.kind == "field_remove"; }));
        auto drains = std::vector<tripoint_bub_ms>{};
        for (const auto& entry : trace) {
            if (entry.kind == "drain") { drains.push_back(entry.position); }
        }
        CHECK(
            drains
            == std::vector<tripoint_bub_ms>{
                tripoint_bub_ms(30, 30, 0), tripoint_bub_ms(40, 40, 0),
                tripoint_bub_ms(32, 30, 0)});
        if (mode.animation_delay == "0" || !mode.bypass_test_mode) {
            CHECK(result.clock.redraws == 0);
            CHECK(result.clock.sleep_ms == 0);
        } else {
            CHECK(result.clock.redraws > 0);
            // Each explosion is paced to its last logical timestamp: 10 ms per time unit per delay
            // step.
            auto expected_ms = 0LL;
            auto last_time = 0.0f;
            for (const auto& entry : trace) {
                if (entry.kind == "drain") {
                    expected_ms += static_cast<long long>(
                        last_time * 10.0f * std::stoi(mode.animation_delay));
                    last_time = 0.0f;
                }
                if (entry.insertion_ordinal) {
                    last_time = std::max(last_time, entry.scheduled_time);
                }
            }
            expected_ms += static_cast<long long>(
                last_time * 10.0f * std::stoi(mode.animation_delay));
            if (mode.skip_after == "0") {
                CHECK(result.clock.elapsed_ms >= expected_ms);
                CHECK(expected_ms > 0);
                if (mode.redraw_ms == 0) { CHECK(result.clock.sleep_ms > 0); }
            }
        }
        write_evidence(mode, result);
        results.push_back(result);
    }
    // New owner-approved logical reference, not an old-build outcome/RNG golden.
    // Preserve numeric outputs as build-identified diagnostics only.
    for (auto n = std::size_t{1}; n < results.size(); ++n) {
        std::cout << "explosion-clock " << modes[0].name << " vs " << modes[n].name
                  << ": state_equal=" << (results[0].state == results[n].state)
                  << " rng_equal=" << (results[0].rng_state == results[n].rng_state)
                  << " next_rng=" << results[n].next_rng << '\n';
    }
    const auto default_mode =
        pacing_mode{.name = "uninstrumented", .animation_delay = "10", .control_clock = false};
    const auto uninstrumented = run_fixture(default_mode);
    check_fixture_sounds_are_clear();
    CHECK(uninstrumented.state == results[2].state);
    CHECK(uninstrumented.rng_state == results[2].rng_state);
    CHECK(uninstrumented.next_rng == results[2].next_rng);
    CHECK(uninstrumented.clock.trace.empty());
    write_evidence(default_mode, uninstrumented);
    CHECK(results[2].state == results[5].state);
    CHECK(results[2].rng_state == results[5].rng_state);
    CHECK(results[2].next_rng == results[5].next_rng);
    // Pacing is proportional to the delay and the skip threshold really suppresses redraws.
    CHECK(results[1].clock.elapsed_ms > results[0].clock.elapsed_ms);
    CHECK(results[3].clock.redraws > 0);
    CHECK(results[3].clock.redraws < results[1].clock.redraws);
    CHECK(results[3].clock.elapsed_ms < results[1].clock.elapsed_ms);
    CHECK(ranges::any_of(results[1].clock.trace, [](const auto& e) {
        return e.kind == "item_impact";
    }));
}
