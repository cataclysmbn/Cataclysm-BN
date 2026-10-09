#include "cata_utility.h"
#include "catch/catch.hpp"
#if defined(CATA_MCP)
#    include "avatar.h"
#    include "client_memory.h"
#    include "game.h"
#    include "game_constants.h"
#    include "options_helpers.h"
#    include "output.h"
#    include "player_activity.h"
#    include "state_helpers.h"
#endif
#include "input.h"
#include "path_info.h"
#include "replay/replay.h"
#include "rng.h"

#include <atomic>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <utility>

namespace {

struct replay_fixture {
    std::filesystem::path directory;
    std::filesystem::path path;

    replay_fixture() {
        REQUIRE_FALSE(replay::is_enabled());
        static auto counter = std::atomic_uint64_t{0};
        do {
            directory =
                std::filesystem::path(PATH_INFO::user_dir())
                / ("replay-test-" + std::to_string(counter.fetch_add(1)));
        } while (!std::filesystem::create_directory(directory));
        path = directory / "input.jsonl";
    }
    ~replay_fixture() {
        replay::stop();
        std::filesystem::remove_all(directory);
    }
    auto write(const std::string& text) const -> void {
        auto stream = std::ofstream(path, std::ios::binary);
        stream << text;
    }
    auto read() const -> std::string {
        auto stream = std::ifstream(path, std::ios::binary);
        return {std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
    }
};

const auto header = std::string{
    R"({"kind":"header","format":"cataclysm-bn-replay","version":1,"rng_seed":7})"
    "\n"};
const auto timeout_event = std::string{
    R"({"kind":"input","version":1,"type":"timeout","sequence":[],"modifiers":[],"mouse_pos":[0,0],"text":"","edit":"","edit_refresh":false,"boundary":{"context":"GAME","actions":["MOVE_N"]}})"
    "\n"};

} // namespace

TEST_CASE("replay preserves every input field and a clean end marker", "[replay][input]") {
    const auto fixture = replay_fixture{};
    const auto boundary =
        replay::input_boundary_metadata{.context = "GAME", .actions = {"MOVE_N", "TEXT.CONFIRM"}};
    auto events = std::vector<input_event>{input_event{}};
    for (const auto type :
         {input_event_t::keyboard, input_event_t::mouse, input_event_t::gamepad,
          input_event_t::timeout}) {
        auto event = input_event('x', type);
        event.modifiers = {KEY_ESCAPE, KEY_ENTER};
        event.mouse_pos = point(17, 23);
        event.text = "한글";
        event.edit = "composition";
        event.edit_refresh = true;
        events.push_back(event);
    }
    replay::configure_recording(fixture.path.string(), {.rng_seed = 8675309});
    replay::start();
    for (const auto& event : events) { replay::record_input_event(event, boundary); }
    replay::finish();
    CHECK(fixture.read().find("\"kind\":\"end\"") != std::string::npos);
    replay::configure_playback(fixture.path.string(), {.rng_seed = 8675309});
    replay::start();
    CHECK(replay::playback_metadata().rng_seed == 8675309);
    for (const auto& expected : events) {
        const auto actual = replay::next_input_event(boundary);
        REQUIRE(actual);
        CHECK(*actual == expected);
        CHECK(actual->mouse_pos == expected.mouse_pos);
        CHECK(actual->text == expected.text);
        CHECK(actual->edit == expected.edit);
        CHECK(actual->edit_refresh == expected.edit_refresh);
    }
    CHECK(replay::playback_exhausted());
    CHECK_THROWS_AS(replay::next_input_event(boundary), replay::completed);
    replay::finish();
    CHECK_FALSE(replay::is_enabled());
}

TEST_CASE("replay rejects invalid headers without changing RNG", "[replay][input]") {
    const auto fixture = replay_fixture{};
    const auto engine = rng_get_engine();
    for (const auto text :
         {std::string{}, std::string{"{}\n"},
          std::string{
              R"({"kind":"header","format":"cataclysm-bn-replay","version":99,"rng_seed":7})"
              "\n"},
          std::string{
              R"({"kind":"header","format":"cataclysm-bn-replay","version":1,"rng_seed":-1})"
              "\n"}}) {
        fixture.write(text);
        replay::configure_playback(fixture.path.string());
        CHECK_THROWS(replay::start());
        CHECK_FALSE(replay::is_enabled());
        CHECK(rng_get_engine() == engine);
    }
}

TEST_CASE("replay rejects non-integer numeric fields", "[replay][input]") {
    const auto fixture = replay_fixture{};
    const auto valid_end = std::string{
        R"({"kind":"end","version":1,"events":0})"
        "\n"};
    const auto event_prefix = std::string{
        R"({"kind":"input","version":1,"type":"keyboard","sequence":[1],"modifiers":[],"mouse_pos":[0,0],"text":"","edit":"","edit_refresh":false,"boundary":{"context":"GAME","actions":["MOVE_N"],"timeout_ms":-1}})"
        "\n"};
    const auto input_end = std::string{
        R"({"kind":"end","version":1,"events":1})"
        "\n"};
    const auto malformed = std::vector<std::string>{
        R"({"kind":"header","format":"cataclysm-bn-replay","version":1.5,"rng_seed":7})"
        "\n" + valid_end,
        R"({"kind":"header","format":"cataclysm-bn-replay","version":1,"rng_seed":7.9})"
        "\n" + valid_end,
        header
            + R"({"kind":"input","version":1.5,"type":"keyboard","sequence":[1],"modifiers":[],"mouse_pos":[0,0],"text":"","edit":"","edit_refresh":false,"boundary":{"context":"GAME","actions":["MOVE_N"],"timeout_ms":-1}})"
              "\n"
            + input_end,
        header
            + R"({"kind":"input","version":1,"type":"keyboard","sequence":[1.5],"modifiers":[],"mouse_pos":[0,0],"text":"","edit":"","edit_refresh":false,"boundary":{"context":"GAME","actions":["MOVE_N"],"timeout_ms":-1}})"
              "\n"
            + input_end,
        header
            + R"({"kind":"input","version":1,"type":"keyboard","sequence":[1],"modifiers":[1e1],"mouse_pos":[0,0],"text":"","edit":"","edit_refresh":false,"boundary":{"context":"GAME","actions":["MOVE_N"],"timeout_ms":-1}})"
              "\n"
            + input_end,
        header
            + R"({"kind":"input","version":1,"type":"keyboard","sequence":[1],"modifiers":[],"mouse_pos":[0,2.5],"text":"","edit":"","edit_refresh":false,"boundary":{"context":"GAME","actions":["MOVE_N"],"timeout_ms":-1}})"
              "\n"
            + input_end,
        header
            + R"({"kind":"input","version":1,"type":"keyboard","sequence":[1],"modifiers":[],"mouse_pos":[0,0],"text":"","edit":"","edit_refresh":false,"boundary":{"context":"GAME","actions":["MOVE_N"],"timeout_ms":1.5}})"
              "\n"
            + input_end,
        header
            + R"({"kind":"end","version":1,"events":0.5})"
              "\n",
    };
    for (const auto& text : malformed) {
        fixture.write(text);
        replay::configure_playback(fixture.path.string());
        CHECK_THROWS_WITH(replay::start(), Catch::Matchers::Contains("integer"));
        CHECK_FALSE(replay::is_enabled());
    }

    fixture.write(header + event_prefix + input_end);
    replay::configure_playback(fixture.path.string());
    CHECK_NOTHROW(replay::start());
}

TEST_CASE(
    "replay rejects malformed semantic interaction payloads", "[replay][input][interaction]") {
    const auto fixture = replay_fixture{};
    const auto end = std::string{
        R"({"kind":"end","version":1,"events":1})"
        "\n"};
    const auto malformed = std::vector<std::string>{
        R"({"kind":"input","version":1,"type":"interaction","sequence":[],"modifiers":[],"mouse_pos":[0,0],"text":"","edit":"","edit_refresh":false,"interaction":{"operation":"fill","schema_id":"field-schema","target_id":"field:value","value":"text"},"boundary":{"context":"STRING_INPUT","actions":["ANY_INPUT"],"timeout_ms":-1}})"
        "\n",
        R"({"kind":"input","version":1,"type":"interaction","sequence":[],"modifiers":[],"mouse_pos":[0,0],"text":"","edit":"","edit_refresh":false,"interaction":{"operation":"mutate","schema_id":"field-schema"},"boundary":{"context":"STRING_INPUT","actions":["ANY_INPUT"],"timeout_ms":-1}})"
        "\n",
        R"({"kind":"input","version":1,"type":"interaction","sequence":[],"modifiers":[],"mouse_pos":[0,0],"text":"","edit":"","edit_refresh":false,"interaction":{"operation":"cancel","schema_id":""},"boundary":{"context":"STRING_INPUT","actions":["ANY_INPUT"],"timeout_ms":-1}})"
        "\n",
        R"({"kind":"input","version":1,"type":"interaction","sequence":[],"modifiers":[],"mouse_pos":[0,0],"text":"","edit":"","edit_refresh":false,"interaction":{"operation":"set_count","schema_id":"inventory-schema","target_id":"item","value":""},"boundary":{"context":"INVENTORY","actions":["ANY_INPUT"],"timeout_ms":-1}})"
        "\n",
        R"({"kind":"input","version":1,"type":"interaction","sequence":[],"modifiers":[],"mouse_pos":[0,0],"text":"","edit":"","edit_refresh":false,"interaction":{"operation":"set_target","schema_id":"target-schema","target_id":"","value":"","position":{"x":1,"y":1.5,"z":0}},"boundary":{"context":"TARGET","actions":["ANY_INPUT"],"timeout_ms":-1}})"
        "\n",
    };
    for (const auto& event : malformed) {
        fixture.write(header + event + end);
        replay::configure_playback(fixture.path.string());
        CHECK_THROWS(replay::start());
        CHECK_FALSE(replay::is_enabled());
    }
}

TEST_CASE("replay validates empty and mismatched input contexts", "[replay][input]") {
    const auto fixture = replay_fixture{};
    fixture.write(header + timeout_event);
    replay::configure_playback(fixture.path.string());
    replay::start();
    CHECK_THROWS_WITH(
        replay::next_input_event(), Catch::Matchers::Contains("boundary mismatch at event 1"));
    CHECK_THROWS_WITH(replay::next_input_event({.context = "MENU", .actions = {"MOVE_N"}}),
                      Catch::Matchers::Contains("actual='MENU'"));
    REQUIRE(replay::next_input_event({.context = "GAME", .actions = {"MOVE_N"}}));
    CHECK_THROWS_WITH(replay::playback_exhausted(), Catch::Matchers::Contains("Unexpected EOF"));
}

TEST_CASE(
    "replay distinguishes presentation actions from gameplay boundary drift", "[replay][input]") {
    const auto fixture = replay_fixture{};
    const auto recorded = replay::input_boundary_metadata{
        .context = "GAME", .actions = {"MOVE_N", "zoom_in", "toggle_pixel_minimap"}};
    replay::configure_recording(fixture.path.string(), {.rng_seed = 7});
    replay::start();
    replay::record_input_event(input_event('k', input_event_t::keyboard), recorded);
    replay::finish();

    replay::configure_playback(fixture.path.string());
    replay::start();
    SECTION("presentation-only action differences are compatible") {
        const auto event = replay::next_input_event(
            {.context = "GAME",
             .actions = {"MOVE_N", "zoom_out", "toggle_zone_overlay", "TEXT.PASTE"},
             .timeout_ms = 125});
        REQUIRE(event);
        CHECK(event->get_first_input() == 'k');
    }
    SECTION("gameplay action differences are diagnosed") {
        CHECK_THROWS_WITH(
            replay::next_input_event(
                {.context = "GAME", .actions = {"WAIT", "zoom_out", "toggle_zone_overlay"}}),
            Catch::Matchers::Contains("missing gameplay actions ['MOVE_N']"));
    }
    SECTION("timeout differences are diagnosed") {
        CHECK_THROWS_WITH(
            replay::next_input_event(
                {.context = "GAME",
                 .actions = {"MOVE_N", "zoom_in", "toggle_pixel_minimap"},
                 .timeout_ms = 25}),
            Catch::Matchers::Contains("timeout_ms recorded=-1, actual=25"));
    }
}

TEST_CASE(
    "replay keeps character preview clothing controls client independent",
    "[replay][input][character_preview]") {
    const auto tiles_recording = GENERATE(false, true);
    const auto fixture = replay_fixture{};
    auto recorded = replay::input_boundary_metadata{
        .context = "NEW_CHAR_TRAITS", .actions = {"LEFT", "RIGHT", "CONFIRM"}};
    auto actual = recorded;
    (tiles_recording ? recorded : actual).actions.push_back("TOGGLE_CHARACTER_PREVIEW_CLOTHES");
    const auto input = input_event('h', input_event_t::keyboard);
    replay::configure_recording(fixture.path.string(), {.rng_seed = 7});
    replay::start();
    replay::record_input_event(input, recorded);
    replay::finish();
    replay::configure_playback(fixture.path.string());
    replay::start();

    SECTION("preview-only registration can differ in either direction") {
        const auto event = replay::next_input_event(actual);
        REQUIRE(event);
        CHECK(*event == input);
        CHECK(replay::playback_exhausted());
        replay::finish();
    }
    SECTION("gameplay registration remains strict") {
        actual.actions.push_back("RANDOMIZE");
        CHECK_THROWS_WITH(replay::next_input_event(actual), Catch::Matchers::Contains("RANDOMIZE"));
    }
    SECTION("gameplay order remains strict") {
        std::swap(actual.actions[0], actual.actions[1]);
        CHECK_THROWS_WITH(
            replay::next_input_event(actual), Catch::Matchers::Contains("gameplay action"));
    }
    SECTION("the native context remains strict") {
        actual.context = "NEW_CHAR_BIONICS";
        CHECK_THROWS_WITH(
            replay::next_input_event(actual), Catch::Matchers::Contains("context recorded="));
    }
}

TEST_CASE("replay streams later records and reports the malformed line", "[replay][input]") {
    const auto fixture = replay_fixture{};
    fixture.write(header + timeout_event + "invalid JSON\n");
    replay::configure_playback(fixture.path.string());
    REQUIRE_NOTHROW(replay::start());
    REQUIRE(replay::next_input_event({.context = "GAME", .actions = {"MOVE_N"}}));
    CHECK_THROWS_WITH(replay::next_input_event(), Catch::Matchers::Contains("line 3"));
}

TEST_CASE("replay does not overwrite recordings or accept unfinished playback", "[replay][input]") {
    const auto fixture = replay_fixture{};
    fixture.write(header + timeout_event);
    const auto original = fixture.read();
    replay::configure_recording(fixture.path.string());
    CHECK_THROWS(replay::start());
    CHECK(fixture.read() == original);
    CHECK_FALSE(replay::is_enabled());

    replay::configure_playback(fixture.path.string());
    replay::start();
    CHECK_THROWS_WITH(replay::finish(), Catch::Matchers::Contains("unconsumed replay input"));
    CHECK_FALSE(replay::is_enabled());
}

#if defined(CATA_MCP)
TEST_CASE(
    "replay preserves activity polls through the shared input gateway", "[replay][input][client]") {
    const auto fixture = replay_fixture{};
    const auto cleanup = on_out_of_scope([]() {
        replay::stop();
        game_client::memory::set_input_provider({});
    });
    auto context = input_context("REPLAY_ACTIVITY_POLL");
    context.register_action("ANY_INPUT");
    auto polls = 0;
    game_client::memory::set_input_provider([&](const int timeout) {
        ++polls;
        if (timeout == 0) { return input_event{}; }
        if (timeout > 0) {
            auto event = input_event{};
            event.type = input_event_t::timeout;
            return event;
        }
        return input_event('x', input_event_t::keyboard);
    });
    replay::configure_recording(fixture.path.string(), {.rng_seed = 123});
    replay::start();
    CHECK(context.handle_input(0) == "ANY_INPUT");
    CHECK(context.get_raw_input().type == input_event_t::error);
    CHECK(context.handle_input(25) == "TIMEOUT");
    CHECK(context.handle_input(-1) == "ANY_INPUT");
    CHECK(context.get_raw_input().text.empty());
    CHECK(polls == 3);
    replay::finish();

    replay::configure_playback(fixture.path.string());
    replay::start();
    CHECK(context.handle_input(0) == "ANY_INPUT");
    CHECK(context.get_raw_input().type == input_event_t::error);
    CHECK(context.handle_input(25) == "TIMEOUT");
    CHECK(context.handle_input(-1) == "ANY_INPUT");
    CHECK(context.get_raw_input().get_first_input() == 'x');
    CHECK(polls == 3);
    CHECK(replay::playback_exhausted());
    replay::finish();
}

TEST_CASE(
    "recording and playback poll activities at each simulation boundary",
    "[replay][input][client][activity]") {
    const auto fixture = replay_fixture{};
    const auto original_bubble_size = g_reality_bubble_size;
    const auto restore_new_game = restore_on_out_of_scope(g->new_game);
    // This test exercises activity input polling, not automatic simulation-window resizing.
    const auto normal_bubble =
        override_option("REALITY_BUBBLE_SIZE", std::to_string(original_bubble_size));
    const auto no_mobile_bubble = override_option("ACTIVITY_MOBILE_BUBBLE_SIZE", "0");
    const auto no_idle_bubble = override_option("ACTIVITY_IDLE_BUBBLE_SIZE", "0");
    const auto no_underground_bubble = override_option("UNDERGROUND_BUBBLE_SIZE", "0");
    const auto no_vehicle_bubble = override_option("VEHICLE_BUBBLE_SIZE", "0");
    const auto no_combat_bubble = override_option("COMBAT_BUBBLE_SIZE", "0");
    const auto cleanup = on_out_of_scope([]() {
        replay::stop();
        game_client::memory::set_input_provider({});
        clear_all_state();
    });
    const auto setup_activity = [] {
        clear_all_state();
        auto& you = get_avatar();
        g->new_game = true;
        you.set_moves(100);
        you.assign_activity(activity_id("ACT_WAIT"), 100000);
        REQUIRE(you.activity);
    };

    setup_activity();
    auto recorded_polls = 0;
    game_client::memory::set_input_provider([&](const int timeout_ms) {
        CHECK(timeout_ms == 0);
        ++recorded_polls;
        return input_event{};
    });
    replay::configure_recording(fixture.path.string(), {.rng_seed = 123});
    replay::start();
    for (const auto simulation_boundary : {0, 1, 2}) {
        CAPTURE(simulation_boundary);
        CHECK_FALSE(g->do_turn());
        CHECK(g_reality_bubble_size == original_bubble_size);
    }
    replay::finish();
    CHECK(recorded_polls == 3);
    CHECK(fixture.read().find("\"type\":\"idle\"") != std::string::npos);

    setup_activity();
    game_client::memory::set_input_provider([](const int /*timeout_ms*/) -> input_event {
        throw std::runtime_error("playback must not read live input");
    });
    replay::configure_playback(fixture.path.string(), {.rng_seed = 123});
    replay::start();
    for (const auto simulation_boundary : {0, 1, 2}) {
        CAPTURE(simulation_boundary);
        TEST_CASE("hit animations neither read nor record input",
                  "[replay][input][client][animation]") {
            const auto fixture = replay_fixture{};
            const auto cleanup = on_out_of_scope([]() {
                replay::stop();
                game_client::memory::set_input_provider({});
                clear_all_state();
            });
            const auto delay = override_option("ANIMATION_DELAY", "0");
            clear_all_state();
            const auto restore_width = restore_on_out_of_scope<int>(TERRAIN_WINDOW_WIDTH);
            const auto restore_height = restore_on_out_of_scope<int>(TERRAIN_WINDOW_HEIGHT);
            const auto restore_x = restore_on_out_of_scope<int>(POSX);
            const auto restore_y = restore_on_out_of_scope<int>(POSY);
            TERRAIN_WINDOW_WIDTH = 11;
            TERRAIN_WINDOW_HEIGHT = 11;
            POSX = 5;
            POSY = 5;
            REQUIRE(is_valid_in_w_terrain(point(POSX, POSY)));
            auto polls = 0;
            game_client::memory::set_input_provider([&](const int /*timeout_ms*/) {
                ++polls;
                return input_event{};
            });
            replay::configure_recording(fixture.path.string(), {.rng_seed = 123});
            replay::start();
            g->draw_hit_player(get_avatar(), 5);
            replay::finish();
            CHECK(polls == 0);
            CHECK(fixture.read().find("\"kind\":\"input\"") == std::string::npos);
        }

        CHECK_FALSE(g->do_turn());
        CHECK(g_reality_bubble_size == original_bubble_size);
    }
    CHECK(replay::playback_exhausted());
    replay::finish();
}
#endif

TEST_CASE("replay rejects seed mismatch and invalid end counts", "[replay][input]") {
    const auto fixture = replay_fixture{};
    fixture.write(
        header
        + R"({"kind":"end","version":1,"events":1})"
          "\n");
    replay::configure_playback(fixture.path.string(), {.rng_seed = 8});
    CHECK_THROWS_WITH(replay::start(), Catch::Matchers::Contains("expected metadata"));
    replay::configure_playback(fixture.path.string());
    CHECK_THROWS_WITH(replay::start(), Catch::Matchers::Contains("input count does not match"));
}
