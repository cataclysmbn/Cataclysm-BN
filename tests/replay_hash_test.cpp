#include "avatar.h"
#include "calendar.h"
#include "catch/catch.hpp"
#include "json.h"
#include "map/map.h"
#include "map_helpers.h"
#include "monster.h"
#include "path_info.h"
#include "recipe.h"
#include "recipe_dictionary.h"
#include "replay/state_hash.h"
#include "rng.h"
#include "state_helpers.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <map>
#include <memory>
#include <new>
#include <sstream>
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

struct logged_state {
    int turn = 0;
    std::map<std::string, std::string> parts;
};

/// The names of the parts whose digests differ.
auto differing(const logged_state& before, const logged_state& after) -> std::vector<std::string> {
    auto names = std::vector<std::string>{};
    for (const auto& [name, digest] : before.parts) {
        if (after.parts.at(name) != digest) { names.push_back(name); }
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
    /// Log the current game state to a new file and read the record back.
    auto logged() const -> logged_state {
        replay::stop_state_hash_log();
        std::filesystem::remove(path);
        replay::configure_state_hash_log(path.string());
        replay::log_state_hash();
        auto stream = std::istringstream(lines().at(1));
        auto input = JsonIn(stream);
        auto record = input.get_object();
        record.allow_omitted_members();
        auto result = logged_state{.turn = record.get_int("turn")};
        for (const auto& part : record.get_object("parts")) {
            result.parts[part.name()] = part.get_string();
        }
        return result;
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
    const auto fixture = hash_log_fixture{};
    const auto start = fixture.logged();
    CHECK(differing(start, fixture.logged()).empty());
    CHECK(start.turn == to_turn<int>(calendar::turn));

    SECTION("RNG") {
        static_cast<void>(rng(0, 1000));
        CHECK(differing(start, fixture.logged()) == std::vector<std::string>{"rng"});
    }
    SECTION("avatar") {
        get_avatar().focus_pool += 1;
        CHECK(differing(start, fixture.logged()) == std::vector<std::string>{"avatar"});
    }
    SECTION("monsters") {
        spawn_test_monster("mon_zombie", you_at + tripoint_east * 3);
        const auto changed = differing(start, fixture.logged());
        CHECK(std::ranges::find(changed, "monsters") != changed.end());
        CHECK(std::ranges::find(changed, "avatar") == changed.end());
    }
    SECTION("map beside the avatar") {
        get_map().ter_set(you_at + tripoint_east, ter_id("t_wall"));
        CHECK(differing(start, fixture.logged()) == std::vector<std::string>{"map"});
    }
    SECTION("clock") {
        calendar::turn += 1_turns;
        CHECK(fixture.logged().turn == start.turn + 1);
    }
}

TEST_CASE("a new monster is written the same whatever its memory held", "[replay][state_hash]") {
    fresh_game();
    const auto written = [](const std::byte fill) {
        alignas(monster) auto storage = std::array<std::byte, sizeof(monster)>{};
        storage.fill(fill);
        // The constructor rolls attack cooldowns.
        rng_set_engine_seed(1);
        auto* const critter = new (storage.data()) monster(mtype_id("mon_zombie"));
        auto text = std::ostringstream{};
        auto json = JsonOut(text);
        critter->serialize(json);
        std::destroy_at(critter);
        return text.str();
    };
    CHECK(written(std::byte{0x00}) == written(std::byte{0xff}));
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
    CHECK(lines[0] == R"({"kind":"header","format":"cataclysm-bn-state-hash"})");
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

TEST_CASE("a recipe subset is written in id order", "[replay][state_hash]") {
    auto subset = recipe_subset{};
    auto expected = std::vector<std::string>{};
    for (const auto& [id, entry] : recipe_dict) {
        subset.include(&entry);
        expected.push_back(id.str());
    }
    REQUIRE(expected.size() > 1);
    std::ranges::sort(expected);
    auto written = std::ostringstream{};
    auto json = JsonOut(written);
    serialize(subset, json);
    auto sorted = std::ostringstream{};
    auto sorted_json = JsonOut(sorted);
    sorted_json.write(expected);
    CHECK(written.str() == sorted.str());
}
