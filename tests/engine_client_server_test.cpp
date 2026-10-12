#if defined(CATA_MCP)

#    include "avatar.h"
#    include "catch/catch.hpp"
#    include "client_input.h"
#    include "engine_client_session.h"
#    include "input.h"
#    include "json.h"
#    include "mcp_server.h"
#    include "messages.h"
#    include "rng.h"

#    include <algorithm>
#    include <chrono>
#    include <functional>
#    include <sstream>
#    include <string>
#    include <vector>

namespace {
using namespace engine_client;

auto rpc(const std::string& method, const std::string& params, const std::string& id = "1")
    -> std::string {
    return R"({"jsonrpc":"2.0","method":")" + method + R"(","params":)" + params
         + (id.empty() ? "" : R"(,"id":)" + id) + "}\n";
}
const auto hello =
    rpc("bn.hello", R"({"versions":["1.0"],"client":{"name":"test","version":"1"}})", "1");
const auto subscribe = rpc("bn.subscribe", "{}", "2");

auto contains(const std::string& text, const std::string& part) -> bool {
    return text.find(part) != std::string::npos;
}
auto index_of(const std::string& text, const std::string& part) -> std::size_t {
    return text.find(part);
}

auto count_of(const std::string& text, const std::string& part) -> std::size_t {
    auto count = std::size_t{0};
    for (auto at = text.find(part); at != std::string::npos; at = text.find(part, at + 1)) {
        ++count;
    }
    return count;
}
auto hello_with(const std::string& delivery) -> std::string {
    return rpc(
        "bn.hello",
        R"({"versions":["1.0"],"client":{"name":"test","version":"1"},"delivery":)" + delivery
            + "}",
        "1");
}

auto world_cell(const int x) -> cell {
    return {.at = {.x = x},
            .known = knowledge::visible,
            .terrain = look{.kind = "terrain", .id = "t_dirt", .glyph = ".", .color = "brown"}};
}
auto known_cells = std::vector<int>{1, 2};
auto declared_viewport = std::vector<std::pair<int, int>>{};
auto with_avatar = true;
auto capture_test_world() -> world_state {
    auto result = world_state{};
    result.coverage = bounds{.min = {.x = 0, .y = -5, .z = 0}, .max = {.x = 1000, .y = 5, .z = 0}};
    for (const auto x : known_cells) { result.cells[{.x = x}] = world_cell(x); }
    if (!declared_viewport.empty()) {
        const auto [cols, rows] = declared_viewport.back();
        result.view =
            bounds{.min = {.x = 0, .y = 0, .z = 0}, .max = {.x = cols - 1, .y = rows - 1, .z = 0}};
    }
    if (with_avatar) { result.avatar = avatar_value{.id = "e:avatar", .name = "Ada"}; }
    return result;
}

struct input_boundary {
    input_context context{"YESNO"};
    game_client::input_context_scope input{context, "YESNO"};
    game_client::interaction_scope interaction{
        context, [] {
            return game_client::interaction_snapshot{
                .kind = game_client::interaction_kind::choices,
                .allow_cancel = true,
                .choices =
                    {{.id = "choice:yes", .label = "Yes"}, {.id = "choice:no", .label = "No"}},
            };
        }};
    input_boundary() {
        context.register_action("YES");
        game_client::begin_input_boundary();
    }
};

struct fixture {
    session authority;
    /// The wall clock that paces activity publication; frozen, so every step is published.
    std::function<auto()->std::chrono::steady_clock::time_point> clock = [] {
        return std::chrono::steady_clock::time_point{};
    };
    std::vector<bn::mcp::key_event> queued;
    std::ostringstream output;
    std::ostringstream errors;
    /// Runs when the controller publishes, as the hub that flushes the spectators does.
    std::function<auto()->void> on_published;
    bn::mcp::server server{
        {
            .observe = [] { return bn::mcp::screen_snapshot{.json = "{}"}; },
            .submit =
                [this](const auto& input) {
                    queued = input;
                    return true;
                },
            .has_input = [this] { return !queued.empty(); },
            .resize_viewport =
                [](const int cols, const int rows) { declared_viewport.push_back({cols, rows}); },
            .contract_session = &authority,
            .published =
                [this] {
                    if (on_published) { on_published(); }
                },
        },
        {.now = [this] { return clock(); }}};
    /// Reads what the authority publishes, as a connection of its own would.
    engine_client::push_reader tap = authority.open_push();
    fixture() {
        known_cells = {1, 2};
        declared_viewport.clear();
        with_avatar = true;
        authority.set_world_capture(&capture_test_world);
    }
    /// Feed `text`, then stop at the next native input. Ending without queued input ends the
    /// server for good, so every step but the last must submit a command.
    auto pump(const std::string& text) -> bool {
        auto input = std::istringstream{text};
        return server.pump_until_input(input, output, errors);
    }
    auto submit_text(const std::string& operation, const std::string& schema) -> std::string {
        const auto current = authority.current();
        REQUIRE(current);
        return rpc(
            "bn.command.submit",
            R"({"epoch":")" + authority.epoch() + R"(","expect":{"revision":")"
                + std::to_string(current->at.revision) + R"(","boundary_id":")"
                + current->value.interaction.id + R"(","schema_id":)" + schema + R"(},"operation":)"
                + operation + "}",
            "3");
    }
    auto schema() -> std::string {
        return "\"" + authority.current()->value.interaction.interaction->schema_id + "\"";
    }
    /// Serve each line of `text` as one request while an activity runs; the server stays alive.
    auto poll(const std::string& text) -> void {
        auto input = std::istringstream{text};
        for (auto lines = count_of(text, "\n"); lines > 0; --lines) {
            REQUIRE(server.poll_input(input, output, errors));
        }
    }
    /// One activity step: the world changes and the engine publishes it without any input boundary.
    auto step() -> void {
        known_cells.push_back(static_cast<int>(known_cells.size()) + 1);
        REQUIRE(server.publish_step(output));
    }
    /// A new native boundary, published before the next step reads its requests.
    auto next_boundary() -> void {
        game_client::begin_input_boundary();
        REQUIRE(authority.publish_boundary());
    }
    auto command_id(const std::string& text) -> std::string {
        const auto at = text.find(R"("command_id":")");
        REQUIRE(at != std::string::npos);
        return text.substr(at + 14, text.find('"', at + 14) - at - 14);
    }
    /// Deliver the queued input as the native backend would, then reach the next boundary.
    auto run_native_input() -> void {
        REQUIRE(queued.size() == 1);
        const auto native = game_client::resolve_input_command(queued.front(), point{80, 24});
        REQUIRE(native);
        authority.delivered(*native);
        queued.clear();
        game_client::begin_input_boundary();
    }
};
} // namespace

TEST_CASE(
    "a timed wait publishes the boundary and lapses with nothing queued",
    "[engine_client_server][minigames]") {
    auto boundary = input_boundary{};
    auto test = fixture{};
    auto waits = std::vector<int>{};
    auto answers = true;
    auto timed = bn::mcp::server{{
        .observe = [] { return bn::mcp::screen_snapshot{.json = "{}"}; },
        .submit = [&](const auto& input) { return test.queued = input, true; },
        .has_input = [&] { return !test.queued.empty(); },
        .wait_input = [&](const int ms) { return waits.push_back(ms), answers; },
        .contract_session = &test.authority,
    }};
    auto input = std::istringstream{hello + subscribe};
    SECTION("a request that arrives in time is served before the wait can lapse") {
        // The two requests are read, then the stream ends: the wait said a frame was ready.
        CHECK_FALSE(timed.pump_until_input(input, test.output, test.errors, 50));
        CHECK(contains(test.output.str(), R"("version":"1.0")"));
        CHECK(waits.size() == 3);
    }
    SECTION("no request within the wait returns with nothing queued") {
        answers = false;
        CHECK(timed.pump_until_input(input, test.output, test.errors, 50));
        CHECK(test.queued.empty());
        CHECK(waits.size() == 1);
        CHECK(waits.front() > 0);
        CHECK(waits.front() <= 50);
        // Nothing was read: the lapse leaves the requests for the next wait.
        CHECK_FALSE(contains(test.output.str(), R"("version":"1.0")"));
    }
    SECTION("an untimed pump never consults the wait") {
        CHECK_FALSE(timed.pump_until_input(input, test.output, test.errors));
        CHECK(waits.empty());
    }
}

TEST_CASE("hello is required and checks the version", "[engine_client_server]") {
    auto boundary = input_boundary{};
    auto test = fixture{};
    test.pump(
        rpc("bn.subscribe", "{}", "1")
        + rpc("bn.hello", R"({"versions":["0.9"],"client":{"name":"t","version":"1"}})", "2")
        + hello);
    const auto out = test.output.str();
    CHECK(contains(out, R"("kind":"negotiation_failed","action":"hello")"));
    CHECK(index_of(out, R"("id":1)") < index_of(out, R"("id":2)"));
    CHECK(contains(out, R"("version":"1.0")"));
    CHECK(contains(out, R"("epoch":")" + test.authority.epoch()));
}

TEST_CASE(
    "hello may declare the map viewport and bn.viewport resizes it", "[engine_client_server]") {
    auto boundary = input_boundary{};
    auto test = fixture{};
    const auto declare = [](const std::string& view) {
        return rpc(
            "bn.hello",
            R"({"versions":["1.0"],"client":{"name":"t","version":"1"},"viewport":)" + view + "}",
            "1");
    };
    SECTION("a hello without a viewport leaves the terminal alone") {
        test.pump(hello);
        CHECK(declared_viewport.empty());
    }
    SECTION("a declared viewport reaches the host once and shapes the next published view") {
        test.pump(declare(R"({"cols":40,"rows":25})") + subscribe);
        CHECK(declared_viewport == std::vector<std::pair<int, int>>{{40, 25}});
        test.next_boundary();
        CHECK(test.authority.current()->value.world.view->max == position{.x = 39, .y = 24});
    }
    SECTION("an invalid viewport is rejected without resizing") {
        test.pump(
            declare(R"({"cols":0,"rows":25})") + declare(R"({"cols":40})")
            + declare(R"({"cols":513,"rows":25})") + declare(R"({"cols":40,"rows":25,"extra":1})"));
        CHECK(declared_viewport.empty());
        CHECK(contains(test.output.str(), R"("code":-32602)"));
    }
    SECTION("bn.viewport resizes and publishes the new view as one event") {
        test.pump(
            declare(R"({"cols":40,"rows":25})") + subscribe
            + rpc("bn.viewport", R"({"cols":60,"rows":30})", "5"));
        CHECK(declared_viewport == std::vector<std::pair<int, int>>{{40, 25}, {60, 30}});
        const auto out = test.output.str();
        CHECK(contains(out, R"("id":5,"result":{})"));
        CHECK(test.authority.current()->value.world.view->max == position{.x = 59, .y = 29});
        CHECK(contains(
            out,
            R"("changes":{"view":{"min":{"dim":"","x":0,"y":0,"z":0},"max":{"dim":"","x":59,"y":29,"z":0}}})"));
    }
    SECTION("bn.viewport needs a hello first") {
        test.pump(rpc("bn.viewport", R"({"cols":60,"rows":30})", "5"));
        CHECK(declared_viewport.empty());
        CHECK(contains(test.output.str(), R"("kind":"negotiation_failed")"));
    }
}

TEST_CASE("subscribe answers first and then sends the cell parts", "[engine_client_server]") {
    auto boundary = input_boundary{};
    auto test = fixture{};
    known_cells.clear();
    for (auto x = 0; x < static_cast<int>(cells_per_part) + 3; ++x) { known_cells.push_back(x); }
    test.pump(hello + subscribe);
    const auto out = test.output.str();
    const auto response = index_of(out, R"("id":2)");
    const auto first = index_of(out, R"("method":"bn.snapshot.part")");
    REQUIRE(response != std::string::npos);
    REQUIRE(first != std::string::npos);
    CHECK(response < first);
    CHECK(contains(out, R"("parts":2)"));
    CHECK(contains(out, R"("index":0,"last":false)"));
    CHECK(contains(out, R"("index":1,"last":true)"));
    CHECK(contains(
        out,
        R"("at":{"epoch":")" + test.authority.epoch() + R"(","sequence":"0","revision":"0"})"));
}

TEST_CASE("subscribing twice changes neither the clock nor the game", "[engine_client_server]") {
    auto boundary = input_boundary{};
    auto test = fixture{};
    REQUIRE(test.authority.publish_boundary());
    const auto before = test.authority.at();
    const auto rng = rng_get_engine();
    const auto actor = [] {
        auto output = std::ostringstream{};
        auto json = JsonOut{output};
        get_avatar().serialize(json);
        return output.str();
    };
    const auto actor_before = actor();
    test.pump(hello + subscribe + subscribe);
    CHECK(test.authority.at() == before);
    CHECK(rng_get_engine() == rng);
    CHECK(actor() == actor_before);
}

TEST_CASE("a command streams its stages around its events", "[engine_client_server]") {
    auto boundary = input_boundary{};
    auto test = fixture{};
    REQUIRE(test.authority.publish_boundary());
    REQUIRE(test.pump(
        hello + subscribe
        + test.submit_text(R"({"kind":"choose","choice_id":"choice:yes"})", test.schema())));
    CHECK(contains(test.output.str(), R"("stage":"received")"));
    CHECK(contains(test.output.str(), R"("method":"bn.command")"));
    CHECK(contains(test.output.str(), R"("stage":"validated")"));
    CHECK_FALSE(contains(test.output.str(), "executing"));

    test.run_native_input();
    known_cells.push_back(3); // the world changes while the input runs
    test.output.str("");
    CHECK_FALSE(test.pump(""));
    const auto out = test.output.str();
    const auto executing = index_of(out, R"("stage":"executing")");
    const auto events = index_of(out, R"("method":"bn.events")");
    const auto completed = index_of(out, R"("stage":"completed")");
    REQUIRE(executing != std::string::npos);
    REQUIRE(events != std::string::npos);
    REQUIRE(completed != std::string::npos);
    CHECK(executing < events);
    CHECK(events < completed);
    CHECK(contains(out, R"("type":"cells.seen")"));
    CHECK(contains(out, R"("command":"command)"));
    const auto now = test.authority.at();
    REQUIRE(now);
    CHECK(now->sequence == 1);
    CHECK(contains(out, R"("at":{"epoch":")" + now->epoch + R"(","sequence":"1","revision":"1"})"));
}

TEST_CASE(
    "a stale submit is rejected with its reason and delivers nothing", "[engine_client_server]") {
    auto boundary = input_boundary{};
    auto test = fixture{};
    REQUIRE(test.authority.publish_boundary());
    const auto stale = rpc(
        "bn.command.submit",
        R"({"epoch":")" + test.authority.epoch()
            + R"(","expect":{"revision":"99","boundary_id":"boundary:old","schema_id":null},"operation":{"kind":"cancel"}})",
        "3");
    const auto other = rpc(
        "bn.command.submit",
        R"({"epoch":"epoch:other","expect":{"revision":"0","boundary_id":"b","schema_id":null},"operation":{"kind":"cancel"}})",
        "4");
    CHECK_FALSE(test.pump(hello + subscribe + stale + other));
    CHECK(test.queued.empty());
    CHECK(contains(test.output.str(), R"("stage":"rejected","error":"stale_revision")"));
    CHECK(contains(test.output.str(), R"("kind":"stale_epoch")"));
}

TEST_CASE("bn.command.result recovers a lost notification", "[engine_client_server]") {
    auto boundary = input_boundary{};
    auto test = fixture{};
    REQUIRE(test.authority.publish_boundary());
    REQUIRE(test.pump(
        hello + test.submit_text(R"({"kind":"choose","choice_id":"choice:yes"})", test.schema())));
    const auto id = test.command_id(test.output.str());
    CHECK(contains(test.output.str(), R"("stage":"received")"));
    test.run_native_input();
    test.output.str("");
    const auto query = [&](const std::string& command, const std::string& n) {
        return rpc(
            "bn.command.result",
            R"({"epoch":")" + test.authority.epoch() + R"(","command_id":")" + command + R"("})",
            n);
    };
    CHECK_FALSE(test.pump(query(id, "9") + query("c:unknown", "10")));
    CHECK(contains(test.output.str(), R"("stage":"completed")")); // the answer, not a push
    CHECK(contains(test.output.str(), R"("kind":"unknown_command")"));
    CHECK_FALSE(contains(test.output.str(), R"("method":"bn.command")"));
}

TEST_CASE(
    "world replacement sends one resync and silences the stale stream", "[engine_client_server]") {
    auto boundary = input_boundary{};
    auto test = fixture{};
    REQUIRE(test.authority.publish_boundary());
    REQUIRE(test.pump(
        hello + subscribe
        + test.submit_text(R"({"kind":"choose","choice_id":"choice:yes"})", test.schema())));
    const auto old_epoch = test.authority.epoch();
    const auto lost_after = test.authority.at()->sequence;
    test.queued.clear();
    test.authority.replace_world();
    test.next_boundary(); // new epoch, first publication has no event
    known_cells.push_back(3);
    test.next_boundary(); // an event of the new epoch the client never subscribed to
    CHECK(test.authority.epoch() != old_epoch);
    REQUIRE(test.authority.at()->sequence == 1);
    test.output.str("");
    REQUIRE(test.pump(
        subscribe
        + test.submit_text(R"({"kind":"choose","choice_id":"choice:yes"})", test.schema())));
    const auto out = test.output.str();
    const auto interrupted = index_of(out, R"("stage":"interrupted","error":"stale_epoch")");
    const auto resync = index_of(out, R"("method":"bn.resync")");
    REQUIRE(interrupted != std::string::npos);
    REQUIRE(resync != std::string::npos);
    CHECK(interrupted < resync);
    CHECK(
        contains(out, R"("reason":"world_replaced","lost_after":")" + std::to_string(lost_after)));
    CHECK(contains(out, R"("epoch":")" + old_epoch));
    CHECK_FALSE(contains(out, "bn.events"));
    CHECK(index_of(out, R"("sequence":"1","revision":"1")") != std::string::npos); // the new
                                                                                   // snapshot
}

TEST_CASE("an unsubscribed client receives no pushes", "[engine_client_server]") {
    auto boundary = input_boundary{};
    auto test = fixture{};
    REQUIRE(test.authority.publish_boundary());
    REQUIRE(test.pump(
        hello + subscribe + rpc("bn.unsubscribe", "{}", "5")
        + test.submit_text(R"({"kind":"choose","choice_id":"choice:yes"})", test.schema())));
    test.run_native_input();
    known_cells.push_back(3);
    test.output.str("");
    CHECK_FALSE(test.pump(""));
    CHECK(test.output.str().empty());
}

TEST_CASE("interaction choices are a passive paged read", "[engine_client_server]") {
    auto boundary = input_boundary{};
    auto test = fixture{};
    REQUIRE(test.authority.publish_boundary());
    const auto before = test.authority.at();
    const auto id = test.authority.current()->value.interaction.id;
    test.pump(
        hello
        + rpc("bn.interaction.choices",
              R"({"epoch":")" + test.authority.epoch() + R"(","boundary_id":")" + id
                  + R"(","offset":1,"limit":5})",
              "6"));
    CHECK(contains(test.output.str(), R"("total":2)"));
    CHECK(contains(test.output.str(), R"("id":"choice:no")"));
    CHECK_FALSE(contains(test.output.str(), R"("id":"choice:yes")"));
    CHECK(test.authority.at() == before);
}

TEST_CASE("world cells are a passive read of the known box", "[engine_client_server]") {
    auto boundary = input_boundary{};
    auto test = fixture{};
    REQUIRE(test.authority.publish_boundary());
    const auto before = test.authority.at();
    const auto rng = rng_get_engine();
    const auto state_before = serialize_snapshot(*test.authority.current())->header;
    const auto epoch = test.authority.epoch();
    const auto cells =
        [&](const std::string& id, const std::string& min, const std::string& max,
            const std::string& with = {}) {
            return rpc(
                "bn.world.cells",
                R"({"epoch":")" + (with.empty() ? epoch : with) + R"(","min":)" + min + R"(,"max":)"
                    + max + "}",
                id);
        };
    const auto at = [](const int x, const int y, const int z = 0) {
        return R"({"dim":"","x":)" + std::to_string(x) + R"(,"y":)" + std::to_string(y) + R"(,"z":)"
             + std::to_string(z) + "}";
    };
    test.pump(
        hello + cells("3", at(0, 0), at(3, 0)) + cells("4", at(0, 0), at(3, 0), "epoch:old")
        + cells("5", at(3, 0), at(0, 0)) + cells("6", at(0, 0, 0), at(1, 0, 1))
        + cells("7", at(0, -5), at(1000, 5)) + cells("8", at(0, 0), at(1, 0)));
    const auto out = test.output.str();
    // Known squares are the snapshot's cells; unknown squares inside the coverage are forgotten.
    CHECK(contains(out, R"("id":3,"result":{"at":{"epoch":")" + epoch));
    CHECK(contains(
        out, R"("forgotten":[{"dim":"","x":0,"y":0,"z":0},{"dim":"","x":3,"y":0,"z":0}]})"));
    CHECK(contains(out, R"("id":"t_dirt")"));
    CHECK(contains(out, R"("known":"visible")"));
    CHECK(contains(out, R"("id":8,"result":{"at":{"epoch":")" + epoch));
    CHECK(contains(out, R"("forgotten":[{"dim":"","x":0,"y":0,"z":0}]})"));
    // A stale epoch, an inverted box, a box across levels and an oversized box are refused.
    CHECK(contains(out, R"("id":4,"error")"));
    CHECK(contains(out, R"("kind":"stale_epoch")"));
    CHECK(contains(out, R"("id":5,"error")"));
    CHECK(contains(out, R"("id":6,"error")"));
    CHECK(contains(out, R"("kind":"validation_failed")"));
    CHECK(contains(out, R"("id":7,"error")"));
    CHECK(contains(out, R"("kind":"resource_limit")"));
    CHECK(test.authority.at() == before);
    CHECK(rng_get_engine() == rng);
    CHECK(serialize_snapshot(*test.authority.current())->header == state_before);
}

TEST_CASE("a registered action is submittable as a command", "[engine_client_server]") {
    auto boundary = input_boundary{};
    auto test = fixture{};
    REQUIRE(test.authority.publish_boundary());
    REQUIRE(test.pump(
        hello + subscribe
        + test.submit_text(R"({"kind":"action","action_id":"YES"})", test.schema())));
    CHECK(contains(test.output.str(), R"("stage":"validated")"));
    REQUIRE(test.queued.size() == 1);
    CHECK(test.queued.front().action == "YES");
}

TEST_CASE("a capture that drops the avatar resynchronizes subscribers", "[engine_client_server]") {
    auto boundary = input_boundary{};
    auto test = fixture{};
    REQUIRE(test.authority.publish_boundary());
    test.tap.take();
    with_avatar = false;
    test.next_boundary();
    const auto pushed = test.tap.take();
    REQUIRE(pushed.size() == 1);
    const auto* notice = std::get_if<resync_notice>(&pushed.front());
    REQUIRE(notice);
    CHECK(notice->reason == "state_removed");
    CHECK(test.authority.current()->value.world.avatar == std::nullopt);
}

TEST_CASE(
    "a command that finishes in a resyncing publication still reports completed",
    "[engine_client_server]") {
    auto boundary = input_boundary{};
    auto test = fixture{};
    REQUIRE(test.authority.publish_boundary());
    REQUIRE(test.pump(
        hello + subscribe
        + test.submit_text(R"({"kind":"choose","choice_id":"choice:yes"})", test.schema())));
    test.run_native_input(); // executing, then the next native boundary
    test.output.str("");
    with_avatar = false;        // the change cannot be one event: subscribers start over
    CHECK_FALSE(test.pump("")); // the input boundary where the command ends
    const auto out = test.output.str();
    CHECK(contains(out, R"("method":"bn.resync")"));
    CHECK(contains(out, R"("stage":"completed")"));
}

TEST_CASE("shutdown flushes the stage changes it caused", "[engine_client_server]") {
    auto boundary = input_boundary{};
    auto test = fixture{};
    REQUIRE(test.authority.publish_boundary());
    REQUIRE(test.pump(
        hello + subscribe
        + test.submit_text(R"({"kind":"choose","choice_id":"choice:yes"})", test.schema())));
    const auto native = game_client::resolve_input_command(test.queued.front(), point{80, 24});
    REQUIRE(native);
    test.authority.delivered(*native); // executing; the game now quits without another boundary
    test.output.str("");
    REQUIRE(test.server.finish(test.output));
    const auto out = test.output.str();
    const auto executing = index_of(out, R"("stage":"executing")");
    const auto interrupted = index_of(out, R"("stage":"interrupted","error":"not_ready")");
    REQUIRE(executing != std::string::npos);
    REQUIRE(interrupted != std::string::npos);
    CHECK(executing < interrupted);
}

TEST_CASE("loading notifications reach only a client that said hello", "[engine_client_server]") {
    auto boundary = input_boundary{};
    auto test = fixture{};
    const auto progress = game_client::loading_progress{.title = "Loading", .entries = {"a"}};
    test.server.notify_loading(progress, test.output);
    CHECK(test.output.str().empty());

    REQUIRE(test.authority.publish_boundary());
    test.pump(hello);
    test.output.str("");
    test.server.notify_loading(progress, test.output);
    test.server.notify_loading({.done = true}, test.output);
    const auto out = test.output.str();
    CHECK(contains(out, R"("method":"bn.loading","params":{"epoch":")" + test.authority.epoch()));
    CHECK(contains(out, R"("title":"Loading","entries":["a"],"index":0})"));
    CHECK(index_of(out, R"("title":"Loading")") < index_of(out, R"("done":true)"));
}

TEST_CASE("a progress heartbeat reaches only a client that said hello", "[engine_client_server]") {
    auto boundary = input_boundary{};
    auto test = fixture{};
    test.server.heartbeat(test.output);
    CHECK(test.output.str().empty());

    REQUIRE(test.authority.publish_boundary());
    test.pump(hello);
    test.output.str("");
    test.server.heartbeat(test.output);
    const auto expected =
        R"({"jsonrpc":"2.0","method":"bn.progress","params":{"epoch":")" + test.authority.epoch()
        + "\"}}\n";
    CHECK(test.output.str() == expected);
}

TEST_CASE("a hello without delivery pushes every step as before", "[engine_client_delivery]") {
    auto boundary = input_boundary{};
    auto test = fixture{};
    REQUIRE(test.authority.publish_boundary());
    test.poll(hello + subscribe);
    test.output.str("");
    for (auto step = 0; step < 5; ++step) { test.step(); }
    CHECK(count_of(test.output.str(), R"("method":"bn.events")") == 5);
    CHECK_FALSE(contains(test.output.str(), "bn.resync"));
    CHECK_FALSE(contains(test.output.str(), "delivery"));
}

TEST_CASE("hello declares the delivery bound it granted", "[engine_client_delivery]") {
    auto boundary = input_boundary{};
    auto test = fixture{};
    REQUIRE(test.authority.publish_boundary());
    test.poll(hello_with(R"({"credits":2})"));
    CHECK(contains(
        test.output.str(), R"("delivery":{"credits":2,"held_frames":256,"held_bytes":1048576})"));
}

TEST_CASE("a delivery window outside 1..256 is invalid params", "[engine_client_delivery]") {
    auto boundary = input_boundary{};
    auto test = fixture{};
    REQUIRE(test.authority.publish_boundary());
    for (const auto* window :
         {R"({"credits":0})", R"({"credits":257})", R"({"credits":"2"})", R"({})",
          R"({"credits":2,"extra":1})"}) {
        test.output.str("");
        test.poll(hello_with(window));
        CHECK(contains(test.output.str(), R"("code":-32602)"));
    }
}

TEST_CASE("hello echoes the frame rate the client capped", "[engine_client_delivery]") {
    auto boundary = input_boundary{};
    auto test = fixture{};
    REQUIRE(test.authority.publish_boundary());
    test.poll(hello_with(R"({"credits":2,"max_hz":30})"));
    CHECK(
        contains(test.output.str(),
                 R"("delivery":{"credits":2,"held_frames":256,"held_bytes":1048576,"max_hz":30})"));
}

TEST_CASE("a frame rate outside 1..240 is invalid params", "[engine_client_delivery]") {
    auto boundary = input_boundary{};
    auto test = fixture{};
    REQUIRE(test.authority.publish_boundary());
    for (const auto* window :
         {R"({"credits":2,"max_hz":0})", R"({"credits":2,"max_hz":241})",
          R"({"credits":2,"max_hz":"30"})", R"({"credits":2,"max_hz":1.5})", R"({"max_hz":30})"}) {
        test.output.str("");
        test.poll(hello_with(window));
        CHECK(contains(test.output.str(), R"("code":-32602)"));
    }
    for (const auto* window : {R"({"credits":2,"max_hz":1})", R"({"credits":2,"max_hz":240})"}) {
        test.output.str("");
        test.poll(hello_with(window));
        CHECK_FALSE(contains(test.output.str(), R"("code":-32602)"));
    }
}

TEST_CASE(
    "a client's frame rate caps activity publication by wall time and loses no turn",
    "[engine_client_delivery]") {
    auto boundary = input_boundary{};
    auto test = fixture{};
    // Publishing is free, so only the client's rate paces; each turn takes one millisecond.
    auto now = std::chrono::milliseconds{0};
    test.clock = [&now] { return std::chrono::steady_clock::time_point{} + now; };
    Messages::clear_messages();
    REQUIRE(test.authority.publish_boundary());
    test.poll(hello_with(R"({"credits":256,"max_hz":10})") + subscribe);
    test.output.str("");
    const auto turns = 250;
    for (auto turn = 0; turn < turns; ++turn, ++now) {
        Messages::add_msg("turn " + std::to_string(turn) + " passed.");
        known_cells.push_back(static_cast<int>(known_cells.size()) + 1);
        REQUIRE(test.server.publish_activity(test.output));
    }
    // At 0, 100 and 200 ms.
    CHECK(count_of(test.output.str(), R"("method":"bn.events")") == 3);
    // The boundary the activity ends at is published whatever the rate, with every turn in order.
    test.step();
    auto from = std::size_t{0};
    for (auto turn = 0; turn < turns; ++turn) {
        const auto at = test.output.str().find("turn " + std::to_string(turn) + " passed.", from);
        CHECK(at != std::string::npos);
        if (at != std::string::npos) { from = at; }
    }
}

TEST_CASE(
    "a client's frame rate does not outpace the fifth of the time publishing may take",
    "[engine_client_delivery]") {
    auto boundary = input_boundary{};
    auto test = fixture{};
    // Every look at the clock takes one millisecond: the cost cap is 5 ms, the rate's 1 ms.
    auto ticks = 0;
    test.clock = [&ticks] {
        return std::chrono::steady_clock::time_point{} + std::chrono::milliseconds{ticks++};
    };
    REQUIRE(test.authority.publish_boundary());
    test.poll(hello_with(R"({"credits":256,"max_hz":240})") + subscribe);
    test.output.str("");
    const auto turns = 40;
    for (auto turn = 0; turn < turns; ++turn) {
        known_cells.push_back(static_cast<int>(known_cells.size()) + 1);
        REQUIRE(test.server.publish_activity(test.output));
    }
    CHECK(count_of(test.output.str(), R"("method":"bn.events")") == turns / 4);
}

TEST_CASE("pushed frames wait for credits and then arrive in order", "[engine_client_delivery]") {
    auto boundary = input_boundary{};
    auto test = fixture{};
    REQUIRE(test.authority.publish_boundary());
    test.poll(hello_with(R"({"credits":2})") + subscribe);
    test.output.str("");
    for (auto step = 0; step < 5; ++step) { test.step(); }
    CHECK(count_of(test.output.str(), R"("method":"bn.events")") == 2);
    CHECK_FALSE(contains(test.output.str(), "bn.resync"));

    test.poll(rpc("bn.credit", R"({"frames":3})", "9"));
    const auto out = test.output.str();
    CHECK(count_of(out, R"("method":"bn.events")") == 5);
    CHECK(contains(out, R"("id":9,"result":{"credits":3})"));
    CHECK(index_of(out, R"("sequence":"1")") < index_of(out, R"("sequence":"5")"));
    CHECK(index_of(out, R"("id":9)") < index_of(out, R"("sequence":"3")"));
}

TEST_CASE(
    "credits are capped at the window and need a negotiated delivery", "[engine_client_delivery]") {
    auto boundary = input_boundary{};
    auto test = fixture{};
    REQUIRE(test.authority.publish_boundary());
    test.poll(hello + rpc("bn.credit", R"({"frames":1})", "9"));
    CHECK(contains(test.output.str(), R"("kind":"negotiation_failed")"));

    auto capped = fixture{};
    capped.poll(hello_with(R"({"credits":200})") + rpc("bn.credit", R"({"frames":100})", "9"));
    CHECK(contains(capped.output.str(), R"("id":9,"result":{"credits":256})"));
    capped.output.str("");
    capped.poll(rpc("bn.credit", R"({"frames":0})", "10"));
    CHECK(contains(capped.output.str(), R"("code":-32602)"));
}

TEST_CASE("a client that grants no credits is resynced, not buffered", "[engine_client_delivery]") {
    auto boundary = input_boundary{};
    auto test = fixture{};
    REQUIRE(test.authority.publish_boundary());
    test.poll(hello_with(R"({"credits":1})") + subscribe);
    test.output.str("");
    auto most_frames = std::size_t{0};
    auto most_bytes = std::size_t{0};
    for (auto step = 0; step < 300; ++step) {
        test.step();
        most_frames = std::max(most_frames, test.server.held_frames());
        most_bytes = std::max(most_bytes, test.server.held_bytes());
    }
    CHECK(most_frames == maximum_held_frames);
    CHECK(most_bytes <= maximum_held_bytes);
    CHECK(test.server.held_frames() == 0);
    const auto out = test.output.str();
    CHECK(count_of(out, R"("method":"bn.events")") == 1);
    REQUIRE(count_of(out, R"("method":"bn.resync")") == 1);
    CHECK(contains(out, R"("reason":"backpressure","lost_after":"1")"));
    // Its snapshot, not a replay, is the way back; credits keep their count.
    test.output.str("");
    test.poll(subscribe + rpc("bn.credit", R"({"frames":1})", "9"));
    CHECK(contains(test.output.str(), R"("method":"bn.snapshot.part")"));
    test.output.str("");
    test.step();
    CHECK(count_of(test.output.str(), R"("method":"bn.events")") == 1);
}

TEST_CASE(
    "a running activity is published only to a client with credits", "[engine_client_delivery]") {
    auto boundary = input_boundary{};
    auto test = fixture{};
    REQUIRE(test.authority.publish_boundary());
    test.poll(hello + subscribe);
    test.output.str("");
    known_cells.push_back(3);
    CHECK(test.server.publish_activity(test.output));
    CHECK(test.output.str().empty());
    CHECK(test.authority.at()->sequence == 0);

    auto credited = fixture{};
    REQUIRE(credited.authority.publish_boundary());
    credited.poll(hello_with(R"({"credits":2})") + subscribe);
    credited.output.str("");
    known_cells.push_back(4);
    CHECK(credited.server.publish_activity(credited.output));
    CHECK(count_of(credited.output.str(), R"("method":"bn.events")") == 1);
}

TEST_CASE(
    "a long activity is published at a pace of a fifth of its time and loses no turn",
    "[engine_client_delivery]") {
    auto boundary = input_boundary{};
    auto test = fixture{};
    // Every look at the clock takes one millisecond, publishing included.
    auto ticks = 0;
    test.clock = [&ticks] {
        return std::chrono::steady_clock::time_point{} + std::chrono::milliseconds{ticks++};
    };
    Messages::clear_messages();
    REQUIRE(test.authority.publish_boundary());
    test.poll(hello_with(R"({"credits":256})") + subscribe);
    test.output.str("");
    const auto turns = 40;
    for (auto turn = 0; turn < turns; ++turn) {
        Messages::add_msg("turn " + std::to_string(turn) + " passed.");
        known_cells.push_back(static_cast<int>(known_cells.size()) + 1);
        REQUIRE(test.server.publish_activity(test.output));
    }
    // One publication, then four times as long as it took without one.
    CHECK(count_of(test.output.str(), R"("method":"bn.events")") == turns / 4);
    // The boundary the activity ends at carries what the turns in between left unpublished.
    test.step();
    auto from = std::size_t{0};
    for (auto turn = 0; turn < turns; ++turn) {
        const auto at = test.output.str().find("turn " + std::to_string(turn) + " passed.", from);
        CHECK(at != std::string::npos);
        if (at != std::string::npos) { from = at; }
    }
    CHECK(contains(test.output.str(), "cells.seen"));
}

TEST_CASE(
    "a step of a running activity does not complete its command", "[engine_client_delivery]") {
    auto boundary = input_boundary{};
    auto test = fixture{};
    REQUIRE(test.authority.publish_boundary());
    REQUIRE(test.pump(
        hello_with(R"({"credits":8})") + subscribe
        + test.submit_text(R"({"kind":"choose","choice_id":"choice:yes"})", test.schema())));
    test.run_native_input();
    known_cells.push_back(3);
    test.output.str("");
    CHECK(test.server.publish_activity(test.output));
    CHECK(contains(test.output.str(), R"("type":"cells.seen")"));
    CHECK_FALSE(contains(test.output.str(), R"("stage":"completed")"));
    known_cells.push_back(4);
    CHECK_FALSE(test.pump(""));
    CHECK(contains(test.output.str(), R"("stage":"completed")"));
}

TEST_CASE(
    "the game is identical with a pushing, a stalled and no client", "[engine_client_delivery]") {
    struct run {
        counter sequence;
        counter revision;
        cata_default_random_engine rng;
    };
    const auto play = [](const std::string& opening) {
        auto boundary = input_boundary{};
        auto test = fixture{};
        REQUIRE(test.authority.publish_boundary());
        if (!opening.empty()) { test.poll(opening); }
        for (auto step = 0; step < 300; ++step) { test.step(); }
        const auto at = test.authority.at();
        REQUIRE(at);
        return run{at->sequence, at->revision, rng_get_engine()};
    };
    const auto none = play("");
    const auto pushing = play(hello_with(R"({"credits":256})") + subscribe);
    const auto stalled = play(hello_with(R"({"credits":1})") + subscribe);
    CHECK(pushing.sequence == none.sequence);
    CHECK(pushing.revision == none.revision);
    CHECK(stalled.sequence == none.sequence);
    CHECK(stalled.revision == none.revision);
    CHECK(pushing.rng == none.rng);
    CHECK(stalled.rng == none.rng);
}

namespace {
/// The JSON-RPC error codes in `text`, in order.
auto error_codes(const std::string& text) -> std::vector<std::string> {
    auto codes = std::vector<std::string>{};
    for (auto at = text.find(R"("code":)"); at != std::string::npos;
         at = text.find(R"("code":)", at + 1)) {
        codes.push_back(text.substr(at + 7, text.find_first_of(",}", at) - at - 7));
    }
    return codes;
}
} // namespace

TEST_CASE(
    "a malformed frame gets its JSON-RPC error and changes nothing", "[engine_client_robustness]") {
    struct bad_frame {
        std::string line;
        std::string code; ///< empty when the engine owes no reply
    };
    const auto frames = std::vector<bad_frame>{
        {"{not json", "-32700"},
        {"", ""},
        {"[]", "-32600"},
        {"42", "-32700"}, // historical: a scalar frame is a parse error
        {"null", "-32700"},
        {R"({"jsonrpc":"2.0"})", "-32600"},
        {R"({"jsonrpc":"1.0","method":"bn.subscribe","id":4})", "-32600"},
        {R"({"jsonrpc":"2.0","method":5,"id":4})", "-32602"}, // historical shape error
        {R"({"jsonrpc":"2.0","method":"bn.subscribe","id":{}})", "-32600"},
        {R"({"jsonrpc":"2.0","method":"bn.nope","params":{},"id":4})", "-32601"},
        {R"({"jsonrpc":"2.0","method":"bn.credit","params":[],"id":4})", "-32602"},
        {R"({"jsonrpc":"2.0","method":"bn.credit","params":{"frames":"1"},"id":4})", "-32602"},
        {R"({"jsonrpc":"2.0","method":"bn.nope","params":{}})", ""},
        {R"({"jsonrpc":"2.0","method":"bn.credit","params":[]})", ""},
    };
    for (const auto* delivery : {"", R"({"credits":2})"}) {
        for (const auto activity : {false, true}) {
            auto boundary = input_boundary{};
            auto test = fixture{};
            REQUIRE(test.authority.publish_boundary());
            test.poll((std::string{delivery}.empty() ? hello : hello_with(delivery)) + subscribe);
            // An activity that is pushing: two credits are spent, the rest of its frames are held.
            if (activity) {
                for (auto step = 0; step < 6; ++step) { test.step(); }
            }
            const auto held = test.server.held_frames();
            const auto sequence = test.authority.at()->sequence;
            for (const auto& frame : frames) {
                CAPTURE(frame.line, delivery, activity);
                test.output.str("");
                test.poll(frame.line + "\n");
                const auto codes = error_codes(test.output.str());
                CHECK(
                    codes
                    == (frame.code.empty() ? std::vector<std::string>{} : std::vector{frame.code}));
                CHECK(test.server.held_frames() == held);
                CHECK_FALSE(test.server.failed());
            }
            CHECK(test.authority.at()->sequence == sequence);
            // The connection still serves a valid request, and the held frames follow it in order.
            test.output.str("");
            test.poll(rpc("bn.credit", R"({"frames":9})", "9"));
            CHECK(contains(test.output.str(), R"("id":9)"));
            if (activity && std::string{delivery}.size()) {
                CHECK(test.server.held_frames() == 0);
                CHECK(count_of(test.output.str(), R"("method":"bn.events")") == 4);
            }
        }
    }
}

TEST_CASE(
    "an oversized or unterminated frame closes the connection", "[engine_client_robustness]") {
    struct bad_input {
        std::string text;
        std::string reason;
    };
    const auto limit = bn::mcp::server::default_max_frame_bytes;
    const auto inputs = std::vector<bad_input>{
        {std::string(limit + 1, 'x') + "\n", "exceeds"},
        {std::string(8 * limit, ' '), "exceeds"},
        {R"({"jsonrpc":"2.0","method":"bn.subscribe","id":2})", "unterminated"},
    };
    for (const auto* delivery : {"", R"({"credits":2})"}) {
        for (const auto& input : inputs) {
            auto boundary = input_boundary{};
            auto test = fixture{};
            REQUIRE(test.authority.publish_boundary());
            test.poll((std::string{delivery}.empty() ? hello : hello_with(delivery)) + subscribe);
            for (auto step = 0; step < 6; ++step) { test.step(); }
            test.output.str("");
            auto text = std::istringstream{input.text};
            CHECK_FALSE(test.server.poll_input(text, test.output, test.errors));
            CHECK(test.server.failed());
            CHECK(contains(test.errors.str(), input.reason));
            // Nothing was dispatched, and the frames the client had credits for were not lost.
            CHECK_FALSE(contains(test.output.str(), "result"));
            CHECK(test.server.held_bytes() <= maximum_held_bytes);
            // At most one frame's worth was read before giving up.
            text.clear();
            CHECK(static_cast<std::size_t>(text.tellg()) <= limit + 1);
        }
    }
}

TEST_CASE("a step of an auto-move does not complete its command", "[engine_client_delivery]") {
    auto boundary = input_boundary{};
    auto test = fixture{};
    REQUIRE(test.authority.publish_boundary());
    REQUIRE(test.pump(
        hello + subscribe
        + test.submit_text(R"({"kind":"choose","choice_id":"choice:yes"})", test.schema())));
    test.run_native_input();
    known_cells.push_back(3);
    test.output.str("");
    CHECK(test.server.publish_step(test.output));
    CHECK(contains(test.output.str(), R"("type":"cells.seen")"));
    CHECK(contains(test.output.str(), R"("command":")"));
    CHECK_FALSE(contains(test.output.str(), R"("stage":"completed")"));
    known_cells.push_back(4);
    CHECK_FALSE(test.pump(""));
    CHECK(contains(test.output.str(), R"("stage":"completed")"));
}

namespace {
/// A client with credits whose command has begun executing: the game now runs an activity.
struct running_activity {
    input_boundary boundary;
    fixture test;
    explicit running_activity(const std::string& opening = hello_with(R"({"credits":2})")) {
        REQUIRE(test.authority.publish_boundary());
        REQUIRE(test.pump(
            opening + subscribe
            + test.submit_text(R"({"kind":"choose","choice_id":"choice:yes"})", test.schema())));
        test.run_native_input();
        test.output.str("");
    }
};
} // namespace

TEST_CASE(
    "requests that start work are refused while an activity runs", "[engine_client_robustness]") {
    auto run = running_activity{};
    auto& test = run.test;
    const auto at = test.authority.at();
    test.poll(
        test.submit_text(R"({"kind":"choose","choice_id":"choice:yes"})", test.schema())
        + rpc("bn.interrupt", "{}", "5"));
    // Commands wait for an input boundary; an interrupt is the one control an activity accepts.
    CHECK(contains(test.output.str(), R"("kind":"not_ready")"));
    CHECK(test.queued.size() == 1);
    CHECK(test.queued.front().action == "pause");
    CHECK(test.authority.at() == at);
    CHECK_FALSE(contains(test.output.str(), R"("stage":"completed")"));
}

TEST_CASE(
    "reads and credits during an activity neither complete nor restart its command",
    "[engine_client_robustness]") {
    auto run = running_activity{hello_with(R"({"credits":256})")};
    auto& test = run.test;
    for (auto step = 0; step < 4; ++step) { test.step(); }
    const auto at = test.authority.at();
    test.output.str("");
    test.poll(
        rpc("bn.viewport", R"({"cols":40,"rows":20})", "4") + subscribe
        + rpc("bn.credit", R"({"frames":1})", "5"));
    CHECK_FALSE(contains(test.output.str(), R"("stage":"completed")"));
    CHECK(error_codes(test.output.str()).empty());
    CHECK(test.queued.empty());
    CHECK(test.authority.at()->sequence >= at->sequence);
    test.step();
    CHECK_FALSE(contains(test.output.str(), R"("stage":"completed")"));
    CHECK_FALSE(test.pump(""));
    CHECK(count_of(test.output.str(), R"("stage":"completed")") == 1);
}

TEST_CASE("a second hello starts over from a snapshot", "[engine_client_robustness]") {
    auto run = running_activity{};
    auto& test = run.test;
    for (auto step = 0; step < 4; ++step) { test.step(); }
    CHECK(test.server.held_frames() > 0);
    test.output.str("");
    test.poll(hello_with(R"({"credits":8})"));
    CHECK(test.server.held_frames() == 0);
    // The dropped frames are not followed by a stream with a hole in it.
    test.output.str("");
    test.step();
    CHECK(count_of(test.output.str(), R"("method":"bn.events")") == 0);
}

TEST_CASE("shutdown drops the frames that credits cannot pay for", "[engine_client_robustness]") {
    auto run = running_activity{};
    auto& test = run.test;
    for (auto step = 0; step < 6; ++step) { test.step(); }
    const auto held = test.server.held_frames();
    CHECK(held > 0);
    test.output.str("");
    CHECK(test.server.finish(test.output));
    // The interruption is told only if it can be paid for, and nothing blocks on the rest.
    CHECK(count_of(test.output.str(), R"("method":"bn.events")") == 0);
    CHECK(test.server.held_bytes() <= maximum_held_bytes);
    test.poll(rpc("bn.credit", R"({"frames":256})", "9"));
    CHECK(test.server.held_frames() == 0);
}

namespace {
/// A second connection on the controller's session that may only read.
struct spectator {
    std::ostringstream output;
    std::ostringstream errors;
    std::vector<bn::mcp::key_event> submitted;
    std::vector<std::pair<int, int>> resized;
    bn::mcp::server server;
    explicit spectator(fixture& test)
        : server{
              {
                  .observe = [] { return bn::mcp::screen_snapshot{.json = "{}"}; },
                  .submit =
                      [this](const auto& input) {
                          submitted = input;
                          return true;
                      },
                  .has_input = [] { return false; },
                  .resize_viewport =
                      [this](const int cols, const int rows) { resized.push_back({cols, rows}); },
                  .contract_session = &test.authority,
              },
              {.now = [&test] { return test.clock(); }, .spectator = true}} {
        test.on_published = [this] { REQUIRE(server.flush_pushed(output)); };
    }
    auto read(const std::string& text) -> void {
        auto input = std::istringstream{text};
        for (auto lines = count_of(text, "\n"); lines > 0; --lines) {
            REQUIRE(server.poll_input(input, output, errors));
        }
    }
};
} // namespace

TEST_CASE(
    "a spectator reads the same events as the controller, in the same order",
    "[engine_client_spectator]") {
    auto boundary = input_boundary{};
    auto test = fixture{};
    REQUIRE(test.authority.publish_boundary());
    auto watcher = spectator{test};
    test.poll(hello + subscribe);
    watcher.read(hello + subscribe);
    CHECK(contains(watcher.output.str(), R"("method":"bn.snapshot.part")"));
    test.output.str("");
    watcher.output.str("");
    test.step();
    test.step();
    CHECK(count_of(test.output.str(), R"("method":"bn.events")") == 2);
    CHECK(watcher.output.str() == test.output.str());
}

TEST_CASE("a spectator is refused every call that commands the game", "[engine_client_spectator]") {
    auto boundary = input_boundary{};
    auto test = fixture{};
    REQUIRE(test.authority.publish_boundary());
    auto watcher = spectator{test};
    watcher.read(
        hello + subscribe + rpc("initialize", "{}", "3")
        + rpc("notifications/initialized", "{}", ""));
    const auto at = test.authority.at();
    watcher.output.str("");
    watcher.read(
        test.submit_text(R"({"kind":"choose","choice_id":"choice:yes"})", test.schema())
        + rpc("bn.interrupt", "{}", "5") + rpc("bn.viewport", R"({"cols":40,"rows":20})", "6")
        + rpc("tools/call", R"({"name":"bn.press","arguments":{"keys":[{"key":"ESCAPE"}]}})", "7"));
    CHECK(count_of(watcher.output.str(), R"("kind":"read_only")") == 4);
    CHECK(watcher.submitted.empty());
    CHECK(test.queued.empty());
    CHECK(declared_viewport.empty());
    CHECK(watcher.resized.empty());
    CHECK(test.authority.at() == at);
    CHECK_FALSE(test.authority.has_received());
}

TEST_CASE("a spectator's hello leaves the controller's view alone", "[engine_client_spectator]") {
    auto boundary = input_boundary{};
    auto test = fixture{};
    REQUIRE(test.authority.publish_boundary());
    auto watcher = spectator{test};
    watcher.read(rpc(
        "bn.hello",
        R"({"versions":["1.0"],"client":{"name":"w","version":"1"},"viewport":{"cols":10,"rows":10}})",
        "1"));
    CHECK(contains(watcher.output.str(), R"("version":"1.0")"));
    CHECK(watcher.resized.empty());
    CHECK(declared_viewport.empty());
}

TEST_CASE(
    "a spectator that drops or fails does not end the controller's command",
    "[engine_client_spectator]") {
    auto boundary = input_boundary{};
    auto test = fixture{};
    REQUIRE(test.authority.publish_boundary());
    auto watcher = spectator{test};
    watcher.read(hello + subscribe);
    REQUIRE(test.pump(
        hello + subscribe
        + test.submit_text(R"({"kind":"choose","choice_id":"choice:yes"})", test.schema())));
    test.run_native_input();
    // The spectator's end: a clean finish, and a write failure that fails its own server.
    CHECK(watcher.server.finish(watcher.output));
    watcher.output.setstate(std::ios::badbit);
    auto flushed = true;
    test.on_published = [&] { flushed = watcher.server.flush_pushed(watcher.output); };
    known_cells.push_back(3);
    test.output.str("");
    CHECK_FALSE(test.pump(""));
    CHECK(contains(test.output.str(), R"("stage":"completed")"));
    CHECK_FALSE(contains(test.output.str(), R"("stage":"interrupted")"));
    CHECK_FALSE(flushed);
    CHECK(watcher.server.failed());
}

TEST_CASE(
    "each connection keeps its own credits, held frames and subscription",
    "[engine_client_spectator]") {
    auto boundary = input_boundary{};
    auto test = fixture{};
    REQUIRE(test.authority.publish_boundary());
    auto watcher = spectator{test};
    watcher.read(hello_with(R"({"credits":1})") + subscribe);
    test.poll(hello_with(R"({"credits":256})") + subscribe);
    test.output.str("");
    watcher.output.str("");
    for (auto step = 0; step < 6; ++step) { test.step(); }
    // The stalled spectator holds its frames and never slows the controller's stream.
    CHECK(watcher.server.held_frames() == 5);
    CHECK(test.server.held_frames() == 0);
    CHECK(count_of(test.output.str(), R"("method":"bn.events")") == 6);
    CHECK(count_of(watcher.output.str(), R"("method":"bn.events")") == 1);
    // Past its bound the spectator is resynchronized on its own; the controller is untouched.
    for (auto step = 0; step < static_cast<int>(maximum_held_frames); ++step) { test.step(); }
    CHECK(contains(watcher.output.str(), R"("reason":"backpressure")"));
    CHECK_FALSE(contains(test.output.str(), R"("method":"bn.resync")"));
    watcher.output.str("");
    watcher.read(rpc("bn.unsubscribe", "{}", "8"));
    test.step();
    CHECK_FALSE(contains(watcher.output.str(), R"("method":"bn.events")"));
    CHECK(contains(test.output.str(), R"("method":"bn.events")"));
}

#endif
