#if defined(CATA_MCP)

#    include "action.h"
#    include "avatar.h"
#    include "cached_options.h"
#    include "calendar.h"
#    include "cata_utility.h"
#    include "catch/catch.hpp"
#    include "character_functions.h"
#    include "character_martial_arts.h"
#    include "client_command.h"
#    include "client_input.h"
#    include "client_interaction.h"
#    include "client_memory.h"
#    include "client_memory_scope.h"
#    include "client_presentation.h"
#    include "client_presentation_scope.h"
#    include "construction.h"
#    include "crafting.h"
#    include "cursesdef.h"
#    include "cursesport.h"
#    include "debug_menu.h"
#    include "debug_test_support.h"
#    include "drop_token.h"
#    include "editmap.h"
#    include "engine_client_component.h"
#    include "engine_client_contract.h"
#    include "engine_client_event.h"
#    include "engine_client_session.h"
#    include "engine_client_world.h"
#    include "faction.h"
#    include "game.h"
#    include "game_session.h"
#    include "gamemode.h"
#    include "input.h"
#    include "inventory_ui.h"
#    include "item.h"
#    include "json.h"
#    include "magic/magic.h"
#    include "magic/magic_teleporter_list.h"
#    include "map/map.h"
#    include "map_helpers.h"
#    include "message_feed.h"
#    include "messages.h"
#    include "monster.h"
#    include "npc.h"
#    include "options.h"
#    include "options_helpers.h"
#    include "output.h"
#    include "overmap/overmap_ui.h"
#    include "overmap/overmapbuffer.h"
#    include "path_info.h"
#    include "pickup.h"
#    include "player_activity.h"
#    include "player_helpers.h"
#    include "popup.h"
#    include "ranged.h"
#    include "recipe.h"
#    include "replay/replay.h"
#    include "rng.h"
#    include "state_helpers.h"
#    include "string_input_popup.h"
#    include "string_utils.h"
#    include "tile_parts.h"
#    include "ui.h"
#    include "uistate.h"
#    include "units_utility.h"
#    include "vehicle/vehicle.h"

#    include <atomic>
#    include <filesystem>
#    include <functional>
#    include <nlohmann/json.hpp>
#    include <ranges>
#    include <set>
#    include <sstream>
#    include <stdexcept>
#    include <string>
#    include <vector>

namespace {

struct interaction_test_guard {
    const game_client::memory::scoped_state memory;
    bool old_test_mode = test_mode;
    int old_termx = TERMX;
    int old_termy = TERMY;
    int old_full_screen_width = FULL_SCREEN_WIDTH;
    int old_full_screen_height = FULL_SCREEN_HEIGHT;

    interaction_test_guard() {
        test_mode = false;
        TERMX = 100;
        TERMY = 40;
        FULL_SCREEN_WIDTH = 80;
        FULL_SCREEN_HEIGHT = 24;
        game_client::memory::resize(TERMX, TERMY);
        catacurses::stdscr = catacurses::newwin(TERMY, TERMX, point_zero);
        catacurses::newscr = catacurses::newwin(TERMY, TERMX, point_zero);
    }

    ~interaction_test_guard() {
        TERMX = old_termx;
        TERMY = old_termy;
        FULL_SCREEN_WIDTH = old_full_screen_width;
        FULL_SCREEN_HEIGHT = old_full_screen_height;
        test_mode = old_test_mode;
    }
};

auto resolve(game_client::interaction_command command) -> input_event {
    auto input = game_client::input_command{};
    input.interaction = std::move(command);
    const auto result =
        game_client::resolve_input_command(input, game_client::memory::screen_size());
    REQUIRE(result.has_value());
    return *result;
}

/// An invoke of a screen's affordance, as a client sends it.
auto invoke_event(const std::string& affordance, game_client::interaction_arguments args = {})
    -> input_event {
    auto event = input_event{};
    event.type = input_event_t::interaction;
    event.interaction = game_client::interaction_event{
        .operation = game_client::interaction_operation::invoke,
        .target_id = affordance,
        .arguments = std::move(args),
    };
    return event;
}

auto current_boundary() -> engine_client::boundary_state {
    const auto state = engine_client::capture_boundary(
        {.epoch = "screen-test-epoch",
         .ready = {.phase = "waiting_for_input",
                   .game_ready = false,
                   .accepts_interaction_commands = true}});
    REQUIRE(state.has_value());
    return *state;
}

/// The screen input goes to, as a client reads it at this boundary.
auto current_screen() -> engine_client::screen_view {
    return engine_client::top_screen(current_boundary());
}

/// The same screen as the wire carries it.
auto screen_wire() -> nlohmann::json {
    return nlohmann::json::parse(engine_client::serialize_interaction_boundary(current_boundary()))
        .at("stack")
        .back();
}

auto component_of(const engine_client::screen_view& screen, const std::string& id)
    -> const engine_client::component& {
    REQUIRE(screen.root);
    const auto* const found = engine_client::find_component(*screen.root, id);
    INFO(id);
    REQUIRE(found != nullptr);
    return *found;
}

/// The rows of the list `id` of `screen`.
auto rows_of(const engine_client::screen_view& screen, const std::string& id)
    -> std::vector<engine_client::row> {
    const auto& list = component_of(screen, id);
    REQUIRE(std::holds_alternative<engine_client::list_body>(list.body));
    return std::get<engine_client::list_body>(list.body).rows;
}

/// The first row whose text has `text`; fails the test when no row has it.
auto row_with(const std::vector<engine_client::row>& rows, const std::string& text)
    -> engine_client::row {
    const auto found = std::ranges::find_if(rows, [&](const auto& row) {
        return row.label.find(text) != std::string::npos;
    });
    INFO(text);
    REQUIRE(found != rows.end());
    return *found;
}

auto tab_ids_of(const engine_client::screen_view& screen, const std::string& id)
    -> std::vector<std::string> {
    const auto& tabs = component_of(screen, id);
    REQUIRE(std::holds_alternative<engine_client::tabs_body>(tabs.body));
    return std::get<engine_client::tabs_body>(tabs.body).tabs
         | std::views::transform(&engine_client::tab_entry::id) | std::ranges::to<std::vector>();
}

auto resolve_action(const std::string& action) -> input_event {
    auto input = game_client::input_command{};
    input.action = action;
    const auto result =
        game_client::resolve_input_command(input, game_client::memory::screen_size());
    REQUIRE(result.has_value());
    return *result;
}

/// The invoke a 0.2 client sends: the affordance with its arguments, as the screen's loop reads it.
auto invoke(const std::string& affordance, game_client::interaction_arguments args = {})
    -> input_event {
    auto event = input_event{};
    event.type = input_event_t::interaction;
    event.interaction = game_client::interaction_event{
        .operation = game_client::interaction_operation::invoke,
        .target_id = affordance,
        .arguments = std::move(args)};
    return event;
}

/// A boundary of a game that takes input, with no 0.1 interaction beside the 0.2 screens.
auto stack_state() -> engine_client::boundary_state {
    return {
        .id = "boundary:test",
        .epoch = "epoch:test",
        .ready =
            {.phase = "waiting_for_input",
             .game_ready = true,
             .accepts_interaction_commands = true,
             .accepts_registered_actions = true},
        .interaction = std::nullopt,
        .frame = {.dim = "", .x = 100, .y = 200}};
}

/// The screen on top of the published 0.2 stack.
auto top_screen() -> nlohmann::json {
    return nlohmann::json::parse(engine_client::serialize_interaction_boundary(
        stack_state()))["stack"]
        .back();
}

/// The pickup list's rows while it is the screen on top; none while another screen is.
auto pickup_rows() -> nlohmann::json {
    const auto screen = top_screen();
    if (!screen["root"].is_object() || screen["root"]["id"] != "pickup") {
        return nlohmann::json::array();
    }
    return screen["root"]["child"]["rows"];
}

/// The weight and volume stats of the pickup list on top, by key.
auto pickup_stat(const std::string& key) -> nlohmann::json {
    for (const auto& stat : top_screen()["root"]["stats"]) {
        if (stat["key"].get<std::string>() == key) { return stat; }
    }
    return nlohmann::json::object();
}

/// The row of the item type `type`, null when the list has none.
auto row_of(const nlohmann::json& rows, const std::string& type) -> nlohmann::json {
    for (const auto& row : rows) {
        if (row["look"]["id"].get<std::string>() == type) { return row; }
    }
    return nullptr;
}

/// Marks or unmarks the row, or sets how many of it are marked.
auto pick_row(const nlohmann::json& row) -> input_event {
    return invoke("pickup:toggle", {.row = row["id"].get<std::string>()});
}
auto pick_count(const nlohmann::json& row, const std::int64_t count) -> input_event {
    return invoke("pickup:count", {.row = row["id"].get<std::string>(), .count = count});
}

/// The target the map of the screen on top shows: the aiming or the look cursor, as a client reads
/// it.
auto cursor_target() -> nlohmann::json {
    const auto screen = top_screen();
    REQUIRE(screen["root"]["id"] == "target");
    return screen["root"]["child"]["target"];
}
/// A square of the published target, in the bubble: the test boundary's frame is taken off.
auto bubble_at(const nlohmann::json& position) -> tripoint_bub_ms {
    return {position["x"].get<int>() - stack_state().frame.x,
            position["y"].get<int>() - stack_state().frame.y, position["z"].get<int>()};
}
/// Why the screen on top refuses to put its cursor on `to`; none when it takes the move.
auto refused_move(const tripoint_bub_ms& to) -> std::optional<engine_client::error> {
    const auto frame = stack_state().frame;
    const auto resolved = engine_client::resolve_invoke(
        stack_state(),
        {.affordance_id = "target:move",
         .args = {
             .pos = engine_client::position{
                 .dim = frame.dim, .x = to.x() + frame.x, .y = to.y() + frame.y, .z = to.z()}}});
    return resolved ? std::nullopt : std::optional{resolved.error()};
}
auto move_cursor(const tripoint_bub_ms& to) -> input_event {
    return invoke(
        "target:move",
        {.position = game_client::interaction_position{.x = to.x(), .y = to.y(), .z = to.z()}});
}
/// The screen on top at a terminal of 80x24 and at one of 200x60.
auto at_two_viewports() -> std::pair<std::string, std::string> {
    const auto saved_x = TERMX;
    const auto saved_y = TERMY;
    TERMX = 80;
    TERMY = 24;
    auto narrow = top_screen().dump();
    TERMX = 200;
    TERMY = 60;
    auto wide = top_screen().dump();
    TERMX = saved_x;
    TERMY = saved_y;
    return {std::move(narrow), std::move(wide)};
}

/// Every list below `node`, outermost first.
auto lists_below(const nlohmann::json& node, std::vector<nlohmann::json>& into) -> void {
    if (!node.is_object()) { return; }
    if (node["kind"] == "list") { into.push_back(node); }
    if (node.contains("child")) { lists_below(node["child"], into); }
    for (const auto& child : node.value("children", nlohmann::json::array())) {
        lists_below(child, into);
    }
}

/// The item rows of the inventory list of the selector on top, category headings left out.
auto selector_rows() -> nlohmann::json {
    auto lists = std::vector<nlohmann::json>{};
    lists_below(top_screen()["root"], lists);
    auto result = nlohmann::json::array();
    for (const auto& list : lists) {
        if (list["id"] != "list:inventory") { continue; }
        // The wire holds a prefix of a long list; the rest is paged as a client pages it.
        auto loaded = std::size_t{0};
        auto rows = list["rows"];
        while (true) {
            loaded += rows.size();
            for (const auto& row : rows) {
                if (!row.value("header", false)) { result.push_back(row); }
            }
            if (loaded >= list["total"].get<std::size_t>() || rows.empty()) { break; }
            const auto page = engine_client::read_rows(
                {.epoch = "epoch:test",
                 .boundary_id = "boundary:test",
                 .component_id = "list:inventory",
                 .offset = loaded,
                 .limit = engine_client::maximum_rows},
                stack_state());
            REQUIRE(page);
            rows = nlohmann::json::parse(engine_client::serialize_rows(*page))["rows"];
        }
    }
    return result;
}

/// The row the selector's cursor is on.
auto selector_cursor() -> std::string {
    return top_screen().value("focus", nlohmann::json::object()).value("row_id", "");
}

/// The selector's own affordances for a row of the carried items' list.
auto choose_item(const nlohmann::json& row) -> input_event {
    return invoke("list:inventory:choose", {.row = row["id"].get<std::string>()});
}
auto count_item(const nlohmann::json& row, const std::int64_t count) -> input_event {
    return invoke("list:inventory:count", {.row = row["id"].get<std::string>(), .count = count});
}

class denied_inventory_preset final: public inventory_selector_preset {
public:
    auto get_denial(const item* candidate) const -> std::string override {
        return candidate->typeId() == itype_id("bandages") ? "Denied by test preset" : "";
    }
};

auto add_crafting_fixture_item(avatar& you, const itype_id& id, const int count) -> void {
    auto first = item::spawn(id, calendar::turn);
    if (first->count_by_charges()) {
        first->charges = count;
        you.i_add(std::move(first));
        return;
    }
    you.i_add(std::move(first));
    std::ranges::for_each(std::views::iota(1, count), [&](const auto /*index*/) {
        you.i_add(item::spawn(id, calendar::turn));
    });
}

auto prepare_crafting_fixture(const bool learn_pipe = true) -> void {
    auto& you = get_avatar();
    const auto& soldering_iron = recipe_id("test_soldering_iron").obj();
    you.learn_recipe(&soldering_iron);
    you.set_skill_level(soldering_iron.skill_used, 2);
    if (learn_pipe) {
        const auto& pipe = recipe_id("test_pipe").obj();
        you.learn_recipe(&pipe);
        you.set_skill_level(pipe.skill_used, 1);
    }
    add_crafting_fixture_item(you, itype_id("test_screwdriver"), 1);
    add_crafting_fixture_item(you, itype_id("knife_steak"), 1);
    add_crafting_fixture_item(you, itype_id("e_scrap"), 4);
    add_crafting_fixture_item(you, itype_id("copper"), 2);
    add_crafting_fixture_item(you, itype_id("scrap"), 2);
    add_crafting_fixture_item(you, itype_id("duct_tape"), 20);
    add_crafting_fixture_item(you, itype_id("cable"), 10);
    you.invalidate_crafting_inventory();
}

auto finish_crafting_activity(avatar& you) -> void {
    auto turns = 0;
    while (you.activity && you.activity->id() == activity_id("ACT_CRAFT") && turns++ < 2000) {
        you.set_moves(100000);
        you.activity->do_turn(you);
    }
    REQUIRE(turns < 2000);
}

/// Reset state that the general map test helpers do not clear.
auto clear_construction_fixture(const tripoint_bub_ms& destination) -> void {
    auto& you = get_avatar();
    if (you.activity && you.activity->id() == activity_id("ACT_BUILD")) {
        you.activity->set_to_null();
    }
    get_map().partial_con_remove(destination);
    get_map().trap_set(destination, trap_id("tr_null"));
    get_map().furn_set(destination, furn_id("f_null"));
}

auto finish_construction_activity(avatar& you) -> void {
    auto turns = 0;
    while (you.activity && you.activity->id() == activity_id("ACT_BUILD") && turns++ < 2000) {
        you.set_moves(100000);
        you.activity->do_turn(you);
    }
    REQUIRE(turns < 2000);
}

constexpr auto pickup_test_pos = tripoint_bub_ms{60, 60, 0};

auto clear_pickup_fixture_state() -> void {
    const auto previous_test_mode = test_mode;
    test_mode = true;
    clear_all_state();
    clear_character(get_avatar(), false);
    test_mode = previous_test_mode;
}

struct pickup_fixture_guard {
    explicit pickup_fixture_guard(const bool with_storage = true) {
        clear_pickup_fixture_state();
        auto& you = get_avatar();
        you.setpos(map_local_to_abs(get_map(), pickup_test_pos));
        you.set_moves(1000);
        if (with_storage) { you.worn.push_back(item::spawn("duffelbag")); }
    }

    ~pickup_fixture_guard() { clear_pickup_fixture_state(); }
};

auto add_ground_item(const itype_id& id, const int charges = -1) -> item& // *NOPAD*
{
    auto added = item::spawn(id, calendar::turn, charges);
    auto& result = *added;
    get_map().add_item(pickup_test_pos, std::move(added));
    return result;
}

auto ground_item_count(const itype_id& id) -> int {
    return std::ranges::count_if(get_map().i_at(pickup_test_pos), [&](const auto* candidate) {
        return candidate->typeId() == id;
    });
}

auto ground_charges(const itype_id& id) -> int {
    auto result = 0;
    for (const auto* candidate : get_map().i_at(pickup_test_pos)) {
        if (candidate->typeId() == id) { result += candidate->charges; }
    }
    return result;
}

auto finish_pickup_activity(avatar& you) -> void {
    REQUIRE(you.activity);
    REQUIRE(you.activity->id() == activity_id("ACT_PICKUP"));
    process_activity(you);
    REQUIRE_FALSE(you.activity);
}

auto find_choice_type(
    const game_client::interaction_snapshot& snapshot, const std::string& type,
    const std::string& role) {
    const auto pane = std::ranges::find(snapshot.panes, role, &game_client::interaction_pane::role);
    REQUIRE(pane != snapshot.panes.end());
    return std::ranges::find_if(snapshot.choices, [&](const auto& choice) {
        return choice.pane_id == pane->id
            && std::ranges::any_of(choice.columns, [&](const auto& column) {
                   return column.key == "type" && column.value == type;
               });
    });
}

class counting_callback final: public uilist_callback {
public:
    auto select(uilist* /*menu*/) -> void override { ++selections; }
    int selections = 0;
};

} // namespace

TEST_CASE("interaction_test_guard leaves a borrowed compositor intact", "[client][mcp]") {
    const auto outer = game_client::memory::scoped_state{};
    game_client::memory::resize(7, 3);
    catacurses::stdscr = catacurses::newwin(3, 7, point_zero);
    catacurses::mvwprintw(catacurses::stdscr, point_zero, "x");
    catacurses::wrefresh(catacurses::stdscr);
    const auto borrowed = catacurses::stdscr;
    {
        const auto guard = interaction_test_guard{};
        CHECK(catacurses::stdscr.get<cata_cursesport::WINDOW>() != nullptr);
    }
    CHECK(game_client::memory::screen_size() == point(7, 3));
    CHECK(game_client::memory::snapshot().cells[0].text == "x");
    CHECK(catacurses::stdscr.get<cata_cursesport::WINDOW>()
          == borrowed.get<cata_cursesport::WINDOW>());
    CHECK(catacurses::stdscr.get<cata_cursesport::WINDOW>()->width == 7);
}
TEST_CASE(
    "the in-game main menu names its rows by the action they run",
    "[client][interaction][mcp][main_menu]") {
    const auto guard = interaction_test_guard{};
    auto observed = game_client::interaction_snapshot{};
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        observed = game_client::current_interaction({.limit = 200});
        return resolve({
            .input_id = observed.input_id,
            .operation = game_client::interaction_operation::cancel,
        });
    });

    CHECK(handle_main_menu() == ACTION_NULL);

    CHECK(std::ranges::all_of(observed.choices, [](const auto& choice) {
        return !choice.action_id.empty();
    }));
    const auto options = std::ranges::
        find(observed.choices, "open_options", &game_client::interaction_choice::action_id);
    REQUIRE(options != observed.choices.end());
    CHECK(options->enabled);
    CHECK(options->label == "Options");
}
TEST_CASE(
    "the map offers Options as an action of its own", "[client][interaction][mcp][main_menu]") {
    const auto ctxt = get_default_mode_input_context();
    const auto actions = ctxt.get_registered_actions_copy();
    CHECK(std::ranges::find(actions, "open_options") != actions.end());
    CHECK(ctxt.get_action_name("open_options") == "Options");
}
TEST_CASE(
    "the map offers zoom to a client that draws the map itself",
    "[client][interaction][mcp][zoom]") {
    const auto ctxt = get_default_mode_input_context();
    const auto actions = ctxt.get_registered_actions_copy();
    for (const auto* const action : {"zoom_in", "zoom_out"}) {
        CAPTURE(action);
        CHECK(std::ranges::find(actions, action) != actions.end());
        // The client zooms its own map on the key the engine names for the action.
        CHECK(ctxt.keys_bound_to(action)
              == std::vector<char>{std::string_view(action) == "zoom_in" ? 'z' : 'Z'});
    }
}
TEST_CASE("real uilist publishes each printable entry hotkey", "[client][interaction][mcp]") {
    const auto guard = interaction_test_guard{};
    auto menu = uilist{};
    menu.title = "Hotkeys";
    menu.entries.emplace_back(10, true, 'e', "explicit");
    menu.entries.emplace_back(20, true, 300, "unprintable");
    menu.entries.emplace_back(30, true, -1, "none");
    auto observed = game_client::interaction_snapshot{};
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        observed = game_client::current_interaction();
        return resolve({
            .input_id = observed.input_id,
            .operation = game_client::interaction_operation::cancel,
        });
    });

    menu.query();

    REQUIRE(observed.choices.size() == 3);
    CHECK(observed.choices[0].hotkey == "e");
    CHECK(observed.choices[1].hotkey.empty());
    CHECK(observed.choices[2].hotkey == "1");
}
TEST_CASE("real uilist exposes filtered effective choices", "[client][interaction][mcp]") {
    const auto guard = interaction_test_guard{};
    auto menu = uilist{};
    menu.title = "Live list";
    menu.allow_disabled = false;
    menu.entries.emplace_back(10, true, MENU_AUTOASSIGN, "show enabled", "first description");
    menu.entries.emplace_back(20, false, MENU_AUTOASSIGN, "show disabled", "blocked");
    menu.entries.emplace_back(30, true, MENU_AUTOASSIGN, "hidden", "filtered");
    menu.set_filter("show");
    auto callback = counting_callback{};
    menu.callback = &callback;
    auto observed = game_client::interaction_snapshot{};
    auto stale_error = std::string{};
    auto disabled_error = std::string{};
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        observed = game_client::current_interaction();
        REQUIRE(observed.structured);
        REQUIRE(observed.choices.size() == 2);
        const auto disabled = std::ranges::
            find(observed.choices, false, &game_client::interaction_choice::selectable);
        REQUIRE(disabled != observed.choices.end());
        const auto disabled_result = game_client::resolve_interaction_command({
            .input_id = observed.input_id,
            .operation = game_client::interaction_operation::choose,
            .target_id = disabled->id,
        });
        REQUIRE_FALSE(disabled_result);
        disabled_error = disabled_result.error();
        const auto stale = game_client::resolve_interaction_command({
            .input_id = observed.input_id + 1,
            .operation = game_client::interaction_operation::choose,
            .target_id = observed.choices.front().id,
        });
        REQUIRE_FALSE(stale);
        stale_error = stale.error();
        return resolve({
            .input_id = observed.input_id,
            .operation = game_client::interaction_operation::choose,
            .target_id = observed.choices.front().id,
        });
    });

    menu.query();

    CHECK(menu.ret == 10);
    CHECK(observed.choices.front().description == "first description");
    CHECK(disabled_error.starts_with("disabled:"));
    CHECK(stale_error.starts_with("stale:"));
    CHECK(callback.selections > 0);
}
TEST_CASE(
    "real query popup supports semantic choice acknowledgement and cancel",
    "[client][interaction][mcp]") {
    const auto guard = interaction_test_guard{};
    SECTION("option") {
        game_client::memory::set_input_provider([&](const int /*timeout*/) {
            const auto snapshot = game_client::current_interaction();
            REQUIRE(snapshot.choices.size() == 2);
            const auto choice = std::ranges::
                find(snapshot.choices, "YES", &game_client::interaction_choice::action_id);
            REQUIRE(choice != snapshot.choices.end());
            return resolve({
                .input_id = snapshot.input_id,
                .operation = game_client::interaction_operation::choose,
                .target_id = choice->id,
            });
        });
        const auto result =
            query_popup()
                .context("YESNO")
                .message("%s", "Continue?")
                .option("YES")
                .option("NO")
                .query();
        CHECK(result.action == "YES");
    }
    SECTION("acknowledgement") {
        game_client::memory::set_input_provider([&](const int /*timeout*/) {
            const auto snapshot = game_client::current_interaction();
            REQUIRE(snapshot.choices.size() == 1);
            CHECK(snapshot.choices.front().action_id == "ANY_INPUT");
            CHECK(snapshot.choices.front().description.empty());
            CHECK(snapshot.choices.front().label == "Press any key");
            CHECK(snapshot.any_key_choice_id == snapshot.choices.front().id);
            // The key the player presses is the field's; the message is said once, by the popup.
            REQUIRE(snapshot.field.has_value());
            CHECK(snapshot.field->type == "key");
            CHECK(snapshot.field->label.empty());
            return resolve({
                .input_id = snapshot.input_id,
                .operation = game_client::interaction_operation::choose,
                .target_id = snapshot.choices.front().id,
            });
        });
        const auto result = query_popup().message("%s", "Notice").allow_anykey(true).query();
        CHECK(result.action == "ANY_INPUT");
    }
    SECTION("a popup that only closes offers OK") {
        game_client::memory::set_input_provider([&](const int /*timeout*/) {
            const auto snapshot = game_client::current_interaction();
            REQUIRE(snapshot.choices.size() == 1);
            CHECK(snapshot.choices.front().action_id == "QUIT");
            CHECK(snapshot.choices.front().description.empty());
            CHECK(snapshot.choices.front().label == "OK");
            return resolve({
                .input_id = snapshot.input_id,
                .operation = game_client::interaction_operation::choose,
                .target_id = snapshot.choices.front().id,
            });
        });
        const auto result = query_popup().message("%s", "Done").allow_cancel(true).query();
        CHECK(result.action == "QUIT");
    }
    SECTION("cancel") {
        game_client::memory::set_input_provider([&](const int /*timeout*/) {
            const auto snapshot = game_client::current_interaction();
            REQUIRE(snapshot.allow_cancel);
            return resolve({
                .input_id = snapshot.input_id,
                .operation = game_client::interaction_operation::cancel,
            });
        });
        const auto result =
            query_popup()
                .context("YESNO")
                .message("%s", "Cancel?")
                .option("YES")
                .allow_cancel(true)
                .query();
        CHECK(result.action == "QUIT");
    }
}
TEST_CASE(
    "real string popup validates replacement fields and submit state",
    "[client][interaction][mcp]") {
    const auto guard = interaction_test_guard{};
    auto reads = 0;
    auto numeric_error = std::string{};
    auto length_error = std::string{};
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto snapshot = game_client::current_interaction();
        REQUIRE(snapshot.field);
        CHECK(snapshot.field->type == "integer");
        CHECK(snapshot.field->max_length == 4);
        if (reads++ == 0) {
            const auto invalid_numeric = game_client::resolve_interaction_command({
                .input_id = snapshot.input_id,
                .operation = game_client::interaction_operation::fill,
                .target_id = snapshot.field->id,
                .value = "1x",
                .submit = true,
            });
            REQUIRE_FALSE(invalid_numeric);
            numeric_error = invalid_numeric.error();
            const auto invalid_length = game_client::resolve_interaction_command({
                .input_id = snapshot.input_id,
                .operation = game_client::interaction_operation::fill,
                .target_id = snapshot.field->id,
                .value = "12345",
                .submit = true,
            });
            REQUIRE_FALSE(invalid_length);
            length_error = invalid_length.error();
            for (const auto* wide : {"\xED\x95\x9C", "\xEF\xBC\x91"}) {
                const auto invalid_unicode = game_client::resolve_interaction_command({
                    .input_id = snapshot.input_id,
                    .operation = game_client::interaction_operation::fill,
                    .target_id = snapshot.field->id,
                    .value = wide,
                    .submit = true,
                });
                REQUIRE_FALSE(invalid_unicode);
                CHECK(invalid_unicode.error().starts_with("invalid:"));
            }
            return resolve({
                .input_id = snapshot.input_id,
                .operation = game_client::interaction_operation::fill,
                .target_id = snapshot.field->id,
                .value = "-12",
                .submit = false,
            });
        }
        CHECK(snapshot.field->value == "-12");
        return resolve({
            .input_id = snapshot.input_id,
            .operation = game_client::interaction_operation::fill,
            .target_id = snapshot.field->id,
            .value = "42",
            .submit = true,
        });
    });
    auto popup = string_input_popup{};
    const auto value =
        popup.title("Number").text("0").max_length(4).only_digits(true).query_string();
    CHECK(value == "42");
    CHECK(popup.confirmed());
    CHECK_FALSE(popup.canceled());
    CHECK(reads == 2);
    CHECK(numeric_error.starts_with("invalid:"));
    CHECK(length_error.starts_with("invalid:"));

    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto snapshot = game_client::current_interaction();
        return resolve({
            .input_id = snapshot.input_id,
            .operation = game_client::interaction_operation::cancel,
        });
    });
    auto canceled = string_input_popup{};
    CHECK(canceled.text("unchanged").query_string().empty());
    CHECK(canceled.canceled());
}
TEST_CASE(
    "real inventory picker exposes and selects live avatar items",
    "[client][interaction][inventory][mcp]") {
    clear_all_state();
    const auto cleanup = on_out_of_scope([]() { clear_all_state(); });
    const auto guard = interaction_test_guard{};
    auto& you = get_avatar();
    auto& expected = you.i_add(item::spawn("test_platinum_bit", calendar::turn, 7));
    auto selector = inventory_pick_selector(you);
    selector.add_character_items(you);
    auto observed = std::string{};
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto rows = selector_rows();
        REQUIRE(rows.size() == 1);
        const auto& choice = rows.front();
        CHECK(choice["selectable"] == true);
        CHECK(choice["selected"] == false);
        CHECK(selector_cursor() == choice["id"].get<std::string>());
        CHECK_FALSE(choice.contains("selected_count"));
        CHECK(choice["look"]["id"] == "test_platinum_bit");
        CHECK(top_screen()["root"]["child"]["children"][0]["id"] == "panel:inventory");
        CHECK(choice["description"].get<std::string>().size()
              > choice["label"].get<std::string>().size());
        observed = choice["id"];
        return choose_item(choice);
    });

    CHECK(selector.execute() == &expected);
    CHECK_FALSE(observed.contains("0x"));
}
TEST_CASE(
    "real inventory multiselect accepts exact quantity and zero",
    "[client][interaction][inventory][mcp]") {
    clear_all_state();
    const auto cleanup = on_out_of_scope([]() { clear_all_state(); });
    const auto guard = interaction_test_guard{};
    auto& you = get_avatar();
    you.i_add(item::spawn("test_platinum_bit", calendar::turn, 7));
    auto selector = inventory_drop_selector(you);
    selector.add_character_items(you);
    auto reads = 0;
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto rows = selector_rows();
        REQUIRE(rows.size() == 1);
        const auto& choice = rows.front();
        const auto screen = top_screen();
        REQUIRE(std::ranges::any_of(screen["affordances"], [](const auto& entry) {
            return entry["id"] == "list:inventory:count";
        }));
        if (reads++ == 0) {
            CHECK(choice["selected_count"] == 0);
            return count_item(choice, 0);
        }
        if (reads == 2) {
            CHECK(choice["selected_count"] == 0);
            // Below the shape's minimum is refused; above what the stack holds is the whole stack.
            const auto below = engine_client::resolve_invoke(
                stack_state(),
                {.affordance_id = "list:inventory:count",
                 .args = {.row = choice["id"].get<std::string>(), .count = -1}});
            REQUIRE_FALSE(below);
            CHECK(below.error() == engine_client::error::invalid_arguments);
            return count_item(choice, 8);
        }
        if (reads == 3) {
            CHECK(choice["selected_count"] == 7);
            return count_item(choice, 3);
        }
        CHECK(choice["selected_count"] == 3);
        return resolve_action("CONFIRM");
    });

    const auto dropped = selector.execute();
    REQUIRE(dropped.size() == 1);
    CHECK(dropped.front().count == 3);
}
namespace {
/// The key as a client presses it: the first enabled affordance of the screen on top that binds it,
/// a digit.
auto press_digit(const std::string& key) -> input_event {
    const auto affordances = top_screen()["affordances"];
    const auto bound = std::find_if(affordances.begin(), affordances.end(), [&](const auto& entry) {
        const auto& keys = entry["keys"];
        return entry["enabled"] == true && std::find(keys.begin(), keys.end(), key) != keys.end();
    });
    REQUIRE(bound != affordances.end());
    CHECK((*bound)["role"] == "digit");
    return invoke((*bound)["id"].get<std::string>());
}
} // namespace
TEST_CASE(
    "real inventory multiselect counts the digits typed before the mark as the native list does",
    "[client][interaction][inventory][mcp]") {
    clear_all_state();
    const auto cleanup = on_out_of_scope([]() { clear_all_state(); });
    const auto guard = interaction_test_guard{};
    auto& you = get_avatar();
    you.i_add(item::spawn("test_platinum_bit", calendar::turn, 7));
    auto selector = inventory_iuse_selector(you, "Use");
    selector.add_character_items(you);
    auto reads = 0;
    game_client::memory::set_input_provider([&](const int /*timeout*/) -> input_event {
        switch (reads++) {
            case 0:
                return press_digit("0");
            case 1:
                return press_digit("6");
            case 2:
                return resolve_action("RIGHT");
            default:
                break;
        }
        CHECK(selector_rows().front()["selected_count"] == 6);
        return resolve_action("CONFIRM");
    });

    const auto used = selector.execute();
    REQUIRE(used.size() == 1);
    CHECK(used.front().count == 6);
}
TEST_CASE(
    "real drop list opens its count prompt with the digit typed as the native list does",
    "[client][interaction][inventory][mcp]") {
    clear_all_state();
    const auto cleanup = on_out_of_scope([]() { clear_all_state(); });
    const auto guard = interaction_test_guard{};
    auto& you = get_avatar();
    you.i_add(item::spawn("test_platinum_bit", calendar::turn, 7));
    auto selector = inventory_drop_selector(you);
    selector.add_character_items(you);
    auto reads = 0;
    game_client::memory::set_input_provider([&](const int /*timeout*/) -> input_event {
        if (reads++ == 0) { return press_digit("6"); }
        if (reads == 2) {
            const auto snapshot = game_client::current_interaction();
            REQUIRE(snapshot.field);
            CHECK(snapshot.field->value == "6");
            return resolve({
                .input_id = snapshot.input_id,
                .operation = game_client::interaction_operation::fill,
                .target_id = snapshot.field->id,
                .value = "3",
                .submit = true,
            });
        }
        CHECK(selector_rows().front()["selected_count"] == 3);
        return resolve_action("CONFIRM");
    });

    const auto dropped = selector.execute();
    REQUIRE(dropped.size() == 1);
    CHECK(dropped.front().count == 3);
}
TEST_CASE(
    "inventory selection and navigation highlights remain independent",
    "[client][interaction][inventory][mcp]") {
    clear_all_state();
    const auto cleanup = on_out_of_scope([]() { clear_all_state(); });
    const auto guard = interaction_test_guard{};
    auto& you = get_avatar();
    auto items = std::vector<detached_ptr<item>>{};
    for (const auto index : std::views::iota(0, 3)) {
        items.push_back(item::spawn("test_platinum_bit", calendar::turn, index + 1));
    }
    auto selector = inventory_drop_selector(you);
    for (const auto& candidate : items) {
        selector.add_item(selector.own_inv_column, candidate.get());
    }
    auto reads = 0;
    auto selected_id = std::string{};
    const auto row_by_id = [](const std::string& id) {
        const auto rows = selector_rows();
        const auto found = std::ranges::find(rows, id, [](const auto& row) {
            return row["id"].template get<std::string>();
        });
        REQUIRE(found != rows.end());
        return *found;
    };
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto rows = selector_rows();
        REQUIRE(rows.size() == 3);
        if (reads++ == 0) {
            const auto cursor = row_by_id(selector_cursor());
            CHECK(cursor["selected"] == false);
            CHECK(cursor["selected_count"] == 0);
            selected_id = cursor["id"];
            return count_item(cursor, 1);
        }
        const auto selected = row_by_id(selected_id);
        if (reads == 2) {
            CHECK(selected["selected"] == true);
            CHECK(selector_cursor() == selected_id);
            CHECK(selected["selected_count"] == 1);
            auto moved = false;
            for (const auto& candidate : items) {
                REQUIRE(selector.select(candidate.get()));
                if (selector_cursor() != selected_id) {
                    moved = true;
                    break;
                }
            }
            REQUIRE(moved);
            const auto persisted = row_by_id(selected_id);
            CHECK(persisted["selected"] == true);
            CHECK(persisted["selected_count"] == 1);
            CHECK(selector_cursor() != selected_id);
            return count_item(persisted, 0);
        }
        if (reads == 3) {
            CHECK(selected["selected"] == false);
            CHECK(selector_cursor() == selected_id);
            CHECK(selected["selected_count"] == 0);
            return resolve_action("CATEGORY_SELECTION");
        }
        CHECK(std::ranges::none_of(rows, [](const auto& row) { return row["selected"] == true; }));
        // The cursor is on the category's heading, which is no item row.
        CHECK(std::ranges::none_of(rows, [](const auto& row) {
            return row["id"].template get<std::string>() == selector_cursor();
        }));
        CHECK_FALSE(selector_cursor().empty());
        return resolve_action("QUIT");
    });

    CHECK(selector.execute().empty());
    CHECK(reads == 4);
}
namespace {
class view_inventory_selector final: public inventory_pick_selector {
public:
    using inventory_pick_selector::inventory_pick_selector;
    using inventory_selector::interaction_view;
    using inventory_selector::open_screen;
};

/// The component of `view` with `id`, null when it has none.
auto component_of(const nlohmann::json& node, const std::string& id) -> nlohmann::json {
    if (!node.is_object()) { return nullptr; }
    if (node["id"].get<std::string>() == id) { return node; }
    auto found = nlohmann::json(nullptr);
    if (node.contains("child")) { found = component_of(node["child"], id); }
    for (const auto& child : node.value("children", nlohmann::json::array())) {
        if (found.is_null()) { found = component_of(child, id); }
    }
    return found;
}
} // namespace

TEST_CASE(
    "a real inventory selector publishes its header totals, hint, categories and colors",
    "[client][interaction][inventory][mcp]") {
    clear_all_state();
    const auto cleanup = on_out_of_scope([]() { clear_all_state(); });
    auto& you = get_avatar();
    you.i_add(item::spawn("bandages"), false);
    REQUIRE_FALSE(you.wear_item(item::spawn("tshirt"), false));
    auto selector = view_inventory_selector(you);
    selector.set_hint("Item hotkeys assigned: <color_light_gray>1</color>/52");
    selector.add_character_items(you);

    const auto screen = [&] {
        const auto scope = selector.open_screen();
        return top_screen();
    }();
    const auto& panel = screen["root"];
    CHECK(panel["subtitle"] == "Item hotkeys assigned: 1/52");
    REQUIRE(panel.contains("stats"));
    CHECK(panel["stats"][0]["label"].get<std::string>()
          == string_format("Weight (%s):", weight_units()));
    CHECK(panel["stats"][1]["label"].get<std::string>()
          == string_format("Volume (%s):", volume_units_abbr()));
    CHECK_FALSE(panel["stats"][0]["max"].get<std::string>().empty());
    // The cursor is on an item, not on a category heading.
    auto lists = std::vector<nlohmann::json>{};
    lists_below(panel, lists);
    CHECK(std::ranges::any_of(lists, [&](const auto& list) {
        return std::ranges::any_of(list["rows"], [&](const auto& row) {
            return !row.value("header", false)
                && row["id"].template get<std::string>()
                       == screen["focus"]["row_id"].template get<std::string>();
        });
    }));

    const auto worn_list = component_of(panel, "list:worn");
    REQUIRE(worn_list.is_object());
    const auto& worn_rows = worn_list["rows"];
    CHECK(worn_rows.front()["header"] == true);
    CHECK(worn_rows.front()["label"] == "ITEMS WORN");
    const auto& worn = worn_rows[1];
    CHECK_FALSE(worn["color"].get<std::string>().empty());
    CHECK_FALSE(worn["key_color"].get<std::string>().empty());
    CHECK_FALSE(worn["markup"].get<std::string>().empty());
    const auto carried_list = component_of(panel, "list:inventory");
    REQUIRE(carried_list.is_object());
    CHECK(carried_list["rows"].front()["label"] != "ITEMS WORN");

    selector.set_display_stats(false);
    const auto bare = selector.interaction_view();
    CHECK(std::get<engine_client::panel_body>(bare.root->body).stats.empty());
}
TEST_CASE(
    "bounded interaction pages control late real inventory entries and reset safely",
    "[client][interaction][inventory][mcp]") {
    clear_all_state();
    const auto cleanup = on_out_of_scope([]() { clear_all_state(); });
    const auto guard = interaction_test_guard{};
    auto& you = get_avatar();
    auto items = std::vector<item*>{};
    items.reserve(102);
    for (const auto index : std::views::iota(0, 101)) {
        auto candidate = item::spawn("test_platinum_bit", calendar::turn, index == 100 ? 7 : 1);
        candidate->set_var("interaction_paging_index", index);
        items.push_back(&you.i_add(std::move(candidate), false));
    }
    items.push_back(&you.i_add(item::spawn("bandages"), false));

    const auto preset = denied_inventory_preset{};
    auto selector = inventory_drop_selector(you, preset);
    selector.add_character_items(you);
    auto selected_id = std::string{};
    auto reads = 0;
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto rows = selector_rows();
        if (reads++ == 0) {
            REQUIRE(rows.size() == 102);
            const auto disabled = std::ranges::find_if(rows, [](const auto& row) {
                return row["selectable"] == false;
            });
            REQUIRE(disabled != rows.end());
            CHECK((*disabled)["denial"] == "Denied by test preset");
            // A denied row is neither chosen nor counted.
            return count_item(*disabled, 1);
        }
        if (reads == 2) {
            CHECK(std::ranges::none_of(rows, [](const auto& row) {
                return row["selected"] == true;
            }));
            // The late entry is a row like any other, however far down the list.
            const auto late = std::ranges::find_if(rows, [](const auto& row) {
                return row["selectable"] == true && row.value("max", 0) == 7;
            });
            REQUIRE(late != rows.end());
            selected_id = (*late)["id"];
            return count_item(*late, 1);
        }
        const auto selected = std::ranges::find_if(rows, [&](const auto& row) {
            return row["id"].template get<std::string>() == selected_id;
        });
        REQUIRE(selected != rows.end());
        if (reads == 3) {
            CHECK((*selected)["selected"] == true);
            CHECK((*selected)["selected_count"] == 1);
            return count_item(*selected, 0);
        }

        CHECK((*selected)["selected"] == false);
        CHECK((*selected)["selected_count"] == 0);
        selector.set_filter("__interaction_paging_no_match__");
        CHECK(selector_rows().empty());
        const auto removed = engine_client::resolve_invoke(
            stack_state(),
            {.affordance_id = "list:inventory:count", .args = {.row = selected_id, .count = 1}});
        REQUIRE_FALSE(removed);

        selector.set_filter("");
        CHECK(selector_rows().size() == 102);
        selector.clear_items();
        selector.add_item(selector.own_inv_column, items.back());
        CHECK(selector_rows().size() == 1);
        selector.add_item(selector.own_inv_column, items.front());
        selector.set_filter(selector.get_filter());
        CHECK(selector_rows().size() == 2);
        return resolve_action("QUIT");
    });

    CHECK(selector.execute().empty());
    CHECK(reads == 4);
}
TEST_CASE(
    "semantic ground pickup transfers exact noncharge and charge quantities",
    "[client][interaction][pickup][mcp]") {
    const auto guard = interaction_test_guard{};

    SECTION("two of three identical items") {
        const auto fixture = pickup_fixture_guard{};
        const auto id = itype_id("test_screwdriver");
        std::ranges::for_each(std::views::iota(0, 3), [&](const auto /*index*/) {
            add_ground_item(id);
        });
        auto reads = 0;
        game_client::memory::set_input_provider([&](const int /*timeout*/) {
            const auto rows = pickup_rows();
            REQUIRE(rows.size() == 1);
            const auto& choice = rows.front();
            CHECK(choice["max"] == 3);
            CHECK(choice["minimum_count"] == 0);
            CHECK(choice["look"]["id"].get<std::string>() == id.str());
            CHECK_FALSE(choice["hotkey"].get<std::string>().empty());
            CHECK_FALSE(pickup_stat("weight")["max"].get<std::string>().empty());
            if (reads++ == 0) { return pick_count(choice, 2); }
            CHECK(choice["selected"] == true);
            CHECK(choice["selected_count"] == 2);
            return resolve_action("CONFIRM");
        });

        pickup::pick_up(pickup_test_pos, 0, pickup::from_ground);
        finish_pickup_activity(get_avatar());

        CHECK(get_avatar().amount_of(id) == 2);
        CHECK(ground_item_count(id) == 1);
    }

    SECTION("the header predicts the carried weight and volume of the marked stacks") {
        const auto fixture = pickup_fixture_guard{};
        const auto id = itype_id("test_screwdriver");
        std::ranges::for_each(std::views::iota(0, 3), [&](const auto /*index*/) {
            add_ground_item(id);
        });
        auto reads = 0;
        auto before = std::string{};
        game_client::memory::set_input_provider([&](const int /*timeout*/) {
            const auto rows = pickup_rows();
            REQUIRE(rows.size() == 1);
            CHECK_FALSE(pickup_stat("volume")["value"].get<std::string>().empty());
            const auto weight = pickup_stat("weight")["value"].get<std::string>();
            if (reads++ == 0) {
                before = weight;
                return pick_count(rows.front(), 2);
            }
            CHECK(weight != before);
            return resolve_action("CONFIRM");
        });

        pickup::pick_up(pickup_test_pos, 0, pickup::from_ground);
        finish_pickup_activity(get_avatar());
        CHECK(reads == 2);
    }

    SECTION("the header marks the predicted weight red once it passes the capacity") {
        const auto fixture = pickup_fixture_guard{};
        const auto id = itype_id("test_1kg_cube");
        std::ranges::for_each(std::views::iota(0, 60), [&](const auto /*index*/) {
            add_ground_item(id);
        });
        auto reads = 0;
        auto tone = std::string{};
        game_client::memory::set_input_provider([&](const int /*timeout*/) {
            const auto rows = pickup_rows();
            REQUIRE(rows.size() == 1);
            if (reads++ == 0) {
                CHECK(pickup_stat("weight").value("tone", "") != "bad");
                return pick_count(rows.front(), 60);
            }
            tone = pickup_stat("weight").value("tone", "");
            return resolve_action("QUIT");
        });

        pickup::pick_up(pickup_test_pos, 0, pickup::from_ground);
        CHECK(reads == 2);
        CHECK(tone == "bad");
    }

    SECTION("three of ten charges") {
        const auto fixture = pickup_fixture_guard{};
        const auto id = itype_id("test_platinum_bit");
        add_ground_item(id, 10);
        auto reads = 0;
        game_client::memory::set_input_provider([&](const int /*timeout*/) {
            const auto rows = pickup_rows();
            REQUIRE(rows.size() == 1);
            const auto& choice = rows.front();
            CHECK(choice["max"] == 10);
            if (reads++ == 0) { return pick_count(choice, 3); }
            CHECK(choice["selected_count"] == 3);
            return resolve_action("CONFIRM");
        });

        pickup::pick_up(pickup_test_pos, 0, pickup::from_ground);
        finish_pickup_activity(get_avatar());

        CHECK(get_avatar().charges_of(id) == 3);
        CHECK(ground_charges(id) == 7);
    }
}
TEST_CASE(
    "ground pickup rows publish the native stolen marker and item symbol, and the window dock",
    "[client][interaction][pickup][mcp]") {
    const auto guard = interaction_test_guard{};
    const auto fixture = pickup_fixture_guard{};
    const auto symbols = override_option("ITEM_SYMBOLS", "true");
    const auto position = override_option("PICKUP_POSITION", "right");
    g->faction_manager_ptr->create_if_needed();
    auto& owner = spawn_npc(pickup_test_pos + tripoint_north, "test_talker");
    owner.set_fac(faction_id("free_merchants"));
    const auto id = itype_id("test_screwdriver");
    auto& stolen = add_ground_item(id);
    stolen.set_owner(owner);
    const auto own = itype_id("test_1kg_cube");
    add_ground_item(own);
    auto checked = false;
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto rows = pickup_rows();
        const auto markup = [&](const itype_id& type) {
            const auto row = row_of(rows, type.str());
            REQUIRE(row.is_object());
            return row["markup"].get<std::string>();
        };
        CHECK(markup(id).starts_with("<color_light_red>!</color> "));
        CHECK(markup(id).find(item::spawn(id)->symbol() + " ") != std::string::npos);
        CHECK_FALSE(markup(own).starts_with("<color_light_red>!</color>"));
        CHECK(markup(own).starts_with(item::spawn(own)->symbol() + " "));
        CHECK(top_screen()["dock"] == "right");
        checked = true;
        return resolve_action("QUIT");
    });
    pickup::pick_up(pickup_test_pos, 0, pickup::from_ground);
    CHECK(checked);
}
TEST_CASE(
    "semantic ground pickup preserves whole zero filter and stale validation",
    "[client][interaction][pickup][mcp]") {
    const auto guard = interaction_test_guard{};

    SECTION("available count normalizes to native whole stack") {
        const auto fixture = pickup_fixture_guard{};
        const auto id = itype_id("test_screwdriver");
        std::ranges::for_each(std::views::iota(0, 2), [&](const auto /*index*/) {
            add_ground_item(id);
        });
        auto reads = 0;
        game_client::memory::set_input_provider([&](const int /*timeout*/) {
            const auto rows = pickup_rows();
            REQUIRE(rows.size() == 1);
            if (reads++ == 0) {
                // Below the shape's minimum is refused; above what the stack holds is the whole
                // stack.
                const auto below = engine_client::resolve_invoke(
                    stack_state(),
                    {.affordance_id = "pickup:count",
                     .args = {.row = rows.front()["id"].get<std::string>(), .count = -1}});
                REQUIRE_FALSE(below);
                return pick_count(rows.front(), 3);
            }
            CHECK(rows.front()["selected_count"] == 2);
            CHECK(rows.front()["mark"]["glyph"] == "+");
            return resolve_action("CONFIRM");
        });
        pickup::pick_up(pickup_test_pos, 0, pickup::from_ground);
        finish_pickup_activity(get_avatar());
        CHECK(get_avatar().amount_of(id) == 2);
        CHECK(ground_item_count(id) == 0);
    }

    SECTION("zero unselects and cancel changes neither world nor activity") {
        const auto fixture = pickup_fixture_guard{};
        const auto id = itype_id("test_screwdriver");
        add_ground_item(id);
        auto reads = 0;
        game_client::memory::set_input_provider([&](const int /*timeout*/) {
            const auto rows = pickup_rows();
            REQUIRE(rows.size() == 1);
            const auto& choice = rows.front();
            if (reads == 0) {
                ++reads;
                return pick_row(choice);
            }
            if (reads == 1) {
                ++reads;
                CHECK(choice["selected"] == true);
                return pick_count(choice, 0);
            }
            CHECK(choice["selected"] == false);
            CHECK(choice["selected_count"] == 0);
            return resolve_action("QUIT");
        });
        pickup::pick_up(pickup_test_pos, 0, pickup::from_ground);
        CHECK_FALSE(get_avatar().activity);
        CHECK(get_avatar().amount_of(id) == 0);
        CHECK(ground_item_count(id) == 1);
    }

    SECTION("filter hides but preserves an existing selection through native confirmation") {
        const auto fixture = pickup_fixture_guard{};
        const auto hidden_id = itype_id("test_screwdriver");
        const auto visible_id = itype_id("hammer");
        add_ground_item(hidden_id);
        add_ground_item(visible_id);
        auto hidden_choice_id = std::string{};
        auto step = 0;
        game_client::memory::set_input_provider([&](const int /*timeout*/) {
            const auto snapshot = game_client::current_interaction();
            if (snapshot.kind == game_client::interaction_kind::field) {
                return resolve({
                    .input_id = snapshot.input_id,
                    .operation = game_client::interaction_operation::fill,
                    .target_id = snapshot.field->id,
                    .value = "hammer",
                    .submit = true,
                });
            }
            const auto rows = pickup_rows();
            if (step == 0) {
                const auto hidden = row_of(rows, hidden_id.str());
                REQUIRE(hidden.is_object());
                hidden_choice_id = hidden["id"].get<std::string>();
                ++step;
                return pick_row(hidden);
            }
            if (step == 1) {
                CHECK(std::ranges::any_of(rows, [&](const auto& row) {
                    return row["id"].template get<std::string>() == hidden_choice_id
                        && row["selected"] == true;
                }));
                ++step;
                return resolve_action("FILTER");
            }
            REQUIRE(rows.size() == 1);
            const auto& visible = rows.front();
            CHECK(visible["look"]["id"].get<std::string>() == visible_id.str());
            CHECK(visible["id"].get<std::string>() != hidden_choice_id);
            CHECK(visible["label"].get<std::string>().find("screwdriver") == std::string::npos);
            CHECK(
                visible["description"].get<std::string>().find("screwdriver") == std::string::npos);
            const auto stale_hidden = engine_client::resolve_invoke(
                stack_state(),
                {.affordance_id = "pickup:toggle", .args = {.row = hidden_choice_id}});
            REQUIRE_FALSE(stale_hidden);
            if (step == 2) {
                ++step;
                return pick_count(visible, 1);
            }
            CHECK(visible["selected"] == true);
            CHECK(visible["selected_count"] == 1);
            return resolve_action("CONFIRM");
        });
        pickup::pick_up(pickup_test_pos, 0, pickup::from_ground);
        finish_pickup_activity(get_avatar());
        CHECK(get_avatar().amount_of(hidden_id) == 1);
        CHECK(get_avatar().amount_of(visible_id) == 1);
        CHECK(ground_item_count(hidden_id) == 0);
        CHECK(ground_item_count(visible_id) == 0);
    }

    SECTION("filter only publishes matches and rejects the removed opaque id") {
        const auto fixture = pickup_fixture_guard{};
        add_ground_item(itype_id("test_screwdriver"));
        add_ground_item(itype_id("bandages"));
        auto old_id = std::string{};
        auto reads = 0;
        game_client::memory::set_input_provider([&](const int /*timeout*/) {
            const auto snapshot = game_client::current_interaction();
            if (snapshot.kind != game_client::interaction_kind::field && reads++ == 0) {
                const auto rows = pickup_rows();
                REQUIRE(rows.size() == 2);
                old_id = rows.front()["id"].get<std::string>();
                return resolve_action("FILTER");
            }
            if (snapshot.kind == game_client::interaction_kind::field) {
                return resolve({
                    .input_id = snapshot.input_id,
                    .operation = game_client::interaction_operation::fill,
                    .target_id = snapshot.field->id,
                    .value = "bandage",
                    .submit = true,
                });
            }
            const auto rows = pickup_rows();
            REQUIRE(rows.size() == 1);
            CHECK(rows.front()["look"]["id"] == "bandages");
            CHECK(rows.front()["id"].get<std::string>() != old_id);
            const auto removed = engine_client::resolve_invoke(
                stack_state(), {.affordance_id = "pickup:toggle", .args = {.row = old_id}});
            REQUIRE_FALSE(removed);
            const auto unknown = engine_client::resolve_invoke(
                stack_state(), {.affordance_id = "pickup:removed", .args = {.row = old_id}});
            REQUIRE_FALSE(unknown);
            return resolve_action("QUIT");
        });
        pickup::pick_up(pickup_test_pos, 0, pickup::from_ground);
        CHECK_FALSE(get_avatar().activity);
        CHECK(ground_item_count(itype_id("test_screwdriver")) == 1);
        CHECK(ground_item_count(itype_id("bandages")) == 1);
    }
}
TEST_CASE(
    "semantic ground pickup preserves native capacity refusal",
    "[client][interaction][pickup][mcp]") {
    const auto guard = interaction_test_guard{};
    const auto fixture = pickup_fixture_guard{false};
    const auto id = itype_id("test_overweight_cube");
    auto& you = get_avatar();
    auto pickup_item = item::spawn(id);
    REQUIRE(pickup_item);
    REQUIRE_FALSE(you.can_pick_weight(pickup_item->weight(), false));
    get_map().add_item(pickup_test_pos, std::move(pickup_item));
    auto saw_capacity_prompt = false;
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto snapshot = game_client::current_interaction();
        if (const auto rows = pickup_rows(); !rows.empty()) {
            if (rows.front()["selected"] == false) { return pick_row(rows.front()); }
            return resolve_action("CONFIRM");
        }
        REQUIRE(snapshot.context == "UILIST");
        CHECK(snapshot.message.find("heavy") != std::string::npos);
        saw_capacity_prompt = true;
        return resolve({
            .input_id = snapshot.input_id,
            .operation = game_client::interaction_operation::cancel,
        });
    });

    pickup::pick_up(pickup_test_pos, 0, pickup::from_ground);
    finish_pickup_activity(you);

    CHECK(saw_capacity_prompt);
    CHECK(you.amount_of(id) == 0);
    CHECK(ground_item_count(id) == 1);
}
TEST_CASE(
    "semantic ground pickup keeps native nesting wear wield and select all paths",
    "[client][interaction][pickup][mcp]") {
    const auto guard = interaction_test_guard{};

    const auto add_nested_items = [] {
        auto& parent = add_ground_item(itype_id("backpack"));
        auto& first_child = add_ground_item(itype_id("test_screwdriver"));
        auto& second_child = add_ground_item(itype_id("bandages"));
        auto& tokens = drop_token::get_provider();
        *parent.drop_token = tokens.make_next(calendar::turn);
        *first_child.drop_token = tokens.make_next(calendar::turn);
        first_child.drop_token->parent_number = parent.drop_token->drop_number;
        *second_child.drop_token = tokens.make_next(calendar::turn);
        second_child.drop_token->parent_number = parent.drop_token->drop_number;
    };

    SECTION("choosing a parent propagates to multiple children") {
        const auto fixture = pickup_fixture_guard{};
        add_nested_items();
        auto reads = 0;
        game_client::memory::set_input_provider([&](const int /*timeout*/) {
            const auto rows = pickup_rows();
            REQUIRE(rows.size() == 3);
            const auto parent = row_of(rows, "backpack");
            REQUIRE(parent.is_object());
            if (reads++ == 0) {
                CHECK_FALSE(parent.contains("nest_color"));
                CHECK_FALSE(parent.contains("parent_id"));
                CHECK(parent["mark"]["glyph"] == "-");
                for (const auto& child : rows) {
                    if (child["id"] == parent["id"]) { continue; }
                    CHECK(child["parent_id"] == parent["id"]);
                    CHECK(child["nest_color"] == "c_dark_gray");
                }
                return pick_row(parent);
            }
            CHECK(std::ranges::all_of(rows, [](const auto& row) {
                return row["selected"] == true;
            }));
            CHECK(std::ranges::all_of(rows, [](const auto& row) {
                return row["mark"]["glyph"] == "+" && row["mark"]["color"] == "c_light_blue";
            }));
            CHECK(
                std::ranges::count_if(
                    rows,
                    [](const auto& row) { return row.value("nest_color", "") == "c_light_blue"; })
                == 2);
            return resolve_action("CONFIRM");
        });
        pickup::pick_up(pickup_test_pos, 0, pickup::from_ground);
        finish_pickup_activity(get_avatar());
        CHECK(get_map().i_at(pickup_test_pos).empty());
        CHECK(get_avatar().amount_of(itype_id("backpack")) == 1);
        CHECK(get_avatar().amount_of(itype_id("test_screwdriver")) == 1);
        CHECK(get_avatar().amount_of(itype_id("bandages")) == 1);
    }

    SECTION("choosing only a child leaves its parent and sibling") {
        const auto fixture = pickup_fixture_guard{};
        add_nested_items();
        auto reads = 0;
        game_client::memory::set_input_provider([&](const int /*timeout*/) {
            const auto rows = pickup_rows();
            const auto child = row_of(rows, "test_screwdriver");
            REQUIRE(child.is_object());
            if (reads++ == 0) {
                CHECK(child["parent_id"].get<std::string>().starts_with("item:"));
                CHECK(child["nest_color"] == "c_dark_gray");
                return pick_row(child);
            }
            CHECK(child["selected"] == true);
            CHECK(
                std::ranges::count_if(rows, [](const auto& row) { return row["selected"] == true; })
                == 1);
            return resolve_action("CONFIRM");
        });
        pickup::pick_up(pickup_test_pos, 0, pickup::from_ground);
        finish_pickup_activity(get_avatar());
        CHECK(get_avatar().amount_of(itype_id("test_screwdriver")) == 1);
        CHECK(ground_item_count(itype_id("backpack")) == 1);
        CHECK(ground_item_count(itype_id("bandages")) == 1);
    }

    SECTION("native filtered select all picks only the visible subset") {
        const auto fixture = pickup_fixture_guard{};
        add_ground_item(itype_id("test_screwdriver"));
        add_ground_item(itype_id("bandages"));
        auto step = 0;
        game_client::memory::set_input_provider([&](const int /*timeout*/) {
            const auto snapshot = game_client::current_interaction();
            if (step++ == 0) { return resolve_action("FILTER"); }
            if (snapshot.kind == game_client::interaction_kind::field) {
                return resolve({
                    .input_id = snapshot.input_id,
                    .operation = game_client::interaction_operation::fill,
                    .target_id = snapshot.field->id,
                    .value = "bandage",
                    .submit = true,
                });
            }
            const auto rows = pickup_rows();
            REQUIRE(rows.size() == 1);
            if (rows.front()["selected"] == false) { return resolve_action("SELECT_ALL"); }
            return resolve_action("CONFIRM");
        });
        pickup::pick_up(pickup_test_pos, 0, pickup::from_ground);
        finish_pickup_activity(get_avatar());
        CHECK(get_avatar().amount_of(itype_id("bandages")) == 1);
        CHECK(ground_item_count(itype_id("test_screwdriver")) == 1);
    }

    SECTION("native wear and wield keep their checks and transfer paths") {
        const auto fixture = pickup_fixture_guard{};
        add_ground_item(itype_id("backpack"));
        auto step = 0;
        game_client::memory::set_input_provider([&](const int /*timeout*/) {
            if (step++ == 0) { return pick_row(pickup_rows().front()); }
            return resolve_action("WEAR");
        });
        pickup::pick_up(pickup_test_pos, 0, pickup::from_ground);
        CHECK_FALSE(get_avatar().activity);
        CHECK(std::ranges::any_of(get_avatar().worn, [](const auto& worn) {
            return worn->typeId() == itype_id("backpack");
        }));
        CHECK(ground_item_count(itype_id("backpack")) == 0);

        add_ground_item(itype_id("machete"));
        step = 0;
        game_client::memory::set_input_provider([&](const int /*timeout*/) {
            if (step++ == 0) { return pick_row(pickup_rows().front()); }
            return resolve_action("WIELD");
        });
        pickup::pick_up(pickup_test_pos, 0, pickup::from_ground);
        CHECK(get_avatar().primary_weapon().typeId() == itype_id("machete"));
        CHECK(ground_item_count(itype_id("machete")) == 0);
    }

    SECTION("native wear denial leaves an unavailable item on the ground") {
        const auto fixture = pickup_fixture_guard{};
        add_ground_item(itype_id("machete"));
        auto reads = 0;
        game_client::memory::set_input_provider([&](const int /*timeout*/) {
            REQUIRE(pickup_rows().size() == 1);
            if (reads++ == 0) { return resolve_action("WEAR"); }
            return resolve_action("QUIT");
        });
        pickup::pick_up(pickup_test_pos, 0, pickup::from_ground);
        CHECK_FALSE(get_avatar().activity);
        CHECK(ground_item_count(itype_id("machete")) == 1);
    }
}
TEST_CASE(
    "semantic replay repeats real partial ground pickup transfer",
    "[client][interaction][pickup][replay][mcp]") {
    const auto guard = interaction_test_guard{};
    static auto counter = std::atomic_uint64_t{0};
    const auto directory =
        std::filesystem::path(PATH_INFO::user_dir())
        / ("semantic-pickup-replay-" + std::to_string(counter.fetch_add(1)));
    REQUIRE(std::filesystem::create_directory(directory));
    const auto cleanup_replay = on_out_of_scope([&] {
        replay::stop();
        std::filesystem::remove_all(directory);
        clear_pickup_fixture_state();
    });
    const auto path = directory / "input.jsonl";
    const auto id = itype_id("test_screwdriver");
    const auto setup = [&] {
        clear_pickup_fixture_state();
        auto& you = get_avatar();
        you.setpos(map_local_to_abs(get_map(), pickup_test_pos));
        you.set_moves(1000);
        you.worn.push_back(item::spawn("duffelbag"));
        std::ranges::for_each(std::views::iota(0, 3), [&](const auto /*index*/) {
            add_ground_item(id);
        });
    };

    setup();
    replay::configure_recording(path.string(), {.rng_seed = 424242});
    replay::start();
    auto reads = 0;
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        if (reads++ == 0) { return pick_count(pickup_rows().front(), 2); }
        return resolve_action("CONFIRM");
    });
    pickup::pick_up(pickup_test_pos, 0, pickup::from_ground);
    finish_pickup_activity(get_avatar());
    replay::finish();
    REQUIRE(get_avatar().amount_of(id) == 2);
    REQUIRE(ground_item_count(id) == 1);

    setup();
    game_client::memory::set_input_provider([](const int /*timeout*/) -> input_event {
        throw std::runtime_error("playback must not read live input");
    });
    replay::configure_playback(path.string(), {.rng_seed = 424242});
    replay::start();
    pickup::pick_up(pickup_test_pos, 0, pickup::from_ground);
    finish_pickup_activity(get_avatar());
    CHECK(get_avatar().amount_of(id) == 2);
    CHECK(ground_item_count(id) == 1);
    CHECK_THROWS_AS(replay::next_input_event(), replay::completed);
    replay::finish();
}
TEST_CASE(
    "semantic replay restores real inventory quantity selection",
    "[client][interaction][inventory][replay][mcp]") {
    clear_all_state();
    const auto cleanup_state = on_out_of_scope([]() { clear_all_state(); });
    const auto guard = interaction_test_guard{};
    static auto counter = std::atomic_uint64_t{0};
    const auto directory =
        std::filesystem::path(PATH_INFO::user_dir())
        / ("semantic-inventory-replay-" + std::to_string(counter.fetch_add(1)));
    REQUIRE(std::filesystem::create_directory(directory));
    const auto cleanup_replay = on_out_of_scope([&] {
        replay::stop();
        std::filesystem::remove_all(directory);
    });
    const auto path = directory / "input.jsonl";
    auto& you = get_avatar();
    you.i_add(item::spawn("test_platinum_bit", calendar::turn, 7));

    replay::configure_recording(path.string(), {.rng_seed = 23});
    replay::start();
    auto reads = 0;
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        if (reads++ == 0) { return count_item(selector_rows().front(), 2); }
        return resolve_action("CONFIRM");
    });
    auto recorded_selector = inventory_drop_selector(you);
    recorded_selector.add_character_items(you);
    const auto recorded = recorded_selector.execute();
    replay::finish();
    REQUIRE(recorded.size() == 1);
    CHECK(recorded.front().count == 2);

    game_client::memory::set_input_provider([](const int /*timeout*/) -> input_event {
        throw std::runtime_error("playback must not read live input");
    });
    replay::configure_playback(path.string(), {.rng_seed = 23});
    replay::start();
    auto playback_selector = inventory_drop_selector(you);
    playback_selector.add_character_items(you);
    const auto playback = playback_selector.execute();
    REQUIRE(playback.size() == 1);
    CHECK(playback.front().count == recorded.front().count);
    CHECK(&*playback.front().loc == &*recorded.front().loc);
    CHECK_THROWS_AS(replay::next_input_event(), replay::completed);
    replay::finish();
}
TEST_CASE(
    "inventory semantic ids reject filtered removed and denied entries",
    "[client][interaction][inventory][mcp]") {
    clear_all_state();
    const auto cleanup = on_out_of_scope([]() { clear_all_state(); });
    const auto guard = interaction_test_guard{};
    auto& you = get_avatar();

    const auto stale = [](const std::string& id) {
        return engine_client::resolve_invoke(
            stack_state(), {.affordance_id = "list:inventory:choose", .args = {.row = id}});
    };

    SECTION("filter removes the rows it hides and keeps the ids of the rest") {
        auto bandages = item::spawn("bandages");
        auto heroin = item::spawn("heroin");
        auto selector = inventory_pick_selector(you);
        selector.add_item(selector.own_inv_column, bandages.get());
        selector.add_item(selector.own_inv_column, heroin.get());
        auto kept = std::string{};
        game_client::memory::set_input_provider([&](const int /*timeout*/) {
            const auto before = selector_rows();
            REQUIRE(before.size() == 2);
            const auto bandage_row = row_of(before, "bandages");
            const auto old_id = row_of(before, "heroin")["id"].get<std::string>();
            REQUIRE(bandage_row.is_object());
            kept = bandage_row["id"];
            selector.set_filter("bandage");
            REQUIRE_FALSE(stale(old_id));
            const auto after = selector_rows();
            REQUIRE(after.size() == 1);
            CHECK(after.front()["id"].get<std::string>() == kept);
            return choose_item(after.front());
        });
        CHECK(selector.execute() == bandages.get());
    }

    SECTION("a row is named by what it is, so adding another leaves its id as it was") {
        auto heroin = item::spawn("heroin");
        auto bandages = item::spawn("bandages");
        auto selector = inventory_pick_selector(you);
        selector.add_item(selector.own_inv_column, heroin.get());
        game_client::memory::set_input_provider([&](const int /*timeout*/) {
            const auto before = selector_rows();
            REQUIRE(before.size() == 1);
            selector.add_item(selector.own_inv_column, bandages.get());
            selector.set_filter(selector.get_filter());
            const auto after = selector_rows();
            REQUIRE(after.size() == 2);
            CHECK(row_of(after, "heroin")["id"] == before.front()["id"]);
            REQUIRE(stale(before.front()["id"].get<std::string>()));
            return resolve_action("QUIT");
        });
        CHECK(selector.execute() == nullptr);
    }

    SECTION("removal invalidates the old opaque id") {
        auto bandages = item::spawn("bandages");
        auto selector = inventory_pick_selector(you);
        selector.add_item(selector.own_inv_column, bandages.get());
        game_client::memory::set_input_provider([&](const int /*timeout*/) {
            const auto before = selector_rows();
            REQUIRE(before.size() == 1);
            selector.remove_item(bandages.get());
            REQUIRE_FALSE(stale(before.front()["id"].get<std::string>()));
            return resolve_action("QUIT");
        });
        CHECK(selector.execute() == nullptr);
    }

    SECTION("denial is published and cannot be selected") {
        auto bandages = item::spawn("bandages");
        const auto preset = denied_inventory_preset{};
        auto selector = inventory_pick_selector(you, preset);
        selector.add_item(selector.own_inv_column, bandages.get());
        auto reads = 0;
        game_client::memory::set_input_provider([&](const int /*timeout*/) {
            const auto rows = selector_rows();
            REQUIRE(rows.size() == 1);
            CHECK(rows.front()["selectable"] == false);
            CHECK(rows.front()["denial"] == "Denied by test preset");
            if (reads++ == 0) { return choose_item(rows.front()); }
            return resolve_action("QUIT");
        });
        CHECK(selector.execute() == nullptr);
        CHECK(reads == 2);
    }
}
TEST_CASE(
    "inventory count popup restores the outer structured selector",
    "[client][interaction][inventory][mcp]") {
    clear_all_state();
    const auto cleanup = on_out_of_scope([]() { clear_all_state(); });
    const auto guard = interaction_test_guard{};
    auto& you = get_avatar();
    you.i_add(item::spawn("test_platinum_bit", calendar::turn, 7));
    auto selector = inventory_drop_selector(you);
    selector.add_character_items(you);
    auto reads = 0;
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto snapshot = game_client::current_interaction();
        if (reads++ == 0) {
            REQUIRE(selector_rows().size() == 1);
            auto input = input_event('2', input_event_t::keyboard);
            return input;
        }
        if (reads == 2) {
            REQUIRE(snapshot.kind == game_client::interaction_kind::field);
            return resolve({
                .input_id = snapshot.input_id,
                .operation = game_client::interaction_operation::fill,
                .target_id = snapshot.field->id,
                .value = "2",
                .submit = true,
            });
        }
        REQUIRE(selector_rows().size() == 1);
        CHECK(selector_rows().front()["selected_count"] == 2);
        return resolve_action("CONFIRM");
    });
    const auto dropped = selector.execute();
    REQUIRE(dropped.size() == 1);
    CHECK(dropped.front().count == 2);
}
TEST_CASE(
    "real target UI moves the cursor by invoke and holds a move beyond the range at its end",
    "[client][interaction][target][mcp]") {
    clear_all_state();
    const auto cleanup = on_out_of_scope([]() { clear_all_state(); });
    const auto guard = interaction_test_guard{};
    build_test_map(ter_id("t_floor"));
    auto& you = get_avatar();
    const auto source = you.bub_pos();
    const auto visible = source + tripoint(2, 0, 0);
    const auto barrier = source + tripoint(4, 0, 0);
    const auto hidden = source + tripoint(5, 0, 0);
    spawn_test_monster("mon_zombie", visible);
    spawn_test_monster("mon_zombie", hidden);
    REQUIRE(get_map().ter_set(barrier, ter_id("t_wall")));
    get_map().invalidate_map_cache(0);
    get_map().build_map_cache(0, true);
    get_map().update_visibility_cache(0);
    REQUIRE(you.sees(visible));
    REQUIRE_FALSE(you.sees(hidden));
    auto rock = item::spawn("rock");
    auto reads = 0;
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto target = cursor_target();
        const auto range = target["range"].get<int>();
        const auto step = reads++;
        if (step == 0) {
            CHECK(bubble_at(target["source"]) == source);
            // The line of fire is published square by square up to the cursor, the source left out.
            const auto line = target["trajectory"];
            REQUIRE_FALSE(line.empty());
            CHECK(bubble_at(line.back()) == bubble_at(target["current"]));
            CHECK(std::ranges::none_of(line, [&](const nlohmann::json& at) {
                return bubble_at(at) == source;
            }));
            const auto has_candidate = [&](const tripoint_bub_ms& at) {
                return std::ranges::any_of(target["candidates"], [&](const nlohmann::json& entry) {
                    return bubble_at(entry["position"]) == at;
                });
            };
            CHECK(has_candidate(visible));
            CHECK_FALSE(has_candidate(hidden));
            const auto stats = top_screen()["root"]["stats"];
            const auto reported = std::ranges::find_if(stats, [](const nlohmann::json& stat) {
                return stat["key"] == "range";
            });
            REQUIRE(reported != stats.end());
            CHECK((*reported)["max"].get<std::string>() == std::to_string(range));
            const auto [narrow, wide] = at_two_viewports();
            CHECK(narrow == wide);
            // A square beyond the range is taken, as the native mouse takes it.
            CHECK(refused_move(source + tripoint(range + 1, 0, 0)) == std::nullopt);
            return move_cursor(source + tripoint(range + 3, 0, 0));
        }
        if (step == 1) {
            // The cursor stops on the farthest square in range.
            CHECK(bubble_at(target["current"]) == source + tripoint(range, 0, 0));
            return move_cursor(source + tripoint_east);
        }
        CHECK(bubble_at(target["current"]) == source + tripoint_east);
        CHECK(target["distance"] == 1);
        REQUIRE(target["trajectory"].size() == 1);
        CHECK(bubble_at(target["trajectory"].front()) == source + tripoint_east);
        return resolve_action("FIRE");
    });

    const auto trajectory = target_handler::mode_throw(you, *rock, false);
    REQUIRE_FALSE(trajectory.empty());
    CHECK(trajectory.back() == source + tripoint_east);
}
TEST_CASE(
    "semantic targeting retains the normal neutral attack confirmation",
    "[client][interaction][target][mcp]") {
    clear_all_state();
    const auto cleanup = on_out_of_scope([]() { clear_all_state(); });
    const auto guard = interaction_test_guard{};
    build_test_map(ter_id("t_floor"));
    auto& you = get_avatar();
    const auto npc_position = you.bub_pos() + tripoint_east;
    auto& bystander = spawn_npc(npc_position, "test_talker");
    bystander.set_attitude(NPCATT_NULL);
    get_map().invalidate_map_cache(0);
    get_map().build_map_cache(0, true);
    get_map().update_visibility_cache(0);
    REQUIRE(you.sees(bystander));
    auto rock = item::spawn("rock");
    auto reads = 0;
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        if (reads++ == 0) {
            const auto target = cursor_target();
            const auto candidate =
                std::ranges::find_if(target["candidates"], [&](const nlohmann::json& entry) {
                    return bubble_at(entry["position"]) == npc_position;
                });
            REQUIRE(candidate != target["candidates"].end());
            CHECK((*candidate)["creature"] == true);
            return move_cursor(npc_position);
        }
        if (reads == 2) {
            CHECK(bubble_at(cursor_target()["current"]) == npc_position);
            return resolve_action("FIRE");
        }
        if (reads == 3) {
            const auto snapshot = game_client::current_interaction();
            REQUIRE(snapshot.kind == game_client::interaction_kind::choices);
            CHECK(snapshot.message.contains("Really attack"));
            const auto no = std::ranges::
                find(snapshot.choices, "NO", &game_client::interaction_choice::action_id);
            REQUIRE(no != snapshot.choices.end());
            return resolve({
                .input_id = snapshot.input_id,
                .operation = game_client::interaction_operation::choose,
                .target_id = no->id,
            });
        }
        // The question is answered: the target screen is the top again, and QUIT leaves it.
        CHECK(top_screen()["root"]["id"] == "target");
        return resolve_action("QUIT");
    });

    CHECK(target_handler::mode_throw(you, *rock, false).empty());
}
TEST_CASE(
    "semantic replay re-enters the same real string widget", "[client][interaction][replay][mcp]") {
    const auto guard = interaction_test_guard{};
    static auto counter = std::atomic_uint64_t{0};
    const auto directory =
        std::filesystem::path(PATH_INFO::user_dir())
        / ("semantic-replay-" + std::to_string(counter.fetch_add(1)));
    REQUIRE(std::filesystem::create_directory(directory));
    const auto cleanup = on_out_of_scope([&] {
        replay::stop();
        std::filesystem::remove_all(directory);
    });
    const auto path = directory / "input.jsonl";

    replay::configure_recording(path.string(), {.rng_seed = 17});
    replay::start();
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto snapshot = game_client::current_interaction();
        REQUIRE(snapshot.field);
        return resolve({
            .input_id = snapshot.input_id,
            .operation = game_client::interaction_operation::fill,
            .target_id = snapshot.field->id,
            .value = "recorded value",
            .submit = true,
        });
    });
    auto recorded_popup = string_input_popup{};
    CHECK(recorded_popup.title("Replay field").max_length(30).query_string() == "recorded value");
    replay::finish();

    game_client::memory::set_input_provider([](const int /*timeout*/) -> input_event {
        throw std::runtime_error("playback must not read live input");
    });
    replay::configure_playback(path.string(), {.rng_seed = 17});
    replay::start();
    auto playback_popup = string_input_popup{};
    CHECK(playback_popup.title("Replay field").max_length(30).query_string() == "recorded value");
    CHECK_THROWS_AS(replay::next_input_event(), replay::completed);
    replay::finish();
}
namespace {

class fixed_clipboard final: public game_client::render_service {
public:
    std::string text;
    auto clipboard_available() const -> bool override { return true; }
    auto clipboard_text() -> std::string override { return text; }
};

} // namespace

TEST_CASE(
    "replay inserts the recorded clipboard text instead of the live clipboard",
    "[client][replay][mcp]") {
    const auto guard = interaction_test_guard{};
    static auto counter = std::atomic_uint64_t{0};
    const auto directory =
        std::filesystem::path(PATH_INFO::user_dir())
        / ("clipboard-replay-" + std::to_string(counter.fetch_add(1)));
    REQUIRE(std::filesystem::create_directory(directory));
    const auto cleanup = on_out_of_scope([&] {
        replay::stop();
        std::filesystem::remove_all(directory);
    });
    const auto path = directory / "input.jsonl";
    auto clipboard = std::make_unique<fixed_clipboard>();
    auto& live_clipboard = *clipboard;
    const auto presentation = game_client::presentation_scope(std::move(clipboard));

    auto presses = 0;
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        return resolve_action(presses++ == 0 ? "TEXT.PASTE" : "TEXT.CONFIRM");
    });
    live_clipboard.text = "Alice";
    replay::configure_recording(path.string(), {.rng_seed = 23});
    replay::start();
    auto recorded_popup = string_input_popup{};
    CHECK(recorded_popup.title("Paste").max_length(30).query_string() == "Alice");
    replay::finish();

    live_clipboard.text = "Bob";
    game_client::memory::set_input_provider([](const int /*timeout*/) -> input_event {
        throw std::runtime_error("playback must not read live input");
    });
    replay::configure_playback(path.string(), {.rng_seed = 23});
    replay::start();
    auto playback_popup = string_input_popup{};
    CHECK(playback_popup.title("Paste").max_length(30).query_string() == "Alice");
    CHECK_THROWS_AS(replay::next_input_event(), replay::completed);
    replay::finish();
}
TEST_CASE(
    "semantic replay rejects a different real uilist in the same context",
    "[client][interaction][replay][mcp]") {
    const auto guard = interaction_test_guard{};
    static auto counter = std::atomic_uint64_t{0};
    const auto directory =
        std::filesystem::path(PATH_INFO::user_dir())
        / ("semantic-list-replay-" + std::to_string(counter.fetch_add(1)));
    REQUIRE(std::filesystem::create_directory(directory));
    const auto cleanup = on_out_of_scope([&] {
        replay::stop();
        std::filesystem::remove_all(directory);
    });
    const auto path = directory / "input.jsonl";

    replay::configure_recording(path.string(), {.rng_seed = 19});
    replay::start();
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto snapshot = game_client::current_interaction();
        return resolve({
            .input_id = snapshot.input_id,
            .operation = game_client::interaction_operation::choose,
            .target_id = snapshot.choices.front().id,
        });
    });
    {
        auto recorded = uilist{};
        recorded.entries.emplace_back(1, true, MENU_AUTOASSIGN, "Recorded choice");
        recorded.query();
    }
    replay::finish();

    replay::configure_playback(path.string(), {.rng_seed = 19});
    replay::start();
    {
        auto different = uilist{};
        different.entries.emplace_back(1, true, MENU_AUTOASSIGN, "Different choice");
        different.entries.emplace_back(2, true, MENU_AUTOASSIGN, "Another choice");
        CHECK_THROWS_WITH(different.query(), Catch::Matchers::Contains("schema changed"));
    }
}
TEST_CASE(
    "common direction choice exposes only native relative vectors",
    "[client][interaction][direction][mcp]") {
    clear_all_state();
    const auto cleanup = on_out_of_scope([]() { clear_all_state(); });
    const auto guard = interaction_test_guard{};

    const auto choose = [&](const std::string& label, const bool allow_vertical) {
        game_client::memory::set_input_provider([&](const int /*timeout*/) {
            const auto snapshot = game_client::current_interaction();
            REQUIRE(snapshot.context == "DEFAULTMODE");
            REQUIRE(snapshot.kind == game_client::interaction_kind::choices);
            CHECK(snapshot.allow_cancel);
            CHECK(snapshot.choices.size() == (allow_vertical ? 11 : 9));
            const auto choice =
                std::ranges::find(snapshot.choices, label, &game_client::interaction_choice::label);
            REQUIRE(choice != snapshot.choices.end());
            return resolve({
                .input_id = snapshot.input_id,
                .operation = game_client::interaction_operation::choose,
                .target_id = choice->id,
            });
        });
        return choose_direction("Test direction", allow_vertical);
    };

    const auto cases = std::vector<std::pair<std::string, tripoint_rel_ms>>{
        {"North", tripoint_rel_ms::north()}, {"North East", tripoint_rel_ms::north_east()},
        {"East", tripoint_rel_ms::east()},   {"South East", tripoint_rel_ms::south_east()},
        {"South", tripoint_rel_ms::south()}, {"South West", tripoint_rel_ms::south_west()},
        {"West", tripoint_rel_ms::west()},   {"North West", tripoint_rel_ms::north_west()},
        {"Here", tripoint_rel_ms::zero()},   {"Above", tripoint_rel_ms::above()},
        {"Below", tripoint_rel_ms::below()},
    };
    for (const auto& [label, expected] : cases) {
        const auto selected = choose(label, expected.z() != 0);
        REQUIRE(selected);
        CHECK(*selected == expected);
    }
    CHECK(get_avatar().facing == FD_LEFT);

    auto stale_error = std::string{};
    auto absent_error = std::string{};
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto snapshot = game_client::current_interaction();
        CHECK(std::ranges::none_of(snapshot.choices, [](const auto& choice) {
            return choice.label == "Above";
        }));
        const auto stale = game_client::resolve_interaction_command({
            .input_id = snapshot.input_id + 1,
            .operation = game_client::interaction_operation::choose,
            .target_id = snapshot.choices.front().id,
        });
        REQUIRE_FALSE(stale);
        stale_error = stale.error();
        const auto absent = game_client::resolve_interaction_command({
            .input_id = snapshot.input_id,
            .operation = game_client::interaction_operation::choose,
            .target_id = game_client::opaque_interaction_id("direction", {"0", "0", "1"}),
        });
        REQUIRE_FALSE(absent);
        absent_error = absent.error();
        return resolve({
            .input_id = snapshot.input_id,
            .operation = game_client::interaction_operation::cancel,
        });
    });
    CHECK_FALSE(choose_direction("Cancel direction"));
    CHECK(stale_error.starts_with("stale:"));
    CHECK(absent_error.contains("not in the active interaction"));
}
namespace {

/// What the construction tests restore: the menu state that persists between openings.
struct construction_state_guard {
    decltype(uistate.construction_filter) filter = uistate.construction_filter;
    decltype(uistate.construction_tab) tab = uistate.construction_tab;
    decltype(uistate.last_construction) last = uistate.last_construction;
    ~construction_state_guard() {
        uistate.construction_filter = filter;
        uistate.construction_tab = tab;
        uistate.last_construction = last;
    }
};

auto construct_row(const std::string& affordance, const std::string& row) -> input_event {
    return invoke_event(affordance, {.row = row});
}

/// Lights the test map and gives the avatar the materials of the semantic construction.
auto prepare_semantic_construction() -> tripoint_bub_ms {
    calendar::turn = calendar::turn_zero + 12_hours;
    g->reset_light_level();
    build_test_map(ter_id("t_floor"));
    auto& you = get_avatar();
    add_crafting_fixture_item(you, itype_id("e_scrap"), 2);
    const auto destination = you.bub_pos() + tripoint_east;
    clear_construction_fixture(destination);
    get_map().invalidate_map_cache(you.bub_pos().z());
    get_map().build_map_cache(you.bub_pos().z());
    get_map().update_visibility_cache(you.bub_pos().z());
    REQUIRE(character_funcs::can_see_fine_details(you));
    return destination;
}

/// Answers the native direction prompt that follows a construction: East.
auto choose_east(const game_client::interaction_snapshot& snapshot) -> input_event {
    const auto east =
        std::ranges::find(snapshot.choices, "East", &game_client::interaction_choice::label);
    REQUIRE(east != snapshot.choices.end());
    return resolve({
        .input_id = snapshot.input_id,
        .operation = game_client::interaction_operation::choose,
        .target_id = east->id,
    });
}

} // namespace

TEST_CASE(
    "the construction screen is published by group identity with its requirements",
    "[construction][engine_client_component][mcp]") {
    clear_all_state();
    const auto cleanup = on_out_of_scope([]() { clear_all_state(); });
    const auto guard = interaction_test_guard{};
    const auto state = construction_state_guard{};
    uistate.construction_filter.clear();
    build_test_map(ter_id("t_floor"));
    add_crafting_fixture_item(get_avatar(), itype_id("e_scrap"), 2);
    get_avatar().invalidate_crafting_inventory();
    const auto& con = construction_str_id("constr_test_semantic_construction").obj();

    auto step = 0;
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto screen = current_screen();
        REQUIRE(screen.context == "CONSTRUCTION");
        switch (step++) {
            case 0: {
                CHECK(screen.role == "screen");
                CHECK(screen.focus_component == "construction:groups");
                const auto tabs = tab_ids_of(screen, "construction:tabs");
                CHECK(tabs.size() > 2);
                REQUIRE(std::ranges::contains(tabs, "OTHER"));
                const auto ids = std::set<std::string>{
                    screen.affordances | std::views::transform(&engine_client::affordance::id)
                    | std::ranges::to<std::set>()};
                for (const auto* id :
                     {"construct:construct", "construct:favorite", "construct:search",
                      "construct:tab", "construct:select"}) {
                    CHECK(ids.contains(id));
                }
                return construct_row("construct:tab", "OTHER");
            }
            case 1: {
                const auto rows = rows_of(screen, "construction:groups");
                const auto mine = row_with(rows, "TEST semantic construction");
                // A group is its own id, not its words.
                CHECK(mine.id == con.group.str());
                CHECK(mine.look.has_value());
                CHECK(mine.enabled);
                return invoke_event("construct:search", {.text = "TEST semantic construction"});
            }
            case 2: {
                const auto rows = rows_of(screen, "construction:groups");
                REQUIRE_FALSE(rows.empty());
                CHECK(
                    std::get<engine_client::list_body>(
                        component_of(screen, "construction:groups").body)
                        .filter
                    == "TEST semantic construction");
                return construct_row("construct:select", con.group.str());
            }
            case 3: {
                CHECK(screen.focus_row == con.group.str());
                const auto stage = "construction:stage:" + con.id.str();
                REQUIRE_FALSE(rows_of(screen, stage + ":components").empty());
                const auto select = std::ranges::
                    find(screen.affordances, "construct:select", &engine_client::affordance::id);
                REQUIRE(select != screen.affordances.end());
                CHECK(select->role == "select");
                return invoke_event("construct:search", {.text = "no such construction"});
            }
            case 4: {
                // A search replaces the category tabs by one, as natively.
                CHECK(rows_of(screen, "construction:groups").empty());
                CHECK(tab_ids_of(screen, "construction:tabs").size() == 1);
                CHECK(std::ranges::none_of(screen.affordances, [](const auto& affordance) {
                    return affordance.id == "construct:tab";
                }));
                return invoke_event("construct:search", {.text = ""});
            }
            default: {
                CHECK(tab_ids_of(screen, "construction:tabs").size() > 2);
                return resolve_action("QUIT");
            }
        }
    });
    CHECK_FALSE(construction_menu(false));
    CHECK(step == 6);
}

TEST_CASE(
    "constructing through the screen keeps the native placement and resource handling",
    "[construction][engine_client_component][mcp]") {
    clear_all_state();
    const auto cleanup = on_out_of_scope([]() { clear_all_state(); });
    const auto guard = interaction_test_guard{};
    const auto restore_turn = restore_on_out_of_scope<decltype(calendar::turn)>(calendar::turn);
    const auto state = construction_state_guard{};
    uistate.construction_filter.clear();
    const auto destination = prepare_semantic_construction();
    const auto cleanup_construction = on_out_of_scope(
        std::bind_front(clear_construction_fixture, destination));
    const auto& con = construction_str_id("constr_test_semantic_construction").obj();
    auto& you = get_avatar();

    auto step = 0;
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto snapshot = game_client::current_interaction();
        if (snapshot.context == "CONSTRUCTION") {
            const auto screen = current_screen();
            if (step++ == 0) {
                return invoke_event("construct:search", {.text = "TEST semantic construction"});
            }
            const auto mine =
                row_with(rows_of(screen, "construction:groups"), "TEST semantic construction");
            CHECK(mine.enabled);
            CHECK(mine.selectable);
            return construct_row("construct:construct", mine.id);
        }
        REQUIRE(snapshot.context == "DEFAULTMODE");
        step = 10;
        return choose_east(snapshot);
    });
    CHECK_FALSE(construction_menu(false));
    CHECK(step == 10);
    CHECK(you.amount_of(itype_id("e_scrap")) == 0);
    CHECK(get_map().partial_con_at(destination) != nullptr);
    REQUIRE(you.activity);
    CHECK(you.activity->id() == activity_id("ACT_BUILD"));
    finish_construction_activity(you);
    CHECK_FALSE(you.activity);
    CHECK(get_map().partial_con_at(destination) == nullptr);
    CHECK(get_map().furn(destination) == furn_id("f_test_semantic_construction"));
}

TEST_CASE(
    "constructing in the dark through the screen retains the resources",
    "[construction][engine_client_component][mcp]") {
    const auto restore_light = on_out_of_scope([]() {
        g->reset_light_level();
        get_map().invalidate_map_cache(get_avatar().bub_pos().z());
    });
    const auto restore_turn = restore_on_out_of_scope<decltype(calendar::turn)>(calendar::turn);
    clear_all_state();
    const auto cleanup = on_out_of_scope([]() { clear_all_state(); });
    const auto guard = interaction_test_guard{};
    const auto state = construction_state_guard{};
    calendar::turn = calendar::turn_zero;
    g->reset_light_level();
    uistate.construction_filter = "TEST semantic construction";
    build_test_map(ter_id("t_floor"));
    auto& you = get_avatar();
    const auto destination = you.bub_pos() + tripoint_east;
    clear_construction_fixture(destination);
    const auto cleanup_construction = on_out_of_scope(
        std::bind_front(clear_construction_fixture, destination));
    add_crafting_fixture_item(you, itype_id("e_scrap"), 2);
    get_map().invalidate_map_cache(you.bub_pos().z());
    get_map().build_map_cache(you.bub_pos().z());
    get_map().update_visibility_cache(you.bub_pos().z());
    REQUIRE_FALSE(character_funcs::can_see_fine_details(you));
    REQUIRE_FALSE(you.has_trait(trait_id("DEBUG_HS")));
    const auto& con = construction_str_id("constr_test_semantic_construction").obj();
    REQUIRE_FALSE(con.dark_craftable);
    REQUIRE(player_can_build(you, you.crafting_inventory(), con));
    const auto materials_before = you.charges_of(itype_id("e_scrap"));
    auto reads = 0;
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        ++reads;
        const auto screen = current_screen();
        REQUIRE(screen.context == "CONSTRUCTION");
        REQUIRE(reads == 1);
        const auto mine =
            row_with(rows_of(screen, "construction:groups"), "TEST semantic construction");
        REQUIRE(mine.enabled);
        return construct_row("construct:construct", mine.id);
    });
    CHECK_FALSE(construction_menu(false));
    CHECK(reads == 1);
    CHECK(you.charges_of(itype_id("e_scrap")) == materials_before);
    CHECK_FALSE(you.activity);
    CHECK(get_map().partial_con_at(destination) == nullptr);
    CHECK(get_map().furn(destination) == furn_id("f_null"));
}

TEST_CASE(
    "the construction screen denies what the crafter cannot build and a blueprint ignores materials",
    "[construction][engine_client_component][mcp]") {
    clear_all_state();
    const auto cleanup = on_out_of_scope([]() { clear_all_state(); });
    const auto guard = interaction_test_guard{};
    const auto state = construction_state_guard{};
    uistate.construction_filter.clear();
    build_test_map(ter_id("t_floor"));
    auto& you = get_avatar();
    const auto material_before = you.amount_of(itype_id("test_1kg_cube"));

    SECTION("native denial") {
        auto step = 0;
        game_client::memory::set_input_provider([&](const int /*timeout*/) {
            const auto snapshot = game_client::current_interaction();
            if (snapshot.context != "CONSTRUCTION") {
                // The native denial is a popup of its own; closing it returns to the screen.
                CHECK(snapshot.message.contains("can't build"));
                REQUIRE(step++ == 2);
                return resolve({
                    .input_id = snapshot.input_id,
                    .operation = game_client::interaction_operation::choose,
                    .target_id = snapshot.choices.front().id,
                });
            }
            const auto screen = current_screen();
            switch (step++) {
                case 0:
                    return invoke_event(
                        "construct:search", {.text = "TEST semantic unavailable construction"});
                case 1: {
                    if (rows_of(screen, "construction:groups").empty()) {
                        --step;
                        return resolve_action("TOGGLE_UNAVAILABLE_CONSTRUCTIONS");
                    }
                    const auto rows = rows_of(screen, "construction:groups");
                    REQUIRE(rows.size() == 1);
                    CHECK_FALSE(rows.front().enabled);
                    CHECK(rows.front().selectable);
                    CHECK_FALSE(rows.front().denial.empty());
                    return construct_row("construct:construct", rows.front().id);
                }
                default:
                    return resolve_action("QUIT");
            }
        });
        CHECK_FALSE(construction_menu(false));
        CHECK(step == 4);
        CHECK_FALSE(you.activity);
        CHECK(you.amount_of(itype_id("test_1kg_cube")) == material_before);
    }

    SECTION("blueprint ignores material availability") {
        uistate.construction_filter = "TEST semantic unavailable construction";
        game_client::memory::set_input_provider([&](const int /*timeout*/) {
            const auto screen = current_screen();
            const auto rows = rows_of(screen, "construction:groups");
            if (rows.empty()) { return resolve_action("TOGGLE_UNAVAILABLE_CONSTRUCTIONS"); }
            REQUIRE(rows.size() == 1);
            CHECK(rows.front().enabled);
            return construct_row("construct:construct", rows.front().id);
        });
        const auto result = construction_menu(true);
        REQUIRE(result);
        CHECK(result->obj().id
              == construction_str_id("constr_test_semantic_construction_unavailable"));
        CHECK_FALSE(you.activity);
        CHECK(you.amount_of(itype_id("test_1kg_cube")) == material_before);
    }
}

TEST_CASE(
    "the construction screen ignores a stale or unknown row and an unknown tab",
    "[construction][engine_client_component][mcp]") {
    clear_all_state();
    const auto cleanup = on_out_of_scope([]() { clear_all_state(); });
    const auto guard = interaction_test_guard{};
    const auto state = construction_state_guard{};
    uistate.construction_filter.clear();
    build_test_map(ter_id("t_floor"));
    auto before = nlohmann::json{};
    auto step = 0;
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto screen = current_screen();
        REQUIRE(screen.context == "CONSTRUCTION");
        switch (step++) {
            case 0: {
                before = screen_wire();
                return construct_row("construct:construct", "no_such_group");
            }
            case 1:
                return construct_row("construct:favorite", "no_such_group");
            case 2:
                return construct_row("construct:tab", "NO_SUCH_TAB");
            case 3:
                return invoke_event("construct:search");
            case 4:
                return invoke_event("construct:no_such_affordance", {.row = "no_such_group"});
            default: {
                CHECK(screen_wire().at("root") == before.at("root"));
                return resolve_action("QUIT");
            }
        }
    });
    CHECK_FALSE(construction_menu(false));
    CHECK(step == 6);
    CHECK_FALSE(get_avatar().activity);
}

TEST_CASE(
    "constructing through the screen replays from the recorded invokes",
    "[construction][engine_client_component][replay][mcp]") {
    clear_all_state();
    const auto cleanup_state = on_out_of_scope([]() { clear_all_state(); });
    const auto guard = interaction_test_guard{};
    const auto restore_turn = restore_on_out_of_scope<decltype(calendar::turn)>(calendar::turn);
    const auto state = construction_state_guard{};
    static auto counter = std::atomic_uint64_t{0};
    const auto directory =
        std::filesystem::path(PATH_INFO::user_dir())
        / ("semantic-construction-replay-" + std::to_string(counter.fetch_add(1)));
    REQUIRE(std::filesystem::create_directory(directory));
    const auto cleanup_replay = on_out_of_scope([&] {
        replay::stop();
        std::filesystem::remove_all(directory);
    });
    const auto path = directory / "input.jsonl";
    const auto& con = construction_str_id("constr_test_semantic_construction").obj();

    auto reads = 0;
    const auto semantic_provider = [&](const int /*timeout*/) {
        ++reads;
        const auto snapshot = game_client::current_interaction();
        if (snapshot.context == "CONSTRUCTION") {
            return construct_row("construct:construct", con.group.str());
        }
        CAPTURE(snapshot.title, snapshot.message, reads);
        REQUIRE(snapshot.context == "DEFAULTMODE");
        return choose_east(snapshot);
    };
    uistate.construction_filter = "TEST semantic construction";
    const auto recorded_destination = prepare_semantic_construction();
    const auto cleanup_construction = on_out_of_scope(
        std::bind_front(clear_construction_fixture, recorded_destination));
    replay::configure_recording(path.string(), {.rng_seed = 37});
    replay::start();
    game_client::memory::set_input_provider(semantic_provider);
    CHECK_FALSE(construction_menu(false));
    CHECK(reads == 2);
    REQUIRE(get_avatar().activity);
    CHECK(get_avatar().activity->id() == activity_id("ACT_BUILD"));
    CHECK(get_avatar().amount_of(itype_id("e_scrap")) == 0);
    finish_construction_activity(get_avatar());
    CHECK(get_map().furn(recorded_destination) == furn_id("f_test_semantic_construction"));
    replay::finish();

    // Normalising the avatar may ask whether to eat while full; the recorded provider must not
    // answer that.
    game_client::memory::set_input_provider([](const int /*timeout*/) {
        return resolve_action("YES");
    });
    clear_all_state();
    uistate.construction_filter = "TEST semantic construction";
    const auto played_destination = prepare_semantic_construction();
    game_client::memory::set_input_provider([](const int /*timeout*/) -> input_event {
        throw std::runtime_error("construction playback must not read live input");
    });
    replay::configure_playback(path.string(), {.rng_seed = 37});
    replay::start();
    CHECK_FALSE(construction_menu(false));
    REQUIRE(get_avatar().activity);
    CHECK(get_avatar().activity->id() == activity_id("ACT_BUILD"));
    CHECK(get_avatar().amount_of(itype_id("e_scrap")) == 0);
    finish_construction_activity(get_avatar());
    CHECK(get_map().partial_con_at(played_destination) == nullptr);
    CHECK(get_map().furn(played_destination) == furn_id("f_test_semantic_construction"));
    CHECK_THROWS_AS(replay::next_input_event(), replay::completed);
    replay::finish();
}

namespace {

/// What the crafting tests restore: the lists the screen reads and writes.
struct crafting_state_guard {
    decltype(uistate.read_recipes) read = uistate.read_recipes;
    decltype(uistate.expanded_recipes) expanded = uistate.expanded_recipes;
    decltype(uistate.hidden_recipes) hidden = uistate.hidden_recipes;
    decltype(uistate.favorite_recipes) favorite = uistate.favorite_recipes;
    ~crafting_state_guard() {
        uistate.read_recipes = read;
        uistate.expanded_recipes = expanded;
        uistate.hidden_recipes = hidden;
        uistate.favorite_recipes = favorite;
    }
};

/// The input that searches the crafting screen for `text`.
auto craft_search(const std::string& text) -> input_event {
    return invoke_event("craft:search", {.text = text});
}

auto craft_row(const std::string& affordance, const std::string& row) -> input_event {
    return invoke_event(affordance, {.row = row});
}

auto craft_batch(const std::string& row, const std::int64_t count) -> input_event {
    return invoke_event("craft:batch", {.row = row, .count = count});
}

} // namespace

TEST_CASE(
    "the crafting screen is published by recipe identity with its tabs and affordances",
    "[crafting][engine_client_component][mcp]") {
    clear_all_state();
    const auto cleanup = on_out_of_scope([]() { clear_all_state(); });
    const auto guard = interaction_test_guard{};
    const auto state = crafting_state_guard{};
    prepare_crafting_fixture();
    const auto& iron = recipe_id("test_soldering_iron").obj();

    auto step = 0;
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto screen = current_screen();
        REQUIRE(screen.context == "CRAFTING");
        const auto at = step++;
        if (at == 0) {
            CHECK(screen.role == "screen");
            CHECK(screen.focus_component == "crafting:recipes");
            const auto tabs = tab_ids_of(screen, "crafting:tabs");
            CHECK(tabs.size() > 1);
            CHECK(std::ranges::contains(tabs, "CC_ELECTRONIC"));
            CHECK_FALSE(tab_ids_of(screen, "crafting:subtabs").empty());
            CHECK(std::ranges::contains(
                screen.affordances, "craft:subtab", &engine_client::affordance::id));
            return craft_search("TEST");
        }
        if (at == 1) {
            const auto rows = rows_of(screen, "crafting:recipes");
            REQUIRE_FALSE(rows.empty());
            // A row is its recipe, never its position or its words.
            for (const auto& row : rows) {
                CAPTURE(row.id);
                CHECK(recipe_id(row.id.substr(0, row.id.find('#'))).is_valid());
            }
            CHECK(std::ranges::count_if(rows, &engine_client::row::selected) == 1);
            const auto ids = std::set<std::string>{
                screen.affordances | std::views::transform(&engine_client::affordance::id)
                | std::ranges::to<std::set>()};
            for (const auto* id :
                 {"craft:craft", "craft:batch", "craft:tab", "craft:search", "craft:favorite",
                  "craft:hide", "craft:select"}) {
                CHECK(ids.contains(id));
            }
            // The search is the registered filter, which is not offered beside it.
            CHECK_FALSE(ids.contains("FILTER"));
            return craft_search(iron.result_name());
        }
        const auto rows = rows_of(screen, "crafting:recipes");
        REQUIRE(rows.size() == 1);
        CHECK(rows.front().id == iron.ident().str());
        CHECK(rows.front().enabled);
        CHECK(rows.front().minimum_count == 1);
        CHECK(rows.front().max == 50);
        CHECK(rows.front().look.has_value());
        const auto available = std::ranges::
            find(rows.front().columns, "availability", &game_client::interaction_column::key);
        REQUIRE(available != rows.front().columns.end());
        CHECK(available->value == "Ready");
        const auto select =
            std::ranges::find(screen.affordances, "craft:select", &engine_client::affordance::id);
        REQUIRE(select != screen.affordances.end());
        CHECK(select->role == "select");
        CHECK(screen.focus_row == iron.ident().str());
        // A search replaces the tabs by one, as natively, and freezes the subcategories.
        CHECK(tab_ids_of(screen, "crafting:tabs") == std::vector<std::string>{"search"});
        CHECK(std::ranges::none_of(screen.affordances, [](const auto& affordance) {
            return affordance.id == "craft:subtab";
        }));
        return resolve_action("QUIT");
    });
    get_avatar().craft(get_avatar().bub_pos());
    CHECK(step == 3);
}

TEST_CASE(
    "a survivor with no favorite or recent recipe opens crafting on the recipes they know",
    "[crafting][engine_client_component][mcp]") {
    clear_all_state();
    const auto cleanup = on_out_of_scope([]() { clear_all_state(); });
    const auto guard = interaction_test_guard{};
    const auto state = crafting_state_guard{};
    const auto recent = restore_on_out_of_scope<decltype(uistate.recent_recipes)>(
        uistate.recent_recipes);
    uistate.favorite_recipes.clear();
    uistate.recent_recipes.clear();
    prepare_crafting_fixture();

    auto step = 0;
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto screen = current_screen();
        REQUIRE(screen.context == "CRAFTING");
        ++step;
        CHECK_FALSE(rows_of(screen, "crafting:recipes").empty());
        return resolve_action("QUIT");
    });
    get_avatar().craft(get_avatar().bub_pos());
    CHECK(step == 1);
}

TEST_CASE(
    "selecting a crafting row moves the cursor and its detail without crafting",
    "[crafting][engine_client_component][mcp]") {
    clear_all_state();
    const auto cleanup = on_out_of_scope([]() { clear_all_state(); });
    const auto guard = interaction_test_guard{};
    const auto highlight_unread = override_option("HIGHLIGHT_UNREAD_RECIPES", "true");
    const auto state = crafting_state_guard{};
    uistate.read_recipes.clear();
    auto& you = get_avatar();
    prepare_crafting_fixture();
    const auto scrap_before = you.amount_of(itype_id("e_scrap"));
    const auto detail_title = [](const engine_client::screen_view& screen) {
        const auto& panel = component_of(screen, "crafting:detail");
        REQUIRE(std::holds_alternative<engine_client::panel_body>(panel.body));
        return std::get<engine_client::panel_body>(panel.body).title;
    };

    auto step = 0;
    auto first_title = std::string{};
    auto last_id = std::string{};
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto screen = current_screen();
        REQUIRE(screen.context == "CRAFTING");
        switch (step++) {
            case 0:
                return craft_search("TEST");
            case 1: {
                const auto rows = rows_of(screen, "crafting:recipes");
                REQUIRE(rows.size() == 2);
                // A tab with something new in it says so as data.
                const auto& tabs = std::get<engine_client::tabs_body>(
                    component_of(screen, "crafting:tabs").body);
                CHECK(std::ranges::any_of(tabs.tabs, &engine_client::tab_entry::unread));
                first_title = detail_title(screen);
                last_id = rows.back().id;
                CHECK(rows.back().id != screen.focus_row);
                return craft_row("craft:select", last_id);
            }
            case 2: {
                CHECK(screen.focus_row == last_id);
                CHECK(detail_title(screen) != first_title);
                // Selecting a row that is gone changes nothing.
                return craft_row("craft:select", "no_such_recipe");
            }
            default: {
                CHECK(screen.focus_row == last_id);
                return resolve_action("QUIT");
            }
        }
    });
    you.craft(you.bub_pos());
    CHECK(step == 4);
    CHECK_FALSE(you.activity);
    CHECK(you.amount_of(itype_id("e_scrap")) == scrap_before);
}

TEST_CASE(
    "the crafting detail tells what the crafter has and lacks",
    "[crafting][engine_client_component][mcp]") {
    clear_all_state();
    const auto cleanup = on_out_of_scope([]() { clear_all_state(); });
    const auto guard = interaction_test_guard{};
    const auto state = crafting_state_guard{};
    const auto& iron = recipe_id("test_soldering_iron").obj();
    get_avatar().learn_recipe(&iron);
    get_avatar().set_skill_level(iron.skill_used, 2);

    const auto enabled_count = [](const engine_client::screen_view& screen, const std::string& id) {
        const auto rows = rows_of(screen, id);
        return std::pair{std::ranges::count_if(rows, &engine_client::row::enabled), rows.size()};
    };
    auto step = 0;
    auto lacking = std::size_t{0};
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto screen = current_screen();
        REQUIRE(screen.context == "CRAFTING");
        switch (step++) {
            case 0:
                return resolve_action("TOGGLE_UNAVAILABLE");
            case 1:
                return craft_search(iron.result_name());
            case 2: {
                const auto rows = rows_of(screen, "crafting:recipes");
                REQUIRE(rows.size() == 1);
                CHECK_FALSE(rows.front().enabled);
                CHECK_FALSE(rows.front().denial.empty());
                const auto [have, total] = enabled_count(screen, "requirements:components");
                CHECK(total > 0);
                CHECK(have == 0);
                lacking = total;
                // The tools are listed beside the components, none of them held yet.
                CHECK(enabled_count(screen, "requirements:tools").first == 0);
                add_crafting_fixture_item(get_avatar(), itype_id("e_scrap"), 4);
                add_crafting_fixture_item(get_avatar(), itype_id("copper"), 2);
                add_crafting_fixture_item(get_avatar(), itype_id("scrap"), 2);
                add_crafting_fixture_item(get_avatar(), itype_id("duct_tape"), 20);
                add_crafting_fixture_item(get_avatar(), itype_id("cable"), 10);
                get_avatar().invalidate_crafting_inventory();
                return craft_search(iron.result_name());
            }
            default: {
                const auto [have, total] = enabled_count(screen, "requirements:components");
                CHECK(total == lacking);
                CHECK(have > 0);
                return resolve_action("QUIT");
            }
        }
    });
    get_avatar().craft(get_avatar().bub_pos());
    CHECK(step == 4);
}

TEST_CASE(
    "the crafting screen reads the same at any viewport size",
    "[crafting][engine_client_component][mcp]") {
    clear_all_state();
    const auto cleanup = on_out_of_scope([]() { clear_all_state(); });
    const auto guard = interaction_test_guard{};
    const auto state = crafting_state_guard{};
    prepare_crafting_fixture();

    const auto top_json = screen_wire;
    auto step = 0;
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        if (step++ == 0) { return craft_search("TEST"); }
        const auto wide = top_json();
        TERMX = 200;
        TERMY = 60;
        const auto large = top_json();
        TERMX = 80;
        TERMY = 24;
        const auto small = top_json();
        CHECK(wide.at("root") == large.at("root"));
        CHECK(wide.at("root") == small.at("root"));
        CHECK(wide.at("affordances") == small.at("affordances"));
        return resolve_action("QUIT");
    });
    get_avatar().craft(get_avatar().bub_pos());
    CHECK(step == 2);
}

TEST_CASE(
    "invoking the crafting screen starts a searched batch",
    "[crafting][engine_client_component][mcp]") {
    clear_all_state();
    const auto cleanup = on_out_of_scope([]() { clear_all_state(); });
    const auto guard = interaction_test_guard{};
    const auto state = crafting_state_guard{};
    auto& you = get_avatar();
    const auto& iron = recipe_id("test_soldering_iron").obj();
    prepare_crafting_fixture();

    auto step = 0;
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto screen = current_screen();
        REQUIRE(screen.context == "CRAFTING");
        switch (step++) {
            case 0:
                return craft_search(iron.result_name());
            case 1:
                return craft_batch(iron.ident().str(), 2);
            default: {
                // The batch list holds the sizes, the cursor on the one asked for.
                CHECK(tab_ids_of(screen, "crafting:tabs") == std::vector<std::string>{"batch"});
                const auto rows = rows_of(screen, "crafting:recipes");
                CHECK(rows.size() == 50);
                CHECK(rows.front().id == "1");
                CHECK(screen.focus_row == "2");
                return craft_row("craft:craft", "2");
            }
        }
    });
    you.craft(you.bub_pos());
    REQUIRE(you.activity);
    REQUIRE(you.activity->id() == activity_id("ACT_CRAFT"));
    CHECK(step == 3);
    CHECK(you.amount_of(itype_id("e_scrap")) == 0);
    CHECK(you.amount_of(itype_id("copper")) == 0);
    CHECK(you.amount_of(itype_id("scrap")) == 0);
    CHECK(you.charges_of(itype_id("duct_tape")) == 0);
    CHECK(you.amount_of(itype_id("cable")) == 0);
    finish_crafting_activity(you);
    REQUIRE_FALSE(you.activity);
    you.invalidate_crafting_inventory();
    CHECK(you.amount_of(itype_id("test_soldering_iron")) == 2);
}

TEST_CASE(
    "a batch entered through the crafting screen keeps the native bookkeeping",
    "[crafting][engine_client_component][mcp]") {
    clear_all_state();
    const auto cleanup = on_out_of_scope([]() { clear_all_state(); });
    const auto guard = interaction_test_guard{};
    const auto highlight_unread = override_option("HIGHLIGHT_UNREAD_RECIPES", "true");
    const auto state = crafting_state_guard{};
    uistate.read_recipes.clear();
    const auto& pipe = recipe_id("test_pipe").obj();
    prepare_crafting_fixture();

    auto step = 0;
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto screen = current_screen();
        REQUIRE(screen.context == "CRAFTING");
        switch (step++) {
            case 0:
                return craft_search("TEST");
            case 1: {
                CHECK(uistate.read_recipes.count(pipe.ident()) == 0);
                const auto row = row_with(rows_of(screen, "crafting:recipes"), pipe.result_name());
                CHECK(row.id == pipe.ident().str());
                return craft_batch(row.id, 3);
            }
            case 2: {
                CHECK(uistate.read_recipes.count(pipe.ident()) == 1);
                CHECK(screen.focus_row == "3");
                // In a batch, the same affordance moves the cursor to another size.
                return craft_batch("5", 5);
            }
            case 3: {
                CHECK(screen.focus_row == "5");
                CHECK(tab_ids_of(screen, "crafting:tabs") == std::vector<std::string>{"batch"});
                return resolve_action("CYCLE_BATCH");
            }
            default: {
                CHECK(screen.focus_row == pipe.ident().str());
                CHECK(tab_ids_of(screen, "crafting:tabs") == std::vector<std::string>{"search"});
                return resolve_action("QUIT");
            }
        }
    });
    get_avatar().craft(get_avatar().bub_pos());
    CHECK(step == 5);
    CHECK_FALSE(get_avatar().activity);
}

TEST_CASE(
    "leaving the crafting screen after a search or a batch crafts nothing",
    "[crafting][engine_client_component][mcp]") {
    clear_all_state();
    const auto cleanup = on_out_of_scope([]() { clear_all_state(); });
    const auto guard = interaction_test_guard{};
    const auto state = crafting_state_guard{};
    auto& you = get_avatar();
    const auto& iron = recipe_id("test_soldering_iron").obj();
    prepare_crafting_fixture();
    auto& here = get_map();
    here.add_item_or_charges(you.bub_pos(), item::spawn("test_rock"));
    const auto scrap_before = you.amount_of(itype_id("e_scrap"));
    const auto ground_before = here.i_at(you.bub_pos()).size();

    const auto batch = GENERATE(false, true);
    auto step = 0;
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        switch (step++) {
            case 0:
                return craft_search(iron.result_name());
            case 1:
                return batch ? craft_batch(iron.ident().str(), 2) : resolve_action("QUIT");
            default:
                return resolve_action("QUIT");
        }
    });
    you.craft(you.bub_pos());
    CHECK(step == (batch ? 3 : 2));
    CHECK_FALSE(you.activity);
    CHECK(you.amount_of(itype_id("e_scrap")) == scrap_before);
    CHECK(you.amount_of(itype_id("test_soldering_iron")) == 0);
    CHECK(here.i_at(you.bub_pos()).size() == ground_before);
}

TEST_CASE(
    "crafting an unavailable recipe through the screen is denied without mutation",
    "[crafting][engine_client_component][mcp]") {
    clear_all_state();
    const auto cleanup = on_out_of_scope([]() { clear_all_state(); });
    const auto guard = interaction_test_guard{};
    const auto state = crafting_state_guard{};
    auto& you = get_avatar();
    add_crafting_fixture_item(you, itype_id("copper"), 3);
    auto& here = get_map();
    here.add_item_or_charges(you.bub_pos(), item::spawn("test_rock"));
    const auto material_before = you.amount_of(itype_id("copper"));
    const auto ground_before = here.i_at(you.bub_pos()).size();
    const auto& unavailable = recipe_id("test_pipe").obj();

    auto step = 0;
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto snapshot = game_client::current_interaction();
        if (snapshot.context != "CRAFTING") {
            // The native denial is a popup of its own; closing it returns to the screen.
            CHECK(snapshot.message.contains("You can't do that"));
            REQUIRE(step++ == 3);
            return resolve({
                .input_id = snapshot.input_id,
                .operation = game_client::interaction_operation::choose,
                .target_id = snapshot.choices.front().id,
            });
        }
        const auto screen = current_screen();
        switch (step++) {
            case 0:
                return resolve_action("TOGGLE_UNAVAILABLE");
            case 1:
                return craft_search(unavailable.result_name());
            case 2: {
                const auto rows = rows_of(screen, "crafting:recipes");
                REQUIRE(rows.size() == 1);
                CHECK_FALSE(rows.front().enabled);
                CHECK_FALSE(rows.front().denial.empty());
                return craft_row("craft:craft", rows.front().id);
            }
            default:
                return resolve_action("QUIT");
        }
    });
    you.craft(you.bub_pos());
    CHECK(step == 5);
    CHECK_FALSE(you.activity);
    CHECK(you.amount_of(itype_id("copper")) == material_before);
    CHECK(you.amount_of(itype_id("test_pipe")) == 0);
    CHECK(here.i_at(you.bub_pos()).size() == ground_before);
}

TEST_CASE(
    "a nested recipe row expands when invoked and takes no batch",
    "[crafting][engine_client_component][mcp]") {
    clear_all_state();
    const auto cleanup = on_out_of_scope([]() { clear_all_state(); });
    const auto guard = interaction_test_guard{};
    const auto nested_categories = override_option("ENABLE_NESTED_CATEGORIES", "true");
    const auto state = crafting_state_guard{};
    uistate.expanded_recipes.clear();
    prepare_crafting_fixture();
    const auto& nested = recipe_id("test_semantic_recipes").obj();

    auto step = 0;
    auto tab = std::size_t{0};
    auto subtab = std::size_t{0};
    auto rows_before = std::size_t{0};
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto screen = current_screen();
        REQUIRE(screen.context == "CRAFTING");
        if (step == 0) {
            ++step;
            return resolve_action("TOGGLE_UNAVAILABLE");
        }
        const auto rows = rows_of(screen, "crafting:recipes");
        if (step == 1) {
            // Look for the category through the tabs and the subtabs of each.
            const auto at = std::ranges::find(rows, nested.ident().str(), &engine_client::row::id);
            if (at == rows.end()) {
                const auto tabs = tab_ids_of(screen, "crafting:tabs");
                const auto subtabs = tab_ids_of(screen, "crafting:subtabs");
                if (subtab + 1 < subtabs.size()) {
                    return craft_row("craft:subtab", subtabs[++subtab]);
                }
                subtab = 0;
                REQUIRE(tab + 1 < tabs.size());
                return craft_row("craft:tab", tabs[++tab]);
            }
        }
        switch (step++) {
            case 1: {
                const auto category = row_with(rows, nested.result_name());
                CHECK(category.id == nested.ident().str());
                CHECK_FALSE(category.max);
                CHECK_FALSE(category.minimum_count);
                CHECK(category.toggled == false);
                rows_before = rows.size();
                // A batch of a category is not a thing; the screen stays as it was.
                return craft_batch(category.id, 2);
            }
            case 2: {
                CHECK(rows.size() == rows_before);
                CHECK(uistate.expanded_recipes.count(nested.ident()) == 0);
                return craft_row("craft:craft", nested.ident().str());
            }
            default: {
                CHECK(uistate.expanded_recipes.count(nested.ident()) == 1);
                CHECK(rows.size() >= rows_before + 2);
                CHECK(row_with(rows, nested.result_name()).toggled == true);
                // The members hang under their category.
                CHECK(
                    std::ranges::count_if(
                        rows,
                        [&](const auto& row) { return row.parent_id == nested.ident().str(); })
                    >= 2);
                return resolve_action("QUIT");
            }
        }
    });
    get_avatar().craft(get_avatar().bub_pos());
    CHECK(step == 4);
    CHECK_FALSE(get_avatar().activity);
}

TEST_CASE(
    "the crafting screen ignores a stale or unknown row and an unknown tab",
    "[crafting][engine_client_component][mcp]") {
    clear_all_state();
    const auto cleanup = on_out_of_scope([]() { clear_all_state(); });
    const auto guard = interaction_test_guard{};
    const auto state = crafting_state_guard{};
    auto& you = get_avatar();
    prepare_crafting_fixture();
    const auto scrap_before = you.amount_of(itype_id("e_scrap"));
    auto before = nlohmann::json{};

    // A row of the listing before a search, as a client that has not read the new one would name
    // it.
    auto step = 0;
    const auto stale_recipe = std::string{"test_soldering_iron"};
    auto stale = std::string{};
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto screen = current_screen();
        REQUIRE(screen.context == "CRAFTING");
        switch (step++) {
            case 0: {
                stale = stale_recipe;
                return craft_search("__no_semantic_recipe__");
            }
            case 1: {
                CHECK(rows_of(screen, "crafting:recipes").empty());
                before = screen_wire();
                return craft_row("craft:craft", stale);
            }
            case 2:
                return craft_row("craft:favorite", stale);
            case 3:
                return craft_row("craft:hide", "no_such_recipe");
            case 4:
                return craft_batch(stale, 2);
            case 5:
                return craft_row("craft:tab", "CC_NO_SUCH_TAB");
            case 6:
                return craft_row("craft:subtab", "CSC_NO_SUCH_TAB");
            case 7:
                return invoke_event("craft:search");
            case 8:
                return invoke_event("craft:no_such_affordance", {.row = stale});
            default: {
                CHECK(screen_wire().at("root") == before.at("root"));
                return resolve_action("QUIT");
            }
        }
    });
    you.craft(you.bub_pos());
    CHECK(step == 10);
    CHECK_FALSE(you.activity);
    CHECK(you.amount_of(itype_id("e_scrap")) == scrap_before);
    CHECK(you.amount_of(itype_id("test_soldering_iron")) == 0);
}

TEST_CASE(
    "crafting through the screen replays from the recorded invokes",
    "[crafting][engine_client_component][replay][mcp]") {
    clear_all_state();
    const auto cleanup_state = on_out_of_scope([]() { clear_all_state(); });
    const auto guard = interaction_test_guard{};
    const auto state = crafting_state_guard{};
    static auto counter = std::atomic_uint64_t{0};
    const auto directory =
        std::filesystem::path(PATH_INFO::user_dir())
        / ("semantic-crafting-replay-" + std::to_string(counter.fetch_add(1)));
    REQUIRE(std::filesystem::create_directory(directory));
    const auto cleanup_replay = on_out_of_scope([&] {
        replay::stop();
        std::filesystem::remove_all(directory);
    });
    const auto path = directory / "input.jsonl";
    const auto& iron = recipe_id("test_soldering_iron").obj();
    prepare_crafting_fixture();

    replay::configure_recording(path.string(), {.rng_seed = 29});
    replay::start();
    auto step = 0;
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        switch (step++) {
            case 0:
                return craft_search(iron.result_name());
            case 1:
                return craft_batch(iron.ident().str(), 2);
            default:
                return craft_row("craft:craft", "2");
        }
    });
    auto& recorded = get_avatar();
    recorded.craft(recorded.bub_pos());
    REQUIRE(recorded.activity);
    REQUIRE(recorded.activity->id() == activity_id("ACT_CRAFT"));
    CHECK(recorded.amount_of(itype_id("e_scrap")) == 0);
    finish_crafting_activity(recorded);
    REQUIRE_FALSE(recorded.activity);
    CHECK(recorded.amount_of(itype_id("test_soldering_iron")) == 2);
    replay::finish();

    // Normalising the avatar may ask whether to eat while full; the recorded provider must not
    // answer that.
    game_client::memory::set_input_provider([](const int /*timeout*/) {
        return resolve_action("YES");
    });
    clear_all_state();
    prepare_crafting_fixture();
    game_client::memory::set_input_provider([](const int /*timeout*/) -> input_event {
        throw std::runtime_error("crafting playback must not read live input");
    });
    replay::configure_playback(path.string(), {.rng_seed = 29});
    replay::start();
    auto& played = get_avatar();
    played.craft(played.bub_pos());
    REQUIRE(played.activity);
    REQUIRE(played.activity->id() == activity_id("ACT_CRAFT"));
    CHECK(played.amount_of(itype_id("e_scrap")) == 0);
    CHECK_THROWS_AS(replay::next_input_event(), replay::completed);
    finish_crafting_activity(played);
    REQUIRE_FALSE(played.activity);
    CHECK(played.amount_of(itype_id("test_soldering_iron")) == 2);
    CHECK(played.amount_of(itype_id("cable")) == 0);
    replay::finish();
}

TEST_CASE(
    "debugmsg reaches an engine client as a structured interaction with its text",
    "[client][interaction][debug_prompt][mcp]") {
    const auto guard = interaction_test_guard{};
    const auto debug = debug_test_support::scoped_state{};
    auto authority = engine_client::session{};
    auto published = std::string{};
    auto reads = 0;
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        REQUIRE(++reads == 1);
        REQUIRE(authority.publish_boundary());
        const auto current = authority.current();
        REQUIRE(current);
        const auto wire = engine_client::serialize_snapshot(*current);
        REQUIRE(wire);
        published = wire->header;
        for (const auto& part : wire->parts) { published += part; }
        const auto snapshot = game_client::current_interaction();
        const auto found = std::ranges::find(
            snapshot.choices, std::string("Continue"), &game_client::interaction_choice::label);
        REQUIRE(found != snapshot.choices.end());
        return resolve({
            .input_id = snapshot.input_id,
            .operation = game_client::interaction_operation::choose,
            .target_id = found->id,
        });
    });
    debugmsg("wire path marker %d", 4217);
    CHECK(reads == 1);
    CHECK(published.contains("DEBUG_MSG"));
    CHECK(published.contains("wire path marker 4217"));
    CHECK(published.contains("Continue"));
    CHECK(published.contains("Ignore"));
}
TEST_CASE("semantic count bounds include an optional minimum", "[client][interaction][mcp]") {
    const auto context = input_context("COUNT_BOUNDS");
    const auto input = game_client::input_context_scope(context, "COUNT_BOUNDS");
    auto minimum = std::optional<std::uint64_t>{};
    const auto interaction = game_client::interaction_scope(context, [&] {
        return game_client::interaction_snapshot{
            .kind = game_client::interaction_kind::choices,
            .choices = {{
                .id = "bounded",
                .label = "Bounded",
                .selected_count = minimum.value_or(0),
                .minimum_count = minimum,
                .available_count = 50,
            }},
            .allow_set_count = true,
        };
    });

    const auto inventory = game_client::current_interaction();
    CHECK(game_client::resolve_interaction_command({
        .input_id = inventory.input_id,
        .operation = game_client::interaction_operation::set_count,
        .target_id = "bounded",
        .count = 0,
    }));
    minimum = 1;
    const auto crafting = game_client::current_interaction();
    CHECK(crafting.schema_id != inventory.schema_id);
    for (const auto count : {std::uint64_t{1}, std::uint64_t{50}}) {
        CHECK(game_client::resolve_interaction_command({
            .input_id = crafting.input_id,
            .operation = game_client::interaction_operation::set_count,
            .target_id = "bounded",
            .count = count,
        }));
    }
    for (const auto count : {std::uint64_t{0}, std::uint64_t{51}}) {
        const auto result = game_client::resolve_interaction_command({
            .input_id = crafting.input_id,
            .operation = game_client::interaction_operation::set_count,
            .target_id = "bounded",
            .count = count,
        });
        REQUIRE_FALSE(result);
        CHECK(result.error().starts_with("invalid:"));
    }
}
TEST_CASE(
    "interaction providers do not leak into nested raw contexts", "[client][interaction][mcp]") {
    const auto outer = input_context("OUTER_INTERACTION");
    const auto inner = input_context("INNER_RAW");
    const auto outer_input = game_client::input_context_scope(outer, "OUTER_INTERACTION");
    const auto outer_interaction = game_client::interaction_scope(outer, [] {
        return game_client::interaction_snapshot{
            .kind = game_client::interaction_kind::choices,
            .choices = {{.id = "outer", .label = "Outer"}},
        };
    });
    CHECK(game_client::current_interaction().structured);

    REQUIRE_THROWS_AS(
        [&] {
            const auto inner_input = game_client::input_context_scope(inner, "INNER_RAW");
            CHECK_FALSE(game_client::current_interaction().structured);
            throw std::runtime_error("nested failure");
        }(),
        std::runtime_error);

    CHECK(game_client::current_interaction().structured);
    CHECK(game_client::current_interaction().choices.front().id == "outer");
}
TEST_CASE(
    "look around moves its cursor by invoke, refuses a square outside the map and leaves on QUIT",
    "[client][interaction][look][mcp]") {
    clear_map();
    auto& you = get_avatar();
    clear_character(you, false);
    const auto guard = interaction_test_guard{};
    auto reads = 0;
    auto cursor = tripoint_bub_ms{};
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto target = cursor_target();
        CHECK(target["status"] == "look");
        CHECK_FALSE(target.contains("trajectory"));
        CHECK(top_screen()["role"] == "dialog");
        cursor = bubble_at(target["current"]);
        switch (reads++) {
            case 0:
                return resolve_action("RIGHT");
            // The map the game holds starts at 0; a square before it is refused, one on it is not.
            case 1:
                CHECK(refused_move({-5, you.bub_pos().y(), you.bub_pos().z()})
                      == engine_client::error::invalid_arguments);
                CHECK(refused_move({0, you.bub_pos().y(), you.bub_pos().z()}) == std::nullopt);
                return resolve_action("QUIT");
            default:
                FAIL("the look screen stays after QUIT");
        }
    });
    auto center = you.bub_pos();
    const auto result = g->look_around(false, center, center, false, false, false);
    CHECK_FALSE(result.position);
    REQUIRE(reads == 2);
    CHECK(cursor == tripoint_bub_ms(you.bub_pos().x() + 1, you.bub_pos().y(), you.bub_pos().z()));
}
TEST_CASE(
    "look around moves only its cursor to a pointed square, as the native mouse does",
    "[client][interaction][look][mcp]") {
    clear_map();
    auto& you = get_avatar();
    clear_character(you, false);
    const auto guard = interaction_test_guard{};
    const auto pointed = you.bub_pos() + tripoint(3, 2, 0);
    auto reads = 0;
    auto cursor = tripoint_bub_ms{};
    auto offset = tripoint_rel_ms{};
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        cursor = bubble_at(cursor_target()["current"]);
        offset = you.view_offset;
        return reads++ == 0 ? move_cursor(pointed) : resolve_action("QUIT");
    });
    auto center = you.bub_pos();
    g->look_around(false, center, center, false, false, false);
    REQUIRE(reads == 2);
    CHECK(cursor == pointed);
    CHECK(offset == tripoint_rel_ms());
}
TEST_CASE(
    "look around tells what is on the cursor square fact by fact, wherever it moves",
    "[client][interaction][look][mcp]") {
    clear_map();
    auto& you = get_avatar();
    clear_character(you, false);
    const auto guard = interaction_test_guard{};
    auto reads = 0;
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto target = cursor_target();
        CHECK(target["status"] == "look");
        // The square under the cursor, as the mouse view describes it too.
        const auto cursor = bubble_at(target["current"]);
        const auto expected = engine_client::tile_parts(cursor);
        REQUIRE(target["parts"].size() == expected.size());
        for (const auto index : std::views::iota(std::size_t{0}, expected.size())) {
            CHECK(target["parts"][index]["kind"].get<std::string>() == expected[index].kind);
            CHECK(target["parts"][index]["text"].get<std::string>() == expected[index].text);
            CHECK(target["parts"][index].value("id", "") == expected[index].id);
        }
        CHECK_FALSE(target.contains("description"));
        const auto [narrow, wide] = at_two_viewports();
        CHECK(narrow == wide);
        if (reads++ == 0) {
            CHECK(cursor == you.bub_pos());
            return resolve_action("RIGHT");
        }
        CHECK(cursor == you.bub_pos() + tripoint_east);
        CHECK(target["distance"] == 1);
        return resolve_action("QUIT");
    });
    auto center = you.bub_pos();
    g->look_around(false, center, center, false, false, false);
    CHECK(reads == 2);
}

namespace {
auto part_of_kind(const nlohmann::json& parts, const std::string& kind) -> nlohmann::json {
    for (const auto& part : parts) {
        if (part["kind"].get<std::string>() == kind) { return part; }
    }
    return nullptr;
}
} // namespace
TEST_CASE(
    "the map editor tells its cursor square by id and number, toggles its view and moves by invoke",
    "[client][interaction][editmap][mcp]") {
    clear_map();
    auto& you = get_avatar();
    clear_character(you, false);
    const auto guard = interaction_test_guard{};
    const auto restore_view = restore_on_out_of_scope<bool>(uistate.editmap_nsa_viewmode);
    uistate.editmap_nsa_viewmode = false;
    const auto start = you.bub_pos();
    auto reads = 0;
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto target = cursor_target();
        CHECK(target["status"] == "editmap");
        CHECK(top_screen()["role"] == "dialog");
        const auto terrain = part_of_kind(target["parts"], "terrain");
        REQUIRE(terrain.is_object());
        const auto& floor = get_map().ter(bubble_at(target["current"])).obj();
        CHECK(terrain["id"].get<std::string>() == floor.id.str());
        CHECK(terrain["values"]["move_cost"] == floor.movecost);
        switch (reads++) {
            case 0:
                CHECK(bubble_at(target["current"]) == start);
                CHECK(pickup_stat("show_all")["value"] == "Off");
                return resolve_action("EDITMAP_SHOW_ALL");
            case 1:
                CHECK(pickup_stat("show_all")["value"] == "On");
                CHECK(pickup_stat("show_all")["tone"] == "good");
                return move_cursor(start + tripoint(2, 0, 0));
            default:
                CHECK(bubble_at(target["current"]) == start + tripoint(2, 0, 0));
                return resolve_action("QUIT");
        }
    });
    auto edit = editmap{};
    edit.edit();
    CHECK(reads == 3);
}
TEST_CASE(
    "the map editor menus open from its screen and give it back",
    "[client][interaction][editmap][mcp]") {
    clear_map();
    auto& you = get_avatar();
    clear_character(you, false);
    get_map().add_item(you.bub_pos(), item::spawn("rock"));
    const auto guard = interaction_test_guard{};
    const auto menus = std::vector<std::pair<std::string, std::string>>{
        {"EDIT_TERRAIN", "EDITMAP_FEATURE"},
        {"EDIT_FIELDS", "EDIT_FIELDS"},
        {"EDIT_ITEMS", "EDIT_ITEMS"},
        {"EDIT_OVERMAP", "EDIT_MAPGEN"}};
    auto reads = 0;
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto index = static_cast<std::size_t>(reads++ / 2);
        if (reads % 2 == 1) {
            CHECK(cursor_target()["status"] == "editmap");
            return resolve_action(index == menus.size() ? "QUIT" : menus[index].first);
        }
        const auto snapshot = game_client::current_interaction();
        CHECK(snapshot.context == menus[index].second);
        CHECK(snapshot.choices.size() > 1);
        return resolve(
            {.input_id = snapshot.input_id,
             .operation = game_client::interaction_operation::cancel});
    });
    auto edit = editmap{};
    edit.edit();
    CHECK(reads == static_cast<int>(menus.size()) * 2 + 1);
}
TEST_CASE(
    "the map editor shape and retarget screens move their cursor by invoke",
    "[client][interaction][editmap][mcp]") {
    clear_map();
    auto& you = get_avatar();
    clear_character(you, false);
    const auto guard = interaction_test_guard{};
    // The shape and retarget screens pan the view to their cursor; the editor that opens them puts
    // it back.
    const auto restore_offset = restore_on_out_of_scope<tripoint_rel_ms>(you.view_offset);
    const auto start = you.bub_pos();
    auto edit = editmap{};
    edit.target = edit.origin = start;
    edit.target_list = {start};
    auto reads = 0;
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto target = cursor_target();
        switch (reads++) {
            case 0:
                CHECK(target["status"] == "editmap_shape_resize");
                return move_cursor(start + tripoint(2, 1, 0));
            case 1:
                CHECK(bubble_at(target["current"]) == start + tripoint(2, 1, 0));
                return resolve_action("EDITMAP_MOVE");
            case 2:
                CHECK(target["status"] == "editmap_shape_move");
                return move_cursor(start + tripoint(4, 1, 0));
            default:
                CHECK(bubble_at(target["current"]) == start + tripoint(4, 1, 0));
                return resolve_action("CONFIRM");
        }
    });
    const auto shape_size = edit.select_shape(editmap_rect);
    CHECK(reads == 4);
    CHECK(edit.target == start + tripoint(4, 1, 0));
    CHECK(edit.origin == start + tripoint_rel_ms(2, 0, 0));
    CHECK(shape_size > 1);

    // The retarget moves in whole map squares and keeps the old target when it is left.
    edit.target = start;
    const auto east = start + tripoint(24, 0, 0);
    reads = 0;
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto target = cursor_target();
        CHECK(target["status"] == "editmap_retarget");
        if (reads++ == 0) { return move_cursor(east); }
        CHECK(bubble_at(target["current"]) == east);
        return resolve_action("QUIT");
    });
    edit.mapgen_retarget();
    CHECK(reads == 2);
    CHECK(edit.target == start);
}

namespace {
/// The marks of `role` on a published target.
auto marks_of(const nlohmann::json& target, const std::string& role)
    -> std::vector<nlohmann::json> {
    const auto marks = target.value("marks", nlohmann::json::array());
    return marks | std::views::filter([&](const auto& mark) {
               return mark["role"].template get<std::string>() == role;
           })
         | std::ranges::to<std::vector>();
}
/// The squares a mark lists, in the bubble and sorted.
auto squares_of(const nlohmann::json& mark) -> std::vector<tripoint_bub_ms> {
    auto squares =
        mark["squares"] | std::views::transform(bubble_at) | std::ranges::to<std::vector>();
    std::ranges::sort(squares, std::less{});
    return squares;
}
/// The map editor's target as its screen in the stack publishes it, wherever that screen is.
auto editmap_target() -> nlohmann::json {
    const auto stack = nlohmann::json::parse(
        engine_client::serialize_interaction_boundary(stack_state()))["stack"];
    const auto screen = std::ranges::find_if(stack, [](const auto& screen) {
        return screen.value("kind", "") == "editmap";
    });
    REQUIRE(screen != stack.end());
    return (*screen)["root"]["child"]["target"];
}
} // namespace
TEST_CASE(
    "the map editor shape screen publishes the selected squares",
    "[client][interaction][editmap][mcp]") {
    clear_map();
    auto& you = get_avatar();
    clear_character(you, false);
    const auto guard = interaction_test_guard{};
    // The shape and retarget screens pan the view to their cursor; the editor that opens them puts
    // it back.
    const auto restore_offset = restore_on_out_of_scope<tripoint_rel_ms>(you.view_offset);
    const auto start = you.bub_pos();
    auto edit = editmap{};
    edit.target = edit.origin = start;
    edit.target_list = {start};
    auto reads = 0;
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto target = cursor_target();
        if (reads++ == 0) {
            const auto marks = marks_of(target, "selection");
            REQUIRE(marks.size() == 1);
            CHECK(squares_of(marks.front()) == std::vector{start});
            return move_cursor(start + tripoint(2, 2, 0));
        }
        const auto marks = marks_of(target, "selection");
        REQUIRE(marks.size() == 1);
        auto expected = std::vector<tripoint_bub_ms>{};
        for (const auto dx : std::views::iota(0, 3)) {
            for (const auto dy : std::views::iota(0, 3)) {
                expected.push_back(start + tripoint(dx, dy, 0));
            }
        }
        CHECK(squares_of(marks.front()) == expected);
        CHECK_FALSE(marks.front().contains("rect"));
        CHECK(marks_of(target, "omt_border").empty());
        return resolve_action("CONFIRM");
    });
    CHECK(edit.select_shape(editmap_rect_filled) == 9);
    CHECK(reads == 2);
}
TEST_CASE(
    "the map editor mapgen retarget publishes the border of the targeted map square",
    "[client][interaction][editmap][mcp]") {
    clear_map();
    auto& you = get_avatar();
    clear_character(you, false);
    const auto guard = interaction_test_guard{};
    // The shape and retarget screens pan the view to their cursor; the editor that opens them puts
    // it back.
    const auto restore_offset = restore_on_out_of_scope<tripoint_rel_ms>(you.view_offset);
    const auto start = you.bub_pos();
    const auto east = start + tripoint(24, 0, 0);
    auto edit = editmap{};
    edit.target = edit.origin = start;
    edit.target_list = {start};
    auto reads = 0;
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        if (reads++ == 0) { return move_cursor(east); }
        const auto target = cursor_target();
        const auto borders = marks_of(target, "omt_border");
        REQUIRE(borders.size() == 1);
        const auto& rect = borders.front()["rect"];
        CHECK(bubble_at(rect["min"]) == east + tripoint(1 - SEEX, 1 - SEEY, 0));
        CHECK(bubble_at(rect["max"]) == east + tripoint(SEEX, SEEY, 0));
        CHECK_FALSE(borders.front().contains("squares"));
        CHECK(marks_of(target, "selection").empty());
        return resolve_action("QUIT");
    });
    edit.mapgen_retarget();
    CHECK(reads == 2);
}
TEST_CASE(
    "the map editor publishes the mapgen stamp and its border under the preview",
    "[client][interaction][editmap][mcp]") {
    clear_map();
    auto& you = get_avatar();
    clear_character(you, false);
    const auto guard = interaction_test_guard{};
    auto reads = 0;
    auto omt_target = tripoint_bub_ms{};
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto snapshot = game_client::current_interaction();
        switch (reads++) {
            case 0:
                return resolve_action("EDIT_OVERMAP");
            case 1: {
                CHECK(snapshot.context == "EDIT_MAPGEN");
                const auto target = editmap_target();
                omt_target = bubble_at(target["current"]);
                const auto borders = marks_of(target, "omt_border");
                REQUIRE(borders.size() == 1);
                CHECK(bubble_at(borders.front()["rect"]["min"])
                      == omt_target + tripoint(1 - SEEX, 1 - SEEY, 0));
                CHECK(bubble_at(borders.front()["rect"]["max"])
                      == omt_target + tripoint(SEEX, SEEY, 0));
                CHECK(marks_of(target, "stamp").empty());
                // The first overmap terrain the list offers; the null terrain before it is
                // disabled.
                const auto oter = std::ranges::
                    find_if(snapshot.choices, &game_client::interaction_choice::enabled);
                REQUIRE(oter != snapshot.choices.end());
                return resolve(
                    {.input_id = snapshot.input_id,
                     .operation = game_client::interaction_operation::choose,
                     .target_id = oter->id});
            }
            case 2: {
                CHECK(snapshot.context == "MAPGEN_PREVIEW");
                const auto target = editmap_target();
                const auto stamps = marks_of(target, "stamp");
                REQUIRE(stamps.size() == 1);
                CHECK(bubble_at(stamps.front()["rect"]["min"])
                      == omt_target + tripoint(-SEEX, -SEEY, 0));
                CHECK(bubble_at(stamps.front()["rect"]["max"])
                      == omt_target + tripoint(1 + SEEX, 1 + SEEY, 0));
                CHECK(marks_of(target, "omt_border").size() == 1);
                return resolve(
                    {.input_id = snapshot.input_id,
                     .operation = game_client::interaction_operation::cancel});
            }
            case 3:
                CHECK(snapshot.context == "EDIT_MAPGEN");
                CHECK(marks_of(editmap_target(), "stamp").empty());
                return resolve(
                    {.input_id = snapshot.input_id,
                     .operation = game_client::interaction_operation::cancel});
            default:
                // Back on the editor, the border the mapgen targeted is what the edits apply to.
                CHECK(cursor_target()["status"] == "editmap");
                CHECK(marks_of(cursor_target(), "selection").size() == 1);
                CHECK(marks_of(cursor_target(), "omt_border").empty());
                return resolve_action("QUIT");
        }
    });
    auto edit = editmap{};
    edit.edit();
    CHECK(reads == 5);
}
TEST_CASE(
    "an invoke names its affordance and its arguments and needs no native widget",
    "[client][interaction][mcp]") {
    const auto guard = interaction_test_guard{};
    auto ctxt = input_context{"INVOKE_TEST"};
    const auto invoke = [&](game_client::interaction_event event) {
        event.operation = game_client::interaction_operation::invoke;
        return game_client::validate_interaction_event(ctxt, event);
    };
    // No interaction is bound to the context: the screen runs the affordance itself.
    CHECK(invoke({.target_id = "aim:move", .arguments = game_client::interaction_arguments{}}));
    CHECK(invoke(
        {.target_id = "aim:move",
         .arguments = game_client::interaction_arguments{.pane = "pane:right"}}));
    CHECK_FALSE(invoke({.arguments = game_client::interaction_arguments{}}));
    CHECK_FALSE(invoke({.target_id = "aim:move"}));
    CHECK_FALSE(invoke(
        {.target_id = "aim:move", .value = "x", .arguments = game_client::interaction_arguments{}}));
    CHECK_FALSE(invoke(
        {.target_id = "aim:move", .count = 1, .arguments = game_client::interaction_arguments{}}));
    CHECK_FALSE(game_client::validate_interaction_event(
        ctxt,
        {.operation = game_client::interaction_operation::choose,
         .schema_id = "schema",
         .target_id = "entry:1",
         .arguments = game_client::interaction_arguments{}}));
}
TEST_CASE(
    "a choice editor accepts only values of its own type and range",
    "[client][interaction][settings][mcp]") {
    const auto guard = interaction_test_guard{};
    auto ctxt = input_context{"EDITOR_TEST"};
    const auto editor = [](std::string type, std::string value) {
        return game_client::interaction_editor{.type = std::move(type), .value = std::move(value)};
    };
    auto number = editor("integer", "5");
    number.minimum = 0;
    number.maximum = 10;
    auto real = editor("float", "0.5");
    real.minimum = 0;
    real.maximum = 1.5;
    auto text = editor("text", "");
    text.max_length = 3;
    auto select = editor("select", "a");
    select.values = {{.id = "a", .label = "A"}, {.id = "b", .label = "B"}};
    const auto scope = game_client::interaction_scope(ctxt, [&]() {
        return game_client::interaction_snapshot{
            .kind = game_client::interaction_kind::choices,
            .choices =
                {
                    {.id = "flag", .label = "Flag", .editor = editor("bool", "true")},
                    {.id = "mode", .label = "Mode", .editor = select},
                    {.id = "count", .label = "Count", .editor = number},
                    {.id = "ratio", .label = "Ratio", .editor = real},
                    {.id = "name", .label = "Name", .editor = text},
                    {.id = "locked",
                     .label = "Locked",
                     .selectable = false,
                     .editor = editor("bool", "false")},
                    {.id = "plain", .label = "Plain"},
                },
            .field = game_client::interaction_field{.id = "key", .type = "key"},
        };
    });
    const auto active = game_client::input_context_scope(ctxt, "EDITOR_TEST");
    const auto schema = game_client::current_interaction().schema_id;
    const auto fill = [&](const std::string& id, const std::string& value) {
        return game_client::validate_interaction_event(
            ctxt,
            {.operation = game_client::interaction_operation::fill,
             .schema_id = schema,
             .target_id = id,
             .value = value,
             .submit = true});
    };

    CHECK(fill("flag", "false"));
    CHECK_FALSE(fill("flag", "False"));
    CHECK(fill("mode", "b"));
    CHECK_FALSE(fill("mode", "c"));
    CHECK(fill("count", "0"));
    CHECK(fill("count", "10"));
    CHECK_FALSE(fill("count", "11"));
    CHECK_FALSE(fill("count", "-1"));
    CHECK_FALSE(fill("count", "2.5"));
    CHECK_FALSE(fill("count", ""));
    CHECK(fill("ratio", "1.5"));
    CHECK_FALSE(fill("ratio", "1.51"));
    CHECK_FALSE(fill("ratio", "nan"));
    CHECK(fill("name", "abc"));
    CHECK_FALSE(fill("name", "abcd"));
    CHECK_FALSE(fill("locked", "true"));
    CHECK_FALSE(fill("plain", "x"));
    CHECK(fill("key", "F9"));
    CHECK(fill("key", "="));
    CHECK_FALSE(fill("key", "no such key"));
}
namespace {

/// The first snapshot of the menu the callable opens; the menu is cancelled right after.
template <typename Open>
auto first_menu_snapshot(Open&& open) -> game_client::interaction_snapshot {
    auto observed = std::vector<game_client::interaction_snapshot>{};
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        observed.push_back(game_client::current_interaction({.limit = 1000}));
        return resolve({
            .input_id = observed.back().input_id,
            .operation = game_client::interaction_operation::cancel,
        });
    });
    open();
    REQUIRE_FALSE(observed.empty());
    return observed.front();
}

auto selected_entry(const game_client::interaction_snapshot& snapshot)
    -> const game_client::interaction_choice& {
    const auto row = std::ranges::find_if(snapshot.choices, [](const auto& choice) {
        return choice.id.starts_with("entry:") && choice.selected;
    });
    REQUIRE(row != snapshot.choices.end());
    return *row;
}

} // namespace

TEST_CASE(
    "real martial art style menu publishes the selected style's info window",
    "[client][interaction][mcp][uilist_panel]") {
    clear_all_state();
    const auto guard = interaction_test_guard{};
    auto& you = get_avatar();

    const auto snapshot = first_menu_snapshot([&]() { you.martial_arts_data->pick_style(you); });

    const auto& row = selected_entry(snapshot);
    INFO(row.label << ": " << row.description);
    // the panel behind the "more info" key, not only the one-line style summary
    CHECK(row.description.find("can be used with all weapons") != std::string::npos);
    CHECK(row.description.find("arm block") != std::string::npos);
    // the entry's own description and the style panel are two blocks, not one run-on sentence
    CHECK(row.description.find("kicking.\n--\n") != std::string::npos);
}
TEST_CASE(
    "real translocator menu publishes the selected gate's distance",
    "[client][interaction][mcp][uilist_panel]") {
    clear_all_state();
    const auto guard = interaction_test_guard{};
    auto& you = get_avatar();
    const auto restore = on_out_of_scope([&you]() {
        you.translocators = pimpl<teleporter_list>();
    });
    const auto target = you.abs_omt_pos() + point(3, 0);
    auto data = std::istringstream{string_format(
        R"({"known_teleporters":[{"position":[%d,%d,%d],"name":"north gate"}]})", target.x(),
        target.y(), target.z())};
    auto in = JsonIn{data};
    you.translocators->deserialize(in);

    const auto snapshot = first_menu_snapshot([&]() { you.translocators->translocate({}); });

    const auto& row = selected_entry(snapshot);
    INFO(row.label << ": " << row.description);
    CHECK(row.description.find("Distance: 3") != std::string::npos);
}
TEST_CASE(
    "real spellbook menu publishes the selected spell's info without an entry description",
    "[client][interaction][mcp][uilist_panel]") {
    clear_all_state();
    const auto guard = interaction_test_guard{};

    const auto snapshot = first_menu_snapshot([]() {
        auto callback = spellbook_callback{};
        callback.add_spell(spell_id("test_spell_pew"));
        auto menu = uilist{};
        menu.callback = &callback;
        menu.entries.emplace_back("test_spell_pew");
        menu.query();
    });

    const auto& row = selected_entry(snapshot);
    INFO(row.label << ": " << row.description);
    CHECK(row.description.find("Difficulty") != std::string::npos);
    // no entry description, so no leading separator
    CHECK_FALSE(row.description.starts_with("\n--"));
}
TEST_CASE(
    "real wish menus publish the selected mutation, monster and item panels",
    "[client][interaction][mcp][uilist_panel]") {
    clear_all_state();
    const auto guard = interaction_test_guard{};
    auto& you = get_avatar();

    const auto mutation = first_menu_snapshot([&]() { debug_menu::wishmutate(&you); });
    INFO(selected_entry(mutation).label << ": " << selected_entry(mutation).description);
    CHECK(selected_entry(mutation).description.find("Id:") != std::string::npos);
    CHECK(selected_entry(mutation).description.find("pts:") != std::string::npos);

    const auto monster = first_menu_snapshot([&]() { debug_menu::wishmonster(std::nullopt); });
    INFO(selected_entry(monster).label << ": " << selected_entry(monster).description);
    CHECK(selected_entry(monster).description.find(selected_entry(monster).label)
          != std::string::npos);
    CHECK(selected_entry(monster).description.size() > selected_entry(monster).label.size());

    const auto item = first_menu_snapshot([&]() { debug_menu::wishitem(&you); });
    INFO(selected_entry(item).label << ": " << selected_entry(item).description);
    CHECK(selected_entry(item).description.find("#") != std::string::npos);
    CHECK(selected_entry(item).description.find("Volume") != std::string::npos);
    // only the selected row carries the panel; the other rows of a thousand-row list stay cheap
    CHECK(
        std::ranges::count_if(
            item.choices,
            [](const auto& choice) {
                return choice.id.starts_with("entry:") && !choice.selected
                    && !choice.description.empty();
            })
        == 0);
}
TEST_CASE(
    "debug effect menus let a client open the effect list and read the selected effect's panel",
    "[client][interaction][mcp][uilist_panel]") {
    clear_all_state();
    const auto guard = interaction_test_guard{};
    auto& you = get_avatar();
    const auto body_part = you.get_all_body_parts().at(1);
    uistate.debug_menu.effect = wisheffect_state{};

    // edit menu -> "add effect" key -> effect list -> "change body part" key -> body part list
    auto observed = std::vector<game_client::interaction_snapshot>{};
    const auto has_key = [](const game_client::interaction_snapshot& snapshot, const int key) {
        return std::ranges::contains(
            snapshot.keys, "bn:key:" + std::to_string(key), &game_client::interaction_key::id);
    };
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        observed.push_back(game_client::current_interaction({.limit = 1000}));
        const auto& snapshot = observed.back();
        const auto press = [&](const int key) {
            return invoke_event("bn:key:" + std::to_string(key));
        };
        if (observed.size() == 1 && has_key(snapshot, 'a')) { return press('a'); }
        if (observed.size() == 2 && has_key(snapshot, 'b')) { return press('b'); }
        auto command = game_client::interaction_command{
            .input_id = snapshot.input_id,
            .operation = game_client::interaction_operation::cancel,
        };
        if (observed.size() == 3) {
            command.operation = game_client::interaction_operation::choose;
            command.target_id = "entry:1";
        }
        return resolve(command);
    });
    debug_menu::effect_edit_menu(you);

    REQUIRE(observed.size() >= 4);
    const auto& list = observed[1];
    INFO(list.title << ": " << selected_entry(list).description);
    const auto& row = selected_entry(list);
    CHECK(row.description.find("ID: " + row.label) != std::string::npos);
    CHECK(row.description.find("Body part") != std::string::npos);
    CHECK(row.description.find("Global") != std::string::npos);
    CHECK(row.description.find("Duration") != std::string::npos);
    CHECK(row.description.find("Permanent") != std::string::npos);
    // the effect list's own handled actions are offered to the client
    for (const auto key : {'b', 'i', 'd', 'o'}) { CHECK(has_key(list, key)); }
    // picking a body part through the client changes the panel of the redrawn list
    CHECK(selected_entry(observed[3]).description.find(body_part->name.translated())
          != std::string::npos);
}
TEST_CASE(
    "an item operation picks up or drops the stack a row names",
    "[client][interaction][mcp][engine_client_contract]") {
    const auto guard = pickup_fixture_guard{};
    auto& you = get_avatar();
    for (const auto _ : std::views::iota(0, 3)) { add_ground_item(itype_id("test_1kg_cube")); }
    add_ground_item(itype_id("test_2x4"));
    const auto ground_id = [&](const std::string& name) {
        const auto stacks = engine_client::world::item_stacks(get_map(), you);
        const auto row = std::ranges::find(stacks.ground, name, [](const auto& stack) {
            return stack.row.appearance.id.value_or("");
        });
        REQUIRE(row != stacks.ground.end());
        return row->row.id;
    };
    const auto test_1kg_cube = ground_id("test_1kg_cube");

    SECTION("a ground row stands for its equal items") {
        const auto stack = engine_client::item_operation_stack(
            {.item_id = test_1kg_cube, .count = 3});
        REQUIRE(stack);
        CHECK(stack->items.size() == 3);
        CHECK(stack->row.count == 3);
        CHECK(test_1kg_cube != ground_id("test_2x4"));
    }
    SECTION("an unknown row, a row of the other list and a count past the stack name nothing") {
        CHECK_FALSE(engine_client::item_operation_stack({.item_id = "item:none", .count = 1}));
        CHECK_FALSE(engine_client::item_operation_stack({.item_id = test_1kg_cube, .count = 4}));
        CHECK_FALSE(engine_client::item_operation_stack(
            {.drop = true, .item_id = test_1kg_cube, .count = 1}));
    }
    SECTION("pickup moves only the asked count to the avatar") {
        engine_client::run_item_operation({.item_id = test_1kg_cube, .count = 2});
        finish_pickup_activity(you);
        CHECK(ground_item_count(itype_id("test_1kg_cube")) == 1);
        CHECK(ground_item_count(itype_id("test_2x4")) == 1);
        CHECK(you.amount_of(itype_id("test_1kg_cube")) == 2);
    }
    SECTION("drop puts the asked count of a carried stack on the ground") {
        for (const auto _ : std::views::iota(0, 3)) { you.i_add(item::spawn("test_1kg_cube")); }
        const auto stacks = engine_client::world::item_stacks(get_map(), you);
        const auto carried = std::ranges::find(stacks.inventory, "carried", [](const auto& stack) {
            return stack.row.slot.value_or("");
        });
        REQUIRE(carried != stacks.inventory.end());
        const auto before = ground_item_count(itype_id("test_1kg_cube"));
        engine_client::run_item_operation({.drop = true, .item_id = carried->row.id, .count = 2});
        REQUIRE(you.activity);
        process_activity(you);
        CHECK(ground_item_count(itype_id("test_1kg_cube")) == before + 2);
    }
}
TEST_CASE(
    "an item operation counts a row of a charges item in charges",
    "[client][interaction][mcp][engine_client_contract]") {
    const auto guard = pickup_fixture_guard{};
    auto& you = get_avatar();
    const auto bits = itype_id("test_platinum_bit");
    const auto row = [&](const bool carried) {
        const auto stacks = engine_client::world::item_stacks(get_map(), you);
        const auto& rows = carried ? stacks.inventory : stacks.ground;
        const auto found = std::ranges::find(rows, bits.str(), [](const auto& stack) {
            return stack.row.appearance.id.value_or("");
        });
        REQUIRE(found != rows.end());
        return found->row;
    };

    SECTION("pickup takes the asked charges of a ground row, up to the charges it holds") {
        add_ground_item(bits, 30);
        const auto ground = row(false);
        CHECK(ground.count == 30);
        CHECK(engine_client::item_operation_stack({.item_id = ground.id, .count = 30}));
        CHECK_FALSE(engine_client::item_operation_stack({.item_id = ground.id, .count = 31}));
        engine_client::run_item_operation({.item_id = ground.id, .count = 12});
        finish_pickup_activity(you);
        CHECK(ground_charges(bits) == 18);
        CHECK(you.charges_of(bits) == 12);
    }
    SECTION("drop of the whole carried row puts every charge on the ground") {
        you.i_add(item::spawn(bits, calendar::turn, 30));
        const auto carried = row(true);
        CHECK(carried.count == 30);
        engine_client::run_item_operation({.drop = true, .item_id = carried.id, .count = 30});
        REQUIRE(you.activity);
        process_activity(you);
        CHECK(ground_charges(bits) == 30);
        CHECK(you.charges_of(bits) == 0);
    }
    SECTION("drop of part of a carried row puts that many charges on the ground") {
        you.i_add(item::spawn(bits, calendar::turn, 30));
        engine_client::run_item_operation({.drop = true, .item_id = row(true).id, .count = 10});
        REQUIRE(you.activity);
        process_activity(you);
        CHECK(ground_charges(bits) == 10);
        CHECK(you.charges_of(bits) == 20);
    }
}
namespace {

/// `game::handle_action` is private; the explicit instantiation reaches the native dispatch without
/// changing the game's headers, as map_perception_bionics_boundary_test.cpp does.
struct handle_action_member {
    using type = auto (game::*)() -> bool;
    friend auto native_member(handle_action_member) -> type;
};
template <typename Tag, typename Tag::type Member> struct native_test_member {
    friend auto native_member(Tag /*tag*/) -> typename Tag::type { return Member; }
};
struct mode_member {
    using type = std::unique_ptr<special_game> game::*;
    friend auto native_member(mode_member) -> type;
};
template struct native_test_member<handle_action_member, &game::handle_action>;
template struct native_test_member<mode_member, &game::gamemode>;

auto handle_action() -> bool {
    // The native loop has a game mode by the time it reads input; the one made here goes with the
    // call.
    auto& mode = g.get()->*native_member(mode_member{});
    const auto made = !mode;
    if (made) { mode = std::make_unique<special_game>(); }
    const auto restore = on_out_of_scope([&] {
        if (made) { mode.reset(); }
    });
    return (g.get()->*native_member(handle_action_member{}))();
}

} // namespace

TEST_CASE(
    "a drop a client submits is recorded, replays and leaves the view where it is",
    "[client][interaction][mcp][engine_client_contract][replay]") {
    const auto guard = interaction_test_guard{};
    static auto counter = std::atomic_uint64_t{0};
    const auto directory =
        std::filesystem::path(PATH_INFO::user_dir())
        / ("item-operation-replay-" + std::to_string(counter.fetch_add(1)));
    REQUIRE(std::filesystem::create_directory(directory));
    const auto running = game_session::running();
    const auto cleanup = on_out_of_scope([&] {
        replay::stop();
        std::filesystem::remove_all(directory);
        game_client::memory::set_input_provider({});
        clear_pickup_fixture_state();
        get_avatar().view_offset = tripoint_rel_ms();
        game_session::set_running(running);
    });
    const auto animations = override_option("ANIMATIONS", "false");
    const auto realtime = override_option("TURN_DURATION", "0");
    const auto path = directory / "input.jsonl";
    const auto cube = itype_id("test_1kg_cube");
    const auto panned = tripoint_rel_ms(3, -2, 0);
    const auto setup = [&] {
        clear_pickup_fixture_state();
        auto& you = get_avatar();
        you.setpos(map_local_to_abs(get_map(), pickup_test_pos));
        you.set_moves(1000);
        you.worn.push_back(item::spawn("duffelbag"));
        for (const auto _ : std::views::iota(0, 3)) { you.i_add(item::spawn(cube)); }
        you.view_offset = panned;
        game_session::set_running(true);
        engine_client::process_session().replace_world();
    };
    const auto check_dropped = [&] {
        auto& you = get_avatar();
        CHECK(you.view_offset == panned);
        REQUIRE(you.activity);
        process_activity(you);
        CHECK(ground_item_count(cube) == 2);
        CHECK(you.amount_of(cube) == 1);
    };

    setup();
    replay::configure_recording(path.string(), {.rng_seed = 424242});
    replay::start();
    auto reads = 0;
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        REQUIRE(reads++ == 0);
        auto& session = engine_client::process_session();
        REQUIRE(session.publish_boundary());
        const auto current = session.current();
        REQUIRE(current);
        const auto stacks = engine_client::world::item_stacks(get_map(), get_avatar());
        const auto carried = std::ranges::find(stacks.inventory, cube.str(), [](const auto& stack) {
            return stack.row.appearance.id.value_or("");
        });
        REQUIRE(carried != stacks.inventory.end());
        REQUIRE(session.submit({
            .epoch = session.epoch(),
            .expect =
                {.revision = current->at.revision, .boundary_id = current->value.interaction.id},
            .operation =
                engine_client::item_operation{.drop = true, .item_id = carried->row.id, .count = 2},
        }));
        // As the controller delivers a command: the prepared input, resolved at the native
        // boundary; the backend then hands the game what the session validated.
        const auto native = session.prepare_input(game_client::memory::screen_size());
        REQUIRE(native);
        const auto event =
            game_client::resolve_input_command(*native, game_client::memory::screen_size());
        REQUIRE(event);
        return *event;
    });
    handle_action();
    check_dropped();
    replay::finish();
    REQUIRE(reads == 1);

    setup();
    game_client::memory::set_input_provider([](const int /*timeout*/) -> input_event {
        throw std::runtime_error("playback must not read live input");
    });
    replay::configure_playback(path.string(), {.rng_seed = 424242});
    replay::start();
    handle_action();
    check_dropped();
    CHECK_THROWS_AS(replay::next_input_event(), replay::completed);
    replay::finish();
}
namespace {

auto tile_ids(const tripoint_bub_ms& at) -> std::vector<std::string> {
    return engine_client::tile_actions(at) | std::views::transform(&engine_client::tile_action::id)
         | std::ranges::to<std::vector>();
}

auto offers(const tripoint_bub_ms& at, const std::string& id) -> bool {
    return std::ranges::contains(tile_ids(at), id);
}

/// As the controller delivers a command: the session validates the operation at the native
/// boundary, then hands the game what it validated.
auto deliver(const engine_client::command_operation& operation)
    -> std::expected<input_event, engine_client::error> {
    auto& session = engine_client::process_session();
    REQUIRE(session.publish_boundary());
    const auto current = session.current();
    REQUIRE(current);
    REQUIRE(session.submit({
        .epoch = session.epoch(),
        .expect = {.revision = current->at.revision, .boundary_id = current->value.interaction.id},
        .operation = operation,
    }));
    const auto native = session.prepare_input(game_client::memory::screen_size());
    if (!native) { return std::unexpected(native.error()); }
    const auto event =
        game_client::resolve_input_command(*native, game_client::memory::screen_size());
    REQUIRE(event);
    return *event;
}

auto absolute(const tripoint_bub_ms& at) -> engine_client::position {
    return engine_client::world::position_at(at);
}

struct vehicle_parts {
    bool controls = false;
    bool horn = false;
};

/// A one-square vehicle on `at` with the given parts on its frame.
auto place_vehicle(const tripoint_bub_ms& at, const vehicle_parts& parts) -> void {
    auto& here = get_map();
    auto* const veh = here.add_vehicle(vproto_id("none"), at, 0_degrees, 0, 0);
    REQUIRE(veh != nullptr);
    const auto install = [&](const char* part) {
        REQUIRE(veh->install_part(tripoint_mnt_veh::zero(), vpart_id(part), true) >= 0);
    };
    install("frame_vertical");
    if (parts.controls) { install("controls"); }
    if (parts.horn) { install("horn_car"); }
    here.add_vehicle_to_cache(veh);
}

auto logged(const std::string& text) -> bool {
    return std::ranges::contains(Messages::feed_since({}), text, &Messages::feed_entry::text);
}

/// Runs one turn in which the client takes the tile action `id` on `at`, with the activity it
/// starts; `then` answers every later read, numbered from 1.
auto run_tile_action(
    const std::string& id, const tripoint_bub_ms& at,
    const std::function<auto(int)->input_event>& then) -> void {
    const auto running = game_session::running();
    const auto cleanup = on_out_of_scope([&] {
        game_client::memory::set_input_provider({});
        game_session::set_running(running);
    });
    game_session::set_running(true);
    engine_client::process_session().replace_world();
    auto reads = 0;
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        if (reads++ > 0) { return then(reads - 1); }
        const auto event = deliver(
            engine_client::tile_operation{.action_id = id, .target = absolute(at)});
        REQUIRE(event);
        return *event;
    });
    handle_action();
    // As the game loop goes on: an aim, for one, runs as the activity the action started.
    if (auto& you = get_avatar(); you.activity) { you.activity->do_turn(you); }
}

} // namespace

TEST_CASE(
    "a square offers the actions the avatar can take on it now",
    "[client][interaction][mcp][engine_client_contract][tile_action]") {
    const auto guard = interaction_test_guard{};
    const auto fixture = pickup_fixture_guard(false);
    auto& here = get_map();
    const auto east = pickup_test_pos + tripoint_east;
    const auto far_away = pickup_test_pos + tripoint(6, 0, 0);

    SECTION("a closed door opens, an open one closes, and neither works from afar") {
        REQUIRE(here.ter_set(east, ter_id("t_door_c")));
        CHECK(offers(east, "open"));
        CHECK_FALSE(offers(east, "close"));
        REQUIRE(here.ter_set(east, ter_id("t_door_o")));
        CHECK(offers(east, "close"));
        CHECK_FALSE(offers(east, "open"));
        REQUIRE(here.ter_set(far_away, ter_id("t_door_c")));
        CHECK_FALSE(offers(far_away, "open"));
    }
    SECTION("items are picked up underfoot and beside, not from afar") {
        here.add_item(pickup_test_pos, item::spawn("test_1kg_cube"));
        here.add_item(east, item::spawn("test_1kg_cube"));
        here.add_item(far_away, item::spawn("test_1kg_cube"));
        CHECK(offers(pickup_test_pos, "pickup"));
        CHECK(offers(east, "pickup"));
        CHECK_FALSE(offers(far_away, "pickup"));
    }
    SECTION(
        "every square can be looked at; walking is offered for any square but the avatar's own") {
        set_time(calendar::turn_zero + 12_hours);
        here.build_map_cache(0, true);
        CHECK(offers(far_away, "look"));
        CHECK(offers(pickup_test_pos + tripoint(3, 0, 0), "travel"));
        CHECK(offers(pickup_test_pos, "look"));
        CHECK_FALSE(offers(pickup_test_pos, "travel"));
        CHECK_FALSE(offers(far_away, "open"));
        CHECK_FALSE(offers(far_away, "close"));
        CHECK_FALSE(offers(far_away, "attack"));
    }
    SECTION("a monster beside the avatar is attacked, and shot at only with a gun") {
        here.build_map_cache(0, true);
        spawn_test_monster("mon_zombie", east);
        here.build_map_cache(0, true);
        CHECK(offers(east, "attack"));
        CHECK_FALSE(offers(east, "fire"));
        get_avatar().wield(item::spawn("glock_19", calendar::start_of_cataclysm, 0));
        CHECK(offers(east, "fire"));
        CHECK_FALSE(offers(pickup_test_pos, "attack"));
    }
    SECTION("a driver is offered no step and no route, as the native mouse steers nothing") {
        set_time(calendar::turn_zero + 12_hours);
        here.build_map_cache(0, true);
        spawn_test_monster("mon_zombie", east);
        here.build_map_cache(0, true);
        auto& you = get_avatar();
        const auto driving = on_out_of_scope([&] { you.controlling_vehicle = false; });
        you.controlling_vehicle = true;
        CHECK_FALSE(offers(east, "attack"));
        CHECK_FALSE(offers(pickup_test_pos + tripoint(3, 0, 0), "travel"));
        CHECK(offers(east, "look"));
    }
    SECTION("a person is talked to only within the range the native chat lists") {
        set_time(calendar::turn_zero + 12_hours);
        const auto near = pickup_test_pos + tripoint(SEEX * 2, 0, 0);
        const auto far = pickup_test_pos + tripoint(0, SEEX * 2 + 1, 0);
        spawn_npc(near, "test_talker");
        spawn_npc(far, "test_talker");
        here.build_map_cache(0, true);
        REQUIRE(get_avatar().sees(far));
        CHECK(offers(near, "talk"));
        CHECK_FALSE(offers(far, "talk"));
    }
    SECTION(
        "a hostile person beside the avatar is attacked as moving into it does; a peaceful one is "
        "not") {
        set_time(calendar::turn_zero + 12_hours);
        const auto west = pickup_test_pos + tripoint_west;
        auto& foe = spawn_npc(east, "test_talker");
        foe.set_attitude(NPCATT_KILL);
        spawn_npc(west, "test_talker").set_attitude(NPCATT_NULL);
        here.build_map_cache(0, true);
        REQUIRE(foe.is_enemy());
        CHECK(offers(east, "attack"));
        CHECK_FALSE(offers(west, "attack"));
    }
    SECTION("vehicle controls are used on their square beside or under the avatar, not from afar") {
        const auto north = pickup_test_pos + tripoint_north;
        place_vehicle(east, {.controls = true});
        place_vehicle(north, {.controls = false});
        place_vehicle(pickup_test_pos + tripoint(3, 0, 0), {.controls = true});
        place_vehicle(pickup_test_pos, {.controls = true});
        CHECK(offers(east, "control_vehicle"));
        CHECK(offers(pickup_test_pos, "control_vehicle"));
        CHECK_FALSE(offers(north, "control_vehicle"));
        CHECK_FALSE(offers(pickup_test_pos + tripoint(3, 0, 0), "control_vehicle"));
    }
    SECTION("controls beside the avatar are not offered while the native control first takes or "
            "drives the avatar's own vehicle") {
        auto& you = get_avatar();
        place_vehicle(east, {.controls = true});
        place_vehicle(pickup_test_pos, {.controls = true});
        const auto seated = on_out_of_scope([&] {
            you.in_vehicle = false;
            you.controlling_vehicle = false;
        });
        you.in_vehicle = true;
        CHECK_FALSE(offers(east, "control_vehicle"));
        you.in_vehicle = false;
        you.controlling_vehicle = true;
        CHECK_FALSE(offers(east, "control_vehicle"));
        you.controlling_vehicle = false;
        CHECK(offers(east, "control_vehicle"));
    }
    SECTION("each action carries the native label and the player's keys") {
        const auto actions = engine_client::tile_actions(east);
        const auto look = std::ranges::find(actions, "look", &engine_client::tile_action::id);
        REQUIRE(look != actions.end());
        CHECK_FALSE(look->label.empty());
        CHECK(std::ranges::contains(look->keys, ";"));
        const auto travel = std::ranges::find(actions, "travel", &engine_client::tile_action::id);
        REQUIRE(travel != actions.end());
        CHECK_FALSE(travel->label.empty());
        CHECK(travel->keys.empty());
    }
}

TEST_CASE(
    "a tile action runs the native action on its square and replays",
    "[client][interaction][mcp][engine_client_contract][tile_action][replay]") {
    const auto guard = interaction_test_guard{};
    static auto counter = std::atomic_uint64_t{0};
    const auto directory =
        std::filesystem::path(PATH_INFO::user_dir())
        / ("tile-action-replay-" + std::to_string(counter.fetch_add(1)));
    REQUIRE(std::filesystem::create_directory(directory));
    const auto running = game_session::running();
    const auto cleanup = on_out_of_scope([&] {
        replay::stop();
        std::filesystem::remove_all(directory);
        game_client::memory::set_input_provider({});
        clear_pickup_fixture_state();
        game_session::set_running(running);
    });
    const auto animations = override_option("ANIMATIONS", "false");
    const auto realtime = override_option("TURN_DURATION", "0");
    const auto path = directory / "input.jsonl";
    const auto east = pickup_test_pos + tripoint_east;
    const auto setup = [&] {
        clear_pickup_fixture_state();
        auto& you = get_avatar();
        you.setpos(map_local_to_abs(get_map(), pickup_test_pos));
        you.set_moves(1000);
        REQUIRE(get_map().ter_set(east, ter_id("t_door_o")));
        game_session::set_running(true);
        engine_client::process_session().replace_world();
    };
    const auto check_closed = [&] {
        CHECK(get_map().ter(east) == ter_id("t_door_c"));
        CHECK(get_avatar().moves < 1000);
    };

    setup();
    replay::configure_recording(path.string(), {.rng_seed = 424242});
    replay::start();
    auto reads = 0;
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        REQUIRE(reads++ == 0);
        const auto event = deliver(
            engine_client::tile_operation{.action_id = "close", .target = absolute(east)});
        REQUIRE(event);
        return *event;
    });
    handle_action();
    check_closed();
    replay::finish();
    REQUIRE(reads == 1);

    setup();
    game_client::memory::set_input_provider([](const int /*timeout*/) -> input_event {
        throw std::runtime_error("playback must not read live input");
    });
    replay::configure_playback(path.string(), {.rng_seed = 424242});
    replay::start();
    handle_action();
    check_closed();
    CHECK_THROWS_AS(replay::next_input_event(), replay::completed);
    replay::finish();
}

TEST_CASE(
    "a tile action the square does not offer is rejected before anything runs",
    "[client][interaction][mcp][engine_client_contract][tile_action]") {
    const auto guard = interaction_test_guard{};
    const auto fixture = pickup_fixture_guard(false);
    const auto running = game_session::running();
    const auto cleanup = on_out_of_scope([&] { game_session::set_running(running); });
    game_session::set_running(true);
    engine_client::process_session().replace_world();
    const auto east = pickup_test_pos + tripoint_east;
    const auto moves = get_avatar().moves;
    const auto cleanup_input = on_out_of_scope([&] {
        game_client::memory::set_input_provider({});
    });
    // The native loop is what makes the map's input context active while it reads.
    auto errors = std::vector<engine_client::error>{};
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        for (const auto& at : {east, pickup_test_pos + tripoint(400, 0, 0)}) {
            const auto event = deliver(engine_client::tile_operation{
                .action_id = at == east ? "close" : "look", .target = absolute(at)});
            INFO(at.x() - pickup_test_pos.x());
            REQUIRE_FALSE(event);
            errors.push_back(event.error());
        }
        return input_event{};
    });
    handle_action();
    CHECK(errors == std::vector(2, engine_client::error::validation_failed));
    CHECK(get_avatar().moves == moves);
}

TEST_CASE(
    "talking on a square refuses an avatar without a face as the native chat does",
    "[client][interaction][mcp][engine_client_contract][tile_action]") {
    const auto guard = interaction_test_guard{};
    const auto fixture = pickup_fixture_guard(false);
    set_time(calendar::turn_zero + 12_hours);
    const auto east = pickup_test_pos + tripoint_east;
    auto& talker = spawn_npc(east, "test_talker");
    get_avatar().set_mutation(trait_id("PROF_FOODP"));
    get_map().build_map_cache(0, true);
    REQUIRE(offers(east, "talk"));
    const auto moves = talker.moves;
    run_tile_action("talk", east, [](const int read) -> input_event {
        FAIL("a dialogue opened and read input " << read);
        return {};
    });
    CHECK(logged("You can't speak without your face!"));
    CHECK(talker.moves == moves);
}

TEST_CASE(
    "firing on a square starts the aim on the creature there, not the closest one",
    "[client][interaction][mcp][engine_client_contract][tile_action]") {
    const auto guard = interaction_test_guard{};
    const auto fixture = pickup_fixture_guard(false);
    set_time(calendar::turn_zero + 12_hours);
    const auto chosen = pickup_test_pos + tripoint(3, 0, 0);
    auto& here = get_map();
    here.build_map_cache(0, true);
    spawn_test_monster("mon_zombie", pickup_test_pos + tripoint_east);
    spawn_test_monster("mon_zombie", chosen);
    arm_character(get_avatar(), "glock_19");
    here.build_map_cache(0, true);
    REQUIRE(offers(chosen, "fire"));
    auto aimed = std::optional<tripoint_bub_ms>{};
    run_tile_action("fire", chosen, [&](const int read) {
        REQUIRE(read == 1);
        aimed = bubble_at(cursor_target()["current"]);
        return resolve_action("QUIT");
    });
    CHECK(aimed == chosen);
}

TEST_CASE(
    "vehicle controls on a square are used there without asking which controls",
    "[client][interaction][mcp][engine_client_contract][tile_action]") {
    const auto guard = interaction_test_guard{};
    const auto fixture = pickup_fixture_guard(false);
    const auto east = pickup_test_pos + tripoint_east;
    place_vehicle(pickup_test_pos + tripoint_west, {.controls = true});
    place_vehicle(east, {.controls = true, .horn = true});
    get_map().build_map_cache(0, true);
    auto labels = std::vector<std::string>{};
    run_tile_action("control_vehicle", east, [&](const int read) {
        REQUIRE(read == 1);
        const auto snapshot = game_client::current_interaction();
        std::ranges::transform(
            snapshot.choices, std::back_inserter(labels), &game_client::interaction_choice::label);
        return resolve_action("QUIT");
    });
    INFO(labels.size());
    CHECK(std::ranges::any_of(labels, [](const auto& label) {
        return label.contains("Honk horn");
    }));
}

TEST_CASE(
    "a client sets a mode to a named value through the session, however often it sends it",
    "[client][interaction][mcp][engine_client_contract][engine_client_mode]") {
    const auto guard = interaction_test_guard{};
    const auto fixture = pickup_fixture_guard(false);
    const auto running = game_session::running();
    const auto safe = g->safe_mode;
    const auto cleanup = on_out_of_scope([&] {
        game_client::memory::set_input_provider({});
        game_session::set_running(running);
        g->safe_mode = safe;
    });
    game_session::set_running(true);
    engine_client::process_session().replace_world();
    g->set_safe_mode(SAFE_MODE_ON);
    const auto set_mode = [](const std::string& mode, const std::string& value) {
        game_client::memory::set_input_provider([&](const int /*timeout*/) {
            const auto event = deliver(engine_client::mode_operation{.mode = mode, .value = value});
            REQUIRE(event);
            return *event;
        });
        handle_action();
    };
    for (const auto* target : {"off", "off", "on", "on"}) {
        INFO(target);
        set_mode("safe_mode", target);
        CHECK((g->safe_mode != SAFE_MODE_OFF) == (std::string{target} == "on"));
    }
    // An unknown mode or value is refused at validation, before the game reads anything.
    auto errors = std::vector<engine_client::error>{};
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        for (const auto& operation :
             {engine_client::mode_operation{.mode = "nope", .value = "on"},
              engine_client::mode_operation{.mode = "safe_mode", .value = "nope"}}) {
            const auto event = deliver(operation);
            REQUIRE_FALSE(event);
            errors.push_back(event.error());
        }
        return input_event{};
    });
    handle_action();
    CHECK(errors == std::vector(2, engine_client::error::validation_failed));
}
namespace {

auto filter_menu(uilist& menu) -> void {
    menu.entries.emplace_back(10, true, MENU_AUTOASSIGN, "Teleport");
    menu.entries.emplace_back(20, true, MENU_AUTOASSIGN, "Spawn monster");
    menu.entries.emplace_back(30, true, MENU_AUTOASSIGN, "Teleport to overmap");
    menu.filterlist();
}

auto list_filter_of(const nlohmann::json& screen) -> std::string {
    return screen.at("root").value("filter", "");
}

auto offers_filter_text(const nlohmann::json& screen) -> bool {
    return std::ranges::any_of(screen.at("affordances"), [](const nlohmann::json& offered) {
        return offered.at("id") == "bn:filter:list:choices" && offered.contains("args")
            && offered.at("args").contains("text");
    });
}

} // namespace

TEST_CASE(
    "a client sets and clears a uilist's filter by invoking its filter affordance",
    "[client][interaction][mcp]") {
    const auto guard = interaction_test_guard{};
    auto menu = uilist{};
    filter_menu(menu);
    auto labels = std::vector<std::vector<std::string>>{};
    auto filters = std::vector<std::string>{};
    auto offered = true;
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto snapshot = game_client::current_interaction();
        const auto screen = screen_wire();
        labels.push_back(
            snapshot.choices | std::views::transform(&game_client::interaction_choice::label)
            | std::ranges::to<std::vector>());
        filters.push_back(list_filter_of(screen));
        offered = offered && offers_filter_text(screen);
        switch (labels.size()) {
            case 1:
                return invoke_event("bn:filter:list:choices", {.text = "tele"});
            case 2:
                return invoke_event("bn:filter:list:choices", {.text = ""});
            default:
                return resolve({
                    .input_id = snapshot.input_id,
                    .operation = game_client::interaction_operation::cancel,
                });
        }
    });
    menu.query();
    CHECK(offered);
    REQUIRE(labels.size() == 3);
    CHECK(labels[0].size() == 3);
    CHECK(filters[0].empty());
    CHECK(labels[1] == std::vector<std::string>{"Teleport", "Teleport to overmap"});
    CHECK(filters[1] == "tele");
    CHECK(labels[2].size() == 3);
    CHECK(filters[2].empty());
}

TEST_CASE(
    "an invoke with text for another affordance leaves a uilist's filter alone",
    "[client][interaction][mcp]") {
    const auto guard = interaction_test_guard{};
    auto menu = uilist{};
    filter_menu(menu);
    auto reads = 0;
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        if (reads++ == 0) { return invoke_event("bn:other:list:choices", {.text = "tele"}); }
        return resolve({
            .input_id = game_client::current_interaction().input_id,
            .operation = game_client::interaction_operation::cancel,
        });
    });
    menu.query();
    CHECK(menu.as_interaction().choices.size() == 3);
    CHECK(menu.as_interaction().filter.empty());
}

TEST_CASE("semantic replay repeats a uilist filter invoke", "[client][interaction][replay][mcp]") {
    const auto guard = interaction_test_guard{};
    static auto counter = std::atomic_uint64_t{0};
    const auto directory =
        std::filesystem::path(PATH_INFO::user_dir())
        / ("list-filter-replay-" + std::to_string(counter.fetch_add(1)));
    REQUIRE(std::filesystem::create_directory(directory));
    const auto cleanup = on_out_of_scope([&] {
        replay::stop();
        std::filesystem::remove_all(directory);
    });
    const auto path = directory / "input.jsonl";

    auto reads = 0;
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        if (reads++ == 0) { return invoke_event("bn:filter:list:choices", {.text = "tele"}); }
        return resolve({
            .input_id = game_client::current_interaction().input_id,
            .operation = game_client::interaction_operation::cancel,
        });
    });
    replay::configure_recording(path.string(), {.rng_seed = 29});
    replay::start();
    {
        auto recorded = uilist{};
        filter_menu(recorded);
        recorded.query();
        CHECK(recorded.as_interaction().choices.size() == 2);
    }
    replay::finish();

    game_client::memory::set_input_provider([](const int /*timeout*/) -> input_event {
        throw std::runtime_error("playback must not read live input");
    });
    replay::configure_playback(path.string(), {.rng_seed = 29});
    replay::start();
    auto played = uilist{};
    filter_menu(played);
    played.query();
    CHECK(played.as_interaction().choices.size() == 2);
    CHECK_THROWS_AS(replay::next_input_event(), replay::completed);
    replay::finish();
}

namespace {

/// A callback that tests two raw keys itself, like the debug item spawner.
class key_recorder: public uilist_callback {
public:
    std::vector<int> pressed;
    auto keys() const -> std::vector<uilist_key> override {
        return {{.key = 'a', .name = "Add a flag"}, {.key = 'c', .name = "Choose a container"}};
    }
    auto key(const input_context&, const input_event& event, int, uilist*) -> bool override {
        pressed.push_back(event.get_first_input());
        return true;
    }
};

auto has_affordance(const nlohmann::json& screen, const std::string& id) -> bool {
    return std::ranges::any_of(screen.at("affordances"), [&](const nlohmann::json& offered) {
        return offered.at("id").get<std::string>() == id;
    });
}

} // namespace

TEST_CASE(
    "a uilist's callback keys are screen affordances, not list rows",
    "[client][interaction][mcp]") {
    const auto guard = interaction_test_guard{};
    auto menu = uilist{};
    filter_menu(menu);
    auto recorder = key_recorder{};
    menu.callback = &recorder;
    auto reads = 0;
    auto rows_seen = std::vector<std::string>{};
    auto offered = false;
    auto labelled = false;
    auto keyed = false;
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto snapshot = game_client::current_interaction();
        const auto screen = screen_wire();
        if (reads++ == 0) {
            rows_seen =
                snapshot.choices | std::views::transform(&game_client::interaction_choice::id)
                | std::ranges::to<std::vector>();
            offered = has_affordance(screen, "bn:key:97") && has_affordance(screen, "bn:key:99");
            for (const auto& entry : screen.at("affordances")) {
                if (entry.at("id") == "bn:key:99") {
                    labelled = entry.at("label") == "Choose a container";
                    keyed = entry.at("keys") == nlohmann::json::array({"c"});
                }
            }
            return invoke_event("bn:key:99");
        }
        return resolve({
            .input_id = snapshot.input_id,
            .operation = game_client::interaction_operation::cancel,
        });
    });
    menu.query();
    CHECK(rows_seen == std::vector<std::string>{"entry:0", "entry:1", "entry:2"});
    CHECK(offered);
    CHECK(labelled);
    CHECK(keyed);
    CHECK(recorder.pressed == std::vector<int>{'c'});
}

TEST_CASE("semantic replay repeats a uilist key invoke", "[client][interaction][replay][mcp]") {
    const auto guard = interaction_test_guard{};
    static auto counter = std::atomic_uint64_t{0};
    const auto directory =
        std::filesystem::path(PATH_INFO::user_dir())
        / ("list-key-replay-" + std::to_string(counter.fetch_add(1)));
    REQUIRE(std::filesystem::create_directory(directory));
    const auto cleanup = on_out_of_scope([&] {
        replay::stop();
        std::filesystem::remove_all(directory);
    });
    const auto path = directory / "input.jsonl";

    auto reads = 0;
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        if (reads++ == 0) { return invoke_event("bn:key:97"); }
        return resolve({
            .input_id = game_client::current_interaction().input_id,
            .operation = game_client::interaction_operation::cancel,
        });
    });
    replay::configure_recording(path.string(), {.rng_seed = 31});
    replay::start();
    {
        auto recorded = uilist{};
        filter_menu(recorded);
        auto recorder = key_recorder{};
        recorded.callback = &recorder;
        recorded.query();
        CHECK(recorder.pressed == std::vector<int>{'a'});
    }
    replay::finish();

    game_client::memory::set_input_provider([](const int /*timeout*/) -> input_event {
        throw std::runtime_error("playback must not read live input");
    });
    replay::configure_playback(path.string(), {.rng_seed = 31});
    replay::start();
    auto played = uilist{};
    filter_menu(played);
    auto recorder = key_recorder{};
    played.callback = &recorder;
    played.query();
    CHECK(recorder.pressed == std::vector<int>{'a'});
    CHECK_THROWS_AS(replay::next_input_event(), replay::completed);
    replay::finish();
}

TEST_CASE(
    "a menu entry's second column reaches the client as a column of its row",
    "[client][interaction][mcp]") {
    const auto guard = interaction_test_guard{};
    auto menu = uilist{};
    menu.entries.emplace_back(0, true, 'a', "Drop item", "", "<color_light_red>25 moves</color>");
    menu.entries.emplace_back(1, true, 'b', "Wear item", "", "");
    menu.filterlist();
    const auto choices = menu.as_interaction().choices;
    REQUIRE(choices.size() == 2);
    REQUIRE(choices[0].columns.size() == 1);
    CHECK(choices[0].columns[0].key == "ctxt");
    CHECK(choices[0].columns[0].value == "25 moves");
    CHECK(choices[0].columns[0].color == string_from_color(c_light_red));
    CHECK(choices[1].columns.empty());
}

TEST_CASE(
    "the unwield prompt names each way plainly with its moves in a column",
    "[client][interaction][mcp]") {
    auto& you = get_avatar();
    // Before the guard turns test mode off: clearing eats, which asks.
    clear_character(you, false);
    const auto guard = interaction_test_guard{};
    you.wield(item::spawn("glock_19", calendar::start_of_cataclysm, 0));
    auto choices = std::vector<game_client::interaction_choice>{};
    auto reads = 0;
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        REQUIRE(reads++ == 0);
        const auto& asked = game_client::current_interaction();
        choices = asked.choices;
        const auto way =
            std::ranges::find(choices, true, &game_client::interaction_choice::selectable);
        REQUIRE(way != choices.end());
        return resolve({
            .input_id = asked.input_id,
            .operation = game_client::interaction_operation::choose,
            .target_id = way->id,
        });
    });
    CHECK(you.unwield());
    REQUIRE_FALSE(choices.empty());
    CHECK(choices[0].label == "Store in inventory");
    for (const auto& choice : choices) {
        CAPTURE(choice.label);
        CHECK(choice.label.find('|') == std::string::npos);
        CHECK(choice.label == trim(choice.label));
    }
    // A way the avatar can take shows its moves; one it cannot has none to show.
    for (const auto& choice : choices) {
        CAPTURE(choice.label);
        REQUIRE(choice.columns.size() == (choice.selectable ? 1 : 0));
        if (choice.selectable) { CHECK(choice.columns[0].value.ends_with(" moves")); }
    }
}

#endif // CATA_MCP
