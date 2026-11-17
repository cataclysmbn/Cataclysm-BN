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
/// The same screen as the wire carries it.
auto screen_wire() -> nlohmann::json {
    return nlohmann::json::parse(engine_client::serialize_boundary(current_boundary()))
        .at("stack")
        .back();
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
    return nlohmann::json::parse(engine_client::serialize_boundary(stack_state()))["stack"].back();
}

/// The weight and volume stats of the pickup list on top, by key.
auto pickup_stat(const std::string& key) -> nlohmann::json {
    for (const auto& stat : top_screen()["root"]["stats"]) {
        if (stat["key"].get<std::string>() == key) { return stat; }
    }
    return nlohmann::json::object();
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
TEST_CASE("every native input read is a new input boundary", "[client][interaction][mcp]") {
    const auto guard = interaction_test_guard{};
    auto context = input_context{"BOUNDARY_READS"};
    const auto rows = game_client::interaction_scope{
        context, [] {
            return game_client::interaction_snapshot{
                .kind = game_client::interaction_kind::choices, .choices = {{.id = "row"}}};
        }};
    const auto before = game_client::current_input_id();
    auto reads = std::vector<std::uint64_t>{};
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        reads.push_back(game_client::current_input_id());
        // No registered action: handle_input reads again.
        if (reads.size() == 1) { return input_event('x', input_event_t::keyboard); }
        return resolve(
            {.input_id = reads.back(),
             .operation = game_client::interaction_operation::choose,
             .target_id = "row"});
    });
    CHECK(context.handle_input() == "ANY_INPUT");
    REQUIRE(reads.size() == 2);
    CHECK(reads[0] != before);
    CHECK(reads[1] != reads[0]);
    REQUIRE(context.get_raw_input().interaction);
    CHECK(context.get_raw_input().interaction->target_id == "row");
}
TEST_CASE(
    "a choices page outside the row limits is refused before the screen is read",
    "[client][interaction][mcp][engine_client_contract]") {
    auto context = input_context{"PAGE_LIMITS"};
    const auto input = game_client::input_context_scope{context, "PAGE_LIMITS"};
    auto reads = 0;
    const auto rows = game_client::interaction_scope{
        context, [&] {
            ++reads;
            return game_client::interaction_snapshot{.choices = {{.id = "row"}}};
        }};
    for (const auto limit : {std::size_t{0}, std::size_t{201}}) {
        CAPTURE(limit);
        const auto page = engine_client::read_rows(
            {.epoch = "epoch:limits",
             .boundary_id = "boundary:limits",
             .component_id = "list:choices",
             .limit = limit},
            {.id = "boundary:limits", .epoch = "epoch:limits"});
        REQUIRE_FALSE(page);
        CHECK(page.error() == engine_client::error::resource_limit);
    }
    CHECK(reads == 0);
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
TEST_CASE(
    "the in-game main menu names the engine action of each entry", "[client][interaction][mcp]") {
    const auto guard = interaction_test_guard{};
    auto observed = game_client::interaction_snapshot{};
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        observed = game_client::current_interaction();
        return resolve({
            .input_id = observed.input_id,
            .operation = game_client::interaction_operation::cancel,
        });
    });

    handle_main_menu();

    const auto options = std::ranges::
        find(observed.choices, "open_options", &game_client::interaction_choice::action_id);
    REQUIRE(options != observed.choices.end());
    CHECK(options->label == "Options");
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
    "a schema id is the same in every build, as replays recorded elsewhere compare it",
    "[client][interaction][mcp]") {
    const auto context = input_context("SCHEMA_PIN");
    const auto input = game_client::input_context_scope(context, "SCHEMA_PIN");
    const auto interaction = game_client::interaction_scope(context, [] {
        return game_client::interaction_snapshot{
            .kind = game_client::interaction_kind::choices,
            .choices = {{.id = "yes", .label = "Yes"}},
        };
    });
    CHECK(game_client::current_interaction().schema_id == "1b1c25a14a0818af");
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
        engine_client::serialize_boundary(stack_state()))["stack"];
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

    SECTION("a row id names the row's place in its list") {
        CHECK(test_1kg_cube == "item:@g:0");
        CHECK(ground_id("test_2x4") == "item:@g:1");
        you.i_add(item::spawn("test_2x4"));
        const auto stacks = engine_client::world::item_stacks(get_map(), you);
        REQUIRE_FALSE(stacks.inventory.empty());
        CHECK(stacks.inventory.back().row.id
              == "item:@u:" + std::to_string(stacks.inventory.size() - 1));
    }
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
                return invoke("bn:filter:list:choices", {.text = "tele"});
            case 2:
                return invoke("bn:filter:list:choices", {.text = ""});
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
        if (reads++ == 0) { return invoke("bn:other:list:choices", {.text = "tele"}); }
        return resolve({
            .input_id = game_client::current_interaction().input_id,
            .operation = game_client::interaction_operation::cancel,
        });
    });
    menu.query();
    CHECK(menu.as_interaction().choices.size() == 3);
    CHECK(menu.as_interaction().filter.empty());
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
            return invoke("bn:key:99");
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

#endif // CATA_MCP
