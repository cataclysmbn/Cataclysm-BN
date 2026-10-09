#if defined(CATA_MCP)

#    include "catch/catch.hpp"
#    include "json.h"
#    include "mcp_server.h"
#    include "mcp_session.h"

#    include <functional>
#    include <sstream>
#    include <string>
#    include <vector>

#    if !defined(_WIN32)
#        include <csignal>
#        include <unistd.h>
#    endif

namespace {

struct protocol_fixture {
    std::vector<bn::mcp::key_event> submitted;
    bool input_available = false;
    int screen_number = 0;

    auto make_server(
        const bool include_state = true,
        const std::size_t max_frame_bytes = bn::mcp::server::default_max_frame_bytes)
        -> bn::mcp::server {
        auto state = include_state ? std::function<auto()->bn::mcp::screen_snapshot>{[] {
            return bn::mcp::screen_snapshot{.json = "{\"avatar\":{\"visible\":true}}"};
        }}
                                   : std::function<auto()->bn::mcp::screen_snapshot>{};
        return bn::mcp::server(
            bn::mcp::mcp_host{
                .observe =
                    [this] {
                        return bn::mcp::screen_snapshot{
                            .json = "{\"screen\":" + std::to_string(screen_number) + "}"};
                    },
                .state = std::move(state),
                .submit =
                    [this](const auto& events) {
                        if (events.empty() || events.size() > 256) { return false; }
                        submitted.insert(submitted.end(), events.begin(), events.end());
                        input_available = true;
                        return true;
                    },
                .has_input = [this] { return input_available; },
                .actions = [] { return std::string{"{\"actions\":[]}"}; },
                .interaction =
                    [this](const std::size_t offset, const std::size_t limit) {
                        return bn::mcp::screen_snapshot{
                            .json = "{\"input_id\":" + std::to_string(screen_number)
                                  + ",\"offset\":" + std::to_string(offset)
                                  + ",\"limit\":" + std::to_string(limit) + "}"};
                    }},

            {.max_frame_bytes = max_frame_bytes});
    }
};

auto request(const std::string& method, const std::string& id, const std::string& params = "{}")
    -> std::string {
    return "{\"jsonrpc\":\"2.0\",\"id\":" + id + ",\"method\":\"" + method
         + "\",\"params\":" + params + "}\n";
}

auto has_error_response(const std::string& output, const int request_id) -> bool {
    auto responses = std::istringstream{output};
    auto line = std::string{};
    while (std::getline(responses, line)) {
        if (line.empty()) { continue; }
        auto input = std::istringstream{line};
        auto json_in = JsonIn{input};
        auto response = json_in.get_object();
        response.allow_omitted_members();
        if (!response.has_member("error") || !response.has_member("id")) { continue; }
        const auto id = response.get_member("id");
        auto error = response.get_object("error");
        error.allow_omitted_members();
        if (response.get_raw("id")->test_number() && id.get_int() == request_id
            && error.get_int("code") == -32602) {
            return true;
        }
    }
    return false;
}

} // namespace

TEST_CASE("MCP initializes and discovers BN tools", "[mcp][protocol]") {
    auto fixture = protocol_fixture{};
    auto server = fixture.make_server();
    auto input = std::stringstream{};
    input << request(
        "initialize", "1",
        "{\"protocolVersion\":\"2025-11-25\",\"capabilities\":{},"
        "\"clientInfo\":{\"name\":\"test\",\"version\":\"1\"}}");
    input << "{\"jsonrpc\":\"2.0\",\"method\":\"notifications/initialized\"}\n";
    input << request("tools/list", "2");
    auto output = std::ostringstream{};
    auto errors = std::ostringstream{};

    CHECK(server.run(input, output, errors) == 0);
    CHECK(output.str().find("cataclysm-bright-nights") != std::string::npos);
    CHECK(output.str().find("bn.observe") != std::string::npos);
    CHECK(output.str().find("bn.press") != std::string::npos);
    CHECK(output.str().find("bn.interaction") != std::string::npos);
    CHECK(output.str().find("bn.interact") != std::string::npos);

    auto responses = std::istringstream{output.str()};
    auto response_line = std::string{};
    REQUIRE(std::getline(responses, response_line));
    REQUIRE(std::getline(responses, response_line));
    auto response_input = std::istringstream{response_line};
    auto response_json = JsonIn{response_input};
    auto response = response_json.get_object();
    response.allow_omitted_members();
    auto result = response.get_object("result");
    result.allow_omitted_members();
    auto tools = result.get_array("tools");
    auto found_press = false;
    for (const auto value : tools) {
        const auto tool = value.get_object();
        tool.allow_omitted_members();
        if (tool.get_string("name") != "bn.press") { continue; }
        auto schema = tool.get_object("inputSchema");
        schema.allow_omitted_members();
        const auto required_arguments = std::vector<std::string>{"keys"};
        CHECK(schema.get_string_array("required") == required_arguments);
        auto properties = schema.get_object("properties");
        properties.allow_omitted_members();
        auto keys = properties.get_object("keys");
        keys.allow_omitted_members();
        auto items = keys.get_object("items");
        items.allow_omitted_members();
        auto item_properties = items.get_object("properties");
        item_properties.allow_omitted_members();
        auto mouse = item_properties.get_object("mouse");
        mouse.allow_omitted_members();
        auto mouse_properties = mouse.get_object("properties");
        mouse_properties.allow_omitted_members();
        CHECK(mouse_properties.has_member("x"));
        CHECK(mouse_properties.has_member("y"));
        CHECK(mouse_properties.has_member("button"));
        const auto required_coordinates = std::vector<std::string>{"x", "y", "button"};
        CHECK(mouse.get_string_array("required") == required_coordinates);
        found_press = true;
    }
    CHECK(found_press);
}

TEST_CASE("MCP exposes structured state as a tool and resource", "[mcp][protocol]") {
    auto fixture = protocol_fixture{};
    auto server = fixture.make_server();
    auto input = std::stringstream{};
    input << request(
        "initialize", "1",
        "{\"protocolVersion\":\"2025-11-25\",\"capabilities\":{},"
        "\"clientInfo\":{\"name\":\"test\",\"version\":\"1\"}}");
    input << "{\"jsonrpc\":\"2.0\",\"method\":\"notifications/initialized\"}\n";
    input << request("tools/list", "2");
    input << request("tools/call", "3", "{\"name\":\"bn.state\",\"arguments\":{}}");
    input << request("resources/read", "4", "{\"uri\":\"bn://state\"}");
    auto output = std::ostringstream{};
    auto errors = std::ostringstream{};

    CHECK(server.run(input, output, errors) == 0);
    CHECK(output.str().find("bn.state") != std::string::npos);
    CHECK(output.str().find("visible") != std::string::npos);
    CHECK(output.str().find("bn://state") != std::string::npos);
}

TEST_CASE("MCP reports unavailable structured state explicitly", "[mcp][protocol]") {
    auto fixture = protocol_fixture{};
    auto server = fixture.make_server(false);
    auto input = std::stringstream{};
    input << request(
        "initialize", "1",
        "{\"protocolVersion\":\"2025-11-25\",\"capabilities\":{},"
        "\"clientInfo\":{\"name\":\"test\",\"version\":\"1\"}}");
    input << "{\"jsonrpc\":\"2.0\",\"method\":\"notifications/initialized\"}\n";
    input << request("resources/read", "2", "{\"uri\":\"bn://state\"}");
    auto output = std::ostringstream{};
    auto errors = std::ostringstream{};

    CHECK(server.run(input, output, errors) == 0);
    CHECK(output.str().find("Structured state is unavailable") != std::string::npos);
    CHECK(output.str().find("avatar") == std::string::npos);
}

TEST_CASE("MCP rejects malformed and unknown requests", "[mcp][protocol]") {
    auto fixture = protocol_fixture{};
    auto server = fixture.make_server();
    auto input = std::stringstream{};
    input << "not-json\n";
    input << request(
        "initialize", "1",
        "{\"protocolVersion\":\"2025-11-25\",\"capabilities\":{},"
        "\"clientInfo\":{\"name\":\"test\",\"version\":\"1\"}}");
    input << "{\"jsonrpc\":\"2.0\",\"method\":\"notifications/initialized\"}\n";
    input << request("unknown/method", "2");
    input << request("ping", "true");
    input << request("ping", "1.5");
    auto output = std::ostringstream{};
    auto errors = std::ostringstream{};

    CHECK(server.run(input, output, errors) == 0);
    CHECK(output.str().find("-32700") != std::string::npos);
    CHECK(output.str().find("-32601") != std::string::npos);
}

TEST_CASE("MCP notifications do not produce responses", "[mcp][protocol]") {
    auto fixture = protocol_fixture{};
    auto server = fixture.make_server();
    auto input = std::stringstream{
        "{\"jsonrpc\":\"2.0\",\"method\":\"notifications/initialized\"}\n"};
    auto output = std::ostringstream{};
    auto errors = std::ostringstream{};

    CHECK(server.run(input, output, errors) == 0);
    CHECK(output.str().empty());
}

TEST_CASE("MCP accepts exact-limit frames and following frames", "[mcp][protocol]") {
    const auto first = request("ping", "1");
    const auto second = request("ping", "2");
    REQUIRE(first.size() == second.size());
    auto fixture = protocol_fixture{};
    auto server = fixture.make_server(true, first.size() - 1);
    auto input = std::stringstream{first + second};
    auto output = std::ostringstream{};
    auto errors = std::ostringstream{};

    CHECK(server.run(input, output, errors) == 0);
    CHECK_FALSE(server.failed());
    CHECK(output.str().find("\"id\":1") != std::string::npos);
    CHECK(output.str().find("\"id\":2") != std::string::npos);
    CHECK(errors.str().empty());
}

TEST_CASE("MCP rejects oversized frames without executing prefixes", "[mcp][protocol]") {
    constexpr auto frame_limit = std::size_t{256};
    auto oversized = request(
        "tools/call", "2", "{\"name\":\"bn.press\",\"arguments\":{\"keys\":[{\"key\":\"a\"}]}}");
    oversized.pop_back();
    REQUIRE(oversized.size() <= frame_limit);
    oversized.resize(frame_limit + 1, ' ');
    oversized += '\n';

    auto fixture = protocol_fixture{};
    auto server = fixture.make_server(true, frame_limit);
    auto input = std::stringstream{};
    input << request("initialize", "1", "{\"protocolVersion\":\"2025-11-25\"}");
    input << "{\"jsonrpc\":\"2.0\",\"method\":\"notifications/initialized\"}\n";
    input << oversized;
    input << request("ping", "3");
    auto output = std::ostringstream{};
    auto errors = std::ostringstream{};

    CHECK(server.run(input, output, errors) == 1);
    CHECK(server.failed());
    CHECK(fixture.submitted.empty());
    CHECK(output.str().find("\"id\":3") == std::string::npos);
    CHECK(errors.str().find("exceeds 256 bytes") != std::string::npos);
}

TEST_CASE("MCP bounds a large stream without a newline", "[mcp][protocol]") {
    constexpr auto frame_limit = std::size_t{32};
    auto fixture = protocol_fixture{};
    auto server = fixture.make_server(true, frame_limit);
    auto input = std::stringstream{std::string(1024 * 1024, 'x')};
    auto output = std::ostringstream{};
    auto errors = std::ostringstream{};

    CHECK(server.run(input, output, errors) == 1);
    CHECK(server.failed());
    CHECK(input.tellg() == static_cast<std::streampos>(frame_limit + 1));
    CHECK(output.str().empty());
    CHECK(errors.str().find("exceeds 32 bytes") != std::string::npos);
}

TEST_CASE("MCP rejects a partial frame at EOF without submitting input", "[mcp][session]") {
    auto fixture = protocol_fixture{};
    auto server = fixture.make_server(true, 256);
    auto input = std::stringstream{};
    input << request("initialize", "1", "{\"protocolVersion\":\"2025-11-25\"}");
    input << "{\"jsonrpc\":\"2.0\",\"method\":\"notifications/initialized\"}\n";
    auto partial = request(
        "tools/call", "2", "{\"name\":\"bn.press\",\"arguments\":{\"keys\":[{\"key\":\"a\"}]}}");
    partial.pop_back();
    input << partial;
    auto output = std::ostringstream{};
    auto errors = std::ostringstream{};

    CHECK_FALSE(server.pump_until_input(input, output, errors));
    CHECK(server.failed());
    CHECK(fixture.submitted.empty());
    CHECK(output.str().find("\"id\":2") == std::string::npos);
    CHECK(errors.str().find("unterminated stdio frame") != std::string::npos);
}

TEST_CASE("MCP preserves string and numeric request ids", "[mcp][protocol]") {
    auto fixture = protocol_fixture{};
    auto server = fixture.make_server();
    auto input = std::stringstream{};
    input << request(
        "initialize", "\"string-id\"",
        "{\"protocolVersion\":\"2025-11-25\",\"capabilities\":{},"
        "\"clientInfo\":{\"name\":\"test\",\"version\":\"1\"}}");
    input << request("ping", "42");
    auto output = std::ostringstream{};
    auto errors = std::ostringstream{};

    CHECK(server.run(input, output, errors) == 0);
    CHECK(output.str().find("\"id\":\"string-id\"") != std::string::npos);
    CHECK(output.str().find("\"id\":42") != std::string::npos);
}

TEST_CASE("MCP rejects non-integer request ids without suppressing valid ids", "[mcp][protocol]") {
    auto fixture = protocol_fixture{};
    auto server = fixture.make_server();
    auto input = std::stringstream{};
    input << request(
        "initialize", "1",
        "{\"protocolVersion\":\"2025-11-25\",\"capabilities\":{},"
        "\"clientInfo\":{\"name\":\"test\",\"version\":\"1\"}}");
    input << "{\"jsonrpc\":\"2.0\",\"method\":\"notifications/initialized\"}\n";
    input << request("ping", "true");
    input << request("ping", "1.5");
    input << request("ping", "1.00000000000000001");
    input << request("ping", "42");
    input << request("ping", "9007199254740993");
    input << request("ping", "\"string-id\"");
    input << request("ping", "null");
    auto output = std::ostringstream{};
    auto errors = std::ostringstream{};

    CHECK(server.run(input, output, errors) == 0);
    auto error_count = 0;
    auto result_count = 0;
    auto found_numeric_id = false;
    auto found_large_id = false;
    auto found_string_id = false;
    auto found_null_id = false;
    auto response_line = std::string{};
    auto responses = std::istringstream{output.str()};
    while (std::getline(responses, response_line)) {
        if (response_line.empty()) { continue; }
        auto response_input = std::istringstream{response_line};
        auto response_json = JsonIn{response_input};
        auto response = response_json.get_object();
        response.allow_omitted_members();
        if (response.has_member("error")) {
            const auto error = response.get_object("error");
            error.allow_omitted_members();
            CHECK(error.get_int("code") == -32600);
            CHECK(response.get_raw("id")->test_null());
            ++error_count;
            continue;
        }
        if (!response.has_member("result")) { continue; }
        ++result_count;
        const auto id = response.get_member("id");
        if (response.get_raw("id")->test_number()) {
            const auto number = id.get_int64();
            found_numeric_id = found_numeric_id || number == 42;
            found_large_id = found_large_id || number == 9007199254740993LL;
        } else if (id.test_string() && id.get_string() == "string-id") {
            found_string_id = true;
        } else if (response.get_raw("id")->test_null()) {
            found_null_id = true;
        }
    }
    CHECK(error_count == 3);
    CHECK(result_count == 5);
    CHECK(found_numeric_id);
    CHECK(found_large_id);
    CHECK(found_string_id);
    CHECK(found_null_id);
    CHECK(output.str().find("\"id\":true") == std::string::npos);
    CHECK(output.str().find("\"id\":1.5") == std::string::npos);
}

TEST_CASE("MCP defers key response until the next input boundary", "[mcp][session]") {
    auto fixture = protocol_fixture{};
    auto server = fixture.make_server();
    auto input = std::stringstream{};
    input << request(
        "initialize", "1",
        "{\"protocolVersion\":\"2025-11-25\",\"capabilities\":{},"
        "\"clientInfo\":{\"name\":\"test\",\"version\":\"1\"}}");
    input << "{\"jsonrpc\":\"2.0\",\"method\":\"notifications/initialized\"}\n";
    input << request(
        "tools/call", "2", "{\"name\":\"bn.press\",\"arguments\":{\"keys\":[{\"key\":\"a\"}]}}");
    auto output = std::ostringstream{};
    auto errors = std::ostringstream{};

    CHECK(server.pump_until_input(input, output, errors));
    REQUIRE(fixture.submitted.size() == 1);
    CHECK(output.str().find("\"id\":2") == std::string::npos);
    fixture.screen_number = 7;
    server.finish_pending(output, errors);
    CHECK(output.str().find("\"id\":2") != std::string::npos);
    CHECK(output.str().find("\"screen\":7") != std::string::npos);
}

TEST_CASE("MCP semantic interaction returns the next descriptor", "[mcp][session]") {
    auto fixture = protocol_fixture{};
    auto server = fixture.make_server();
    auto input = std::stringstream{};
    input << request("initialize", "1", "{\"protocolVersion\":\"2025-11-25\"}");
    input << "{\"jsonrpc\":\"2.0\",\"method\":\"notifications/initialized\"}\n";
    input << request(
        "tools/call", "2",
        R"({"name":"bn.interact","arguments":{"input_id":47,"operation":"choose","choice_id":"opaque-choice"}})");
    auto output = std::ostringstream{};
    auto errors = std::ostringstream{};

    REQUIRE(server.pump_until_input(input, output, errors));
    REQUIRE(fixture.submitted.size() == 1);
    REQUIRE(fixture.submitted.front().interaction);
    CHECK(fixture.submitted.front().interaction->input_id == 47);
    CHECK(fixture.submitted.front().interaction->target_id == "opaque-choice");
    CHECK(output.str().find("\"id\":2") == std::string::npos);
    fixture.input_available = false;
    fixture.screen_number = 48;
    server.finish_pending(output, errors);
    CHECK(output.str().find("\"id\":2") != std::string::npos);
    CHECK(output.str().find("\\\"input_id\\\":48") != std::string::npos);
}

TEST_CASE("MCP parses count and target semantic interaction payloads", "[mcp][protocol]") {
    SECTION("count") {
        auto fixture = protocol_fixture{};
        auto server = fixture.make_server();
        auto input = std::stringstream{};
        input << request("initialize", "1", "{\"protocolVersion\":\"2025-11-25\"}");
        input << "{\"jsonrpc\":\"2.0\",\"method\":\"notifications/initialized\"}\n";
        input << request(
            "tools/call", "2",
            R"({"name":"bn.interact","arguments":{"input_id":47,"operation":"set_count","choice_id":"opaque-item","count":0}})");
        auto output = std::ostringstream{};
        auto errors = std::ostringstream{};
        REQUIRE(server.pump_until_input(input, output, errors));
        REQUIRE(fixture.submitted.size() == 1);
        REQUIRE(fixture.submitted.front().interaction);
        CHECK(fixture.submitted.front().interaction->count == 0);
        CHECK(fixture.submitted.front().interaction->target_id == "opaque-item");
    }
    SECTION("target") {
        auto fixture = protocol_fixture{};
        auto server = fixture.make_server();
        auto input = std::stringstream{};
        input << request("initialize", "1", "{\"protocolVersion\":\"2025-11-25\"}");
        input << "{\"jsonrpc\":\"2.0\",\"method\":\"notifications/initialized\"}\n";
        input << request(
            "tools/call", "2",
            R"({"name":"bn.interact","arguments":{"input_id":47,"operation":"set_target","candidate_id":"opaque-target","position":{"x":12,"y":-3,"z":-1}}})");
        auto output = std::ostringstream{};
        auto errors = std::ostringstream{};
        REQUIRE(server.pump_until_input(input, output, errors));
        REQUIRE(fixture.submitted.size() == 1);
        REQUIRE(fixture.submitted.front().interaction);
        CHECK(fixture.submitted.front().interaction->target_id == "opaque-target");
        const auto expected = game_client::interaction_position{.x = 12, .y = -3, .z = -1};
        CHECK(fixture.submitted.front().interaction->position == expected);
    }
}

TEST_CASE("MCP rejects malformed semantic interaction payloads", "[mcp][protocol]") {
    auto fixture = protocol_fixture{};
    auto server = fixture.make_server();
    auto input = std::stringstream{};
    input << request("initialize", "1", "{\"protocolVersion\":\"2025-11-25\"}");
    input << "{\"jsonrpc\":\"2.0\",\"method\":\"notifications/initialized\"}\n";
    input << request(
        "tools/call", "2",
        R"({"name":"bn.interact","arguments":{"input_id":47,"operation":"fill","field_id":"field:value","value":"missing submit"}})");
    input << request(
        "tools/call", "3",
        R"({"name":"bn.interact","arguments":{"input_id":47.5,"operation":"cancel"}})");
    input << request(
        "tools/call", "4",
        R"({"name":"bn.interact","arguments":{"input_id":47,"operation":"cancel","choice_id":"extra"}})");
    input << request(
        "tools/call", "5",
        R"({"name":"bn.interact","arguments":{"input_id":47,"operation":"set_count","choice_id":"item","count":1.5}})");
    input << request(
        "tools/call", "6",
        R"({"name":"bn.interact","arguments":{"input_id":47,"operation":"set_count","choice_id":"item","count":-1}})");
    input << request(
        "tools/call", "7",
        R"({"name":"bn.interact","arguments":{"input_id":47,"operation":"set_target","position":{"x":1,"y":2.5,"z":0}}})");
    input << request(
        "tools/call", "8",
        R"({"name":"bn.interact","arguments":{"input_id":47,"operation":"set_target","position":{"x":1,"y":2}}})");
    auto output = std::ostringstream{};
    auto errors = std::ostringstream{};

    CHECK(server.run(input, output, errors) == 0);
    CHECK(has_error_response(output.str(), 2));
    CHECK(has_error_response(output.str(), 3));
    CHECK(has_error_response(output.str(), 4));
    CHECK(has_error_response(output.str(), 5));
    CHECK(has_error_response(output.str(), 6));
    CHECK(has_error_response(output.str(), 7));
    CHECK(has_error_response(output.str(), 8));
    CHECK(fixture.submitted.empty());
}

TEST_CASE("MCP keeps a pending response while input remains available", "[mcp][session]") {
    auto fixture = protocol_fixture{};
    auto server = fixture.make_server();
    auto input = std::stringstream{};
    input << request(
        "initialize", "1",
        "{\"protocolVersion\":\"2025-11-25\",\"capabilities\":{},"
        "\"clientInfo\":{\"name\":\"test\",\"version\":\"1\"}}");
    input << "{\"jsonrpc\":\"2.0\",\"method\":\"notifications/initialized\"}\n";
    input << request(
        "tools/call", "2", "{\"name\":\"bn.press\",\"arguments\":{\"keys\":[{\"key\":\"a\"}]}}");
    auto output = std::ostringstream{};
    auto errors = std::ostringstream{};

    REQUIRE(server.pump_until_input(input, output, errors));
    REQUIRE(fixture.submitted.size() == 1);
    const auto before_repeated_pump = output.str();
    CHECK(server.pump_until_input(input, output, errors));
    CHECK(fixture.submitted.size() == 1);
    CHECK(output.str() == before_repeated_pump);

    fixture.input_available = false;
    fixture.screen_number = 9;
    CHECK_FALSE(server.pump_until_input(input, output, errors));
    CHECK(output.str().find("\"id\":2") != std::string::npos);
    CHECK(output.str().find("\"screen\":9") != std::string::npos);
}

TEST_CASE("MCP notifications with invalid payloads remain silent", "[mcp][protocol]") {
    auto fixture = protocol_fixture{};
    auto server = fixture.make_server();
    auto input = std::stringstream{};
    input << request(
        "initialize", "1",
        "{\"protocolVersion\":\"2025-11-25\",\"capabilities\":{},"
        "\"clientInfo\":{\"name\":\"test\",\"version\":\"1\"}}");
    input << "{\"jsonrpc\":\"2.0\",\"method\":\"notifications/initialized\"}\n";
    input << "{\"jsonrpc\":\"2.0\",\"method\":\"tools/call\",\"params\":{"
             "\"name\":\"bn.press\",\"arguments\":{}}}\n";
    input << "{\"jsonrpc\":\"2.0\",\"method\":\"unknown/method\",\"params\":null}\n";
    auto output = std::ostringstream{};
    auto errors = std::ostringstream{};

    CHECK(server.run(input, output, errors) == 0);
    CHECK(output.str().find("\"id\":1") != std::string::npos);
    CHECK(output.str().find("error") == std::string::npos);
    CHECK(fixture.submitted.empty());
}

TEST_CASE("MCP accepts raw UTF-8 text-only input", "[mcp][session]") {
    auto fixture = protocol_fixture{};
    auto server = fixture.make_server();
    auto input = std::stringstream{};
    input << request(
        "initialize", "1",
        "{\"protocolVersion\":\"2025-11-25\",\"capabilities\":{},"
        "\"clientInfo\":{\"name\":\"test\",\"version\":\"1\"}}");
    input << "{\"jsonrpc\":\"2.0\",\"method\":\"notifications/initialized\"}\n";
    input << request(
        "tools/call", "2", "{\"name\":\"bn.press\",\"arguments\":{\"keys\":[{\"text\":\"한\"}]}}");
    auto output = std::ostringstream{};
    auto errors = std::ostringstream{};

    CHECK(server.pump_until_input(input, output, errors));
    REQUIRE(fixture.submitted.size() == 1);
    CHECK(fixture.submitted.front().text == "한");
    CHECK(fixture.submitted.front().key.empty());
}

TEST_CASE("MCP rejects modifiers without a key event", "[mcp][protocol]") {
    auto fixture = protocol_fixture{};
    auto server = fixture.make_server();
    auto input = std::stringstream{};
    input << request(
        "initialize", "1",
        "{\"protocolVersion\":\"2025-11-25\",\"capabilities\":{},"
        "\"clientInfo\":{\"name\":\"test\",\"version\":\"1\"}}");
    input << "{\"jsonrpc\":\"2.0\",\"method\":\"notifications/initialized\"}\n";
    input << request(
        "tools/call", "2",
        "{\"name\":\"bn.press\",\"arguments\":{\"keys\":[{\"text\":\"한\","
        "\"modifiers\":[\"SHIFT\"]}]}}");
    auto output = std::ostringstream{};
    auto errors = std::ostringstream{};

    CHECK_FALSE(server.pump_until_input(input, output, errors));
    CHECK(has_error_response(output.str(), 2));
    CHECK(fixture.submitted.empty());
}

TEST_CASE("MCP forwards mouse coordinates through input events", "[mcp][session]") {
    auto fixture = protocol_fixture{};
    auto server = fixture.make_server();
    auto input = std::stringstream{};
    input << request(
        "initialize", "1",
        "{\"protocolVersion\":\"2025-11-25\",\"capabilities\":{},"
        "\"clientInfo\":{\"name\":\"test\",\"version\":\"1\"}}");
    input << "{\"jsonrpc\":\"2.0\",\"method\":\"notifications/initialized\"}\n";
    input << request(
        "tools/call", "2",
        "{\"name\":\"bn.press\",\"arguments\":{\"keys\":[{\"mouse\":{"
        "\"x\":17,\"y\":23,\"button\":\"left\"}}]}}");
    auto output = std::ostringstream{};
    auto errors = std::ostringstream{};

    CHECK(server.pump_until_input(input, output, errors));
    REQUIRE(fixture.submitted.size() == 1);
    REQUIRE(fixture.submitted.front().mouse_position.has_value());
    CHECK(fixture.submitted.front().mouse_position->x == 17);
    CHECK(fixture.submitted.front().mouse_position->y == 23);
    CHECK(fixture.submitted.front().mouse_button == "left");
}

TEST_CASE("MCP rejects invalid mouse events and mixed input modes", "[mcp][protocol]") {
    auto fixture = protocol_fixture{};
    auto server = fixture.make_server();
    auto input = std::stringstream{};
    input << request(
        "initialize", "1",
        "{\"protocolVersion\":\"2025-11-25\",\"capabilities\":{},"
        "\"clientInfo\":{\"name\":\"test\",\"version\":\"1\"}}");
    input << "{\"jsonrpc\":\"2.0\",\"method\":\"notifications/initialized\"}\n";
    input << request(
        "tools/call", "2",
        "{\"name\":\"bn.press\",\"arguments\":{\"keys\":[{\"mouse\":{"
        "\"x\":-1,\"y\":0,\"button\":\"left\"}}]}}");
    input << request(
        "tools/call", "3",
        "{\"name\":\"bn.press\",\"arguments\":{\"keys\":[{\"mouse\":{"
        "\"x\":0,\"y\":0,\"button\":\"middle\"}}]}}");
    input << request(
        "tools/call", "4",
        "{\"name\":\"bn.press\",\"arguments\":{\"keys\":[{\"key\":\"a\","
        "\"mouse\":{\"x\":0,\"y\":0,\"button\":\"left\"}}]}}");
    input << request(
        "tools/call", "5",
        "{\"name\":\"bn.press\",\"arguments\":{\"keys\":[{\"action\":\"CONFIRM\","
        "\"text\":\"ignored\"}]}}");
    auto output = std::ostringstream{};
    auto errors = std::ostringstream{};

    CHECK(server.run(input, output, errors) == 0);
    CHECK(has_error_response(output.str(), 2));
    CHECK(has_error_response(output.str(), 3));
    CHECK(has_error_response(output.str(), 4));
    CHECK(has_error_response(output.str(), 5));
    CHECK(fixture.submitted.empty());
}

TEST_CASE("MCP rejects fractional and exponent mouse coordinates", "[mcp][protocol]") {
    auto fixture = protocol_fixture{};
    auto server = fixture.make_server();
    auto input = std::stringstream{};
    input << request(
        "initialize", "1",
        "{\"protocolVersion\":\"2025-11-25\",\"capabilities\":{},"
        "\"clientInfo\":{\"name\":\"test\",\"version\":\"1\"}}");
    input << "{\"jsonrpc\":\"2.0\",\"method\":\"notifications/initialized\"}\n";
    const auto mouse = std::vector<std::string>{
        "\"x\":17.9,\"y\":23", "\"x\":-0.5,\"y\":0", "\"x\":1,\"y\":2.0", "\"x\":1e1,\"y\":2"};
    for (auto i = std::size_t{0}; i < mouse.size(); ++i) {
        input << request(
            "tools/call", std::to_string(i + 2),
            "{\"name\":\"bn.press\",\"arguments\":{\"keys\":[{\"mouse\":{" + mouse[i]
                + ",\"button\":\"left\"}}]}}");
    }
    auto output = std::ostringstream{};
    auto errors = std::ostringstream{};

    CHECK(server.run(input, output, errors) == 0);
    for (auto i = std::size_t{0}; i < mouse.size(); ++i) {
        CHECK(has_error_response(output.str(), static_cast<int>(i + 2)));
    }
    CHECK(fixture.submitted.empty());
}

#    if !defined(_WIN32)
TEST_CASE(
    "MCP session ignores SIGPIPE so a closed output pipe becomes a write error", "[mcp][session]") {
    const auto stdout_copy = dup(STDOUT_FILENO);
    REQUIRE(stdout_copy >= 0);
    const auto previous = std::signal(SIGPIPE, SIG_DFL);
    REQUIRE(previous != SIG_ERR);
    bn::mcp::start_session();
    const auto installed = std::signal(SIGPIPE, SIG_DFL);
    bn::mcp::finish_session();
    REQUIRE(dup2(stdout_copy, STDOUT_FILENO) >= 0);
    close(stdout_copy);
    std::signal(SIGPIPE, previous);
    CHECK(installed == SIG_IGN);
}
#    endif

TEST_CASE("MCP rejects empty and oversized key batches", "[mcp][session]") {
    auto fixture = protocol_fixture{};
    auto server = fixture.make_server();
    auto input = std::stringstream{};
    input << request(
        "initialize", "1",
        "{\"protocolVersion\":\"2025-11-25\",\"capabilities\":{},"
        "\"clientInfo\":{\"name\":\"test\",\"version\":\"1\"}}");
    input << "{\"jsonrpc\":\"2.0\",\"method\":\"notifications/initialized\"}\n";
    input << request("tools/call", "2", "{\"name\":\"bn.press\",\"arguments\":{\"keys\":[]}}");
    auto output = std::ostringstream{};
    auto errors = std::ostringstream{};
    CHECK_FALSE(server.pump_until_input(input, output, errors));
    CHECK(output.str().find("\"isError\":true") != std::string::npos);

    auto oversized = std::string{"["};
    for (int index = 0; index < 257; ++index) {
        if (index > 0) { oversized += ","; }
        oversized += "{\"key\":\"a\"}";
    }
    oversized += "]";
    auto fixture_two = protocol_fixture{};
    auto server_two = fixture_two.make_server();
    auto input_two = std::stringstream{};
    input_two << request(
        "initialize", "1",
        "{\"protocolVersion\":\"2025-11-25\",\"capabilities\":{},"
        "\"clientInfo\":{\"name\":\"test\",\"version\":\"1\"}}");
    input_two << "{\"jsonrpc\":\"2.0\",\"method\":\"notifications/initialized\"}\n";
    input_two << request(
        "tools/call", "2", "{\"name\":\"bn.press\",\"arguments\":{\"keys\":" + oversized + "}}");
    auto output_two = std::ostringstream{};
    auto errors_two = std::ostringstream{};
    CHECK_FALSE(server_two.pump_until_input(input_two, output_two, errors_two));
    CHECK(output_two.str().find("\"isError\":true") != std::string::npos);
}

TEST_CASE("MCP preserves input boundary identity without numeric truncation", "[mcp][protocol]") {
    auto fixture = protocol_fixture{};
    auto server = fixture.make_server();
    auto input = std::stringstream{};
    input << request("initialize", "1", "{\"protocolVersion\":\"2025-11-25\"}");
    input << "{\"jsonrpc\":\"2.0\",\"method\":\"notifications/initialized\"}\n";
    input << request("tools/call", "2",
                     R"({"name":"bn.press","arguments":{"keys":[{"key":"ENTER","input_id":47}]}})");
    input << request(
        "tools/call", "3",
        R"({"name":"bn.press","arguments":{"keys":[{"key":"ENTER","input_id":47.5}]}})");
    input << request("tools/call", "4",
                     R"({"name":"bn.press","arguments":{"keys":[{"key":"ENTER","input_id":-1}]}})");
    auto output = std::ostringstream{};
    auto errors = std::ostringstream{};
    CHECK(server.run(input, output, errors) == 0);
    REQUIRE(fixture.submitted.size() == 1);
    REQUIRE(fixture.submitted.front().input_id);
    CHECK(*fixture.submitted.front().input_id == 47);
    CHECK(has_error_response(output.str(), 3));
    CHECK(has_error_response(output.str(), 4));
}


TEST_CASE("MCP validates all frame bytes before host callbacks", "[mcp][framing_regression]") {
    auto fixture = protocol_fixture{};
    auto server = fixture.make_server();
    auto text =
        request("initialize", "1")
        + "{\"jsonrpc\":\"2.0\",\"method\":\"notifications/initialized\"}\n";
    auto command = request(
        "tools/call", "2", "{\"name\":\"bn.press\",\"arguments\":{\"keys\":[{\"key\":\"x\"}]}}");
    command.pop_back();
    text += command + " false\n";
    auto input = std::istringstream(text);
    auto output = std::ostringstream{};
    auto errors = std::ostringstream{};
    CHECK(server.run(input, output, errors) == 0);
    CHECK(fixture.submitted.empty());
    CHECK(output.str().find("-32700") != std::string::npos);
}

TEST_CASE("MCP response admission publishes no oversized prefix", "[mcp][framing_regression]") {
    auto server = bn::mcp::server({
        .observe =
            [] {
                return bn::mcp::screen_snapshot{
                    .json = "{\"value\":\""
                          + std::string(bn::mcp::server::default_max_frame_bytes, 'x') + "\"}"};
            },
        .submit = [](const auto& /*events*/) { return true; },
    });
    auto input = std::istringstream(
        request("initialize", "1")
        + "{\"jsonrpc\":\"2.0\",\"method\":\"notifications/initialized\"}\n"
        + request("tools/call", "2", "{\"name\":\"bn.observe\",\"arguments\":{}}"));
    auto output = std::ostringstream{};
    auto errors = std::ostringstream{};
    CHECK(server.run(input, output, errors) == 1);
    CHECK(server.failed());
    CHECK(output.str().find("xxxxx") == std::string::npos);
    CHECK(output.str().find("\"id\": 2") == std::string::npos);
}

TEST_CASE("MCP configurable limit cannot exceed absolute cap", "[mcp][framing_regression]") {
    auto fixture = protocol_fixture{};
    auto server = fixture.make_server(true, bn::mcp::server::default_max_frame_bytes * 2);
    auto input = std::istringstream(
        std::string(bn::mcp::server::default_max_frame_bytes + 1, ' ') + "\n");
    auto output = std::ostringstream{};
    auto errors = std::ostringstream{};
    CHECK(server.run(input, output, errors) == 1);
    CHECK(server.failed());
    CHECK(output.str().empty());
}

namespace {
class short_mcp_output: public std::streambuf {
protected:
    auto xsputn(const char* /*bytes*/, std::streamsize count) -> std::streamsize override {
        return count > 0 ? count - 1 : 0;
    }
    auto overflow(int_type /*byte*/) -> int_type override { return traits_type::eof(); }
};
} // namespace

TEST_CASE("MCP write loss terminates before next input", "[mcp][framing_regression]") {
    auto fixture = protocol_fixture{};
    auto server = fixture.make_server();
    auto input = std::istringstream(request("ping", "1") + request("initialize", "2"));
    auto buffer = short_mcp_output{};
    auto output = std::ostream(&buffer);
    auto errors = std::ostringstream{};
    CHECK(server.run(input, output, errors) == 1);
    CHECK(server.failed());
    CHECK(input.peek() == '{');
}

#endif // CATA_MCP
