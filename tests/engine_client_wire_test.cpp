#include "avatar.h"
#include "catch/catch.hpp"
#include "client_command.h"
#include "client_input.h"
#include "client_interaction.h"
#include "engine_client_wire.h"
#include "input.h"
#include "json.h"
#include "rng.h"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace {
using namespace engine_client;
const auto negotiation = std::string{
    R"({"supported_versions":["1.0"],"required_capabilities":[],"optional_capabilities":[]})"};
const auto snapshot_params = std::string{
    R"({"session_epoch":"e","page":{"offset":0,"limit":200}})"};
const auto result_params = std::string{R"({"session_epoch":"e","command_id":"c"})"};
auto command(const std::string& operation) -> std::string {
    return R"({"session_epoch":"e","based_on":{"state_revision":"0","input_boundary_id":"b","interaction_schema_id":"s"},"operation":)"
         + operation + "}";
}
auto replace(std::string value, std::string_view from, std::string_view to) -> std::string {
    const auto offset = value.find(from);
    REQUIRE(offset != std::string::npos);
    value.replace(offset, from.size(), to);
    return value;
}
auto read_text(const std::filesystem::path& path) -> std::string {
    auto input = std::ifstream{path, std::ios::binary};
    REQUIRE(input.good());
    return std::string{std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{}};
}
auto actor_state() -> std::string {
    auto output = std::ostringstream{};
    auto json = JsonOut{output};
    get_avatar().serialize(json);
    return output.str();
}
auto sample(const std::expected<std::string, error>& value) -> void {
    REQUIRE(value);
    CHECK(value->size() <= maximum_inline_bytes);
    if (std::getenv("BN_WIRE_SCHEMA_SAMPLES") != nullptr) {
        std::cout << "WIRE_SAMPLE " << *value << '\n';
    }
}
} // namespace

TEST_CASE(
    "wire decodes owned native operation distinctions without live legality",
    "[engine_client_wire]") {
    const auto negotiated = decode_negotiation_request(negotiation);
    REQUIRE(negotiated);
    CHECK(negotiated->supported_versions == std::vector<std::string>{"1.0"});
    CHECK(negotiate(*negotiated));
    // Unknown capabilities/versions are valid strings, not a parsing-time policy decision.
    CHECK(decode_negotiation_request(replace(negotiation, "1.0", "unknown")));
    const auto page = decode_snapshot_request(snapshot_params);
    REQUIRE(page);
    CHECK(page->session_epoch == "e");
    CHECK(page->page == projection{.offset = 0, .limit = 200});
    const auto lookup = decode_result_request(result_params);
    REQUIRE(lookup);
    CHECK(lookup->command_id == "c");
    const auto operations = std::array{
        R"({"kind":"choose","choice_id":"unknown-live-id"})",
        R"({"kind":"fill","field_id":"f","value":"","submit":false})",
        R"({"kind":"set_count","choice_id":"c","count":0})",
        R"({"kind":"set_target","position":{"space":"reality_bubble_map_square","frame_id":"stale","x":-2147483648,"y":2147483647,"z":0}})",
        R"({"kind":"cancel"})",
        R"({"kind":"invoke_registered_action","action_id":"unregistered"})"};
    for (const auto operation : operations) {
        const auto decoded = decode_command_request(command(operation));
        CAPTURE(operation);
        REQUIRE(decoded);
        CHECK(decoded->state_revision == 0);
        if (const auto semantic = std::get_if<semantic_operation>(&decoded->operation)) {
            const auto& native = semantic->command;
            CHECK(native.input_id == 0);
            CHECK_FALSE(native.position); // Frame checking/mapping belongs to core validation.
            if (native.operation == game_client::interaction_operation::fill) {
                REQUIRE(native.submit.has_value());
                CHECK_FALSE(*native.submit);
                CHECK(native.value.empty());
            } else if (native.operation == game_client::interaction_operation::set_count) {
                REQUIRE(native.count.has_value());
                CHECK(*native.count == 0);
            } else if (native.operation == game_client::interaction_operation::set_target) {
                REQUIRE(semantic->target);
                CHECK(semantic->target->frame_id == "stale");
                CHECK(semantic->target->position.x == INT32_MIN);
                CHECK(semantic->target->position.y == INT32_MAX);
                CHECK(native.target_id.empty());
            }
        } else {
            CHECK(std::get<registered_action>(decoded->operation).id == "unregistered");
        }
    }
    const auto candidate = decode_command_request(command(
        R"({"candidate_id":"c","position":{"z":0,"y":0,"x":0,"frame_id":"b","space":"reality_bubble_map_square"},"kind":"set_target"})"));
    REQUIRE(candidate);
    CHECK(std::get<semantic_operation>(candidate->operation).command.target_id == "c");
    const auto registered =
        replace(command(operations.back()), R"(,"interaction_schema_id":"s")", "");
    CHECK(decode_command_request(registered));
    CHECK_FALSE(decode_command_request(replace(registered, "invoke_registered_action", "choose")));
}

TEST_CASE(
    "wire rejects closed grammar violations including decoded duplicate keys",
    "[engine_client_wire]") {
    const auto invalid_negotiations = std::array{
        "null",
        "[]",
        "{}",
        R"({"supported_versions":["1.0"]})",
        R"({"supported_versions":[],"required_capabilities":[],"optional_capabilities":[]})",
        R"({"supported_versions":["1.0","\u0031.0"],"required_capabilities":[],"optional_capabilities":[]})",
        R"({"supported_versions":[null],"required_capabilities":[],"optional_capabilities":[]})",
        R"({"supported_versions":[1],"required_capabilities":[],"optional_capabilities":[]})"};
    for (const auto value : invalid_negotiations) {
        CAPTURE(value);
        CHECK_FALSE(decode_negotiation_request(value));
    }
    CHECK_FALSE(decode_negotiation_request(replace(negotiation, "[]", "[\"a\",\"a\"]")));
    CHECK_FALSE(decode_negotiation_request(
        replace(negotiation, R"("optional_capabilities":[])", R"("optional_capabilities":null)")));
    CHECK_FALSE(decode_negotiation_request(
        replace(negotiation, R"("optional_capabilities":[])",
                R"("optional_capabilities":{},"unknown":0)")));
    const auto invalid_pages = std::array{
        "null",
        "[]",
        "{}",
        R"({"offset":0})",
        R"({"offset":0,"limit":0})",
        R"({"offset":0,"limit":201})",
        R"({"offset":-1,"limit":1})",
        R"({"offset":null,"limit":1})",
        R"({"offset":0,"limit":"1"})",
        R"({"offset":0,"limit":true})",
        R"({"offset":0,"limit":1,"extra":0})",
        R"({"offset":0,"off\u0073et":1,"limit":1})"};
    for (const auto page : invalid_pages) {
        CAPTURE(page);
        CHECK_FALSE(
            decode_snapshot_request(replace(snapshot_params, R"({"offset":0,"limit":200})", page)));
    }
    const auto choose = command(R"({"kind":"choose","choice_id":"c"})");
    const auto invalid_ops = std::array{
        "null",
        "[]",
        "{}",
        R"({"kind":"unknown"})",
        R"({"kind":"choose"})",
        R"({"kind":"choose","choice_id":""})",
        R"({"kind":"choose","choice_id":null})",
        R"({"kind":"choose","choice_id":"c","field_id":"f"})",
        R"({"kind":"choose","choice_id":"c","value":""})",
        R"({"kind":"choose","choice_id":"c","count":0})",
        R"({"kind":"choose","choice_id":"c","kind":"cancel"})",
        R"({"kind":"cancel","candidate_id":"c"})",
        R"({"kind":"cancel","extra":0})",
        R"({"kind":"fill","field_id":"f","value":""})",
        R"({"kind":"fill","field_id":"f","value":null,"submit":false})",
        R"({"kind":"fill","field_id":"f","value":"","submit":null})",
        R"({"kind":"fill","field_id":"f","value":"","submit":0})",
        R"({"kind":"set_count","choice_id":"c","count":-1})",
        R"({"kind":"set_count","choice_id":"c","count":"1"})",
        R"({"kind":"set_target","candidate_id":"c"})",
        R"({"kind":"set_target","position":null})",
        R"({"kind":"set_target","position":{"space":"absolute_map_square","dimension_id":"d","x":0,"y":0,"z":0}})",
        R"({"kind":"invoke_registered_action","action_id":"a","submit":false})"};
    for (const auto op : invalid_ops) {
        CAPTURE(op);
        CHECK_FALSE(decode_command_request(command(op)));
    }
    CHECK_FALSE(decode_command_request(replace(choose, R"(,"interaction_schema_id":"s")", "")));
    CHECK_FALSE(decode_command_request(
        replace(choose, R"("interaction_schema_id":"s")", R"("interaction_schema_id":null)")));
    CHECK_FALSE(decode_command_request(
        replace(choose, R"("input_boundary_id":"b")", R"("input_boundary_id":"b","input_id":0)")));
    for (const auto member : {"session_epoch", "command_id"}) {
        const auto key = std::string{"\""} + member + "\"";
        CHECK_FALSE(decode_result_request(replace(result_params, key, key + ":null," + key)));
        CHECK_FALSE(decode_result_request(replace(result_params, member, "unknown")));
    }
    CHECK_FALSE(
        decode_result_request(R"({"session_epoch":"e","command_id":"c","command\u005fid":"d"})"));
    CHECK_FALSE(decode_snapshot_request(replace(
        snapshot_params, R"("session_epoch":"e")", R"("session_epoch":"e","session_epoch":"e")")));
    CHECK_FALSE(decode_command_request(
        replace(choose, R"("state_revision":"0")", R"("state_revision":"0","state_revision":"0")")));
}

TEST_CASE(
    "wire numeric conversion is exact for decimal exponent and canonical uint64 strings",
    "[engine_client_wire]") {
    const auto accepted = std::array{
        "1",
        "1.0",
        "1e0",
        "1E+0",
        "0.1e1",
        "100e-2",
        "0.0001e4",
        "1.0000000000000000000000",
        "0",
        "-0",
        "-0.0e999999999999999999999",
        "0e-99999999999999999999999999",
        "9007199254740991.0",
        "900719925474099100000e-5",
        "9.007199254740991e15"};
    for (const auto number : accepted) {
        CAPTURE(number);
        const auto decoded = decode_command_request(
            command(std::string{R"({"kind":"set_count","choice_id":"c","count":)"} + number + "}"));
        REQUIRE(decoded);
        const auto expected =
            std::string_view{number}.starts_with("900") || std::string_view{number}.starts_with("9.")
                ? maximum_safe_integer
            : std::string_view{number}.find('1') != std::string_view::npos
                ? 1u
                : 0u;
        CHECK(std::get<semantic_operation>(decoded->operation).command.count == expected);
    }
    const auto rejected = std::array{
        "1.0000000000000000000001",
        "0.9999999999999999999999",
        "9007199254740990.9999999999999",
        "9007199254740991.0000000000001",
        "9007199254740992",
        "1e999999999999999999999",
        "1e-999999999999999999999",
        "1e-1",
        "10.1",
        "-1e0",
        "01",
        "00",
        "+1",
        ".1",
        "1.",
        "1e",
        "1e+",
        "NaN",
        "Infinity",
        "0x1"};
    for (const auto number : rejected) {
        CAPTURE(number);
        CHECK_FALSE(decode_command_request(command(
            std::string{R"({"kind":"set_count","choice_id":"c","count":)"} + number + "}")));
    }
    auto target = command(
        R"({"kind":"set_target","position":{"space":"reality_bubble_map_square","frame_id":"b","x":0,"y":0,"z":0}})");
    for (const auto number :
         {"-2147483648.0", "2147483647e0", "-2.147483648e9", "-0e99999999999999999"}) {
        CAPTURE(number);
        CHECK(decode_command_request(replace(target, R"("x":0)", std::string{R"("x":)"} + number)));
    }
    for (const auto number :
         {"2147483648", "-2147483649", "2147483647.00000000000001", "-2147483648.0000000001"}) {
        CAPTURE(number);
        CHECK_FALSE(
            decode_command_request(replace(target, R"("x":0)", std::string{R"("x":)"} + number)));
    }
    const auto cancel = command(R"({"kind":"cancel"})");
    const auto maximum = decode_command_request(
        replace(cancel, R"("state_revision":"0")", R"("state_revision":"18446744073709551615")"));
    REQUIRE(maximum);
    CHECK(maximum->state_revision == std::numeric_limits<counter>::max());
    for (const auto value :
         {"\"18446744073709551616\"", "\"184467440737095516150\"", "\"01\"", "\"-0\"", "\"+1\"",
          "\"1.0\"", "\"1e0\"", "\" 1\"", "\"\"", "1", "null"}) {
        CAPTURE(value);
        CHECK_FALSE(decode_command_request(replace(
            cancel, R"("state_revision":"0")", std::string{R"("state_revision":)"} + value)));
    }
    const auto safe_offset = decode_snapshot_request(
        replace(snapshot_params, R"("offset":0)", R"("offset":9007199254740991.0)"));
    CHECK(safe_offset.has_value() == (std::numeric_limits<size_t>::max() >= 9007199254740991ULL));
    const auto maximum_offset =
        std::min<counter>(std::numeric_limits<size_t>::max(), 9007199254740991ULL);
    const auto native_offset = decode_snapshot_request(
        replace(snapshot_params, R"("offset":0)",
                std::string{R"("offset":)"} + std::to_string(maximum_offset) + ".0"));
    REQUIRE(native_offset);
    CHECK(native_offset->page.offset == maximum_offset);
    CHECK_FALSE(decode_snapshot_request(
        replace(snapshot_params, R"("offset":0)",
                std::string{R"("offset":)"} + std::to_string(maximum_offset + 1))));
    CHECK_FALSE(decode_snapshot_request(
        replace(snapshot_params, R"("offset":0)", R"("offset":9007199254740992)")));
    CHECK_FALSE(decode_snapshot_request(
        replace(snapshot_params, R"("offset":0)", R"("offset":1.0000000000000001)")));
    // Large numeric spellings stay bounded, without floating point or exponent-sized allocation.
    CHECK(decode_command_request(command(
        std::string{R"({"kind":"set_count","choice_id":"c","count":1.)"} + std::string(100000, '0')
        + "}")));
    CHECK_FALSE(decode_command_request(command(
        std::string{R"({"kind":"set_count","choice_id":"c","count":1.)"} + std::string(100000, '0')
        + "1}")));
}

TEST_CASE(
    "wire preserves NUL supplementary Unicode and strict UTF8 with byte limited IDs",
    "[engine_client_wire]") {
    const auto unicode = decode_result_request(
        R"({"session_epoch":"a\u0000b","command_id":"\ud83d\ude80"})");
    REQUIRE(unicode);
    CHECK(unicode->session_epoch == std::string("a\0b", 3));
    CHECK(unicode->command_id == "\xf0\x9f\x9a\x80");
    CHECK(decode_result_request(
        std::string{R"({"session_epoch":"a","command_id":")"} + "\xf0\x9f\x9a\x80\"}"));
    const auto fill = decode_command_request(command(
        R"({"kind":"fill","field_id":"f","value":"\u0000\udbff\udfff\b\f\n\r\t\/\\\"","submit":true})"));
    REQUIRE(fill);
    CHECK(std::get<semantic_operation>(fill->operation).command.value
          == std::string("\0\xf4\x8f\xbf\xbf\b\f\n\r\t/\\\"", 13));
    const auto bad_escapes = std::array<std::string_view, 10>{
        R"(\ud800)",
        R"(\udfff)",
        R"(\ud800\u0000)",
        R"(\ud800\ud800)",
        R"(\ud83dX)",
        R"(\u12)",
        R"(\uGGGG)",
        R"(\x00)",
        std::string_view{"\x01"},
        std::string_view{"\0", 1}};
    for (const auto value : bad_escapes) {
        CAPTURE(value);
        CHECK_FALSE(decode_result_request(
            std::string{R"({"session_epoch":"e","command_id":")"} + std::string{value} + "\"}"));
    }
    const auto bad_utf8 = std::array{
        "\x80",
        "\xc0\x80",
        "\xc1\xbf",
        "\xe0\x80\x80",
        "\xed\xa0\x80",
        "\xf0\x80\x80\x80",
        "\xf4\x90\x80\x80",
        "\xf5\x80\x80\x80",
        "\xff",
        "\xc2",
        "\xe2\x28\xa1",
        "\xef\xbb"};
    for (const auto value : bad_utf8) {
        CAPTURE(value);
        CHECK_FALSE(decode_result_request(
            replace(result_params, R"("c")", std::string{"\""} + value + "\"")));
        CHECK_FALSE(serialize_receipt({.session_epoch = "e", .command_id = value}));
    }
    for (const auto size : {0u, 257u}) {
        CHECK_FALSE(decode_result_request(
            replace(result_params, R"("c")", "\"" + std::string(size, 'x') + "\"")));
        CHECK_FALSE(
            serialize_receipt({.session_epoch = "e", .command_id = std::string(size, 'x')}));
    }
    CHECK(decode_result_request(
        replace(result_params, R"("c")", "\"" + std::string(256, 'x') + "\"")));
    auto escaped = std::string{};
    for (auto index = 0; index < 64; ++index) { escaped += R"(\ud83d\ude80)"; }
    CHECK(decode_result_request(replace(result_params, R"("c")", "\"" + escaped + "\"")));
    CHECK_FALSE(decode_result_request(replace(result_params, R"("c")", "\"" + escaped + "x\"")));
    CHECK_FALSE(
        decode_result_request(R"({"session_epoch":"e","command_id":"c","command_id\u0000":"x"})"));
    CHECK_FALSE(decode_negotiation_request(replace(negotiation, "[]", R"(["\ud83d\ude80","🚀"])")));
}

TEST_CASE(
    "wire rejects complete malformed suffixes and bounds bytes depth and allocation",
    "[engine_client_wire]") {
    const auto cancel = command(R"({"kind":"cancel"})");
    for (const auto suffix : {"x", "{}", "[]", "null", ",", "\xef\xbb\xbf", "\x01"}) {
        CAPTURE(suffix);
        CHECK_FALSE(decode_negotiation_request(negotiation + suffix));
        CHECK_FALSE(decode_snapshot_request(snapshot_params + suffix));
        CHECK_FALSE(decode_command_request(cancel + suffix));
        CHECK_FALSE(decode_result_request(result_params + suffix));
    }
    CHECK_FALSE(decode_command_request(cancel + std::string("\0", 1)));
    for (auto size = std::size_t{0}; size < cancel.size(); ++size) {
        CHECK_FALSE(decode_command_request(std::string_view{cancel}.substr(0, size)));
    }
    CHECK(decode_command_request(" \t\r\n" + cancel + "\n\t "));
    for (const auto malformed :
         {"{", "{,}", "{\"a\"}", "{\"a\":", "{\"a\":1,}", "[", "\xef\xbb\xbf{}"}) {
        CHECK_FALSE(decode_result_request(malformed));
    }
    CHECK_FALSE(decode_negotiation_request(replace(negotiation, "[]", "[\"x\",]")));
    CHECK_FALSE(decode_result_request(replace(result_params, "}", ",}")));
    const auto exact = cancel + std::string(maximum_inline_bytes - cancel.size(), ' ');
    CHECK(decode_command_request(exact));
    const auto overflow = decode_command_request(exact + ' ');
    REQUIRE_FALSE(overflow);
    CHECK(overflow.error() == decode_error::resource_limit);
    const auto exact_negotiation =
        negotiation + std::string(maximum_inline_bytes - negotiation.size(), ' ');
    const auto exact_snapshot =
        snapshot_params + std::string(maximum_inline_bytes - snapshot_params.size(), ' ');
    const auto exact_result =
        result_params + std::string(maximum_inline_bytes - result_params.size(), ' ');
    CHECK(decode_negotiation_request(exact_negotiation));
    CHECK(decode_snapshot_request(exact_snapshot));
    CHECK(decode_result_request(exact_result));
    CHECK(decode_negotiation_request(exact_negotiation + ' ').error()
          == decode_error::resource_limit);
    CHECK(decode_snapshot_request(exact_snapshot + ' ').error() == decode_error::resource_limit);
    CHECK(decode_result_request(exact_result + ' ').error() == decode_error::resource_limit);
    const auto deep = std::string(50000, '[') + "0" + std::string(50000, ']');
    CHECK_FALSE(decode_command_request(command(deep)));
    CHECK_FALSE(
        decode_snapshot_request(replace(snapshot_params, R"({"offset":0,"limit":200})", deep)));
    CHECK_FALSE(decode_negotiation_request(replace(negotiation, "[]", deep)));
    auto many = std::string{R"({"supported_versions":[)"};
    for (auto index = 0; index < 10000; ++index) {
        if (index != 0) { many += ','; }
        many += '"' + std::to_string(index) + '"';
    }
    many += R"(],"required_capabilities":[],"optional_capabilities":[]})";
    const auto array = decode_negotiation_request(many);
    REQUIRE(array);
    CHECK(array->supported_versions.size() == 10000);
}

TEST_CASE(
    "wire parsing has no prefix receipt provider authority or RNG effects",
    "[engine_client_wire]") {
    auto context = input_context{"WIRE_TEST"};
    const auto input_scope = game_client::input_context_scope{context, "WIRE_TEST"};
    auto provider_calls = 0;
    const auto interaction_scope = game_client::interaction_scope{
        context, [&]() {
            ++provider_calls;
            return game_client::interaction_snapshot{};
        }};
    class authority final: public command_authority {
    public:
        mutable int calls = 0;
        auto current_permissions() const -> command_permissions override {
            ++calls;
            return {};
        }
    };
    auto policy = authority{};
    auto lifecycle = command_lifecycle{"e", policy};
    const auto rng_before = rng_get_engine();
    const auto boundary_before = game_client::current_input_id();
    const auto actor_before = actor_state();
    const auto cancel = command(R"({"kind":"cancel"})");
    for (const auto suffix : {"x", ",{}", "\n{"}) {
        const auto parsed = decode_command_request(cancel + suffix);
        if (parsed) { FAIL("malformed suffix must not admit a prefix receipt"); }
        CHECK_FALSE(lifecycle.result("e:command:1"));
    }
    const auto parsed = decode_command_request(cancel);
    REQUIRE(parsed);
    CHECK(provider_calls == 0);
    CHECK(policy.calls == 0);
    CHECK(game_client::current_input_id() == boundary_before);
    CHECK(actor_state() == actor_before);
    CHECK(rng_get_engine() == rng_before);
    auto untouched = command_lifecycle{"e", policy};
    const auto received = lifecycle.submit(*parsed);
    const auto reference = untouched.submit(*parsed);
    REQUIRE(received);
    REQUIRE(reference);
    CHECK(received->command_id == reference->command_id);
    CHECK(provider_calls == 0);
    CHECK(policy.calls == 0);
    sample(serialize_receipt(
        {.session_epoch = received->session_epoch, .command_id = received->command_id}));
}

TEST_CASE("wire receipt and closed application error serialization", "[engine_client_wire]") {
    CHECK(application_error_code == 1000);
    CHECK(std::string{application_error_message} == "Engine contract error");
    const auto received = serialize_receipt({.session_epoch = "e", .command_id = "c"});
    sample(received);
    auto input = std::istringstream{*received};
    auto json = JsonIn{input};
    const auto object = json.get_object();
    CHECK(object.size() == 3);
    CHECK(object.get_string("session_epoch") == "e");
    CHECK(object.get_string("command_id") == "c");
    CHECK(object.get_string("stage") == "received");
    const auto unicode_receipt = serialize_receipt(
        {.session_epoch = std::string("e\0x", 3), .command_id = "🚀"});
    sample(unicode_receipt);
    const auto unicode_lookup = decode_result_request(
        replace(*unicode_receipt, R"(,"stage":"received")", ""));
    REQUIRE(unicode_lookup);
    CHECK(unicode_lookup->session_epoch == std::string("e\0x", 3));
    CHECK(unicode_lookup->command_id == "🚀");
    const auto escaped = serialize_receipt(
        {.session_epoch = std::string(256, '\0'), .command_id = std::string(256, '\0')});
    sample(escaped);
    CHECK(escaped->find("\\u0000") != std::string::npos);
    for (const auto kind :
         {error::negotiation_failed, error::unsupported_capability, error::not_ready,
          error::stale_epoch, error::stale_revision, error::stale_boundary,
          error::stale_interaction_schema, error::validation_failed, error::command_busy,
          error::unknown_command, error::invalid_lifecycle, error::resync_required,
          error::resource_limit}) {
        sample(serialize_application_error({.kind = kind}));
    }
    for (const auto stage :
         {application_error_stage::negotiation, application_error_stage::receipt,
          application_error_stage::validation, application_error_stage::execution,
          application_error_stage::completion}) {
        for (const auto action :
             {required_action::negotiate, required_action::read_snapshot, required_action::retry,
              required_action::none}) {
            const auto value = serialize_application_error(
                {.kind = error::stale_revision,
                 .stage = stage,
                 .retryable = true,
                 .action = action,
                 .current = error_current{
                     .session_epoch = "e",
                     .state_revision = std::numeric_limits<counter>::max(),
                     .through_public_sequence = 0}});
            sample(value);
            CHECK(value->find("18446744073709551615") != std::string::npos);
            CHECK(value->find("diagnostic") == std::string::npos);
        }
    }
    CHECK_FALSE(serialize_application_error({.kind = static_cast<error>(999)}));
    CHECK_FALSE(serialize_application_error({.stage = static_cast<application_error_stage>(999)}));
    CHECK_FALSE(serialize_application_error({.action = static_cast<required_action>(999)}));
    CHECK_FALSE(serialize_application_error({.current = error_current{.session_epoch = ""}}));
    CHECK_FALSE(serialize_application_error(
        {.current = error_current{.session_epoch = std::string(257, 'x')}}));
    CHECK_FALSE(
        serialize_application_error({.current = error_current{.session_epoch = "\xed\xa0\x80"}}));
}

TEST_CASE("wire accepts all three locales normative request examples", "[engine_client_wire]") {
    for (const auto locale : {"en", "ja", "ko"}) {
        const auto text = read_text(
            std::filesystem::path{"docs"} / locale / "dev/reference/engine_client_protocol.md");
        auto offset = std::size_t{0};
        auto examples = 0;
        while (true) {
            const auto start = text.find("```json\n", offset);
            if (start == std::string::npos) { break; }
            const auto end = text.find("\n```", start + 8);
            REQUIRE(end != std::string::npos);
            const auto value = std::string_view{text}.substr(start + 8, end - start - 8);
            if (examples == 0) {
                CHECK(decode_negotiation_request(value));
            } else if (examples == 1) {
                CHECK(decode_snapshot_request(value));
            } else if (examples == 2) {
                CHECK(decode_command_request(value));
            }
            ++examples;
            offset = end + 4;
        }
        CHECK(examples == 4); // Fourth is the core's empty event batch, not request grammar.
    }
}

TEST_CASE(
    "wire checks externally captured accepted core request fixtures when supplied",
    "[engine_client_wire]") {
    const auto directory = std::getenv("BN_WIRE_FIXTURES");
    if (directory == nullptr) {
        return;
    } // Optional external corpus; built-in negatives run always.
    auto requests = 0;
    for (const auto folder : {"schema-valid", "schema-invalid"}) {
        const auto valid = std::string_view{folder} == "schema-valid";
        auto files = std::vector<std::filesystem::path>{};
        for (const auto& file :
             std::filesystem::directory_iterator{std::filesystem::path{directory} / folder}) {
            files.push_back(file.path());
        }
        std::ranges::sort(files);
        for (const auto& file : files) {
            const auto name = file.filename().string();
            CAPTURE(name);
            if (name.starts_with("command_request--")) {
                CHECK(decode_command_request(read_text(file)).has_value() == valid);
                ++requests;
            } else if (name.starts_with("application--normative-")) {
                const auto value = read_text(file);
                if (name.ends_with("-0.json")) {
                    CHECK(decode_negotiation_request(value));
                    ++requests;
                } else if (name.ends_with("-1.json")) {
                    CHECK(decode_snapshot_request(value));
                    ++requests;
                } else if (name.ends_with("-2.json")) {
                    CHECK(decode_command_request(value));
                    ++requests;
                }
            }
        }
    }
    CHECK(requests == 22); // Actual accepted capture: 6 operations, 7 negatives, 9 normative
                           // requests.
}
