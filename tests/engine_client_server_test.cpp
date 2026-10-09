#if defined(CATA_MCP)

#    include "catch/catch.hpp"
#    include "client_input.h"
#    include "engine_client_session.h"
#    include "input.h"
#    include "json.h"
#    include "mcp_server.h"

#    include <algorithm>
#    include <sstream>
#    include <streambuf>
#    include <string>
#    include <utility>
#    include <vector>

namespace {

const auto capabilities = std::string{
    R"({"supported_versions":["1.0"],"required_capabilities":["snapshot.readiness","snapshot.actions","snapshot.interaction","command.semantic_interaction","command.registered_action","delivery.inline_completion","events.interaction_replaced"],"optional_capabilities":[]})"};

auto request(const std::string& method, const std::string& params, const std::string& id = "1")
    -> std::string {
    return "{\"jsonrpc\":\"2.0\",\"method\":\"" + method + "\",\"params\":" + params
         + (id.empty() ? "" : ",\"id\":" + id) + "}";
}

auto command(const engine_client::snapshot& snapshot) -> std::string {
    return "{\"session_epoch\":\"" + snapshot.session_epoch
         + "\",\"based_on\":{\"state_revision\":\"" + std::to_string(snapshot.state_revision)
         + "\",\"input_boundary_id\":\"" + snapshot.state.input_boundary_id
         + "\",\"interaction_schema_id\":\"" + snapshot.state.interaction->schema_id
         + "\"},\"operation\":{\"kind\":\"choose\",\"choice_id\":\"choice:yes\"}}";
}

struct fixture {
    engine_client::session authority;
    std::vector<bn::mcp::key_event> queued;
    std::size_t deliveries = 0;
    auto server() -> bn::mcp::server {
        return bn::mcp::server{{
            .observe = [] { return bn::mcp::screen_snapshot{.json = "{}"}; },
            .submit =
                [this](const auto& input) {
                    queued = input;
                    ++deliveries;
                    return true;
                },
            .has_input = [this] { return !queued.empty(); },
            .contract_session = &authority,
        }};
    }
};

struct input_boundary {
    input_context context{"YESNO"};
    game_client::input_context_scope input{context, "YESNO"};
    game_client::interaction_scope interaction{
        context, [] {
            return game_client::interaction_snapshot{
                .kind = game_client::interaction_kind::choices,
                .allow_cancel = true,
                .choices = {{.id = "choice:yes", .label = "Yes"}},
            };
        }};
    input_boundary() {
        context.register_action("YES");
        game_client::begin_input_boundary();
    }
};

/// One hundred choices make the default 100-row page exceed the inline limit when sized large.
struct wide_boundary {
    input_context context{"YESNO"};
    std::size_t description_bytes;
    std::string label;
    game_client::input_context_scope input{context, "YESNO"};
    game_client::interaction_scope interaction{
        context, [this] {
            auto value = game_client::interaction_snapshot{
                .kind = game_client::interaction_kind::choices, .allow_cancel = true};
            for (auto i = 0; i < 100; ++i) {
                value.choices.push_back(
                    {.id = "choice:" + std::to_string(i),
                     .label = label,
                     .description = std::string(description_bytes, 'x')});
            }
            return value;
        }};
    explicit wide_boundary(const std::size_t bytes, std::string text = "Choice")
        : description_bytes{bytes},
          label{std::move(text)} {
        context.register_action("YES");
        game_client::begin_input_boundary();
    }
};

auto snapshot_params(const std::string& epoch) -> std::string {
    return "{\"session_epoch\":\"" + epoch + "\",\"page\":{\"offset\":0,\"limit\":100}}";
}

auto result_params(const engine_client::result_request& value) -> std::string {
    return "{\"session_epoch\":\"" + value.session_epoch + "\",\"command_id\":\"" + value.command_id
         + "\"}";
}

struct wire_response {
    std::istringstream input;
    JsonIn reader;
    JsonObject envelope;
    explicit wire_response(const std::string& bytes)
        : input{bytes},
          reader{input},
          envelope{reader.get_object()} {
        envelope.allow_omitted_members();
    }
};

struct recovery_expectation {
    std::string id;
    std::string kind;
    std::string stage;
    std::string action;
    engine_client::snapshot current;
};

auto check_recovery(const std::string& bytes, const recovery_expectation& expected) -> void {
    auto response = wire_response{bytes};
    CHECK(response.envelope.get_string("id") == expected.id);
    CHECK_FALSE(response.envelope.has_member("result"));
    auto error = response.envelope.get_object("error");
    error.allow_omitted_members();
    CHECK(error.get_int("code") == engine_client::application_error_code);
    auto data = error.get_object("data");
    data.allow_omitted_members();
    CHECK(data.get_string("kind") == expected.kind);
    CHECK(data.get_string("stage") == expected.stage);
    CHECK(data.get_string("required_action") == expected.action);
    CHECK_FALSE(data.get_bool("retryable"));
    auto current = data.get_object("current");
    current.allow_omitted_members();
    CHECK(current.get_string("session_epoch") == expected.current.session_epoch);
    CHECK(current.get_string("state_revision") == std::to_string(expected.current.state_revision));
    CHECK(current.get_string("through_public_sequence")
          == std::to_string(expected.current.through_public_sequence));
}

class broken_output final: public std::streambuf {
    auto xsputn(const char* /*data*/, std::streamsize /*size*/) -> std::streamsize override {
        return 0;
    }
    auto overflow(int_type /*value*/) -> int_type override { return traits_type::eof(); }
};

} // namespace

TEST_CASE(
    "direct negotiation retains exact IDs and does not initialize MCP", "[engine_client_server]") {
    auto boundary = input_boundary{};
    auto fixture = ::fixture{};
    auto server = fixture.server();
    const auto id = std::string{R"("\ud83d\ude00\u0000")"};
    auto input = std::istringstream{
        request("bn.contract.negotiate", capabilities, id) + "\n"
        + request("bn.snapshot.get", "{}", "1.234567890123456789e+500") + "\n"
        + request("tools/list", "{}", "3") + "\n"};
    auto output = std::ostringstream{};
    auto errors = std::ostringstream{};
    CHECK(server.run(input, output, errors) == 0);
    CHECK(output.str().find("\"id\":" + id) != std::string::npos);
    CHECK(output.str().find("\"id\":1.234567890123456789e+500") != std::string::npos);
    CHECK(output.str().find("-32002") != std::string::npos);
    CHECK(output.str().find("contract_version") != std::string::npos);
    CHECK(fixture.queued.empty());
}

TEST_CASE(
    "direct notifications have no negotiation or authority effects", "[engine_client_server]") {
    auto boundary = input_boundary{};
    auto fixture = ::fixture{};
    REQUIRE(fixture.authority.publish_boundary());
    const auto before = fixture.authority.read_snapshot({});
    REQUIRE(before);
    auto server = fixture.server();
    auto input = std::istringstream{
        "[" + request("bn.contract.negotiate", capabilities, "") + ","
        + request("bn.command.submit", command(*before), "") + ","
        + request("bn.snapshot.get", "{}", "") + "," + request("bn.command.result", "{}", "")
        + "]\n" + request("bn.snapshot.get", "{}") + "\n"};
    auto output = std::ostringstream{};
    auto errors = std::ostringstream{};
    CHECK(server.run(input, output, errors) == 0);
    CHECK(output.str().find("negotiation_failed") != std::string::npos);
    CHECK(output.str().find("contract_version") == std::string::npos);
    const auto lines = output.str();
    CHECK(std::count(lines.begin(), lines.end(), '\n') == 1);
    CHECK(fixture.queued.empty());
    const auto after = fixture.authority.read_snapshot({});
    REQUIRE(after);
    CHECK(after->state_revision == before->state_revision);
    CHECK(after->through_public_sequence == before->through_public_sequence);
}

TEST_CASE("complete frame validation prevents malformed suffix effects", "[engine_client_server]") {
    auto boundary = input_boundary{};
    auto fixture = ::fixture{};
    auto server = fixture.server();
    auto input = std::istringstream{
        request("bn.contract.negotiate", capabilities) + " garbage\n"
        + request("bn.snapshot.get", "{}", "2") + "\n"};
    auto output = std::ostringstream{};
    auto errors = std::ostringstream{};
    CHECK(server.run(input, output, errors) == 0);
    CHECK(output.str().find("-32700") != std::string::npos);
    CHECK(output.str().find("negotiation_failed") != std::string::npos);
    CHECK(output.str().find("contract_version") == std::string::npos);
}

TEST_CASE(
    "direct receipt precedes one native delivery and next boundary completion",
    "[engine_client_server]") {
    auto boundary = input_boundary{};
    auto fixture = ::fixture{};
    REQUIRE(fixture.authority.publish_boundary());
    const auto initial = fixture.authority.read_snapshot({});
    REQUIRE(initial);
    auto server = fixture.server();
    const auto submit = request("bn.command.submit", command(*initial), "2");
    auto input = std::istringstream{
        request("bn.contract.negotiate", capabilities) + "\n[" + submit + ","
        + request("bn.command.submit", command(*initial), "3") + "]\n"};
    auto output = std::ostringstream{};
    auto errors = std::ostringstream{};
    REQUIRE(server.pump_until_input(input, output, errors));
    REQUIRE(fixture.queued.size() == 1);
    CHECK(output.str().find("\"stage\":\"received\"") != std::string::npos);
    CHECK(output.str().find("command_busy") != std::string::npos);
    CHECK(output.str().find("completed") == std::string::npos);
    auto response_input = std::istringstream{output.str().substr(output.str().find('\n') + 1)};
    auto json = JsonIn{response_input};
    auto responses = json.get_array();
    auto response = responses.get_object(0);
    response.allow_omitted_members();
    auto receipt = response.get_object("result");
    receipt.allow_omitted_members();
    const auto command_id = receipt.get_string("command_id");
    const auto result_request = engine_client::
        result_request{.session_epoch = initial->session_epoch, .command_id = command_id};
    REQUIRE(fixture.authority.result(result_request));
    CHECK(
        fixture.authority.result(result_request)->stage == engine_client::command_stage::validated);
    const auto native = game_client::resolve_input_command(fixture.queued.front(), point{80, 24});
    REQUIRE(native);
    const auto event = fixture.authority.delivered(*native);
    CHECK(event.type == input_event_t::interaction);
    CHECK(
        fixture.authority.result(result_request)->stage == engine_client::command_stage::executing);
    fixture.queued.clear();
    game_client::begin_input_boundary();
    auto result_input = std::istringstream{
        request("bn.command.result",
                "{\"session_epoch\":\"" + initial->session_epoch + "\",\"command_id\":\""
                    + command_id + "\"}",
                "4")
        + "\n"};
    CHECK_FALSE(server.pump_until_input(result_input, output, errors));
    CHECK(output.str().find("\"stage\":\"completed\"") != std::string::npos);
    CHECK(output.str().find("native_input_delivered") != std::string::npos);
    CHECK(output.str().find("\"resync_required\":false") != std::string::npos);
    const auto completed = fixture.authority.result(result_request);
    REQUIRE(completed);
    REQUIRE(completed->completed);
    CHECK(completed->completed->state_revision > initial->state_revision);
}

TEST_CASE(
    "oversized default page publishes a one-row reference instead of failing the session",
    "[engine_client_server]") {
    auto boundary = wide_boundary{3000};
    auto authority = engine_client::session{};
    REQUIRE(authority.publish_boundary());
    const auto one_row = authority.read_snapshot({.offset = 0, .limit = 1});
    REQUIRE(one_row);
    CHECK(one_row->state.interaction->choices.size() == 1);
    const auto default_page = authority.read_snapshot({});
    REQUIRE_FALSE(default_page);
    CHECK(default_page.error() == engine_client::error::resource_limit);
}

TEST_CASE(
    "oversized later boundary starts a new epoch and keeps the session alive",
    "[engine_client_server]") {
    auto authority = engine_client::session{};
    auto first_epoch = std::string{};
    {
        auto small = input_boundary{};
        REQUIRE(authority.publish_boundary());
        first_epoch = authority.epoch();
    }
    auto wide = wide_boundary{3000};
    REQUIRE(authority.publish_boundary());
    CHECK(authority.epoch() != first_epoch);
    REQUIRE(authority.read_snapshot({.offset = 0, .limit = 1}));
}

TEST_CASE(
    "boundary whose event envelope overflows resynchronizes from a snapshot",
    "[engine_client_server]") {
    auto resynchronized = 0;
    for (auto bytes = std::size_t{2400}; bytes < 2700; bytes += 4) {
        auto authority = engine_client::session{};
        const auto epoch = authority.epoch();
        auto before_revision = engine_client::counter{};
        {
            auto narrow = wide_boundary{bytes - 100, "Before"};
            REQUIRE(authority.publish_boundary());
            const auto before = authority.read_snapshot({.offset = 0, .limit = 1});
            REQUIRE(before);
            before_revision = before->state_revision;
        }
        auto wide = wide_boundary{bytes, "After"};
        const auto published = authority.publish_boundary();
        REQUIRE(published);
        const auto after = authority.read_snapshot({.offset = 0, .limit = 1});
        REQUIRE(after);
        // Past the page limit the session restarts its epoch, which has no earlier revision.
        if (authority.epoch() == epoch) { CHECK(after->state_revision > before_revision); }
        if (!authority.latest_event()) { ++resynchronized; }
    }
    CHECK(resynchronized > 0);
}

TEST_CASE(
    "world replacement interrupts an outstanding receipt without successful completion",
    "[engine_client_server]") {
    auto boundary = input_boundary{};
    auto authority = engine_client::session{};
    REQUIRE(authority.publish_boundary());
    const auto initial = authority.read_snapshot({});
    REQUIRE(initial);
    const auto decoded = engine_client::decode_command_request(command(*initial));
    REQUIRE(decoded);
    const auto receipt = authority.submit(*decoded);
    REQUIRE(receipt);
    authority.replace_world();
    REQUIRE(authority.publish_boundary());
    CHECK(authority.epoch() != receipt->session_epoch);
    const auto interrupted = authority.result(
        {.session_epoch = receipt->session_epoch, .command_id = receipt->command_id});
    REQUIRE(interrupted);
    CHECK(interrupted->stage == engine_client::command_stage::interrupted);
    CHECK_FALSE(interrupted->completed);
    CHECK_FALSE(interrupted->execution_started);
    CHECK_FALSE(authority.has_received());
}

TEST_CASE(
    "output loss interrupts a receipt before native queue delivery", "[engine_client_server]") {
    auto boundary = input_boundary{};
    auto fixture = ::fixture{};
    REQUIRE(fixture.authority.publish_boundary());
    const auto initial = fixture.authority.read_snapshot({});
    REQUIRE(initial);
    auto server = fixture.server();
    auto input = std::istringstream{request("bn.contract.negotiate", capabilities) + "\n"};
    auto good_output = std::ostringstream{};
    auto errors = std::ostringstream{};
    CHECK_FALSE(server.pump_until_input(input, good_output, errors));
    // A fresh pump needs a live input stream; use one complete batch so failure happens after
    // receipt allocation.
    auto second = fixture.server();
    auto batch = std::istringstream{
        "[" + request("bn.contract.negotiate", capabilities) + ","
        + request("bn.command.submit", command(*initial), "2") + "]\n"};
    auto buffer = broken_output{};
    auto output = std::ostream{&buffer};
    CHECK_FALSE(second.pump_until_input(batch, output, errors));
    CHECK(second.failed());
    CHECK(fixture.queued.empty());
    CHECK_FALSE(fixture.authority.has_received());
}

TEST_CASE("legacy batch cursor resumes only after native input returns", "[engine_client_server]") {
    auto boundary = input_boundary{};
    auto fixture = ::fixture{};
    auto server = fixture.server();
    const auto first =
        request("tools/call", R"({"name":"bn.press","arguments":{"keys":[{"key":"a"}]}})", "2");
    const auto second =
        request("tools/call", R"({"name":"bn.press","arguments":{"keys":[{"key":"b"}]}})", "3");
    auto input = std::istringstream{
        request("initialize", "{}") + "\n"
        + R"({"jsonrpc":"2.0","method":"notifications/initialized"})" + "\n[" + first + "," + second
        + "]\n"};
    auto output = std::ostringstream{};
    auto errors = std::ostringstream{};
    REQUIRE(server.pump_until_input(input, output, errors));
    REQUIRE(fixture.queued.size() == 1);
    CHECK(fixture.queued.front().key == "a");
    CHECK(output.str().find("\"id\":2") == std::string::npos);
    const auto native = game_client::resolve_input_command(fixture.queued.front(), point{80, 24});
    REQUIRE(native);
    CHECK(fixture.authority.delivered(*native).type == input_event_t::keyboard);
    fixture.queued.clear();
    game_client::begin_input_boundary();
    REQUIRE(server.pump_until_input(input, output, errors));
    REQUIRE(fixture.queued.size() == 1);
    CHECK(fixture.queued.front().key == "b");
    CHECK(output.str().find("\"id\":2") == std::string::npos);
    fixture.queued.clear();
    game_client::begin_input_boundary();
    CHECK_FALSE(server.pump_until_input(input, output, errors));
    CHECK_FALSE(server.failed());
    CHECK(output.str().find("[{\"jsonrpc\":\"2.0\",\"id\":2") != std::string::npos);
    CHECK(output.str().find("\"id\":3") != std::string::npos);
    CHECK(errors.str().empty());
}

TEST_CASE(
    "mixed decoded method policies preserve legacy errors and direct IDs",
    "[engine_client_server]") {
    auto boundary = input_boundary{};
    auto fixture = ::fixture{};
    auto server = fixture.server();
    auto input = std::istringstream{
        "[" + request("bn.contract.\\u006eegotiate", capabilities, "1.5") + ","
        + request("ping", "{}", "1.5") + ","
        + R"({"jsonrpc":"2.0","method":"unknown/legacy","params":null})" + "]\n"};
    auto output = std::ostringstream{};
    auto errors = std::ostringstream{};
    REQUIRE(server.run(input, output, errors) == 0);
    CHECK(output.str().find("\"id\":1.5,\"result\"") != std::string::npos);
    CHECK(output.str().find("\"id\":null,\"error\":{\"code\":-32600") != std::string::npos);
    CHECK(output.str().find("-32601") == std::string::npos);
    CHECK(errors.str().empty());
}

TEST_CASE(
    "negotiated direct authority blocks initialized legacy mutation", "[engine_client_server]") {
    auto boundary = input_boundary{};
    auto fixture = ::fixture{};
    auto server = fixture.server();
    auto input = std::istringstream{
        request("initialize", "{}") + "\n"
        + R"({"jsonrpc":"2.0","method":"notifications/initialized"})" + "\n"
        + request("bn.contract.negotiate", capabilities, "2") + "\n"
        + request("tools/call", R"({"name":"bn.press","arguments":{"keys":[{"key":"a"}]}})", "3")
        + "\n"};
    auto output = std::ostringstream{};
    auto errors = std::ostringstream{};
    CHECK(server.run(input, output, errors) == 0);
    CHECK(output.str().find("\"code\":1000") != std::string::npos);
    CHECK(output.str().find("\"kind\":\"invalid_lifecycle\"") != std::string::npos);
    CHECK(fixture.queued.empty());
}

TEST_CASE(
    "old epoch snapshot and result errors require fresh negotiation",
    "[engine_client_server][world_epoch_recovery]") {
    auto boundary = input_boundary{};
    auto fixture = ::fixture{};
    REQUIRE(fixture.authority.publish_boundary());
    const auto old_epoch = fixture.authority.epoch();
    auto server = fixture.server();
    fixture.authority.replace_world();
    REQUIRE(fixture.authority.publish_boundary());
    const auto current = fixture.authority.read_snapshot({});
    REQUIRE(current);
    REQUIRE(current->session_epoch != old_epoch);
    auto input = std::istringstream{
        request("bn.contract.negotiate", capabilities, R"("negotiate")") + "\n"
        + request("bn.snapshot.get", snapshot_params(old_epoch), R"("snapshot")") + "\n"
        + request("bn.command.result",
                  result_params({.session_epoch = old_epoch, .command_id = "command:unknown"}),
                  R"("result")")
        + "\n"};
    auto output = std::ostringstream{};
    auto errors = std::ostringstream{};
    REQUIRE(server.run(input, output, errors) == 0);
    auto responses = std::istringstream{output.str()};
    auto line = std::string{};
    REQUIRE(static_cast<bool>(std::getline(responses, line)));
    REQUIRE(static_cast<bool>(std::getline(responses, line)));
    check_recovery(
        line,
        {.id = "snapshot",
         .kind = "stale_epoch",
         .stage = "validation",
         .action = "negotiate",
         .current = *current});
    REQUIRE(static_cast<bool>(std::getline(responses, line)));
    check_recovery(
        line,
        {.id = "result",
         .kind = "stale_epoch",
         .stage = "completion",
         .action = "negotiate",
         .current = *current});
    CHECK_FALSE(static_cast<bool>(std::getline(responses, line)));
    CHECK(fixture.queued.empty());
    CHECK(fixture.deliveries == 0);
    CHECK_FALSE(fixture.authority.has_received());
    CHECK(errors.str().empty());
}

TEST_CASE(
    "fresh negotiation recovers after a delivered old command is interrupted without replay",
    "[engine_client_server][world_epoch_recovery]") {
    auto boundary = input_boundary{};
    auto fixture = ::fixture{};
    REQUIRE(fixture.authority.publish_boundary());
    const auto initial = fixture.authority.read_snapshot({});
    REQUIRE(initial);
    auto server = fixture.server();
    auto input = std::istringstream{
        request("bn.contract.negotiate", capabilities) + "\n"
        + request("bn.command.submit", command(*initial), R"("submit")") + "\n"};
    auto output = std::ostringstream{};
    auto errors = std::ostringstream{};
    REQUIRE(server.pump_until_input(input, output, errors));
    REQUIRE(fixture.queued.size() == 1);
    REQUIRE(fixture.deliveries == 1);
    const auto receipt_offset = output.str().find('\n') + 1;
    auto response = wire_response{output.str().substr(receipt_offset)};
    auto receipt = response.envelope.get_object("result");
    receipt.allow_omitted_members();
    const auto old_command = engine_client::result_request{
        .session_epoch = receipt.get_string("session_epoch"),
        .command_id = receipt.get_string("command_id"),
    };
    REQUIRE(old_command.session_epoch == initial->session_epoch);
    const auto native = game_client::resolve_input_command(fixture.queued.front(), point{80, 24});
    REQUIRE(native);
    CHECK(fixture.authority.delivered(*native).type == input_event_t::interaction);
    fixture.queued.clear();
    fixture.authority.replace_world();
    game_client::begin_input_boundary();
    REQUIRE(fixture.authority.publish_boundary());
    const auto current = fixture.authority.read_snapshot({});
    REQUIRE(current);
    REQUIRE(current->session_epoch != initial->session_epoch);
    const auto interrupted = fixture.authority.result(old_command);
    REQUIRE(interrupted);
    REQUIRE(interrupted->stage == engine_client::command_stage::interrupted);
    CHECK(interrupted->validation_succeeded);
    CHECK(interrupted->execution_started);
    CHECK_FALSE(interrupted->completed);
    CHECK_FALSE(fixture.authority.has_received());

    output.str("");
    input.str(
        request("bn.command.result", result_params(old_command), R"("terminal")") + "\n"
        + request("bn.snapshot.get", snapshot_params(initial->session_epoch), R"("stale")") + "\n"
        + request("bn.contract.negotiate", capabilities, R"("fresh")") + "\n"
        + request("bn.snapshot.get", snapshot_params(current->session_epoch), R"("snapshot")")
        + "\n" + request("bn.command.result", result_params(old_command), R"("retired")") + "\n");
    CHECK_FALSE(server.pump_until_input(input, output, errors));
    CHECK_FALSE(server.failed());
    auto responses = std::istringstream{output.str()};
    auto line = std::string{};
    for (const auto id : {"terminal", "stale", "fresh", "snapshot", "retired"}) {
        REQUIRE(static_cast<bool>(std::getline(responses, line)));
        auto reply = wire_response{line};
        CHECK(reply.envelope.get_string("id") == id);
        if (std::string_view{id} == "stale") {
            check_recovery(
                line,
                {.id = id,
                 .kind = "stale_epoch",
                 .stage = "validation",
                 .action = "negotiate",
                 .current = *current});
            continue;
        }
        CHECK_FALSE(reply.envelope.has_member("error"));
        auto result = reply.envelope.get_object("result");
        result.allow_omitted_members();
        if (std::string_view{id} == "terminal" || std::string_view{id} == "retired") {
            auto command = result.get_object("command");
            command.allow_omitted_members();
            CHECK(command.get_string("session_epoch") == old_command.session_epoch);
            CHECK(command.get_string("command_id") == old_command.command_id);
            CHECK(command.get_string("stage") == "interrupted");
            CHECK(command.has_null("completion"));
            auto events = result.get_object("event_batch");
            events.allow_omitted_members();
            CHECK(events.get_array("events").empty());
        } else {
            CHECK(result.get_string("session_epoch") == current->session_epoch);
            if (std::string_view{id} == "fresh") {
                CHECK(result.get_string("contract_version") == "1.0");
            } else {
                CHECK(
                    result.get_string("state_revision") == std::to_string(current->state_revision));
                CHECK(result.get_string("through_public_sequence")
                      == std::to_string(current->through_public_sequence));
                CHECK(result.has_object("state"));
            }
        }
    }
    CHECK_FALSE(static_cast<bool>(std::getline(responses, line)));
    CHECK(fixture.deliveries == 1);
    CHECK(fixture.queued.empty());
    CHECK_FALSE(fixture.authority.has_received());
    const auto after = fixture.authority.read_snapshot({});
    REQUIRE(after);
    CHECK(after->state_revision == current->state_revision);
    CHECK(after->through_public_sequence == current->through_public_sequence);
    CHECK(errors.str().empty());
}

TEST_CASE(
    "stale command validation never delivers or advances authority",
    "[engine_client_server][world_epoch_recovery]") {
    auto boundary = input_boundary{};
    auto fixture = ::fixture{};
    REQUIRE(fixture.authority.publish_boundary());
    const auto current = fixture.authority.read_snapshot({});
    REQUIRE(current);
    auto based_on = *current;
    auto reason = std::string{};
    SECTION("old epoch") {
        based_on.session_epoch = "epoch:retired";
        reason = "stale_epoch";
    }
    SECTION("same epoch stale revision") {
        ++based_on.state_revision;
        reason = "stale_revision";
    }
    SECTION("same epoch stale boundary") {
        based_on.state.input_boundary_id = "boundary:retired";
        reason = "stale_boundary";
    }
    SECTION("same epoch stale schema") {
        based_on.state.interaction->schema_id = "schema:retired";
        reason = "stale_interaction_schema";
    }
    auto server = fixture.server();
    auto input = std::istringstream{
        request("bn.contract.negotiate", capabilities) + "\n"
        + request("bn.command.submit", command(based_on), R"("submit")") + "\n"};
    auto output = std::ostringstream{};
    auto errors = std::ostringstream{};
    CHECK_FALSE(server.pump_until_input(input, output, errors));
    CHECK_FALSE(server.failed());
    auto response = wire_response{output.str().substr(output.str().find('\n') + 1)};
    auto receipt = response.envelope.get_object("result");
    receipt.allow_omitted_members();
    const auto rejected = engine_client::result_request{
        .session_epoch = receipt.get_string("session_epoch"),
        .command_id = receipt.get_string("command_id"),
    };
    const auto result = fixture.authority.result(rejected);
    REQUIRE(result);
    CHECK(result->stage == engine_client::command_stage::rejected);
    REQUIRE(result->failure);
    CHECK(engine_client::error_name(*result->failure) == reason);
    CHECK_FALSE(result->execution_started);
    CHECK_FALSE(result->completed);
    CHECK(fixture.deliveries == 0);
    CHECK(fixture.queued.empty());
    CHECK_FALSE(fixture.authority.has_received());

    // Rejection is a terminal result, not a JSON-RPC application error. Preserve that 1.0 shape.
    input.str(
        request("bn.command.result", result_params(rejected), R"("rejected")") + "\n"
        + request("bn.snapshot.get", snapshot_params(current->session_epoch), R"("snapshot")")
        + "\n");
    input.clear();
    output.str("");
    REQUIRE(server.run(input, output, errors) == 0);
    auto responses = std::istringstream{output.str()};
    auto line = std::string{};
    REQUIRE(static_cast<bool>(std::getline(responses, line)));
    auto reply = wire_response{line};
    auto wire_result = reply.envelope.get_object("result");
    wire_result.allow_omitted_members();
    auto wire_command = wire_result.get_object("command");
    wire_command.allow_omitted_members();
    CHECK(wire_command.get_string("stage") == "rejected");
    auto validation = wire_command.get_object("validation");
    validation.allow_omitted_members();
    CHECK(validation.get_string("error") == reason);
    REQUIRE(static_cast<bool>(std::getline(responses, line)));
    auto snapshot_reply = wire_response{line};
    CHECK_FALSE(snapshot_reply.envelope.has_member("error"));
    auto snapshot = snapshot_reply.envelope.get_object("result");
    snapshot.allow_omitted_members();
    CHECK(snapshot.get_string("session_epoch") == current->session_epoch);
    CHECK(snapshot.get_string("state_revision") == std::to_string(current->state_revision));
    CHECK(snapshot.get_string("through_public_sequence")
          == std::to_string(current->through_public_sequence));
    CHECK(errors.str().empty());
}

#endif
