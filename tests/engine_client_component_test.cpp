#include "catch/catch.hpp"
#include "client_input.h"
#include "engine_client_component.h"
#include "input.h"

#include <algorithm>
#include <fstream>
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
    return {
        .id = "boundary:test",
        .ready = ready(),
        .actions = {{.id = "UP", .name = "Move up", .keys = {"k"}}},
        .interaction = std::move(native),
        .frame = {.dim = "", .x = 100, .y = 200}};
}
auto view_of(const ec::boundary_state& state) -> ec::interaction_view {
    auto view = ec::interaction_components(state);
    REQUIRE(view);
    return *view;
}
auto find(const ec::interaction_view& view, const std::string& id) -> const ec::component& {
    const auto* const found = ec::find_component(view.root, id);
    INFO(id);
    REQUIRE(found != nullptr);
    return *found;
}
template <typename Body> auto body(const ec::component& value) -> const Body& {
    REQUIRE(std::holds_alternative<Body>(value.body));
    return std::get<Body>(value.body);
}
auto affordance(const ec::interaction_view& view, const std::string& id) -> const ec::affordance& {
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
/// The value of the 0.2 example `name` in `docs/schema/engine-client/0.2.examples/<area>.json`.
auto example(const std::string& area, const std::string& name) -> nlohmann::json {
    // Relative to the repository root, where cata_test runs.
    const auto path = "docs/schema/engine-client/0.2.examples/" + area + ".json";
    auto stream = std::ifstream{path};
    INFO("run cata_test from the repository root to read " << path);
    REQUIRE(stream.good());
    const auto examples = nlohmann::json::parse(stream);
    for (const auto& entry : examples["valid"]) {
        if (entry["name"] == name) { return entry["value"]; }
    }
    FAIL("no example " << name);
    return {};
}
auto main_menu() -> client::interaction_snapshot {
    return {
        .context = "MAIN_MENU",
        .kind = client::interaction_kind::choices,
        .title = "Cataclysm: Bright Nights",
        .banner = {"<color_white>BN</color>", "0.9"},
        .allow_cancel = true,
        .panes = {{.id = "new_game", .label = "New Game", .role = "focused"},
                  {.id = "load", .label = "Load", .role = "category"}},
        .choices =
            {{.id = "tab:new_game", .label = "New Game", .selected = true},
             {.id = "tab:load", .label = "Load"},
             {.id = "new_game:custom", .label = "Custom Character", .pane_id = "new_game"},
             {.id = "new_game:tutorial", .label = "Tutorial", .pane_id = "new_game"}},
        .choice_total = 4,
        .focus_choice_id = "tab:new_game",
    };
}
} // namespace

TEST_CASE("choices map to tabs over lists grouped by pane", "[engine_client_component]") {
    const auto view = view_of(boundary(main_menu()));
    CHECK(view.context == "MAIN_MENU");
    CHECK(ec::component_kind(view.root) == "split");
    CHECK(body<ec::text_body>(find(view, "banner")).role == "banner");
    const auto& tabs = body<ec::tabs_body>(find(view, "tabs"));
    REQUIRE(tabs.tabs.size() == 2);
    CHECK(tabs.tabs[0].id == "tab:new_game");
    CHECK(tabs.tabs[0].selected);
    CHECK(tabs.select == "bn:choose:tabs");
    // The shown tab's pane holds its rows; the hidden tab's empty pane is left to its tab.
    CHECK(ec::component_kind(find(view, "tabs").children.at(0)) == "panel");
    CHECK(ec::find_component(view.root, "panel:load") == nullptr);
    const auto& list = body<ec::list_body>(find(view, "list:new_game"));
    CHECK(row_ids_of(find(view, "list:new_game"))
          == std::vector<std::string>{"new_game:custom", "new_game:tutorial"});
    CHECK(list.selection == "single");
    CHECK(list.activate == "bn:choose:list:new_game");
    CHECK(view.focus_component == "tabs");
    CHECK(view.focus_row == "tab:new_game");
    CHECK(affordance(view, "bn:cancel").role == "cancel");
    CHECK(affordance(view, "UP").keys == std::vector<std::string>{"k"});
}

TEST_CASE("subtab rows nest under the tab rows", "[engine_client_component]") {
    auto native = client::interaction_snapshot{
        .context = "CRAFTING",
        .kind = client::interaction_kind::choices,
        .choices =
            {{.id = "tab:CC_FOOD", .label = "FOOD", .selected = true},
             {.id = "subtab:CSC_ALL", .label = "ALL", .pane_id = "CC_FOOD", .selected = true},
             {.id = "recipe:1", .label = "sandwich", .pane_id = "CC_FOOD"}},
        .choice_total = 3,
    };
    const auto view = view_of(boundary(std::move(native)));
    CHECK(view.root.id == "tabs");
    CHECK(view.root.children.at(0).id == "subtabs");
    CHECK(view.root.children.at(0).children.at(0).id == "list:CC_FOOD");
}

TEST_CASE("a paged list counts the rows it does not hold", "[engine_client_component]") {
    auto native = client::interaction_snapshot{
        .kind = client::interaction_kind::choices,
        .choices = {{.id = "a", .label = "A"}, {.id = "b", .label = "B"}},
        .choice_total = 250,
        .groups = {{"", {.total = 250, .offset = 0}}},
    };
    const auto view = view_of(boundary(std::move(native)));
    CHECK(body<ec::list_body>(view.root).total == 250);
    CHECK_FALSE(ec::row_ids(view.root));
}

TEST_CASE("each pane's list pages within its own rows", "[engine_client_component]") {
    // The page holds the end of the left pane's rows and none of the right pane's.
    const auto view = view_of(boundary({
        .kind = client::interaction_kind::inventory,
        .panes = {{.id = "left", .label = "Source"}, {.id = "right", .label = "Destination"}},
        .choices = {{.id = "item:119", .label = "rock", .pane_id = "left"}},
        .choice_offset = 119,
        .choice_total = 150,
        .groups = {{"left", {.total = 120, .offset = 119}}, {"right", {.total = 30, .offset = 0}}},
    }));
    const auto& left = body<ec::list_body>(find(view, "list:left"));
    CHECK(left.total == 120);
    CHECK(left.offset == 119);
    REQUIRE(left.rows.size() == 1);
    const auto& right = body<ec::list_body>(find(view, "list:right"));
    CHECK(right.total == 30);
    CHECK(right.rows.empty());
    // Rows read later are still picked, and checked natively against the full list.
    CHECK(right.activate == "bn:choose:list:right");
    CHECK_FALSE(ec::row_ids(find(view, "list:right")));
    CHECK(body<ec::split_body>(find(view, "body")).relation == "peers");
}

TEST_CASE("a field maps to a form that fills and submits", "[engine_client_component]") {
    const auto view = view_of(boundary({
        .kind = client::interaction_kind::field,
        .title = "Name",
        .field = client::interaction_field{.id = "field:name", .label = "Name", .value = "Ada"},
    }));
    const auto& form = body<ec::form_body>(find(view, "field"));
    REQUIRE(form.fields.size() == 1);
    CHECK(form.fields[0].value == "Ada");
    CHECK(form.submit == "bn:submit:field");
    CHECK(affordance(view, "bn:submit:field").role == "confirm");
    CHECK(affordance(view, "bn:fill:field").args.text->field_id == "field:name");
}

TEST_CASE("editor rows map to a form beside their list", "[engine_client_component]") {
    const auto view = view_of(boundary({
        .kind = client::interaction_kind::choices,
        .choices =
            {{.id = "option:DROP_EMPTY",
              .label = "Drop empty containers",
              .pane_id = "page:general",
              .editor =
                  client::interaction_editor{
                      .type = "select",
                      .value = "no",
                      .values = {{.id = "no", .label = "No"}, {.id = "all", .label = "All"}}}}},
        .choice_total = 1,
    }));
    const auto& form = body<ec::form_body>(find(view, "editors"));
    REQUIRE(form.fields.size() == 1);
    CHECK(form.fields[0].type == "select");
    CHECK(form.fields[0].options.size() == 2);
    CHECK(row_ids_of(find(view, "list:page:general"))
          == std::vector<std::string>{"option:DROP_EMPTY"});
    const auto json = wire(boundary({
        .kind = client::interaction_kind::choices,
        .choices =
            {{.id = "x", .label = "X", .pane_id = "p", .editor = client::interaction_editor{}}},
        .choice_total = 1,
    }));
    const auto& row = json["interaction"]["root"]["children"][0]["rows"][0];
    CHECK_FALSE(row.contains("pane_id"));
    CHECK_FALSE(row.contains("editor"));
}

TEST_CASE("inventory panes map to panels with the area picker", "[engine_client_component]") {
    const auto view = view_of(boundary({
        .context = "ADVANCED_INVENTORY",
        .kind = client::interaction_kind::inventory,
        .message = "Moved.",
        .panes =
            {{.id = "left",
              .label = "Source",
              .area_label = "North",
              .filter = "rock",
              .totals =
                  client::interaction_totals{
                      .weight = "1.0",
                      .weight_max = "45.0",
                      .weight_unit = "kg",
                      .over_volume = true},
              .blocked_areas = {"ITEMS_CE"}},
             {.id = "right", .label = "Destination", .area_label = "Container"}},
        .areas = {{.action_id = "ITEMS_N", .label = "North", .x = 11},
                  {.action_id = "ITEMS_CE", .label = "Below", .x = 11, .y = 1},
                  {.action_id = "ITEMS_CONTAINER", .label = "Container", .available = false}},
        .choices = {{.id = "item:1", .label = "rock", .pane_id = "left", .available_count = 3},
                    {.id = "item:2", .label = "stick", .pane_id = "right"}},
        .allow_set_count = true,
        .choice_total = 2,
        .focus_choice_id = "item:1",
        .focus_pane_id = "left",
    }));
    const auto& left = body<ec::panel_body>(find(view, "panel:left"));
    CHECK(left.subtitle == "North");
    CHECK(left.filter == "rock");
    REQUIRE(left.stats.size() == 2);
    CHECK(left.stats[0].value == "1.0");
    CHECK(left.stats[0].max == "45.0");
    CHECK(left.stats[0].unit == "kg");
    CHECK(left.stats[0].tone.empty());
    CHECK(left.stats[1].tone == "bad");
    CHECK(body<ec::split_body>(find(view, "body")).active == "panel:left");
    CHECK(body<ec::list_body>(find(view, "list:left")).selection == "count");
    CHECK(affordance(view, "bn:set_count:list:left").args.count->min == 0);
    // The picker is a list of the area actions: blocked for the pane, or with nothing there,
    // closed.
    const auto& areas = body<ec::list_body>(find(view, "areas"));
    CHECK(row_ids_of(find(view, "areas"))
          == std::vector<std::string>{"ITEMS_N", "ITEMS_CE", "ITEMS_CONTAINER"});
    CHECK(areas.rows[0].choice.enabled);
    CHECK_FALSE(areas.rows[1].choice.enabled);
    CHECK_FALSE(areas.rows[2].choice.enabled);
    CHECK(areas.activate == "bn:choose:areas");
    CHECK(view.focus_component == "list:left");
    CHECK(body<ec::text_body>(find(view, "message")).markup == "Moved.");
}

TEST_CASE("an area row runs its registered action", "[engine_client_component]") {
    const auto state = boundary({
        .context = "ADVANCED_INVENTORY",
        .kind = client::interaction_kind::inventory,
        .panes = {{.id = "left", .label = "Source", .blocked_areas = {"ITEMS_CE"}}},
        .areas = {{.action_id = "ITEMS_N", .label = "North"},
                  {.action_id = "ITEMS_CE", .label = "Below"}},
        .focus_pane_id = "left",
    });
    const auto chosen =
        ec::resolve_invoke(state, {.affordance_id = "bn:choose:areas", .args = {.row = "ITEMS_N"}});
    REQUIRE(chosen);
    CHECK(std::get<ec::registered_action>(*chosen).id == "ITEMS_N");
    const auto closed = ec::
        resolve_invoke(state, {.affordance_id = "bn:choose:areas", .args = {.row = "ITEMS_CE"}});
    REQUIRE_FALSE(closed);
    CHECK(closed.error() == ec::error::validation_failed);
    const auto unknown = ec::
        resolve_invoke(state, {.affordance_id = "bn:choose:areas", .args = {.row = "ITEMS_NW"}});
    REQUIRE_FALSE(unknown);
    CHECK(unknown.error() == ec::error::invalid_arguments);
}

TEST_CASE("a counted row bounds its count on the wire", "[engine_client_component]") {
    const auto json = wire(boundary({
        .choices = {{.id = "item:1", .label = "rock", .available_count = 3}},
        .allow_set_count = true,
        .choice_total = 1,
    }));
    const auto& row = json["interaction"]["root"]["rows"][0];
    CHECK(row["max"] == 3);
    CHECK_FALSE(row.contains("available_count"));
}

TEST_CASE("a pane's description and list are a detail split", "[engine_client_component]") {
    const auto view = view_of(boundary({
        .panes = {{.id = "spell", .label = "Spells", .area_description = "Fireball: burns."}},
        .choices = {{.id = "spell:1", .label = "Fireball", .pane_id = "spell"}},
        .choice_total = 1,
    }));
    const auto& split = body<ec::split_body>(find(view, "split:spell"));
    CHECK(split.relation == "detail");
    CHECK(
        wire(boundary({
            .panes = {{.id = "spell", .label = "Spells", .area_description = "Fireball: burns."}},
            .choices = {{.id = "spell:1", .label = "Fireball", .pane_id = "spell"}},
            .choice_total = 1,
        }))["interaction"]["root"]["child"]["relation"]
        == "detail");
}

TEST_CASE("a screen publishes its own view and handlers", "[engine_client_component]") {
    const auto context = input_context("TEST_PROVIDER");
    auto runs = 0;
    const auto provider = [&](const ec::boundary_state& /*state*/) {
        return ec::interaction_view{
            .context = "TEST_PROVIDER",
            .root = {.id = "own", .body = ec::text_body{.markup = "drawn by the screen"}},
            .affordances = {
                {.id = "own:go",
                 .label = "Go",
                 .args = {.text = ec::text_shape{}},
                 .handler = [&](const ec::invoke_args& args)
                     -> std::expected<ec::command_operation, ec::error> {
                     ++runs;
                     return ec::registered_action{.id = *args.text};
                 }}}};
    };
    const auto state = boundary(main_menu());
    {
        const auto scope = ec::view_scope(context, provider);
        // Published only while its context waits for input.
        CHECK(view_of(state).root.id != "own");
        const auto input = client::input_context_scope(context, "TEST_PROVIDER");
        const auto view = view_of(state);
        CHECK(view.root.id == "own");
        CHECK(view.affordances.at(0).id == "own:go");
        CHECK(view.affordances.back().id == "UP");
        CHECK(wire(state)["interaction"]["root"]["markup"] == "drawn by the screen");
        const auto ran =
            ec::resolve_invoke(state, {.affordance_id = "own:go", .args = {.text = "UP"}});
        REQUIRE(ran);
        CHECK(std::get<ec::registered_action>(*ran).id == "UP");
        CHECK(runs == 1);
        const auto missing = ec::resolve_invoke(state, {.affordance_id = "own:go"});
        REQUIRE_FALSE(missing);
        CHECK(missing.error() == ec::error::invalid_arguments);
        CHECK(runs == 1);
        CHECK_FALSE(ec::resolve_invoke(state, {.affordance_id = "bn:cancel"}));
    }
    const auto input = client::input_context_scope(context, "TEST_PROVIDER");
    CHECK(view_of(state).root.id != "own");
}

TEST_CASE("a target maps to a world map with target affordances", "[engine_client_component]") {
    const auto view = view_of(boundary({
        .context = "TARGET",
        .kind = client::interaction_kind::target,
        .allow_cancel = true,
        .target =
            client::interaction_target{
                .cursor = {.x = 1, .y = 2},
                .range = 10,
                .candidates = {{.id = "target:1", .position = {.x = 3, .y = 4}, .creature = true}}},
    }));
    const auto& map = body<ec::map_body>(find(view, "map"));
    CHECK(map.mode == "world");
    REQUIRE(map.target);
    CHECK(affordance(view, "bn:set_target").args.pos->unit == "square");
    CHECK(affordance(view, "bn:target_candidate").args.row->component_id == "map");
    CHECK(row_ids_of(find(view, "map")) == std::vector<std::string>{"target:1"});
    const auto json = wire(boundary({
        .kind = client::interaction_kind::target,
        .target = client::interaction_target{.cursor = {.x = 1, .y = 2}},
    }));
    CHECK(json["interaction"]["root"]["target"]["current"]["x"] == 101);
}

TEST_CASE("the overmap maps to an overmap map", "[engine_client_component]") {
    auto state = boundary({
        .context = "OVERMAP",
        .overmap =
            client::interaction_overmap{
                .cols = 1, .rows = 1, .glyphs = {"."}, .foreground = {7}, .background = {0}},
    });
    state.frame = {.omt = true};
    const auto view = view_of(state);
    CHECK(body<ec::map_body>(view.root).mode == "overmap");
    CHECK(wire(state)["interaction"]["root"]["overmap"]["cells"][0][0][0] == ".");
}

TEST_CASE("a vehicle maps to a grid, a part list and text", "[engine_client_component]") {
    const auto view = view_of(boundary({
        .context = "VEH_INTERACT",
        .vehicle =
            client::interaction_vehicle{
                .name = "Bicycle",
                .owner = "Ada",
                .verb = "Install",
                .blocked = true,
                .cells = {{.dx = 0, .dy = -1, .glyph = "H"}},
                .parts =
                    {{.name = "frame", .open = "[", .close = "]", .health = 62, .broken = true}},
                .stats = {"Mass: 16 kg", ""},
                .detail =
                    client::
                        interaction_vehicle_detail{
                            .name = "board",
                            .groups = {{.kind = "skill", .heading = "Skills:", .empty_text = "NONE"},
                                       {.kind = "tool", .heading = "Tools:"},
                                       {.kind = "extra", .heading = "Extra:"}},
                            .requirements = {{.kind = "tool", .text = "hammer", .met = false}}}},
    }));
    const auto& panel = body<ec::panel_body>(find(view, "vehicle"));
    CHECK(panel.title == "Bicycle");
    CHECK(panel.subtitle == "Install");
    const auto& grid = body<ec::grid_body>(find(view, "vehicle:grid"));
    CHECK(grid.rows == 2);
    REQUIRE(grid.cursor);
    CHECK(*grid.cursor == ec::grid_point{0, 1});
    REQUIRE(grid.cells.size() == 1);
    CHECK(grid.cells[0].at == ec::grid_point{0, 0});
    const auto& parts = body<ec::list_body>(find(view, "vehicle:parts"));
    REQUIRE(parts.rows.size() == 1);
    CHECK(parts.rows[0].choice.markup.empty());
    CHECK(parts.rows[0].choice.columns.at(0).key == "kind");
    CHECK(parts.rows[0].choice.columns.at(0).value == "structure");
    CHECK_FALSE(parts.rows[0].choice.enabled);
    CHECK(body<ec::text_body>(find(view, "vehicle:stats")).markup == "Mass: 16 kg\n");
    const auto& needs = body<ec::list_body>(find(view, "vehicle:requirements"));
    // The skill group prints its empty text; the empty extra group is left out.
    REQUIRE(needs.rows.size() == 4);
    CHECK(needs.rows[0].header);
    CHECK(needs.rows[1].choice.label == "NONE");
    CHECK(needs.rows[2].choice.label == "Tools:");
    CHECK_FALSE(needs.rows[3].choice.enabled);
}

TEST_CASE("a drawn window maps to a canvas", "[engine_client_component]") {
    const auto view = view_of(boundary({
        .context = "SNAKE",
        .canvas = client::interaction_canvas{.cols = 3, .rows = {{{.text = "*", .foreground = 9}}}},
        .refresh_ms = 100,
    }));
    CHECK(ec::component_kind(view.root) == "canvas");
    CHECK(view.refresh_ms == 100);
    CHECK(wire(boundary(
              {.canvas = client::interaction_canvas{.cols = 3}}))["interaction"]["root"]["cols"]
          == 3);
}

TEST_CASE("terminal and screen text map to text", "[engine_client_component]") {
    const auto terminal = view_of(boundary({
        .kind = client::interaction_kind::choices,
        .message = "ERROR",
        .choices = {{.id = "yes", .label = "Yes", .pane_id = "terminal"}},
        .choice_total = 1,
        .any_key_choice_id = "yes",
    }));
    CHECK(body<ec::text_body>(find(terminal, "message")).role == "terminal_output");
    CHECK(affordance(terminal, "bn:any_key").role == "any_key");
    const auto screen = view_of(boundary({.context = "HELP", .message = "Keys"}));
    CHECK(ec::component_kind(screen.root) == "text");
    CHECK(body<ec::text_body>(screen.root).role.empty());
}

TEST_CASE("the map boundary offers clicks, item rows and actions", "[engine_client_component]") {
    auto state = boundary({});
    state.interaction.reset();
    const auto view = view_of(state);
    CHECK(body<ec::map_body>(view.root).mode == "world");
    for (const auto* id : {"bn:travel", "bn:context", "bn:pickup", "bn:drop", "UP"}) {
        CHECK(affordance(view, id).enabled);
    }
    CHECK(affordance(view, "bn:pickup").args.row->component_id == ec::ground_rows);
    state.ready = {.phase = "starting"};
    state.actions.clear();
    CHECK_FALSE(ec::interaction_components(state));
    const auto early = ec::resolve_invoke(state, {.affordance_id = "bn:travel"});
    REQUIRE_FALSE(early);
    CHECK(early.error() == ec::error::not_ready);
    CHECK(wire(state)["interaction"].is_null());
}

TEST_CASE("the wire carries no layout hints", "[engine_client_component]") {
    const auto text = ec::serialize_interaction_boundary(boundary(main_menu()));
    for (const auto* removed :
         {"\"direction\"", "\"placement\"", "\"show\"", "\"modal\"", "\"style\"", "\"compat\"",
          "\"choices\"", "\"choice_total\"", "\"actions\"", "\"allow_cancel\""}) {
        INFO(removed);
        CHECK(text.find(removed) == std::string::npos);
    }
    const auto json = nlohmann::json::parse(text);
    CHECK(json["interaction"]["root"]["kind"] == "split");
    CHECK(json["interaction"]["focus"]["component_id"] == "tabs");
}

TEST_CASE("every 0.1 operation is an invoke", "[engine_client_component]") {
    const auto semantic = [](const auto& result) -> client::interaction_command {
        REQUIRE(result);
        REQUIRE(std::holds_alternative<ec::semantic_operation>(*result));
        return std::get<ec::semantic_operation>(*result).command;
    };
    const auto menu = boundary(main_menu());
    SECTION("choose") {
        const auto& command = semantic(ec::resolve_invoke(
            menu,
            {.affordance_id = "bn:choose:list:new_game", .args = {.row = "new_game:tutorial"}}));
        CHECK(command.operation == client::interaction_operation::choose);
        CHECK(command.target_id == "new_game:tutorial");
        CHECK(
            semantic(ec::resolve_invoke(
                         menu, {.affordance_id = "bn:choose:tabs", .args = {.row = "tab:load"}}))
                .target_id
            == "tab:load");
    }
    SECTION("cancel") {
        CHECK(semantic(ec::resolve_invoke(menu, {.affordance_id = "bn:cancel"})).operation
              == client::interaction_operation::cancel);
    }
    SECTION("registered action") {
        const auto result = ec::resolve_invoke(menu, {.affordance_id = "UP"});
        REQUIRE(result);
        CHECK(std::get<ec::registered_action>(*result).id == "UP");
    }
    SECTION("fill and submit") {
        const auto field = boundary({.field = client::interaction_field{.id = "field:name"}});
        const auto& typed = semantic(
            ec::resolve_invoke(field, {.affordance_id = "bn:fill:field", .args = {.text = "Ada"}}));
        CHECK(typed.operation == client::interaction_operation::fill);
        CHECK(typed.target_id == "field:name");
        CHECK(typed.value == "Ada");
        CHECK(typed.submit == false);
        CHECK(
            semantic(ec::resolve_invoke(
                         field, {.affordance_id = "bn:submit:field", .args = {.text = "Ada"}}))
                .submit
            == true);
        const auto editors = boundary(
            {.choices = {{.id = "option:X", .label = "X", .editor = client::interaction_editor{}}},
             .choice_total = 1});
        const auto& set = semantic(ec::resolve_invoke(
            editors,
            {.affordance_id = "bn:submit:editors", .args = {.row = "option:X", .text = "8"}}));
        CHECK(set.target_id == "option:X");
        CHECK(set.submit == true);
    }
    SECTION("set_count") {
        const auto counted = boundary(
            {.choices = {{.id = "item:1", .label = "rock", .available_count = 3}},
             .allow_set_count = true,
             .choice_total = 1});
        const auto& command = semantic(ec::resolve_invoke(
            counted,
            {.affordance_id = "bn:set_count:list", .args = {.row = "item:1", .count = 2}}));
        CHECK(command.operation == client::interaction_operation::set_count);
        CHECK(command.count == 2U);
    }
    SECTION("set_target and a candidate") {
        const auto target = boundary(
            {.target = client::interaction_target{
                 .candidates = {{.id = "target:1", .position = {.x = 3, .y = 4}}}}});
        const auto at = ec::position{.dim = "", .x = 5, .y = 6};
        const auto moved =
            ec::resolve_invoke(target, {.affordance_id = "bn:set_target", .args = {.pos = at}});
        REQUIRE(moved);
        CHECK(std::get<ec::semantic_operation>(*moved).target == at);
        const auto aimed = ec::resolve_invoke(
            target, {.affordance_id = "bn:target_candidate", .args = {.row = "target:1"}});
        REQUIRE(aimed);
        const auto& operation = std::get<ec::semantic_operation>(*aimed);
        CHECK(operation.command.target_id == "target:1");
        CHECK(operation.target == ec::position{.dim = "", .x = 103, .y = 204});
    }
    SECTION("travel, context, pickup and drop at the map") {
        auto map = boundary({});
        map.interaction.reset();
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

TEST_CASE("invoke rejects what the affordance does not take", "[engine_client_component]") {
    const auto menu = boundary(main_menu());
    const auto fails = [&](const ec::invoke_operation& invoke) {
        const auto result = ec::resolve_invoke(menu, invoke);
        REQUIRE_FALSE(result);
        return result.error();
    };
    CHECK(fails({.affordance_id = "bn:nothing"}) == ec::error::unknown_affordance);
    CHECK(fails({.affordance_id = "bn:travel", .args = {.pos = ec::position{}}})
          == ec::error::unknown_affordance);
    // A missing, an extra or a foreign argument.
    CHECK(fails({.affordance_id = "bn:choose:list:new_game"}) == ec::error::invalid_arguments);
    CHECK(fails({.affordance_id = "bn:cancel", .args = {.row = "new_game:custom"}})
          == ec::error::invalid_arguments);
    CHECK(fails({.affordance_id = "bn:choose:list:new_game", .args = {.row = "tab:load"}})
          == ec::error::invalid_arguments);
    CHECK(fails({.affordance_id = "bn:choose:list:new_game", .args = {.row = ""}})
          == ec::error::invalid_arguments);
    auto map = boundary({});
    map.interaction.reset();
    const auto pickup = ec::
        resolve_invoke(map, {.affordance_id = "bn:pickup", .args = {.row = "item:1", .count = 0}});
    REQUIRE_FALSE(pickup);
    CHECK(pickup.error() == ec::error::invalid_arguments);
    map.ready.accepts_registered_actions = false;
    const auto closed =
        ec::resolve_invoke(map, {.affordance_id = "bn:travel", .args = {.pos = ec::position{}}});
    REQUIRE_FALSE(closed);
    CHECK(closed.error() == ec::error::not_ready);
    auto menu_closed = boundary(main_menu());
    menu_closed.ready.accepts_interaction_commands = false;
    const auto refused = ec::resolve_invoke(menu_closed, {.affordance_id = "bn:cancel"});
    REQUIRE_FALSE(refused);
    CHECK(refused.error() == ec::error::not_ready);
    const auto field = boundary({.field = client::interaction_field{.id = "field:name"}});
    const auto long_text = std::string(ec::maximum_inline_bytes + 1, 'a');
    const auto too_long =
        ec::resolve_invoke(field, {.affordance_id = "bn:fill:field", .args = {.text = long_text}});
    REQUIRE_FALSE(too_long);
    CHECK(too_long.error() == ec::error::invalid_arguments);
    CHECK(ec::error_name(ec::error::invalid_arguments) == "invalid_arguments");
    CHECK(ec::error_name(ec::error::unknown_affordance) == "unknown_affordance");
}

TEST_CASE("the wire matches the 0.2 schema examples", "[engine_client_component]") {
    SECTION("main menu") {
        auto native = main_menu();
        native.schema_id = "schema:ab";
        native.banner.clear();
        native.panes.push_back({.id = "quit", .label = "Quit", .role = "category"});
        native.choices.insert(native.choices.begin() + 2, {.id = "tab:quit", .label = "Quit"});
        native.choices[3].description = "Allows you to fully customize the character.";
        native.choice_total = 5;
        const auto state = ec::boundary_state{
            .id = "boundary:12",
            .ready = {.phase = "menu",
                      .accepts_interaction_commands = true,
                      .accepts_registered_actions = true},
            .actions = {{.id = "UP", .name = "Move up", .keys = {"UP", "k"}}},
            .interaction = std::move(native)};
        CHECK(wire(state) == example("menu", "subscribe result at the main menu")["interaction"]);
    }
    SECTION("map") {
        const auto state = ec::boundary_state{
            .id = "boundary:90",
            .ready = ready(),
            .actions = {{.id = "move_e", .name = "Move East", .keys = {"l", "RIGHT"}}}};
        CHECK(wire(state)
              == example("interaction", "map boundary with its clicks, item rows and actions"));
    }
}

TEST_CASE("the overmap target takes overmap squares", "[engine_client_component]") {
    auto state = boundary({.target = client::interaction_target{}});
    state.frame = {.omt = true};
    const auto view = view_of(state);
    CHECK(body<ec::map_body>(view.root).mode == "overmap");
    CHECK(affordance(view, "bn:set_target").args.pos->unit == "overmap");
}
