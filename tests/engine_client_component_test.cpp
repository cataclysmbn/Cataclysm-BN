#include "cached_options.h"
#include "cata_utility.h"
#include "catch/catch.hpp"
#include "client_input.h"
#include "client_interaction.h"
#include "client_shortcut.h"
#include "engine_client_component.h"
#include "input.h"
#include "ui.h"

#include <algorithm>
#include <map>
#include <nlohmann/json.hpp>
#include <ranges>
#include <string>
#include <variant>
#include <vector>

namespace {
namespace client = game_client;
namespace ec = engine_client;

auto ready() -> ec::readiness {
    return {.phase = "waiting_for_input",
            .game_ready = true,
            .accepts_interaction_commands = true,
            .accepts_registered_actions = true};
}
auto boundary(client::interaction_snapshot native) -> ec::boundary_state {
    native.schema_id = native.schema_id.empty() ? "schema:test" : native.schema_id;
    // A captured boundary is structured; the snapshot's defaults describe a bare actions screen.
    native.structured = true;
    native.actions_only = false;
    return {
        .id = "boundary:test",
        .epoch = "epoch:test",
        .ready = ready(),
        .actions = {{.id = "UP", .name = "Move up", .keys = {"k"}}},
        .interaction = std::move(native),
        .frame = {.dim = "", .x = 100, .y = 200}};
}
auto map_only() -> ec::boundary_state {
    auto state = boundary({});
    state.interaction.reset();
    return state;
}
auto top_of(const ec::boundary_state& state) -> ec::screen_view { return ec::top_screen(state); }
auto find(const ec::screen_view& view, const std::string& id) -> const ec::component& {
    REQUIRE(view.root);
    const auto* const found = ec::find_component(*view.root, id);
    INFO(id);
    REQUIRE(found != nullptr);
    return *found;
}
template <typename Body> auto body(const ec::component& value) -> const Body& {
    REQUIRE(std::holds_alternative<Body>(value.body));
    return std::get<Body>(value.body);
}
auto affordance(const ec::screen_view& view, const std::string& id) -> const ec::affordance& {
    const auto found = std::ranges::find(view.affordances, id, &ec::affordance::id);
    INFO(id);
    REQUIRE(found != view.affordances.end());
    return *found;
}
auto row_ids_of(const ec::component& value) -> std::vector<std::string> {
    const auto ids = ec::row_ids(value);
    REQUIRE(ids);
    return *ids;
}
auto wire(const ec::boundary_state& state) -> nlohmann::json {
    return nlohmann::json::parse(ec::serialize_interaction_boundary(state));
}
auto menu() -> client::interaction_snapshot {
    return {
        .context = "MAIN_MENU",
        .kind = client::interaction_kind::choices,
        .title = "Cataclysm: Bright Nights",
        .allow_cancel = true,
        .choices = {{.id = "new_game:custom", .label = "Custom Character"},
                    {.id = "new_game:tutorial", .label = "Tutorial"}},
        .choice_total = 2,
        .focus_choice_id = "new_game:custom",
    };
}
auto operation_of(const std::expected<ec::command_operation, ec::error>& result)
    -> client::interaction_command {
    const auto why = result ? std::string{"ok"} : ec::error_name(result.error());
    INFO(why);
    REQUIRE(result);
    REQUIRE(std::holds_alternative<ec::semantic_operation>(*result));
    return std::get<ec::semantic_operation>(*result).command;
}
} // namespace

TEST_CASE("a generic list is a list with a choose affordance", "[engine_client_component]") {
    const auto view = top_of(boundary(menu()));
    CHECK(view.role == "dialog");
    CHECK(view.context == "MAIN_MENU");
    const auto& list = body<ec::list_body>(find(view, "list:choices"));
    CHECK(row_ids_of(find(view, "list:choices"))
          == std::vector<std::string>{"new_game:custom", "new_game:tutorial"});
    CHECK(list.selection == "single");
    CHECK(list.activate == "bn:choose:list:choices");
    CHECK(view.focus_component == "list:choices");
    CHECK(view.focus_row == "new_game:custom");
    CHECK(affordance(view, "bn:cancel").role == "cancel");
    CHECK(affordance(view, "UP").keys == std::vector<std::string>{"k"});
}

TEST_CASE(
    "the row any key acknowledges is the any_key affordance, not a list row",
    "[engine_client_component]") {
    const auto view = top_of(boundary(
        {.kind = client::interaction_kind::choices,
         .message = "The door is locked.",
         .choices = {{.id = "acknowledge", .label = "Press any key"}},
         .choice_total = 1,
         .any_key_choice_id = "acknowledge"}));
    REQUIRE(view.root);
    CHECK(ec::find_component(*view.root, "list:choices") == nullptr);
    const auto& any_key = affordance(view, "bn:any_key");
    CHECK(any_key.role == "any_key");
    CHECK(any_key.label == "Press any key");
    CHECK(any_key.target == "acknowledge");
}

TEST_CASE("an affordance label is the native action name", "[engine_client_component]") {
    auto state = boundary(menu());
    // Native has no name for CONFIRM or QUIT here: the label is empty, never the id and never
    // prose.
    CHECK(affordance(top_of(state), "bn:choose:list:choices").label.empty());
    CHECK(affordance(top_of(state), "bn:cancel").label.empty());
    state.actions.push_back({.id = "CONFIRM", .name = "Confirm", .keys = {"RETURN"}});
    state.actions.push_back({.id = "QUIT", .name = "Quit", .keys = {"ESC"}});
    CHECK(affordance(top_of(state), "bn:choose:list:choices").label == "Confirm");
    CHECK(affordance(top_of(state), "bn:cancel").label == "Quit");
}

TEST_CASE("the registered CONFIRM accepts and QUIT leaves", "[engine_client_component]") {
    auto native = menu();
    native.allow_cancel = false;
    auto state = boundary(std::move(native));
    state.actions.push_back({.id = "CONFIRM", .name = "Confirm", .keys = {"RETURN"}});
    state.actions.push_back({.id = "QUIT", .name = "Quit", .keys = {"ESC"}});
    const auto view = top_of(state);
    CHECK(affordance(view, "CONFIRM").role == "confirm");
    CHECK(affordance(view, "QUIT").role == "cancel");
    CHECK(affordance(view, "UP").role == "navigate");
}

TEST_CASE("the wide cursor steps only navigate", "[engine_client_component]") {
    auto state = boundary(menu());
    for (const auto* id : {"LEFT_WIDE", "RIGHT_WIDE", "UP_WIDE", "DOWN_WIDE"}) {
        state.actions.push_back({.id = id, .name = std::string{id} + " name", .keys = {}});
    }
    const auto view = top_of(state);
    for (const auto* id : {"LEFT_WIDE", "RIGHT_WIDE", "UP_WIDE", "DOWN_WIDE"}) {
        INFO(id);
        CHECK(affordance(view, id).role == "navigate");
    }
}

TEST_CASE("a screen publishes each role once", "[engine_client_component]") {
    const auto holders = [](const ec::screen_view& view, const std::string& role) {
        return view.affordances
             | std::views::filter([&](const ec::affordance& each) { return each.role == role; })
             | std::views::transform(&ec::affordance::id) | std::ranges::to<std::vector>();
    };
    auto state = map_only();
    state.actions.push_back({.id = "CONFIRM", .name = "Confirm", .keys = {"RETURN"}});
    state.actions.push_back({.id = "QUIT", .name = "Quit", .keys = {"ESC"}});
    state.actions.push_back({.id = "ANY_INPUT", .name = "Press any key", .keys = {}});
    SECTION("the generic screen's own cancel stands for the registered QUIT, with its keys") {
        auto generic = boundary(menu());
        generic.actions = state.actions;
        const auto view = top_of(generic);
        CHECK(holders(view, "cancel") == std::vector<std::string>{"bn:cancel"});
        CHECK(holders(view, "confirm") == std::vector<std::string>{"CONFIRM"});
        CHECK_FALSE(std::ranges::contains(view.affordances, "QUIT", &ec::affordance::id));
        CHECK(affordance(view, "bn:cancel").keys == std::vector<std::string>{"ESC"});
    }
    SECTION("a provider's own cancel and confirm keep the role over the registered ones") {
        const auto context = input_context("TEST_ROLES");
        const auto scope = ec::screen_scope(
            context, "test", {.view = [](const auto&) {
                return ec::screen_view{
                    .root = ec::panel({.id = "panel:one"}),
                    .affordances =
                        {ec::invoke_affordance({.id = "own:close", .role = "cancel"}),
                         ec::invoke_affordance({.id = "own:ok", .role = "confirm"})}};
            }});
        const auto input = client::input_context_scope(context, "TEST_ROLES");
        const auto view = top_of(state);
        CHECK(holders(view, "cancel") == std::vector<std::string>{"own:close"});
        CHECK(holders(view, "confirm") == std::vector<std::string>{"own:ok"});
        CHECK(holders(view, "any_key") == std::vector<std::string>{"ANY_INPUT"});
        CHECK(affordance(view, "QUIT").runs == ec::invoke_kind::action);
    }
    SECTION("a screen with no cancel of its own takes the registered one") {
        const auto view = top_of(state);
        CHECK(holders(view, "cancel") == std::vector<std::string>{"QUIT"});
    }
}

TEST_CASE("ids come from identity, not from the translated text", "[engine_client_component]") {
    auto korean = menu();
    korean.title = "카타클리즘";
    korean.choices[0].label = "사용자 지정 캐릭터";
    korean.choices[1].label = "튜토리얼";
    const auto english = boundary(menu());
    auto translated = boundary(std::move(korean));
    translated.actions = {{.id = "UP", .name = "위로 이동", .keys = {"k"}}};
    const auto ids = [](const ec::screen_view& view) {
        return view.affordances | std::views::transform(&ec::affordance::id)
             | std::ranges::to<std::vector>();
    };
    const auto a = top_of(english);
    const auto b = top_of(translated);
    CHECK(a.screen_id == b.screen_id);
    CHECK(ids(a) == ids(b));
    CHECK(row_ids_of(find(a, "list:choices")) == row_ids_of(find(b, "list:choices")));
    CHECK(body<ec::list_body>(find(a, "list:choices")).rows[0].label
          != body<ec::list_body>(find(b, "list:choices")).rows[0].label);
    CHECK(affordance(a, "UP").label != affordance(b, "UP").label);
}

TEST_CASE("a paged list counts the rows it does not hold", "[engine_client_component]") {
    auto native = client::interaction_snapshot{
        .kind = client::interaction_kind::choices,
        .choices = {{.id = "a", .label = "A"}, {.id = "b", .label = "B"}},
        .choice_total = 250,
    };
    const auto view = top_of(boundary(std::move(native)));
    CHECK(body<ec::list_body>(*view.root).total == 250);
    CHECK_FALSE(ec::row_ids(*view.root));
}

TEST_CASE("the wire lists at most the maximum rows", "[engine_client_component]") {
    auto native = client::interaction_snapshot{.kind = client::interaction_kind::choices};
    for (auto i = std::size_t{0}; i < ec::maximum_rows + 50; ++i) {
        native.choices.push_back({.id = "row:" + std::to_string(i), .label = "Row"});
    }
    native.choice_total = native.choices.size();
    const auto json = wire(boundary(std::move(native)));
    const auto& list = json["stack"].back()["root"];
    CHECK(list["rows"].size() == ec::maximum_rows);
    CHECK(list["total"] == ec::maximum_rows + 50);
}

TEST_CASE("a field maps to a form that fills and submits", "[engine_client_component]") {
    const auto view = top_of(boundary({
        .kind = client::interaction_kind::field,
        .title = "Name",
        .field = client::interaction_field{.id = "field:name", .label = "Name", .value = "Ada"},
    }));
    const auto& form = body<ec::form_body>(find(view, "form:field"));
    REQUIRE(form.fields.size() == 1);
    CHECK(form.fields[0].value == "Ada");
    CHECK(form.submit == "bn:submit:form:field");
    CHECK(affordance(view, "bn:submit:form:field").role == "confirm");
    CHECK(affordance(view, "bn:fill:form:field").args.text->field_id == "field:name");
    // The screen's own word for typing is the field's label.
    CHECK(affordance(view, "bn:fill:form:field").label == "Name");
}

TEST_CASE("editor rows map to a form beside their list", "[engine_client_component]") {
    const auto state = boundary({
        .kind = client::interaction_kind::choices,
        .choices =
            {{.id = "option:DROP_EMPTY",
              .label = "Drop empty containers",
              .editor =
                  client::interaction_editor{
                      .type = "select",
                      .value = "no",
                      .values = {{.id = "no", .label = "No"}, {.id = "all", .label = "All"}}}}},
        .choice_total = 1,
    });
    const auto view = top_of(state);
    const auto& form = body<ec::form_body>(find(view, "form:editors"));
    REQUIRE(form.fields.size() == 1);
    CHECK(form.fields[0].type == "select");
    CHECK(form.fields[0].options.size() == 2);
    CHECK(row_ids_of(find(view, "list:choices")) == std::vector<std::string>{"option:DROP_EMPTY"});
    const auto json = wire(state);
    const auto& root = json["stack"].back()["root"];
    CHECK(root["children"][1]["id"] == "form:editors");
    const auto& listed = root["children"][0]["rows"][0];
    CHECK_FALSE(listed.contains("editor"));
    CHECK_FALSE(listed.contains("pane_id"));
}

TEST_CASE("a screen with panes or a target is not described yet", "[engine_client_component]") {
    for (auto native : {
             client::interaction_snapshot{
                 .kind = client::interaction_kind::choices,
                 .panes = {{.id = "p", .label = "P"}},
                 .choices = {{.id = "a", .label = "A", .pane_id = "p"}},
                 .choice_total = 1},
             client::interaction_snapshot{
                 .kind = client::interaction_kind::target, .target = client::interaction_target{}},
         }) {
        const auto state = boundary(std::move(native));
        const auto view = top_of(state);
        CHECK_FALSE(view.root);
        CHECK(view.affordances.empty());
        const auto json = wire(state);
        CHECK(json["stack"].back()["root"].is_null());
        // The 0.1 interaction still serves it.
        CHECK(json["interaction"].is_object());
    }
}

TEST_CASE(
    "the stack holds the map, the scoped screens and the read in progress",
    "[engine_client_component]") {
    const auto context = input_context("TEST_SCOPE");
    const auto state = map_only();
    {
        const auto before = ec::interaction_stack(state);
        REQUIRE(before.size() == 1);
        CHECK(before[0].screen_id == "screen:map:epoch:test");
        CHECK(before[0].role == "map");
        CHECK(before[0].kind == "map");
        CHECK(body<ec::map_body>(*before[0].root).mode == "world");
    }
    auto first_id = std::string{};
    {
        const auto scope = ec::screen_scope(
            context, "own",
            {.role = "screen", .view = [](const auto&) {
                 return ec::screen_view{
                     .title = "Own",
                     .root = ec::component{.id = "own", .body = ec::text_body{.markup = "drawn"}},
                     .affordances = {ec::invoke_affordance({.id = "own:go"})}};
             }});
        const auto input = client::input_context_scope(context, "TEST_SCOPE");
        const auto stack = ec::interaction_stack(state);
        REQUIRE(stack.size() == 2);
        CHECK(stack[0].role == "map");
        // Only the top has affordances.
        CHECK(stack[0].affordances.empty());
        CHECK(stack[1].role == "screen");
        CHECK(stack[1].title == "Own");
        CHECK(stack[1].kind == "own");
        CHECK(stack[1].context == "TEST_SCOPE");
        CHECK(affordance(stack[1], "own:go").runs == ec::invoke_kind::screen);
        CHECK(ec::top_screen(state).screen_id == stack[1].screen_id);
        first_id = stack[1].screen_id;
    }
    // Reopened: a new screen, so a key meant for the first never lands on it.
    const auto reopened = ec::screen_scope(context, "own");
    const auto input = client::input_context_scope(context, "TEST_SCOPE");
    CHECK(ec::top_screen(state).screen_id != first_id);
    // The same screen still: a client keeps what it remembers of the first.
    CHECK(ec::top_screen(state).kind == "own");
}

TEST_CASE(
    "a read without a scope is a transient screen with a new id per boundary",
    "[engine_client_component]") {
    auto state = boundary(menu());
    const auto stack = ec::interaction_stack(state);
    REQUIRE(stack.size() == 2);
    CHECK(stack[0].role == "map");
    CHECK(stack[0].affordances.empty());
    // A covered screen's components name no affordance either.
    CHECK(stack[0].root->affordances.empty());
    CHECK(stack[1].screen_id == "screen:read:boundary:test");
    CHECK(stack[1].role == "dialog");
    // Nothing names it, so a client remembers nothing of it.
    CHECK(stack[1].kind.empty());
    state.id = "boundary:next";
    CHECK(ec::top_screen(state).screen_id == "screen:read:boundary:next");
    // The map's id is fixed for the epoch.
    CHECK(ec::interaction_stack(state)[0].screen_id == stack[0].screen_id);
    state.ready.game_ready = false;
    const auto loading = ec::interaction_stack(state);
    REQUIRE(loading.size() == 1);
    CHECK(loading[0].role == "screen");
}

TEST_CASE(
    "an expectation scope follows the arguments an affordance takes", "[engine_client_component]") {
    const auto view = top_of(boundary(menu()));
    CHECK(ec::scope_of(affordance(view, "bn:cancel")) == ec::expect_scope::screen);
    CHECK(ec::scope_of(affordance(view, "UP")) == ec::expect_scope::screen);
    CHECK(ec::scope_of(affordance(view, "bn:choose:list:choices")) == ec::expect_scope::boundary);
    const auto map = top_of(map_only());
    CHECK(ec::scope_of(affordance(map, "bn:travel")) == ec::expect_scope::boundary);
    CHECK(ec::scope_of(affordance(map, "bn:pickup")) == ec::expect_scope::boundary);
    CHECK(ec::scope_of({.args = {.count = ec::count_shape{}}}) == ec::expect_scope::screen);
    CHECK(ec::scope_name(ec::expect_scope::screen) == "screen");
    CHECK(ec::scope_name(ec::expect_scope::boundary) == "boundary");
}

TEST_CASE("the wire carries the stack beside the 0.1 interaction", "[engine_client_component]") {
    const auto state = boundary(menu());
    const auto text = ec::serialize_interaction_boundary(state);
    for (const auto* removed :
         {"\"direction\"", "\"placement\"", "\"show\"", "\"modal\"", "\"style\"", "\"layout\""}) {
        INFO(removed);
        CHECK(text.find(removed) == std::string::npos);
    }
    const auto json = nlohmann::json::parse(text);
    CHECK(json["interaction"]["choice_total"] == 2);
    REQUIRE(json["stack"].size() == 2);
    const auto& top = json["stack"][1];
    CHECK(top["screen_id"] == "screen:read:boundary:test");
    CHECK(top["root"]["kind"] == "list");
    CHECK(top["focus"]["component_id"] == "list:choices");
    const auto choose = std::ranges::find_if(top["affordances"], [](const auto& entry) {
        return entry["id"] == "bn:choose:list:choices";
    });
    REQUIRE(choose != top["affordances"].end());
    CHECK((*choose)["expect"] == "boundary");
    CHECK(json["stack"][0]["affordances"].empty());
    CHECK(json["stack"][0]["root"]["kind"] == "map");
}

TEST_CASE(
    "the map names the avatar's item lists and the verb that moves a row out of each",
    "[engine_client_component]") {
    const auto expected = nlohmann::json::parse(R"([
        {"id": "avatar.ground", "label": "Ground", "verb": "Pick up"},
        {"id": "avatar.inventory", "label": "Inventory", "verb": "Drop"}
    ])");
    CHECK(wire(map_only())["stack"][0]["root"]["item_lists"] == expected);
    // A covered map still names them.
    CHECK(wire(boundary(menu()))["stack"][0]["root"]["item_lists"] == expected);
}

TEST_CASE("every 0.1 operation is an invoke", "[engine_client_component]") {
    const auto state = boundary(menu());
    SECTION("choose") {
        const auto command = operation_of(ec::resolve_invoke(
            state,
            {.affordance_id = "bn:choose:list:choices", .args = {.row = "new_game:tutorial"}}));
        CHECK(command.operation == client::interaction_operation::choose);
        CHECK(command.target_id == "new_game:tutorial");
    }
    SECTION("cancel") {
        CHECK(operation_of(ec::resolve_invoke(state, {.affordance_id = "bn:cancel"})).operation
              == client::interaction_operation::cancel);
    }
    SECTION("registered action") {
        const auto result = ec::resolve_invoke(state, {.affordance_id = "UP"});
        REQUIRE(result);
        CHECK(std::get<ec::registered_action>(*result).id == "UP");
    }
    SECTION("fill and submit") {
        const auto field = boundary(
            {.kind = client::interaction_kind::field,
             .field = client::interaction_field{.id = "field:name"}});
        const auto typed = operation_of(ec::resolve_invoke(
            field, {.affordance_id = "bn:fill:form:field", .args = {.text = "Ada"}}));
        CHECK(typed.operation == client::interaction_operation::fill);
        CHECK(typed.target_id == "field:name");
        CHECK(typed.value == "Ada");
        CHECK(typed.submit == false);
        CHECK(
            operation_of(
                ec::resolve_invoke(
                    field, {.affordance_id = "bn:submit:form:field", .args = {.text = "Ada"}}))
                .submit);
        const auto editors = boundary(
            {.kind = client::interaction_kind::choices,
             .choices = {{.id = "option:X", .label = "X", .editor = client::interaction_editor{}}},
             .choice_total = 1});
        const auto set = operation_of(ec::resolve_invoke(
            editors,
            {.affordance_id = "bn:submit:form:editors", .args = {.row = "option:X", .text = "8"}}));
        CHECK(set.target_id == "option:X");
        CHECK(set.submit);
    }
    SECTION("set_count, offered by a screen's own view") {
        const auto context = input_context("TEST_COUNT");
        const auto scope = ec::screen_scope(
            context, "test", {.view = [](const auto&) {
                return ec::screen_view{
                    .root = ec::list(
                        {.id = "list:own", .rows = {{.id = "item:1", .label = "rock"}}}),
                    .affordances = {
                        {.id = "own:count",
                         .args = {.row = ec::row_shape{.component_id = "list:own"},
                                  .count = ec::count_shape{.min = 1, .max = 3}},
                         .runs = ec::invoke_kind::set_count}}};
            }});
        const auto input = client::input_context_scope(context, "TEST_COUNT");
        const auto command = operation_of(ec::resolve_invoke(
            map_only(), {.affordance_id = "own:count", .args = {.row = "item:1", .count = 2}}));
        CHECK(command.operation == client::interaction_operation::set_count);
        CHECK(command.target_id == "item:1");
        CHECK(command.count == 2U);
        const auto over = ec::resolve_invoke(
            map_only(), {.affordance_id = "own:count", .args = {.row = "item:1", .count = 4}});
        REQUIRE_FALSE(over);
        CHECK(over.error() == ec::error::invalid_arguments);
    }
    SECTION("travel, context, pickup and drop at the map") {
        const auto map = map_only();
        const auto at = ec::position{.dim = "", .x = 9, .y = 4};
        const auto travel =
            ec::resolve_invoke(map, {.affordance_id = "bn:travel", .args = {.pos = at}});
        REQUIRE(travel);
        CHECK(std::get<ec::travel_operation>(*travel).button == "left");
        const auto context =
            ec::resolve_invoke(map, {.affordance_id = "bn:context", .args = {.pos = at}});
        REQUIRE(context);
        CHECK(std::get<ec::travel_operation>(*context).button == "right");
        const auto pickup = ec::resolve_invoke(
            map, {.affordance_id = "bn:pickup", .args = {.row = "item:3f9a", .count = 2}});
        REQUIRE(pickup);
        CHECK_FALSE(std::get<ec::item_operation>(*pickup).drop);
        CHECK(std::get<ec::item_operation>(*pickup).count == 2U);
        const auto drop = ec::resolve_invoke(
            map, {.affordance_id = "bn:drop", .args = {.row = "item:77c1", .count = 1}});
        REQUIRE(drop);
        CHECK(std::get<ec::item_operation>(*drop).drop);
    }
}

TEST_CASE(
    "an affordance the screen runs itself reaches its loop as an invoke",
    "[engine_client_component]") {
    const auto context = input_context("TEST_OWN");
    const auto scope = ec::screen_scope(
        context, "test",
        {.role = "screen", .view = [](const auto&) {
             return ec::screen_view{
                 .root = ec::list({.id = "list:own", .rows = {{.id = "row:1", .label = "One"}}}),
                 .affordances = {ec::invoke_affordance(
                     {.id = "own:pick",
                      .args = {.row = ec::row_shape{.component_id = "list:own"}}})}};
         }});
    const auto input = client::input_context_scope(context, "TEST_OWN");
    const auto state = map_only();
    const auto resolved =
        ec::resolve_invoke(state, {.affordance_id = "own:pick", .args = {.row = "row:1"}});
    REQUIRE(resolved);
    const auto& invoke = std::get<ec::invoke_operation>(*resolved);
    CHECK(invoke.affordance_id == "own:pick");
    CHECK(invoke.args.row == "row:1");
    auto event = input_event{};
    event.type = input_event_t::interaction;
    event.interaction = client::interaction_event{
        .operation = client::interaction_operation::invoke,
        .target_id = "own:pick",
        .arguments = client::interaction_arguments{.row = "row:1"}};
    const auto read = ec::invoked(event);
    REQUIRE(read);
    CHECK(read->affordance_id == "own:pick");
    CHECK(read->args.row == "row:1");
    event.interaction->operation = client::interaction_operation::choose;
    CHECK_FALSE(ec::invoked(event));
    CHECK_FALSE(ec::invoked(input_event{}));
    const auto missing = ec::resolve_invoke(state, {.affordance_id = "own:pick"});
    REQUIRE_FALSE(missing);
    CHECK(missing.error() == ec::error::invalid_arguments);
}

TEST_CASE("invoke rejects what the affordance does not take", "[engine_client_component]") {
    const auto state = boundary(menu());
    const auto fails = [&](const ec::boundary_state& at, const ec::invoke_operation& invoke) {
        const auto result = ec::resolve_invoke(at, invoke);
        REQUIRE_FALSE(result);
        return result.error();
    };
    CHECK(fails(state, {.affordance_id = "bn:nothing"}) == ec::error::unknown_affordance);
    // The map's affordances are not the top screen's.
    CHECK(fails(state, {.affordance_id = "bn:travel", .args = {.pos = ec::position{}}})
          == ec::error::unknown_affordance);
    // A missing, an extra, a foreign or an empty argument.
    CHECK(
        fails(state, {.affordance_id = "bn:choose:list:choices"}) == ec::error::invalid_arguments);
    CHECK(fails(state, {.affordance_id = "bn:cancel", .args = {.row = "new_game:custom"}})
          == ec::error::invalid_arguments);
    CHECK(fails(state, {.affordance_id = "bn:choose:list:choices", .args = {.row = "tab:load"}})
          == ec::error::invalid_arguments);
    CHECK(fails(state, {.affordance_id = "bn:choose:list:choices", .args = {.row = ""}})
          == ec::error::invalid_arguments);
    auto map = map_only();
    CHECK(fails(map, {.affordance_id = "bn:pickup", .args = {.row = "item:1", .count = 0}})
          == ec::error::invalid_arguments);
    map.ready.accepts_registered_actions = false;
    CHECK(fails(map, {.affordance_id = "bn:travel", .args = {.pos = ec::position{}}})
          == ec::error::not_ready);
    auto closed = boundary(menu());
    closed.ready.accepts_interaction_commands = false;
    CHECK(fails(closed, {.affordance_id = "bn:cancel"}) == ec::error::not_ready);
    const auto field = boundary(
        {.kind = client::interaction_kind::field,
         .field = client::interaction_field{.id = "field:name"}});
    const auto long_text = std::string(ec::maximum_inline_bytes + 1, 'a');
    CHECK(fails(field, {.affordance_id = "bn:fill:form:field", .args = {.text = long_text}})
          == ec::error::invalid_arguments);
    CHECK(ec::error_name(ec::error::invalid_arguments) == "invalid_arguments");
    CHECK(ec::error_name(ec::error::unknown_affordance) == "unknown_affordance");
    CHECK(ec::error_name(ec::error::stale_screen) == "stale_screen");
}

TEST_CASE("before the game offers anything an invoke is not ready", "[engine_client_component]") {
    auto state = map_only();
    state.ready = {.phase = "starting"};
    state.actions.clear();
    const auto early = ec::resolve_invoke(state, {.affordance_id = "bn:travel"});
    REQUIRE_FALSE(early);
    CHECK(early.error() == ec::error::not_ready);
}

namespace {
auto numbered_rows(const std::size_t count) -> std::vector<client::interaction_choice> {
    return std::views::iota(std::size_t{0}, count) | std::views::transform([](const auto i) {
               return client::interaction_choice{.id = "row:" + std::to_string(i), .label = "Row"};
           })
         | std::ranges::to<std::vector>();
}
auto ids_of(const ec::rows_page& page) -> std::vector<std::string> {
    return page.rows | std::views::transform(&ec::row::id) | std::ranges::to<std::vector>();
}
auto rows_at(const std::size_t offset, const std::size_t limit) -> ec::rows_request {
    return {
        .epoch = "epoch:test",
        .boundary_id = "boundary:test",
        .component_id = "list:choices",
        .offset = offset,
        .limit = limit};
}
} // namespace

TEST_CASE("rows are paged within the list a component names", "[engine_client_component]") {
    const auto state = boundary(
        {.kind = client::interaction_kind::choices, .choices = numbered_rows(5), .choice_total = 5});
    SECTION("a window of a complete list is sliced from its own rows") {
        const auto page = ec::read_rows(rows_at(2, 2), state);
        REQUIRE(page);
        CHECK(ids_of(*page) == std::vector<std::string>{"row:2", "row:3"});
        CHECK(page->total == 5);
        CHECK(page->component_id == "list:choices");
        CHECK(page->boundary_id == "boundary:test");
    }
    SECTION("a window past the end is empty and states the total") {
        const auto page = ec::read_rows(rows_at(9, 10), state);
        REQUIRE(page);
        CHECK(page->rows.empty());
        CHECK(page->total == 5);
    }
    SECTION("a component that is not a list of the top screen is refused") {
        auto request = rows_at(0, 10);
        request.component_id = "list:elsewhere";
        const auto missing = ec::read_rows(request, state);
        REQUIRE_FALSE(missing);
        CHECK(missing.error() == ec::error::validation_failed);
        const auto map = ec::read_rows(
            {.epoch = "epoch:test", .boundary_id = "boundary:test", .component_id = "map:world"},
            map_only());
        REQUIRE_FALSE(map);
        CHECK(map.error() == ec::error::validation_failed);
    }
    SECTION("the window is bounded and the boundary must be live") {
        CHECK(ec::read_rows(rows_at(0, 0), state).error() == ec::error::resource_limit);
        CHECK(ec::read_rows(rows_at(0, ec::maximum_rows + 1), state).error()
              == ec::error::resource_limit);
        CHECK(ec::read_rows(rows_at(0, ec::maximum_rows), state));
        auto stale = rows_at(0, 10);
        stale.epoch = "epoch:old";
        CHECK(ec::read_rows(stale, state).error() == ec::error::stale_epoch);
        stale = rows_at(0, 10);
        stale.boundary_id = "boundary:old";
        CHECK(ec::read_rows(stale, state).error() == ec::error::stale_boundary);
    }
}

TEST_CASE(
    "a screen's list puts on the wire the rows that fit its byte budget and pages the rest",
    "[engine_client_component]") {
    // Two hundred rows of a kilobyte each are far over the budget of one list.
    constexpr auto count = std::size_t{200};
    auto rows = std::vector<ec::row>{};
    for (const auto index : std::views::iota(std::size_t{0}, count)) {
        rows.push_back(
            {.id = "row:" + std::to_string(index), .description = std::string(1024, 'x')});
    }
    const auto context = input_context("TEST_LONG");
    const auto scope = ec::screen_scope(
        context, "test", {.view = [&](const auto&) {
            return ec::screen_view{
                .root = ec::list({.id = "list:long", .rows = rows, .activate = "own:pick"}),
                .affordances = {ec::invoke_affordance(
                    {.id = "own:pick",
                     .args = {.row = ec::row_shape{.component_id = "list:long"}}})}};
        }});
    const auto input = client::input_context_scope(context, "TEST_LONG");
    const auto state = map_only();

    SECTION("the boundary lists a prefix and states the total") {
        const auto json = wire(state);
        const auto& list = json["stack"].back()["root"];
        REQUIRE(list["rows"].size() < count);
        CHECK_FALSE(list["rows"].empty());
        CHECK(list["rows"].dump().size() <= ec::maximum_list_bytes + 2000);
        CHECK(list["total"] == count);
        CHECK(list["rows"].front()["id"] == "row:0");
        CHECK(ec::serialize_interaction_boundary(state).size() <= ec::maximum_inline_bytes);
    }
    SECTION("a page is a prefix of its window that fits the budget, and pages reach every row") {
        auto seen = std::vector<std::string>{};
        while (seen.size() < count) {
            const auto page = ec::read_rows(
                {.epoch = "epoch:test",
                 .boundary_id = "boundary:test",
                 .component_id = "list:long",
                 .offset = seen.size(),
                 .limit = ec::maximum_rows},
                state);
            REQUIRE(page);
            const auto json = nlohmann::json::parse(ec::serialize_rows(*page));
            REQUIRE_FALSE(json["rows"].empty());
            CHECK(json["total"] == count);
            CHECK(ec::serialize_rows(*page).size() <= ec::maximum_list_bytes + 2000);
            for (const auto& row : json["rows"]) { seen.push_back(row["id"]); }
        }
        CHECK(
            seen
            == ids_of(*ec::read_rows(
                {.epoch = "epoch:test",
                 .boundary_id = "boundary:test",
                 .component_id = "list:long",
                 .limit = ec::maximum_rows},
                state)));
    }
    SECTION("a row past the wire's prefix is still a row of the list") {
        CHECK(ec::resolve_invoke(
            state,
            {.affordance_id = "own:pick", .args = {.row = "row:" + std::to_string(count - 1)}}));
    }
}

TEST_CASE(
    "a partial list pages natively beyond the page the boundary holds",
    "[engine_client_component]") {
    const auto context = input_context("TEST_PAGED");
    const auto input = client::input_context_scope(context, "TEST_PAGED");
    const auto scope = client::interaction_scope(context, [] {
        return client::interaction_snapshot{
            .kind = client::interaction_kind::choices,
            .choices = numbered_rows(250),
            .choice_total = 250};
    });
    client::begin_input_boundary();
    // The boundary holds the first page only.
    const auto state = boundary(
        {.kind = client::interaction_kind::choices,
         .choices = numbered_rows(2),
         .choice_total = 250});
    const auto page = ec::read_rows(rows_at(100, 2), state);
    REQUIRE(page);
    CHECK(ids_of(*page) == std::vector<std::string>{"row:100", "row:101"});
    CHECK(page->total == 250);
    // The wire page names its list and carries the rows as the boundary does.
    const auto json = nlohmann::json::parse(ec::serialize_rows(*page));
    CHECK(json["component_id"] == "list:choices");
    CHECK(json["total"] == 250);
    CHECK(json["rows"][0]["id"] == "row:100");
}

TEST_CASE(
    "a row listed by another identity is chosen by its native id", "[engine_client_component]") {
    auto native = menu();
    native.choices[0].id = "entry:0";
    native.choices[0].identity = "entry:7";
    native.choices[1].id = "entry:1";
    native.choices[1].identity = "entry:7#1";
    native.focus_choice_id = "entry:1";
    const auto state = boundary(std::move(native));
    const auto view = top_of(state);
    CHECK(
        row_ids_of(find(view, "list:choices")) == std::vector<std::string>{"entry:7", "entry:7#1"});
    CHECK(view.focus_row == "entry:7#1");
    const auto command = operation_of(ec::resolve_invoke(
        state, {.affordance_id = "bn:choose:list:choices", .args = {.row = "entry:7#1"}}));
    CHECK(command.target_id == "entry:1");
    // The 0.1 position id is not a row of the 0.2 list.
    const auto old = ec::resolve_invoke(
        state, {.affordance_id = "bn:choose:list:choices", .args = {.row = "entry:1"}});
    REQUIRE_FALSE(old);
    CHECK(old.error() == ec::error::invalid_arguments);
    // The 0.1 interaction keeps its ids.
    CHECK(wire(state)["interaction"]["choices"][0]["id"] == "entry:0");
}

TEST_CASE("a row past the first page is chosen by its native id", "[engine_client_component]") {
    const auto keyed = [](const std::size_t count) {
        auto rows = numbered_rows(count);
        for (auto i = std::size_t{0}; i < count; ++i) {
            rows[i].id = "entry:" + std::to_string(i);
            rows[i].identity = "key:" + std::to_string(i);
        }
        return rows;
    };
    const auto context = input_context("TEST_KEYED");
    const auto input = client::input_context_scope(context, "TEST_KEYED");
    const auto scope = client::interaction_scope(context, [&] {
        return client::interaction_snapshot{
            .kind = client::interaction_kind::choices, .choices = keyed(450), .choice_total = 450};
    });
    client::begin_input_boundary();
    // The boundary holds the first page only.
    const auto state = boundary(
        {.kind = client::interaction_kind::choices, .choices = keyed(2), .choice_total = 450});
    const auto page = ec::read_rows(rows_at(300, 1), state);
    REQUIRE(page);
    REQUIRE(page->rows.size() == 1);
    CHECK(page->rows[0].id == "key:300");
    const auto command = operation_of(ec::resolve_invoke(
        state, {.affordance_id = "bn:choose:list:choices", .args = {.row = page->rows[0].id}}));
    CHECK(command.target_id == "entry:300");
    // A row on the held page resolves the same way.
    CHECK(
        operation_of(
            ec::resolve_invoke(
                state, {.affordance_id = "bn:choose:list:choices", .args = {.row = "key:1"}}))
            .target_id
        == "entry:1");
}

TEST_CASE("a row says whether it is switched on", "[engine_client_component]") {
    auto native = menu();
    native.choices[0].toggled = true;
    native.choices[1].toggled = false;
    const auto state = boundary(native);
    const auto& rows = body<ec::list_body>(find(top_of(state), "list:choices")).rows;
    CHECK(rows[0].toggled == std::optional<bool>{true});
    CHECK(rows[1].toggled == std::optional<bool>{false});
    const auto listed = wire(state)["stack"].back()["root"]["rows"];
    CHECK(listed[0]["toggled"] == true);
    CHECK(listed[1]["toggled"] == false);
    // A row that is no switch says nothing, and the 0.1 choice never carries it.
    CHECK_FALSE(wire(boundary(menu()))["stack"].back()["root"]["rows"][0].contains("toggled"));
    CHECK_FALSE(wire(state)["interaction"]["choices"][0].contains("toggled"));
}

TEST_CASE("a list says whether it can be filtered", "[engine_client_component]") {
    auto native = menu();
    native.filterable = true;
    auto state = boundary(native);
    state.actions.push_back({.id = "FILTER", .name = "Filter", .keys = {"/"}});
    const auto view = top_of(state);
    CHECK(body<ec::list_body>(find(view, "list:choices")).filterable);
    CHECK(std::ranges::contains(find(view, "list:choices").affordances, "FILTER"));
    CHECK(affordance(view, "FILTER").label == "Filter");
    CHECK(wire(state)["stack"].back()["root"]["filterable"] == true);

    SECTION("a list that cannot be filtered offers no filter") {
        auto plain = boundary(menu());
        plain.actions.push_back({.id = "FILTER", .name = "Filter", .keys = {"/"}});
        CHECK_FALSE(body<ec::list_body>(find(top_of(plain), "list:choices")).filterable);
        CHECK_FALSE(
            std::ranges::contains(find(top_of(plain), "list:choices").affordances, "FILTER"));
        CHECK_FALSE(wire(plain)["stack"].back()["root"].contains("filterable"));
    }
    SECTION("a filter that leaves no row keeps the list that changes it") {
        native.choices.clear();
        native.choice_total = 0;
        native.focus_choice_id.clear();
        const auto empty = top_of(boundary(native));
        CHECK(body<ec::list_body>(find(empty, "list:choices")).filterable);
        CHECK(body<ec::list_body>(find(empty, "list:choices")).rows.empty());
    }
}

TEST_CASE(
    "a uilist publishes its filtered rows, switches and filterability",
    "[engine_client_component]") {
    // A list is built only outside the test harness; nothing here reads input.
    const auto live = restore_on_out_of_scope<bool>(test_mode);
    test_mode = false;
    auto menu = uilist();
    menu.addentry(1, true, 'a', "Teleport");
    menu.addentry(2, true, 'b', "Spawn monster");
    menu.entries.back().toggled = true;
    menu.addentry(3, true, 'c', "Teleport to overmap");
    menu.filterlist();
    const auto labels = [](const client::interaction_snapshot& snapshot) {
        return snapshot.choices | std::views::transform(&client::interaction_choice::label)
             | std::ranges::to<std::vector>();
    };
    SECTION("the filter keeps the rows it matches") {
        menu.set_filter("tele");
        const auto snapshot = menu.as_interaction();
        CHECK(labels(snapshot) == std::vector<std::string>{"Teleport", "Teleport to overmap"});
        CHECK(snapshot.filterable);
    }
    SECTION("a switch carries its state") {
        const auto snapshot = menu.as_interaction();
        CHECK_FALSE(snapshot.choices[0].toggled);
        CHECK(snapshot.choices[1].toggled == std::optional<bool>{true});
    }
    SECTION("a list that does not filter says so") {
        menu.filtering = false;
        CHECK_FALSE(menu.as_interaction().filterable);
    }
}

TEST_CASE(
    "a uilist row is known by its entry's return value, not by its place",
    "[engine_client_component]") {
    // A list is built only outside the test harness; nothing here reads input.
    const auto live = restore_on_out_of_scope<bool>(test_mode);
    test_mode = false;
    const auto ids_by_label = [](const std::vector<std::pair<std::string, int>>& entries) {
        auto menu = uilist();
        for (const auto& [label, retval] : entries) { menu.addentry(retval, true, -1, label); }
        menu.filterlist();
        const auto state = boundary(menu.as_interaction());
        auto result = std::map<std::string, std::string>{};
        for (const auto& row : body<ec::list_body>(find(top_of(state), "list:choices")).rows) {
            result[row.label] = row.id;
        }
        return result;
    };
    const auto first = ids_by_label({{"Heal", 7}, {"Repair", 9}, {"Craft", 12}});
    const auto reordered = ids_by_label({{"Craft", 12}, {"Heal", 7}, {"Repair", 9}});
    const auto inserted = ids_by_label({{"Eat", 3}, {"Heal", 7}, {"Repair", 9}, {"Craft", 12}});
    CHECK(first == reordered);
    for (const auto& [label, id] : first) { CHECK(inserted.at(label) == id); }
    CHECK(first.at("Heal") == "entry:7");
    // Two entries that return the same value are told apart by their order among them.
    const auto twins = ids_by_label({{"One", 5}, {"Two", 5}});
    CHECK(twins.at("One") == "entry:5");
    CHECK(twins.at("Two") == "entry:5#1");
}

TEST_CASE(
    "a row an affordance names must be one the component or the lists inside its panes hold",
    "[engine_client_component]") {
    const auto left = ec::list({.id = "list:left", .rows = {{.id = "item:1"}, {.id = "item:2"}}});
    const auto right = ec::list(
        {.id = "list:right",
         .rows = {{.id = "heading", .header = true},
                  {.id = "item:3"},
                  {.id = "item:5", .denial = "No room.", .selectable = false}}});
    const auto paged = ec::list({.id = "list:paged", .rows = {{.id = "item:4"}}, .total = 9});
    // A screen of its own with one affordance per container, each naming rows of that container.
    const auto context = input_context("TEST_ROWS");
    const auto scope = ec::screen_scope(
        context, "test", {.view = [&](const auto&) {
            const auto naming = [](const std::string& id, const std::string& component_id) {
                return ec::affordance{
                    .id = id,
                    .args = {.row = ec::row_shape{.component_id = component_id}},
                    .runs = ec::invoke_kind::screen};
            };
            return ec::screen_view{
                .root = ec::split(
                    {.id = "split:root",
                     .children = ec::children_of(
                         ec::split({.id = "split:two", .children = ec::children_of(left, right)}),
                         ec::panel({.id = "panel:one", .child = left}),
                         ec::tabs(
                             {.id = "tabs:page",
                              .tabs = {{.id = "tab:a"},
                                       {.id = "tab:b"},
                                       {.id = "tab:off", .enabled = false}},
                              .child = left}),
                         ec::panel({.id = "panel:paged", .child = paged}))}),
                .affordances =
                    {naming("own:split", "split:two"), naming("own:panel", "panel:one"),
                     naming("own:tabs", "tabs:page"), naming("own:paged", "panel:paged")}};
        }});
    const auto input = client::input_context_scope(context, "TEST_ROWS");
    const auto resolve = [](const std::string& affordance, const std::string& row) {
        return ec::resolve_invoke(map_only(), {.affordance_id = affordance, .args = {.row = row}});
    };
    const auto refused = [&](const std::string& affordance, const std::string& row) {
        const auto result = resolve(affordance, row);
        INFO(affordance << " " << row);
        REQUIRE_FALSE(result);
        CHECK(result.error() == ec::error::invalid_arguments);
    };

    SECTION("a split holds the rows of both its lists and no heading") {
        CHECK(resolve("own:split", "item:1"));
        CHECK(resolve("own:split", "item:3"));
        refused("own:split", "item:9");
        refused("own:split", "heading");
    }
    SECTION("a panel holds the rows of the list inside it") {
        CHECK(resolve("own:panel", "item:2"));
        refused("own:panel", "item:3");
        refused("own:panel", "tab:a");
    }
    SECTION("a tab strip holds its tabs, not the rows of its page") {
        CHECK(resolve("own:tabs", "tab:b"));
        refused("own:tabs", "item:1");
        refused("own:tabs", "tab:c");
    }
    SECTION("a row or tab the screen does not let be picked is no row an affordance may name") {
        refused("own:split", "item:5");
        refused("own:tabs", "tab:off");
        CHECK(resolve("own:tabs", "tab:a"));
    }
    SECTION("the rows a paged list did not read are checked natively") {
        CHECK_FALSE(ec::row_ids(*ec::top_screen(map_only()).root));
    }
}

TEST_CASE(
    "an action the screen denies is published disabled with the reason",
    "[engine_client_component]") {
    const auto context = input_context("TEST_DENIAL");
    const auto scope = ec::screen_scope(
        context, "test", {.view = [](const auto&) {
            return ec::screen_view{
                .root = ec::panel({.id = "panel:one"}),
                .denials = {{.id = "REPAIR", .reason = "Nothing is damaged."}, {.id = "UNBOUND"}}};
        }});
    const auto input = client::input_context_scope(context, "TEST_DENIAL");
    auto state = map_only();
    state.actions.push_back({.id = "REPAIR", .name = "Repair", .keys = {"r"}});
    state.actions.push_back({.id = "INSTALL", .name = "Install", .keys = {"i"}});
    const auto view = top_of(state);

    CHECK_FALSE(affordance(view, "REPAIR").enabled);
    CHECK(affordance(view, "REPAIR").denial == "Nothing is damaged.");
    CHECK(affordance(view, "INSTALL").enabled);
    CHECK(affordance(view, "INSTALL").denial.empty());
    // A denial names no affordance of its own, and a refused action is not resolved.
    CHECK(std::ranges::none_of(view.affordances, [](const auto& each) {
        return each.id == "UNBOUND";
    }));
    CHECK_FALSE(ec::resolve_invoke(state, {.affordance_id = "REPAIR"}).has_value());
    const auto listed = wire(state)["stack"].back()["affordances"];
    const auto repair = std::ranges::find_if(listed, [](const auto& each) {
        return each["id"] == "REPAIR";
    });
    REQUIRE(repair != listed.end());
    CHECK((*repair)["enabled"] == false);
    CHECK((*repair)["denial"] == "Nothing is damaged.");
}

namespace {
/// The objects under `value` that the player could pick but that are off: no `selectable: false`
/// row.
auto off_on_wire(const nlohmann::json& value, std::vector<nlohmann::json>& found) -> void {
    if (value.is_object() && value.value("enabled", true) == false
        && value.value("selectable", true)) {
        found.push_back(value);
    }
    if (value.is_structured()) {
        for (const auto& member : value) { off_on_wire(member, found); }
    }
}
} // namespace

TEST_CASE(
    "everything off that the player can pick carries a reason on the wire",
    "[engine_client_component]") {
    const auto context = input_context("TEST_OFF");
    const auto scope = ec::screen_scope(
        context, "test", {.view = [](const auto&) {
            const auto rows = ec::list(
                {.id = "list:rows",
                 .rows =
                     {{.id = "row:on"},
                      {.id = "row:off", .enabled = false},
                      {.id = "row:full", .denial = "No room.", .enabled = false},
                      {.id = "heading", .enabled = false, .selectable = false, .header = true}}});
            const auto grid = ec::component{
                .id = "grid:cells",
                .body = ec::grid_body{
                    .cols = 2,
                    .rows = 1,
                    .cells = {{.at = {0, 0}, .id = "cell:on"},
                              {.at = {1, 0}, .id = "cell:off", .enabled = false}}}};
            return ec::screen_view{
                .root = ec::split(
                    {.id = "split:root",
                     .children = ec::children_of(
                         ec::tabs({.id = "tabs:page",
                                   .tabs = {{.id = "tab:on"}, {.id = "tab:off", .enabled = false}},
                                   .child = rows}),
                         grid)}),
                .affordances = {ec::invoke_affordance({.id = "own:off", .enabled = false})}};
        }});
    const auto input = client::input_context_scope(context, "TEST_OFF");
    auto found = std::vector<nlohmann::json>{};
    off_on_wire(wire(map_only())["stack"].back(), found);

    // The off affordance, row, row with its own reason, tab and cell; the heading is not picked.
    REQUIRE(found.size() == 5);
    for (const auto& each : found) {
        INFO(each.dump());
        REQUIRE(each.contains("denial"));
        CHECK_FALSE(each["denial"].get<std::string>().empty());
    }
    CHECK(
        std::ranges::count_if(found, [](const auto& each) { return each["denial"] == "No room."; })
        == 1);
}

namespace {
auto holdable_on_wire(const ec::boundary_state& state, const std::string& id) -> bool {
    const auto json = wire(state);
    const auto& affordances = json["stack"].back()["affordances"];
    const auto found = std::ranges::find_if(affordances, [&](const nlohmann::json& entry) {
        return entry["id"].get<std::string>() == id;
    });
    REQUIRE(found != affordances.end());
    return found->value("holdable", false);
}
} // namespace

TEST_CASE("only a step of the map may be held", "[engine_client_component][held_walk]") {
    CHECK(holdable_on_wire(map_only(), "UP"));
    CHECK_FALSE(holdable_on_wire(map_only(), "bn:travel"));
    CHECK_FALSE(holdable_on_wire(map_only(), "bn:pickup"));
    // The same action on a list is a key press, not a walk.
    CHECK_FALSE(holdable_on_wire(boundary(menu()), "UP"));
}

namespace {
auto invoke_of(const std::string& affordance_id, const std::string& row_id) -> input_event {
    auto event = input_event{};
    event.type = input_event_t::keyboard;
    event.interaction = client::interaction_event{
        .operation = client::interaction_operation::invoke,
        .target_id = affordance_id,
        .arguments = client::interaction_arguments{.row = row_id}};
    return event;
}
/// A screen of its own with one list; `pick` is the affordance the list activates and `role` its
/// role.
struct picking_options {
    std::string role = {};
    std::string activate = {};
    std::string covers = {};
    std::string denial = {};
};
auto picking_screen(const picking_options& options) -> ec::screen_view {
    const auto context = input_context("TEST_PICK");
    const auto scope = ec::screen_scope(
        context, "test", {.view = [&](const auto&) {
            return ec::screen_view{
                .root = ec::list(
                    {.id = "list:rows",
                     .rows = {{.id = "r:1"}, {.id = "r:2"}},
                     .activate = options.activate}),
                .affordances = {ec::invoke_affordance(
                    {.id = "t:pick",
                     .label = "Confirm Choice",
                     .args = {.row = ec::row_shape{.component_id = "list:rows"}},
                     .role = options.role,
                     .covers = options.covers})},
                .denials =
                    options.denial.empty()
                        ? std::vector<ec::denial>{}
                        : std::vector<ec::denial>{{.id = "CONFIRM", .reason = options.denial}}};
        }});
    const auto input = client::input_context_scope(context, "TEST_PICK");
    auto state = map_only();
    state.actions.push_back({.id = "CONFIRM", .name = "Confirm Choice", .keys = {"ENTER"}});
    return top_of(state);
}
} // namespace

TEST_CASE(
    "a select affordance only moves the cursor and the list's activate runs the row",
    "[engine_client_component]") {
    const auto screen = picking_screen({.role = "select", .activate = "t:pick"});
    const auto& rows = body<ec::list_body>(find(screen, "list:rows"));
    CHECK(affordance(screen, "t:pick").role == "select");
    REQUIRE_FALSE(rows.activate.empty());
    CHECK(rows.activate != "t:pick");
    const auto& activate = affordance(screen, rows.activate);
    CHECK(activate.role.empty());
    CHECK(activate.label == "Confirm Choice");
    CHECK(activate.args.row->component_id == "list:rows");
    const auto selected = ec::invoked(invoke_of("t:pick", "r:2"));
    REQUIRE(selected);
    CHECK(selected->affordance_id == "t:pick");
    CHECK_FALSE(selected->activates);
    const auto activated = ec::invoked(invoke_of(rows.activate, "r:2"));
    REQUIRE(activated);
    CHECK(activated->affordance_id == "t:pick");
    CHECK(activated->activates);
    CHECK(activated->args.row == "r:2");
}

TEST_CASE(
    "an activate that is no select affordance stays as the screen made it",
    "[engine_client_component]") {
    const auto screen = picking_screen({.activate = "t:pick"});
    CHECK(body<ec::list_body>(find(screen, "list:rows")).activate == "t:pick");
    const auto invoke = ec::invoked(invoke_of("t:pick", "r:1"));
    REQUIRE(invoke);
    CHECK(invoke->affordance_id == "t:pick");
    CHECK_FALSE(invoke->activates);
}

TEST_CASE(
    "a registered action an affordance of the screen covers is not offered beside it",
    "[engine_client_component]") {
    const auto own = picking_screen({.activate = "t:pick", .covers = "CONFIRM"});
    CHECK(std::ranges::find(own.affordances, "CONFIRM", &ec::affordance::id)
          == own.affordances.end());
    // Two actions with the same label are two actions: only what an affordance declares it covers
    // is dropped.
    const auto alike = picking_screen({.activate = "t:pick"});
    CHECK(affordance(alike, "t:pick").label == affordance(alike, "CONFIRM").label);
    CHECK(affordance(alike, "CONFIRM").role == "confirm");
    // A denial keeps the registered action, which is where it is shown.
    const auto denied = picking_screen(
        {.activate = "t:pick", .covers = "CONFIRM", .denial = "No."});
    CHECK_FALSE(affordance(denied, "CONFIRM").enabled);
    const auto none = picking_screen({});
    CHECK(affordance(none, "CONFIRM").role == "confirm");
    // The generic list has its own activate and keeps the registered action beside it.
    auto state = boundary(menu());
    state.actions.push_back({.id = "CONFIRM", .name = "Confirm Choice", .keys = {"ENTER"}});
    const auto generic = top_of(state);
    CHECK(affordance(generic, "CONFIRM").role == "confirm");
    CHECK_FALSE(body<ec::list_body>(find(generic, "list:choices")).activate.empty());
}

namespace {
auto shortcut_of(const ec::screen_view& view, const std::string& id) -> const ec::shortcut& {
    const auto found = std::ranges::find(view.shortcuts, id, &ec::shortcut::affordance_id);
    INFO(id);
    REQUIRE(found != view.shortcuts.end());
    return *found;
}
auto with_map_actions() -> ec::boundary_state {
    auto state = map_only();
    state.actions = {
        {.id = "inventory", .name = "Open Inventory", .keys = {"i"}},
        {.id = "craft", .name = "Craft Items", .keys = {"&"}},
        {.id = "map", .name = "map", .keys = {"m"}},
    };
    return state;
}
} // namespace

TEST_CASE(
    "the map publishes the data-defined shortcuts the way its input context names them",
    "[engine_client_component][shortcuts]") {
    const auto map = ec::interaction_stack(with_map_actions()).front();
    REQUIRE_FALSE(map.shortcuts.empty());
    const auto& inventory = shortcut_of(map, "inventory");
    CHECK(inventory.label == "Open Inventory");
    CHECK(inventory.icon == "inventory");
    CHECK_FALSE(inventory.glyph.empty());
    CHECK(std::ranges::contains(inventory.keys, "i"));
    CHECK(inventory.enabled);
    // Order is the data's `order`, not the id.
    const auto ids =
        map.shortcuts | std::views::transform(&ec::shortcut::affordance_id)
        | std::ranges::to<std::vector>();
    CHECK(ids.front() == "inventory");
    // An action the context does not offer now is listed but cannot run; one native does not name
    // is not listed.
    CHECK_FALSE(shortcut_of(map, "player_data").enabled);
    CHECK(std::ranges::none_of(map.shortcuts, [](const auto& each) {
        return each.label == each.affordance_id;
    }));
}

TEST_CASE(
    "a screen over the map leaves the shortcut bar in place but not runnable",
    "[engine_client_component][shortcuts]") {
    auto state = boundary(menu());
    state.actions = with_map_actions().actions;
    const auto stack = ec::interaction_stack(state);
    REQUIRE(stack.size() == 2);
    CHECK(shortcut_of(stack.front(), "inventory").label == "Open Inventory");
    CHECK(std::ranges::none_of(stack.front().shortcuts, &ec::shortcut::enabled));
    CHECK(stack.back().shortcuts.empty());
}

TEST_CASE(
    "a mod adds a shortcut with a new id and replaces one by reusing its id",
    "[engine_client_component][shortcuts]") {
    const auto original = client_shortcuts::all();
    const auto before = ec::interaction_stack(with_map_actions()).front().shortcuts.size();
    const auto restore = on_out_of_scope([&] {
        client_shortcuts::reset();
        std::ranges::for_each(original, [](const auto& each) { client_shortcuts::add(each); });
    });
    client_shortcuts::add({.id = "mod_craft", .action = "craft", .icon = "mod", .order = 1});
    client_shortcuts::add({.id = "inventory", .action = "inventory", .icon = "bag", .order = 5});
    const auto map = ec::interaction_stack(with_map_actions()).front();
    CHECK(map.shortcuts.size() == before + 1);
    CHECK(map.shortcuts.front().affordance_id == "craft");
    CHECK(map.shortcuts.front().icon == "mod");
    CHECK(shortcut_of(map, "inventory").icon == "bag");
}

TEST_CASE(
    "the shortcut bar is on the wire as the schema has it",
    "[engine_client_component][shortcuts]") {
    const auto json = wire(with_map_actions())["stack"].front();
    const auto& bar = json["shortcuts"];
    REQUIRE(bar.is_array());
    const auto inventory = std::ranges::find_if(bar, [](const nlohmann::json& each) {
        return each["affordance_id"] == "inventory";
    });
    REQUIRE(inventory != bar.end());
    CHECK((*inventory)["label"] == "Open Inventory");
    CHECK((*inventory)["enabled"] == true);
    CHECK((*inventory)["icon"] == "inventory");
    CHECK_FALSE(wire(boundary(menu()))["stack"].back().contains("shortcuts"));
}

TEST_CASE(
    "cursor, paging and tab actions of a screen only navigate, and the keybindings menu is its help",
    "[engine_client_component][roles]") {
    auto state = boundary(menu());
    state.actions = {
        {.id = "UP", .name = "Pan up", .keys = {"k"}},
        {.id = "PAGE_DOWN", .name = "Fast scroll down", .keys = {"NPAGE"}},
        {.id = "NEXT_TAB", .name = "Go to next tab", .keys = {"TAB"}},
        {.id = "HELP_KEYBINDINGS", .name = "Display keybindings menu", .keys = {"?"}},
        {.id = "FILTER", .name = "Filter", .keys = {"f"}},
    };
    const auto view = top_of(state);
    CHECK(affordance(view, "UP").role == "navigate");
    CHECK(affordance(view, "PAGE_DOWN").role == "navigate");
    CHECK(affordance(view, "NEXT_TAB").role == "navigate");
    CHECK(affordance(view, "HELP_KEYBINDINGS").role == "help");
    CHECK(affordance(view, "FILTER").role.empty());
    // On the map the same key is a step.
    auto map = map_only();
    map.actions = state.actions;
    CHECK(affordance(top_of(map), "UP").role.empty());
}
