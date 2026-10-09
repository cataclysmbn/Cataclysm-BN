#include "avatar.h"
#include "catch/catch.hpp"
#include "client_command.h"
#include "client_input.h"
#include "client_interaction_prepared.h"
#include "client_interaction_validation.h"
#include "engine_client_contract.h"
#include "engine_client_event.h"
#include "input.h"
#include "json.h"
#include "rng.h"

#include <array>
#include <bit>
#include <limits>
#include <memory>
#include <ranges>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace {
namespace client = game_client;

auto model(const std::size_t count = 513) -> client::interaction_snapshot {
    auto result = client::interaction_snapshot{
        .input_id = 999,
        .schema_id = "untrusted",
        .context = "wrong",
        .kind = client::interaction_kind::inventory,
        .title = "Title",
        .message = "Message",
        .allow_cancel = true,
        .panes =
            {{.id = "pane",
              .label = "Pane",
              .role = "source",
              .area_id = "area",
              .area_label = "Area",
              .area_description = "Area description",
              .filter = "filter",
              .storage_kind = "ground"}},
        .field =
            client::interaction_field{
                .id = "field",
                .label = "Field",
                .description = "Text",
                .value = "before",
                .type = "integer",
                .max_length = 4,
                .printable = true},
        .target =
            client::interaction_target{
                .source = {},
                .cursor = {.x = 1},
                .minimum_position = client::interaction_position{.x = -5, .y = -5, .z = -5},
                .maximum_position = client::interaction_position{.x = 5, .y = 5, .z = 5},
                .range = 4,
                .distance_metric = "square",
                .status = "ready",
                .limit_to_reality_bubble = true,
                .candidates =
                    {{.id = "target",
                      .label = "Target",
                      .description = "Creature",
                      .position = {.x = 2},
                      .creature = true}}},
        .allow_set_count = true,
        .choice_offset = 22,
        .choice_total = 1,
    };
    for (const auto ordinal : std::views::iota(std::size_t{0}, count)) {
        result.choices.push_back(
            {.id = "choice:" + std::to_string(ordinal),
             .label = "Label " + std::to_string(ordinal),
             .description = std::string(1024, 'x'),
             .denial = "Denied",
             .pane_id = "pane",
             .area_id = "area",
             .storage_kind = "ground",
             .enabled = false,
             .selectable = true,
             .selected = true,
             .highlighted = true,
             .columns = {{.label = "Weight", .value = "2"}},
             .selected_count = 2,
             .minimum_count = 1,
             .available_count = 4});
    }
    return result;
}

auto command(const std::string& id = "choice:512") -> client::interaction_command {
    return {.input_id = client::current_input_id(),
            .operation = client::interaction_operation::choose,
            .target_id = id};
}

auto actor_state() -> std::string {
    auto output = std::ostringstream{};
    auto json = JsonOut{output};
    get_avatar().serialize(json);
    return output.str();
}

auto schema(const input_context& context, client::interaction_snapshot snapshot) -> std::string {
    const auto scope = client::prepared_interaction_scope{
        context, client::prepare_interaction(context, std::move(snapshot))};
    return client::current_interaction({.limit = 0}).schema_id;
}

/// Heap-backed fixtures make storage identity a deterministic no-clone witness (not an allocation
/// timing estimate).  These addresses are compared only while the owning result/handle is alive.
struct metadata_storage {
    const client::interaction_pane* panes = nullptr;
    const client::interaction_target_candidate* targets = nullptr;
    const char* title = nullptr;
    const char* message = nullptr;
    const char* pane_description = nullptr;
    const char* field_value = nullptr;
    const char* field_description = nullptr;
    const char* target_status = nullptr;
    const char* target_label = nullptr;
    const char* target_description = nullptr;
};

auto metadata_addresses(const client::interaction_snapshot& snapshot) -> metadata_storage {
    return {
        .panes = snapshot.panes.data(),
        .targets = snapshot.target->candidates.data(),
        .title = snapshot.title.data(),
        .message = snapshot.message.data(),
        .pane_description = snapshot.panes.front().area_description.data(),
        .field_value = snapshot.field->value.data(),
        .field_description = snapshot.field->description.data(),
        .target_status = snapshot.target->status.data(),
        .target_label = snapshot.target->candidates.back().label.data(),
        .target_description = snapshot.target->candidates.back().description.data(),
    };
}

auto check_metadata_storage(
    const metadata_storage& actual, const metadata_storage& original, const bool transferred)
    -> void {
    CHECK((actual.panes == original.panes) == transferred);
    CHECK((actual.targets == original.targets) == transferred);
    CHECK((actual.title == original.title) == transferred);
    CHECK((actual.message == original.message) == transferred);
    CHECK((actual.pane_description == original.pane_description) == transferred);
    CHECK((actual.field_value == original.field_value) == transferred);
    CHECK((actual.field_description == original.field_description) == transferred);
    CHECK((actual.target_status == original.target_status) == transferred);
    CHECK((actual.target_label == original.target_label) == transferred);
    CHECK((actual.target_description == original.target_description) == transferred);
}

auto metadata_model() -> client::interaction_snapshot {
    auto snapshot = model(205);
    snapshot.title = std::string(1024, 't');
    snapshot.message = std::string(4096, 'm');
    snapshot.panes.front().area_description = std::string(4096, 'p');
    snapshot.field->value = std::string(4096, 'v');
    snapshot.field->description = std::string(4096, 'f');
    snapshot.target->status = std::string(4096, 's');
    snapshot.target->candidates.clear();
    for (const auto ordinal : std::views::iota(0, 32)) {
        snapshot.target->candidates.push_back(
            {.id = "target:" + std::to_string(ordinal),
             .label = std::string(256, 'l'),
             .description = std::string(4096, 'd'),
             .position = {.x = 2}});
    }
    return snapshot;
}

} // namespace

TEST_CASE(
    "prepared pages equal legacy pages and only materialize requested choices",
    "[client_interaction_prepared]") {
    auto context = input_context{"PREPARED"};
    const auto input = client::input_context_scope{context, "PREPARED"};
    client::begin_input_boundary();
    auto snapshot = model();
    auto pages = std::vector<client::interaction_page>{
        {},
        {.offset = 200, .limit = 17},
        {.offset = 510, .limit = 200},
        {.offset = 12, .limit = 0},
        {.offset = 0, .limit = 500},
        {.offset = std::numeric_limits<std::size_t>::max(), .limit = 200}};
    auto expected = std::vector<std::string>{};
    auto reads = std::size_t{0};
    {
        const auto legacy = client::interaction_scope{
            context, [&]() {
                ++reads;
                return snapshot;
            }};
        for (const auto page : pages) {
            expected.push_back(client::serialize_interaction(client::current_interaction(page)));
        }
    }
    CHECK(reads == pages.size());
    const auto before = actor_state();
    const auto rng = rng_get_engine();
    client::reset_interaction_work();
    const auto prepared = client::prepare_interaction(context, std::move(snapshot));
    CHECK(client::interaction_work().schema_hashes == 1);
    CHECK(client::interaction_work().hashed_choices == 513);
    const auto scope = client::prepared_interaction_scope{context, prepared};
    client::reset_interaction_work();
    auto total_rows = std::size_t{0};
    for (const auto index : std::views::iota(std::size_t{0}, pages.size())) {
        const auto result = client::current_interaction(pages[index]);
        CHECK(client::serialize_interaction(result) == expected[index]);
        CHECK(result.schema_id != "untrusted");
        CHECK(result.choice_total == 513);
        CHECK(result.input_id == client::current_input_id());
        total_rows += result.choices.size();
    }
    CHECK(client::interaction_work().schema_hashes == 0);
    CHECK(client::interaction_work().hashed_choices == 0);
    CHECK(client::interaction_work().materialized_choices == total_rows);
    CHECK(reads == pages.size());
    CHECK(actor_state() == before);
    CHECK(rng_get_engine() == rng);
}

TEST_CASE(
    "prepared schema preserves full-model hash inclusions and exclusions",
    "[client_interaction_prepared]") {
    auto context = input_context{"HASH"};
    const auto input = client::input_context_scope{context, "HASH"};
    auto snapshot = model();
    const auto original = schema(context, snapshot);
    auto changes_hash = true;
    SECTION("off-page description") { snapshot.choices.back().description += "new"; }
    SECTION("off-page denial") { snapshot.choices.back().denial += "new"; }
    SECTION("ordered choices") { std::swap(snapshot.choices.front(), snapshot.choices.back()); }
    SECTION("selectability") { snapshot.choices.back().selectable = false; }
    SECTION("enabled") { snapshot.choices.back().enabled = true; }
    SECTION("columns") { snapshot.choices.back().columns.front().value = "3"; }
    SECTION("quantity limits") { snapshot.choices.back().minimum_count = 2; }
    SECTION("pane") { snapshot.panes.front().area_description = "new"; }
    SECTION("target cursor") { snapshot.target->cursor.x = 2; }
    SECTION("target candidates") { snapshot.target->candidates.front().description += "new"; }
    SECTION("field constraints") { snapshot.field->max_length = 3; }
    SECTION("excluded selection state and field value") {
        changes_hash = false;
        snapshot.input_id = 234;
        snapshot.schema_id = "caller";
        snapshot.choice_offset = 432;
        snapshot.choice_total = 432;
        snapshot.choices.back().selected = false;
        snapshot.choices.back().highlighted = false;
        snapshot.choices.back().selected_count = 3;
        snapshot.field->value = "after";
    }
    CHECK((schema(context, snapshot) != original) == changes_hash);
    const auto scope =
        client::prepared_interaction_scope{context, client::prepare_interaction(context, snapshot)};
    const auto page = client::current_interaction({.offset = 512, .limit = 1});
    CHECK(page.choices.front().selected == snapshot.choices.back().selected);
    CHECK(page.choices.front().selected_count == snapshot.choices.back().selected_count);
    CHECK(page.field->value == snapshot.field->value);
}

TEST_CASE(
    "pagination resets and clamps with one acquisition and materialization",
    "[client_interaction_prepared]") {
    auto context = input_context{"PAGES"};
    const auto input = client::input_context_scope{context, "PAGES"};
    auto snapshot = model(205);
    auto reads = 0;
    const auto legacy = client::interaction_scope{
        context, [&]() {
            ++reads;
            return snapshot;
        }};
    auto state = client::interaction_page_state{.offset = 200, .limit = 100, .schema_id = "stale"};
    client::reset_interaction_work();
    auto page = client::paginated_interaction(state);
    CHECK(reads == 1);
    CHECK(page.choice_offset == 0);
    CHECK(client::interaction_work().materialized_choices == 100);
    state.offset = std::numeric_limits<std::size_t>::max();
    reads = 0;
    client::reset_interaction_work();
    page = client::paginated_interaction(state);
    CHECK(reads == 1);
    CHECK(state.offset == 200);
    CHECK(page.choices.size() == 5);
    CHECK(client::interaction_work().materialized_choices == 5);
    snapshot.choices.clear();
    reads = 0;
    state.limit = 0;
    page = client::paginated_interaction(state);
    CHECK(reads == 1);
    CHECK(state.offset == 0);
    CHECK(state.limit == 1);
    CHECK(page.choices.empty());
    const auto prepared = client::
        prepared_interaction_scope{context, client::prepare_interaction(context, model(205))};
    client::reset_interaction_work();
    state.limit = 999;
    page = client::paginated_interaction(state);
    CHECK(state.limit == 200);
    CHECK(page.choices.size() == 200);
    CHECK(client::interaction_work().schema_hashes == 0);
    CHECK(client::interaction_work().materialized_choices == 200);
}

TEST_CASE(
    "prepared validation uses full indexed authority without copying or hashing",
    "[client_interaction_prepared]") {
    auto context = input_context{"VALIDATE"};
    const auto input = client::input_context_scope{context, "VALIDATE"};
    auto snapshot = model();
    snapshot.choices[511].selectable = false;
    const auto scope = client::prepared_interaction_scope{
        context, client::prepare_interaction(context, std::move(snapshot))};
    client::begin_input_boundary();
    const auto page = client::current_interaction({.limit = 1});
    auto request = command();
    auto expected = true;
    auto expected_kind = client::interaction_rejection::invalid;
    auto expected_schema = page.schema_id;
    auto space = std::optional<std::string_view>{};
    SECTION("off-page disabled but selectable choose") {}
    SECTION("off-page count") {
        request.operation = client::interaction_operation::set_count;
        request.count = 3;
    }
    SECTION("below minimum") {
        request.operation = client::interaction_operation::set_count;
        request.count = 0;
        expected = false;
    }
    SECTION("above available") {
        request.operation = client::interaction_operation::set_count;
        request.count = 5;
        expected = false;
    }
    SECTION("unknown") {
        request.target_id = "missing";
        expected = false;
    }
    SECTION("blocked") {
        request.target_id = "choice:511";
        expected = false;
    }
    SECTION("stale schema") {
        expected_schema = "stale";
        expected = false;
        expected_kind = client::interaction_rejection::stale_schema;
    }
    SECTION("stale boundary") {
        ++request.input_id;
        expected = false;
        expected_kind = client::interaction_rejection::stale_boundary;
    }
    SECTION("cancel") {
        request.operation = client::interaction_operation::cancel;
        request.target_id.clear();
    }
    SECTION("invalid cancel shape") {
        request.operation = client::interaction_operation::cancel;
        expected = false;
    }
    SECTION("fill explicit false") {
        request.operation = client::interaction_operation::fill;
        request.target_id = "field";
        request.value = "-12";
        request.submit = false;
    }
    SECTION("invalid integer") {
        request.operation = client::interaction_operation::fill;
        request.target_id = "field";
        request.value = "1a";
        request.submit = true;
        expected = false;
    }
    SECTION("invalid length") {
        request.operation = client::interaction_operation::fill;
        request.target_id = "field";
        request.value = "12345";
        request.submit = true;
        expected = false;
    }
    SECTION("target candidate") {
        request.operation = client::interaction_operation::set_target;
        request.target_id = "target";
        request.position = client::interaction_position{.x = 2};
        space = "bubble_ms";
    }
    SECTION("wrong coordinate space") {
        request.operation = client::interaction_operation::set_target;
        request.target_id = "target";
        request.position = client::interaction_position{.x = 2};
        space = "absolute";
        expected = false;
    }
    SECTION("candidate position mismatch") {
        request.operation = client::interaction_operation::set_target;
        request.target_id = "target";
        request.position = client::interaction_position{.x = 3};
        expected = false;
    }
    SECTION("out of range") {
        request.operation = client::interaction_operation::set_target;
        request.target_id.clear();
        request.position = client::interaction_position{.x = 5};
        expected = false;
    }
    SECTION("out of bounds") {
        request.operation = client::interaction_operation::set_target;
        request.target_id.clear();
        request.position = client::interaction_position{.x = -6};
        expected = false;
    }
    const auto before = actor_state();
    const auto rng = rng_get_engine();
    client::reset_interaction_work();
    const auto resolved = client::resolve_checked_interaction(
        {.command = request, .expected_schema = expected_schema, .target_space = space});
    CHECK(static_cast<bool>(resolved) == expected);
    if (!resolved) { CHECK(resolved.error().kind == expected_kind); }
    CHECK(client::interaction_work().schema_hashes == 0);
    CHECK(client::interaction_work().hashed_choices == 0);
    CHECK(client::interaction_work().materialized_choices == 0);
    CHECK(client::interaction_work().id_comparisons <= 11);
    CHECK(actor_state() == before);
    CHECK(rng_get_engine() == rng);
}

TEST_CASE(
    "prepared indexes preserve the first original duplicate ID", "[client_interaction_prepared]") {
    auto context = input_context{"DUPLICATES"};
    const auto input = client::input_context_scope{context, "DUPLICATES"};
    auto snapshot = model(2);
    snapshot.choices[1].id = snapshot.choices[0].id;
    snapshot.choices[0].selectable = false;
    snapshot.target->candidates.push_back(snapshot.target->candidates.front());
    snapshot.target->candidates.back().position.x = 3;
    auto request = command("choice:0");
    auto expected = false;
    SECTION("first blocked choice wins") {}
    SECTION("first selectable choice wins") {
        snapshot.choices[0].selectable = true;
        snapshot.choices[1].selectable = false;
        expected = true;
    }
    SECTION("first quantity limits win") {
        snapshot.choices[0].selectable = true;
        snapshot.choices[0].available_count = 1;
        request.operation = client::interaction_operation::set_count;
        request.count = 4;
    }
    SECTION("first target position wins") {
        request.operation = client::interaction_operation::set_target;
        request.target_id = "target";
        request.position = client::interaction_position{.x = 3};
    }
    const auto scope = client::prepared_interaction_scope{
        context, client::prepare_interaction(context, std::move(snapshot))};
    CHECK(static_cast<bool>(client::resolve_interaction_command(request)) == expected);
    request.operation = client::interaction_operation::set_target;
    request.count.reset();
    request.target_id = "target";
    request.position = client::interaction_position{.x = 3};
    CHECK_FALSE(client::resolve_interaction_command(request));
    request.position = client::interaction_position{.x = 2};
    CHECK(client::resolve_interaction_command(request));
}

TEST_CASE(
    "prepared scope ownership nesting registry growth and native delivery",
    "[client_interaction_prepared]") {
    auto context = input_context{"SAME_CATEGORY"};
    auto other = input_context{"SAME_CATEGORY"};
    auto retained = client::prepared_interaction_handle{};
    auto page = client::interaction_snapshot{};
    auto event = client::interaction_event{};
    {
        auto source = model();
        retained = client::prepare_interaction(context, std::move(source));
        const auto parent = client::prepared_interaction_scope{context, retained};
        {
            const auto input = client::input_context_scope{context, "SAME_CATEGORY"};
            client::begin_input_boundary();
            page = client::current_interaction({.offset = 512, .limit = 1});
            const auto resolved = client::resolve_interaction_command(command());
            REQUIRE(resolved);
            event = *resolved;
            const auto old_id = page.input_id;
            client::begin_input_boundary();
            CHECK(client::current_interaction({.limit = 0}).input_id != old_id);
            CHECK_FALSE(client::resolve_interaction_command(
                {.input_id = old_id,
                 .operation = client::interaction_operation::choose,
                 .target_id = "choice:512"}));
            auto nested = model(1);
            nested.title = "Nested";
            {
                const auto child = client::prepared_interaction_scope{
                    context, client::prepare_interaction(context, std::move(nested))};
                CHECK(client::current_interaction().title == "Nested");
                CHECK_FALSE(client::validate_interaction_event(context, event));
            }
            CHECK(client::current_interaction({.limit = 1}).title == "Title");
            {
                const auto other_input = client::input_context_scope{other, "SAME_CATEGORY"};
                CHECK_FALSE(client::current_interaction().structured);
                const auto other_scope =
                    client::interaction_scope{other, []() { return model(1); }};
                CHECK(client::current_interaction().choice_total == 1);
            }
            auto scopes = std::vector<std::unique_ptr<client::prepared_interaction_scope>>{};
            for ([[maybe_unused]] const auto index : std::views::iota(0, 256)) {
                scopes.push_back(std::make_unique<client::prepared_interaction_scope>(
                    other, client::prepare_interaction(other, model(0))));
            }
            CHECK(client::current_interaction({.offset = 512, .limit = 1}).choices.front().id
                  == "choice:512");
        }
        // handle_input's active context has unwound before native semantic delivery.
        REQUIRE(client::active_input_context().context != &context);
        CHECK(client::validate_interaction_event(context, event));
    }
    CHECK_FALSE(client::validate_interaction_event(context, event));
    REQUIRE(retained);
    CHECK(page.choices.front().description == std::string(1024, 'x'));
    CHECK(event.target_id == "choice:512");
    const auto input = client::input_context_scope{context, "SAME_CATEGORY"};
    auto updated = model();
    updated.choices.pop_back();
    const auto scope = client::prepared_interaction_scope{
        context, client::prepare_interaction(context, std::move(updated))};
    CHECK_FALSE(client::validate_interaction_event(context, event));
    CHECK_FALSE(client::resolve_interaction_command(command()));
    CHECK(page.choice_total == 513);
    CHECK_THROWS_AS(client::prepared_interaction_scope(other, retained), std::invalid_argument);
    CHECK_THROWS_AS(client::prepared_interaction_scope(context, nullptr), std::invalid_argument);
}

TEST_CASE(
    "prepared pages preserve same-clock event projection and owned lifetimes",
    "[client_interaction_prepared][engine_client_event]") {
    auto context = input_context{"PREPARED_EVENTS"};
    const auto input = client::input_context_scope{context, "PREPARED_EVENTS"};
    client::begin_input_boundary();
    auto observed = engine_client::snapshot{};
    auto sink = engine_client::recording_event_sink{};
    auto owned_projection = std::optional<engine_client::public_event>{};
    const auto rng = rng_get_engine();
    const auto before = actor_state();
    {
        const auto scope = client::
            prepared_interaction_scope{context, client::prepare_interaction(context, model())};
        const auto capture = [](const std::size_t offset) {
            return engine_client::capture_state(
                {.session_epoch = "epoch:prepared", .page = {.offset = offset, .limit = 1}});
        };
        const auto first = capture(0);
        REQUIRE(first);
        observed = {.session_epoch = "epoch:prepared", .state = *first};
        auto created = engine_client::event_stream::create(observed);
        REQUIRE(created);
        auto stream = std::move(*created);
        const auto later = capture(512);
        REQUIRE(later);
        const auto projection = stream.project_snapshot(*later);
        REQUIRE(projection);
        CHECK(projection->state_revision == 0);
        CHECK(projection->through_public_sequence == 0);
        CHECK(projection->state.interaction->choices.front().id == "choice:512");
        const auto back = capture(0);
        REQUIRE(back);
        REQUIRE(stream.project_snapshot(*back));
        CHECK(stream.current_snapshot().state_revision == 0);
        CHECK(sink.events().empty());
        client::begin_input_boundary();
        const auto next = capture(0);
        REQUIRE(next);
        const auto delta =
            stream.replace(engine_client::disclosure::publish, {.state = *next}, sink);
        REQUIRE(delta);
        REQUIRE(*delta);
        const auto next_page = capture(512);
        REQUIRE(next_page);
        const auto projected_event = engine_client::project_event(**delta, *next_page);
        REQUIRE(projected_event);
        owned_projection = *projected_event;
        CHECK(projected_event->value().public_sequence == (**delta).value().public_sequence);
        CHECK(projected_event->value().state_revision == (**delta).value().state_revision);
        CHECK(
            projected_event->value().payload.state.interaction->choices.front().id == "choice:512");
        CHECK_FALSE(stream.project_snapshot(*first));
        CHECK_FALSE(engine_client::project_event(**delta, *first));
        CHECK(actor_state() == before);
        CHECK(rng_get_engine() == rng);
    }
    REQUIRE(owned_projection);
    CHECK(owned_projection->value().payload.state.interaction->choices.front().id == "choice:512");
    CHECK_FALSE(engine_client::serialize_event(*owned_projection).empty());
    const auto batch = sink.drain();
    REQUIRE(engine_client::apply_batch(observed, batch));
    CHECK(observed.state_revision == 1);
    CHECK(observed.state.interaction->choices.front().id == "choice:0");
    CHECK_FALSE(engine_client::serialize_batch(batch).empty());
}

TEST_CASE(
    "prepared ID work scales logarithmically for choices and complete targets",
    "[client_interaction_prepared]") {
    auto context = input_context{"INDEX_WORK"};
    const auto input = client::input_context_scope{context, "INDEX_WORK"};
    for (const auto count :
         {std::size_t{1}, std::size_t{17}, std::size_t{1025}, std::size_t{8193}}) {
        auto snapshot = model(count);
        snapshot.target->candidates.clear();
        for (const auto ordinal : std::views::iota(std::size_t{0}, count)) {
            snapshot.target->candidates.push_back(
                {.id = "target:" + std::to_string(ordinal), .position = {.x = 2}});
        }
        const auto scope = client::prepared_interaction_scope{
            context, client::prepare_interaction(context, std::move(snapshot))};
        const auto bound = static_cast<std::size_t>(std::bit_width(count)) + 1;
        for (const auto ordinal : {std::size_t{0}, count / 2, count - 1, count}) {
            client::reset_interaction_work();
            auto request = command("choice:" + std::to_string(ordinal));
            CHECK(static_cast<bool>(client::resolve_interaction_command(request))
                  == (ordinal < count));
            CHECK(client::interaction_work().id_comparisons <= bound);
            CHECK(client::interaction_work().materialized_choices == 0);
            CHECK(client::interaction_work().schema_hashes == 0);
            client::reset_interaction_work();
            request.operation = client::interaction_operation::set_target;
            request.target_id = "target:" + std::to_string(ordinal);
            request.position = client::interaction_position{.x = 2};
            CHECK(static_cast<bool>(client::resolve_interaction_command(request))
                  == (ordinal < count));
            CHECK(client::interaction_work().id_comparisons <= bound);
            CHECK(client::interaction_work().materialized_choices == 0);
            CHECK(client::interaction_work().schema_hashes == 0);
        }
    }
}

TEST_CASE(
    "legacy callbacks retain mutable state and survive nested registry growth",
    "[client_interaction_prepared]") {
    auto context = input_context{"LEGACY_LIVE"};
    auto other = input_context{"LEGACY_OTHER"};
    const auto input = client::input_context_scope{context, "LEGACY_LIVE"};
    const auto legacy = client::interaction_scope{
        context, [&, reads = 0]() mutable {
            auto scopes = std::vector<std::unique_ptr<client::prepared_interaction_scope>>{};
            for ([[maybe_unused]] const auto ordinal : std::views::iota(0, 256)) {
                scopes.push_back(std::make_unique<client::prepared_interaction_scope>(
                    other, client::prepare_interaction(other, model(0))));
            }
            auto snapshot = model(1);
            snapshot.title = std::to_string(++reads);
            return snapshot;
        }};
    CHECK(client::current_interaction().title == "1");
    {
        auto outer = std::make_unique<client::prepared_interaction_scope>(
            context, client::prepare_interaction(context, model(2)));
        const auto inner = client::interaction_scope{context, []() { return model(3); }};
        outer.reset(); // Token removal must not remove the newer registration.
        CHECK(client::current_interaction().choice_total == 3);
    }
    CHECK(client::current_interaction().title == "2");
}

TEST_CASE(
    "engine page limits reject before acquiring either interaction provider",
    "[client_interaction_prepared]") {
    auto context = input_context{"PAGE_LIMITS"};
    const auto input = client::input_context_scope{context, "PAGE_LIMITS"};
    auto reads = 0;
    const auto legacy = client::interaction_scope{
        context, [&]() {
            ++reads;
            return model(1);
        }};
    const auto check_invalid = [&]() {
        for (const auto limit : {std::size_t{0}, std::size_t{201}}) {
            client::reset_interaction_work();
            CHECK_FALSE(engine_client::capture_state(
                {.session_epoch = "epoch:limits", .page = {.limit = limit}}));
            CHECK(client::interaction_work().schema_hashes == 0);
            CHECK(client::interaction_work().materialized_choices == 0);
        }
    };
    check_invalid();
    CHECK(reads == 0);
    const auto prepared =
        client::prepared_interaction_scope{context, client::prepare_interaction(context, model(1))};
    check_invalid();
    CHECK(reads == 0);
    CHECK(client::current_interaction({.limit = 0}).choices.empty());
}

#if defined(CATA_MCP)
#    include "client_memory.h"

TEST_CASE(
    "prepared native delivery survives retries input increments and context restoration",
    "[client_interaction_prepared]") {
    struct provider_guard {
        ~provider_guard() { client::memory::set_input_provider({}); }
    };
    const auto guard = provider_guard{};
    auto context = input_context{"PREPARED_NATIVE"};
    // Preparation deliberately precedes both increments inside handle_input.
    const auto old_id = client::current_input_id();
    const auto prepared =
        client::prepared_interaction_scope{context, client::prepare_interaction(context, model(2))};
    const auto enclosing = client::active_input_context().context;
    auto reads = 0;
    auto ids = std::vector<std::uint64_t>{};
    client::reset_interaction_work();
    client::memory::set_input_provider([&](const int /*timeout*/) {
        REQUIRE(client::active_input_context().context == &context);
        const auto page = client::current_interaction({.limit = 1});
        ids.push_back(page.input_id);
        CHECK(page.input_id == client::current_input_id());
        CHECK(page.input_id != old_id);
        if (reads++ == 0) {
            auto invalid = input_event{};
            invalid.type = input_event_t::keyboard;
            invalid.sequence.push_back('x'); // No registered action: handle_input reads again.
            return invalid;
        }
        REQUIRE(reads == 2);
        auto native = client::input_command{};
        native.interaction = command("choice:1");
        const auto resolved = client::resolve_input_command(native, client::memory::screen_size());
        REQUIRE(resolved);
        return *resolved;
    });
    CHECK(context.handle_input() == "ANY_INPUT");
    CHECK(reads == 2);
    REQUIRE(ids.size() == 2);
    CHECK(ids[0] != ids[1]);
    CHECK(client::active_input_context().context == enclosing);
    REQUIRE(context.get_raw_input().interaction);
    CHECK(context.get_raw_input().interaction->target_id == "choice:1");
    CHECK(client::interaction_work().schema_hashes == 0);
    CHECK(client::interaction_work().materialized_choices == 2);
}
#endif


TEST_CASE(
    "legacy pages transfer complete ephemeral metadata storage without cloning",
    "[client_interaction_prepared]") {
    auto context = input_context{"LEGACY_STORAGE"};
    const auto input = client::input_context_scope{context, "LEGACY_STORAGE"};
    auto original = metadata_storage{};
    auto reads = 0;
    const auto legacy = client::interaction_scope{
        context, [&]() {
            auto snapshot = metadata_model();
            original = metadata_addresses(snapshot);
            ++reads;
            return snapshot;
        }};
    auto result = client::interaction_snapshot{};
    auto offset = std::size_t{200};
    auto limit = std::size_t{1};
    SECTION("direct page") {
        result = client::current_interaction({.offset = offset, .limit = limit});
    }
    SECTION("schema reset pagination") {
        auto state =
            client::interaction_page_state{.offset = offset, .limit = limit, .schema_id = "stale"};
        result = client::paginated_interaction(state);
        offset = 0;
    }
    SECTION("offset clamp pagination") {
        auto state = client::interaction_page_state{
            .offset = std::numeric_limits<std::size_t>::max(),
            .limit = 100,
            .schema_id = schema(context, metadata_model())};
        result = client::paginated_interaction(state);
        limit = 5;
    }
    CHECK(reads == 1);
    check_metadata_storage(metadata_addresses(result), original, true);
    CHECK(result.choice_offset == offset);
    CHECK(result.choices.size() == limit);
    CHECK(result.target->candidates.size() == 32);
    CHECK(result.target->candidates.back().description == std::string(4096, 'd'));
    CHECK(result.field->value == std::string(4096, 'v'));
}

TEST_CASE(
    "prepared pages copy complete metadata while preserving immutable owner storage",
    "[client_interaction_prepared]") {
    auto context = input_context{"PREPARED_STORAGE"};
    const auto input = client::input_context_scope{context, "PREPARED_STORAGE"};
    auto source = metadata_model();
    const auto original = metadata_addresses(source);
    const auto handle = client::prepare_interaction(context, std::move(source));
    auto first = client::interaction_snapshot{};
    auto later = client::interaction_snapshot{};
    {
        const auto prepared = client::prepared_interaction_scope{context, handle};
        first = client::current_interaction({.limit = 1});
        later = client::current_interaction({.offset = 200, .limit = 1});
        check_metadata_storage(metadata_addresses(first), original, false);
        check_metadata_storage(metadata_addresses(later), original, false);
        check_metadata_storage(metadata_addresses(first), metadata_addresses(later), false);
        first.target->candidates.back().description = "Changed owned page";
        first.field->value = "Changed owned page";
        const auto again = client::current_interaction({.limit = 1});
        CHECK(again.target->candidates.back().description == std::string(4096, 'd'));
        CHECK(again.field->value == std::string(4096, 'v'));
        CHECK(again.schema_id == later.schema_id);
    }
    CHECK(later.target->candidates.back().description == std::string(4096, 'd'));
    CHECK(later.field->value == std::string(4096, 'v'));
}

TEST_CASE(
    "lazy prepared descriptions own inputs and render only requested uncached rows",
    "[client_interaction_prepared][client_interaction_lazy]") {
    auto context = input_context{"LAZY"};
    const auto input = client::input_context_scope{context, "LAZY"};
    client::begin_input_boundary();
    auto snapshot = model(5);
    for (auto& choice : snapshot.choices) { choice.description.clear(); }
    auto text = std::make_shared<const std::vector<std::string>>(std::vector<std::string>{
        "first", "second", std::string(2 * 1024 * 1024, 'x'), "fourth", "throw"});
    const auto* storage = text->at(2).data();
    auto calls = std::size_t{0};
    const auto prepared = client::prepare_interaction(
        context, std::move(snapshot),
        {
            .dependency_keys = {"first-v1", "second-v1", "long-v1", "fourth-v1", "throw-v1"},
            .render = [owned = text, &calls, storage](const auto index) -> std::string {
                ++calls;
                CHECK(owned->at(2).data() == storage); // capture/preparation did not clone off-page
                                                       // text
                if (index == 4) { throw std::runtime_error("off-page renderer"); }
                return owned->at(index);
            },
        });
    text.reset(); // only the prepared boundary retains this source
    const auto scope = client::prepared_interaction_scope{context, prepared};
    CHECK(calls == 0);
    const auto empty = client::current_interaction({.limit = 0});
    const auto past = client::current_interaction({.offset = 99, .limit = 1});
    CHECK(empty.choices.empty());
    CHECK(past.choices.empty());
    CHECK(calls == 0);
    const auto one = client::current_interaction({.limit = 1});
    CHECK(one.choices.front().description == "first");
    CHECK(calls == 1);
    CHECK(client::current_interaction({.limit = 1}).choices.front().description == "first");
    CHECK(calls == 1);
    const auto two = client::current_interaction({.offset = 1, .limit = 2});
    CHECK(two.choices.back().description == std::string(2 * 1024 * 1024, 'x'));
    CHECK(calls == 3);
    CHECK(empty.schema_id == two.schema_id);
    CHECK(one.schema_id == two.schema_id);
    CHECK_THROWS_AS(client::current_interaction({.offset = 4, .limit = 1}), std::runtime_error);
    CHECK_THROWS_AS(client::current_interaction({.offset = 4, .limit = 1}), std::runtime_error);
    CHECK(calls == 5); // failed rendering never publishes a cache entry
    CHECK(client::current_interaction({.limit = 1}).schema_id == empty.schema_id);
}

TEST_CASE(
    "lazy validation nesting and stale rules never invoke description renderers",
    "[client_interaction_prepared][client_interaction_lazy]") {
    auto context = input_context{"LAZY_VALIDATE"};
    const auto input = client::input_context_scope{context, "LAZY_VALIDATE"};
    client::begin_input_boundary();
    auto calls = 0;
    const auto prepare = [&](const std::string& dependency) {
        auto snapshot = model(2);
        for (auto& choice : snapshot.choices) { choice.description.clear(); }
        snapshot.choices.back().selectable = false;
        return client::prepare_interaction(
            context, std::move(snapshot),
            {
                .dependency_keys = {dependency, "blocked"},
                .render = [&](const auto /*ordinal*/) -> std::string {
                    ++calls;
                    throw std::runtime_error("validation must not render");
                },
            });
    };
    const auto parent = client::prepared_interaction_scope{context, prepare("parent")};
    const auto schema = client::current_interaction({.limit = 0}).schema_id;
    const auto event = client::resolve_interaction_command(command("choice:0"));
    REQUIRE(event); // disabled-but-selectable must still reach native denial
    CHECK(client::validate_interaction_event(context, *event));
    CHECK_FALSE(client::resolve_interaction_command(command("choice:1")));
    CHECK_FALSE(client::resolve_interaction_command(command("missing")));
    auto stale = command("choice:0");
    ++stale.input_id;
    CHECK_FALSE(client::resolve_interaction_command(stale));
    CHECK_FALSE(client::resolve_checked_interaction(
        {.command = command("choice:0"), .expected_schema = "stale"}));
    {
        const auto child = client::prepared_interaction_scope{context, prepare("child")};
        CHECK(client::current_interaction({.limit = 0}).schema_id != schema);
        CHECK_FALSE(client::validate_interaction_event(context, *event));
    }
    CHECK(client::current_interaction({.limit = 0}).schema_id == schema);
    CHECK(client::validate_interaction_event(context, *event));
    CHECK(calls == 0);
}

TEST_CASE(
    "lazy schemas retain complete metadata and off-page dependency invalidation",
    "[client_interaction_prepared][client_interaction_lazy]") {
    auto context = input_context{"LAZY_SCHEMA"};
    const auto input = client::input_context_scope{context, "LAZY_SCHEMA"};
    auto snapshot = model(3);
    for (auto& choice : snapshot.choices) { choice.description.clear(); }
    auto keys = std::vector<std::string>{"input-0", "input-1", "input-2"};
    const auto identity =
        [&](client::interaction_snapshot value, std::vector<std::string> dependencies) {
            const auto scope = client::prepared_interaction_scope{
                context,
                client::prepare_interaction(
                    context, std::move(value),
                    {
                        .dependency_keys = std::move(dependencies),
                        .render = [](const auto /*ordinal*/) -> std::string {
                            throw std::runtime_error("hash must not render");
                        },
                    })};
            return client::current_interaction({.limit = 0}).schema_id;
        };
    const auto original = identity(snapshot, keys);
    SECTION("off-page dependency") { keys.back() += "new input"; }
    SECTION("ordered dependency") { std::swap(keys.front(), keys.back()); }
    SECTION("off-page metadata") { snapshot.choices.back().denial += "new denial"; }
    SECTION("constraints") { snapshot.choices.back().available_count = 3; }
    SECTION("panes") { snapshot.panes.front().label += "new pane"; }
    CHECK(identity(snapshot, keys) != original);
    // The eager path's historical identity is unchanged and domain-separated from lazy identities.
    CHECK(schema(context, snapshot) != identity(snapshot, keys));
    auto invalid = model(3);
    CHECK_THROWS_AS(
        client::prepare_interaction(
            context, invalid,
            {.dependency_keys = keys, .render = [](const auto /*ordinal*/) { return "text"; }}),
        std::invalid_argument);
    for (auto& choice : invalid.choices) { choice.description.clear(); }
    CHECK_THROWS_AS(
        client::prepare_interaction(
            context, invalid,
            {.dependency_keys = {"missing"},
             .render = [](const auto /*ordinal*/) { return "text"; }}),
        std::invalid_argument);
}

TEST_CASE(
    "lazy renderer reentry and throwing nested providers restore parent authority",
    "[client_interaction_prepared][client_interaction_lazy]") {
    auto context = input_context{"RENDER_PARENT"};
    const auto input = client::input_context_scope{context, "RENDER_PARENT"};
    client::begin_input_boundary();
    auto texts = std::make_shared<const std::vector<std::string>>(
        std::vector<std::string>{"parent zero", "parent one"});
    const auto weak = std::weak_ptr{texts};
    auto snapshot = model(2);
    for (auto& choice : snapshot.choices) { choice.description.clear(); }
    auto fail_child = true;
    auto parent_calls = std::array<int, 2>{};
    auto child_calls = 0;
    auto prepared = client::prepare_interaction(
        context, std::move(snapshot),
        {
            .dependency_keys = {"zero-v1", "one-v1"},
            .render =
                [owned = texts, &fail_child, &parent_calls, &child_calls](const auto ordinal) {
                    ++parent_calls[ordinal];
                    if (ordinal == 0) {
                        // Real same-provider reentry, not an inert nested scope: another row
                        // renders here.
                        const auto other = client::current_interaction({.offset = 1, .limit = 1});
                        REQUIRE(other.choices.size() == 1);
                        CHECK(other.choices.front().description == owned->at(1));
                        auto child_context = input_context{"RENDER_CHILD"};
                        const auto child_input =
                            client::input_context_scope{child_context, "RENDER_CHILD"};
                        auto child_model = model(1);
                        child_model.choices.front().description.clear();
                        const auto child = client::prepared_interaction_scope{
                            child_context,
                            client::prepare_interaction(
                                child_context, std::move(child_model),
                                {
                                    .dependency_keys = {"child-v1"},
                                    .render = [&fail_child, &child_calls](const auto /*index*/)
                                        -> std::string {
                                        ++child_calls;
                                        if (std::exchange(fail_child, false)) {
                                            throw std::runtime_error("nested render failed");
                                        }
                                        return "child text";
                                    },
                                })};
                        const auto nested = client::current_interaction({.limit = 1});
                        CHECK(nested.context == "RENDER_CHILD");
                        CHECK(nested.choices.front().description == "child text");
                    }
                    return owned->at(ordinal);
                },
        });
    texts.reset();
    {
        const auto parent = client::prepared_interaction_scope{context, prepared};
        const auto metadata = client::current_interaction({.limit = 0});
        const auto boundary = client::current_input_id();
        CHECK_THROWS_AS(client::current_interaction({.limit = 1}), std::runtime_error);
        CHECK(client::current_interaction({.limit = 0}).schema_id == metadata.schema_id);
        CHECK(client::current_input_id() == boundary);
        CHECK(client::resolve_interaction_command(command("choice:0")));
        CHECK(parent_calls[0] == 1);
        CHECK(parent_calls[1] == 1);
        const auto rendered = client::current_interaction({.limit = 1});
        REQUIRE(rendered.choices.size() == 1);
        CHECK(rendered.context == "RENDER_PARENT");
        CHECK(rendered.schema_id == metadata.schema_id);
        CHECK(rendered.choices.front().description == "parent zero");
        CHECK(parent_calls[0] == 2); // failed parent row was not cached
        CHECK(parent_calls[1] == 1); // successful reentrant row was cached
        CHECK(child_calls == 2);
        CHECK(
            client::current_interaction({.limit = 1}).choices.front().description == "parent zero");
        CHECK(parent_calls[0] == 2);
        CHECK_FALSE(weak.expired());
    }
    prepared.reset();
    CHECK(weak.expired()); // last actual owner releases renderer inputs; no provider history
}
