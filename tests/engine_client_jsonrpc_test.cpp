#include "catch/catch.hpp"
#include "engine_client_jsonrpc.h"

#include <array>
#include <expected>
#include <ios>
#include <limits>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <streambuf>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace rpc = engine_client::jsonrpc;

namespace {
/// Strict-policy view over the production inspection path; an empty batch is one invalid_request.
class request_cursor {
public:
    request_cursor() = default;
    request_cursor(rpc::envelope_cursor origin, const bool empty_batch_error)
        : origin_(std::move(origin)),
          empty_batch_error_(empty_batch_error) {}
    auto remaining() const -> std::size_t {
        return origin_.remaining() + (empty_batch_error_ ? 1u : 0u);
    }
    auto next() -> std::optional<rpc::request_entry> {
        if (std::exchange(empty_batch_error_, false)) { return rpc::error_code::invalid_request; }
        const auto input = origin_.next();
        if (!input) { return std::nullopt; }
        return rpc::apply_strict_policy(*input);
    }

private:
    rpc::envelope_cursor origin_;
    bool empty_batch_error_ = false;
};
struct parsed_frame {
    bool batch = false;
    request_cursor origin;
    auto size() const -> std::size_t { return origin.remaining(); }
    auto cursor() const -> request_cursor { return origin; }
};
auto parse_frame(std::string_view input) -> std::expected<parsed_frame, rpc::parse_error> {
    const auto inspected = rpc::inspect_frame(input);
    if (!inspected) { return std::unexpected(inspected.error()); }
    const auto empty_batch = inspected->batch && inspected->size() == 0;
    return parsed_frame{
        .batch = inspected->batch && !empty_batch,
        .origin = request_cursor(inspected->cursor(), empty_batch)};
}
auto first_request(const parsed_frame& frame) -> rpc::request_entry {
    auto cursor = frame.cursor();
    const auto entry = cursor.next();
    REQUIRE(entry);
    return *entry;
}
auto first_envelope(const rpc::inspected_frame& frame) -> rpc::envelope {
    auto cursor = frame.cursor();
    const auto entry = cursor.next();
    REQUIRE(entry);
    return *entry;
}
auto one_request(std::string_view text) -> rpc::request {
    const auto frame = parse_frame(text);
    REQUIRE(frame.has_value());
    REQUIRE_FALSE(frame->batch);
    REQUIRE(frame->size() == 1);
    REQUIRE(std::holds_alternative<rpc::request>(first_request(*frame)));
    return std::get<rpc::request>(first_request(*frame));
}
auto invalid_request(std::string_view text) -> void {
    const auto frame = parse_frame(text);
    REQUIRE(frame.has_value());
    REQUIRE(frame->size() == 1);
    REQUIRE(std::holds_alternative<rpc::error_code>(first_request(*frame)));
    CHECK(std::get<rpc::error_code>(first_request(*frame)) == rpc::error_code::invalid_request);
}
class failing_output: public std::streambuf {
public:
    std::size_t writes = 0;
    bool short_write = true;
    bool throws = false;

protected:
    auto xsputn(const char* /*bytes*/, std::streamsize count) -> std::streamsize override {
        ++writes;
        if (throws) { throw std::runtime_error("private device diagnostic"); }
        return short_write ? count - 1 : count;
    }
    auto sync() -> int override { return -1; }
};
class failing_input: public std::streambuf {
protected:
    auto underflow() -> int_type override { throw std::runtime_error("private device diagnostic"); }
};
} // namespace

TEST_CASE("jsonrpc_validates_complete_frame_before_exposing_requests", "[engine_client_jsonrpc]") {
    const auto good = std::string{
        R"({"jsonrpc":"2.0","method":"engine.submit","id":1,"params":{}})"};
    const auto suffixes = std::array<
        std::string_view,
        7>{"{}", "x", ",", std::string_view{"\0", 1}, "/*comment*/", "true", "\v"};
    for (const auto suffix : suffixes) {
        CAPTURE(suffix);
        const auto frame = parse_frame(good + std::string(suffix));
        REQUIRE_FALSE(frame);
        CHECK(frame.error() == rpc::parse_error::invalid_json);
    }
    for (auto length = std::size_t{0}; length < good.size(); ++length) {
        CAPTURE(length);
        const auto frame = parse_frame(std::string_view(good).substr(0, length));
        REQUIRE_FALSE(frame);
        CHECK(frame.error() == rpc::parse_error::invalid_json);
    }
    // Later syntax errors invalidate the WHOLE batch, even after apparently dispatchable members.
    for (const auto tail : {",]", ",{", ",falsee]", ",[1,]]"}) {
        REQUIRE_FALSE(parse_frame("[" + good + tail));
    }
    CHECK(parse_frame(" \t\r\n" + good + " \r\n"));
}

TEST_CASE("jsonrpc_classifies_envelopes_not_application_members", "[engine_client_jsonrpc]") {
    for (const auto text :
         {"null", "true", "0", "\"request\"", "{}", "[]", R"({"jsonrpc":"1.0","method":"ping"})",
          R"({"jsonrpc":2,"method":"ping"})", R"({"jsonrpc":"2.0","method":null})",
          R"({"jsonrpc":"2.0","result":{},"id":1})",
          R"({"jsonrpc":"2.0","method":"ping","id":true})",
          R"({"jsonrpc":"2.0","method":"ping","id":[]})",
          R"({"jsonrpc":"2.0","method":"ping","params":null})",
          R"({"jsonrpc":"2.0","method":"ping","params":42})",
          R"({"jsonrpc":"2.0","method":"ping","params":"bad"})"}) {
        CAPTURE(text);
        invalid_request(text);
    }
    const auto params = std::string{R"({"text":"\u0000😀","unknown":[100.0,{"a":2}]})"};
    const auto request = one_request(
        R"({"jsonrpc":"2.0","method":"engine.submit","params":)" + params
        + R"(,"envelope_only":true,"id":null})");
    REQUIRE(request.params);
    CHECK(*request.params == params);
    CHECK(*request.id.json == "null");
    const auto omitted = one_request(R"({"jsonrpc":"2.0","method":""})");
    CHECK(omitted.method.empty());
    CHECK_FALSE(omitted.id.json);
    CHECK_FALSE(omitted.params);
    CHECK(*one_request(R"({"jsonrpc":"2.0","method":"ping","params":[]})").params == "[]");
    CHECK(one_request(R"({"jsonrpc":"2.0","\u006dethod":"ping"})").method == "ping");
}

TEST_CASE("jsonrpc_echoes_ids_in_the_library_normal_form", "[engine_client_jsonrpc]") {
    // nlohmann/json is the parser: ids are echoed as it normalizes them, never invented.
    for (const auto id : {"0", "-7", "1.5", "9007199254740993", "\"text\"", "null"}) {
        const auto request = one_request(
            std::string{R"({"jsonrpc":"2.0","method":"ping","id":)"} + id + "}");
        REQUIRE(request.id.json);
        CHECK(*request.id.json == std::string{id});
        const auto result = rpc::make_result(request, "null");
        REQUIRE(result);
        REQUIRE(*result);
        CHECK((*result)->bytes()
              == R"({"jsonrpc":"2.0","id":)" + std::string{id} + R"(,"result":null})");
    }
    for (const auto id : {"+1", "01", "-01", "1.", ".1", "1e", "1e+", "NaN", "Infinity", "0x10"}) {
        const auto frame = parse_frame(
            std::string{R"({"jsonrpc":"2.0","method":"ping","id":)"} + id + "}");
        REQUIRE_FALSE(frame);
        CHECK(frame.error() == rpc::parse_error::invalid_json);
    }
}

TEST_CASE(
    "jsonrpc_unicode_is_strict_and_raw_params_and_ids_are_lossless", "[engine_client_jsonrpc]") {
    const auto method = std::string{"m\0", 2} + "😀한";
    const auto id = std::string{R"("\u0000😀한")"};
    const auto escaped_id = std::string{R"("\u0000\ud83d\ude00한")"};
    const auto request = one_request(
        R"({"jsonrpc":"2.0","method":"m\u0000\ud83d\ude00한","id":)" + escaped_id
        + R"(,"params":{"text":"\u0000\ud83d\ude00"}})");
    CHECK(request.method == method);
    CHECK(*request.id.json == id);
    const auto result = rpc::make_result(request, "\n{\"text\": \"😀한\\u0000\"}\r\n");
    REQUIRE(result);
    REQUIRE(*result);
    CHECK((*result)->bytes()
          == R"({"jsonrpc":"2.0","id":)" + id + R"(,"result":{"text":"😀한\u0000"}})");
    const auto bad = std::vector<std::string>{
        R"("\ud800")",
        R"("\udc00")",
        R"("\ud800\u0041")",
        R"("\uZZZZ")",
        R"("\x20")",
        std::string{"\"\0\"", 3},
        "\"\n\"",
        "\"\x80\"",
        "\"\xc0\xaf\"",
        "\"\xe0\x80\x80\"",
        "\"\xed\xa0\x80\"",
        "\"\xf0\x80\x80\x80\"",
        "\"\xf4\x90\x80\x80\"",
        "\"\xf5\x80\x80\x80\"",
        "\"\xc2\"",
        "\"\xe2\x82\"",
        "\"\xf0\x9f\x98\""};
    for (const auto& value : bad) {
        const auto prefix = std::string{R"({"jsonrpc":"2.0","method":"ping","id":1,)"};
        for (const auto key : {"extra", "params", "id"}) {
            const auto frame = parse_frame(prefix + "\"" + key + "\":" + value + "}");
            REQUIRE_FALSE(frame);
            CHECK(frame.error() == rpc::parse_error::invalid_json);
        }
    }
}

TEST_CASE(
    "jsonrpc_valid_batches_keep_members_and_notifications_in_order", "[engine_client_jsonrpc]") {
    const auto frame = parse_frame(R"([
        {"jsonrpc":"2.0","method":"sum","params":[1,2],"id":"a"},
        {"jsonrpc":"2.0","method":"notify","params":{}},
        42, [], {"jsonrpc":"2.0","method":"ping","id":null}
    ])");
    REQUIRE(frame);
    REQUIRE(frame->batch);
    REQUIRE(frame->size() == 5);
    auto output = rpc::response_frame(frame->batch);
    auto cursor = frame->cursor();
    while (const auto entry = cursor.next()) {
        if (const auto* request = std::get_if<rpc::request>(&*entry)) {
            const auto response = rpc::make_result(*request, "{}");
            REQUIRE(response);
            REQUIRE(output.append(*response));
        } else {
            const auto response = rpc::make_error(
                {.id = {.json = "null"}, .code = std::get<rpc::error_code>(*entry)});
            REQUIRE(response);
            REQUIRE(output.append(*response));
        }
    }
    const auto bytes = std::move(output).finish();
    REQUIRE(bytes);
    REQUIRE(*bytes);
    CHECK(
        **bytes
        == R"([{"jsonrpc":"2.0","id":"a","result":{}},{"jsonrpc":"2.0","id":null,"error":{"code":-32600,"message":"Invalid Request"}},{"jsonrpc":"2.0","id":null,"error":{"code":-32600,"message":"Invalid Request"}},{"jsonrpc":"2.0","id":null,"result":{}}])");
    const auto empty = parse_frame("[]");
    REQUIRE(empty);
    CHECK_FALSE(empty->batch);
    invalid_request("[]");
}

TEST_CASE(
    "jsonrpc_suppresses_notification_errors_and_all_notification_batches",
    "[engine_client_jsonrpc]") {
    const auto frame = parse_frame(
        R"([{"jsonrpc":"2.0","method":"engine.submit"},{"jsonrpc":"2.0","method":"unknown"}])");
    REQUIRE(frame);
    auto output = rpc::response_frame(frame->batch);
    auto cursor = frame->cursor();
    while (const auto entry = cursor.next()) {
        const auto& request = std::get<rpc::request>(*entry);
        const auto result = rpc::make_result(request, "deliberately invalid output");
        REQUIRE(result);
        CHECK_FALSE(*result);
        for (const auto code :
             {rpc::error_code::method_not_found, rpc::error_code::invalid_params,
              rpc::error_code::internal_error, rpc::error_code::application_error}) {
            const auto error = rpc::make_error({.id = request.id, .code = code});
            REQUIRE(error);
            CHECK_FALSE(*error);
            REQUIRE(output.append(*error));
        }
    }
    const auto bytes = std::move(output).finish();
    REQUIRE(bytes);
    CHECK_FALSE(*bytes);
    auto stream = std::ostringstream{};
    REQUIRE(rpc::write_frame(stream, *bytes));
    CHECK(stream.str().empty());
    // Framing NEVER executes methods: submission-notification no-execution is session policy.
    const auto explicit_null = one_request(R"({"jsonrpc":"2.0","method":"unknown","id":null})");
    const auto response = rpc::make_error(
        {.id = explicit_null.id, .code = rpc::error_code::method_not_found});
    REQUIRE(response);
    REQUIRE(*response);
}

TEST_CASE(
    "jsonrpc_transport_bound_does_not_replace_application_byte_limit", "[engine_client_jsonrpc]") {
    const auto prefix = std::string{
        R"({"jsonrpc":"2.0","method":"engine.submit","id":0,"params":{"value":")"};
    const auto suffix = std::string{R"("}})"};
    const auto exact =
        prefix + std::string(rpc::maximum_frame_bytes - prefix.size() - suffix.size(), 'x')
        + suffix;
    REQUIRE(exact.size() == rpc::maximum_frame_bytes);
    const auto request = one_request(exact);
    REQUIRE(request.params);
    CHECK(request.params->size() > 262144); // Subsequent typed application decoding must reject it.
    const auto over = parse_frame(exact + " ");
    REQUIRE_FALSE(over);
    CHECK(over.error() == rpc::parse_error::resource_limit);
    // Batch work/storage are bounded by input bytes, not by an invented member-count ceiling.
    auto batch = std::string{"[0"};
    for (auto count = 0; count < 100000; ++count) { batch += ",0"; }
    batch += "]";
    const auto large = parse_frame(batch);
    REQUIRE(large);
    CHECK(large->size() == 100001);
}

TEST_CASE(
    "jsonrpc_read_frame_requires_complete_newline_and_bounds_before_dispatch",
    "[engine_client_jsonrpc]") {
    const auto good = std::string{R"({"jsonrpc":"2.0","method":"ping","id":1})"};
    auto input = std::istringstream(good + "\n" + good + "\n");
    const auto first = rpc::read_frame(input);
    CHECK(first.status == rpc::read_status::complete);
    CHECK(first.bytes == good);
    CHECK(rpc::read_frame(input).bytes == good);
    CHECK(rpc::read_frame(input).status == rpc::read_status::eof);
    for (const auto& text : {good, std::string(rpc::maximum_frame_bytes, ' ')}) {
        auto partial = std::istringstream(text);
        const auto result = rpc::read_frame(partial);
        CHECK(result.status == rpc::read_status::partial_eof);
        CHECK(result.bytes.empty());
    }
    auto exact = std::istringstream(std::string(rpc::maximum_frame_bytes, ' ') + "\n");
    const auto bounded = rpc::read_frame(exact);
    CHECK(bounded.status == rpc::read_status::complete);
    CHECK(bounded.bytes.size() == rpc::maximum_frame_bytes);
    auto over = std::istringstream(std::string(rpc::maximum_frame_bytes + 1, ' ') + "\n");
    const auto rejected = rpc::read_frame(over);
    CHECK(rejected.status == rpc::read_status::too_large);
    CHECK(rejected.bytes.empty());
    auto bad = std::istringstream{};
    bad.setstate(std::ios::badbit);
    CHECK(rpc::read_frame(bad).status == rpc::read_status::io_error);
    auto blank = std::istringstream("\n");
    CHECK(rpc::read_frame(blank).status == rpc::read_status::complete);
    auto throwing_eof = std::istringstream{};
    throwing_eof.exceptions(std::ios::failbit | std::ios::badbit);
    CHECK(rpc::read_frame(throwing_eof).status == rpc::read_status::eof);
    auto device = failing_input{};
    auto device_input = std::istream(&device);
    device_input.exceptions(std::ios::badbit | std::ios::failbit);
    CHECK(rpc::read_frame(device_input).status == rpc::read_status::io_error);
}

TEST_CASE("jsonrpc_response_factories_are_safe_exact_and_bounded", "[engine_client_jsonrpc]") {
    const auto request = one_request(R"({"jsonrpc":"2.0","method":"ping","id":"\u0000😀"})");
    struct error_case {
        rpc::error_code code;
        std::string_view message;
    };
    const auto errors = std::array<error_case, 7>{
        {{.code = rpc::error_code::parse_error, .message = "Parse error"},
         {.code = rpc::error_code::invalid_request, .message = "Invalid Request"},
         {.code = rpc::error_code::method_not_found, .message = "Method not found"},
         {.code = rpc::error_code::invalid_params, .message = "Invalid params"},
         {.code = rpc::error_code::internal_error, .message = "Internal error"},
         {.code = rpc::error_code::not_initialized, .message = "Server is not initialized"},
         {.code = rpc::error_code::application_error, .message = "Engine contract error"}}};
    for (const auto& [code, message] : errors) {
        const auto response = rpc::make_error({.id = request.id, .code = code});
        REQUIRE(response);
        REQUIRE(*response);
        CHECK(
            (*response)->bytes()
            == R"({"jsonrpc":"2.0","id":"\u0000😀","error":{"code":)"
                   + std::to_string(static_cast<int>(code)) + R"(,"message":")"
                   + std::string(message) + R"("}})");
        CHECK((*response)->bytes().find("result") == std::string_view::npos);
    }
    const auto app = rpc::make_error(
        {.id = request.id,
         .code = rpc::error_code::application_error,
         .data = "{\n\"stage\": \"receipt\"}"});
    REQUIRE(app);
    REQUIRE(*app);
    CHECK((*app)->bytes().find(R"("data":{"stage":"receipt"})") != std::string_view::npos);
    CHECK_FALSE(rpc::make_error({.id = request.id, .code = static_cast<rpc::error_code>(123)}));
    CHECK_FALSE(
        rpc::make_error({.id = request.id, .code = rpc::error_code::internal_error, .data = "{}"}));
    CHECK_FALSE(rpc::make_error(
        {.id = request.id, .code = rpc::error_code::application_error, .data = "{}x"}));
    for (const auto value : {"{}x", "", "[1,]", "\"\\ud800\""}) {
        CHECK_FALSE(rpc::make_result(request, value));
    }
    for (const auto id : {"[]", "{}", "true", "1x", "", "\"\\ud800\""}) {
        auto broken = request;
        broken.id.json = id;
        CHECK_FALSE(rpc::make_result(broken, "{}"));
        CHECK_FALSE(rpc::make_error({.id = broken.id}));
    }
    const auto tiny = rpc::make_result(request, "\"\"");
    REQUIRE(tiny);
    REQUIRE(*tiny);
    const auto padding = rpc::maximum_frame_bytes - (*tiny)->bytes().size();
    const auto exact = rpc::make_result(request, "\"" + std::string(padding, 'x') + "\"");
    REQUIRE(exact);
    REQUIRE(*exact);
    CHECK((*exact)->bytes().size() == rpc::maximum_frame_bytes);
    const auto over = rpc::make_result(request, "\"" + std::string(padding + 1, 'x') + "\"");
    REQUIRE_FALSE(over);
    CHECK(over.error() == rpc::output_error::resource_limit);
    const auto huge = std::string(rpc::maximum_frame_bytes + 1, 'x');
    CHECK(rpc::make_result(request, huge).error() == rpc::output_error::resource_limit);
    CHECK(
        rpc::make_error(
            {.id = request.id, .code = rpc::error_code::application_error, .data = huge})
            .error()
        == rpc::output_error::resource_limit);
}

TEST_CASE(
    "jsonrpc_batch_assembly_failure_never_returns_successful_prefix", "[engine_client_jsonrpc]") {
    const auto request = one_request(R"({"jsonrpc":"2.0","method":"ping","id":1})");
    const auto tiny = rpc::make_result(request, "\"\"");
    REQUIRE(tiny);
    REQUIRE(*tiny);
    const auto exact = rpc::make_result(
        request,
        "\"" + std::string(rpc::maximum_frame_bytes - (*tiny)->bytes().size() - 2, 'x') + "\"");
    REQUIRE(exact);
    auto batch = rpc::response_frame(true);
    REQUIRE(batch.append(*exact));
    const auto bytes = std::move(batch).finish();
    REQUIRE(bytes);
    REQUIRE(*bytes);
    CHECK((**bytes).size() == rpc::maximum_frame_bytes);
    auto overflow = rpc::response_frame(true);
    REQUIRE(overflow.append(*exact));
    const auto failure = overflow.append(*tiny);
    REQUIRE_FALSE(failure);
    CHECK(failure.error() == rpc::output_error::resource_limit);
    CHECK_FALSE(overflow.append(std::nullopt));
    CHECK(std::move(overflow).finish().error() == rpc::output_error::resource_limit);
    auto single = rpc::response_frame(false);
    REQUIRE(single.append(*tiny));
    CHECK_FALSE(single.append(*tiny));
    CHECK(std::move(single).finish().error() == rpc::output_error::invalid_value);
    CHECK(std::move(batch).finish().error() == rpc::output_error::invalid_value);
    CHECK_FALSE(batch.append(*tiny));
}

TEST_CASE("jsonrpc_batch_reserves_closing_bracket_for_every_response", "[engine_client_jsonrpc]") {
    const auto request = one_request(R"({"jsonrpc":"2.0","method":"ping","id":1})");
    const auto tiny = rpc::make_result(request, "\"\"");
    REQUIRE(tiny);
    REQUIRE(*tiny);
    const auto tiny_size = (*tiny)->bytes().size();
    const auto padded = [&](const size_t size) {
        return rpc::make_result(request, "\"" + std::string(size - tiny_size, 'x') + "\"");
    };
    // "[" + first + "," + second + "]" must fit: first + second == maximum - 3 is the largest.
    const auto first = padded(rpc::maximum_frame_bytes / 2);
    const auto fits = padded(rpc::maximum_frame_bytes - 3 - rpc::maximum_frame_bytes / 2);
    const auto over = padded(rpc::maximum_frame_bytes - 2 - rpc::maximum_frame_bytes / 2);
    REQUIRE(first);
    REQUIRE(fits);
    REQUIRE(over);
    auto batch = rpc::response_frame(true);
    REQUIRE(batch.append(*first));
    REQUIRE(batch.append(*fits));
    const auto bytes = std::move(batch).finish();
    REQUIRE(bytes);
    REQUIRE(*bytes);
    CHECK((**bytes).size() == rpc::maximum_frame_bytes);
    auto overflow = rpc::response_frame(true);
    REQUIRE(overflow.append(*first));
    const auto failure = overflow.append(*over);
    REQUIRE_FALSE(failure);
    CHECK(failure.error() == rpc::output_error::resource_limit);
}

TEST_CASE(
    "jsonrpc_output_checks_all_bytes_before_write_and_reports_transport_loss",
    "[engine_client_jsonrpc]") {
    auto output = std::ostringstream{};
    REQUIRE(rpc::write_frame(output, "{\n\"result\": [1, 2], \"id\":\"a\\n b\"\n}"));
    CHECK(output.str() == "{\"result\":[1,2],\"id\":\"a\\n b\"}\n");
    const auto before = output.str();
    CHECK_FALSE(rpc::write_frame(output, "{} garbage"));
    CHECK(output.str() == before);
    CHECK(rpc::write_frame(output, std::string(rpc::maximum_frame_bytes + 1, ' ')).error()
          == rpc::output_error::resource_limit);
    CHECK(output.str() == before);
    auto exact_output = std::ostringstream{};
    const auto exact = std::string{"\""} + std::string(rpc::maximum_frame_bytes - 2, 'x') + "\"";
    REQUIRE(rpc::write_frame(exact_output, exact));
    CHECK(exact_output.str().size() == rpc::maximum_frame_bytes + 1);
    CHECK(exact_output.str().back() == '\n');
    auto buffer = failing_output{};
    auto failing = std::ostream(&buffer);
    CHECK(rpc::write_frame(failing, "{}").error() == rpc::output_error::io_error);
    CHECK(buffer.writes == 1);
    failing.clear();
    failing.exceptions(std::ios::badbit | std::ios::failbit);
    CHECK(rpc::write_frame(failing, "{}").error() == rpc::output_error::io_error);
    auto flush_buffer = failing_output{};
    flush_buffer.short_write = false;
    auto flush_failing = std::ostream(&flush_buffer);
    CHECK(rpc::write_frame(flush_failing, "{}").error() == rpc::output_error::io_error);
    auto throwing_buffer = failing_output{};
    throwing_buffer.throws = true;
    auto throwing_output = std::ostream(&throwing_buffer);
    throwing_output.exceptions(std::ios::badbit | std::ios::failbit);
    CHECK(rpc::write_frame(throwing_output, "{}").error() == rpc::output_error::io_error);
}

TEST_CASE(
    "jsonrpc_assembler_ownership_transfer_preserves_destination_and_poisoned_source",
    "[engine_client_jsonrpc]") {
    const auto request = one_request(R"({"jsonrpc":"2.0","method":"ping","id":1})");
    const auto response = rpc::make_result(request, "{}");
    REQUIRE(response);
    REQUIRE(*response);
    const auto raw = std::string((*response)->bytes());
    const auto reject_source = [&](rpc::response_frame& source) {
        CHECK(source.append(*response).error() == rpc::output_error::invalid_value);
        CHECK(source.append(std::nullopt).error() == rpc::output_error::invalid_value);
        CHECK(std::move(source).finish().error() == rpc::output_error::invalid_value);
    };
    for (const auto batch : {false, true}) {
        auto source = rpc::response_frame(batch);
        REQUIRE(source.append(*response));
        auto destination = rpc::response_frame(std::move(source));
        reject_source(source);
        // Batch punctuation and count transfer along with the buffer, not just final bytes.
        if (batch) { REQUIRE(destination.append(*response)); }
        const auto complete = std::move(destination).finish();
        REQUIRE(complete);
        REQUIRE(*complete);
        CHECK(**complete == (batch ? "[" + raw + "," + raw + "]" : raw));

        auto assigned_source = rpc::response_frame(batch);
        REQUIRE(assigned_source.append(*response));
        auto assigned_destination = rpc::response_frame(false);
        REQUIRE(assigned_destination.append(*response));
        REQUIRE_FALSE(assigned_destination.append(*response)); // Old destination state is
                                                               // overwritten.
        assigned_destination = std::move(assigned_source);
        reject_source(assigned_source);
        const auto assigned = std::move(assigned_destination).finish();
        REQUIRE(assigned);
        REQUIRE(*assigned);
        CHECK(**assigned == (batch ? "[" + raw + "]" : raw));

        auto empty_source = rpc::response_frame(batch);
        auto empty_destination = rpc::response_frame(std::move(empty_source));
        reject_source(empty_source);
        // Even an empty moved-from source is not reusable. Destination retains batch identity.
        REQUIRE(empty_destination.append(*response));
        const auto initially_empty = std::move(empty_destination).finish();
        REQUIRE(initially_empty);
        REQUIRE(*initially_empty);
        CHECK(**initially_empty == (batch ? "[" + raw + "]" : raw));

        auto empty_assigned_source = rpc::response_frame(batch);
        auto empty_assigned_destination = rpc::response_frame(!batch);
        REQUIRE(empty_assigned_destination.append(*response));
        empty_assigned_destination = std::move(empty_assigned_source);
        reject_source(empty_assigned_source);
        const auto no_responses = std::move(empty_assigned_destination).finish();
        REQUIRE(no_responses);
        CHECK_FALSE(*no_responses);
    }
    auto failed_source = rpc::response_frame(false);
    REQUIRE(failed_source.append(*response));
    REQUIRE_FALSE(failed_source.append(*response));
    auto failed_destination = rpc::response_frame(std::move(failed_source));
    reject_source(failed_source);
    CHECK(std::move(failed_destination).finish().error() == rpc::output_error::invalid_value);
    auto consumed_source = rpc::response_frame(true);
    REQUIRE(consumed_source.append(*response));
    REQUIRE(std::move(consumed_source).finish());
    auto consumed_destination = rpc::response_frame(false);
    consumed_destination = std::move(consumed_source);
    reject_source(consumed_source);
    CHECK(std::move(consumed_destination).finish().error() == rpc::output_error::invalid_value);

    auto original = rpc::response_frame(true);
    REQUIRE(original.append(*response));
    auto copied = original;
    auto copy_assigned = rpc::response_frame(false);
    copy_assigned = original;
    const auto check_copy = [&](rpc::response_frame& frame) {
        const auto bytes = std::move(frame).finish();
        REQUIRE(bytes);
        REQUIRE(*bytes);
        CHECK(**bytes == "[" + raw + "]");
    };
    check_copy(copied);
    check_copy(copy_assigned);
    auto& alias = original;
    original = std::move(alias); // Self move preserves its single valid destination.
    check_copy(original);
}

TEST_CASE(
    "jsonrpc_strict_policy_wrapper_and_legacy_id_rules_stay_separate", "[engine_client_jsonrpc]") {
    for (const auto id :
         {"1.5", "1.00000000000000001", "1e0", "-1E+2", "9223372036854775808",
          "-9223372036854775809", "18446744073709551615", "true", "{}", "[]"}) {
        CAPTURE(id);
        const auto text = std::string{R"({"jsonrpc":"2.0","method":"ping","id":)"} + id + "}";
        const auto frame = rpc::inspect_frame(text);
        REQUIRE(frame);
        const auto policy = rpc::apply_legacy_policy(first_envelope(*frame));
        REQUIRE(std::holds_alternative<rpc::legacy_error>(policy));
        const auto& error = std::get<rpc::legacy_error>(policy);
        CHECK(*error.id.json == "null");
        CHECK(error.reason == rpc::legacy_reason::invalid_request);
        const auto response = rpc::make_legacy_error(error);
        REQUIRE(response);
        REQUIRE(*response);
        CHECK(
            (*response)->bytes()
            == R"({"jsonrpc":"2.0","id":null,"error":{"code":-32600,"message":"Invalid request"}})");
        const auto strict = parse_frame(text);
        REQUIRE(strict);
        const auto strict_entry = rpc::apply_strict_policy(first_envelope(*frame));
        CHECK(first_request(*strict).index() == strict_entry.index());
        if (first_envelope(*frame).at("id").is_number()) {
            REQUIRE(std::holds_alternative<rpc::request>(strict_entry));
        } else {
            CHECK(std::holds_alternative<rpc::error_code>(strict_entry));
        }
    }
    for (const auto id :
         {"-9223372036854775808", "9223372036854775807", "9007199254740993", "0", "null",
          "\"string-id\"", R"("\u0000😀")"}) {
        const auto frame = rpc::inspect_frame(
            std::string{R"({"jsonrpc":"2.0","method":"ping","id":)"} + id + "}");
        REQUIRE(frame);
        const auto policy = rpc::apply_legacy_policy(first_envelope(*frame));
        REQUIRE(std::holds_alternative<rpc::request>(policy));
        CHECK(*std::get<rpc::request>(policy).id.json == id);
    }
    for (const auto params : {"null", "42", "true", "\"wrong-type\"", "{}", "[]"}) {
        const auto frame = rpc::inspect_frame(
            std::string{R"({"jsonrpc":"2.0","method":"unknown/method","params":)"} + params + "}");
        REQUIRE(frame);
        const auto policy = rpc::apply_legacy_policy(first_envelope(*frame));
        REQUIRE(std::holds_alternative<rpc::request>(policy));
        const auto& notification = std::get<rpc::request>(policy);
        CHECK_FALSE(notification.id.json);
        CHECK(*notification.params == params);
        const auto error = rpc::make_legacy_error(
            {.id = notification.id, .reason = rpc::legacy_reason::invalid_parameters});
        REQUIRE(error);
        CHECK_FALSE(*error);
        const auto strict = rpc::apply_strict_policy(first_envelope(*frame));
        CHECK(std::holds_alternative<rpc::request>(strict)
              == (first_envelope(*frame).at("params").is_object()
                  || first_envelope(*frame).at("params").is_array()));
    }
}

TEST_CASE(
    "jsonrpc_legacy_envelope_errors_keep_historical_phase_id_and_closed_reason",
    "[engine_client_jsonrpc]") {
    struct envelope_case {
        std::string_view json;
        std::string_view id;
        rpc::legacy_reason reason;
    };
    const auto cases = std::array<envelope_case, 12>{
        {{.json = "[]", .id = "null", .reason = rpc::legacy_reason::parse_error},
         {.json = "7", .id = "null", .reason = rpc::legacy_reason::parse_error},
         {.json = R"({"method":"ping"})",
          .id = "null",
          .reason = rpc::legacy_reason::invalid_jsonrpc_request},
         {.json = R"({"method":"ping","id":1})",
          .id = "1",
          .reason = rpc::legacy_reason::invalid_jsonrpc_version},
         {.json = R"({"jsonrpc":"1.0","method":"ping","id":null})",
          .id = "null",
          .reason = rpc::legacy_reason::invalid_jsonrpc_version},
         {.json = R"({"jsonrpc":null,"method":"ping"})",
          .id = "null",
          .reason = rpc::legacy_reason::invalid_request},
         {.json = R"({"jsonrpc":2,"method":"ping","id":1})",
          .id = "1",
          .reason = rpc::legacy_reason::invalid_parameters},
         {.json = R"({"jsonrpc":"2.0"})",
          .id = "null",
          .reason = rpc::legacy_reason::method_required},
         {.json = R"({"jsonrpc":"2.0","id":1})",
          .id = "1",
          .reason = rpc::legacy_reason::method_required},
         {.json = R"({"jsonrpc":"2.0","method":null})",
          .id = "null",
          .reason = rpc::legacy_reason::invalid_request},
         {.json = R"({"jsonrpc":"2.0","method":null,"id":null})",
          .id = "null",
          .reason = rpc::legacy_reason::invalid_parameters},
         {.json = R"({"jsonrpc":"2.0","method":42,"id":"a"})",
          .id = R"("a")",
          .reason = rpc::legacy_reason::invalid_parameters}}};
    for (const auto& entry : cases) {
        CAPTURE(entry.json);
        // Wrap arrays as batch members so inspection retains their non-object type.
        const auto frame = rpc::inspect_frame("[" + std::string(entry.json) + "]");
        REQUIRE(frame);
        REQUIRE(frame->size() == 1);
        const auto policy = rpc::apply_legacy_policy(first_envelope(*frame));
        REQUIRE(std::holds_alternative<rpc::legacy_error>(policy));
        const auto& error = std::get<rpc::legacy_error>(policy);
        CHECK(*error.id.json == entry.id);
        CHECK(error.reason == entry.reason);
    }
}

TEST_CASE(
    "jsonrpc_trusted_legacy_error_table_has_no_arbitrary_diagnostic_seam",
    "[engine_client_jsonrpc]") {
    struct message_case {
        rpc::legacy_reason reason;
        rpc::error_code code;
        std::string_view message;
    };
    const auto cases = std::array<message_case, 13>{
        {{.reason = rpc::legacy_reason::parse_error,
          .code = rpc::error_code::parse_error,
          .message = "Parse error"},
         {.reason = rpc::legacy_reason::invalid_request,
          .code = rpc::error_code::invalid_request,
          .message = "Invalid request"},
         {.reason = rpc::legacy_reason::invalid_parameters,
          .code = rpc::error_code::invalid_params,
          .message = "Invalid parameters"},
         {.reason = rpc::legacy_reason::invalid_jsonrpc_version,
          .code = rpc::error_code::invalid_request,
          .message = "Invalid JSON-RPC version"},
         {.reason = rpc::legacy_reason::invalid_jsonrpc_request,
          .code = rpc::error_code::invalid_request,
          .message = "Invalid JSON-RPC request"},
         {.reason = rpc::legacy_reason::method_required,
          .code = rpc::error_code::invalid_request,
          .message = "JSON-RPC method is required"},
         {.reason = rpc::legacy_reason::not_initialized,
          .code = rpc::error_code::not_initialized,
          .message = "Server is not initialized"},
         {.reason = rpc::legacy_reason::unknown_resource,
          .code = rpc::error_code::invalid_params,
          .message = "Unknown resource"},
         {.reason = rpc::legacy_reason::structured_state_unavailable,
          .code = rpc::error_code::internal_error,
          .message = "Structured state is unavailable"},
         {.reason = rpc::legacy_reason::subscriptions_not_supported,
          .code = rpc::error_code::method_not_found,
          .message = "Resource subscriptions are not supported"},
         {.reason = rpc::legacy_reason::structured_interaction_unavailable,
          .code = rpc::error_code::internal_error,
          .message = "Structured interaction is unavailable"},
         {.reason = rpc::legacy_reason::unknown_tool,
          .code = rpc::error_code::invalid_params,
          .message = "Unknown tool"},
         {.reason = rpc::legacy_reason::method_not_found,
          .code = rpc::error_code::method_not_found,
          .message = "Method not found"}}};
    const auto id = rpc::request_id{.json = R"("\u0000😀")"};
    for (const auto& entry : cases) {
        const auto response = rpc::make_legacy_error({.id = id, .reason = entry.reason});
        REQUIRE(response);
        REQUIRE(*response);
        CHECK(
            (*response)->bytes()
            == R"({"jsonrpc":"2.0","id":"\u0000😀","error":{"code":)"
                   + std::to_string(static_cast<int>(entry.code)) + R"(,"message":")"
                   + std::string(entry.message) + R"("}})");
        const auto notification = rpc::make_legacy_error({.reason = entry.reason});
        REQUIRE(notification);
        CHECK_FALSE(*notification);
    }
    CHECK(rpc::make_legacy_error({.id = id, .reason = static_cast<rpc::legacy_reason>(999)}).error()
          == rpc::output_error::invalid_value);
    CHECK(rpc::make_legacy_error({.id = {.json = "true"}}).error()
          == rpc::output_error::invalid_value);
    CHECK(rpc::make_legacy_error({.id = {.json = std::string(rpc::maximum_frame_bytes + 1, '0')}})
              .error()
          == rpc::output_error::resource_limit);
}

TEST_CASE(
    "jsonrpc_configurable_read_limit_cannot_relax_absolute_transport_cap",
    "[engine_client_jsonrpc]") {
    auto exact = std::istringstream("123\nnext\n");
    const auto accepted = rpc::read_frame(exact, 3);
    CHECK(accepted.status == rpc::read_status::complete);
    CHECK(accepted.bytes == "123");
    CHECK(exact.peek() == 'n');
    auto over = std::istringstream("1234\nnext\n");
    const auto rejected = rpc::read_frame(over, 3);
    CHECK(rejected.status == rpc::read_status::too_large);
    CHECK(rejected.bytes.empty());
    // Only the one byte needed to detect overflow is consumed; do not drain another frame.
    CHECK(over.peek() == '\n');
    auto partial = std::istringstream("123");
    CHECK(rpc::read_frame(partial, 3).status == rpc::read_status::partial_eof);
    auto zero = std::istringstream("x\n");
    CHECK(rpc::read_frame(zero, 0).status == rpc::read_status::too_large);
    auto blank = std::istringstream("\n");
    CHECK(rpc::read_frame(blank, 0).status == rpc::read_status::complete);
    auto absolute_exact = std::istringstream(std::string(rpc::maximum_frame_bytes, ' ') + "\n");
    const auto bounded = rpc::read_frame(absolute_exact, std::numeric_limits<std::size_t>::max());
    CHECK(bounded.status == rpc::read_status::complete);
    CHECK(bounded.bytes.size() == rpc::maximum_frame_bytes);
    auto absolute_over = std::istringstream(std::string(rpc::maximum_frame_bytes + 1, ' ') + "\n");
    const auto still_rejected =
        rpc::read_frame(absolute_over, std::numeric_limits<std::size_t>::max());
    CHECK(still_rejected.status == rpc::read_status::too_large);
    CHECK(still_rejected.bytes.empty());
}

TEST_CASE(
    "jsonrpc_owned_cursors_and_escaped_requests_survive_all_source_lifetimes",
    "[engine_client_jsonrpc]") {
    const auto text = std::string{
        R"([{"jsonrpc":"2.0","method":"bn.\u0063ommand.submit","id":"\u0000\ud83d\ude00","params":{ "value":"\u0000😀", "n":1.00000000000000001 }},{"jsonrpc":"2.0","method":"unknown/method","params":null},0])"};
    auto cursor = [&]() {
        auto caller_input = text;
        const auto frame = rpc::inspect_frame(caller_input);
        REQUIRE(frame);
        REQUIRE(frame->size() == 3);
        auto pinned = frame->cursor();
        caller_input.assign(caller_input.size(), 'x'); // It is not the frame's immutable storage.
        return pinned;
    }(); // Caller string AND frame are gone; only a cursor pins the immutable input.
    auto independent = cursor;
    auto moved = std::move(cursor);
    CHECK(cursor.remaining() == 0);
    CHECK_FALSE(cursor.next());
    CHECK(moved.remaining() == 3);
    const auto first = moved.next();
    REQUIRE(first);
    CHECK(first->at("method") == "bn.command.submit");
    CHECK(first->at("id") == std::string{"\0", 1} + "😀");
    const auto entry = rpc::apply_strict_policy(*first);
    REQUIRE(std::holds_alternative<rpc::request>(entry));
    const auto escaped = std::get<rpc::request>(entry);
    CHECK(*escaped.params == R"({"n":1.0,"value":"\u0000😀"})");
    REQUIRE(independent.next());
    CHECK(independent.remaining() == 2);
    const auto second = moved.next();
    REQUIRE(second);
    CHECK_FALSE(second->contains("id"));
    CHECK(second->at("params").is_null());
    const auto legacy = rpc::apply_legacy_policy(*second);
    REQUIRE(std::holds_alternative<rpc::request>(legacy));
    CHECK_FALSE(std::get<rpc::request>(legacy).id.json);
    REQUIRE(moved.next());
    CHECK_FALSE(moved.next());
    CHECK(moved.remaining() == 0);
    moved = rpc::envelope_cursor{};
    independent = rpc::envelope_cursor{};                  // All shared input owners are now gone.
    CHECK(first->at("id") == std::string{"\0", 1} + "😀"); // Inspection is also owned.
    const auto response = rpc::make_result(escaped, "{}");
    REQUIRE(response);
    REQUIRE(*response);
    CHECK((*response)->bytes() == R"({"jsonrpc":"2.0","id":"\u0000😀","result":{}})");

    auto strict = []() {
        const auto frame = parse_frame(
            R"([{"jsonrpc":"2.0","method":"ping","id":1.50e+01,"params":[]},false])");
        REQUIRE(frame);
        return frame->cursor();
    }();
    auto copied_strict = strict;
    const auto owned = strict.next();
    REQUIRE(owned);
    REQUIRE(std::holds_alternative<rpc::request>(*owned));
    const auto owned_request = std::get<rpc::request>(*owned);
    strict = request_cursor{};
    copied_strict = request_cursor{};
    CHECK(*owned_request.id.json == "15.0");
    CHECK(*owned_request.params == "[]");
    CHECK(owned_request.method == "ping");
}

TEST_CASE(
    "jsonrpc_maximum_scalar_batch_is_streamed_without_member_or_metadata_cap",
    "[engine_client_jsonrpc]") {
    const auto count = (rpc::maximum_frame_bytes - 1) / 2;
    auto text = std::string(rpc::maximum_frame_bytes, ' ');
    text[0] = '[';
    for (auto index = std::size_t{0}; index < count; ++index) {
        text[2 * index + 1] = '0';
        text[2 * index + 2] = index + 1 == count ? ']' : ',';
    }
    REQUIRE(text.size() == rpc::maximum_frame_bytes);
    auto inspected = rpc::inspect_frame(text);
    auto parsed = parse_frame(text);
    REQUIRE(inspected);
    REQUIRE(parsed);
    CHECK(inspected->batch);
    CHECK(parsed->batch);
    CHECK(inspected->size() == count);
    CHECK(parsed->size() == count);
    auto input = inspected->cursor();
    auto strict = parsed->cursor();
    auto inspected_count = std::size_t{0};
    auto strict_count = std::size_t{0};
    auto all_scalars = true;
    auto all_invalid = true;
    while (const auto envelope = input.next()) {
        all_scalars &= envelope->is_number();
        ++inspected_count;
    }
    while (const auto entry = strict.next()) {
        const auto* error = std::get_if<rpc::error_code>(&*entry);
        all_invalid &= error && *error == rpc::error_code::invalid_request;
        ++strict_count;
    }
    CHECK(all_scalars);
    CHECK(all_invalid);
    CHECK(inspected_count == count);
    CHECK(strict_count == count);
    CHECK(input.remaining() == 0);
    CHECK(strict.remaining() == 0);
    // A malformed final byte still exposes NO syntax-validated prefix/cursor.
    text[text.size() - 2] = ',';
    CHECK_FALSE(rpc::inspect_frame(text));
    CHECK_FALSE(parse_frame(text));
    text.back() = 'x';
    CHECK_FALSE(rpc::inspect_frame(text));
}


TEST_CASE("jsonrpc_rejects_nesting_beyond_the_bound_without_recursion", "[engine_client_jsonrpc]") {
    const auto nested = [](const std::size_t depth) {
        return std::string(depth, '[') + std::string(depth, ']');
    };
    CHECK(rpc::inspect_frame(nested(60)));
    const auto deep = rpc::inspect_frame(nested(100000));
    REQUIRE_FALSE(deep);
    CHECK(deep.error() == rpc::parse_error::invalid_json);
}

TEST_CASE(
    "jsonrpc_rejects_raw_nul_bytes_instead_of_truncating_the_frame", "[engine_client_jsonrpc]") {
    // The library treats NUL as end of input; a hidden suffix must never be accepted.
    const auto good = std::string{R"({"jsonrpc":"2.0","method":"ping","id":1})"};
    for (const auto tail : {std::string{"\0", 1}, std::string{"\0garbage", 8}}) {
        const auto frame = parse_frame(good + tail);
        REQUIRE_FALSE(frame);
        CHECK(frame.error() == rpc::parse_error::invalid_json);
    }
}
