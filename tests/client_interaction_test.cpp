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
#    include "engine_client_component.h"
#    include "engine_client_contract.h"
#    include "engine_client_event.h"
#    include "engine_client_session.h"
#    include "engine_client_world.h"
#    include "faction.h"
#    include "game.h"
#    include "game_session.h"
#    include "input.h"
#    include "inventory_ui.h"
#    include "item.h"
#    include "json.h"
#    include "magic/magic.h"
#    include "magic/magic_teleporter_list.h"
#    include "map/map.h"
#    include "map_helpers.h"
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
#    include "string_editor_window.h"
#    include "string_input_popup.h"
#    include "string_utils.h"
#    include "ui.h"
#    include "uistate.h"
#    include "units_utility.h"
#    include "vehicle/veh_interact.h"
#    include "vehicle/veh_type.h"
#    include "vehicle/vehicle.h"

#    include <atomic>
#    include <filesystem>
#    include <functional>
#    include <nlohmann/json.hpp>
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

/// The crafting menu lists its tabs ahead of the recipes; most checks only look at the recipes.
auto recipe_rows(game_client::interaction_snapshot snapshot) -> game_client::interaction_snapshot {
    std::erase_if(snapshot.choices, [](const auto& choice) {
        return choice.id.starts_with("tab:") || choice.id.starts_with("subtab:");
    });
    return snapshot;
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
        for (const auto& row : list["rows"]) {
            if (!row.value("header", false)) { result.push_back(row); }
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

auto add_ground_item(const itype_id& id, const int charges = -1) -> item& { // *NOPAD*
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
                find(snapshot.choices, "YES", &game_client::interaction_choice::description);
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
            CHECK(snapshot.choices.front().description == "ANY_INPUT");
            CHECK(snapshot.choices.front().label == "Press any key");
            CHECK(snapshot.any_key_choice_id == snapshot.choices.front().id);
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
            CHECK(snapshot.choices.front().description == "QUIT");
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
    "the multi-line text editor exposes its text as a field and takes fill and cancel",
    "[client][interaction][mcp]") {
    clear_all_state();
    const auto cleanup = on_out_of_scope([]() { clear_all_state(); });
    const auto guard = interaction_test_guard{};
    auto reads = 0;
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto snapshot = game_client::current_interaction();
        REQUIRE(snapshot.kind == game_client::interaction_kind::field);
        REQUIRE(snapshot.field);
        const auto fill = [&](const std::string& value, const bool submit) {
            return resolve({
                .input_id = snapshot.input_id,
                .operation = game_client::interaction_operation::fill,
                .target_id = snapshot.field->id,
                .value = value,
                .submit = submit,
            });
        };
        if (reads++ == 0) {
            CHECK(snapshot.field->value == "seed");
            return fill("print(1); print(2)", false);
        }
        CHECK(snapshot.field->value == "print(1); print(2)");
        return fill("print(3)", true);
    });
    auto editor =
        string_editor_window{[] { return catacurses::newwin(10, 40, point_zero); }, "seed"};
    const auto [confirmed, text] = editor.query_string();
    CHECK(confirmed);
    CHECK(text == "print(3)");
    CHECK(reads == 2);

    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto snapshot = game_client::current_interaction();
        return resolve({
            .input_id = snapshot.input_id,
            .operation = game_client::interaction_operation::cancel,
        });
    });
    auto canceled =
        string_editor_window{[] { return catacurses::newwin(10, 40, point_zero); }, "kept"};
    const auto [ok, kept] = canceled.query_string();
    CHECK_FALSE(ok);
    CHECK(kept == "kept");
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
    CHECK(panel["stats"][0]["unit"] == weight_units());
    CHECK(panel["stats"][1]["unit"] == volume_units_abbr());
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
                CHECK(child["parent_id"].get<std::string>().starts_with("pickup:"));
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
    "real target UI accepts semantic cursor movement and rejects range overflow",
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
    auto target_position = game_client::interaction_position{};
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto snapshot = game_client::current_interaction();
        REQUIRE(snapshot.kind == game_client::interaction_kind::target);
        REQUIRE(snapshot.target);
        if (reads++ == 0) {
            const auto expected_source = game_client::
                interaction_position{.x = source.x(), .y = source.y(), .z = source.z()};
            CHECK(snapshot.target->source == expected_source);
            const auto visible_candidate = std::ranges::find(
                snapshot.target->candidates,
                game_client::
                    interaction_position{.x = visible.x(), .y = visible.y(), .z = visible.z()},
                &game_client::interaction_target_candidate::position);
            CHECK(visible_candidate != snapshot.target->candidates.end());
            const auto hidden_candidate = std::ranges::find(
                snapshot.target->candidates,
                game_client::interaction_position{.x = hidden.x(), .y = hidden.y(), .z = hidden.z()},
                &game_client::interaction_target_candidate::position);
            CHECK(hidden_candidate == snapshot.target->candidates.end());
            target_position = {
                .x = source.x() + 1,
                .y = source.y(),
                .z = source.z(),
            };
            const auto outside = game_client::resolve_interaction_command({
                .input_id = snapshot.input_id,
                .operation = game_client::interaction_operation::set_target,
                .position =
                    game_client::interaction_position{
                        .x = source.x() + snapshot.target->range + 1,
                        .y = source.y(),
                        .z = source.z(),
                    },
            });
            REQUIRE_FALSE(outside);
            CHECK(outside.error().contains("outside the active range"));
            return resolve({
                .input_id = snapshot.input_id,
                .operation = game_client::interaction_operation::set_target,
                .position = target_position,
            });
        }
        CHECK(snapshot.target->cursor == target_position);
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
        const auto snapshot = game_client::current_interaction();
        if (reads++ == 0) {
            REQUIRE(snapshot.target);
            const auto expected_position = game_client::interaction_position{
                .x = npc_position.x(), .y = npc_position.y(), .z = npc_position.z()};
            const auto candidate = std::ranges::
                find(snapshot.target->candidates, expected_position,
                     &game_client::interaction_target_candidate::position);
            REQUIRE(candidate != snapshot.target->candidates.end());
            return resolve({
                .input_id = snapshot.input_id,
                .operation = game_client::interaction_operation::set_target,
                .target_id = candidate->id,
                .position = candidate->position,
            });
        }
        if (reads == 2) {
            REQUIRE(snapshot.kind == game_client::interaction_kind::target);
            return resolve_action("FIRE");
        }
        if (reads == 3) {
            REQUIRE(snapshot.kind == game_client::interaction_kind::choices);
            CHECK(snapshot.message.contains("Really attack"));
            const auto no = std::ranges::
                find(snapshot.choices, "NO", &game_client::interaction_choice::description);
            REQUIRE(no != snapshot.choices.end());
            return resolve({
                .input_id = snapshot.input_id,
                .operation = game_client::interaction_operation::choose,
                .target_id = no->id,
            });
        }
        REQUIRE(snapshot.kind == game_client::interaction_kind::target);
        return resolve({
            .input_id = snapshot.input_id,
            .operation = game_client::interaction_operation::cancel,
        });
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
TEST_CASE(
    "semantic construction choice preserves native placement and resource handling",
    "[client][interaction][construction][mcp]") {
    clear_all_state();
    const auto cleanup = on_out_of_scope([]() { clear_all_state(); });
    const auto guard = interaction_test_guard{};
    const auto restore_turn = restore_on_out_of_scope<decltype(calendar::turn)>(calendar::turn);
    const auto restore_filter = restore_on_out_of_scope<decltype(uistate.construction_filter)>(
        uistate.construction_filter);
    const auto restore_tab = restore_on_out_of_scope<decltype(uistate.construction_tab)>(
        uistate.construction_tab);
    const auto restore_last = restore_on_out_of_scope<decltype(uistate.last_construction)>(
        uistate.last_construction);
    calendar::turn = calendar::turn_zero + 12_hours;
    g->reset_light_level();
    uistate.construction_filter.clear();
    build_test_map(ter_id("t_floor"));
    auto& you = get_avatar();
    add_crafting_fixture_item(you, itype_id("e_scrap"), 2);
    const auto origin = you.bub_pos();
    const auto destination = origin + tripoint_east;
    clear_construction_fixture(destination);
    const auto cleanup_construction = on_out_of_scope(
        std::bind_front(clear_construction_fixture, destination));
    get_map().invalidate_map_cache(you.bub_pos().z());
    get_map().build_map_cache(you.bub_pos().z());
    get_map().update_visibility_cache(you.bub_pos().z());
    REQUIRE(character_funcs::can_see_fine_details(you));
    auto step = 0;

    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto snapshot = game_client::current_interaction();
        if (snapshot.context == "CONSTRUCTION") {
            if (step == 0) {
                step = 1;
                return resolve_action("FILTER");
            }
            REQUIRE(step == 2);
            const auto choice = std::ranges::
                find(snapshot.choices, "TEST semantic construction",
                     &game_client::interaction_choice::label);
            REQUIRE(choice != snapshot.choices.end());
            CHECK(choice->enabled);
            CHECK(choice->selectable);
            step = 3;
            return resolve({
                .input_id = snapshot.input_id,
                .operation = game_client::interaction_operation::choose,
                .target_id = choice->id,
            });
        }
        if (snapshot.context != "DEFAULTMODE") {
            REQUIRE(step == 1);
            REQUIRE(snapshot.field);
            step = 2;
            return resolve({
                .input_id = snapshot.input_id,
                .operation = game_client::interaction_operation::fill,
                .target_id = snapshot.field->id,
                .value = "TEST semantic construction",
                .submit = true,
            });
        }
        REQUIRE(step == 3);
        const auto east =
            std::ranges::find(snapshot.choices, "East", &game_client::interaction_choice::label);
        REQUIRE(east != snapshot.choices.end());
        step = 4;
        return resolve({
            .input_id = snapshot.input_id,
            .operation = game_client::interaction_operation::choose,
            .target_id = east->id,
        });
    });

    CHECK_FALSE(construction_menu(false));
    CHECK(step == 4);
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
    "semantic construction darkness denial retains resources without placement",
    "[client][interaction][construction][mcp]") {
    const auto restore_light = on_out_of_scope([]() {
        g->reset_light_level();
        get_map().invalidate_map_cache(get_avatar().bub_pos().z());
    });
    const auto restore_turn = restore_on_out_of_scope<decltype(calendar::turn)>(calendar::turn);
    clear_all_state();
    const auto cleanup = on_out_of_scope([]() { clear_all_state(); });
    const auto guard = interaction_test_guard{};
    const auto restore_filter = restore_on_out_of_scope<decltype(uistate.construction_filter)>(
        uistate.construction_filter);
    const auto restore_tab = restore_on_out_of_scope<decltype(uistate.construction_tab)>(
        uistate.construction_tab);
    const auto restore_last = restore_on_out_of_scope<decltype(uistate.last_construction)>(
        uistate.last_construction);
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
    REQUIRE(can_construct(con, destination));
    REQUIRE(player_can_build(you, you.crafting_inventory(), con));
    const auto materials_before = you.charges_of(itype_id("e_scrap"));
    REQUIRE(materials_before == 2);
    auto reads = 0;
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        ++reads;
        const auto snapshot = game_client::current_interaction();
        REQUIRE(snapshot.context == "CONSTRUCTION");
        REQUIRE(reads == 1);
        const auto choice = std::ranges::
            find(snapshot.choices, "TEST semantic construction",
                 &game_client::interaction_choice::label);
        REQUIRE(choice != snapshot.choices.end());
        REQUIRE(choice->enabled);
        return resolve({
            .input_id = snapshot.input_id,
            .operation = game_client::interaction_operation::choose,
            .target_id = choice->id,
        });
    });
    CHECK_FALSE(construction_menu(false));
    CHECK(reads == 1);
    CHECK(you.charges_of(itype_id("e_scrap")) == materials_before);
    CHECK_FALSE(you.activity);
    CHECK(get_map().partial_con_at(destination) == nullptr);
    CHECK(get_map().furn(destination) == furn_id("f_null"));
}
TEST_CASE(
    "semantic construction denial and blueprint choice do not bypass native rules",
    "[client][interaction][construction][mcp]") {
    clear_all_state();
    const auto cleanup = on_out_of_scope([]() { clear_all_state(); });
    const auto guard = interaction_test_guard{};
    const auto restore_filter = restore_on_out_of_scope<decltype(uistate.construction_filter)>(
        uistate.construction_filter);
    const auto restore_tab = restore_on_out_of_scope<decltype(uistate.construction_tab)>(
        uistate.construction_tab);
    const auto restore_last = restore_on_out_of_scope<decltype(uistate.last_construction)>(
        uistate.last_construction);
    uistate.construction_filter.clear();
    build_test_map(ter_id("t_floor"));
    auto& you = get_avatar();
    const auto material_before = you.amount_of(itype_id("test_1kg_cube"));

    SECTION("native denial") {
        auto step = 0;
        game_client::memory::set_input_provider([&](const int /*timeout*/) {
            const auto snapshot = game_client::current_interaction();
            if (snapshot.context == "CONSTRUCTION") {
                if (step == 0) {
                    step = 1;
                    return resolve_action("FILTER");
                }
                if (step == 2 && recipe_rows(snapshot).choices.empty()) {
                    return resolve_action("TOGGLE_UNAVAILABLE_CONSTRUCTIONS");
                }
                if (step == 2) {
                    const auto rows = recipe_rows(snapshot).choices;
                    REQUIRE(rows.size() == 1);
                    const auto& choice = rows.front();
                    CHECK_FALSE(choice.enabled);
                    CHECK(choice.selectable);
                    CHECK_FALSE(choice.denial.empty());
                    step = 3;
                    return resolve({
                        .input_id = snapshot.input_id,
                        .operation = game_client::interaction_operation::choose,
                        .target_id = choice.id,
                    });
                }
                REQUIRE(step == 4);
                step = 5;
                return resolve({
                    .input_id = snapshot.input_id,
                    .operation = game_client::interaction_operation::cancel,
                });
            }
            if (step == 1) {
                REQUIRE(snapshot.field);
                step = 2;
                return resolve({
                    .input_id = snapshot.input_id,
                    .operation = game_client::interaction_operation::fill,
                    .target_id = snapshot.field->id,
                    .value = "TEST semantic unavailable construction",
                    .submit = true,
                });
            }
            REQUIRE(step == 3);
            CHECK(snapshot.message.contains("can't build"));
            step = 4;
            return resolve({
                .input_id = snapshot.input_id,
                .operation = game_client::interaction_operation::choose,
                .target_id = recipe_rows(snapshot).choices.front().id,
            });
        });
        CHECK_FALSE(construction_menu(false));
        CHECK(step == 5);
        CHECK_FALSE(you.activity);
        CHECK(you.amount_of(itype_id("test_1kg_cube")) == material_before);
    }

    SECTION("blueprint ignores material availability") {
        uistate.construction_filter = "TEST semantic unavailable construction";
        game_client::memory::set_input_provider([&](const int /*timeout*/) {
            const auto snapshot = game_client::current_interaction();
            const auto rows = recipe_rows(snapshot).choices;
            if (rows.empty()) { return resolve_action("TOGGLE_UNAVAILABLE_CONSTRUCTIONS"); }
            REQUIRE(snapshot.context == "CONSTRUCTION");
            REQUIRE(rows.size() == 1);
            CHECK(rows.front().enabled);
            return resolve({
                .input_id = snapshot.input_id,
                .operation = game_client::interaction_operation::choose,
                .target_id = recipe_rows(snapshot).choices.front().id,
            });
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
    "construction menu publishes category tabs, search and requirements",
    "[client][interaction][construction][mcp]") {
    clear_all_state();
    const auto cleanup = on_out_of_scope([]() { clear_all_state(); });
    const auto guard = interaction_test_guard{};
    const auto restore_filter = restore_on_out_of_scope<decltype(uistate.construction_filter)>(
        uistate.construction_filter);
    const auto restore_tab = restore_on_out_of_scope<decltype(uistate.construction_tab)>(
        uistate.construction_tab);
    uistate.construction_filter.clear();
    build_test_map(ter_id("t_floor"));
    add_crafting_fixture_item(get_avatar(), itype_id("e_scrap"), 2);
    get_avatar().invalidate_crafting_inventory();
    const auto choose = [](const game_client::interaction_snapshot& snapshot, const auto& id) {
        return resolve({
            .input_id = snapshot.input_id,
            .operation = game_client::interaction_operation::choose,
            .target_id = id,
        });
    };
    const auto tabs = [](const game_client::interaction_snapshot& snapshot) {
        return snapshot.choices
             | std::views::filter([](const auto& choice) { return choice.id.starts_with("tab:"); })
             | std::ranges::to<std::vector>();
    };
    auto step = 0;
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto snapshot = game_client::current_interaction({.limit = 200});
        REQUIRE(snapshot.context == "CONSTRUCTION");
        const auto rows = recipe_rows(snapshot).choices;
        const auto mine = std::ranges::
            find(rows, "TEST semantic construction", &game_client::interaction_choice::label);
        switch (step++) {
            case 0: {
                const auto all_tabs = tabs(snapshot);
                CHECK(all_tabs.size() > 2);
                CHECK(std::ranges::count_if(all_tabs, &game_client::interaction_choice::selected)
                      == 1);
                CHECK(snapshot.panes.size() == all_tabs.size());
                REQUIRE(snapshot.field);
                CHECK(snapshot.field->id == "filter");
                CHECK_FALSE(snapshot.field->description.empty());
                CHECK_FALSE(snapshot.field->description.contains("<color"));
                REQUIRE(
                    std::ranges::find(all_tabs, "tab:OTHER", &game_client::interaction_choice::id)
                    != all_tabs.end());
                return choose(snapshot, "tab:OTHER");
            }
            case 1: {
                const auto selected =
                    tabs(snapshot)
                    | std::views::filter([](const auto& choice) { return choice.selected; })
                    | std::ranges::to<std::vector>();
                REQUIRE(selected.size() == 1);
                CHECK(selected.front().id == "tab:OTHER");
                REQUIRE(mine != rows.end());
                // Requirements arrive with the row as plain text: time, terrain, components.
                CHECK(mine->description.contains("Time to complete"));
                CHECK(mine->description.contains("Requires"));
                CHECK(mine->description.contains("scrap"));
                CHECK_FALSE(mine->description.contains("<color"));
                return resolve({
                    .input_id = snapshot.input_id,
                    .operation = game_client::interaction_operation::fill,
                    .target_id = "filter",
                    .value = "no such construction",
                    .submit = true,
                });
            }
            case 2: {
                // A search replaces the category tabs by one, as natively.
                CHECK(rows.empty());
                REQUIRE(tabs(snapshot).size() == 1);
                CHECK(tabs(snapshot).front().id == "tab:FILTER");
                REQUIRE(snapshot.field);
                CHECK(snapshot.field->value == "no such construction");
                return resolve({
                    .input_id = snapshot.input_id,
                    .operation = game_client::interaction_operation::fill,
                    .target_id = "filter",
                    .value = "",
                    .submit = true,
                });
            }
            default: {
                CHECK(tabs(snapshot).size() > 2);
                return resolve({
                    .input_id = snapshot.input_id,
                    .operation = game_client::interaction_operation::cancel,
                });
            }
        }
    });
    CHECK_FALSE(construction_menu(false));
    CHECK(step == 4);
}
TEST_CASE(
    "semantic replay repeats native construction placement",
    "[client][interaction][construction][replay][mcp]") {
    clear_all_state();
    const auto cleanup_state = on_out_of_scope([]() { clear_all_state(); });
    const auto guard = interaction_test_guard{};
    const auto restore_turn = restore_on_out_of_scope<decltype(calendar::turn)>(calendar::turn);
    const auto restore_filter = restore_on_out_of_scope<decltype(uistate.construction_filter)>(
        uistate.construction_filter);
    const auto restore_tab = restore_on_out_of_scope<decltype(uistate.construction_tab)>(
        uistate.construction_tab);
    const auto restore_last = restore_on_out_of_scope<decltype(uistate.last_construction)>(
        uistate.last_construction);
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

    const auto prepare = [&]() {
        calendar::turn = calendar::turn_zero + 12_hours;
        g->reset_light_level();
        uistate.construction_filter = "TEST semantic construction";
        build_test_map(ter_id("t_floor"));
        auto& you = get_avatar();
        const auto destination = you.bub_pos() + tripoint_east;
        clear_construction_fixture(destination);
        get_map().invalidate_map_cache(you.bub_pos().z());
        get_map().build_map_cache(you.bub_pos().z());
        get_map().update_visibility_cache(you.bub_pos().z());
        REQUIRE(character_funcs::can_see_fine_details(you));
        add_crafting_fixture_item(you, itype_id("e_scrap"), 2);
        return destination;
    };
    auto recorded_reads = 0;
    const auto semantic_provider = [&](const int /*timeout*/) {
        ++recorded_reads;
        const auto snapshot = game_client::current_interaction();
        if (snapshot.context == "CONSTRUCTION") {
            const auto choice = std::ranges::
                find(snapshot.choices, "TEST semantic construction",
                     &game_client::interaction_choice::label);
            REQUIRE(choice != snapshot.choices.end());
            return resolve({
                .input_id = snapshot.input_id,
                .operation = game_client::interaction_operation::choose,
                .target_id = choice->id,
            });
        }
        REQUIRE(snapshot.context == "DEFAULTMODE");
        const auto east =
            std::ranges::find(snapshot.choices, "East", &game_client::interaction_choice::label);
        REQUIRE(east != snapshot.choices.end());
        return resolve({
            .input_id = snapshot.input_id,
            .operation = game_client::interaction_operation::choose,
            .target_id = east->id,
        });
    };

    const auto recorded_destination = prepare();
    const auto cleanup_construction = on_out_of_scope(
        std::bind_front(clear_construction_fixture, recorded_destination));
    replay::configure_recording(path.string(), {.rng_seed = 37});
    replay::start();
    game_client::memory::set_input_provider(semantic_provider);
    CHECK_FALSE(construction_menu(false));
    CHECK(recorded_reads == 2);
    REQUIRE(get_avatar().activity);
    CHECK(get_avatar().activity->id() == activity_id("ACT_BUILD"));
    CHECK(get_avatar().amount_of(itype_id("e_scrap")) == 0);
    CHECK(get_map().partial_con_at(recorded_destination) != nullptr);
    finish_construction_activity(get_avatar());
    CHECK_FALSE(get_avatar().activity);
    CHECK(get_map().partial_con_at(recorded_destination) == nullptr);
    CHECK(get_map().furn(recorded_destination) == furn_id("f_test_semantic_construction"));
    replay::finish();

    game_client::memory::set_input_provider([](const int /*timeout*/) {
        return resolve_action("YES");
    });
    clear_all_state();
    const auto played_destination = prepare();
    game_client::memory::set_input_provider([](const int /*timeout*/) -> input_event {
        throw std::runtime_error("construction playback must not read live input");
    });
    replay::configure_playback(path.string(), {.rng_seed = 37});
    replay::start();
    CHECK_FALSE(construction_menu(false));
    REQUIRE(get_avatar().activity);
    CHECK(get_avatar().activity->id() == activity_id("ACT_BUILD"));
    CHECK(get_avatar().amount_of(itype_id("e_scrap")) == 0);
    CHECK(get_map().partial_con_at(played_destination) != nullptr);
    finish_construction_activity(get_avatar());
    CHECK_FALSE(get_avatar().activity);
    CHECK(get_map().partial_con_at(played_destination) == nullptr);
    CHECK(get_map().furn(played_destination) == furn_id("f_test_semantic_construction"));
    CHECK_THROWS_AS(replay::next_input_event(), replay::completed);
    replay::finish();
}
TEST_CASE(
    "semantic crafting batch entry preserves native selection bookkeeping",
    "[client][interaction][crafting][mcp]") {
    clear_all_state();
    const auto cleanup = on_out_of_scope([]() { clear_all_state(); });
    const auto guard = interaction_test_guard{};
    const auto highlight_unread = override_option("HIGHLIGHT_UNREAD_RECIPES", "true");
    const auto restore_read = restore_on_out_of_scope<decltype(uistate.read_recipes)>(
        uistate.read_recipes);
    const auto restore_expanded = restore_on_out_of_scope<decltype(uistate.expanded_recipes)>(
        uistate.expanded_recipes);
    uistate.read_recipes.clear();
    auto& you = get_avatar();
    const auto& pipe = recipe_id("test_pipe").obj();
    prepare_crafting_fixture();

    auto step = 0;
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto snapshot = recipe_rows(game_client::current_interaction({.limit = 100}));
        if (snapshot.context != "CRAFTING") {
            REQUIRE(step == 1);
            step = 2;
            return resolve({
                .input_id = snapshot.input_id,
                .operation = game_client::interaction_operation::fill,
                .target_id = snapshot.field->id,
                .value = "TEST",
                .submit = true,
            });
        }
        if (step == 0) {
            step = 1;
            return resolve_action("FILTER");
        }
        if (step == 2) {
            const auto target = std::ranges::find_if(snapshot.choices, [&](const auto& choice) {
                return choice.label.find(pipe.result_name()) != std::string::npos;
            });
            REQUIRE(target != snapshot.choices.end());
            CHECK(std::distance(snapshot.choices.begin(), target) > 0);
            CHECK(uistate.read_recipes.count(pipe.ident()) == 0);
            step = 3;
            return resolve({
                .input_id = snapshot.input_id,
                .operation = game_client::interaction_operation::set_count,
                .target_id = target->id,
                .count = 3,
            });
        }
        if (step == 3) {
            const auto selected = std::ranges::
                find(snapshot.choices, true, &game_client::interaction_choice::highlighted);
            REQUIRE(selected != snapshot.choices.end());
            CHECK(selected->selected_count == 3);
            CHECK(uistate.read_recipes.count(pipe.ident()) == 1);
            const auto five =
                std::ranges::find(snapshot.choices, std::uint64_t{5}, [](const auto& choice) {
                    return choice.selected_count.value_or(0);
                });
            REQUIRE(five != snapshot.choices.end());
            step = 4;
            return resolve({
                .input_id = snapshot.input_id,
                .operation = game_client::interaction_operation::set_count,
                .target_id = five->id,
                .count = 5,
            });
        }
        if (step == 4) {
            const auto selected = std::ranges::
                find(snapshot.choices, true, &game_client::interaction_choice::highlighted);
            REQUIRE(selected != snapshot.choices.end());
            CHECK(selected->selected_count == 5);
            CHECK(snapshot.message.contains("Batch"));
            step = 5;
            return resolve_action("CYCLE_BATCH");
        }
        REQUIRE(step == 5);
        const auto selected = std::ranges::
            find(snapshot.choices, true, &game_client::interaction_choice::highlighted);
        REQUIRE(selected != snapshot.choices.end());
        CHECK(selected->label.find(pipe.result_name()) != std::string::npos);
        CHECK_FALSE(snapshot.message.contains("Batch"));
        step = 6;
        return resolve({
            .input_id = snapshot.input_id,
            .operation = game_client::interaction_operation::cancel,
        });
    });

    you.craft(you.bub_pos());
    CHECK(step == 6);
    CHECK_FALSE(you.activity);
    CHECK(uistate.read_recipes.count(pipe.ident()) == 1);
}
TEST_CASE(
    "the overmap is a structured interaction with a cursor that a client can move",
    "[client][interaction][overmap][mcp]") {
    clear_all_state();
    const auto cleanup = on_out_of_scope([]() { clear_all_state(); });
    const auto guard = interaction_test_guard{};
    const auto player = get_avatar().abs_omt_pos();
    // The native screen blinks its overlays; a snapshot taken while they are on shows the marker.
    const auto restore_overlays = restore_on_out_of_scope<bool>(uistate.overmap_show_overlays);
    uistate.overmap_show_overlays = true;
    auto step = 0;
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto snapshot = game_client::current_interaction();
        REQUIRE(snapshot.context == "OVERMAP");
        REQUIRE(snapshot.kind == game_client::interaction_kind::target);
        REQUIRE(snapshot.overmap);
        REQUIRE(snapshot.target);
        const auto& om = *snapshot.overmap;
        CHECK(snapshot.target->coordinate_space == "omt");
        CHECK(om.cols > 20);
        CHECK(om.rows > 10);
        CHECK(om.glyphs.size() == static_cast<std::size_t>(om.cols * om.rows));
        CHECK(om.foreground.size() == om.glyphs.size());
        CHECK(om.player == game_client::interaction_position{player.x(), player.y(), player.z()});
        CHECK_FALSE(om.legend.empty());
        // The cursor square is described in plain words, not as the sidebar's framed text.
        CHECK_FALSE(om.cursor_description.empty());
        CHECK_FALSE(om.cursor_description.contains("│"));
        INFO(om.cursor_description);
        CHECK(std::ranges::any_of(om.legend, [&](const auto& line) {
            return line.contains(om.cursor_description.substr(0, om.cursor_description.find(' ')));
        }));
        const auto cursor = snapshot.target->cursor;
        // The window is centered on the cursor, as on the native screen.
        CHECK(om.origin.x == cursor.x - om.cols / 2);
        CHECK(om.origin.y == cursor.y - om.rows / 2);
        if (step++ == 0) {
            CHECK(cursor == om.player);
            // The player's own square shows the player marker.
            const auto at = (om.rows / 2) * om.cols + om.cols / 2;
            CHECK(om.glyphs[static_cast<std::size_t>(at)] == "@");
            return resolve({
                .input_id = snapshot.input_id,
                .operation = game_client::interaction_operation::set_target,
                .position = game_client::interaction_position{cursor.x + 3, cursor.y - 2, cursor.z},
            });
        }
        if (step == 2) {
            CHECK(cursor
                  == game_client::interaction_position{player.x() + 3, player.y() - 2, player.z()});
            // The player marker moved off the center with the view.
            CHECK(
                om.glyphs[static_cast<std::size_t>((om.rows / 2) * om.cols + om.cols / 2)] != "@");
            // Positions beyond the safe bounds are refused.
            CHECK_FALSE(game_client::resolve_interaction_command({
                .input_id = snapshot.input_id,
                .operation = game_client::interaction_operation::set_target,
                .position = game_client::interaction_position{1 << 30, 0, 0},
            }));
        }
        return resolve({
            .input_id = snapshot.input_id,
            .operation = game_client::interaction_operation::cancel,
        });
    });
    ui::omap::display();
    CHECK(step == 2);
}
TEST_CASE(
    "the overmap carries tile ids only for a client that renders tiles, and the travel route",
    "[client][interaction][overmap][mcp]") {
    clear_all_state();
    const auto cleanup = on_out_of_scope([]() {
        game_client::set_overmap_tiles(false);
        clear_all_state();
    });
    const auto guard = interaction_test_guard{};
    auto& you = get_avatar();
    const auto here = you.abs_omt_pos();
    you.omt_path = {here + point_east, here + point_east * 2};
    auto reads = 0;
    auto tiles = false;
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto snapshot = game_client::current_interaction();
        REQUIRE(snapshot.overmap);
        const auto& om = *snapshot.overmap;
        ++reads;
        CHECK(om.path.size() == 2);
        CHECK(
            om.path.front() == game_client::interaction_position{here.x() + 1, here.y(), here.z()});
        if (!tiles) {
            CHECK(om.tiles.empty());
            CHECK(om.looks_like.empty());
        } else {
            REQUIRE(om.tiles.size() == om.glyphs.size());
            CHECK(std::ranges::none_of(om.tiles, [](const auto& tile) { return tile.id.empty(); }));
            // Squares the player has not seen are unknown terrain; the one under the player is
            // known.
            const auto center =
                om.tiles[static_cast<std::size_t>((om.rows / 2) * om.cols + om.cols / 2)];
            CHECK(center.id != "unknown_terrain");
            CHECK(om.looks_like.contains(center.id));
        }
        return resolve({
            .input_id = snapshot.input_id,
            .operation = game_client::interaction_operation::cancel,
        });
    });
    for (const auto wants_tiles : {false, true}) {
        tiles = wants_tiles;
        game_client::set_overmap_tiles(wants_tiles);
        ui::omap::display();
    }
    CHECK(reads == 2);
}
TEST_CASE(
    "the overmap says when the cursor is on the player and gives its note and the route length",
    "[client][interaction][overmap][mcp]") {
    clear_all_state();
    const auto cleanup = on_out_of_scope([]() { clear_all_state(); });
    const auto guard = interaction_test_guard{};
    const auto restore_notes = restore_on_out_of_scope<bool>(uistate.overmap_show_map_notes);
    uistate.overmap_show_map_notes = true;
    auto& you = get_avatar();
    const auto here = you.abs_omt_pos();
    const auto noted = here + point_east * 3;
    // A note with a glyph prefix: the corner shows only its text.
    ACTIVE_OVERMAP_BUFFER.add_note(noted, "C:cache");
    const auto remove_note = on_out_of_scope([noted]() {
        ACTIVE_OVERMAP_BUFFER.delete_note(noted);
    });
    const auto restore_path = restore_on_out_of_scope<std::vector<tripoint_abs_omt>>(you.omt_path);
    you.omt_path = {here + point_east, here + point_east * 2};
    auto reads = 0;
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto snapshot = game_client::current_interaction();
        REQUIRE(snapshot.overmap);
        const auto& om = *snapshot.overmap;
        CHECK(om.route_text == "Route of 2 squares.");
        if (reads++ == 0) {
            CHECK(om.cursor_here == "You are here.");
            CHECK(om.cursor_note.empty());
            return resolve({
                .input_id = snapshot.input_id,
                .operation = game_client::interaction_operation::set_target,
                .position = game_client::interaction_position{noted.x(), noted.y(), noted.z()},
            });
        }
        CHECK(om.cursor_here.empty());
        CHECK(om.cursor_note == "Note: cache");
        return resolve({
            .input_id = snapshot.input_id,
            .operation = game_client::interaction_operation::cancel,
        });
    });
    ui::omap::display();
    CHECK(reads == 2);
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
TEST_CASE(
    "semantic crafting publishes tabs, search and recipe details",
    "[client][interaction][crafting][mcp]") {
    clear_all_state();
    const auto cleanup = on_out_of_scope([]() { clear_all_state(); });
    const auto guard = interaction_test_guard{};
    const auto restore_read = restore_on_out_of_scope<decltype(uistate.read_recipes)>(
        uistate.read_recipes);
    const auto restore_expanded = restore_on_out_of_scope<decltype(uistate.expanded_recipes)>(
        uistate.expanded_recipes);
    auto& you = get_avatar();
    const auto& pipe = recipe_id("test_pipe").obj();
    prepare_crafting_fixture();

    const auto choose =
        [](const game_client::interaction_snapshot& snapshot, const std::string& id) {
            REQUIRE(
                std::ranges::contains(snapshot.choices, id, &game_client::interaction_choice::id));
            return resolve({
                .input_id = snapshot.input_id,
                .operation = game_client::interaction_operation::choose,
                .target_id = id,
            });
        };
    const auto selected =
        [](const game_client::interaction_snapshot& snapshot, const std::string& prefix) {
            return snapshot.choices | std::views::filter([&](const auto& choice) {
                       return choice.selected && choice.id.starts_with(prefix);
                   })
                 | std::views::transform(&game_client::interaction_choice::id)
                 | std::ranges::to<std::vector>();
        };
    const auto pipe_row = [&](const game_client::interaction_snapshot& snapshot) {
        return std::ranges::find_if(snapshot.choices, [&](const auto& choice) {
            return choice.id.starts_with("recipe:")
                && choice.label.find(pipe.result_name()) != std::string::npos;
        });
    };

    auto step = 0;
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto snapshot = game_client::current_interaction({.limit = 200});
        REQUIRE(snapshot.context == "CRAFTING");
        const auto recipes = recipe_rows(snapshot).choices;
        switch (step++) {
            case 0: {
                // Tab rows have no pane; exactly one is selected; the search is a field.
                CHECK(
                    std::ranges::count_if(
                        snapshot.choices,
                        [](const auto& choice) { return choice.id.starts_with("tab:"); })
                    > 1);
                CHECK(std::ranges::none_of(snapshot.choices, [](const auto& choice) {
                    return choice.id.starts_with("tab:") && choice.pane_id;
                }));
                CHECK(selected(snapshot, "tab:").size() == 1);
                REQUIRE(snapshot.field);
                CHECK(snapshot.field->id == "filter");
                CHECK(snapshot.field->value.empty());
                CHECK_FALSE(snapshot.field->description.empty());
                CHECK_FALSE(snapshot.field->description.contains("<color"));
                return choose(snapshot, "tab:CC_ELECTRONIC");
            }
            case 1: {
                CHECK(selected(snapshot, "tab:") == std::vector<std::string>{"tab:CC_ELECTRONIC"});
                CHECK(std::ranges::all_of(recipes, [](const auto& choice) {
                    return choice.pane_id == "CC_ELECTRONIC";
                }));
                // Details come with the row, without color markup.
                const auto row = pipe_row(snapshot);
                REQUIRE(row != snapshot.choices.end());
                CHECK(row->description.contains("Time to complete"));
                CHECK_FALSE(row->description.contains("<color"));
                return choose(snapshot, "subtab:CSC_ELECTRONIC_TOOLS");
            }
            case 2: {
                CHECK(selected(snapshot, "subtab:")
                      == std::vector<std::string>{"subtab:CSC_ELECTRONIC_TOOLS"});
                CHECK(pipe_row(snapshot) != snapshot.choices.end());
                return resolve({
                    .input_id = snapshot.input_id,
                    .operation = game_client::interaction_operation::fill,
                    .target_id = "filter",
                    .value = "no recipe is named like this",
                    .submit = true,
                });
            }
            case 3: {
                // A search lists across the tab and freezes the sub-tabs, as native does.
                CHECK(recipes.empty());
                CHECK(snapshot.field->value == "no recipe is named like this");
                CHECK(std::ranges::none_of(snapshot.choices, [](const auto& choice) {
                    return choice.id.starts_with("subtab:");
                }));
                return resolve({
                    .input_id = snapshot.input_id,
                    .operation = game_client::interaction_operation::fill,
                    .target_id = "filter",
                    .value = "",
                    .submit = true,
                });
            }
            default: {
                CHECK(pipe_row(snapshot) != snapshot.choices.end());
                return resolve({
                    .input_id = snapshot.input_id,
                    .operation = game_client::interaction_operation::cancel,
                });
            }
        }
    });

    you.craft(you.bub_pos());
    CHECK(step == 5);
    CHECK_FALSE(you.activity);
}
TEST_CASE(
    "semantic crafting cancel leaves normal and batch selections untouched",
    "[client][interaction][crafting][mcp]") {
    clear_all_state();
    const auto cleanup = on_out_of_scope([]() { clear_all_state(); });
    const auto guard = interaction_test_guard{};
    const auto restore_read = restore_on_out_of_scope<decltype(uistate.read_recipes)>(
        uistate.read_recipes);
    const auto restore_expanded = restore_on_out_of_scope<decltype(uistate.expanded_recipes)>(
        uistate.expanded_recipes);
    auto& you = get_avatar();
    const auto& recipe = recipe_id("test_soldering_iron").obj();
    prepare_crafting_fixture();
    auto& here = get_map();
    here.add_item_or_charges(you.bub_pos(), item::spawn("test_rock"));
    const auto e_scrap_before = you.amount_of(itype_id("e_scrap"));
    const auto ground_before = here.i_at(you.bub_pos()).size();

    const auto run_cancel = [&](const bool batch) {
        auto step = 0;
        game_client::memory::set_input_provider([&](const int /*timeout*/) {
            const auto snapshot = recipe_rows(game_client::current_interaction({.limit = 100}));
            if (snapshot.context != "CRAFTING") {
                REQUIRE(step == 1);
                step = 2;
                return resolve({
                    .input_id = snapshot.input_id,
                    .operation = game_client::interaction_operation::fill,
                    .target_id = snapshot.field->id,
                    .value = recipe.result_name(),
                    .submit = true,
                });
            }
            if (step == 0) {
                step = 1;
                return resolve_action("FILTER");
            }
            if (batch && step == 2) {
                REQUIRE(snapshot.choices.size() == 1);
                step = 3;
                return resolve({
                    .input_id = snapshot.input_id,
                    .operation = game_client::interaction_operation::set_count,
                    .target_id = snapshot.choices.front().id,
                    .count = 2,
                });
            }
            CHECK(step == (batch ? 3 : 2));
            step = 4;
            return resolve({
                .input_id = snapshot.input_id,
                .operation = game_client::interaction_operation::cancel,
            });
        });
        you.craft(you.bub_pos());
        CHECK(step == 4);
    };

    SECTION("normal") { run_cancel(false); }
    SECTION("batch") { run_cancel(true); }

    CHECK_FALSE(you.activity);
    CHECK(you.amount_of(itype_id("e_scrap")) == e_scrap_before);
    CHECK(you.amount_of(itype_id("test_soldering_iron")) == 0);
    CHECK(here.i_at(you.bub_pos()).size() == ground_before);
}
TEST_CASE(
    "semantic unavailable crafting choice follows native denial without mutation",
    "[client][interaction][crafting][mcp]") {
    clear_all_state();
    const auto cleanup = on_out_of_scope([]() { clear_all_state(); });
    const auto guard = interaction_test_guard{};
    const auto restore_read = restore_on_out_of_scope<decltype(uistate.read_recipes)>(
        uistate.read_recipes);
    const auto restore_expanded = restore_on_out_of_scope<decltype(uistate.expanded_recipes)>(
        uistate.expanded_recipes);
    auto& you = get_avatar();
    add_crafting_fixture_item(you, itype_id("copper"), 3);
    auto& here = get_map();
    here.add_item_or_charges(you.bub_pos(), item::spawn("test_rock"));
    const auto material_before = you.amount_of(itype_id("copper"));
    const auto ground_before = here.i_at(you.bub_pos()).size();
    const auto& unavailable = recipe_id("test_pipe").obj();

    auto step = 0;
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto snapshot = recipe_rows(game_client::current_interaction({.limit = 100}));
        if (snapshot.context == "CRAFTING") {
            if (step == 0) {
                step = 1;
                return resolve_action("TOGGLE_UNAVAILABLE");
            }
            if (step == 1) {
                step = 2;
                return resolve_action("FILTER");
            }
            if (step == 3) {
                REQUIRE(snapshot.choices.size() == 1);
                const auto& choice = snapshot.choices.front();
                CHECK(choice.label.find(unavailable.result_name()) != std::string::npos);
                CHECK_FALSE(choice.enabled);
                CHECK(choice.selectable);
                CHECK_FALSE(choice.denial.empty());
                step = 4;
                return resolve({
                    .input_id = snapshot.input_id,
                    .operation = game_client::interaction_operation::choose,
                    .target_id = choice.id,
                });
            }
            REQUIRE(step == 5);
            step = 6;
            return resolve({
                .input_id = snapshot.input_id,
                .operation = game_client::interaction_operation::cancel,
            });
        }
        if (step == 2) {
            REQUIRE(snapshot.field);
            step = 3;
            return resolve({
                .input_id = snapshot.input_id,
                .operation = game_client::interaction_operation::fill,
                .target_id = snapshot.field->id,
                .value = unavailable.result_name(),
                .submit = true,
            });
        }
        REQUIRE(step == 4);
        CHECK(snapshot.message.contains("You can't do that"));
        REQUIRE(snapshot.choices.size() == 1);
        step = 5;
        return resolve({
            .input_id = snapshot.input_id,
            .operation = game_client::interaction_operation::choose,
            .target_id = snapshot.choices.front().id,
        });
    });

    you.craft(you.bub_pos());
    CHECK(step == 6);
    CHECK_FALSE(you.activity);
    CHECK(you.amount_of(itype_id("copper")) == material_before);
    CHECK(you.amount_of(itype_id("test_pipe")) == 0);
    CHECK(here.i_at(you.bub_pos()).size() == ground_before);
}
TEST_CASE(
    "semantic nested crafting choice expands and rejects quantities",
    "[client][interaction][crafting][mcp]") {
    clear_all_state();
    const auto cleanup = on_out_of_scope([]() { clear_all_state(); });
    const auto guard = interaction_test_guard{};
    const auto nested_categories = override_option("ENABLE_NESTED_CATEGORIES", "true");
    const auto restore_read = restore_on_out_of_scope<decltype(uistate.read_recipes)>(
        uistate.read_recipes);
    const auto restore_expanded = restore_on_out_of_scope<decltype(uistate.expanded_recipes)>(
        uistate.expanded_recipes);
    uistate.expanded_recipes.clear();
    auto& you = get_avatar();
    prepare_crafting_fixture();
    const auto& nested = recipe_id("test_semantic_recipes").obj();

    auto visited = std::set<std::string>{};
    auto choice_count_before = std::size_t{0};
    auto show_unavailable = false;
    auto expanded = false;
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto snapshot = game_client::current_interaction({.limit = 200});
        REQUIRE(snapshot.context == "CRAFTING");
        if (!expanded) {
            const auto category = std::ranges::find_if(snapshot.choices, [&](const auto& choice) {
                return choice.label.find(nested.result_name()) != std::string::npos;
            });
            if (category == snapshot.choices.end()) {
                if (!show_unavailable) {
                    show_unavailable = true;
                    return resolve_action("TOGGLE_UNAVAILABLE");
                }
                return resolve_action(
                    visited.insert(snapshot.message).second ? "RIGHT" : "NEXT_TAB");
            }
            CHECK_FALSE(category->selected_count);
            CHECK_FALSE(category->available_count);
            const auto rejected = game_client::resolve_interaction_command({
                .input_id = snapshot.input_id,
                .operation = game_client::interaction_operation::set_count,
                .target_id = category->id,
                .count = 2,
            });
            REQUIRE_FALSE(rejected);
            CHECK(rejected.error().starts_with("invalid:"));
            choice_count_before = snapshot.choices.size();
            expanded = true;
            return resolve({
                .input_id = snapshot.input_id,
                .operation = game_client::interaction_operation::choose,
                .target_id = category->id,
            });
        }
        CHECK(uistate.expanded_recipes.count(nested.ident()) == 1);
        CHECK(snapshot.choices.size() >= choice_count_before + 2);
        CHECK(
            std::ranges::count_if(
                snapshot.choices,
                [&](const auto& choice) {
                    return choice.label.find("TEST soldering iron") != std::string::npos
                        || choice.label.find("TEST pipe") != std::string::npos;
                })
            == 2);
        return resolve({
            .input_id = snapshot.input_id,
            .operation = game_client::interaction_operation::cancel,
        });
    });

    you.craft(you.bub_pos());
    CHECK(expanded);
    CHECK_FALSE(you.activity);
}
TEST_CASE(
    "crafting rejects filtered and stale semantic recipe identities without mutation",
    "[client][interaction][crafting][mcp]") {
    clear_all_state();
    const auto cleanup = on_out_of_scope([]() { clear_all_state(); });
    const auto guard = interaction_test_guard{};
    const auto restore_read = restore_on_out_of_scope<decltype(uistate.read_recipes)>(
        uistate.read_recipes);
    const auto restore_expanded = restore_on_out_of_scope<decltype(uistate.expanded_recipes)>(
        uistate.expanded_recipes);
    auto& you = get_avatar();
    prepare_crafting_fixture();
    const auto material_before = you.amount_of(itype_id("e_scrap"));
    auto old_id = std::string{};
    auto old_input_id = std::uint64_t{0};
    auto step = 0;

    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto snapshot = recipe_rows(game_client::current_interaction({.limit = 100}));
        if (snapshot.context != "CRAFTING") {
            REQUIRE(snapshot.field);
            if (step == 1) {
                step = 2;
                return resolve({
                    .input_id = snapshot.input_id,
                    .operation = game_client::interaction_operation::fill,
                    .target_id = snapshot.field->id,
                    .value = "TEST",
                    .submit = true,
                });
            }
            REQUIRE(step == 3);
            step = 4;
            return resolve({
                .input_id = snapshot.input_id,
                .operation = game_client::interaction_operation::fill,
                .target_id = snapshot.field->id,
                .value = "__no_semantic_recipe__",
                .submit = true,
            });
        }
        if (step == 0) {
            step = 1;
            return resolve_action("FILTER");
        }
        if (step == 2) {
            REQUIRE(snapshot.choices.size() == 2);
            old_id = snapshot.choices.back().id;
            old_input_id = snapshot.input_id;
            step = 3;
            return resolve_action("FILTER");
        }
        REQUIRE(step == 4);
        CHECK(snapshot.choices.empty());
        const auto removed = game_client::resolve_interaction_command({
            .input_id = snapshot.input_id,
            .operation = game_client::interaction_operation::choose,
            .target_id = old_id,
        });
        REQUIRE_FALSE(removed);
        CHECK(removed.error().starts_with("invalid:"));
        const auto stale = game_client::resolve_interaction_command({
            .input_id = old_input_id,
            .operation = game_client::interaction_operation::choose,
            .target_id = old_id,
        });
        REQUIRE_FALSE(stale);
        CHECK(stale.error().starts_with("stale:"));
        step = 5;
        return resolve({
            .input_id = snapshot.input_id,
            .operation = game_client::interaction_operation::cancel,
        });
    });

    you.craft(you.bub_pos());
    CHECK(step == 5);
    CHECK_FALSE(you.activity);
    CHECK(you.amount_of(itype_id("e_scrap")) == material_before);
    CHECK(you.amount_of(itype_id("test_pipe")) == 0);
    CHECK(you.amount_of(itype_id("test_soldering_iron")) == 0);
}
TEST_CASE(
    "crafting replay rejects a changed real recipe schema without mutation",
    "[client][interaction][crafting][replay][mcp]") {
    clear_all_state();
    const auto cleanup_state = on_out_of_scope([]() { clear_all_state(); });
    const auto guard = interaction_test_guard{};
    const auto restore_read = restore_on_out_of_scope<decltype(uistate.read_recipes)>(
        uistate.read_recipes);
    const auto restore_expanded = restore_on_out_of_scope<decltype(uistate.expanded_recipes)>(
        uistate.expanded_recipes);
    const auto restore_hidden = restore_on_out_of_scope<decltype(uistate.hidden_recipes)>(
        uistate.hidden_recipes);
    static auto counter = std::atomic_uint64_t{0};
    const auto directory =
        std::filesystem::path(PATH_INFO::user_dir())
        / ("semantic-crafting-schema-" + std::to_string(counter.fetch_add(1)));
    REQUIRE(std::filesystem::create_directory(directory));
    const auto cleanup_replay = on_out_of_scope([&] {
        replay::stop();
        std::filesystem::remove_all(directory);
    });
    const auto path = directory / "input.jsonl";
    prepare_crafting_fixture();

    replay::configure_recording(path.string(), {.rng_seed = 31});
    replay::start();
    auto reads = 0;
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto snapshot = recipe_rows(game_client::current_interaction({.limit = 100}));
        if (snapshot.context != "CRAFTING") {
            return resolve({
                .input_id = snapshot.input_id,
                .operation = game_client::interaction_operation::fill,
                .target_id = snapshot.field->id,
                .value = "TEST",
                .submit = true,
            });
        }
        if (reads++ == 0) { return resolve_action("FILTER"); }
        REQUIRE(snapshot.choices.size() == 2);
        return resolve({
            .input_id = snapshot.input_id,
            .operation = game_client::interaction_operation::cancel,
        });
    });
    auto& recorded = get_avatar();
    recorded.craft(recorded.bub_pos());
    CHECK_FALSE(recorded.activity);
    replay::finish();

    game_client::memory::set_input_provider([](const int /*timeout*/) {
        return resolve_action("YES");
    });
    clear_all_state();
    prepare_crafting_fixture(false);
    uistate.hidden_recipes.insert(recipe_id("test_pipe"));
    const auto material_before = get_avatar().amount_of(itype_id("e_scrap"));
    game_client::memory::set_input_provider([](const int /*timeout*/) -> input_event {
        throw std::runtime_error("crafting schema playback must not read live input");
    });
    replay::configure_playback(path.string(), {.rng_seed = 31});
    replay::start();
    CHECK_THROWS_WITH(
        get_avatar().craft(get_avatar().bub_pos()), Catch::Matchers::Contains("schema changed"));
    CHECK_FALSE(get_avatar().activity);
    CHECK(get_avatar().amount_of(itype_id("e_scrap")) == material_before);
    CHECK(get_avatar().amount_of(itype_id("test_pipe")) == 0);
}
TEST_CASE(
    "real crafting selector filters and starts a batch through semantic input",
    "[client][interaction][crafting][mcp]") {
    clear_all_state();
    const auto cleanup = on_out_of_scope([]() { clear_all_state(); });
    const auto guard = interaction_test_guard{};
    const auto restore_read = restore_on_out_of_scope<decltype(uistate.read_recipes)>(
        uistate.read_recipes);
    const auto restore_expanded = restore_on_out_of_scope<decltype(uistate.expanded_recipes)>(
        uistate.expanded_recipes);
    auto& you = get_avatar();
    const auto& recipe = recipe_id("test_soldering_iron").obj();
    prepare_crafting_fixture();

    auto crafting_reads = 0;
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto snapshot = recipe_rows(game_client::current_interaction({.limit = 100}));
        if (snapshot.context == "CRAFTING") {
            if (crafting_reads++ == 0) { return resolve_action("FILTER"); }
            const auto target =
                crafting_reads == 2
                    ? std::ranges::find_if(
                          snapshot.choices,
                          [&](const auto& choice) {
                              return choice.label.find(recipe.result_name()) != std::string::npos;
                          })
                    : std::ranges::find(
                          snapshot.choices, true, &game_client::interaction_choice::highlighted);
            REQUIRE(target != snapshot.choices.end());
            if (crafting_reads == 2) {
                CHECK(snapshot.choices.size() == 1);
                CHECK(target->enabled);
                CHECK(target->selectable);
                CHECK(target->minimum_count == 1);
                CHECK(target->available_count == 50);
                for (const auto count : {std::uint64_t{1}, std::uint64_t{50}}) {
                    CHECK(game_client::resolve_interaction_command({
                        .input_id = snapshot.input_id,
                        .operation = game_client::interaction_operation::set_count,
                        .target_id = target->id,
                        .count = count,
                    }));
                }
                for (const auto count : {std::uint64_t{0}, std::uint64_t{51}}) {
                    const auto rejected = game_client::resolve_interaction_command({
                        .input_id = snapshot.input_id,
                        .operation = game_client::interaction_operation::set_count,
                        .target_id = target->id,
                        .count = count,
                    });
                    REQUIRE_FALSE(rejected);
                    CHECK(rejected.error().starts_with("invalid:"));
                }
                return resolve({
                    .input_id = snapshot.input_id,
                    .operation = game_client::interaction_operation::set_count,
                    .target_id = target->id,
                    .count = 2,
                });
            }
            CHECK(snapshot.choices.size() == 50);
            CHECK(target->selected_count == 2);
            CHECK(target->highlighted);
            return resolve({
                .input_id = snapshot.input_id,
                .operation = game_client::interaction_operation::choose,
                .target_id = target->id,
            });
        }
        REQUIRE(snapshot.kind == game_client::interaction_kind::field);
        return resolve({
            .input_id = snapshot.input_id,
            .operation = game_client::interaction_operation::fill,
            .target_id = snapshot.field->id,
            .value = recipe.result_name(),
            .submit = true,
        });
    });

    you.craft(you.bub_pos());
    REQUIRE(you.activity);
    REQUIRE(you.activity->id() == activity_id("ACT_CRAFT"));
    CHECK(crafting_reads == 3);
    CHECK_FALSE(you.has_amount(itype_id("e_scrap"), 1));

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
    "semantic replay restores the real crafting filter and batch selection",
    "[client][interaction][crafting][replay][mcp]") {
    clear_all_state();
    const auto cleanup_state = on_out_of_scope([]() { clear_all_state(); });
    const auto guard = interaction_test_guard{};
    const auto restore_read = restore_on_out_of_scope<decltype(uistate.read_recipes)>(
        uistate.read_recipes);
    const auto restore_expanded = restore_on_out_of_scope<decltype(uistate.expanded_recipes)>(
        uistate.expanded_recipes);
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
    const auto& recipe = recipe_id("test_soldering_iron").obj();
    prepare_crafting_fixture();

    replay::configure_recording(path.string(), {.rng_seed = 29});
    replay::start();
    auto reads = 0;
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto snapshot = game_client::current_interaction({.limit = 100});
        if (snapshot.context != "CRAFTING") {
            return resolve({
                .input_id = snapshot.input_id,
                .operation = game_client::interaction_operation::fill,
                .target_id = snapshot.field->id,
                .value = recipe.result_name(),
                .submit = true,
            });
        }
        if (reads++ == 0) { return resolve_action("FILTER"); }
        const auto target =
            reads == 2
                ? std::ranges::find_if(
                      snapshot.choices,
                      [&](const auto& choice) {
                          return choice.label.find(recipe.result_name()) != std::string::npos;
                      })
                : std::ranges::
                      find(snapshot.choices, true, &game_client::interaction_choice::highlighted);
        REQUIRE(target != snapshot.choices.end());
        return reads == 2
                 ? resolve({
                       .input_id = snapshot.input_id,
                       .operation = game_client::interaction_operation::set_count,
                       .target_id = target->id,
                       .count = 2,
                   })
                 : resolve({
                       .input_id = snapshot.input_id,
                       .operation = game_client::interaction_operation::choose,
                       .target_id = target->id,
                   });
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
    CHECK(played.amount_of(itype_id("copper")) == 0);
    CHECK(played.amount_of(itype_id("scrap")) == 0);
    CHECK(played.charges_of(itype_id("duct_tape")) == 0);
    CHECK(played.amount_of(itype_id("cable")) == 0);
    replay::finish();
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
    "look around publishes its cursor and leaves on semantic cancel",
    "[client][interaction][mcp]") {
    clear_map();
    auto& you = get_avatar();
    clear_character(you, false);
    const auto guard = interaction_test_guard{};
    auto reads = 0;
    auto cursor = tripoint_bub_ms{};
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto snapshot = game_client::current_interaction();
        REQUIRE(snapshot.target);
        CHECK(snapshot.kind == game_client::interaction_kind::target);
        CHECK(snapshot.allow_cancel);
        cursor = {snapshot.target->cursor.x, snapshot.target->cursor.y, snapshot.target->cursor.z};
        if (reads++ == 0) { return resolve_action("RIGHT"); }
        return resolve({
            .input_id = snapshot.input_id,
            .operation = game_client::interaction_operation::cancel,
        });
    });
    auto center = you.bub_pos();
    const auto result = g->look_around(false, center, center, false, false, false);
    CHECK_FALSE(result.position);
    REQUIRE(reads == 2);
    CHECK(cursor == tripoint_bub_ms(you.bub_pos().x() + 1, you.bub_pos().y(), you.bub_pos().z()));
}
TEST_CASE(
    "look around publishes what the native look window prints of the cursor square",
    "[client][interaction][mcp]") {
    clear_map();
    auto& you = get_avatar();
    clear_character(you, false);
    const auto guard = interaction_test_guard{};
    auto reads = 0;
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto snapshot = game_client::current_interaction();
        REQUIRE(snapshot.target);
        const auto& target = *snapshot.target;
        CHECK(target.status == "look");
        CHECK_FALSE(target.description.empty());
        // The square under the cursor, wherever it moves, as the mouse view describes it too.
        CHECK(target.description == engine_client::tile_info_lines(target.cursor));
        if (reads++ == 0) {
            CHECK(target.cursor == target.source);
            return resolve_action("RIGHT");
        }
        CHECK(target.cursor.x == target.source.x + 1);
        return resolve({
            .input_id = snapshot.input_id,
            .operation = game_client::interaction_operation::cancel,
        });
    });
    auto center = you.bub_pos();
    g->look_around(false, center, center, false, false, false);
    CHECK(reads == 2);
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
TEST_CASE(
    "the native options menu publishes pages and editable options and applies fill",
    "[client][interaction][settings][mcp]") {
    const auto guard = interaction_test_guard{};
    const auto original = get_option<int>("SAFEMODEPROXIMITY");
    auto step = 0;
    auto seen_page = std::string{};

    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto snapshot = game_client::current_interaction({.limit = 200});
        const auto find = [&](const std::string& id) {
            return std::ranges::find(snapshot.choices, id, &game_client::interaction_choice::id);
        };
        if (snapshot.context != "OPTIONS") {
            REQUIRE(step == 3);
            step = 4;
            const auto no =
                std::ranges::find(snapshot.choices, "No", &game_client::interaction_choice::label);
            REQUIRE(no != snapshot.choices.end());
            return resolve(
                {.input_id = snapshot.input_id,
                 .operation = game_client::interaction_operation::choose,
                 .target_id = no->id});
        }
        const auto row = find("option:SAFEMODEPROXIMITY");
        if (step == 0) {
            REQUIRE(row != snapshot.choices.end());
            REQUIRE(row->editor);
            CHECK(row->editor->type == "integer");
            CHECK(row->editor->value == std::to_string(original));
            CHECK(
                std::ranges::count_if(
                    snapshot.choices,
                    [](const auto& choice) {
                        return !choice.pane_id && choice.id.starts_with("page:");
                    })
                >= 5);
            const auto too_big = game_client::resolve_interaction_command(
                {.input_id = snapshot.input_id,
                 .operation = game_client::interaction_operation::fill,
                 .target_id = row->id,
                 .value = std::to_string(*row->editor->maximum + 1),
                 .submit = true});
            REQUIRE_FALSE(too_big);
            CHECK(too_big.error().starts_with("invalid:"));
            step = 1;
            return resolve(
                {.input_id = snapshot.input_id,
                 .operation = game_client::interaction_operation::fill,
                 .target_id = row->id,
                 .value = "7",
                 .submit = true});
        }
        if (step == 1) {
            REQUIRE(row != snapshot.choices.end());
            CHECK(row->editor->value == "7");
            CHECK(row->columns.front().value == "7");
            seen_page = snapshot.choices.front().id;
            step = 2;
            return resolve(
                {.input_id = snapshot.input_id,
                 .operation = game_client::interaction_operation::choose,
                 .target_id = "page:interface"});
        }
        REQUIRE(step == 2);
        CHECK(row == snapshot.choices.end());
        CHECK(find("page:interface")->selected);
        step = 3;
        return resolve(
            {.input_id = snapshot.input_id,
             .operation = game_client::interaction_operation::cancel});
    });

    get_options().show(false);
    CHECK(step == 4);
    CHECK(seen_page == "page:general");
    CHECK(get_option<int>("SAFEMODEPROXIMITY") == original);
}
TEST_CASE(
    "the native keybindings menu adds a key through the key field and discards it unsaved",
    "[client][interaction][settings][mcp]") {
    const auto guard = interaction_test_guard{};
    auto ctxt = input_context{"KEYBINDING_TEST"};
    ctxt.register_action("UP");
    const auto before = inp_mngr.get_input_for_action("UP", "default");
    auto step = 0;

    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto snapshot = game_client::current_interaction({.limit = 200});
        const auto command =
            [&](const game_client::interaction_operation operation, const std::string& target,
                const std::string& value = {}) {
                return resolve(
                    {.input_id = snapshot.input_id,
                     .operation = operation,
                     .target_id = target,
                     .value = value,
                     .submit = value.empty() ? std::nullopt : std::optional{true}});
            };
        const auto row =
            std::ranges::find(snapshot.choices, "action:UP", &game_client::interaction_choice::id);
        if (snapshot.field && snapshot.field->type == "key") {
            REQUIRE(step == 2);
            const auto unknown = game_client::resolve_interaction_command(
                {.input_id = snapshot.input_id,
                 .operation = game_client::interaction_operation::fill,
                 .target_id = snapshot.field->id,
                 .value = "no such key",
                 .submit = true});
            CHECK_FALSE(unknown);
            step = 3;
            return command(game_client::interaction_operation::fill, snapshot.field->id, "F9");
        }
        if (step == 0) {
            REQUIRE(row != snapshot.choices.end());
            CHECK(snapshot.title == "Keybindings");
            step = 1;
            return command(game_client::interaction_operation::choose, "mode:add_global");
        }
        if (step == 1) {
            step = 2;
            return command(game_client::interaction_operation::choose, "action:UP");
        }
        if (step == 3) {
            REQUIRE(row != snapshot.choices.end());
            CHECK(row->columns.front().value.find("F9") != std::string::npos);
            step = 5;
            return command(game_client::interaction_operation::cancel, "");
        }
        REQUIRE(step == 5);
        step = 6;
        const auto no =
            std::ranges::find(snapshot.choices, "No", &game_client::interaction_choice::label);
        REQUIRE(no != snapshot.choices.end());
        return command(game_client::interaction_operation::choose, no->id);
    });

    ctxt.display_menu();
    CHECK(step == 6);
    CHECK(inp_mngr.get_input_for_action("UP", "default") == before);
}
TEST_CASE(
    "vehicle interaction publishes the grid, denial reasons and the requirements of each task",
    "[client][interaction][veh_interact][mcp]") {
    clear_all_state();
    const auto guard = interaction_test_guard{};
    set_time(calendar::turn_zero + 12_hours);
    auto& you = get_avatar();
    const auto origin = tripoint_bub_ms(60, 60, 0);
    you.setpos(origin + point_south);
    auto* veh = get_map().add_vehicle(vproto_id("bicycle"), origin, 0_degrees, 0, 0);
    REQUIRE(veh != nullptr);
    auto step = 0;
    auto all_parts = std::size_t{0};
    auto cursor = game_client::interaction_position{};

    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto snapshot = game_client::current_interaction({.limit = 400});
        REQUIRE(snapshot.context == "VEH_INTERACT");
        REQUIRE(snapshot.vehicle);
        const auto& view = *snapshot.vehicle;
        const auto find = [&](const std::string& id) {
            return std::ranges::find(snapshot.choices, id, &game_client::interaction_choice::id);
        };
        const auto command =
            [&](const game_client::interaction_operation operation, const std::string& id) {
                ++step;
                return resolve(
                    {.input_id = snapshot.input_id, .operation = operation, .target_id = id});
            };
        const auto choose = game_client::interaction_operation::choose;
        const auto highlighted = std::ranges::find_if(snapshot.choices, [](const auto& choice) {
            return choice.highlighted;
        });
        switch (step) {
            case 0: {
                // The main screen: the grid with the cursor part, the part list, the stat lines.
                CHECK(view.mode == "main");
                CHECK(view.name == veh->name);
                CHECK(std::ranges::any_of(view.cells, [](const auto& cell) {
                    return cell.dx == 0 && cell.dy == 0 && !cell.glyph.empty();
                }));
                CHECK_FALSE(view.parts.empty());
                CHECK_FALSE(view.stats.empty());
                CHECK_FALSE(view.detail);
                CHECK(view.verb.empty());
                CHECK(view.owned);
                // Every denied action says why; nothing is damaged yet, so repair is denied.
                for (const auto& choice : snapshot.choices) {
                    CHECK(choice.enabled == choice.denial.empty());
                }
                REQUIRE(find("action:REPAIR") != snapshot.choices.end());
                CHECK_FALSE(find("action:REPAIR")->enabled);
                CHECK(
                    find("action:REPAIR")->denial == "There are no damaged parts on this vehicle.");
                REQUIRE(find("action:INSTALL") != snapshot.choices.end());
                CHECK(find("action:INSTALL")->enabled);
                cursor = view.cursor;
                return command(choose, "action:INSTALL");
            }
            case 1: {
                // The install list: tabs, candidate parts, and the requirements of the highlighted
                // one.
                CHECK(view.mode == "install");
                CHECK(view.verb == "Install");
                REQUIRE(view.tabs);
                CHECK(view.tabs->labels.size() == 8);
                CHECK(view.tabs->selected == 0);
                REQUIRE(highlighted != snapshot.choices.end());
                CHECK(highlighted->id == "part:0");
                REQUIRE(view.detail);
                CHECK(view.detail->name == highlighted->label);
                CHECK_FALSE(view.detail->requirements.empty());
                CHECK_FALSE(view.detail->time.empty());
                CHECK(view.detail->time_heading == "Time required:");
                REQUIRE(view.detail->groups.size() == 4);
                const auto& groups = view.detail->groups;
                CHECK(groups[0].kind == "skill");
                CHECK(groups[0].heading == "Skills required:");
                CHECK(groups[0].empty_text == "NONE");
                CHECK(groups[1].heading == "Components required:");
                CHECK(groups[2].heading == "Tools required:");
                CHECK(groups[3].heading == "Additional requirements:");
                CHECK(groups[1].empty_text.empty());
                // Without the materials the highlighted part cannot be installed, and says why.
                CHECK(highlighted->enabled == view.detail->can_do);
                CHECK((view.detail->can_do || !highlighted->denial.empty()));
                all_parts = std::ranges::count_if(snapshot.choices, [](const auto& choice) {
                    return choice.id.starts_with("part:");
                });
                REQUIRE(all_parts > 2);
                return command(choose, "tab:1");
            }
            case 2: {
                CHECK(view.tabs->selected == 1);
                const auto shown = std::ranges::count_if(snapshot.choices, [](const auto& choice) {
                    return choice.id.starts_with("part:");
                });
                CHECK(static_cast<std::size_t>(shown) < all_parts);
                return command(choose, "tab:0");
            }
            case 3:
                // Choosing a part other than the highlighted one only moves the highlight.
                REQUIRE(find("part:1") != snapshot.choices.end());
                CHECK_FALSE(find("part:1")->highlighted);
                return command(choose, "part:1");
            case 4:
                CHECK(view.mode == "install");
                REQUIRE(highlighted != snapshot.choices.end());
                CHECK(highlighted->id == "part:1");
                return command(game_client::interaction_operation::cancel, "");
            case 5:
                CHECK(view.mode == "main");
                CHECK_FALSE(view.detail);
                return command(choose, "action:REMOVE");
            case 6: {
                CHECK(view.mode == "remove");
                CHECK(view.verb == "Remove");
                REQUIRE(view.detail);
                CHECK(view.detail->name == highlighted->label);
                // Removing needs a tool the character lacks, which the rows say.
                CHECK(std::ranges::any_of(view.detail->requirements, [](const auto& need) {
                    return !need.met;
                }));
                CHECK_FALSE(view.detail->can_do);
                CHECK_FALSE(highlighted->enabled);
                return command(game_client::interaction_operation::cancel, "");
            }
            case 7:
                CHECK(view.mode == "main");
                return command(choose, "action:RIGHT");
            case 8:
                CHECK(view.cursor != cursor);
                return command(game_client::interaction_operation::cancel, "");
            default:
                FAIL("the vehicle screen outlived its cancel");
                return resolve({});
        }
    });

    veh_interact::run(*veh, tripoint_mnt_veh::zero());
    CHECK(step == 9);
}
TEST_CASE(
    "real spell menu publishes each spell's details and its callback key rows",
    "[client][interaction][mcp][magic]") {
    clear_all_state();
    const auto guard = interaction_test_guard{};
    auto& you = get_avatar();
    const auto forget_all = on_out_of_scope([&you]() { you.magic = pimpl<known_magic>(); });
    you.magic->learn_spell(spell_id("test_spell_pew"), you, true);
    auto snapshots = std::vector<game_client::interaction_snapshot>{};
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        snapshots.push_back(game_client::current_interaction());
        const auto& observed = snapshots.back();
        const auto favorite = std::ranges::
            find(observed.choices, std::string("Toggle favorite"),
                 &game_client::interaction_choice::label);
        const auto choose_favorite = snapshots.size() == 1 && favorite != observed.choices.end();
        return resolve({
            .input_id = observed.input_id,
            .operation = choose_favorite ? game_client::interaction_operation::choose
                                         : game_client::interaction_operation::cancel,
            .target_id = choose_favorite ? favorite->id : std::string{},
        });
    });

    const auto selected = you.magic->select_spell(you);

    INFO(snapshots.front().context << ": " << snapshots.front().message);
    CHECK(selected == UILIST_CANCEL);
    // choosing the favorite key row runs the native handler and leaves the menu open
    REQUIRE(snapshots.size() == 2);
    const auto& choices = snapshots.front().choices;
    const auto spell_row = std::ranges::find_if(choices, [](const auto& choice) {
        return choice.id.starts_with("entry:");
    });
    REQUIRE(spell_row != choices.end());
    const auto column_value = [&](const std::string& label) {
        const auto column =
            std::ranges::find(spell_row->columns, label, &game_client::interaction_column::key);
        return column == spell_row->columns.end() ? std::string{} : column->value;
    };
    CHECK(column_value("spell_level")
          == std::to_string(you.magic->get_spell(spell_id("test_spell_pew")).get_level()));
    CHECK_FALSE(column_value("failure_chance").empty());
    CHECK_FALSE(column_value("casting_cost").empty());
    CHECK_FALSE(column_value("casting_time").empty());
    CHECK_FALSE(column_value("range").empty());
    CHECK(spell_row->description.find("Casting Time") != std::string::npos);
    CHECK(spell_row->description.find("<color_") != std::string::npos);
    CHECK(std::ranges::any_of(choices, [](const auto& choice) {
        return choice.label == "Assign spell hotkey" && choice.id.starts_with("action:key:");
    }));
}
TEST_CASE(
    "real spell menu details do not change between reads for any known spell",
    "[client][interaction][mcp][magic]") {
    clear_all_state();
    const auto guard = interaction_test_guard{};
    auto& you = get_avatar();
    // The avatar is shared with later tests; forget every spell on the way out.
    const auto forget_all = on_out_of_scope([&you]() { you.magic = pimpl<known_magic>(); });
    std::ranges::for_each(spell_type::get_all(), [&](const auto& type) {
        you.magic->learn_spell(type.id, you, true);
    });
    auto reads = std::vector<game_client::interaction_snapshot>{};
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        reads.push_back(game_client::current_interaction({.limit = 1000}));
        reads.push_back(game_client::current_interaction({.limit = 1000}));
        return resolve({
            .input_id = reads.back().input_id,
            .operation = game_client::interaction_operation::cancel,
        });
    });

    you.magic->select_spell(you);

    REQUIRE(reads.size() == 2);
    CHECK(reads[0].schema_id == reads[1].schema_id);
    REQUIRE(reads[0].choices.size() == reads[1].choices.size());
    for (const auto i : std::views::iota(std::size_t{}, reads[0].choices.size())) {
        INFO(reads[0].choices[i].label);
        CHECK(reads[0].choices[i].description == reads[1].choices[i].description);
    }
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
    const auto find_key = [](const game_client::interaction_snapshot& snapshot, const int key) {
        return std::ranges::
            find(snapshot.choices, "action:key:" + std::to_string(key),
                 &game_client::interaction_choice::id);
    };
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        observed.push_back(game_client::current_interaction({.limit = 1000}));
        const auto& snapshot = observed.back();
        auto command = game_client::interaction_command{
            .input_id = snapshot.input_id,
            .operation = game_client::interaction_operation::cancel,
        };
        const auto press = [&](const int key) {
            if (find_key(snapshot, key) != snapshot.choices.end()) {
                command.operation = game_client::interaction_operation::choose;
                command.target_id = "action:key:" + std::to_string(key);
            }
        };
        if (observed.size() == 1) {
            press('a');
        } else if (observed.size() == 2) {
            press('b');
        } else if (observed.size() == 3) {
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
    for (const auto key : {'b', 'i', 'd', 'o'}) {
        CHECK(find_key(list, key) != list.choices.end());
    }
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
template struct native_test_member<handle_action_member, &game::handle_action>;

auto handle_action() -> bool { return (g.get()->*native_member(handle_action_member{}))(); }

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
#endif // CATA_MCP
