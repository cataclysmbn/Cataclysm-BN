#include "catch/catch.hpp"
#include "client_input.h"
#include "engine_client_contract.h"
#include "input.h"

#include <algorithm>
#include <climits>
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
                .panes = {{.id = "pane:left", .label = "Left", .filter = "rock"}},
                .choices = {{.id = "choice:yes", .label = "Yes", .highlighted = true}},
            };
        }};
    game_client::begin_input_boundary();
    const auto wire = engine_client::serialize_boundary(capture());
    CHECK(contains(wire, R"("choice_total":1)"));
    CHECK(contains(
        wire, R"("compat":{"focus":{"choice_id":"choice:yes"},"panes":[{"id":"pane:left")"));
    CHECK(contains(wire, R"("filter":"rock")"));
    for (const auto* removed :
         {"structured", "actions_only", "choice_page", "highlighted", "projection", "input_id",
          "bubble_ms", "frame_id"}) {
        CHECK_FALSE(contains(wire, removed));
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
