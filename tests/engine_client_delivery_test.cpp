#if defined(CATA_MCP)

#    include "avatar.h"
#    include "calendar.h"
#    include "catch/catch.hpp"
#    include "client_input.h"
#    include "engine_client_schema.h"
#    include "engine_client_session.h"
#    include "engine_client_world.h"
#    include "game.h"
#    include "game_session.h"
#    include "input.h"
#    include "item.h"
#    include "json.h"
#    include "map/map.h"
#    include "map/mapbuffer.h"
#    include "map/submap.h"
#    include "map_helpers.h"
#    include "map_perception.h"
#    include "mcp_server.h"
#    include "monster.h"
#    include "player_activity.h"
#    include "rng.h"
#    include "state_helpers.h"
#    include "weather/weather.h"

#    include <algorithm>
#    include <chrono>
#    include <functional>
#    include <iostream>
#    include <ranges>
#    include <sstream>
#    include <string>

namespace {
using namespace engine_client;

/// `watched` is a paying controller with a stalled spectator beside it.
enum class client_mode { absent, paying, stalled, watched };

struct wait_run {
    std::string state;
    cata_default_random_engine rng;
    std::size_t frames = 0;
    std::size_t oracle_mismatches = 0;
    std::size_t most_held_frames = 0;
    std::size_t most_held_bytes = 0;
    std::size_t watcher_held_frames = 0;
    std::size_t watcher_held_bytes = 0;
    std::size_t watcher_frames = 0;
    double milliseconds = 0;
};

auto request(const std::string& method, const std::string& params, const std::string& id)
    -> std::string {
    return R"({"jsonrpc":"2.0","method":")" + method + R"(","params":)" + params + R"(,"id":)" + id
         + "}\n";
}

auto count_of(const std::string& text, const std::string& part) -> std::size_t {
    auto count = std::size_t{0};
    for (auto at = text.find(part); at != std::string::npos; at = text.find(part, at + 1)) {
        ++count;
    }
    return count;
}

auto avatar_save() -> std::string {
    auto stream = std::ostringstream{};
    auto json = JsonOut(stream);
    get_avatar().serialize(json);
    return stream.str();
}

/// Every run starts from the same avatar: the helpers' reset keeps skill practice and focus.
auto restore_avatar(const std::string& save) -> void {
    auto stream = std::istringstream{save};
    auto json = JsonIn{stream};
    get_avatar().deserialize(json);
}

/// Everything gameplay owns that a waiting avatar can change: the avatar's save and the monsters.
auto authority() -> std::string {
    auto stream = std::ostringstream{};
    auto json = JsonOut(stream);
    json.start_object();
    json.member("monsters");
    json.start_array();
    for (const auto& mon : g->all_monsters()) { mon.serialize(json); }
    json.end_array();
    json.member("avatar");
    get_avatar().serialize(json);
    json.end_object();
    return stream.str();
}

/// The avatar waits `turns` turns, each of which draws from the RNG and ends with a publication
/// to the client, if there is one. Unless `paced`, the clock stands still and every turn is
/// published.
auto wait_for(
    const client_mode mode, const std::string& start, const int turns, const bool paced = false)
    -> wait_run {
    rng_set_engine_seed(424242);
    clear_all_state();
    restore_avatar(start);
    build_test_map(ter_id("t_floor"));
    auto& here = get_map();
    // A small lit room in solid rock: the client is told only what the avatar knows of it.
    for (const auto y : std::views::iota(0, here.getmapsize() * SEEY)) {
        for (const auto x : std::views::iota(0, here.getmapsize() * SEEX)) {
            const auto inside = std::abs(x - 60) <= 6 && std::abs(y - 60) <= 6;
            here.ter_set(tripoint_bub_ms(x, y, 0), ter_id(inside ? "t_floor" : "t_rock"));
        }
    }
    calendar::turn = calendar::turn_zero + 12_hours;
    get_weather().weather_id = weather_type_id("clear");
    g->reset_light_level();
    game_session::set_running(true);
    auto& you = get_avatar();
    you.setpos(tripoint_bub_ms(60, 60, 0));
    spawn_test_monster("mon_zombie", tripoint_bub_ms(64, 60, 0));
    map_perception::acquire();

    auto context = input_context{"YESNO"};
    auto input = game_client::input_context_scope{context, "YESNO"};
    context.register_action("YES");
    game_client::begin_input_boundary();
    auto authority_session = session{};
    authority_session.set_world_capture(
        &engine_client::world::capture_world, &engine_client::world::capture_patch);
    authority_session.set_oracle(true);
    auto output = std::ostringstream{};
    auto errors = std::ostringstream{};
    auto watcher_output = std::ostringstream{};
    const auto frozen = std::function<auto()->std::chrono::steady_clock::time_point>{[] {
        return std::chrono::steady_clock::time_point{};
    }};
    auto watcher = bn::mcp::server{
        {
            .observe = [] { return bn::mcp::screen_snapshot{.json = "{}"}; },
            .submit = [](const auto&) { return true; },
            .has_input = [] { return false; },
            .contract_session = &authority_session,
        },
        {.now = paced ? std::function<auto()->std::chrono::steady_clock::time_point>{} : frozen,
         .spectator = true}};
    auto server = bn::mcp::server{
        {
            .observe = [] { return bn::mcp::screen_snapshot{.json = "{}"}; },
            .submit = [](const auto&) { return true; },
            .has_input = [] { return false; },
            .contract_session = &authority_session,
            .published = [&] { REQUIRE(watcher.flush_pushed(watcher_output)); },
        },
        {.now = paced ? std::function<auto()->std::chrono::steady_clock::time_point>{} : [] {
            return std::chrono::steady_clock::time_point{};
        }}};
    const auto feed = [&](const std::string& text) {
        auto lines = std::istringstream{text};
        for (auto left = count_of(text, "\n"); left > 0; --left) {
            REQUIRE(server.poll_input(lines, output, errors));
        }
    };
    REQUIRE(authority_session.publish_boundary());
    if (mode == client_mode::watched) {
        auto input = std::istringstream{
            request("bn.hello",
                    R"({"versions":["0.2"],"client":{"name":"w","version":"1"},"schema":")"
                        + std::string{engine_schema_hash} + R"(","delivery":{"credits":1}})",
                    "1")
            + request("bn.subscribe", "{}", "2")};
        REQUIRE(watcher.poll_input(input, watcher_output, errors));
        REQUIRE(watcher.poll_input(input, watcher_output, errors));
    }
    if (mode != client_mode::absent) {
        const auto window = mode == client_mode::stalled ? "1" : "256";
        feed(request(
            "bn.hello",
            R"({"versions":["0.2"],"client":{"name":"t","version":"1"},"schema":")"
                + std::string{engine_schema_hash} + R"(","delivery":{"credits":)"
                + std::string{window} + "}}",
            "1"));
        feed(request("bn.subscribe", "{}", "2"));
    }
    output.str("");
    watcher_output.str("");

    you.assign_activity(
        std::make_unique<player_activity>(activity_id("ACT_WAIT"), to_moves<int>(1_days), 0),
        false);
    auto result = wait_run{};
    const auto began = std::chrono::steady_clock::now();
    for (auto turn = 0; turn < turns; ++turn) {
        you.set_moves(100);
        you.update_body();
        you.activity->do_turn(you);
        here.process_items();
        calendar::turn += 1_turns;
        if (mode == client_mode::absent) { continue; }
        REQUIRE(server.publish_activity(output));
        result.most_held_frames = std::max(result.most_held_frames, server.held_frames());
        result.most_held_bytes = std::max(result.most_held_bytes, server.held_bytes());
        result.watcher_held_frames = std::max(result.watcher_held_frames, watcher.held_frames());
        result.watcher_held_bytes = std::max(result.watcher_held_bytes, watcher.held_bytes());
        if (mode == client_mode::paying || mode == client_mode::watched) {
            feed(request("bn.credit", R"({"frames":1})", "3"));
        }
    }
    result.milliseconds =
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - began).count();
    result.frames = count_of(output.str(), R"("method":"bn.events")");
    result.oracle_mismatches = authority_session.oracle_mismatches().size();
    result.watcher_frames = count_of(watcher_output.str(), R"("method":"bn.events")");
    result.rng = rng_get_engine();
    result.state = authority();
    return result;
}

auto baseline() -> std::string {
    clear_all_state();
    return avatar_save();
}
} // namespace

TEST_CASE(
    "a wait replayed from the same start is identical with a corpse rotting outside the bubble",
    "[engine_client_delivery]") {
    const auto start = baseline();
    auto& buffer = get_map().get_mapbuffer();
    const auto outside = tripoint_abs_sm(35, 20, 0);
    REQUIRE(buffer.lookup_submap_in_memory(outside) != nullptr);
    buffer.add_item(project_to<coords::ms>(outside), item::spawn("corpse_child_calm"));
    REQUIRE(buffer.get_submaps_with_active_items().contains(outside));
    const auto absent = wait_for(client_mode::absent, start, 40);
    const auto again = wait_for(client_mode::absent, start, 40);
    CHECK(again.state == absent.state);
    CHECK(again.rng == absent.rng);
}

TEST_CASE(
    "a waiting avatar is identical with a paying, a stalled and no client",
    "[engine_client_delivery]") {
    const auto start = baseline();
    const auto turns = 40;
    const auto absent = wait_for(client_mode::absent, start, turns);
    const auto again = wait_for(client_mode::absent, start, turns);
    REQUIRE(again.state == absent.state);
    REQUIRE(again.rng == absent.rng);

    const auto paying = wait_for(client_mode::paying, start, turns);
    CHECK(paying.oracle_mismatches == 0);
    CHECK(paying.frames == static_cast<std::size_t>(turns));
    CHECK(paying.most_held_frames == 0);
    CHECK(paying.rng == absent.rng);
    CHECK(paying.state == absent.state);

    // One credit pays for the first turn; the other turns wait in the engine, within its bound.
    const auto stalled = wait_for(client_mode::stalled, start, turns);
    CHECK(stalled.oracle_mismatches == 0);
    CHECK(stalled.frames == 1);
    CHECK(stalled.most_held_frames == static_cast<std::size_t>(turns - 1));
    CHECK(stalled.most_held_bytes <= maximum_held_bytes);
    CHECK(stalled.rng == absent.rng);
    CHECK(stalled.state == absent.state);
}

TEST_CASE(
    "a waiting avatar is identical with a stalled spectator beside a paying controller",
    "[engine_client_delivery]") {
    const auto start = baseline();
    const auto turns = 40;
    const auto absent = wait_for(client_mode::absent, start, turns);
    const auto watched = wait_for(client_mode::watched, start, turns);
    // The controller is paid every frame; the spectator waits for credits within the same bound.
    CHECK(watched.frames == static_cast<std::size_t>(turns));
    CHECK(watched.most_held_frames == 0);
    CHECK(watched.watcher_frames == 1);
    CHECK(watched.watcher_held_frames == static_cast<std::size_t>(turns - 1));
    CHECK(watched.watcher_held_bytes <= maximum_held_bytes);
    CHECK(watched.rng == absent.rng);
    CHECK(watched.state == absent.state);
}

/// Milliseconds a turn of an hour-long wait costs the engine with no, a paying and a stalled
/// client: `cata_test-mcp "[.engine_client_delivery_cost]"` prints them.
TEST_CASE("the cost of a waiting turn per client", "[.engine_client_delivery_cost]") {
    const auto start = baseline();
    const auto turns = 200;
    for (const auto mode : {client_mode::absent, client_mode::paying, client_mode::stalled}) {
        const auto run = wait_for(mode, start, turns, true);
        std::cerr << "mode " << static_cast<int>(mode) << ": " << run.milliseconds / turns
                  << " ms/turn\n";
    }
}

#endif
