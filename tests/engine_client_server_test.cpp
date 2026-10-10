#if defined(CATA_MCP)

#    include "catch/catch.hpp"
#    include "client_input.h"
#    include "engine_client_session.h"
#    include "input.h"
#    include "mcp_server.h"

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

auto world_cell(const int x) -> cell {
    return {.at = {.x = x},
            .known = knowledge::visible,
            .terrain = look{.kind = "terrain", .id = "t_dirt", .glyph = ".", .color = "brown"}};
}
auto known_cells = std::vector<int>{1, 2};
auto capture_test_world() -> world_state {
    auto result = world_state{};
    result.coverage = bounds{.min = {.x = 0, .y = -5, .z = 0}, .max = {.x = 1000, .y = 5, .z = 0}};
    for (const auto x : known_cells) { result.cells[{.x = x}] = world_cell(x); }
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
    std::vector<bn::mcp::key_event> queued;
    std::ostringstream output;
    std::ostringstream errors;
    bn::mcp::server server{{
        .observe = [] { return bn::mcp::screen_snapshot{.json = "{}"}; },
        .submit =
            [this](const auto& input) {
                queued = input;
                return true;
            },
        .has_input = [this] { return !queued.empty(); },
        .contract_session = &authority,
    }};
    fixture() {
        known_cells = {1, 2};
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
    test.pump(hello + subscribe + subscribe);
    CHECK(test.authority.at() == before);
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
    const auto interrupted = index_of(out, R"("stage":"interrupted")");
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

#endif
