#if defined(CATA_MCP)

#    include "catch/catch.hpp"
#    include "client_input.h"
#    include "engine_client_component.h"
#    include "engine_client_session.h"
#    include "engine_client_world.h"
#    include "input.h"
#    include "mcp_server.h"

#    include <algorithm>
#    include <nlohmann/json.hpp>
#    include <sstream>
#    include <string>
#    include <vector>

namespace {
using namespace engine_client;

auto rpc(const std::string& method, const std::string& params, const std::string& id)
    -> std::string {
    return R"({"jsonrpc":"2.0","method":")" + method + R"(","params":)" + params
         + (id.empty() ? "" : R"(,"id":)" + id) + "}\n";
}
auto call(const std::string& id, const std::string& name, const std::string& arguments = "{}")
    -> std::string {
    return rpc("tools/call", R"({"name":")" + name + R"(","arguments":)" + arguments + "}", id);
}
/// One frame that holds the requests `lines`, each written by `rpc`.
auto batch(const std::vector<std::string>& lines) -> std::string {
    auto result = std::string{};
    for (const auto& line : lines) {
        result += (result.empty() ? "[" : ",") + line.substr(0, line.size() - 1);
    }
    return result + "]\n";
}
const auto handshake =
    rpc("initialize", R"({"protocolVersion":"2025-06-18"})", "1")
    + rpc("notifications/initialized", "{}", "");

/// The response to request `id` in `output`, alone or in a batch; null when none was written.
auto answer(const std::string& output, const int id) -> nlohmann::json {
    auto lines = std::istringstream{output};
    for (auto line = std::string{}; std::getline(lines, line);) {
        const auto frame = nlohmann::json::parse(line);
        for (const auto& response : frame.is_array() ? frame : nlohmann::json::array({frame})) {
            if (response.value("id", nlohmann::json{}) == id) { return response; }
        }
    }
    return nullptr;
}
auto is_tool_error(const nlohmann::json& response) -> bool {
    return response.at("result").value("isError", false);
}

auto capture_world() -> world_state {
    auto result = world_state{};
    result.cells[{.x = 1}] = cell{.at = {.x = 1}, .known = knowledge::visible};
    result.avatar = avatar_value{.id = "e:avatar", .at = {.x = 1}, .name = "Ada"};
    return result;
}
/// An avatar whose name makes the state header about 230 kB: two `bn.state` results fit a frame,
/// three do not.
auto capture_large_world() -> world_state {
    auto result = capture_world();
    result.avatar->name = std::string(230000, 'a');
    return result;
}

struct fixture {
    input_context context{"YESNO"};
    game_client::input_context_scope input{context, "YESNO"};
    session authority;
    std::vector<bn::mcp::key_event> queued;
    std::ostringstream output;
    std::ostringstream errors;
    bn::mcp::server server{{
        .submit = [this](const auto& keys) { return queued = keys, true; },
        .has_input = [this] { return !queued.empty(); },
        .contract_session = &authority,
    }};
    fixture() {
        context.register_action("YES");
        game_client::begin_input_boundary();
        authority.set_world_capture(&capture_world);
    }
    /// Feed `text` and stop at the next native input; true when input was queued.
    auto pump(const std::string& text) -> bool {
        auto in = std::istringstream{text};
        return server.pump_until_input(in, output, errors);
    }
    /// Answer `text` outside an input boundary, as an activity does.
    auto run(const std::string& text) -> void {
        auto in = std::istringstream{text};
        REQUIRE(server.run(in, output, errors) == 0);
    }
    /// Answer `text` while an activity runs, which reaches no input boundary.
    auto poll(const std::string& text) -> bool {
        auto in = std::istringstream{text};
        return server.poll_input(in, output, errors);
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

TEST_CASE("MCP answers only ping and the handshake before initialization", "[mcp]") {
    auto test = fixture{};
    // tools/list before the handshake is refused; a notification with any params stays silent.
    test.run(
        rpc("tools/list", "{}", "2") + rpc("ping", "{}", "3")
        + rpc("notifications/initialized", "[1]", "") + handshake + rpc("tools/list", "{}", "4")
        + rpc("tools/call", R"({"name":"bn.observe","extra":1})", "5"));
    const auto out = test.output.str();
    CHECK(answer(out, 2).at("error").at("code") == -32002);
    CHECK(answer(out, 3).at("result") == nlohmann::json::object());
    CHECK(answer(out, 1).at("result").at("protocolVersion") == "2025-06-18");
    auto names = std::vector<std::string>{};
    for (const auto& tool : answer(out, 4).at("result").at("tools")) {
        names.push_back(tool.at("name"));
    }
    CHECK(names
          == std::vector<std::string>{
              "bn.observe", "bn.state", "bn.press", "bn.actions", "bn.interaction", "bn.interact"});
    CHECK(answer(out, 5).at("error").at("code") == -32602);
    CHECK(std::count(out.begin(), out.end(), '\n') == 5);
}

TEST_CASE("bn.press answers with the screen of the next input boundary", "[mcp]") {
    auto test = fixture{};
    const auto press = call("5", "bn.press", R"({"keys":[{"action":"YES"}]})");
    SECTION("the answer waits until the input is delivered") {
        REQUIRE(test.pump(handshake + press));
        CHECK(answer(test.output.str(), 5).is_null());
        test.run_native_input();
        CHECK_FALSE(test.pump(""));
        const auto response = answer(test.output.str(), 5);
        CHECK_FALSE(is_tool_error(response));
        CHECK(response.at("result").at("structuredContent").contains("rows"));
    }
    SECTION("a press while another waits is refused and leaves the waiting keys") {
        REQUIRE(test.pump(handshake + press));
        // An activity reads `bn.press` 6 before the input of 5 reaches its boundary.
        CHECK(test.poll(call("6", "bn.press", R"({"keys":[{"key":"n"}]})")));
        CHECK(is_tool_error(answer(test.output.str(), 6)));
        REQUIRE(test.queued.size() == 1);
        CHECK(test.queued.front().action == "YES");
        CHECK(answer(test.output.str(), 5).is_null());
    }
    SECTION("shutdown writes the batch that holds the waiting answer") {
        // `[ping 2, bn.press 5, ping 4]`: the press ends the game before 4 is read.
        REQUIRE(
            test.pump(handshake + batch({rpc("ping", "{}", "2"), press, rpc("ping", "{}", "4")})));
        test.run_native_input();
        // As `finish_session` does.
        test.server.finish_pending(test.output, test.errors);
        const auto out = test.output.str();
        CHECK(answer(out, 2).at("result") == nlohmann::json::object());
        CHECK(answer(out, 5).at("result").at("structuredContent").contains("rows"));
        CHECK(answer(out, 4).contains("error"));
    }
    SECTION("a client of the engine contract owns the input") {
        CHECK_FALSE(test.pump(
            rpc("bn.hello", R"({"versions":["0.3"],"client":{"name":"test","version":"1"}})", "9")
            + handshake + press));
        CHECK(test.queued.empty());
        CHECK(answer(test.output.str(), 5).at("error").at("code") == 1000);
    }
}

TEST_CASE("bn.press refuses keys outside its schema", "[mcp]") {
    auto test = fixture{};
    // Two modes, a negative or fractional number, an unknown button or member, a lone modifier.
    const auto keys = std::vector<std::string>{
        R"({"action":"YES","key":"y"})",
        R"({"mouse":{"x":-1,"y":0,"button":"left"}})",
        R"({"mouse":{"x":0,"y":0,"button":"middle"}})",
        R"({"key":"y","input_id":1.5})",
        R"({"key":"y","repeat":2})",
        R"({"text":"y","modifiers":["CTRL"]})",
    };
    auto requests = handshake;
    for (auto id = 0; id < static_cast<int>(keys.size()); ++id) {
        requests += call(std::to_string(10 + id), "bn.press", R"({"keys":[)" + keys[id] + "]}");
    }
    test.run(requests);
    for (auto id = 0; id < static_cast<int>(keys.size()); ++id) {
        CAPTURE(keys[id]);
        CHECK(is_tool_error(answer(test.output.str(), 10 + id)));
    }
    CHECK(test.queued.empty());
}

TEST_CASE("bn.interact invokes an affordance of the top screen", "[mcp]") {
    auto test = fixture{};
    const auto screen = screen_scope{test.context, "test"};
    SECTION("the next top screen answers once the input is delivered") {
        REQUIRE(test.pump(
            handshake + call("6", "bn.interaction")
            + call("7", "bn.interact", R"({"affordance_id":"YES"})")));
        const auto top = answer(test.output.str(), 6).at("result").at("structuredContent");
        CHECK(top.at("screen_id").get<std::string>()
              == top_screen(*test.authority.boundary()).screen_id);
        CHECK(top.at("affordances").dump().find(R"("id":"YES")") != std::string::npos);
        CHECK(answer(test.output.str(), 7).is_null());
        test.run_native_input();
        CHECK_FALSE(test.pump(""));
        const auto response = answer(test.output.str(), 7);
        CHECK_FALSE(is_tool_error(response));
        CHECK(response.at("result").at("structuredContent").contains("screen_id"));
    }
    SECTION("an affordance the screen does not offer is a tool error") {
        CHECK_FALSE(
            test.pump(handshake + call("7", "bn.interact", R"({"affordance_id":"bn:nothing"})")));
        CHECK(test.queued.empty());
        CHECK(is_tool_error(answer(test.output.str(), 7)));
    }
    SECTION("a command interrupted before it executes is a tool error") {
        REQUIRE(test.pump(handshake + call("7", "bn.interact", R"({"affordance_id":"YES"})")));
        // As `session::delivered` does when execution cannot start.
        test.queued.clear();
        test.authority.interrupt(error::not_ready);
        game_client::begin_input_boundary();
        CHECK_FALSE(test.pump(""));
        const auto response = answer(test.output.str(), 7);
        CHECK(is_tool_error(response));
        CHECK(response.at("result").at("structuredContent").contains("screen_id"));
    }
}

TEST_CASE("bn.state and bn://state return the published world", "[mcp]") {
    auto test = fixture{};
    test.run(
        handshake + call("2", "bn.state") + call("3", "bn.state", R"({"part":0})")
        + call("4", "bn.state", R"({"part":1})")
        + rpc("resources/read", R"({"uri":"bn://state"})", "5")
        + rpc("resources/read", R"({"uri":"bn://nothing"})", "6"));
    const auto out = test.output.str();
    const auto header = answer(out, 2).at("result").at("structuredContent");
    CHECK(header.at("parts") == 1);
    CHECK(header.at("avatar").at("name") == "Ada");
    CHECK(answer(out, 3).at("result").at("structuredContent").at("cells").size() == 1);
    CHECK(is_tool_error(answer(out, 4)));
    const auto content = answer(out, 5).at("result").at("contents").at(0);
    CHECK(nlohmann::json::parse(content.at("text").get<std::string>()) == header);
    CHECK(answer(out, 6).at("error").at("code") == -32602);
}

TEST_CASE("resources/read accepts _meta", "[mcp]") {
    auto test = fixture{};
    test.run(handshake + rpc("resources/read", R"({"uri":"bn://screen","_meta":{}})", "2"));
    CHECK(answer(test.output.str(), 2).contains("result"));
}

TEST_CASE("a batch result that overflows the frame fails only its own member", "[mcp]") {
    auto test = fixture{};
    test.authority.set_world_capture(&capture_large_world);
    test.run(
        handshake
        + batch({call("2", "bn.state"), call("3", "bn.state"), call("4", "bn.state"),
                 rpc("resources/read", R"({"uri":"bn://state"})", "5"), rpc("ping", "{}", "6")}));
    const auto out = test.output.str();
    CHECK_FALSE(is_tool_error(answer(out, 2)));
    CHECK_FALSE(is_tool_error(answer(out, 3)));
    CHECK(is_tool_error(answer(out, 4)));
    CHECK(answer(out, 5).at("error").at("code") == -32603);
    CHECK(answer(out, 6).at("result") == nlohmann::json::object());
    CHECK(test.errors.str().empty());
}

#endif
