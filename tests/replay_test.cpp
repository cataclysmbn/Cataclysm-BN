#include "cata_utility.h"
#include "catch/catch.hpp"
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
#include <vector>

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
    CHECK_THROWS_WITH(replay::next_input_event(), Catch::Matchers::Contains("Unexpected EOF"));
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
        CHECK_THROWS_AS(replay::next_input_event(), replay::completed);
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
    replay::configure_recording(fixture.path.string(), {.rng_seed = 7});
    CHECK_THROWS(replay::start());
    CHECK(fixture.read() == original);
    CHECK_FALSE(replay::is_enabled());

    replay::configure_playback(fixture.path.string());
    replay::start();
    CHECK_THROWS_WITH(replay::finish(), Catch::Matchers::Contains("unconsumed replay input"));
    CHECK_FALSE(replay::is_enabled());
}

TEST_CASE("discard deletes only a recording that start created", "[replay][input]") {
    const auto fixture = replay_fixture{};
    replay::configure_recording(fixture.path.string(), {.rng_seed = 7});
    replay::start();
    replay::discard();
    CHECK_FALSE(replay::is_enabled());
    CHECK_FALSE(std::filesystem::exists(fixture.path));

    fixture.write(header);
    replay::configure_recording(fixture.path.string(), {.rng_seed = 7});
    CHECK_THROWS(replay::start());
    replay::configure_recording(fixture.path.string(), {.rng_seed = 7});
    replay::discard();
    CHECK(fixture.read() == header);

    fixture.write(header + timeout_event);
    replay::configure_playback(fixture.path.string());
    replay::start();
    replay::discard();
    CHECK_FALSE(replay::is_enabled());
    CHECK(fixture.read() == header + timeout_event);
}

TEST_CASE("replay returns recorded external text instead of reading it again", "[replay][input]") {
    const auto fixture = replay_fixture{};
    const auto boundary = replay::input_boundary_metadata{.context = "GAME"};
    replay::configure_recording(fixture.path.string(), {.rng_seed = 9});
    replay::start();
    CHECK(replay::external_text("clipboard", [] { return std::string("Alice"); }) == "Alice");
    replay::record_input_event(input_event{}, boundary);
    replay::finish();

    SECTION("playback ignores the live source") {
        replay::configure_playback(fixture.path.string());
        replay::start();
        CHECK(
            replay::external_text(
                "clipboard",
                []() -> std::string {
                    throw std::runtime_error("playback must not read the live source");
                })
            == "Alice");
        CHECK(replay::next_input_event(boundary));
        replay::finish();
    }
    SECTION("a different source is rejected") {
        replay::configure_playback(fixture.path.string());
        replay::start();
        CHECK_THROWS_WITH(replay::external_text("file", [] { return std::string(); }),
                          Catch::Matchers::Contains("external text"));
    }
    SECTION("an input read before the recorded text is rejected") {
        replay::configure_playback(fixture.path.string());
        replay::start();
        CHECK_THROWS_WITH(
            replay::next_input_event(boundary), Catch::Matchers::Contains("external text"));
    }
}

TEST_CASE("replay rejects mouse events recorded in another pointer space", "[replay][input]") {
    const auto fixture = replay_fixture{};
    const auto cells = replay::input_boundary_metadata{.context = "GAME", .pointer_space = "cell"};
    auto click = input_event(MOUSE_BUTTON_LEFT, input_event_t::mouse);
    click.mouse_pos = point(100, 100);
    replay::configure_recording(fixture.path.string(), {.rng_seed = 9});
    replay::start();
    replay::record_input_event(click, cells);
    replay::record_input_event(input_event('x', input_event_t::keyboard), cells);
    replay::finish();

    replay::configure_playback(fixture.path.string());
    replay::start();
    const auto pixels =
        replay::input_boundary_metadata{.context = "GAME", .pointer_space = "pixel"};
    CHECK_THROWS_WITH(replay::next_input_event(pixels), Catch::Matchers::Contains("pointer_space"));
}

TEST_CASE("replay rejects a zero or mismatched seed and invalid end counts", "[replay][input]") {
    const auto fixture = replay_fixture{};
    CHECK_THROWS_WITH(replay::configure_recording(fixture.path.string(), {}),
                      Catch::Matchers::Contains("nonzero"));
    CHECK_FALSE(replay::is_enabled());
    fixture.write(
        header
        + R"({"kind":"end","version":1,"events":1})"
          "\n");
    replay::configure_playback(fixture.path.string(), {.rng_seed = 8});
    CHECK_THROWS_WITH(replay::start(), Catch::Matchers::Contains("expected metadata"));
    replay::configure_playback(fixture.path.string());
    CHECK_THROWS_WITH(replay::start(), Catch::Matchers::Contains("input count does not match"));
}

TEST_CASE("playback leaves the pressed key for menus that open on it", "[replay][input]") {
    const auto fixture = replay_fixture{};
    const auto boundary = replay::input_boundary_metadata{
        .timeout_ms = inp_mngr.get_timeout(),
#if defined(TILES)
        .pointer_space = "pixel",
#else
        .pointer_space = "cell",
#endif
    };
    // Curses keeps the first UTF-8 byte of a multi-byte key and reports its code point as pressed.
    auto hangul = input_event(0xED, input_event_t::keyboard);
    hangul.text = "한";
    replay::configure_recording(fixture.path.string(), {.rng_seed = 9});
    replay::start();
    replay::record_input_event(input_event('r', input_event_t::keyboard), boundary);
    replay::record_input_event(hangul, boundary);
    replay::finish();

    replay::configure_playback(fixture.path.string());
    replay::start();
    CHECK(inp_mngr.get_input_event().get_first_input() == 'r');
    CHECK(inp_mngr.get_previously_pressed_key() == 'r');
    CHECK(inp_mngr.get_input_event().text == "한");
    CHECK(inp_mngr.get_previously_pressed_key() == 0xD55C);
}

TEST_CASE("replay ignores record fields it does not know", "[replay][input]") {
    const auto fixture = replay_fixture{};
    fixture.write(
        R"({"kind":"header","format":"cataclysm-bn-replay","version":1,"rng_seed":7,"client":"x"})"
        "\n"
        R"({"kind":"input","version":1,"type":"keyboard","sequence":[107],"modifiers":[],"mouse_pos":[0,0],"text":"","edit":"","edit_refresh":false,"shortcut_last_used_action_counter":3,"boundary":{"context":"GAME","actions":["MOVE_N"],"layout":"x"}})"
        "\n"
        R"({"kind":"end","version":1,"events":1})"
        "\n");
    replay::configure_playback(fixture.path.string());
    REQUIRE_NOTHROW(replay::start());
    const auto event = replay::next_input_event({.context = "GAME", .actions = {"MOVE_N"}});
    REQUIRE(event);
    CHECK(event->get_first_input() == 'k');
    CHECK_THROWS_AS(replay::next_input_event(), replay::completed);
}
