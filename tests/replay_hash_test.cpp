#include "avatar.h"
#include "calendar.h"
#include "catch/catch.hpp"
#include "map/map.h"
#include "map_helpers.h"
#include "path_info.h"
#include "replay/state_hash.h"
#include "rng.h"
#include "state_helpers.h"

#include <algorithm>
#include <atomic>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace {

constexpr auto you_at = tripoint_bub_ms(62, 62, 0);

/// A game on open floor with the avatar at `you_at`.
auto fresh_game() -> void {
    clear_all_state();
    build_test_map(ter_id("t_floor"));
    get_avatar().setpos(you_at);
}

/// The names of the parts whose digests differ.
auto differing(const replay::state_hash& before, const replay::state_hash& after)
    -> std::vector<std::string> {
    REQUIRE(before.parts.size() == after.parts.size());
    auto names = std::vector<std::string>{};
    for (auto index = std::size_t{}; index < before.parts.size(); ++index) {
        REQUIRE(before.parts[index].name == after.parts[index].name);
        if (before.parts[index].digest != after.parts[index].digest) {
            names.push_back(before.parts[index].name);
        }
    }
    return names;
}

struct hash_log_fixture {
    std::filesystem::path directory;
    std::filesystem::path path;

    hash_log_fixture() {
        static auto counter = std::atomic_uint64_t{0};
        do {
            directory =
                std::filesystem::path(PATH_INFO::user_dir())
                / ("state-hash-test-" + std::to_string(counter.fetch_add(1)));
        } while (!std::filesystem::create_directory(directory));
        path = directory / "state.jsonl";
    }
    ~hash_log_fixture() {
        replay::stop_state_hash_log();
        std::filesystem::remove_all(directory);
    }
    auto lines() const -> std::vector<std::string> {
        auto stream = std::ifstream(path, std::ios::binary);
        auto result = std::vector<std::string>{};
        for (auto line = std::string{}; std::getline(stream, line);) { result.push_back(line); }
        return result;
    }
};

} // namespace

TEST_CASE("the state hash is stable and names the part that changed", "[replay][state_hash]") {
    fresh_game();
    const auto start = replay::hash_game_state();
    CHECK(differing(start, replay::hash_game_state()).empty());
    CHECK(start.turn == to_turn<int>(calendar::turn));

    SECTION("RNG") {
        static_cast<void>(rng(0, 1000));
        CHECK(differing(start, replay::hash_game_state()) == std::vector<std::string>{"rng"});
    }
    SECTION("avatar") {
        get_avatar().focus_pool += 1;
        CHECK(differing(start, replay::hash_game_state()) == std::vector<std::string>{"avatar"});
    }
    SECTION("monsters") {
        spawn_test_monster("mon_zombie", you_at + tripoint_east * 3);
        const auto changed = differing(start, replay::hash_game_state());
        CHECK(std::ranges::find(changed, "monsters") != changed.end());
        CHECK(std::ranges::find(changed, "avatar") == changed.end());
    }
    SECTION("map beside the avatar") {
        get_map().ter_set(you_at + tripoint_east, ter_id("t_wall"));
        CHECK(differing(start, replay::hash_game_state()) == std::vector<std::string>{"map"});
    }
    SECTION("clock") {
        calendar::turn += 1_turns;
        CHECK(replay::hash_game_state().turn == start.turn + 1);
    }
}

TEST_CASE("a state record names the step, the turn and every part", "[replay][state_hash]") {
    const auto hash = replay::state_hash{
        .turn = 12,
        .parts = {{.name = "rng", .digest = "0123456789abcdef"}, {.name = "map", .digest = "f"}},
    };
    CHECK(replay::format_state_hash(3, hash)
          == R"({"kind":"state","step":3,"turn":12,"parts":{"rng":"0123456789abcdef","map":"f"}})");
}

TEST_CASE("the state log has a header and one record per logged turn", "[replay][state_hash]") {
    fresh_game();
    const auto fixture = hash_log_fixture{};
    replay::log_state_hash();
    CHECK_FALSE(std::filesystem::exists(fixture.path));

    const auto turn = to_turn<int>(calendar::turn);
    replay::configure_state_hash_log(fixture.path.string());
    replay::log_state_hash();
    calendar::turn += 1_turns;
    replay::log_state_hash();
    const auto lines = fixture.lines();
    REQUIRE(lines.size() == 3);
    CHECK(lines[0] == R"({"kind":"header","format":"cataclysm-bn-state-hash","version":1})");
    CHECK(lines[1].starts_with(R"({"kind":"state","step":1,"turn":)" + std::to_string(turn) + ","));
    CHECK(lines[2].starts_with(
        R"({"kind":"state","step":2,"turn":)" + std::to_string(turn + 1) + ","));
}

TEST_CASE("the state log refuses to overwrite a file or open twice", "[replay][state_hash]") {
    const auto fixture = hash_log_fixture{};
    { std::ofstream(fixture.path) << "keep"; }
    CHECK_THROWS(replay::configure_state_hash_log(fixture.path.string()));
    CHECK(fixture.lines() == std::vector<std::string>{"keep"});

    const auto other = fixture.directory / "other.jsonl";
    replay::configure_state_hash_log(other.string());
    CHECK_THROWS(replay::configure_state_hash_log((fixture.directory / "third.jsonl").string()));
}
