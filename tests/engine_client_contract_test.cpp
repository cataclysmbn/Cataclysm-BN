#include "catch/catch.hpp"
#include "client_input.h"
#include "engine_client_contract.h"
#include "engine_client_wire.h"
#include "input.h"
#include "path_info.h"

#include <algorithm>
#include <climits>
#include <fstream>
#include <map>
#include <nlohmann/json.hpp>
#include <set>
#include <string>
#include <utility>

namespace {
class test_authority final: public engine_client::command_authority {
public:
    auto current_permissions() const -> engine_client::command_permissions override {
        return {.accepts_interaction_commands = true, .accepts_registered_actions = true};
    }
};
auto ready() -> engine_client::readiness {
    return {
        .phase = "menu", .accepts_interaction_commands = true, .accepts_registered_actions = true};
}
auto capture() -> engine_client::boundary_state {
    const auto value = engine_client::capture_boundary({.epoch = "epoch:test", .ready = ready()});
    REQUIRE(value);
    return *value;
}
auto at_of(const engine_client::boundary_state&, engine_client::counter revision = 1)
    -> engine_client::clock_point {
    return {.epoch = "epoch:test", .sequence = revision, .revision = revision};
}
auto choose_request(const engine_client::boundary_state& boundary, const std::string& choice)
    -> engine_client::command_request {
    return {
        .epoch = "epoch:test",
        .expect =
            {.revision = 1,
             .boundary_id = boundary.id,
             .schema_id = boundary.interaction->schema_id},
        .operation = engine_client::semantic_operation{
            .command =
                {.operation = game_client::interaction_operation::choose, .target_id = choice}}};
}
auto contains(const std::string& text, const std::string& part) -> bool {
    return text.find(part) != std::string::npos;
}
} // namespace

TEST_CASE("wire interaction is trimmed and renamed", "[engine_client_contract]") {
    auto context = input_context{"YESNO"};
    const auto input_scope = game_client::input_context_scope{context, "YESNO"};
    const auto scope = game_client::interaction_scope{
        context, [] {
            return game_client::interaction_snapshot{
                .kind = game_client::interaction_kind::choices,
                .allow_cancel = true,
                .panes =
                    {{.id = "pane:left",
                      .label = "Left",
                      .filter = "rock",
                      .blocked_areas = {"ITEMS_CONTAINER"}}},
                .choices = {{.id = "choice:yes", .label = "Yes", .highlighted = true}},
            };
        }};
    game_client::begin_input_boundary();
    const auto wire = engine_client::serialize_boundary(capture());
    CHECK(contains(wire, R"("choice_total":1)"));
    CHECK(contains(
        wire, R"("compat":{"focus":{"choice_id":"choice:yes"},"panes":[{"id":"pane:left")"));
    CHECK(contains(wire, R"("filter":"rock")"));
    CHECK(contains(wire, R"("blocked_areas":["ITEMS_CONTAINER"])"));
    for (const auto* removed :
         {"structured", "actions_only", "choice_page", "highlighted", "projection", "input_id",
          "bubble_ms", "frame_id"}) {
        CHECK_FALSE(contains(wire, removed));
    }
}

TEST_CASE("screen text and keys are published on the wire", "[engine_client_contract]") {
    auto context = input_context{"YESNO"};
    const auto input_scope = game_client::input_context_scope{context, "YESNO"};
    const auto scope = game_client::interaction_scope{
        context, [] {
            return game_client::interaction_snapshot{
                .kind = game_client::interaction_kind::choices,
                .areas =
                    {{.action_id = "ITEMS_N",
                      .label = "North",
                      .key = "z",
                      .x = 11,
                      .y = 0,
                      .available = false}},
                .choices = {{.id = "entry:4", .label = "Options", .action_id = "open_options"}},
                .overmap = game_client::
                    interaction_overmap{.cols = 0, .rows = 0, .cursor_description = "forest"},
            };
        }};
    game_client::begin_input_boundary();
    const auto wire = engine_client::serialize_boundary(capture());
    CHECK(contains(
        wire,
        R"("areas":[{"action_id":"ITEMS_N","label":"North","key":"z","x":11,"y":0,"available":false}])"));
    CHECK(contains(wire, R"("action_id":"open_options")"));
    CHECK(contains(wire, R"("cursor_description":"forest")"));
}

TEST_CASE("a colored message line is published with its tone", "[engine_client_contract]") {
    auto context = input_context{"NPC_TRADE"};
    const auto input_scope = game_client::input_context_scope{context, "NPC_TRADE"};
    const auto tone_wire = [&context](const std::string& tone) {
        const auto scope = game_client::interaction_scope{
            context, [tone] {
                return game_client::interaction_snapshot{
                    .kind = game_client::interaction_kind::choices,
                    .message = "Credit $1.00 | Balance acceptable: yes",
                    .message_tone = tone,
                    .choices = {{.id = "choice:yes", .label = "Yes"}},
                };
            }};
        game_client::begin_input_boundary();
        return engine_client::serialize_boundary(capture());
    };
    CHECK(contains(tone_wire("good"), R"("message_tone":"good")"));
    CHECK_FALSE(contains(tone_wire(""), "message_tone"));
}

TEST_CASE("a paged category list publishes its mode and page", "[engine_client_contract]") {
    auto context = input_context{"NPC_TRADE"};
    const auto input_scope = game_client::input_context_scope{context, "NPC_TRADE"};
    const auto wire = [&context](const std::optional<bool>& mode) {
        const auto scope = game_client::interaction_scope{
            context, [mode] {
                return game_client::interaction_snapshot{
                    .kind = game_client::interaction_kind::choices,
                    .category_mode = mode,
                    .panes = {{.id = "pane", .page = game_client::interaction_page_position{2, 3}}},
                    .choices = {{.id = "choice:yes", .label = "Yes"}},
                };
            }};
        game_client::begin_input_boundary();
        return engine_client::serialize_boundary(capture());
    };
    CHECK(contains(wire(true), R"("category_mode":true)"));
    CHECK(contains(wire(false), R"("category_mode":false)"));
    CHECK_FALSE(contains(wire(std::nullopt), "category_mode"));
    CHECK(contains(wire(true), R"("number":2)"));
    CHECK(contains(wire(true), R"("count":3)"));
}

TEST_CASE(
    "a row publishes its column keys and typed colors, marks and nesting",
    "[engine_client_contract]") {
    auto context = input_context{"TYPED_ROW"};
    const auto input_scope = game_client::input_context_scope{context, "TYPED_ROW"};
    const auto wire = [&context](game_client::interaction_choice choice) {
        const auto scope = game_client::interaction_scope{
            context, [&choice] {
                return game_client::interaction_snapshot{
                    .kind = game_client::interaction_kind::choices, .choices = {choice}};
            }};
        game_client::begin_input_boundary();
        return engine_client::serialize_boundary(capture());
    };
    const auto typed = wire({
        .id = "row:0",
        .label = "Zeile",
        .columns = {{.key = "weight", .label = "Gewicht", .value = "2", .color = "c_red"}},
        .favorite = true,
        .color = "c_white",
        .key_color = "c_yellow",
        .markup = "<color_white>Zeile</color>",
        .parent_id = "row:1",
        .nest_color = "c_dark_gray",
        .mark = game_client::interaction_mark{.glyph = "#", .color = "c_light_blue"},
    });
    CHECK(contains(
        typed, R"("columns":[{"key":"weight","label":"Gewicht","value":"2","color":"c_red"}])"));
    CHECK(contains(typed, R"("favorite":true)"));
    CHECK(contains(typed, R"("key_color":"c_yellow")"));
    CHECK(contains(typed, R"("parent_id":"row:1")"));
    CHECK(contains(typed, R"("nest_color":"c_dark_gray")"));
    CHECK(contains(typed, R"("mark":{"glyph":"#","color":"c_light_blue"})"));
    const auto plain = wire(
        {.id = "row:0", .label = "Row", .mark = game_client::interaction_mark{.glyph = "-"}});
    CHECK(contains(plain, R"("mark":{"glyph":"-"})"));
    for (const auto* absent : {"favorite", "key_color", "markup", "parent_id", "nest_color"}) {
        CHECK_FALSE(contains(plain, absent));
    }
}

TEST_CASE("compat focus survives paging past the focused row", "[engine_client_contract]") {
    auto context = input_context{"BIGLIST"};
    const auto input_scope = game_client::input_context_scope{context, "BIGLIST"};
    const auto scope = game_client::interaction_scope{
        context, [] {
            auto value = game_client::interaction_snapshot{
                .kind = game_client::interaction_kind::choices};
            for (auto i = 0; i < 300; ++i) {
                value.choices.push_back(
                    {.id = "choice:" + std::to_string(i), .label = "Row", .highlighted = i == 250});
            }
            return value;
        }};
    game_client::begin_input_boundary();
    const auto boundary = capture();
    REQUIRE(boundary.interaction);
    CHECK(boundary.interaction->choices.size() == engine_client::maximum_rows);
    CHECK(boundary.interaction->choice_total == 300);
    const auto wire = engine_client::serialize_boundary(boundary);
    CHECK(contains(wire, R"("choice_total":300)"));
    CHECK(contains(wire, R"("focus":{"choice_id":"choice:250"})"));
    CHECK_FALSE(contains(wire, R"("id":"choice:250")"));

    const auto page = engine_client::read_choices(
        {.epoch = "epoch:test", .boundary_id = boundary.id, .offset = 200, .limit = 200},
        "epoch:test");
    REQUIRE(page);
    CHECK(page->total == 300);
    CHECK(page->choices.size() == 100);
    CHECK(page->choices.front().id == "choice:200");
    const auto stale = engine_client::
        read_choices({.epoch = "epoch:test", .boundary_id = "boundary:old"}, "epoch:test");
    REQUIRE_FALSE(stale);
    CHECK(stale.error() == engine_client::error::stale_boundary);
    const auto other_epoch = engine_client::
        read_choices({.epoch = "epoch:other", .boundary_id = boundary.id}, "epoch:test");
    REQUIRE_FALSE(other_epoch);
    CHECK(other_epoch.error() == engine_client::error::stale_epoch);
}

TEST_CASE("absolute target maps to native bounds", "[engine_client_contract]") {
    auto context = input_context{"TARGET"};
    const auto input_scope = game_client::input_context_scope{context, "TARGET"};
    const auto scope = game_client::interaction_scope{
        context, [] {
            return game_client::interaction_snapshot{
                .kind = game_client::interaction_kind::target,
                .target =
                    game_client::interaction_target{
                        .source = {.x = 10, .y = 10},
                        .cursor = {.x = 10, .y = 10},
                        .minimum_position =
                            game_client::interaction_position{.x = 0, .y = 0, .z = 0},
                        .maximum_position =
                            game_client::interaction_position{.x = 20, .y = 20, .z = 0},
                        .range = 5,
                        .distance_metric = "square",
                    },
            };
        }};
    game_client::begin_input_boundary();
    const auto boundary = capture();
    const auto frame = engine_client::current_bubble_frame();
    const auto absolute = [&](const int x, const int y) {
        return engine_client::
            position{.dim = frame.dim, .x = x + frame.x, .y = y + frame.y, .z = 0};
    };
    const auto wire = engine_client::serialize_boundary(boundary);
    const auto expected = [&](const std::string& name, const int x, const int y) {
        return "\"" + name + "\":{\"dim\":\"" + frame.dim + "\",\"x\":"
             + std::to_string(x + frame.x) + ",\"y\":" + std::to_string(y + frame.y) + ",\"z\":0}";
    };
    CHECK(contains(wire, expected("current", 10, 10)));
    CHECK(contains(wire, expected("minimum", 0, 0)));
    CHECK(contains(wire, expected("maximum", 20, 20)));
    const auto target_request = [&](const engine_client::position& target) {
        return engine_client::command_request{
            .epoch = "epoch:test",
            .expect =
                {.revision = 1,
                 .boundary_id = boundary.id,
                 .schema_id = boundary.interaction->schema_id},
            .operation = engine_client::semantic_operation{
                .command = {.operation = game_client::interaction_operation::set_target},
                .target = target}};
    };
    auto authority = test_authority{};
    SECTION("an absolute square inside the bubble reaches the native position") {
        auto lifecycle = engine_client::command_lifecycle{"epoch:test", authority};
        REQUIRE(lifecycle.submit(target_request(absolute(12, 10))));
        const auto resolved = lifecycle.validate(at_of(boundary), boundary, point{80, 24});
        REQUIRE(resolved);
        REQUIRE(resolved->interaction);
        CHECK(
            resolved->interaction->position == game_client::interaction_position{.x = 12, .y = 10});
    }
    SECTION("native range and bounds still reject") {
        auto lifecycle = engine_client::command_lifecycle{"epoch:test", authority};
        REQUIRE(lifecycle.submit(target_request(absolute(19, 10))));
        CHECK_FALSE(lifecycle.validate(at_of(boundary), boundary, point{80, 24}));
        REQUIRE(lifecycle.submit(target_request(absolute(-1, 10))));
        CHECK_FALSE(lifecycle.validate(at_of(boundary), boundary, point{80, 24}));
    }
    SECTION("another dimension is rejected") {
        auto lifecycle = engine_client::command_lifecycle{"epoch:test", authority};
        auto elsewhere = absolute(12, 10);
        elsewhere.dim = "elsewhere";
        REQUIRE(lifecycle.submit(target_request(elsewhere)));
        const auto rejected = lifecycle.validate(at_of(boundary), boundary, point{80, 24});
        REQUIRE_FALSE(rejected);
        CHECK(rejected.error() == engine_client::error::validation_failed);
    }
}

TEST_CASE("command stages and expectations", "[engine_client_contract]") {
    auto context = input_context{"YESNO"};
    context.register_action("YES");
    const auto input_scope = game_client::input_context_scope{context, "YESNO"};
    const auto scope = game_client::interaction_scope{
        context, [] {
            return game_client::interaction_snapshot{
                .kind = game_client::interaction_kind::choices,
                .allow_cancel = true,
                .choices = {{.id = "choice:yes", .label = "Yes"}},
            };
        }};
    game_client::begin_input_boundary();
    const auto boundary = capture();
    auto authority = test_authority{};
    auto lifecycle = engine_client::command_lifecycle{"epoch:test", authority};

    SECTION("received, validated, executing, completed at a distinct boundary") {
        const auto request = choose_request(boundary, "choice:yes");
        const auto received = lifecycle.submit(request);
        REQUIRE(received);
        CHECK(received->stage == engine_client::command_stage::received);
        CHECK_FALSE(lifecycle.submit(request)); // command_busy
        CHECK_FALSE(lifecycle.execution_started());
        REQUIRE(lifecycle.validate(at_of(boundary), boundary, point{80, 24}));
        CHECK(lifecycle.result(received->command_id)->stage
              == engine_client::command_stage::validated);
        CHECK_FALSE(lifecycle.validate(at_of(boundary), boundary, point{80, 24})); // never twice
        REQUIRE(lifecycle.execution_started());
        CHECK_FALSE(lifecycle.complete_at_boundary(at_of(boundary, 2), boundary)); // same boundary
        game_client::begin_input_boundary();
        const auto next = capture();
        REQUIRE(lifecycle.complete_at_boundary(at_of(next, 2), next));
        const auto done = lifecycle.result(received->command_id);
        REQUIRE(done);
        CHECK(done->stage == engine_client::command_stage::completed);
        REQUIRE(done->at);
        CHECK(done->at->sequence == 2);
        const auto wire = engine_client::serialize_command_result(*done);
        CHECK(contains(wire, R"("stage":"completed")"));
        CHECK(contains(wire, R"("at":{"epoch":"epoch:test","sequence":"2","revision":"2"})"));
    }
    SECTION("stale expectations reject without a boundary change") {
        const auto check =
            [&](const engine_client::command_request& request, const engine_client::error reason) {
                REQUIRE(lifecycle.submit(request));
                const auto rejected = lifecycle.validate(at_of(boundary), boundary, point{80, 24});
                REQUIRE_FALSE(rejected);
                CHECK(rejected.error() == reason);
            };
        auto request = choose_request(boundary, "choice:yes");
        request.expect.revision = 7;
        check(request, engine_client::error::stale_revision);
        request = choose_request(boundary, "choice:yes");
        request.expect.boundary_id = "boundary:old";
        check(request, engine_client::error::stale_boundary);
        request = choose_request(boundary, "choice:yes");
        request.expect.schema_id = std::nullopt;
        check(request, engine_client::error::stale_interaction_schema);
        request = choose_request(boundary, "choice:yes");
        request.epoch = "epoch:other";
        check(request, engine_client::error::stale_epoch);
        check(choose_request(boundary, "choice:missing"), engine_client::error::validation_failed);
    }
    SECTION("a registered action is a command and needs the live schema") {
        auto request = choose_request(boundary, "choice:yes");
        request.operation = engine_client::registered_action{.id = "YES"};
        REQUIRE(lifecycle.submit(request));
        CHECK(lifecycle.validate(at_of(boundary), boundary, point{80, 24}));
    }
    SECTION("malformed operations never allocate a receipt") {
        auto request = choose_request(boundary, "choice:yes");
        std::get<engine_client::semantic_operation>(request.operation).target =
            engine_client::position{};
        CHECK_FALSE(lifecycle.submit(request));
    }
    SECTION("interrupt is terminal and a new submit replaces the old result") {
        const auto first = lifecycle.submit(choose_request(boundary, "choice:yes"));
        REQUIRE(first);
        REQUIRE(lifecycle.interrupt(engine_client::error::not_ready));
        CHECK(lifecycle.result(first->command_id)->stage
              == engine_client::command_stage::interrupted);
        const auto second = lifecycle.submit(choose_request(boundary, "choice:yes"));
        REQUIRE(second);
        CHECK_FALSE(lifecycle.result(first->command_id));
    }
}

TEST_CASE("a boundary without interaction expects a null schema", "[engine_client_contract]") {
    auto context = input_context{"DEFAULTMODE"};
    context.register_action("RIGHT");
    const auto input_scope = game_client::input_context_scope{context, "DEFAULTMODE"};
    game_client::begin_input_boundary();
    const auto boundary = capture();
    CHECK_FALSE(boundary.interaction);
    CHECK(contains(engine_client::serialize_boundary(boundary), R"("interaction":null)"));
    auto authority = test_authority{};
    auto lifecycle = engine_client::command_lifecycle{"epoch:test", authority};
    const auto request = engine_client::command_request{
        .epoch = "epoch:test",
        .expect = {.revision = 1, .boundary_id = boundary.id, .schema_id = std::nullopt},
        .operation = engine_client::registered_action{.id = "RIGHT"}};
    REQUIRE(lifecycle.submit(request));
    CHECK(lifecycle.validate(at_of(boundary), boundary, point{80, 24}));
    REQUIRE(lifecycle.interrupt(engine_client::error::not_ready));
    auto wrong = request;
    wrong.expect.schema_id = "schema:invented";
    REQUIRE(lifecycle.submit(wrong));
    const auto rejected = lifecycle.validate(at_of(boundary), boundary, point{80, 24});
    REQUIRE_FALSE(rejected);
    CHECK(rejected.error() == engine_client::error::stale_interaction_schema);
}

TEST_CASE(
    "boundary actions list the single keys the active context binds", "[engine_client_contract]") {
    auto context = input_context{"DEFAULTMODE"};
    context.register_action("LEVEL_DOWN");
    context.register_action("QUIT");
    const auto input_scope = game_client::input_context_scope{context, "DEFAULTMODE"};
    game_client::begin_input_boundary();
    const auto boundary = capture();
    const auto binds = [&](const std::string& id, const std::string& key) {
        const auto found = std::ranges::find(boundary.actions, id, &engine_client::action::id);
        REQUIRE(found != boundary.actions.end());
        return std::ranges::find(found->keys, key) != found->keys.end();
    };
    CHECK(binds("LEVEL_DOWN", ">"));
    CHECK(binds("QUIT", "ESC"));
    CHECK_FALSE(binds("QUIT", ">"));
    CHECK(contains(engine_client::serialize_boundary(boundary), R"("keys":[)"));
}

TEST_CASE(
    "every bound action of every input context carries its keys in the boundary",
    "[engine_client_contract]") {
    // The expected keys come from the shipped file; the boundary must carry each one (control,
    // function, navigation and numpad keys included), so a client never needs its own key table.
    auto stream = std::ifstream{PATH_INFO::keybindingsdir() + "keybindings.json"};
    REQUIRE(stream.good());
    const auto file = nlohmann::json::parse(stream);
    auto expected = std::map<std::string, std::map<std::string, std::set<std::string>>>{};
    for (const auto& entry : file) {
        if (entry.value("type", "") != "keybinding") { continue; }
        for (const auto& binding : entry.value("bindings", nlohmann::json::array())) {
            if (binding.value("input_method", "") != "keyboard") { continue; }
            const auto& key = binding["key"];
            const auto names = key.is_array() ? key : nlohmann::json::array({key});
            if (names.size() == 1 && names[0].is_string() && entry.value("category", "") != "") {
                // The file may write a space as itself; the portable name is SPACE.
                const auto name = names[0].get<std::string>();
                expected[entry.value("category", "")][entry.value("id", "")].insert(
                    name == " " ? "SPACE" : name);
            }
        }
    }
    REQUIRE(expected["DEFAULTMODE"].size() > 50);
    auto checked = std::set<std::string>{};
    for (const auto& [category, actions] : expected) {
        auto context = input_context{category};
        for (const auto& [id, keys] : actions) { context.register_action(id); }
        const auto input_scope = game_client::input_context_scope{context, category};
        game_client::begin_input_boundary();
        const auto boundary = capture();
        auto carried = std::map<std::string, std::set<std::string>>{};
        for (const auto& action : boundary.actions) {
            carried[action.id].insert(action.keys.begin(), action.keys.end());
        }
        for (const auto& [id, keys] : actions) {
            for (const auto& key : keys) {
                INFO(category << " " << id << " " << key);
                CHECK(carried[id].contains(key));
                checked.insert(key);
            }
        }
    }
    // The classes of key a client has to name: control, function, navigation and numpad.
    for (const auto* key :
         {"CTRL+S", "F1", "BACKTAB", "PPAGE", "NUMPAD_8", "ESC", "SPACE", "RETURN"}) {
        INFO(key);
        CHECK(checked.contains(key));
    }
}

TEST_CASE(
    "an open menu boundary carries its input context's navigation keys",
    "[engine_client_contract]") {
    // The menu's own bindings (including the default context's) are what a client forwards to
    // move the engine's cursor, page, confirm and cancel.
    auto context = input_context{"UILIST"};
    for (const auto* id : {"UP", "DOWN", "PAGE_UP", "PAGE_DOWN", "CONFIRM", "QUIT"}) {
        context.register_action(id);
    }
    const auto input_scope = game_client::input_context_scope{context, "UILIST"};
    const auto scope = game_client::interaction_scope{
        context, [] {
            return game_client::interaction_snapshot{
                .kind = game_client::interaction_kind::choices,
                .choices = {{.id = "a", .label = "A"}, {.id = "b", .label = "B"}}};
        }};
    game_client::begin_input_boundary();
    const auto boundary = capture();
    REQUIRE(boundary.interaction);
    const auto binds = [&](const std::string& id, const std::string& key) {
        const auto found = std::ranges::find(boundary.actions, id, &engine_client::action::id);
        REQUIRE(found != boundary.actions.end());
        return std::ranges::find(found->keys, key) != found->keys.end();
    };
    CHECK(binds("UP", "UP"));
    CHECK(binds("DOWN", "DOWN"));
    CHECK(binds("PAGE_UP", "PPAGE"));
    CHECK(binds("PAGE_DOWN", "NPAGE"));
    CHECK(binds("CONFIRM", "RETURN"));
    CHECK(binds("QUIT", "ESC"));
}

TEST_CASE(
    "bubble conversion rejects what the native int range cannot hold", "[engine_client_contract]") {
    const auto frame = engine_client::bubble_frame{.dim = "", .x = 1000, .y = -1000};
    const auto at = [](const int x, const int y) {
        return engine_client::position{.dim = "", .x = x, .y = y, .z = 3};
    };
    CHECK(engine_client::to_bubble(at(1010, -990), frame)
          == game_client::interaction_position{.x = 10, .y = 10, .z = 3});
    // INT_MIN - 1000 and INT_MAX + 1000 overflow int; they must not reach the native checks.
    CHECK_FALSE(engine_client::to_bubble(at(INT_MIN, 0), frame));
    CHECK_FALSE(engine_client::to_bubble(at(0, INT_MAX), frame));
    CHECK(engine_client::to_bubble(at(INT_MIN + 1000, 0), frame));
    CHECK_FALSE(engine_client::to_bubble({.dim = "elsewhere"}, frame));
}

TEST_CASE("an oversized choices page is a recoverable resource limit", "[engine_client_contract]") {
    auto context = input_context{"HEAVY"};
    const auto input_scope = game_client::input_context_scope{context, "HEAVY"};
    const auto scope = game_client::interaction_scope{
        context, [] {
            auto value = game_client::interaction_snapshot{
                .kind = game_client::interaction_kind::choices};
            for (auto i = 0; i < 200; ++i) {
                value.choices.push_back(
                    {.id = "choice:" + std::to_string(i),
                     .label = "Row",
                     .description = std::string(4000, 'x')});
            }
            return value;
        }};
    game_client::begin_input_boundary();
    const auto boundary = capture();
    const auto page = [&](const std::size_t limit) {
        return engine_client::read_choices(
            {.epoch = "epoch:test", .boundary_id = boundary.id, .offset = 0, .limit = limit},
            "epoch:test");
    };
    const auto heavy = page(200);
    REQUIRE_FALSE(heavy);
    CHECK(heavy.error() == engine_client::error::resource_limit);
    const auto light = page(10);
    REQUIRE(light);
    CHECK(light->choices.size() == 10);
    CHECK(light->total == 200);
}

TEST_CASE(
    "a target publishes its distance by the game's metric and its status in words",
    "[engine_client_contract]") {
    auto context = input_context{"TARGET"};
    const auto input_scope = game_client::input_context_scope{context, "TARGET"};
    const auto wire_of = [&](const std::string& metric, const int dz) {
        const auto scope = game_client::interaction_scope{
            context, [&] {
                return game_client::interaction_snapshot{
                    .kind = game_client::interaction_kind::target,
                    .target =
                        game_client::interaction_target{
                            .source = {.x = 10, .y = 10, .z = 0},
                            .cursor = {.x = 13, .y = 14, .z = dz},
                            .range = 9,
                            .distance_metric = metric,
                            .status = "good",
                            .status_text = "range here, ready",
                        },
                };
            }};
        game_client::begin_input_boundary();
        return engine_client::serialize_boundary(capture());
    };
    CHECK(contains(wire_of("square", 0), "\"distance\":4"));
    CHECK(contains(wire_of("trig", 0), "\"distance\":5"));
    // The vertical axis counts, which a client computing from x and y alone misses.
    CHECK(contains(wire_of("square", 6), "\"distance\":6"));
    CHECK(contains(wire_of("trig", 6), "\"distance\":8"));
    CHECK(contains(wire_of("square", 0), "\"status_text\":\"range here, ready\""));
}

TEST_CASE("a screen that acknowledges any key names its choice", "[engine_client_contract]") {
    auto context = input_context{"POPUP"};
    const auto input_scope = game_client::input_context_scope{context, "POPUP"};
    const auto wire_of = [&](const std::string& choice) {
        const auto scope = game_client::interaction_scope{
            context, [&] {
                return game_client::interaction_snapshot{
                    .kind = game_client::interaction_kind::choices,
                    .choices = {{.id = "acknowledge", .label = "Press any key"}},
                    .any_key_choice_id = choice,
                };
            }};
        game_client::begin_input_boundary();
        return engine_client::serialize_boundary(capture());
    };
    CHECK(contains(wire_of("acknowledge"), "\"any_key_choice_id\":\"acknowledge\""));
    CHECK_FALSE(contains(wire_of(""), "any_key_choice_id"));
}

TEST_CASE("a failed command says why in the game's language", "[engine_client_contract]") {
    const auto failed = [](const std::string& message) {
        return engine_client::serialize_command_result(
            {.epoch = "epoch:test",
             .command_id = "command:1",
             .stage = engine_client::command_stage::rejected,
             .failure = engine_client::error::validation_failed,
             .message = message});
    };
    CHECK(contains(failed(""), "\"error_text\":\"Not possible here.\""));
    CHECK(contains(failed("Out of reach."), "\"error_text\":\"Out of reach.\""));
    CHECK_FALSE(contains(
        engine_client::serialize_command_result(
            {.epoch = "epoch:test",
             .command_id = "command:1",
             .stage = engine_client::command_stage::completed}),
        "error_text"));
    const auto stale = engine_client::serialize_application_error(
        {.kind = engine_client::error::stale_boundary});
    REQUIRE(stale);
    CHECK(contains(*stale, "\"message\":\"The screen changed, try again.\""));
}

TEST_CASE("an invoke expects its screen or its boundary by scope", "[engine_client_contract]") {
    auto context = input_context{"YESNO"};
    const auto input_scope = game_client::input_context_scope{context, "YESNO"};
    const auto scope = game_client::interaction_scope{
        context, [] {
            return game_client::interaction_snapshot{
                .kind = game_client::interaction_kind::choices,
                .allow_cancel = true,
                .choices = {{.id = "choice:yes", .label = "Yes"}},
                .choice_total = 1,
            };
        }};
    game_client::begin_input_boundary();
    const auto boundary = capture();
    const auto screen_id = "screen:read:" + boundary.id;
    auto authority = test_authority{};
    auto lifecycle = engine_client::command_lifecycle{"epoch:test", authority};
    const auto invoke =
        [&](const std::string& affordance, const engine_client::expectation& expect,
            engine_client::invoke_args args = {}) {
            return engine_client::command_request{
                .epoch = "epoch:test",
                .expect = expect,
                .operation = engine_client::
                    invoke_operation{.affordance_id = affordance, .args = std::move(args)}};
        };
    const auto boundary_form =
        engine_client::expectation{.revision = 1, .boundary_id = boundary.id};
    const auto screen_form = engine_client::expectation{.screen_id = screen_id};
    const auto run = [&](const engine_client::command_request& request) {
        // One command at a time: each run has its own lifecycle.
        auto fresh = engine_client::command_lifecycle{"epoch:test", authority};
        REQUIRE(fresh.submit(request));
        return fresh.validate(at_of(boundary), boundary, point{80, 24});
    };
    SECTION("a screen expectation holds while its screen is on top") {
        const auto resolved = run(invoke("bn:cancel", screen_form));
        REQUIRE(resolved);
        REQUIRE(resolved->interaction);
        CHECK(resolved->interaction->operation == game_client::interaction_operation::cancel);
    }
    SECTION("a screen expectation for another screen is stale_screen") {
        const auto resolved = run(invoke("bn:cancel", {.screen_id = "screen:read:boundary:old"}));
        REQUIRE_FALSE(resolved);
        CHECK(resolved.error() == engine_client::error::stale_screen);
    }
    SECTION("a boundary expectation names rows and checks the boundary") {
        const auto resolved = run(
            invoke("bn:choose:list:choices", boundary_form, {.row = "choice:yes"}));
        REQUIRE(resolved);
        REQUIRE(resolved->interaction);
        CHECK(resolved->interaction->target_id == "choice:yes");
        const auto stale = run(
            invoke("bn:choose:list:choices", {.revision = 1, .boundary_id = "boundary:old"},
                   {.row = "choice:yes"}));
        REQUIRE_FALSE(stale);
        CHECK(stale.error() == engine_client::error::stale_boundary);
    }
    SECTION("the wrong form for the scope is invalid_arguments") {
        const auto row_on_screen = run(
            invoke("bn:choose:list:choices", screen_form, {.row = "choice:yes"}));
        REQUIRE_FALSE(row_on_screen);
        CHECK(row_on_screen.error() == engine_client::error::invalid_arguments);
        const auto cancel_on_boundary = run(invoke("bn:cancel", boundary_form));
        REQUIRE_FALSE(cancel_on_boundary);
        CHECK(cancel_on_boundary.error() == engine_client::error::invalid_arguments);
    }
    SECTION("a screen expectation on a 0.1 operation is invalid_arguments") {
        auto request = choose_request(boundary, "choice:yes");
        request.expect.screen_id = screen_id;
        const auto resolved = run(request);
        REQUIRE_FALSE(resolved);
        CHECK(resolved.error() == engine_client::error::invalid_arguments);
    }
    SECTION("an unknown affordance is named after the expectation holds") {
        const auto resolved = run(invoke("bn:nothing", screen_form));
        REQUIRE_FALSE(resolved);
        CHECK(resolved.error() == engine_client::error::unknown_affordance);
    }
}

namespace {
auto invoke_request(const std::string& screen_id, const std::string& affordance = "action:any")
    -> engine_client::command_request {
    return {.epoch = "epoch:test",
            .expect = {.screen_id = screen_id},
            .operation = engine_client::invoke_operation{.affordance_id = affordance}};
}
} // namespace

TEST_CASE("screen-scoped invokes wait behind the running command", "[engine_client_contract]") {
    auto context = input_context{"YESNO"};
    context.register_action("YES");
    const auto input_scope = game_client::input_context_scope{context, "YESNO"};
    const auto scope = game_client::interaction_scope{
        context, [] {
            return game_client::interaction_snapshot{
                .kind = game_client::interaction_kind::choices,
                .allow_cancel = true,
                .choices = {{.id = "choice:yes", .label = "Yes"}},
            };
        }};
    game_client::begin_input_boundary();
    const auto boundary = capture();
    auto authority = test_authority{};
    auto lifecycle = engine_client::command_lifecycle{"epoch:test", authority};

    SECTION("nothing waits while nothing runs") {
        const auto idle = lifecycle.enqueue(invoke_request("screen:1"));
        REQUIRE_FALSE(idle);
        CHECK(idle.error() == engine_client::error::invalid_lifecycle);
        CHECK_FALSE(lifecycle.promote());
    }
    SECTION("only an invoke that expects a screen waits") {
        REQUIRE(lifecycle.submit(choose_request(boundary, "choice:yes")));
        const auto cancel = lifecycle.enqueue(choose_request(boundary, "choice:yes"));
        REQUIRE_FALSE(cancel);
        CHECK(cancel.error() == engine_client::error::command_busy);
        auto by_boundary = invoke_request("screen:1");
        by_boundary.expect = {.revision = 1, .boundary_id = boundary.id};
        const auto boundary_scoped = lifecycle.enqueue(by_boundary);
        REQUIRE_FALSE(boundary_scoped);
        CHECK(boundary_scoped.error() == engine_client::error::command_busy);
        // The same request is refused outright when submitted while busy.
        const auto busy = lifecycle.submit(invoke_request("screen:1"));
        REQUIRE_FALSE(busy);
        CHECK(busy.error() == engine_client::error::command_busy);
    }
    SECTION("a waiting command keeps its receipt and takes over in order") {
        const auto running = lifecycle.submit(choose_request(boundary, "choice:yes"));
        REQUIRE(running);
        const auto first = lifecycle.enqueue(invoke_request("screen:1", "action:first"));
        const auto second = lifecycle.enqueue(invoke_request("screen:1", "action:second"));
        REQUIRE(first);
        REQUIRE(second);
        CHECK(first->stage == engine_client::command_stage::received);
        CHECK(first->command_id != running->command_id);
        CHECK(first->command_id != second->command_id);
        CHECK(
            lifecycle.result(second->command_id)->stage == engine_client::command_stage::received);
        // The running command is still the one that is validated, and nothing takes over before it
        // ends.
        CHECK_FALSE(lifecycle.promote());
        REQUIRE(lifecycle.interrupt(engine_client::error::not_ready));
        const auto next = lifecycle.promote();
        REQUIRE(next);
        CHECK(next->result.command_id == first->command_id);
        CHECK(std::get<engine_client::invoke_operation>(next->request.operation).affordance_id
              == "action:first");
        CHECK(lifecycle.busy());
        CHECK_FALSE(lifecycle.promote());
        REQUIRE(lifecycle.interrupt(engine_client::error::not_ready));
        const auto last = lifecycle.promote();
        REQUIRE(last);
        CHECK(last->result.command_id == second->command_id);
        REQUIRE(lifecycle.interrupt(engine_client::error::not_ready));
        CHECK_FALSE(lifecycle.promote());
    }
    SECTION("a waiting command is validated when it runs, against the screen then on top") {
        REQUIRE(lifecycle.submit(choose_request(boundary, "choice:yes")));
        const auto waiting = lifecycle.enqueue(invoke_request("screen:closed"));
        REQUIRE(waiting);
        REQUIRE(lifecycle.interrupt(engine_client::error::not_ready));
        REQUIRE(lifecycle.promote());
        const auto rejected = lifecycle.validate(at_of(boundary), boundary, point{80, 24});
        REQUIRE_FALSE(rejected);
        CHECK(rejected.error() == engine_client::error::stale_screen);
        CHECK(lifecycle.result(waiting->command_id)->failure == engine_client::error::stale_screen);
        CHECK_FALSE(lifecycle.busy());
    }
    SECTION("the queue holds eight") {
        REQUIRE(lifecycle.submit(choose_request(boundary, "choice:yes")));
        for (auto i = std::size_t{0}; i < engine_client::maximum_queued_commands; ++i) {
            REQUIRE(lifecycle.enqueue(invoke_request("screen:1")));
        }
        const auto over = lifecycle.enqueue(invoke_request("screen:1"));
        REQUIRE_FALSE(over);
        CHECK(over.error() == engine_client::error::command_busy);
    }
    SECTION("draining ends every waiting command and keeps the running one") {
        REQUIRE(lifecycle.submit(choose_request(boundary, "choice:yes")));
        const auto first = lifecycle.enqueue(invoke_request("screen:1"));
        const auto second = lifecycle.enqueue(invoke_request("screen:1"));
        REQUIRE(first);
        REQUIRE(second);
        const auto ended = lifecycle.drain(engine_client::error::stale_epoch);
        REQUIRE(ended.size() == 2);
        CHECK(ended[0].command_id == first->command_id);
        CHECK(ended[1].command_id == second->command_id);
        for (const auto& result : ended) {
            CHECK(result.stage == engine_client::command_stage::interrupted);
            CHECK(result.failure == engine_client::error::stale_epoch);
        }
        CHECK(lifecycle.busy());
        CHECK_FALSE(lifecycle.promote());
        CHECK_FALSE(lifecycle.result(first->command_id));
        CHECK(lifecycle.drain(engine_client::error::stale_epoch).empty());
    }
}
