#include "catch/catch.hpp"
#include "engine_client_event.h"
#include "rng.h"

#include <limits>
#include <string>
#include <type_traits>
#include <utility>

namespace {
auto state(const std::string& boundary) -> engine_client::state_value {
    return {.ready = {.phase = "menu", .accepts_interaction_commands = true},
            .input_boundary_id = boundary};
}
auto initial() -> engine_client::snapshot {
    return {.session_epoch = "epoch:test", .state = state("boundary:initial")};
}
auto make_stream(engine_client::snapshot snapshot) -> engine_client::event_stream {
    auto created = engine_client::event_stream::create(std::move(snapshot));
    REQUIRE(created);
    return std::move(*created);
}
auto publish(
    engine_client::event_stream& stream, engine_client::event_sink& sink,
    const std::string& boundary) -> engine_client::public_event {
    const auto result =
        stream.replace(engine_client::disclosure::publish, {.state = state(boundary)}, sink);
    REQUIRE(result.has_value());
    REQUIRE(result->has_value());
    return **result;
}
} // namespace

TEST_CASE("event values survive source mutation and destruction", "[engine_client_event]") {
    static_assert(
        std::is_same_v<decltype(std::declval<const engine_client::public_event&>().value()),
                       const engine_client::event_value&>);
    auto recorder = engine_client::recording_event_sink{};
    auto stream = make_stream(initial());
    {
        auto native = game_client::interaction_snapshot{
            .schema_id = "schema:owned",
            .title = "original title",
            .structured = true,
            .choices =
                {{.id = "choice:owned",
                  .label = "original label",
                  .description = "original description",
                  .columns = {{.label = "column", .value = "owned"}}}},
            .choice_total = 1,
        };
        auto source = state("boundary:owned");
        source.interaction = native;
        const auto published =
            stream.replace(engine_client::disclosure::publish, {.state = source}, recorder);
        REQUIRE(published);
        REQUIRE(published->has_value());
        native.title = "mutated native";
        native.choices.clear();
        source.interaction->choices.front().description = "mutated source";
        source.interaction.reset();
    }
    REQUIRE(recorder.events().size() == 1);
    const auto& owned = recorder.events().front().value().payload.state.interaction;
    REQUIRE(owned);
    CHECK(owned->title == "original title");
    REQUIRE(owned->choices.size() == 1);
    CHECK(owned->choices.front().description == "original description");
    CHECK(owned->choices.front().columns.front().value == "owned");
    CHECK(engine_client::serialize_event(recorder.events().front()).find("mutated")
          == std::string::npos);
}

TEST_CASE(
    "engine disclosure precedes public IDs sequences revisions and cause checks",
    "[engine_client_event]") {
    auto recorder = engine_client::recording_event_sink{};
    auto stream = make_stream(initial());
    const auto first = publish(stream, recorder, "boundary:A");
    const auto hidden = stream.replace(
        engine_client::disclosure::withheld,
        {.state = state("SECRET_SENTINEL"),
         .cause_sequence = std::numeric_limits<engine_client::counter>::max()},
        recorder);
    REQUIRE(hidden);
    CHECK_FALSE(*hidden);
    CHECK(stream.current_snapshot().state_revision == 1);
    CHECK(stream.current_snapshot().through_public_sequence == 1);
    const auto second = stream.replace(
        engine_client::disclosure::publish,
        {.state = state("boundary:C"), .command_id = "command:public", .cause_sequence = 1},
        recorder);
    REQUIRE(second);
    REQUIRE(*second);
    CHECK(first.value().public_sequence == 1);
    CHECK((**second).value().public_sequence == 2);
    CHECK((**second).value().payload.cause_sequence == 1);
    CHECK((**second).value().payload.command_id == "command:public");
    CHECK(first.value().event_id != (**second).value().event_id);
    const auto bytes = engine_client::serialize_batch(recorder.drain());
    CHECK(bytes.find("SECRET_SENTINEL") == std::string::npos);
    CHECK_FALSE(stream.replace(
        engine_client::disclosure::publish,
        {.state = state("boundary:invalid"), .cause_sequence = 3}, recorder));
    CHECK_FALSE(stream.replace(
        engine_client::disclosure::publish,
        {.state = state("boundary:invalid"), .cause_sequence = 0}, recorder));
    CHECK(stream.current_snapshot().through_public_sequence == 2);
}

TEST_CASE(
    "presentation grouping and time do not control causal order or reconstruction",
    "[engine_client_event]") {
    auto fast = make_stream(initial());
    auto slow = make_stream(initial());
    auto fast_sink = engine_client::recording_event_sink{};
    auto slow_sink = engine_client::recording_event_sink{};
    for (const auto ordinal : {0, 1}) {
        const auto next = state("boundary:" + std::to_string(ordinal));
        const auto a = fast.replace(
            engine_client::disclosure::publish,
            {.state = next,
             .display =
                 engine_client::presentation{
                     .group_id = "group:fast", .ordinal = ordinal, .count = 2, .duration_ms = 0}},
            fast_sink);
        const auto b = slow.replace(
            engine_client::disclosure::publish,
            {.state = next,
             .display =
                 engine_client::presentation{
                     .group_id = "group:slow" + std::to_string(ordinal), .duration_ms = 99999}},
            slow_sink);
        REQUIRE(a);
        REQUIRE(b);
        REQUIRE(*a);
        REQUIRE(*b);
        CHECK((**a).value().public_sequence == (**b).value().public_sequence);
        CHECK((**a).value().state_revision == (**b).value().state_revision);
        CHECK((**a).value().event_id == (**b).value().event_id);
    }
    auto reconstructed = initial();
    REQUIRE(engine_client::apply_batch(reconstructed, slow_sink.drain()));
    CHECK(engine_client::serialize_snapshot(reconstructed)
          == engine_client::serialize_snapshot(fast.current_snapshot()));
}

TEST_CASE(
    "replacement reconstruction rejects epochs gaps duplicates bases and malformed batches",
    "[engine_client_event]") {
    auto recorder = engine_client::recording_event_sink{};
    auto stream = make_stream(initial());
    const auto event = publish(stream, recorder, "boundary:next");
    auto receiver = initial();
    REQUIRE(engine_client::apply_event(receiver, event));
    CHECK(engine_client::serialize_snapshot(receiver)
          == engine_client::serialize_snapshot(stream.current_snapshot()));
    const auto committed = engine_client::serialize_snapshot(receiver);
    CHECK_FALSE(engine_client::apply_event(receiver, event));
    CHECK(engine_client::serialize_snapshot(receiver) == committed);
    for (const auto fault : {"epoch", "gap", "base", "cause", "revision", "unchanged"}) {
        auto corrupted = event.value();
        if (std::string{fault} == "epoch") { corrupted.session_epoch = "epoch:restart"; }
        if (std::string{fault} == "gap") { corrupted.public_sequence = 2; }
        if (std::string{fault} == "base") {
            corrupted.base_state_revision = 1;
            corrupted.state_revision = 2;
        }
        if (std::string{fault} == "cause") { corrupted.payload.cause_sequence = 1; }
        if (std::string{fault} == "revision") { corrupted.state_revision = 3; }
        if (std::string{fault} == "unchanged") { corrupted.payload.state = initial().state; }
        auto fresh = initial();
        CHECK_FALSE(engine_client::apply_event(fresh, engine_client::public_event{corrupted}));
        CHECK(engine_client::serialize_snapshot(fresh)
              == engine_client::serialize_snapshot(initial()));
    }
    auto batch = recorder.drain();
    batch.events.push_back(event); // Valid first event followed by a duplicate: no prefix applied.
    auto fresh = initial();
    CHECK_FALSE(engine_client::apply_batch(fresh, batch));
    CHECK(fresh.state_revision == 0);
    CHECK_FALSE(engine_client::apply_batch(fresh, {.first_sequence = 1}));
    CHECK_FALSE(engine_client::apply_batch(fresh, {.last_sequence = 1}));
    CHECK(engine_client::apply_batch(fresh, {}));

    auto unchanged = event.value();
    unchanged.event_id = "event:unchanged";
    unchanged.public_sequence = 2;
    unchanged.base_state_revision = 1;
    unchanged.state_revision = 2;
    const auto invalid_suffix = engine_client::event_batch{
        .first_sequence = 1,
        .last_sequence = 2,
        .events = {event, engine_client::public_event{std::move(unchanged)}},
    };
    auto untouched = initial();
    const auto rejected = engine_client::apply_batch(untouched, invalid_suffix);
    REQUIRE_FALSE(rejected);
    CHECK(rejected.error() == engine_client::error::resync_required);
    CHECK(engine_client::serialize_snapshot(untouched)
          == engine_client::serialize_snapshot(initial()));
}

TEST_CASE(
    "empty batches are accurate and recorder overflow is atomic and recoverable",
    "[engine_client_event]") {
    auto recorder = engine_client::recording_event_sink{};
    auto stream = make_stream(initial());
    const auto empty = recorder.drain();
    CHECK_FALSE(empty.first_sequence);
    CHECK_FALSE(empty.last_sequence);
    CHECK(empty.events.empty());
    CHECK(engine_client::serialize_batch(empty)
          == R"({"complete":true,"first_sequence":null,"last_sequence":null,"events":[]})");
    for (const auto index : {1, 2, 3, 4, 5, 6, 7, 8}) {
        publish(stream, recorder, "boundary:" + std::to_string(index));
    }
    const auto before = engine_client::serialize_snapshot(stream.current_snapshot());
    const auto overflow = stream.replace(
        engine_client::disclosure::publish, {.state = state("boundary:overflow")}, recorder);
    REQUIRE_FALSE(overflow);
    CHECK(overflow.error() == engine_client::error::resource_limit);
    CHECK(recorder.events().size() == 8);
    CHECK(engine_client::serialize_snapshot(stream.current_snapshot()) == before);
    auto receiver = initial();
    REQUIRE(engine_client::apply_batch(receiver, recorder.drain()));
    const auto retry = publish(stream, recorder, "boundary:overflow");
    CHECK(retry.value().public_sequence == 9);
    REQUIRE(engine_client::apply_batch(receiver, recorder.drain()));
    CHECK(engine_client::serialize_snapshot(receiver)
          == engine_client::serialize_snapshot(stream.current_snapshot()));

    auto oversized = state("boundary:oversized");
    oversized.actions.push_back({.id = "action:large", .name = std::string(262144, 'x')});
    CHECK_FALSE(stream.replace(engine_client::disclosure::publish, {.state = oversized}, recorder));
    CHECK(stream.current_snapshot().through_public_sequence == 9);
    CHECK(recorder.events().empty());
}

TEST_CASE(
    "inline byte overflow preserves required values and exposes explicit resynchronization",
    "[engine_client_event]") {
    auto stream = make_stream(initial());
    auto sink = engine_client::recording_event_sink{};
    auto small = state("boundary:small");
    small.actions.push_back({.id = "action", .name = ""});
    REQUIRE(stream.replace(
        engine_client::disclosure::publish, {.state = small, .command_id = "command:test"}, sink));
    const auto overhead = engine_client::serialize_batch(sink.drain()).size();
    auto large = state("boundary:large");
    large.actions.push_back(
        {.id = "action", .name = std::string(engine_client::maximum_inline_bytes - overhead, 'x')});
    REQUIRE(stream.replace(
        engine_client::disclosure::publish, {.state = large, .command_id = "command:test"}, sink));
    const auto batch = sink.drain();
    CHECK(engine_client::serialize_batch(batch).size() == engine_client::maximum_inline_bytes);
    CHECK(batch.events.front().value().payload.state.actions.front().name
          == large.actions.front().name);
    auto completed = engine_client::command_result{
        .session_epoch = "epoch:test",
        .command_id = "command:test",
        .stage = engine_client::command_stage::completed,
        .validation_succeeded = true,
        .execution_started = true,
        .completed = engine_client::completion{.state_revision = 2, .through_public_sequence = 2},
    };
    const auto overflow = engine_client::serialize_command_response(completed, batch);
    REQUIRE_FALSE(overflow);
    CHECK(overflow.error() == engine_client::error::resource_limit);
    completed.completed->resync_required = true;
    CHECK(engine_client::serialize_command_response(completed, {}));

    auto next_stream = make_stream(initial());
    auto recorder = engine_client::recording_event_sink{};
    auto candidate = state("boundary:first");
    candidate.actions.push_back({.id = "action", .name = std::string(150000, 'x')});
    REQUIRE(
        next_stream.replace(engine_client::disclosure::publish, {.state = candidate}, recorder));
    candidate.input_boundary_id = "boundary:other";
    const auto too_many_bytes =
        next_stream.replace(engine_client::disclosure::publish, {.state = candidate}, recorder);
    REQUIRE_FALSE(too_many_bytes);
    CHECK(too_many_bytes.error() == engine_client::error::resource_limit);
    CHECK(recorder.events().size() == 1);
    CHECK(next_stream.current_snapshot().through_public_sequence == 1);
    recorder.drain();
    const auto retry =
        next_stream.replace(engine_client::disclosure::publish, {.state = candidate}, recorder);
    REQUIRE(retry);
    REQUIRE(*retry);
    CHECK((**retry).value().public_sequence == 2);
}

TEST_CASE(
    "reads unchanged publication null sinks and counter exhaustion preserve authority RNG",
    "[engine_client_event]") {
    const auto rng = rng_get_engine();
    auto null_sink = engine_client::null_event_sink{};
    auto record = engine_client::recording_event_sink{};
    auto discard = make_stream(initial());
    auto retained = make_stream(initial());
    publish(discard, null_sink, "boundary:next");
    publish(retained, record, "boundary:next");
    CHECK(engine_client::serialize_snapshot(discard.current_snapshot())
          == engine_client::serialize_snapshot(retained.current_snapshot()));
    const auto unchanged = retained.replace(
        engine_client::disclosure::publish, {.state = retained.current_snapshot().state}, record);
    REQUIRE(unchanged);
    CHECK_FALSE(*unchanged);
    CHECK(retained.current_snapshot().state_revision == 1);
    auto exhausted = initial();
    exhausted.state_revision = std::numeric_limits<engine_client::counter>::max();
    auto full = make_stream(exhausted);
    CHECK_FALSE(full.replace(
        engine_client::disclosure::publish, {.state = state("boundary:overflow")}, null_sink));
    CHECK(rng_get_engine() == rng);
}

TEST_CASE(
    "explicit state resynchronization commits a fresh snapshot without sequence gaps or read effects",
    "[engine_client_event]") {
    auto stream = make_stream(initial());
    const auto rng = rng_get_engine();
    const auto hidden =
        stream.resynchronize(engine_client::disclosure::withheld, state("SECRET_SENTINEL"));
    REQUIRE(hidden);
    CHECK_FALSE(*hidden);
    CHECK(stream.current_snapshot().state_revision == 0);
    const auto refreshed =
        stream.resynchronize(engine_client::disclosure::publish, state("boundary:resync"));
    REQUIRE(refreshed);
    REQUIRE(*refreshed);
    CHECK((**refreshed).session_epoch == "epoch:test");
    CHECK((**refreshed).state_revision == 1);
    CHECK((**refreshed).through_public_sequence == 0);
    const auto unchanged =
        stream.resynchronize(engine_client::disclosure::publish, stream.current_snapshot().state);
    REQUIRE(unchanged);
    CHECK(stream.current_snapshot().state_revision == 1);
    auto oversized = state("boundary:large");
    oversized.actions.push_back({.id = "action", .name = std::string(262144, 'x')});
    CHECK_FALSE(stream.resynchronize(engine_client::disclosure::publish, oversized));
    CHECK(stream.current_snapshot().state_revision == 1);
    auto sink = engine_client::recording_event_sink{};
    const auto event = publish(stream, sink, "boundary:after");
    CHECK(event.value().public_sequence == 1);
    CHECK(event.value().base_state_revision == 1);
    CHECK(event.value().state_revision == 2);
    auto stale = initial();
    CHECK_FALSE(engine_client::apply_event(stale, event));
    auto fresh = **refreshed;
    REQUIRE(engine_client::apply_event(fresh, event));
    CHECK(engine_client::serialize_snapshot(fresh)
          == engine_client::serialize_snapshot(stream.current_snapshot()));
    const auto another =
        stream.resynchronize(engine_client::disclosure::publish, state("boundary:second-resync"));
    REQUIRE(another);
    REQUIRE(*another);
    const auto discontinuous = stream.replace(
        engine_client::disclosure::publish, {.state = state("boundary:final")}, sink);
    REQUIRE_FALSE(discontinuous);
    CHECK(discontinuous.error() == engine_client::error::resync_required);
    CHECK(sink.events().size() == 1);
    CHECK(stream.current_snapshot().through_public_sequence == 1);
    sink.drain();
    const auto retry = publish(stream, sink, "boundary:final");
    CHECK(retry.value().public_sequence == 2);
    auto second_fresh = **another;
    REQUIRE(engine_client::apply_batch(second_fresh, sink.drain()));
    CHECK(engine_client::serialize_snapshot(second_fresh)
          == engine_client::serialize_snapshot(stream.current_snapshot()));
    CHECK(rng_get_engine() == rng);
}

TEST_CASE(
    "inline scope binds command epoch causality lifecycle and empty resync",
    "[engine_client_event]") {
    auto stream = make_stream(initial());
    auto recorder = engine_client::recording_event_sink{};
    REQUIRE(stream.replace(
        engine_client::disclosure::publish,
        {.state = state("boundary:next"), .command_id = "command:scope"}, recorder));
    const auto batch = recorder.drain();
    auto command = engine_client::command_result{};
    command.session_epoch = "epoch:test";
    command.command_id = "command:scope";
    command.stage = engine_client::command_stage::completed;
    command.validation_succeeded = true;
    command.execution_started = true;
    command.completed =
        engine_client::completion{.state_revision = 1, .through_public_sequence = 1};
    REQUIRE(engine_client::serialize_command_response(command, batch));
    auto foreign = batch.events.front().value();
    foreign.session_epoch = "epoch:foreign";
    const auto wrong_epoch = engine_client::serialize_command_response(
        command,
        {.first_sequence = 1, .last_sequence = 1, .events = {engine_client::public_event{foreign}}});
    REQUIRE_FALSE(wrong_epoch);
    CHECK(wrong_epoch.error() == engine_client::error::stale_epoch);
    foreign = batch.events.front().value();
    foreign.payload.command_id = "command:foreign";
    const auto wrong_command = engine_client::serialize_command_response(
        command,
        {.first_sequence = 1, .last_sequence = 1, .events = {engine_client::public_event{foreign}}});
    REQUIRE_FALSE(wrong_command);
    CHECK(wrong_command.error() == engine_client::error::validation_failed);
    foreign.payload.command_id.reset(); // Incidental engine boundary events may be uncaused.
    REQUIRE(engine_client::serialize_command_response(
        command,
        {.first_sequence = 1,
         .last_sequence = 1,
         .events = {engine_client::public_event{foreign}}}));
    command.completed->resync_required = true;
    CHECK_FALSE(engine_client::serialize_command_response(command, batch));
    REQUIRE(engine_client::serialize_command_response(command, {}));
    CHECK_FALSE(engine_client::serialize_command_response(command, {.first_sequence = 1}));
    CHECK_FALSE(engine_client::serialize_command_response(command, {.last_sequence = 1}));
    command.completed->resync_required = false;
    CHECK_FALSE(engine_client::serialize_command_response(command, {}));
    for (const auto stage :
         {engine_client::command_stage::received, engine_client::command_stage::validated,
          engine_client::command_stage::executing, engine_client::command_stage::rejected,
          engine_client::command_stage::interrupted}) {
        auto pending = engine_client::command_result{};
        pending.session_epoch = "epoch:test";
        pending.command_id = "command:scope";
        pending.stage = stage;
        pending.validation_succeeded =
            stage == engine_client::command_stage::validated
            || stage == engine_client::command_stage::executing;
        pending.execution_started = stage == engine_client::command_stage::executing;
        if (stage == engine_client::command_stage::rejected) {
            pending.failure = engine_client::error::not_ready;
        }
        REQUIRE(engine_client::serialize_command_response(pending, {}));
        CHECK_FALSE(engine_client::serialize_command_response(pending, batch));
        pending.completed = command.completed;
        CHECK_FALSE(engine_client::serialize_command_response(pending, {}));
    }
    for (const auto validated : {false, true}) {
        auto interrupted = engine_client::command_result{};
        interrupted.session_epoch = "epoch:test";
        interrupted.command_id = "command:scope";
        interrupted.stage = engine_client::command_stage::interrupted;
        interrupted.validation_succeeded = validated;
        REQUIRE(engine_client::serialize_command_response(interrupted, {}));
        interrupted.execution_started = true;
        CHECK(engine_client::serialize_command_response(interrupted, {}).has_value() == validated);
    }
    command.session_epoch.clear();
    CHECK_FALSE(engine_client::serialize_command_response(command, {}));
}

TEST_CASE(
    "initial snapshots invalid display and safe integer overflow fail explicitly",
    "[engine_client_event]") {
    CHECK_FALSE(engine_client::event_stream::create({}));
    auto oversized = initial();
    oversized.state.actions.push_back({.id = "large", .name = std::string(262144, 'x')});
    const auto too_large = engine_client::event_stream::create(oversized);
    REQUIRE_FALSE(too_large);
    CHECK(too_large.error() == engine_client::error::resource_limit);
    auto stream = make_stream(initial());
    auto sink = engine_client::recording_event_sink{};
    const auto invalid_display = stream.replace(
        engine_client::disclosure::publish,
        {.state = state("boundary:next"),
         .display = engine_client::presentation{.group_id = "group", .ordinal = 1, .count = 1}},
        sink);
    CHECK_FALSE(invalid_display);
    CHECK_FALSE(stream.replace(
        engine_client::disclosure::publish,
        {.state = state("boundary:next"),
         .display = engine_client::presentation{.group_id = std::string(257, 'x')}},
        sink));
    CHECK(stream.current_snapshot().state_revision == 0);
    auto native = game_client::interaction_snapshot{
        .schema_id = "schema:large",
        .structured = true,
        .choices =
            {{.id = "choice:large", .available_count = engine_client::maximum_safe_integer + 1}},
        .field = game_client::interaction_field{.id = "field:unlimited", .max_length = -2},
        .target = game_client::interaction_target{.range = -1},
        .choice_total = 1,
    };
    auto next = state("boundary:next");
    next.interaction = native;
    const auto unsafe = stream.replace(engine_client::disclosure::publish, {.state = next}, sink);
    REQUIRE_FALSE(unsafe);
    CHECK(unsafe.error() == engine_client::error::resource_limit);
    native.choices.front().available_count = engine_client::maximum_safe_integer;
    next.interaction = native;
    REQUIRE(stream.replace(engine_client::disclosure::publish, {.state = next}, sink));
    auto sequence_exhausted = initial();
    sequence_exhausted.through_public_sequence = std::numeric_limits<engine_client::counter>::max();
    auto full = make_stream(sequence_exhausted);
    CHECK_FALSE(full.replace(engine_client::disclosure::publish, {.state = next}, sink));
    CHECK(sink.events().size() == 1);
    const auto& owned = sink.events().front().value().payload.state.interaction;
    CHECK(owned->field->max_length == -2);
    CHECK(owned->target->range == -1);
}
