#if defined(CATA_MCP)

#    include "cata_utility.h"
#    include "catch/catch.hpp"
#    include "client_command.h"
#    include "client_input.h"
#    include "client_interaction.h"
#    include "client_memory.h"
#    include "client_memory_scope.h"
#    include "cursesdef.h"
#    include "engine_client_component.h"
#    include "engine_client_contract.h"
#    include "filesystem.h"
#    include "game_constants.h"
#    include "input.h"
#    include "locale_fixture.h"
#    include "locale_scope.h"
#    include "mod_manager.h"
#    include "options.h"
#    include "output.h"
#    include "path_info.h"
#    include "state_helpers.h"
#    include "world.h"
#    include "worldfactory.h"

#    include <algorithm>
#    include <functional>
#    include <nlohmann/json.hpp>
#    include <ranges>
#    include <string>
#    include <variant>
#    include <vector>

namespace {
namespace ec = engine_client;
using json = nlohmann::json;

struct screen_guard {
    const game_client::memory::scoped_state memory;
    bool old_test_mode = test_mode;
    int old_termx = TERMX;
    int old_termy = TERMY;
    int old_full_screen_width = FULL_SCREEN_WIDTH;
    int old_full_screen_height = FULL_SCREEN_HEIGHT;

    screen_guard() {
        test_mode = false;
        TERMX = 100;
        TERMY = 40;
        FULL_SCREEN_WIDTH = 80;
        FULL_SCREEN_HEIGHT = 24;
        game_client::memory::resize(TERMX, TERMY);
        catacurses::stdscr = catacurses::newwin(TERMY, TERMX, point_zero);
        catacurses::newscr = catacurses::newwin(TERMY, TERMX, point_zero);
    }

    ~screen_guard() {
        game_client::memory::set_input_provider({});
        TERMX = old_termx;
        TERMY = old_termy;
        FULL_SCREEN_WIDTH = old_full_screen_width;
        FULL_SCREEN_HEIGHT = old_full_screen_height;
        test_mode = old_test_mode;
    }
};

auto captured() -> ec::boundary_state {
    const auto state = ec::capture_boundary(
        {.epoch = "epoch:worldgen",
         .ready =
             {.phase = "waiting_for_input",
              .game_ready = true,
              .accepts_interaction_commands = true,
              .accepts_registered_actions = true}});
    REQUIRE(state.has_value());
    return *state;
}

auto act(const std::string& action) -> input_event {
    auto input = game_client::input_command{};
    input.action = action;
    const auto result =
        game_client::resolve_input_command(input, game_client::memory::screen_size());
    REQUIRE(result.has_value());
    return *result;
}

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

auto part(const ec::screen_view& screen, const std::string& id) -> const ec::component& {
    REQUIRE(screen.root);
    const auto* const found = ec::find_component(*screen.root, id);
    INFO(id);
    REQUIRE(found != nullptr);
    return *found;
}
auto has_part(const ec::screen_view& screen, const std::string& id) -> bool {
    return screen.root && ec::find_component(*screen.root, id) != nullptr;
}
template <typename Body> auto body(const ec::component& value) -> const Body& {
    REQUIRE(std::holds_alternative<Body>(value.body));
    return std::get<Body>(value.body);
}
auto text_of(const ec::screen_view& screen, const std::string& id) -> const std::string& {
    return body<ec::text_body>(part(screen, id)).markup;
}
auto note_of(const ec::screen_view& screen, const std::string& panel, const std::string& key)
    -> std::string {
    const auto& stats = body<ec::panel_body>(part(screen, panel)).stats;
    const auto found = std::ranges::find(stats, key, &ec::panel_stat::key);
    REQUIRE(found != stats.end());
    return found->label;
}
auto rows_of(const ec::screen_view& screen, const std::string& list)
    -> const std::vector<ec::row>& {
    return body<ec::list_body>(part(screen, list)).rows;
}
auto tab_ids(const ec::screen_view& screen, const std::string& tabs) -> std::vector<std::string> {
    return body<ec::tabs_body>(part(screen, tabs)).tabs | std::views::transform(&ec::tab_entry::id)
         | std::ranges::to<std::vector>();
}
auto selected_tab(const ec::screen_view& screen, const std::string& tabs) -> std::string {
    const auto& entries = body<ec::tabs_body>(part(screen, tabs)).tabs;
    const auto found = std::ranges::find(entries, true, &ec::tab_entry::selected);
    REQUIRE(found != entries.end());
    return found->id;
}
auto resolves_to(
    const ec::boundary_state& state, const std::string& affordance,
    std::optional<std::string> row = std::nullopt) -> std::optional<ec::error> {
    const auto result =
        ec::resolve_invoke(state, {.affordance_id = affordance, .args = {.row = row}});
    return result ? std::nullopt : std::optional{result.error()};
}

/// Answers the questions of the flow with yes and records the contexts it passed through.
auto answer_popup(const game_client::interaction_snapshot& snapshot) -> input_event {
    auto command = game_client::interaction_command{.input_id = snapshot.input_id};
    const auto yes =
        std::ranges::find(snapshot.choices, "YES", &game_client::interaction_choice::description);
    command.operation =
        yes != snapshot.choices.end()
            ? game_client::interaction_operation::choose
            : game_client::interaction_operation::cancel;
    if (yes != snapshot.choices.end()) { command.target_id = yes->id; }
    auto input = game_client::input_command{};
    input.interaction = std::move(command);
    const auto result =
        game_client::resolve_input_command(input, game_client::memory::screen_size());
    REQUIRE(result.has_value());
    return *result;
}

/// The top screen as a client reads it, with the ids of this screen instance left out.
auto published() -> json {
    auto top = json::parse(ec::serialize_interaction_boundary(captured()))["stack"].back();
    top.erase("screen_id");
    return top;
}
auto without_words(json value) -> json {
    if (value.is_string()) { return ""; }
    if (!value.is_structured()) { return value; }
    for (auto& member : value) { member = without_words(std::move(member)); }
    return value;
}
void collect_ids(const json& value, std::vector<std::string>& into) {
    if (value.is_object()) {
        for (const auto& [name, member] : value.items()) {
            if ((name == "id" || name == "affordance") && member.is_string()) {
                into.push_back(member.get<std::string>());
            }
            collect_ids(member, into);
        }
    } else if (value.is_array()) {
        for (const auto& member : value) { collect_ids(member, into); }
    }
}

const auto available_rows = std::string{"worldgen:mods:available:rows"};
const auto active_rows = std::string{"worldgen:mods:active:rows"};

} // namespace

TEST_CASE(
    "a world is created through the tab affordances, skipping the options page by a jump",
    "[worldgen_screen][engine_client_component]") {
    clear_all_state();
    const auto guard = screen_guard{};
    const auto saved_dir = PATH_INFO::savedir();
    const auto test_dir = saved_dir + "worldgen_screen/";
    REQUIRE(assure_dir_exist(test_dir));
    PATH_INFO::set_savedir(test_dir);
    const auto cleanup = on_out_of_scope([&] {
        // A failed flow creates no world, and deleting a world that does not exist crashes.
        if (world_generator->has_world("Ada")) { world_generator->delete_world("Ada", true); }
        PATH_INFO::set_savedir(saved_dir);
        CHECK(remove_tree(test_dir));
    });

    auto contexts = std::vector<std::string>{};
    auto added = std::string{};
    auto category = std::size_t{0};
    auto active_before = std::size_t{0};
    auto active_after = std::size_t{0};
    auto stage = 0;
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto snapshot = game_client::current_interaction();
        if (snapshot.context != "MODMANAGER_DIALOG"
            && snapshot.context != "WORLDGEN_CONFIRM_DIALOG") {
            contexts.push_back(snapshot.context);
            return answer_popup(snapshot);
        }
        contexts.push_back(snapshot.context);
        const auto state = captured();
        const auto screen = ec::top_screen(state);
        CHECK(screen.title == "World creation");
        switch (stage++) {
            case 0: {
                CHECK(tab_ids(screen, "worldgen:tabs")
                      == std::vector<std::string>{"mods", "options", "confirm"});
                CHECK(selected_tab(screen, "worldgen:tabs") == "mods");
                // Refused, not run: a tab or a row the screen does not have, and a row of the other
                // list.
                CHECK(
                    resolves_to(state, "worldgen:tab", "nowhere") == ec::error::invalid_arguments);
                CHECK(resolves_to(state, "worldgen:mods:add", "mod:no_such_mod")
                      == ec::error::invalid_arguments);
                CHECK(resolves_to(state, "worldgen:mods:remove", "mod:no_such_mod")
                      == ec::error::invalid_arguments);
                CHECK(resolves_to(state, "worldgen:mods:category", "tab:nowhere")
                      == ec::error::invalid_arguments);
                CHECK(resolves_to(state, "worldgen:mods:add") == ec::error::invalid_arguments);
                active_before = rows_of(screen, active_rows).size();
                [[fallthrough]];
            }
            case 1: {
                // Look for a category that has a mod to activate.
                const auto& available = rows_of(screen, available_rows);
                if (available.empty()) {
                    const auto categories = tab_ids(screen, "worldgen:mods:categories");
                    REQUIRE(++category < categories.size());
                    CHECK_FALSE(note_of(screen, "worldgen:mods:available", "none").empty());
                    stage = 1;
                    return invoke("worldgen:mods:category", {.row = categories[category]});
                }
                added = available.front().id;
                CHECK(available.front().id.starts_with("mod:"));
                stage = 2;
                return invoke("bn:activate:worldgen:mods:add", {.row = added});
            }
            case 2: {
                const auto& active = rows_of(screen, active_rows);
                // The mod manager also activates what the mod depends on.
                REQUIRE(active.size() > active_before);
                active_after = active.size();
                CHECK(active.back().id == "active:" + added.substr(4));
                CHECK(active.back().toggled == std::optional{true});
                CHECK(std::ranges::none_of(rows_of(screen, available_rows), [&](const auto& row) {
                    return row.id == added;
                }));
                // The options page is stepped over, the finalize page is the next screen.
                return invoke("worldgen:tab", {.row = "confirm"});
            }
            case 3: {
                CHECK(selected_tab(screen, "worldgen:tabs") == "confirm");
                const auto& form = body<ec::form_body>(part(screen, "worldgen:confirm:name"));
                REQUIRE(form.fields.size() == 1);
                CHECK(form.fields.front().id == "name");
                CHECK(form.fields.front().max_length > 0);
                CHECK(note_of(screen, "worldgen:confirm:notes", "format").find("V2")
                      != std::string::npos);
                return invoke("worldgen:confirm:name", {.text = "Ada"});
            }
            case 4:
                CHECK(
                    body<ec::form_body>(part(screen, "worldgen:confirm:name")).fields.front().value
                    == "Ada");
                return act("TOGGLE_V2_SAVE_FORMAT");
            case 5:
                CHECK(note_of(screen, "worldgen:confirm:notes", "format").find("V1")
                      != std::string::npos);
                return act("NEXT_TAB");
            default:
                FAIL("the world creation outlived its finish");
                return act("QUIT");
        }
    });
    const auto* const created = world_generator->make_new_world(true);
    REQUIRE(created != nullptr);
    CHECK(created->world_name == "Ada");
    CHECK(created->world_save_format == save_format::V1);
    REQUIRE(created->active_mod_order.size() == active_after);
    CHECK(created->active_mod_order.back().str() == added.substr(4));
    CHECK(stage == 6);
    CHECK(std::ranges::find(contexts, "OPTIONS") == contexts.end());
    CHECK(std::ranges::count(contexts, "WORLDGEN_CONFIRM_DIALOG") == 3);
}

TEST_CASE(
    "the active world mods list shows the mods and the description of the one under the cursor",
    "[worldgen_screen][engine_client_component]") {
    clear_all_state();
    REQUIRE(world_generator->active_world != nullptr);
    const auto mods = world_generator->active_world->info->active_mod_order;
    REQUIRE(mods.size() >= 2);
    const auto guard = screen_guard{};
    auto stage = 0;
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto state = captured();
        const auto screen = ec::top_screen(state);
        switch (stage++) {
            case 0: {
                const auto& rows = rows_of(screen, "worldgen:active_mods:rows");
                REQUIRE(rows.size() == mods.size());
                CHECK(rows.front().id == "mod:" + mods.front().str());
                CHECK(rows.front().label == mods.front()->name());
                CHECK(screen.focus_row == rows.front().id);
                const auto first = text_of(screen, "worldgen:active_mods:details");
                CHECK_FALSE(first.empty());
                CHECK(resolves_to(state, "worldgen:active_mods:pick", "mod:no_such_mod")
                      == ec::error::invalid_arguments);
                CHECK_FALSE(has_part(screen, "worldgen:tabs"));
                return invoke("worldgen:active_mods:pick", {.row = rows.back().id});
            }
            case 1:
                CHECK(screen.focus_row == "mod:" + mods.back().str());
                CHECK_FALSE(text_of(screen, "worldgen:active_mods:details").empty());
                return act("QUIT");
            default:
                FAIL("unexpected extra screen");
                return act("QUIT");
        }
    });
    world_generator->show_active_world_mods(mods);
    CHECK(stage == 2);
}

TEST_CASE(
    "a world mod a client activates closes the active mods list as Enter does",
    "[worldgen_screen][engine_client_component]") {
    clear_all_state();
    REQUIRE(world_generator->active_world != nullptr);
    const auto mods = world_generator->active_world->info->active_mod_order;
    REQUIRE_FALSE(mods.empty());
    const auto guard = screen_guard{};
    auto stage = 0;
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        if (stage++ == 0) {
            return invoke(
                "bn:activate:worldgen:active_mods:pick", {.row = "mod:" + mods.back().str()});
        }
        FAIL("the list outlived its Enter");
        return act("QUIT");
    });
    world_generator->show_active_world_mods(mods);
    CHECK(stage == 1);
}

TEST_CASE(
    "the mod manager of a world marks an active mod before removing it and keeps the list on a refused save",
    "[worldgen_screen][engine_client_component]") {
    clear_all_state();
    REQUIRE(world_generator->active_world != nullptr);
    const auto active_before = world_generator->active_world->info->active_mod_order;
    REQUIRE_FALSE(active_before.empty());
    const auto guard = screen_guard{};
    auto stage = 0;
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto snapshot = game_client::current_interaction();
        if (snapshot.context == "YESNO") {
            // Leaving asks whether to save the changes: no.
            const auto no = std::ranges::
                find(snapshot.choices, "NO", &game_client::interaction_choice::description);
            REQUIRE(no != snapshot.choices.end());
            auto input = game_client::input_command{};
            input.interaction = game_client::interaction_command{
                .input_id = snapshot.input_id,
                .operation = game_client::interaction_operation::choose,
                .target_id = no->id};
            return *game_client::resolve_input_command(input, game_client::memory::screen_size());
        }
        const auto state = captured();
        const auto screen = ec::top_screen(state);
        switch (stage++) {
            case 0: {
                CHECK(snapshot.context == "MODMANAGER_DIALOG");
                // Edited on its own, the manager has no world creation tabs.
                CHECK_FALSE(has_part(screen, "worldgen:tabs"));
                CHECK(screen.title == "Mods");
                CHECK(rows_of(screen, active_rows).size() == active_before.size());
                CHECK(screen.focus_component == available_rows);
                CHECK(
                    resolves_to(state, "worldgen:tab", "confirm") == ec::error::unknown_affordance);
                return invoke(
                    "worldgen:mods:remove", {.row = "active:" + active_before.front().str()});
            }
            case 1:
                // Selecting only moves the cursor to the active mod; it stays active.
                CHECK(rows_of(screen, active_rows).size() == active_before.size());
                CHECK(screen.focus_component == active_rows);
                CHECK(screen.focus_row == "active:" + active_before.front().str());
                CHECK_FALSE(text_of(screen, "worldgen:mods:details").empty());
                return act("QUIT");
            default:
                FAIL("unexpected extra screen");
                return act("QUIT");
        }
    });
    world_generator->edit_active_world_mods(&*world_generator->active_world->info);
    CHECK(stage == 2);
    CHECK(world_generator->active_world->info->active_mod_order == active_before);
}

TEST_CASE(
    "the world creation tabs are the same at any viewport size and have the same ids in another language",
    "[worldgen_screen][engine_client_component]") {
    clear_all_state();
    const auto guard = screen_guard{};
    const auto fixture_ko = locale_fixture::korean_catalogue{};
    REQUIRE(world_generator->active_world != nullptr);
    const auto mods = world_generator->active_world->info->active_mod_order;
    auto reads = 0;
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        ++reads;
        const auto saved_x = TERMX;
        const auto saved_y = TERMY;
        TERMX = 80;
        TERMY = 24;
        const auto narrow = published();
        TERMX = 200;
        TERMY = 60;
        const auto wide = published();
        TERMX = saved_x;
        TERMY = saved_y;
        CHECK(narrow.dump() == wide.dump());
        const auto korean = [] {
            const auto scope = localization::locale_scope(locale_fixture::korean);
            return published();
        }();
        auto english_ids = std::vector<std::string>{};
        auto korean_ids = std::vector<std::string>{};
        collect_ids(narrow, english_ids);
        collect_ids(korean, korean_ids);
        CHECK(english_ids == korean_ids);
        CHECK(without_words(narrow) == without_words(korean));
        return act("QUIT");
    });
    world_generator->show_active_world_mods(mods);
    CHECK(reads == 1);
}

TEST_CASE(
    "the world picker lists the worlds and enters the one a client selects",
    "[worldgen_screen][engine_client_component]") {
    clear_all_state();
    const auto guard = screen_guard{};
    const auto saved_dir = PATH_INFO::savedir();
    const auto test_dir = saved_dir + "world_picker/";
    REQUIRE(assure_dir_exist(test_dir));
    PATH_INFO::set_savedir(test_dir);
    const auto cleanup = on_out_of_scope([&] {
        PATH_INFO::set_savedir(saved_dir);
        CHECK(remove_tree(test_dir));
    });
    for (const auto* const name : {"Ada", "Bea"}) {
        auto info = WORLDINFO{};
        info.world_name = name;
        info.active_mod_order = {mod_id("test_data")};
        REQUIRE(info.save());
    }
    auto factory = worldfactory{};
    factory.init();
    auto stage = 0;
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto state = captured();
        const auto screen = ec::top_screen(state);
        switch (stage++) {
            case 0: {
                CHECK(screen.title == "WORLD SELECTION");
                CHECK(note_of(screen, "world_pick:worlds", "hint") == "Pick a world to enter game");
                const auto& rows = rows_of(screen, "world_pick:rows");
                CHECK((rows | std::views::transform(&ec::row::id) | std::ranges::to<std::vector>())
                      == std::vector<std::string>{"world:Ada", "world:Bea"});
                CHECK(rows.front().label == "Ada (0)");
                CHECK(screen.focus_row == "world:Ada");
                CHECK(resolves_to(state, "world_pick:pick", "world:Nobody")
                      == ec::error::invalid_arguments);
                return invoke("world_pick:pick", {.row = "world:Bea"});
            }
            case 1:
                CHECK(screen.focus_row == "world:Bea");
                // Enter on the row, as a client sends it.
                return invoke("bn:activate:world_pick:pick", {.row = "world:Bea"});
            default:
                FAIL("unexpected extra screen");
                return act("QUIT");
        }
    });
    const auto* const picked = factory.pick_world();
    CHECK(stage == 2);
    REQUIRE(picked != nullptr);
    CHECK(picked->world_name == "Bea");
}

#endif // CATA_MCP
