#include "avatar.h"
#include "catch/catch.hpp"
#include "client_input.h"
#include "engine_client_contract.h"
#include "engine_client_event.h"
#include "input.h"
#include "json.h"
#include "rng.h"

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <ranges>
#include <sstream>
#include <string>
#include <utility>

namespace {
auto make_stream(engine_client::snapshot snapshot) -> engine_client::event_stream {
    auto created = engine_client::event_stream::create(std::move(snapshot));
    REQUIRE(created);
    return std::move(*created);
}
auto check_wire_sample(const std::string& value) -> void { CHECK_FALSE(value.empty()); }
auto check_command_response(const engine_client::command_result& value) -> void {
    const auto encoded = engine_client::serialize_command_response(value, {});
    REQUIRE(encoded);
    check_wire_sample(*encoded);
}
class test_authority final: public engine_client::command_authority {
public:
    engine_client::command_permissions policy =
        {.accepts_interaction_commands = true, .accepts_registered_actions = true};
    auto current_permissions() const -> engine_client::command_permissions override {
        return policy;
    }
};
auto ready() -> engine_client::readiness {
    return {
        .phase = "menu", .accepts_interaction_commands = true, .accepts_registered_actions = true};
}
auto capture() -> engine_client::state_value {
    const auto value = engine_client::capture_state(
        {.session_epoch = "epoch:test", .ready = ready()});
    REQUIRE(value);
    return *value;
}
auto choose(const std::string& id) -> game_client::interaction_command {
    auto command = game_client::interaction_command{};
    command.operation = game_client::interaction_operation::choose;
    command.target_id = id;
    return command;
}
auto named_choice(const std::string& id, const std::string& label)
    -> game_client::interaction_choice {
    auto value = game_client::interaction_choice{};
    value.id = id;
    value.label = label;
    return value;
}
auto request_for(
    const engine_client::snapshot& observed, const game_client::interaction_command& command)
    -> engine_client::command_request {
    return {
        .session_epoch = observed.session_epoch,
        .state_revision = observed.state_revision,
        .input_boundary_id = observed.state.input_boundary_id,
        .interaction_schema_id = observed.state.interaction->schema_id,
        .operation = engine_client::semantic_operation{.command = command}};
}
auto actor_state() -> std::string {
    auto output = std::ostringstream{};
    auto json = JsonOut{output};
    get_avatar().serialize(json);
    return output.str();
}
} // namespace

TEST_CASE(
    "BN exact version and capabilities are independent from MCP", "[engine_client_contract]") {
    const auto selected = engine_client::negotiate({
        .supported_versions = {"2025-03-26", "1.0"},
        .required_capabilities = {"snapshot.readiness", "snapshot.interaction"},
        .optional_capabilities =
            {"delivery.push", "snapshot.readiness", "events.interaction_replaced"},
    });
    REQUIRE(selected);
    CHECK(selected->capabilities
          == std::vector<std::string>{
              "snapshot.readiness", "snapshot.interaction", "events.interaction_replaced"});
    CHECK_FALSE(engine_client::negotiate({.supported_versions = {"1", "1.1", "2.0"}}));
    const auto unsupported = engine_client::negotiate(
        {.supported_versions = {"1.0"}, .required_capabilities = {"delivery.reconnect"}});
    REQUIRE_FALSE(unsupported);
    CHECK(unsupported.error() == engine_client::error::unsupported_capability);
    const auto rng = rng_get_engine();
    const auto epoch = engine_client::new_session_epoch();
    REQUIRE(epoch);
    CHECK_FALSE(epoch->empty());
    CHECK(rng_get_engine() == rng);
    const auto encoded = engine_client::serialize_negotiation(*selected, *epoch);
    CHECK(encoded.find(R"("contract_version":"1.0")") != std::string::npos);
    CHECK(encoded.find(R"("history_supported":false)") != std::string::npos);
    check_wire_sample(encoded);
}

TEST_CASE(
    "contract receipt validation delivery and boundary completion are separate",
    "[engine_client_contract]") {
    auto context = input_context{"YESNO"};
    context.register_action("YES");
    const auto input_scope = game_client::input_context_scope{context, "YESNO"};
    const auto interaction_scope = game_client::interaction_scope{
        context, []() {
            return game_client::interaction_snapshot{
                .kind = game_client::interaction_kind::choices,
                .allow_cancel = true,
                .choices = {{.id = "choice:yes", .label = "Yes"}},
            };
        }};
    game_client::begin_input_boundary();
    auto observed = engine_client::snapshot{.session_epoch = "epoch:test", .state = capture()};
    auto authority = test_authority{};
    auto lifecycle = engine_client::command_lifecycle{"epoch:test", authority};
    auto sink = engine_client::recording_event_sink{};
    auto stream = make_stream(observed);
    const auto request = request_for(observed, choose("choice:yes"));
    const auto received = lifecycle.submit(request);
    REQUIRE(received);
    CHECK(received->stage == engine_client::command_stage::received);
    CHECK_FALSE(received->validation_succeeded);
    CHECK_FALSE(received->execution_started);
    check_command_response(*received);
    CHECK_FALSE(lifecycle.execution_started());
    CHECK_FALSE(lifecycle.complete_at_boundary(observed));
    CHECK_FALSE(lifecycle.submit(request));
    const auto resolved = lifecycle.validate(observed, point{80, 24});
    REQUIRE(resolved);
    auto native = game_client::input_command{};
    native.interaction = std::get<engine_client::semantic_operation>(request.operation).command;
    native.interaction->input_id = game_client::current_input_id();
    const auto legacy = game_client::resolve_input_command(native, point{80, 24});
    REQUIRE(legacy);
    CHECK(resolved->type == legacy->type);
    CHECK(resolved->interaction == legacy->interaction);
    CHECK(lifecycle.result(received->command_id)->stage == engine_client::command_stage::validated);
    check_command_response(*lifecycle.result(received->command_id));
    CHECK_FALSE(lifecycle.validate(observed, point{80, 24})); // Cannot resolve or deliver twice.
    REQUIRE(lifecycle.execution_started());
    CHECK(lifecycle.result(received->command_id)->stage == engine_client::command_stage::executing);
    check_command_response(*lifecycle.result(received->command_id));
    CHECK_FALSE(lifecycle.execution_started());
    CHECK_FALSE(lifecycle.complete_at_boundary(observed));
    game_client::begin_input_boundary();
    const auto delta = stream.replace(
        engine_client::disclosure::publish,
        {.state = capture(), .command_id = received->command_id}, sink);
    REQUIRE(delta);
    REQUIRE(*delta);
    REQUIRE(lifecycle.complete_at_boundary(stream.current_snapshot()));
    const auto completed = lifecycle.result(received->command_id);
    REQUIRE(completed);
    CHECK(completed->stage == engine_client::command_stage::completed);
    CHECK(completed->validation_succeeded);
    CHECK(completed->execution_started);
    REQUIRE(completed->completed);
    CHECK(completed->completed->state_revision == 1);
    CHECK(completed->completed->through_public_sequence == 1);
    const auto batch = sink.drain();
    const auto response = engine_client::serialize_command_response(*completed, batch);
    REQUIRE(response);
    check_wire_sample(*response);
    check_wire_sample(engine_client::serialize_batch(batch));
    check_wire_sample(engine_client::serialize_event(batch.events.front()));
    CHECK_FALSE(engine_client::serialize_command_response(*completed, {}));
    auto wrong_endpoint = *completed;
    wrong_endpoint.completed->through_public_sequence += 1;
    CHECK_FALSE(engine_client::serialize_command_response(wrong_endpoint, batch));
    auto resync = *completed;
    resync.completed->resync_required = true;
    const auto recoverable = engine_client::serialize_command_response(resync, {});
    REQUIRE(recoverable);
    check_wire_sample(*recoverable);
    CHECK_FALSE(engine_client::serialize_command_response(*received, batch));
    REQUIRE(engine_client::apply_batch(observed, batch));
    CHECK(engine_client::serialize_snapshot(observed)
          == engine_client::serialize_snapshot(stream.current_snapshot()));
    const auto next = lifecycle.submit(
        request_for(observed, {.operation = game_client::interaction_operation::cancel}));
    REQUIRE(next);
    CHECK_FALSE(lifecycle.result(received->command_id)); // No unbounded terminal history.
    REQUIRE(lifecycle.interrupt());
    CHECK(lifecycle.result(next->command_id)->stage == engine_client::command_stage::interrupted);
    check_command_response(*lifecycle.result(next->command_id));
}

TEST_CASE(
    "semantic validation captures the full live provider once without narrowing choices",
    "[engine_client_contract]") {
    auto context = input_context{"CONTRACT_SINGLE_CAPTURE"};
    const auto input_scope = game_client::input_context_scope{context, "CONTRACT_SINGLE_CAPTURE"};
    auto reads = 0;
    auto changed = false;
    const auto interaction_scope = game_client::interaction_scope{
        context, [&]() {
            ++reads;
            auto snapshot = game_client::interaction_snapshot{
                .kind = game_client::interaction_kind::choices,
                .title = changed ? "Changed native choices" : "Native choices",
                .allow_cancel = true,
            };
            for (const auto index : std::views::iota(0, 205)) {
                snapshot.choices.push_back(
                    named_choice(std::to_string(index), std::to_string(index)));
            }
            return snapshot;
        }};
    game_client::begin_input_boundary();
    const auto observed =
        engine_client::snapshot{.session_epoch = "epoch:test", .state = capture()};
    REQUIRE(observed.state.interaction->choices.size() == 100);
    const auto before = actor_state();
    const auto rng = rng_get_engine();
    auto request = request_for(observed, choose("204"));
    auto expected = std::optional<engine_client::error>{};
    auto expected_reads = 1;
    auto authority = test_authority{};
    SECTION("off-page choice uses the same complete native validation") {}
    SECTION("unknown choice is rejected after one capture") {
        std::get<engine_client::semantic_operation>(request.operation).command.target_id =
            "missing";
        expected = engine_client::error::validation_failed;
    }
    SECTION("changed live schema is rejected after one capture") {
        changed = true;
        expected = engine_client::error::stale_interaction_schema;
    }
    SECTION("published-schema mismatch does not invoke the provider") {
        request.interaction_schema_id = "schema:stale";
        expected = engine_client::error::stale_interaction_schema;
        expected_reads = 0;
    }
    SECTION("live permission denial does not invoke the provider") {
        authority.policy.accepts_interaction_commands = false;
        expected = engine_client::error::not_ready;
        expected_reads = 0;
    }
    auto lifecycle = engine_client::command_lifecycle{"epoch:test", authority};
    REQUIRE(lifecycle.submit(request));
    reads = 0;
    const auto resolved = lifecycle.validate(observed, point{80, 24});
    CHECK(reads == expected_reads);
    if (expected) {
        REQUIRE_FALSE(resolved);
        CHECK(resolved.error() == *expected);
    } else {
        REQUIRE(resolved);
        REQUIRE(resolved->interaction);
        CHECK(resolved->interaction->target_id == "204");
    }
    CHECK(actor_state() == before);
    CHECK(rng_get_engine() == rng);
}

TEST_CASE(
    "command overflow recovery completes against a newly committed resynchronization snapshot",
    "[engine_client_contract]") {
    auto context = input_context{"CONTRACT_RESYNC"};
    const auto input_scope = game_client::input_context_scope{context, "CONTRACT_RESYNC"};
    const auto interaction_scope = game_client::interaction_scope{
        context, []() {
            return game_client::interaction_snapshot{
                .kind = game_client::interaction_kind::choices,
                .allow_cancel = true,
                .choices = {{.id = "choice:yes", .label = "Yes"}},
            };
        }};
    game_client::begin_input_boundary();
    auto stream = make_stream({.session_epoch = "epoch:test", .state = capture()});
    auto recorder = engine_client::recording_event_sink{};
    for ([[maybe_unused]] const auto index : std::views::iota(0, 8)) {
        game_client::begin_input_boundary();
        REQUIRE(stream.replace(engine_client::disclosure::publish, {.state = capture()}, recorder));
    }
    const auto observed = stream.current_snapshot();
    auto authority = test_authority{};
    auto lifecycle = engine_client::command_lifecycle{"epoch:test", authority};
    const auto received = lifecycle.submit(request_for(observed, choose("choice:yes")));
    REQUIRE(received);
    REQUIRE(lifecycle.validate(observed, point{80, 24}));
    REQUIRE(lifecycle.execution_started());
    game_client::begin_input_boundary();
    const auto next = capture();
    const auto overflow = stream.replace(
        engine_client::disclosure::publish, {.state = next, .command_id = received->command_id},
        recorder);
    REQUIRE_FALSE(overflow);
    CHECK(overflow.error() == engine_client::error::resource_limit);
    CHECK_FALSE(lifecycle.complete_at_boundary(stream.current_snapshot(), true));
    const auto fresh = stream.resynchronize(engine_client::disclosure::publish, next);
    REQUIRE(fresh);
    REQUIRE(*fresh);
    CHECK((**fresh).state_revision == 9);
    CHECK((**fresh).through_public_sequence == 8);
    REQUIRE(lifecycle.complete_at_boundary(stream.current_snapshot(), true));
    const auto result = lifecycle.result(received->command_id);
    REQUIRE(result);
    const auto response = engine_client::serialize_command_response(*result, {});
    REQUIRE(response);
    check_wire_sample(*response);
    check_wire_sample(engine_client::serialize_snapshot(**fresh));
    CHECK(recorder.events().size() == 8); // No silent prefix drop or fabricated event 9.
}

TEST_CASE(
    "stale invalid and unavailable commands do not execute or change authority",
    "[engine_client_contract]") {
    auto context = input_context{"YESNO"};
    const auto input_scope = game_client::input_context_scope{context, "YESNO"};
    const auto interaction_scope = game_client::interaction_scope{
        context, []() {
            return game_client::interaction_snapshot{
                .kind = game_client::interaction_kind::inventory,
                .allow_cancel = true,
                .choices =
                    {{.id = "choice:allowed", .label = "Allowed", .available_count = 2},
                     {.id = "choice:disabled",
                      .label = "Disabled",
                      .enabled = false,
                      .selectable = false}},
                .allow_set_count = true,
            };
        }};
    game_client::begin_input_boundary();
    const auto observed =
        engine_client::snapshot{.session_epoch = "epoch:test", .state = capture()};
    const auto before = actor_state();
    const auto rng = rng_get_engine();
    auto request = request_for(observed, choose("choice:allowed"));
    auto expected = engine_client::error::validation_failed;
    SECTION("wrong epoch") {
        request.session_epoch = "epoch:other";
        expected = engine_client::error::stale_epoch;
    }
    SECTION("wrong revision") {
        request.state_revision = 1;
        expected = engine_client::error::stale_revision;
    }
    SECTION("wrong boundary") {
        request.input_boundary_id = "boundary:old";
        expected = engine_client::error::stale_boundary;
    }
    SECTION("wrong schema") {
        request.interaction_schema_id = "schema:old";
        expected = engine_client::error::stale_interaction_schema;
    }
    SECTION("unknown choice") {
        std::get<engine_client::semantic_operation>(request.operation).command.target_id =
            "SECRET_SENTINEL";
    }
    SECTION("disabled choice") {
        std::get<engine_client::semantic_operation>(request.operation).command.target_id =
            "choice:disabled";
    }
    SECTION("unavailable quantity") {
        std::get<engine_client::semantic_operation>(request.operation).command =
            {.operation = game_client::interaction_operation::set_count,
             .target_id = "choice:allowed",
             .count = 3};
    }
    SECTION("live boundary changed without publication") {
        game_client::begin_input_boundary();
        expected = engine_client::error::stale_boundary;
    }
    auto authority = test_authority{};
    auto lifecycle = engine_client::command_lifecycle{"epoch:test", authority};
    const auto received = lifecycle.submit(request);
    REQUIRE(received);
    const auto rejected = lifecycle.validate(observed, point{80, 24});
    REQUIRE_FALSE(rejected);
    CHECK(rejected.error() == expected);
    const auto result = lifecycle.result(received->command_id);
    REQUIRE(result);
    CHECK(result->stage == engine_client::command_stage::rejected);
    CHECK_FALSE(result->execution_started);
    const auto encoded = engine_client::serialize_command_response(*result, {});
    REQUIRE(encoded);
    check_wire_sample(*encoded);
    CHECK(encoded->find("SECRET_SENTINEL") == std::string::npos);
    CHECK(actor_state() == before);
    CHECK(rng_get_engine() == rng);
}

TEST_CASE(
    "operation shape and target frames are checked before native resolution",
    "[engine_client_contract]") {
    auto context = input_context{"TARGET"};
    const auto input_scope = game_client::input_context_scope{context, "TARGET"};
    auto reads = 0;
    const auto interaction_scope = game_client::interaction_scope{
        context, [&]() {
            ++reads;
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
    const auto observed =
        engine_client::snapshot{.session_epoch = "epoch:test", .state = capture()};
    auto command =
        request_for(observed, {.operation = game_client::interaction_operation::set_target});
    auto& semantic = std::get<engine_client::semantic_operation>(command.operation);
    semantic.target = engine_client::coordinate{
        .space = "reality_bubble_map_square",
        .frame_id = observed.state.input_boundary_id,
        .position = {.x = 12, .y = 10}};
    SECTION("exact tagged frame maps to the native target") {
        auto authority = test_authority{};
        auto lifecycle = engine_client::command_lifecycle{"epoch:test", authority};
        REQUIRE(lifecycle.submit(command));
        reads = 0;
        const auto resolved = lifecycle.validate(observed, point{80, 24});
        CHECK(reads == 1);
        REQUIRE(resolved);
        REQUIRE(resolved->interaction);
        CHECK(resolved->interaction->position == semantic.target->position);
        const auto wire = engine_client::serialize_snapshot(observed);
        check_wire_sample(wire);
        CHECK(wire.find(R"("space":"reality_bubble_map_square")") != std::string::npos);
        CHECK(wire.find("bubble_ms") == std::string::npos);
        CHECK(wire.find("input_id") == std::string::npos);
    }
    SECTION("stale frame does not execute") {
        semantic.target->frame_id = "boundary:old";
        auto authority = test_authority{};
        auto lifecycle = engine_client::command_lifecycle{"epoch:test", authority};
        REQUIRE(lifecycle.submit(command));
        reads = 0;
        const auto rejected = lifecycle.validate(observed, point{80, 24});
        REQUIRE_FALSE(rejected);
        CHECK(rejected.error() == engine_client::error::stale_boundary);
        CHECK(reads == 0);
    }
    SECTION("absolute coordinates are not silently guessed") {
        semantic.target->space = "absolute_map_square";
        auto authority = test_authority{};
        auto lifecycle = engine_client::command_lifecycle{"epoch:test", authority};
        CHECK_FALSE(lifecycle.submit(command));
    }
    SECTION("mixed native position and framed position are malformed") {
        semantic.command.position = semantic.target->position;
        auto authority = test_authority{};
        auto lifecycle = engine_client::command_lifecycle{"epoch:test", authority};
        CHECK_FALSE(lifecycle.submit(command));
    }
    SECTION("native range validation is retained") {
        semantic.target->position.x = 19;
        auto authority = test_authority{};
        auto lifecycle = engine_client::command_lifecycle{"epoch:test", authority};
        REQUIRE(lifecycle.submit(command));
        CHECK_FALSE(lifecycle.validate(observed, point{80, 24}));
    }
}

TEST_CASE(
    "capture row bounds and first repeated reads preserve bound authority and RNG",
    "[engine_client_contract]") {
    auto context = input_context{"DEFAULTMODE"};
    context.register_action("UP");
    const auto input_scope = game_client::input_context_scope{context, "DEFAULTMODE"};
    const auto before = actor_state();
    const auto rng = rng_get_engine();
    const auto first = capture();
    const auto second = capture();
    CHECK(engine_client::same_state(first, second));
    CHECK_FALSE(first.interaction);
    REQUIRE(first.actions.size() == 1);
    CHECK(first.actions.front().id == "UP");
    CHECK_FALSE(
        engine_client::capture_state({.session_epoch = "epoch:test", .page = {.limit = 201}}));
    CHECK_FALSE(
        engine_client::capture_state({.session_epoch = "epoch:test", .page = {.limit = 0}}));
    CHECK(actor_state() == before);
    CHECK(rng_get_engine() == rng);
    auto observed = engine_client::snapshot{.session_epoch = "epoch:test", .state = first};
    auto authority = test_authority{};
    auto lifecycle = engine_client::command_lifecycle{"epoch:test", authority};
    const auto request = engine_client::command_request{
        .session_epoch = observed.session_epoch,
        .input_boundary_id = first.input_boundary_id,
        .operation = engine_client::registered_action{.id = "UP"},
    };
    auto oversized_identity = request;
    oversized_identity.interaction_schema_id = std::string(257, 'x');
    CHECK_FALSE(lifecycle.submit(oversized_identity));
    authority.policy.accepts_registered_actions = false;
    REQUIRE(lifecycle.submit(request));
    const auto denied = lifecycle.validate(observed, point{80, 24});
    REQUIRE_FALSE(denied);
    CHECK(denied.error() == engine_client::error::not_ready);
    observed.state.ready.accepts_registered_actions = false; // Stale published flag is not
                                                             // authority.
    authority.policy.accepts_registered_actions = true;
    REQUIRE(lifecycle.submit(request));
    REQUIRE(lifecycle.validate(observed, point{80, 24}));
    CHECK(actor_state() == before);
    CHECK(rng_get_engine() == rng);
}

TEST_CASE(
    "bounded pages keep schema identity and native field quantity and selection semantics",
    "[engine_client_contract]") {
    auto context = input_context{"CONTRACT_FIELDS"};
    const auto input_scope = game_client::input_context_scope{context, "CONTRACT_FIELDS"};
    auto source = game_client::interaction_snapshot{
        .kind = game_client::interaction_kind::inventory,
        .allow_cancel = true,
        .panes = {{.id = "pane:native", .label = "Actual pane", .role = "source"}},
        .field =
            game_client::interaction_field{
                .id = "field:native",
                .label = "Quantity",
                .description = "Native field",
                .value = "1",
                .type = "integer",
                .max_length = 3,
                .printable = true},
        .allow_set_count = true,
    };
    for (const auto index : std::views::iota(0, 205)) {
        source.choices.push_back({
            .id = "choice:" + std::to_string(index),
            .label = "Choice",
            .description = "Native description",
            .denial = "Native denial",
            .pane_id = "pane:native",
            .area_id = "area:native",
            .storage_kind = "inventory",
            .enabled = false,
            .selectable = true,
            .selected = true,
            .highlighted = false,
            .columns = {{.label = "Count", .value = "2"}},
            .selected_count = 1,
            .minimum_count = 0,
            .available_count = 2,
        });
    }
    const auto interaction_scope =
        game_client::interaction_scope{context, [&]() { return source; }};
    game_client::begin_input_boundary();
    const auto first = engine_client::capture_state(
        {.session_epoch = "epoch:test", .ready = ready(), .page = {.offset = 0, .limit = 200}});
    const auto second = engine_client::capture_state(
        {.session_epoch = "epoch:test", .ready = ready(), .page = {.offset = 200, .limit = 200}});
    REQUIRE(first);
    REQUIRE(second);
    REQUIRE(first->interaction);
    REQUIRE(second->interaction);
    CHECK(first->interaction->choices.size() == 200);
    CHECK(second->interaction->choices.size() == 5);
    CHECK(first->interaction->choice_total == 205);
    CHECK(second->interaction->choice_offset == 200);
    CHECK(first->interaction->schema_id == second->interaction->schema_id);
    auto observed = engine_client::snapshot{.session_epoch = "epoch:test", .state = *first};
    check_wire_sample(engine_client::serialize_snapshot(observed));
    const auto before = actor_state();
    const auto rng = rng_get_engine();
    auto authority = test_authority{};
    auto lifecycle = engine_client::command_lifecycle{"epoch:test", authority};
    auto malformed = request_for(
        observed,
        {.operation = game_client::interaction_operation::fill,
         .target_id = "field:native",
         .value = "12"});
    CHECK_FALSE(lifecycle.submit(malformed)); // Missing submit is not a receipt.
    CHECK_FALSE(lifecycle.result("command:unknown"));
    std::get<engine_client::semantic_operation>(malformed.operation).command.value =
        std::string(262145, '1');
    CHECK_FALSE(lifecycle.submit(malformed));
    REQUIRE(lifecycle.submit(request_for(
        observed,
        {.operation = game_client::interaction_operation::fill,
         .target_id = "field:native",
         .value = "12",
         .submit = false})));
    const auto field = lifecycle.validate(observed, point{80, 24});
    REQUIRE(field);
    REQUIRE(field->interaction);
    CHECK(field->interaction->submit == false);
    CHECK(field->interaction->value == "12");
    REQUIRE(lifecycle.interrupt());
    REQUIRE(lifecycle.submit(request_for(observed, choose("choice:0"))));
    CHECK(lifecycle.validate(observed, point{80, 24})); // selectable=true, even with enabled=false.
    REQUIRE(lifecycle.interrupt());
    for (const auto quantity : {0, 2}) {
        REQUIRE(lifecycle.submit(request_for(
            observed,
            {.operation = game_client::interaction_operation::set_count,
             .target_id = "choice:0",
             .count = static_cast<std::uint64_t>(quantity)})));
        CHECK(lifecycle.validate(observed, point{80, 24}));
        REQUIRE(lifecycle.interrupt());
    }
    source.choices.front().label = "changed schema";
    REQUIRE(lifecycle.submit(request_for(observed, choose("choice:0"))));
    const auto stale = lifecycle.validate(observed, point{80, 24});
    REQUIRE_FALSE(stale);
    CHECK(stale.error() == engine_client::error::stale_interaction_schema);
    CHECK(actor_state() == before);
    CHECK(rng_get_engine() == rng);
}

TEST_CASE(
    "validation uses current engine policy rather than matching published readiness",
    "[engine_client_contract]") {
    auto context = input_context{"DEFAULTMODE"};
    context.register_action("UP");
    const auto input_scope = game_client::input_context_scope{context, "DEFAULTMODE"};
    const auto interaction_scope = game_client::interaction_scope{
        context, []() {
            auto native = game_client::interaction_snapshot{};
            native.kind = game_client::interaction_kind::choices;
            native.choices.push_back(named_choice("choice:live", "Live"));
            return native;
        }};
    game_client::begin_input_boundary();
    const auto before = actor_state();
    const auto rng = rng_get_engine();
    auto published = engine_client::snapshot{.session_epoch = "epoch:test", .state = capture()};
    for (const auto semantic : {false, true}) {
        for (const auto allowed : {false, true}) {
            auto authority = test_authority{};
            authority.policy =
                {.accepts_interaction_commands = !allowed, .accepts_registered_actions = !allowed};
            published.state.ready.accepts_interaction_commands = !allowed;
            published.state.ready.accepts_registered_actions = !allowed;
            auto request = request_for(published, choose("choice:live"));
            if (!semantic) { request.operation = engine_client::registered_action{.id = "UP"}; }
            auto lifecycle = engine_client::command_lifecycle{"epoch:test", authority};
            const auto receipt = lifecycle.submit(request);
            REQUIRE(receipt);
            CHECK(receipt->session_epoch == "epoch:test");
            authority.policy =
                {.accepts_interaction_commands = allowed, .accepts_registered_actions = allowed};
            const auto resolved = lifecycle.validate(published, point{80, 24});
            CHECK(resolved.has_value() == allowed);
            if (!allowed) { CHECK(resolved.error() == engine_client::error::not_ready); }
            CHECK_FALSE(lifecycle.result(receipt->command_id)->execution_started);
            CHECK(actor_state() == before);
            CHECK(rng_get_engine() == rng);
        }
    }
}

TEST_CASE(
    "page switches materialize one session clock across command replacement and reconnection",
    "[engine_client_contract][engine_client_event]") {
    auto context = input_context{"CONTRACT_PROJECTIONS"};
    const auto input_scope = game_client::input_context_scope{context, "CONTRACT_PROJECTIONS"};
    auto source = game_client::interaction_snapshot{};
    source.kind = game_client::interaction_kind::choices;
    for (const auto index : std::views::iota(0, 205)) {
        source.choices.push_back(named_choice("choice:" + std::to_string(index), "Actual choice"));
    }
    const auto interaction_scope =
        game_client::interaction_scope{context, [&]() { return source; }};
    const auto page = [&](const std::size_t offset) {
        const auto value = engine_client::capture_state(
            {.session_epoch = "epoch:test",
             .ready = ready(),
             .page = {.offset = offset, .limit = 200}});
        REQUIRE(value);
        return *value;
    };
    game_client::begin_input_boundary();
    const auto before = actor_state();
    const auto rng = rng_get_engine();
    auto stream = make_stream({.session_epoch = "epoch:test", .state = page(0)});
    auto recorder = engine_client::recording_event_sink{};
    const auto second_page = page(200);
    const auto switched = stream.project_snapshot(second_page);
    REQUIRE(switched);
    CHECK(switched->state.view.offset == 200);
    CHECK(switched->state.view.limit == 200);
    CHECK(switched->state.interaction->choices.front().id == "choice:200");
    CHECK(switched->state_revision == 0);
    CHECK(switched->through_public_sequence == 0);
    const auto passive =
        stream.replace(engine_client::disclosure::publish, {.state = second_page}, recorder);
    REQUIRE(passive);
    CHECK_FALSE(*passive);
    CHECK(recorder.events().empty());
    CHECK(stream.current_snapshot().state.view.offset == 0);
    CHECK_FALSE(stream.resynchronize(engine_client::disclosure::publish, second_page));
    check_wire_sample(engine_client::serialize_snapshot(*switched));
    REQUIRE(stream.project_snapshot(page(0)));
    auto authority = test_authority{};
    auto lifecycle = engine_client::command_lifecycle{"epoch:test", authority};
    const auto receipt = lifecycle.submit(request_for(*switched, choose("choice:202")));
    REQUIRE(receipt);
    const auto resolved = lifecycle.validate(stream.current_snapshot(), point{80, 24});
    REQUIRE(resolved);
    REQUIRE(resolved->interaction);
    CHECK(resolved->interaction->target_id == "choice:202");
    REQUIRE(lifecycle.execution_started());
    source.choices[202].selected = true;
    game_client::begin_input_boundary(); // Real native boundary, not a page read.
    const auto published = stream.replace(
        engine_client::disclosure::publish, {.state = page(0), .command_id = receipt->command_id},
        recorder);
    REQUIRE(published);
    REQUIRE(*published);
    const auto projected = engine_client::project_event(**published, page(200));
    REQUIRE(projected);
    CHECK(projected->value().event_id == (**published).value().event_id);
    CHECK(projected->value().public_sequence == 1);
    CHECK(projected->value().state_revision == 1);
    CHECK(projected->value().payload.state.view.offset == 200);
    auto wrong_view = *switched;
    CHECK_FALSE(engine_client::apply_event(wrong_view, **published));
    CHECK(wrong_view.state_revision == 0);
    auto rebuilt = *switched;
    REQUIRE(engine_client::apply_event(rebuilt, *projected));
    const auto reconnected = stream.project_snapshot(page(200));
    REQUIRE(reconnected);
    CHECK(engine_client::serialize_snapshot(rebuilt)
          == engine_client::serialize_snapshot(*reconnected));
    const auto negotiated = engine_client::negotiate(
        {.supported_versions = {"1.0"},
         .required_capabilities =
             {"snapshot.readiness", "snapshot.actions", "snapshot.interaction"}});
    REQUIRE(negotiated); // Renegotiation neither creates a stream nor changes the clock.
    CHECK(stream.current_snapshot().state_revision == 1);
    CHECK(stream.current_snapshot().through_public_sequence == 1);
    CHECK(reconnected->session_epoch == switched->session_epoch);
    REQUIRE(lifecycle.complete_at_boundary(stream.current_snapshot()));
    const auto result = lifecycle.result(receipt->command_id);
    REQUIRE(result);
    const auto response = engine_client::serialize_command_response(
        *result, {.first_sequence = 1, .last_sequence = 1, .events = {*projected}});
    REQUIRE(response);
    check_wire_sample(*response);
    check_wire_sample(engine_client::serialize_snapshot(*reconnected));
    const auto stale_projection = engine_client::project_event(**published, second_page);
    CHECK_FALSE(stale_projection); // Captures from the previous boundary cannot be relabeled.
    auto invalid = page(200);
    invalid.interaction->schema_id = "schema:foreign";
    CHECK_FALSE(stream.project_snapshot(invalid));
    CHECK_FALSE(engine_client::project_event(**published, invalid));

    // A real boundary change must not permit changing the session's reference projection.
    REQUIRE(recorder.events().size() == 1);
    const auto retained = engine_client::serialize_event(recorder.events().front());
    const auto committed = engine_client::serialize_snapshot(stream.current_snapshot());
    game_client::begin_input_boundary();
    const auto wrong_reference = page(200);
    const auto rejected =
        stream.replace(engine_client::disclosure::publish, {.state = wrong_reference}, recorder);
    REQUIRE_FALSE(rejected);
    CHECK(rejected.error() == engine_client::error::stale_boundary);
    const auto rejected_recovery =
        stream.resynchronize(engine_client::disclosure::publish, wrong_reference);
    REQUIRE_FALSE(rejected_recovery);
    CHECK(rejected_recovery.error() == engine_client::error::stale_boundary);
    CHECK(engine_client::serialize_snapshot(stream.current_snapshot()) == committed);
    CHECK(stream.current_snapshot().state.view.offset == 0);
    CHECK(stream.current_snapshot().state_revision == 1);
    CHECK(stream.current_snapshot().through_public_sequence == 1);
    REQUIRE(recorder.events().size() == 1);
    CHECK(engine_client::serialize_event(recorder.events().front()) == retained);
    const auto valid_reference =
        stream.replace(engine_client::disclosure::publish, {.state = page(0)}, recorder);
    REQUIRE(valid_reference);
    REQUIRE(*valid_reference);
    CHECK((**valid_reference).value().public_sequence == 2);
    CHECK((**valid_reference).value().state_revision == 2);
    CHECK(stream.current_snapshot().state.view.offset == 0);
    REQUIRE(recorder.events().size() == 2);
    CHECK(engine_client::serialize_event(recorder.events().front()) == retained);
    const auto requested = engine_client::project_event(**valid_reference, page(200));
    REQUIRE(requested);
    REQUIRE(engine_client::apply_event(rebuilt, *requested));
    const auto fresh_requested = stream.project_snapshot(page(200));
    REQUIRE(fresh_requested);
    CHECK(engine_client::serialize_snapshot(rebuilt)
          == engine_client::serialize_snapshot(*fresh_requested));
    CHECK(actor_state() == before);
    CHECK(rng_get_engine() == rng);
}

TEST_CASE(
    "capture and native offsets obey actual wire precision boundaries",
    "[engine_client_contract]") {
    auto context = input_context{"CONTRACT_PRECISION"};
    const auto input_scope = game_client::input_context_scope{context, "CONTRACT_PRECISION"};
    auto reads = 0;
    const auto interaction_scope = game_client::interaction_scope{
        context, [&]() {
            ++reads;
            auto native = game_client::interaction_snapshot{};
            native.kind = game_client::interaction_kind::choices;
            native.choices.push_back(named_choice("choice:one", "One"));
            return native;
        }};
    game_client::begin_input_boundary();
    const auto maximum = engine_client::maximum_safe_integer;
    const auto accepted = engine_client::capture_state(
        {.session_epoch = "epoch:test",
         .ready = ready(),
         .page = {.offset = static_cast<std::size_t>(maximum), .limit = 200}});
    REQUIRE(accepted);
    CHECK(accepted->view.offset == maximum);
    CHECK(accepted->interaction->choice_offset == 1); // Native clamp is unchanged.
    check_wire_sample(
        engine_client::serialize_snapshot({.session_epoch = "epoch:test", .state = *accepted}));
    const auto before = reads;
    const auto oversized = engine_client::capture_state(
        {.session_epoch = "epoch:test",
         .ready = ready(),
         .page = {.offset = static_cast<std::size_t>(maximum + 1), .limit = 200}});
    REQUIRE_FALSE(oversized);
    CHECK(oversized.error() == engine_client::error::resource_limit);
    CHECK(reads == before); // Reject requested overflow before calling a native provider.
    auto native_boundary = *accepted;
    native_boundary.interaction->choice_offset = static_cast<std::size_t>(maximum);
    native_boundary.interaction->choice_total = static_cast<std::size_t>(maximum);
    REQUIRE(engine_client::validate_state(native_boundary));
    const auto valid = engine_client::event_stream::create(
        {.session_epoch = "epoch:test", .state = native_boundary});
    REQUIRE(valid);
    check_wire_sample(engine_client::serialize_snapshot(valid->current_snapshot()));
    native_boundary.interaction->choice_offset = static_cast<std::size_t>(maximum + 1);
    const auto bad = engine_client::validate_state(native_boundary);
    REQUIRE_FALSE(bad);
    CHECK(bad.error() == engine_client::error::resource_limit);
    CHECK_FALSE(engine_client::event_stream::create(
        {.session_epoch = "epoch:test", .state = native_boundary}));
}

#if defined(CATA_MCP)
#    include "client_memory.h"
#    include "cursesdef.h"
#    include "output.h"
#    include "popup.h"
#    include "ui.h"

namespace {
struct widget_guard {
    bool old_test_mode = test_mode;
    int old_termx = TERMX;
    int old_termy = TERMY;
    int old_width = FULL_SCREEN_WIDTH;
    int old_height = FULL_SCREEN_HEIGHT;
    point old_screen_size = game_client::memory::screen_size();
    catacurses::window old_stdscr = catacurses::stdscr;
    catacurses::window old_newscr = catacurses::newscr;
    widget_guard() {
        test_mode = false;
        TERMX = 100;
        TERMY = 40;
        FULL_SCREEN_WIDTH = 80;
        FULL_SCREEN_HEIGHT = 24;
        game_client::memory::resize(TERMX, TERMY);
        catacurses::stdscr = catacurses::newwin(TERMY, TERMX, point_zero);
        catacurses::newscr = catacurses::newwin(TERMY, TERMX, point_zero);
    }
    ~widget_guard() {
        game_client::memory::set_input_provider({});
        if (old_screen_size.x == 0 || old_screen_size.y == 0) {
            game_client::memory::shutdown();
        } else {
            game_client::memory::resize(old_screen_size.x, old_screen_size.y);
        }
        catacurses::stdscr = old_stdscr;
        catacurses::newscr = old_newscr;
        TERMX = old_termx;
        TERMY = old_termy;
        FULL_SCREEN_WIDTH = old_width;
        FULL_SCREEN_HEIGHT = old_height;
        test_mode = old_test_mode;
    }
};
auto run_native_slice(engine_client::event_sink& sink) -> void {
    const auto guard = widget_guard{};
    auto menu = uilist{};
    menu.title = "Real contract interaction";
    menu.allow_disabled = false;
    menu.entries.emplace_back(10, true, MENU_AUTOASSIGN, "show enabled", "actual description");
    menu.entries.emplace_back(20, false, MENU_AUTOASSIGN, "show disabled", "native denial");
    menu.entries.emplace_back(30, true, MENU_AUTOASSIGN, "filtered", "hidden description");
    menu.set_filter("show");
    auto authority = test_authority{};
    auto lifecycle = engine_client::command_lifecycle{"epoch:test", authority};
    auto stream = std::optional<engine_client::event_stream>{};
    auto received_id = std::string{};
    auto reads = 0;
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto before = actor_state();
        const auto rng = rng_get_engine();
        const auto first = capture(); // Baseline is BEFORE the first description query.
        CHECK(actor_state() == before);
        CHECK(rng_get_engine() == rng);
        CHECK(engine_client::same_state(first, capture()));
        CHECK(actor_state() == before);
        CHECK(rng_get_engine() == rng);
        REQUIRE(first.interaction);
        check_wire_sample(
            engine_client::serialize_snapshot({.session_epoch = "epoch:test", .state = first}));
        if (reads++ == 0) {
            REQUIRE(first.interaction->choices.size() == 2);
            CHECK(first.interaction->choices.front().description == "actual description");
            stream.emplace(make_stream({.session_epoch = "epoch:test", .state = first}));
        } else {
            const auto replaced = stream->replace(
                engine_client::disclosure::publish, {.state = first, .command_id = received_id},
                sink);
            REQUIRE(replaced);
            REQUIRE(*replaced);
            REQUIRE(lifecycle.complete_at_boundary(stream->current_snapshot()));
            CHECK(lifecycle.result(received_id)->stage == engine_client::command_stage::completed);
        }
        const auto observed = stream->current_snapshot();
        const auto selected = std::ranges::
            find(first.interaction->choices, true, &game_client::interaction_choice::selectable);
        REQUIRE(selected != first.interaction->choices.end());
        const auto received = lifecycle.submit(request_for(observed, choose(selected->id)));
        REQUIRE(received);
        received_id = received->command_id;
        const auto resolved = lifecycle.validate(observed, game_client::memory::screen_size());
        REQUIRE(resolved);
        CHECK(actor_state() == before);
        CHECK(rng_get_engine() == rng);
        REQUIRE(lifecycle.execution_started());
        return *resolved; // Delivered through the existing input provider/widget.
    });
    menu.query();
    CHECK(menu.ret == 10);
    const auto ack =
        query_popup().message("%s", "Next real native interaction").allow_anykey(true).query();
    CHECK(ack.action == "ANY_INPUT");
    CHECK(reads == 2);
    REQUIRE(lifecycle.interrupt()); // No fabricated completion after the final widget closes.
}
} // namespace

TEST_CASE(
    "null and recording capture preserve real native interaction outcomes authority and RNG",
    "[engine_client_contract][mcp]") {
    const auto before = actor_state();
    const auto rng = rng_get_engine();
    const auto screen = game_client::memory::screen_size();
    auto null_sink = engine_client::null_event_sink{};
    run_native_slice(null_sink);
    CHECK(game_client::memory::screen_size() == screen);
    CHECK(actor_state() == before);
    CHECK(rng_get_engine() == rng);
    auto recording = engine_client::recording_event_sink{};
    run_native_slice(recording);
    CHECK(game_client::memory::screen_size() == screen);
    CHECK(actor_state() == before);
    CHECK(rng_get_engine() == rng);
    REQUIRE(recording.events().size() == 1);
    CHECK(recording.events().front().value().public_sequence == 1);
    CHECK(recording.events().front().value().payload.state.interaction->kind
          == game_client::interaction_kind::choices);
}
#endif
