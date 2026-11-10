#include "catch/catch.hpp"
#include "engine_client_wire.h"

#include <nlohmann/json.hpp>
#include <string>

namespace {
using namespace engine_client;
auto command(const std::string& operation) -> std::string {
    return R"({"epoch":"epoch:a","expect":{"revision":"7","boundary_id":"boundary:1"},"operation":)"
         + operation + "}";
}
auto contains(const std::string& text, const std::string& part) -> bool {
    return text.find(part) != std::string::npos;
}
} // namespace

namespace {
const auto hash_a = std::string{"sha256:0123456789abcdef"};
auto hello_02(const std::string& members) -> std::string {
    return R"({"versions":["0.2"],"client":{"name":"a","version":"1"})" + members + "}";
}
} // namespace

TEST_CASE("hello request grammar", "[engine_client_wire]") {
    const auto client = std::string{R"("client":{"name":"deno","version":"1"})"};
    const auto ok = decode_hello_request(R"({"versions":["0.9","0.2"],)" + client + "}");
    REQUIRE(ok);
    CHECK(ok->versions == std::vector<std::string>{"0.9", "0.2"});
    CHECK(ok->client_name == "deno");
    CHECK_FALSE(decode_hello_request(R"({"versions":[],)" + client + "}"));
    CHECK_FALSE(decode_hello_request(R"({"versions":["0.2"]})"));
    CHECK_FALSE(decode_hello_request(hello_02(R"(,"x":1)")));
    CHECK_FALSE(decode_hello_request(R"({"versions":["0.2","0.2"],)" + client + "}"));
    CHECK_FALSE(decode_hello_request(R"({"supported_versions":["0.2"]})"));
    // The tints of the client's tileset are ids; anything else is refused.
    const auto tints = decode_hello_request(hello_02(R"(,"tint_ids":["hair_black","jeans"])"));
    REQUIRE(tints);
    CHECK(tints->tint_ids == std::vector<std::string>{"hair_black", "jeans"});
    CHECK_FALSE(decode_hello_request(hello_02(R"(,"tint_ids":[1])")));
    CHECK_FALSE(decode_hello_request(hello_02(R"(,"tint_ids":[""])")));
    CHECK_FALSE(decode_hello_request(hello_02(R"(,"tint_ids":"jeans")")));
}

TEST_CASE("empty requests accept only an empty object", "[engine_client_wire]") {
    CHECK(decode_empty_request("{}"));
    // JSON-RPC lets a request without parameters leave `params` out.
    CHECK(decode_empty_request(""));
    CHECK(decode_empty_request(" { } "));
    CHECK_FALSE(decode_empty_request(R"({"page":{}})"));
    CHECK_FALSE(decode_empty_request("[]"));
}

TEST_CASE("choices request bounds", "[engine_client_wire]") {
    const auto window = [](const std::string& offset, const std::string& limit) {
        return R"({"epoch":"e","boundary_id":"b","component_id":"list:choices","offset":)" + offset
             + R"(,"limit":)" + limit + "}";
    };
    const auto ok = decode_choices_request(window("200", "200"));
    REQUIRE(ok);
    CHECK(ok->offset == 200);
    CHECK(ok->limit == 200);
    CHECK_FALSE(decode_choices_request(window("0", "201")));
    CHECK_FALSE(decode_choices_request(window("0", "0")));
    CHECK_FALSE(decode_choices_request(window("-1", "10")));
    CHECK_FALSE(decode_choices_request(
        R"({"epoch":"e","boundary_id":"b","component_id":"list:choices","offset":0})"));
}

TEST_CASE("command operations", "[engine_client_wire]") {
    SECTION("choose expects the boundary") {
        const auto decoded = decode_command_request(
            command(R"({"kind":"choose","choice_id":"c"})"));
        REQUIRE(decoded);
        CHECK(decoded->epoch == "epoch:a");
        CHECK(decoded->expect.revision == 7);
        CHECK(decoded->expect.boundary_id == "boundary:1");
        const auto* semantic = std::get_if<semantic_operation>(&decoded->operation);
        REQUIRE(semantic);
        CHECK(semantic->command.operation == game_client::interaction_operation::choose);
        CHECK(semantic->command.target_id == "c");
    }
    SECTION("action") {
        const auto decoded = decode_command_request(
            command(R"({"kind":"action","action_id":"RIGHT"})"));
        REQUIRE(decoded);
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
    SECTION("travel click on an absolute square") {
        const auto decoded = decode_command_request(
            command(R"({"kind":"travel","pos":{"dim":"","x":-9,"y":4,"z":1}})"));
        REQUIRE(decoded);
        const auto* travel = std::get_if<travel_operation>(&decoded->operation);
        REQUIRE(travel);
        CHECK(travel->target == position{.dim = "", .x = -9, .y = 4, .z = 1});
        CHECK_FALSE(decode_command_request(command(R"({"kind":"travel"})")));
        CHECK_FALSE(decode_command_request(
            command(R"({"kind":"travel","pos":{"dim":"","x":1,"y":1,"z":0},"candidate_id":"t"})")));
        CHECK_FALSE(decode_command_request(command(
            R"({"kind":"travel","pos":{"space":"reality_bubble_map_square","frame_id":"b","x":1,"y":1,"z":0}})")));
    }
    SECTION("pickup and drop name a row and a positive count") {
        const auto pickup = decode_command_request(
            command(R"({"kind":"pickup","item_id":"item:1","count":2})"));
        REQUIRE(pickup);
        const auto* item = std::get_if<item_operation>(&pickup->operation);
        REQUIRE(item);
        CHECK_FALSE(item->drop);
        CHECK(item->item_id == "item:1");
        CHECK(item->count == 2);
        const auto drop = decode_command_request(
            command(R"({"kind":"drop","item_id":"item:1","count":1})"));
        REQUIRE(drop);
        CHECK(std::get<item_operation>(drop->operation).drop);
        CHECK_FALSE(
            decode_command_request(command(R"({"kind":"pickup","item_id":"item:1","count":0})")));
        CHECK_FALSE(decode_command_request(command(R"({"kind":"pickup","count":1})")));
        CHECK_FALSE(decode_command_request(command(R"({"kind":"drop","item_id":"","count":1})")));
    }
    SECTION("a context click is the right button on an absolute square") {
        const auto decoded = decode_command_request(
            command(R"({"kind":"context","pos":{"dim":"","x":2,"y":3,"z":0}})"));
        REQUIRE(decoded);
        const auto* click = std::get_if<travel_operation>(&decoded->operation);
        REQUIRE(click);
        CHECK(click->button == "right");
        CHECK(click->target == position{.dim = "", .x = 2, .y = 3, .z = 0});
        const auto left = decode_command_request(
            command(R"({"kind":"travel","pos":{"dim":"","x":2,"y":3,"z":0}})"));
        REQUIRE(left);
        CHECK(std::get<travel_operation>(left->operation).button == "left");
        CHECK_FALSE(decode_command_request(command(R"({"kind":"context"})")));
    }
    SECTION("a tile action names an action id and one absolute square") {
        const auto decoded = decode_command_request(command(
            R"({"kind":"tile_action","action_id":"examine","pos":{"dim":"","x":2,"y":3,"z":0}})"));
        REQUIRE(decoded);
        const auto* tile = std::get_if<tile_operation>(&decoded->operation);
        REQUIRE(tile);
        CHECK(tile->action_id == "examine");
        CHECK(tile->target == position{.dim = "", .x = 2, .y = 3, .z = 0});
        CHECK_FALSE(decode_command_request(
            command(R"({"kind":"tile_action","pos":{"dim":"","x":2,"y":3,"z":0}})")));
        CHECK_FALSE(
            decode_command_request(command(R"({"kind":"tile_action","action_id":"look"})")));
        CHECK_FALSE(decode_command_request(command(
            R"({"kind":"tile_action","action_id":"","pos":{"dim":"","x":2,"y":3,"z":0}})")));
        CHECK_FALSE(decode_command_request(command(
            R"({"kind":"tile_action","action_id":"look","pos":{"dim":"","x":2,"y":3,"z":0},"count":1})")));
    }
    SECTION("a description lists the parts and the actions of the square") {
        const auto text = serialize_description(
            {.parts = {{.kind = "terrain", .text = "floor"}},
             .actions = {{.id = "look", .label = "Look Around", .keys = {";"}}}});
        const auto json = nlohmann::json::parse(text);
        CHECK(json["parts"].size() == 1);
        REQUIRE(json["actions"].size() == 1);
        CHECK(json["actions"][0]
              == nlohmann::json{{"id", "look"}, {"label", "Look Around"}, {"keys", {";"}}});
    }
    SECTION("describe takes an epoch, a boundary and one square") {
        const auto decoded = decode_describe_request(
            R"({"epoch":"e","boundary_id":"b","pos":{"dim":"","x":2,"y":3,"z":0}})");
        REQUIRE(decoded);
        CHECK(decoded->target == position{.dim = "", .x = 2, .y = 3, .z = 0});
        CHECK_FALSE(decode_describe_request(R"({"epoch":"e","boundary_id":"b"})"));
        CHECK_FALSE(decode_describe_request(
            R"({"epoch":"e","boundary_id":"b","pos":{"dim":"","x":2,"y":3,"z":0},"extra":1})"));
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
        CHECK_FALSE(decode_command_request(
            R"({"epoch":"e","expect":{"revision":"7","boundary_id":"b","schema_id":null},"operation":{"kind":"cancel"}})"));
        CHECK_FALSE(decode_command_request(
            R"({"expect":{"revision":"7","boundary_id":"b"},"operation":{"kind":"cancel"}})"));
        CHECK_FALSE(decode_command_request(
            R"({"epoch":"e","expect":{"revision":7,"boundary_id":"b"},"operation":{"kind":"cancel"}})"));
        CHECK_FALSE(decode_command_request(
            R"({"epoch":"e","based_on":{"state_revision":"7","input_boundary_id":"b"},"operation":{"kind":"cancel"}})"));
    }
}

TEST_CASE("serialized values", "[engine_client_wire]") {
    const auto reply = hello_reply{
        .epoch = "epoch:a",
        .engine = {.build = "0.9", .mods = {"bn"}, .use_tiles = false, .tileset = "MSX"},
        .identity = {.locale = "ko", .engine_locale = "en"}};
    const auto hello = serialize_hello(reply);
    CHECK(contains(hello, R"("display":{"tiles":false,"tileset":"MSX"})"));
    // Sound off names no pack.
    CHECK_FALSE(contains(hello, R"("sound")"));
    auto sounding = reply;
    sounding.engine.soundpack = "/bn/data/sound/Basic";
    CHECK(contains(serialize_hello(sounding), R"("sound":{"soundpack":"/bn/data/sound/Basic"})"));
    CHECK(contains(hello, R"("version":"0.3")"));
    CHECK(contains(hello, R"("locale":"ko","engine_locale":"en")"));
    CHECK_FALSE(contains(hello, R"("schema")"));
    CHECK(contains(hello, R"("screen_queue":8)"));
    CHECK(contains(hello, R"("mods":["bn"])"));
    CHECK(contains(hello, R"("cells_per_part":512)"));
    const auto encoded = serialize_receipt({.epoch = "epoch:a", .command_id = "c:1"});
    REQUIRE(encoded);
    CHECK(
        *encoded
        == R"({"epoch":"epoch:a","command_id":"c:1","stage":"received","stage_text":"Received."})");
    CHECK_FALSE(serialize_receipt({.epoch = "", .command_id = "c:1"}));
    const auto failure = serialize_application_error(
        {.kind = error::stale_revision,
         .action = required_action::subscribe,
         .at = clock_point{"epoch:a", 4, 3}});
    REQUIRE(failure);
    CHECK(contains(*failure, R"("kind":"stale_revision")"));
    CHECK(contains(*failure, R"("action":"subscribe")"));
    CHECK(contains(*failure, R"("at":{"epoch":"epoch:a","sequence":"4","revision":"3"})"));
    // The 0.2 errors are application errors too.
    for (const auto kind :
         {error::invalid_arguments, error::unknown_affordance, error::stale_screen}) {
        CHECK(serialize_application_error({.kind = kind}));
    }
    const auto bare = serialize_application_error({.kind = error::command_busy});
    REQUIRE(bare);
    CHECK(*bare == R"({"kind":"command_busy","message":"Another command is still running."})");
}

TEST_CASE(
    "a text that is not UTF-8 is published with replacement characters", "[engine_client_wire]") {
    const auto encoded = serialize_command_result(
        {.epoch = "e",
         .command_id = "c",
         .stage = command_stage::rejected,
         .failure = error::validation_failed,
         .message = "bad \xff byte"});
    CHECK(nlohmann::json::parse(encoded)["error_text"] == "bad \xEF\xBF\xBD byte");
}

TEST_CASE("wire decoding rejects a raw NUL that would hide a suffix", "[engine_client_wire]") {
    CHECK_FALSE(decode_empty_request(std::string{"{}\0{\"x\":1}", 10}));
    CHECK_FALSE(decode_result_request(std::string{"{\"epoch\":\"e\",\"command_id\":\"c\"}\0", 32}));
}

TEST_CASE("wire accepts what the schema accepts", "[engine_client_wire]") {
    // Integral floats are JSON Schema integers; ids count characters, not bytes; the client
    // name and version have no length bound beyond the frame.
    CHECK(decode_choices_request(
        R"({"epoch":"e","boundary_id":"b","component_id":"c","offset":1.0,"limit":2e0})"));
    std::string id;
    for (auto i = 0; i < 256; ++i) { id += "한"; }
    CHECK(decode_result_request(R"({"epoch":")" + id + R"(","command_id":"c"})"));
    CHECK_FALSE(decode_result_request(R"({"epoch":")" + id + "한" + R"(","command_id":"c"})"));
    CHECK(decode_hello_request(
        R"({"versions":["0.2"],"client":{"name":")" + std::string(1000, 'n')
        + R"(","version":"1"}})"));
    const auto with_revision = [](const std::string& revision) {
        return R"({"epoch":"e","expect":{"revision":")" + revision
             + R"(","boundary_id":"b"},"operation":{"kind":"cancel"}})";
    };
    CHECK(decode_command_request(with_revision("18446744073709551615")));
    CHECK_FALSE(decode_command_request(with_revision("18446744073709551616")));
}

TEST_CASE("a 0.2 hello carries its optional members", "[engine_client_wire]") {
    SECTION("the locale and the capabilities") {
        const auto decoded = decode_hello_request(hello_02(
            R"(,"locale":"ko","capabilities":{"tiles":true,"tint_ids":["jeans"],"viewport":{"cols":60,"rows":30},"delivery":{"credits":8,"max_hz":30}})"));
        REQUIRE(decoded);
        CHECK(decoded->locale == "ko");
        CHECK(decoded->tiles);
        CHECK(decoded->tint_ids == std::vector<std::string>{"jeans"});
        REQUIRE(decoded->view);
        CHECK(decoded->view->cols == 60);
        CHECK(decoded->credits == 8);
        CHECK(decoded->max_hz == 30);
    }
    SECTION("the loose members stay accepted") {
        const auto decoded = decode_hello_request(hello_02(R"(,"tiles":true)"));
        REQUIRE(decoded);
        CHECK(decoded->tiles);
        CHECK_FALSE(decoded->locale);
    }
    SECTION("a member is not said both loose and under capabilities") {
        CHECK_FALSE(
            decode_hello_request(hello_02(R"(,"tiles":true,"capabilities":{"tiles":true})")));
        CHECK_FALSE(decode_hello_request(hello_02(R"(,"capabilities":{"locale":"ko"})")));
        CHECK_FALSE(
            decode_hello_request(hello_02(R"(,"capabilities":{"delivery":{"credits":0}})")));
    }
    SECTION("a schema member of an earlier 0.2 client is accepted and ignored") {
        const auto decoded = decode_hello_request(hello_02(R"(,"schema":")" + hash_a + R"(")"));
        REQUIRE(decoded);
        CHECK(decoded->versions == std::vector<std::string>{"0.2"});
        CHECK(decoded->client_name == "a");
    }
    SECTION(
        "a hello that does not offer 0.2 decodes with its members, for the negotiation to refuse it") {
        const auto base = std::string{R"({"versions":["0.3"],"client":{"name":"a","version":"1"})"};
        const auto decoded = decode_hello_request(base + "}");
        REQUIRE(decoded);
        CHECK(decoded->versions == std::vector<std::string>{"0.3"});
        CHECK(decode_hello_request(base + R"(,"schema":")" + hash_a + R"("})"));
        CHECK(decode_hello_request(base + R"(,"locale":"ko"})"));
        CHECK(decode_hello_request(base + R"(,"capabilities":{}})"));
    }
}

namespace {
auto invoke_command(const std::string& operation, const std::string& expect) -> std::string {
    return R"({"epoch":"epoch:a","expect":)" + expect + R"(,"operation":)" + operation + "}";
}
const auto screen_expect = std::string{R"({"screen_id":"screen:1"})"};
const auto boundary_expect = std::string{R"({"boundary_id":"boundary:1","revision":"7"})"};
} // namespace

TEST_CASE(
    "an invoke names an affordance and expects its screen or the boundary",
    "[engine_client_wire]") {
    SECTION("a screen expectation") {
        const auto decoded = decode_command_request(
            invoke_command(R"({"kind":"invoke","affordance_id":"action:move"})", screen_expect));
        REQUIRE(decoded);
        CHECK(decoded->expect.screen_id == "screen:1");
        const auto* operation = std::get_if<invoke_operation>(&decoded->operation);
        REQUIRE(operation);
        CHECK(operation->affordance_id == "action:move");
        CHECK_FALSE(operation->args.row);
    }
    SECTION("a boundary expectation") {
        const auto decoded = decode_command_request(
            invoke_command(R"({"kind":"invoke","affordance_id":"choose"})", boundary_expect));
        REQUIRE(decoded);
        CHECK_FALSE(decoded->expect.screen_id);
        CHECK(decoded->expect.boundary_id == "boundary:1");
        CHECK(decoded->expect.revision == 7);
    }
    SECTION("every argument") {
        const auto decoded = decode_command_request(invoke_command(
            R"({"kind":"invoke","affordance_id":"a","args":{"row":"r","count":-5,"text":"x","pos":{"dim":"","x":-9,"y":4,"z":1},"pane":"left"}})",
            boundary_expect));
        REQUIRE(decoded);
        const auto& args = std::get<invoke_operation>(decoded->operation).args;
        CHECK(args.row == "r");
        CHECK(args.count == -5);
        CHECK(args.text == "x");
        CHECK(args.pos == position{.dim = "", .x = -9, .y = 4, .z = 1});
        CHECK(args.pane == "left");
    }
    SECTION("an empty text and a zero count are values") {
        const auto decoded = decode_command_request(invoke_command(
            R"({"kind":"invoke","affordance_id":"a","args":{"text":"","count":0}})",
            boundary_expect));
        REQUIRE(decoded);
        const auto& args = std::get<invoke_operation>(decoded->operation).args;
        CHECK(args.text == "");
        CHECK(args.count == 0);
    }
    SECTION("the expectation form must belong to the operation") {
        // No operation expects a schema, and only an invoke expects a screen.
        CHECK_FALSE(decode_command_request(invoke_command(
            R"({"kind":"invoke","affordance_id":"a"})",
            R"({"revision":"7","boundary_id":"b","schema_id":null})")));
        CHECK_FALSE(decode_command_request(invoke_command(R"({"kind":"cancel"})", screen_expect)));
        CHECK(decode_command_request(invoke_command(R"({"kind":"cancel"})", boundary_expect)));
        CHECK_FALSE(decode_command_request(invoke_command(
            R"({"kind":"invoke","affordance_id":"a"})",
            R"({"screen_id":"screen:1","boundary_id":"b","revision":"7"})")));
        CHECK_FALSE(decode_command_request(
            invoke_command(R"({"kind":"invoke","affordance_id":"a"})", R"({})")));
        CHECK_FALSE(decode_command_request(
            invoke_command(R"({"kind":"invoke","affordance_id":"a"})", R"({"screen_id":""})")));
        CHECK_FALSE(decode_command_request(
            invoke_command(R"({"kind":"invoke","affordance_id":"a"})", R"({"boundary_id":"b"})")));
        CHECK_FALSE(decode_command_request(
            invoke_command(R"({"kind":"invoke","affordance_id":"a"})", "null")));
    }
    SECTION("rejected shapes") {
        const auto rejects = [](const std::string& operation) {
            CHECK_FALSE(decode_command_request(invoke_command(operation, screen_expect)));
        };
        rejects(R"({"kind":"invoke"})");
        rejects(R"({"kind":"invoke","affordance_id":""})");
        rejects(R"({"kind":"invoke","affordance_id":"a","extra":1})");
        rejects(R"({"kind":"invoke","affordance_id":"a","args":null})");
        rejects(R"({"kind":"invoke","affordance_id":"a","args":{"unknown":1}})");
        rejects(R"({"kind":"invoke","affordance_id":"a","args":{"count":9007199254740992}})");
        rejects(R"({"kind":"invoke","affordance_id":"a","args":{"count":"1"}})");
        rejects(R"({"kind":"invoke","affordance_id":"a","args":{"text":1}})");
        rejects(R"({"kind":"invoke","affordance_id":"a","args":{"pos":{"x":1,"y":1,"z":0}}})");
        rejects(R"({"kind":"invoke","affordance_id":"a","args":{"pane":""}})");
    }
}

TEST_CASE("a choices request names the list to read", "[engine_client_wire]") {
    const auto window = [](const std::string& extra) {
        return R"({"epoch":"e","boundary_id":"b","offset":0,"limit":10)" + extra + "}";
    };
    const auto named = decode_choices_request(window(R"(,"component_id":"list:items")"));
    REQUIRE(named);
    CHECK(named->component_id == "list:items");
    CHECK_FALSE(decode_choices_request(window("")));
    CHECK_FALSE(decode_choices_request(window(R"(,"component_id":"")")));
    CHECK_FALSE(decode_choices_request(window(R"(,"component_id":7)")));
}

TEST_CASE(
    "set_mode names a mode and one of its values", "[engine_client_wire][engine_client_mode]") {
    const auto decoded = decode_command_request(
        command(R"({"kind":"set_mode","mode":"safe_mode","value":"off"})"));
    REQUIRE(decoded);
    const auto* mode = std::get_if<mode_operation>(&decoded->operation);
    REQUIRE(mode);
    CHECK(mode->mode == "safe_mode");
    CHECK(mode->value == "off");
    CHECK_FALSE(decode_command_request(command(R"({"kind":"set_mode","mode":"safe_mode"})")));
    CHECK_FALSE(decode_command_request(command(R"({"kind":"set_mode","value":"off"})")));
    CHECK_FALSE(decode_command_request(command(R"({"kind":"set_mode","mode":"","value":"off"})")));
    CHECK_FALSE(
        decode_command_request(command(R"({"kind":"set_mode","mode":"safe_mode","value":""})")));
    CHECK_FALSE(decode_command_request(
        command(R"({"kind":"set_mode","mode":"safe_mode","value":"off","count":1})")));
}
