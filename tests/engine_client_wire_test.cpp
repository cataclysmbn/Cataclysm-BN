#include "catch/catch.hpp"
#include "engine_client_wire.h"

#include <string>

namespace {
using namespace engine_client;
auto command(const std::string& operation, const std::string& schema = "\"schema:a\"")
    -> std::string {
    return R"({"epoch":"epoch:a","expect":{"revision":"7","boundary_id":"boundary:1","schema_id":)"
         + schema + R"(},"operation":)" + operation + "}";
}
auto contains(const std::string& text, const std::string& part) -> bool {
    return text.find(part) != std::string::npos;
}
} // namespace

TEST_CASE("hello request grammar", "[engine_client_wire]") {
    const auto ok = decode_hello_request(
        R"({"versions":["0.9","1.0"],"client":{"name":"deno","version":"1"}})");
    REQUIRE(ok);
    CHECK(ok->versions == std::vector<std::string>{"0.9", "1.0"});
    CHECK(ok->client_name == "deno");
    CHECK_FALSE(decode_hello_request(R"({"versions":[],"client":{"name":"a","version":"1"}})"));
    CHECK_FALSE(decode_hello_request(R"({"versions":["1.0"]})"));
    CHECK_FALSE(
        decode_hello_request(R"({"versions":["1.0"],"client":{"name":"a","version":"1"},"x":1})"));
    CHECK_FALSE(
        decode_hello_request(R"({"versions":["1.0","1.0"],"client":{"name":"a","version":"1"}})"));
    CHECK_FALSE(decode_hello_request(R"({"supported_versions":["1.0"]})"));
}

TEST_CASE("empty requests accept only an empty object", "[engine_client_wire]") {
    CHECK(decode_empty_request("{}"));
    CHECK(decode_empty_request(" { } "));
    CHECK_FALSE(decode_empty_request(R"({"page":{}})"));
    CHECK_FALSE(decode_empty_request("[]"));
}

TEST_CASE("choices request bounds", "[engine_client_wire]") {
    const auto window = [](const std::string& offset, const std::string& limit) {
        return R"({"epoch":"e","boundary_id":"b","offset":)" + offset + R"(,"limit":)" + limit
             + "}";
    };
    const auto ok = decode_choices_request(window("200", "200"));
    REQUIRE(ok);
    CHECK(ok->offset == 200);
    CHECK(ok->limit == 200);
    CHECK_FALSE(decode_choices_request(window("0", "201")));
    CHECK_FALSE(decode_choices_request(window("0", "0")));
    CHECK_FALSE(decode_choices_request(window("-1", "10")));
    CHECK_FALSE(decode_choices_request(R"({"epoch":"e","boundary_id":"b","offset":0})"));
}

TEST_CASE("command operations", "[engine_client_wire]") {
    SECTION("choose with a schema") {
        const auto decoded = decode_command_request(
            command(R"({"kind":"choose","choice_id":"c"})"));
        REQUIRE(decoded);
        CHECK(decoded->epoch == "epoch:a");
        CHECK(decoded->expect.revision == 7);
        CHECK(decoded->expect.schema_id == "schema:a");
        const auto* semantic = std::get_if<semantic_operation>(&decoded->operation);
        REQUIRE(semantic);
        CHECK(semantic->command.operation == game_client::interaction_operation::choose);
        CHECK(semantic->command.target_id == "c");
    }
    SECTION("action with a null schema") {
        const auto decoded = decode_command_request(
            command(R"({"kind":"action","action_id":"RIGHT"})", "null"));
        REQUIRE(decoded);
        CHECK_FALSE(decoded->expect.schema_id);
        REQUIRE(std::holds_alternative<registered_action>(decoded->operation));
        CHECK(std::get<registered_action>(decoded->operation).id == "RIGHT");
    }
    SECTION("absolute target with an optional candidate") {
        const auto decoded = decode_command_request(command(
            R"({"kind":"set_target","pos":{"dim":"","x":-9,"y":4,"z":1},"candidate_id":"t"})"));
        REQUIRE(decoded);
        const auto& semantic = std::get<semantic_operation>(decoded->operation);
        REQUIRE(semantic.target);
        CHECK(*semantic.target == position{.dim = "", .x = -9, .y = 4, .z = 1});
        CHECK(semantic.command.target_id == "t");
        CHECK(decode_command_request(
            command(R"({"kind":"set_target","pos":{"dim":"","x":1,"y":1,"z":0}})")));
    }
    SECTION("fill always states submit; count is a safe integer") {
        CHECK(decode_command_request(
            command(R"({"kind":"fill","field_id":"f","value":"x","submit":false})")));
        CHECK_FALSE(
            decode_command_request(command(R"({"kind":"fill","field_id":"f","value":"x"})")));
        CHECK(decode_command_request(command(R"({"kind":"set_count","choice_id":"c","count":3})")));
        CHECK_FALSE(decode_command_request(
            command(R"({"kind":"set_count","choice_id":"c","count":9007199254740992})")));
        CHECK(decode_command_request(command(R"({"kind":"cancel"})")));
    }
    SECTION("rejected shapes") {
        // bubble-frame target, mixed alternatives, removed names, wrong scalar types
        CHECK_FALSE(decode_command_request(command(
            R"({"kind":"set_target","pos":{"space":"reality_bubble_map_square","frame_id":"b","x":1,"y":1,"z":0}})")));
        CHECK_FALSE(
            decode_command_request(command(R"({"kind":"set_target","pos":{"x":1,"y":1,"z":0}})")));
        CHECK_FALSE(decode_command_request(
            command(R"({"kind":"choose","choice_id":"c","action_id":"a"})")));
        CHECK_FALSE(decode_command_request(
            command(R"({"kind":"invoke_registered_action","action_id":"a"})")));
        CHECK_FALSE(decode_command_request(command(R"({"kind":"choose","choice_id":""})")));
        CHECK_FALSE(decode_command_request(command(R"({"kind":"cancel"})", "\"\"")));
        CHECK_FALSE(decode_command_request(
            R"({"expect":{"revision":"7","boundary_id":"b","schema_id":null},"operation":{"kind":"cancel"}})"));
        CHECK_FALSE(decode_command_request(
            R"({"epoch":"e","expect":{"revision":7,"boundary_id":"b","schema_id":null},"operation":{"kind":"cancel"}})"));
        CHECK_FALSE(decode_command_request(
            R"({"epoch":"e","based_on":{"state_revision":"7","input_boundary_id":"b"},"operation":{"kind":"cancel"}})"));
    }
}

TEST_CASE("serialized values", "[engine_client_wire]") {
    const auto hello = serialize_hello("epoch:a", {.build = "0.9", .mods = {"bn"}});
    CHECK(contains(hello, R"("version":"1.0")"));
    CHECK(contains(hello, R"("mods":["bn"])"));
    CHECK(contains(hello, R"("cells_per_part":512)"));
    const auto encoded = serialize_receipt({.epoch = "epoch:a", .command_id = "c:1"});
    REQUIRE(encoded);
    CHECK(*encoded == R"({"epoch":"epoch:a","command_id":"c:1","stage":"received"})");
    CHECK_FALSE(serialize_receipt({.epoch = "", .command_id = "c:1"}));
    const auto failure = serialize_application_error(
        {.kind = error::stale_revision,
         .action = required_action::subscribe,
         .at = clock_point{"epoch:a", 4, 3}});
    REQUIRE(failure);
    CHECK(contains(*failure, R"("kind":"stale_revision")"));
    CHECK(contains(*failure, R"("action":"subscribe")"));
    CHECK(contains(*failure, R"("at":{"epoch":"epoch:a","sequence":"4","revision":"3"})"));
    const auto bare = serialize_application_error({.kind = error::command_busy});
    REQUIRE(bare);
    CHECK(*bare == R"({"kind":"command_busy"})");
}
