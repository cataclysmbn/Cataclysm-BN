#if defined(CATA_MCP)

#    include "action.h"
#    include "calendar.h"
#    include "cata_utility.h"
#    include "catch/catch.hpp"
#    include "client_command.h"
#    include "client_input.h"
#    include "client_interaction.h"
#    include "client_memory.h"
#    include "client_memory_scope.h"
#    include "cursesdef.h"
#    include "cursesport.h"
#    include "engine_client_component.h"
#    include "engine_client_contract.h"
#    include "game_constants.h"
#    include "input.h"
#    include "locale_fixture.h"
#    include "locale_scope.h"
#    include "options.h"
#    include "output.h"
#    include "state_helpers.h"

#    include <algorithm>
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
        TERMX = old_termx;
        TERMY = old_termy;
        FULL_SCREEN_WIDTH = old_full_screen_width;
        FULL_SCREEN_HEIGHT = old_full_screen_height;
        test_mode = old_test_mode;
    }
};

auto captured() -> ec::boundary_state {
    const auto state = ec::capture_boundary(
        {.epoch = "epoch:settings",
         .ready =
             {.phase = "waiting_for_input",
              .game_ready = true,
              .accepts_interaction_commands = true,
              .accepts_registered_actions = true}});
    REQUIRE(state.has_value());
    return *state;
}

/// The key a client sends for a registered action.
auto act(const std::string& action) -> input_event {
    auto input = game_client::input_command{};
    input.action = action;
    const auto result =
        game_client::resolve_input_command(input, game_client::memory::screen_size());
    REQUIRE(result.has_value());
    return *result;
}

/// The invoke a client sends for an affordance the screen's loop runs.
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

/// A command for the native widget on top, such as the answer to a prompt.
auto resolve(game_client::interaction_command command) -> input_event {
    auto input = game_client::input_command{};
    input.interaction = std::move(command);
    const auto result =
        game_client::resolve_input_command(input, game_client::memory::screen_size());
    REQUIRE(result.has_value());
    return *result;
}
auto answer_no() -> input_event {
    const auto snapshot = game_client::current_interaction({.limit = 200});
    const auto no =
        std::ranges::find(snapshot.choices, "No", &game_client::interaction_choice::label);
    REQUIRE(no != snapshot.choices.end());
    return resolve(
        {.input_id = snapshot.input_id,
         .operation = game_client::interaction_operation::choose,
         .target_id = no->id});
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
auto rows(const ec::screen_view& screen, const std::string& id) -> const std::vector<ec::row>& {
    return body<ec::list_body>(part(screen, id)).rows;
}
auto row_of(const ec::screen_view& screen, const std::string& list, const std::string& id)
    -> const ec::row& {
    const auto& all = rows(screen, list);
    const auto found = std::ranges::find(all, id, &ec::row::id);
    INFO(id);
    REQUIRE(found != all.end());
    return *found;
}
auto affordance(const ec::screen_view& screen, const std::string& id) -> const ec::affordance& {
    const auto found = std::ranges::find(screen.affordances, id, &ec::affordance::id);
    INFO(id);
    REQUIRE(found != screen.affordances.end());
    return *found;
}
auto field_of(const ec::screen_view& screen) -> const ec::form_field& {
    const auto& fields = body<ec::form_body>(part(screen, "options:editor")).fields;
    REQUIRE(fields.size() == 1);
    return fields.front();
}
auto selected_tab(const ec::screen_view& screen) -> std::string {
    const auto& tabs = body<ec::tabs_body>(part(screen, "options:pages")).tabs;
    const auto found = std::ranges::find(tabs, true, &ec::tab_entry::selected);
    REQUIRE(found != tabs.end());
    return found->id;
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

/// The name the keybindings screen gives its own actions.
auto mode_name(const std::string& id) -> std::string {
    auto help = input_context{"HELP_KEYBINDINGS"};
    help.register_action(id);
    return help.get_action_name(id);
}

/// The screen at 80x24 and at 200x60, and in Korean: its shape must not depend on any of them.
void check_invariance(const std::string& context) {
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
    const auto fixture_ko = locale_fixture::korean_catalogue{};
    const auto korean = [] {
        const auto scope = localization::locale_scope(locale_fixture::korean);
        return published();
    }();
    auto english_ids = std::vector<std::string>{};
    auto korean_ids = std::vector<std::string>{};
    collect_ids(narrow, english_ids);
    collect_ids(korean, korean_ids);
    INFO(context);
    CHECK_FALSE(english_ids.empty());
    CHECK(english_ids == korean_ids);
    CHECK(without_words(narrow) == without_words(korean));
}

} // namespace

TEST_CASE(
    "the options screen is driven by its tabs, rows and value editor",
    "[options_screen][engine_client_component]") {
    const auto guard = screen_guard{};
    const auto original = get_option<int>("SAFEMODEPROXIMITY");
    auto step = 0;
    auto maximum = 0.0;

    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto state = captured();
        const auto screen = ec::top_screen(state);
        if (step == 8) {
            // The prompt over the options screen: it is the top, and the options screen below it
            // offers nothing.
            const auto stack = ec::interaction_stack(state);
            REQUIRE(stack.size() >= 2);
            CHECK(stack[stack.size() - 2].context == "OPTIONS");
            CHECK(stack[stack.size() - 2].affordances.empty());
            ++step;
            return answer_no();
        }
        REQUIRE(screen.context == "OPTIONS");
        REQUIRE(screen.root);
        switch (step++) {
            case 0: {
                CHECK(screen.title == "Options");
                const auto& tabs = body<ec::tabs_body>(part(screen, "options:pages"));
                REQUIRE(tabs.tabs.size() >= 5);
                CHECK(tabs.select == "options:page");
                CHECK(tabs.tabs.front().id == "general");
                CHECK(tabs.tabs.front().selected);
                // A row is an option by its name, and a value is set by the editor beside the list.
                const auto& row = row_of(screen, "options:items", "option:SAFEMODEPROXIMITY");
                CHECK(row.columns.front().value == std::to_string(original));
                CHECK(row.enabled);
                CHECK(affordance(screen, "options:select").role == "select");
                CHECK(affordance(screen, "QUIT").role == "cancel");
                CHECK(affordance(screen, "HELP_KEYBINDINGS").enabled);
                CHECK_FALSE(has_part(screen, "options:note"));
                // Rows, tabs and the editor's row are named by identity; an unknown one is refused.
                CHECK(
                    ec::resolve_invoke(
                        state, {.affordance_id = "options:select", .args = {.row = "option:NOPE"}})
                        .error()
                    == ec::error::invalid_arguments);
                CHECK(
                    ec::resolve_invoke(
                        state, {.affordance_id = "options:page", .args = {.row = "page:general"}})
                        .error()
                    == ec::error::invalid_arguments);
                CHECK(
                    ec::resolve_invoke(state, {.affordance_id = "options:set",
                                               .args = {.row = "option:SAFEMODE", .text = "true"}})
                        .error()
                    == ec::error::invalid_arguments);
                return invoke("options:select", {.row = "option:SAFEMODEPROXIMITY"});
            }
            case 1: {
                CHECK(screen.focus_component == "options:items");
                CHECK(screen.focus_row == "option:SAFEMODEPROXIMITY");
                CHECK(row_of(screen, "options:items", "option:SAFEMODEPROXIMITY").selected);
                const auto& field = field_of(screen);
                CHECK(field.id == "option:SAFEMODEPROXIMITY");
                CHECK(field.type == "integer");
                CHECK(field.value == std::to_string(original));
                REQUIRE(field.max);
                maximum = *field.max;
                CHECK(body<ec::panel_body>(part(screen, "options:detail")).title == field.label);
                CHECK(body<ec::text_body>(part(screen, "options:tooltip")).markup.find('#')
                      != std::string::npos);
                // Past the maximum is refused: the value stays.
                return invoke(
                    "options:set",
                    {.row = "option:SAFEMODEPROXIMITY", .text = std::to_string(maximum + 1)});
            }
            case 2:
                CHECK(field_of(screen).value == std::to_string(original));
                // Exponent notation is refused too: the native setValue would read "1e1" as 1,
                // not 10.
                return invoke("options:set", {.row = "option:SAFEMODEPROXIMITY", .text = "1e1"});
            case 3:
                CHECK(field_of(screen).value == std::to_string(original));
                return invoke("options:set", {.row = "option:SAFEMODEPROXIMITY", .text = "7"});
            case 4:
                CHECK(field_of(screen).value == "7");
                CHECK(
                    row_of(screen, "options:items", "option:SAFEMODEPROXIMITY").columns.front().value
                    == "7");
                return invoke("options:page", {.row = "interface"});
            case 5:
                CHECK(selected_tab(screen) == "interface");
                CHECK(std::ranges::none_of(rows(screen, "options:items"), [](const auto& row) {
                    return row.id == "option:SAFEMODEPROXIMITY";
                }));
                return invoke("options:page", {.row = "debug"});
            case 6: {
                // A group is a row that folds its options in and out.
                const auto& group = row_of(screen, "options:items", "group:debug_log");
                CHECK(group.toggled == false);
                CHECK(std::ranges::none_of(rows(screen, "options:items"), [](const auto& row) {
                    return row.parent_id == "group:debug_log";
                }));
                return invoke("options:activate", {.row = "group:debug_log"});
            }
            case 7:
                CHECK(row_of(screen, "options:items", "group:debug_log").toggled == true);
                CHECK(std::ranges::any_of(rows(screen, "options:items"), [](const auto& row) {
                    return row.parent_id == "group:debug_log";
                }));
                // The group is the row under the cursor, and it has a tooltip but no value.
                CHECK(screen.focus_row == "group:debug_log");
                CHECK_FALSE(has_part(screen, "options:editor"));
                CHECK_FALSE(affordance(screen, "options:set").enabled);
                return act("QUIT");
            default:
                FAIL("the options screen outlived its answer");
                return act("QUIT");
        }
    });

    get_options().show(false);
    CHECK(step == 9);
    // The changed value was discarded with the prompt's No.
    CHECK(get_option<int>("SAFEMODEPROXIMITY") == original);
}

TEST_CASE(
    "an option whose prerequisite is off is a disabled row with the native reason and no editor",
    "[options_screen][engine_client_component]") {
    const auto guard = screen_guard{};
    const auto animations = get_option<bool>("ANIMATIONS");
    const auto rain = get_option<bool>("ANIMATION_RAIN");
    get_options().get_option("ANIMATIONS").setValue("false");
    const auto restore = on_out_of_scope([&]() {
        get_options().get_option("ANIMATIONS").setValue(animations ? "true" : "false");
        get_options().get_option("ANIMATION_RAIN").setValue(rain ? "true" : "false");
    });
    auto step = 0;

    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto screen = ec::top_screen(captured());
        switch (step++) {
            case 0:
                return invoke("options:page", {.row = "graphics"});
            case 1: {
                const auto& row = row_of(screen, "options:items", "option:ANIMATION_RAIN");
                CHECK_FALSE(row.enabled);
                CHECK(row.denial.starts_with("Prerequisite for this option not met!"));
                return invoke("options:select", {.row = "option:ANIMATION_RAIN"});
            }
            case 2:
                CHECK_FALSE(has_part(screen, "options:editor"));
                CHECK_FALSE(affordance(screen, "options:set").enabled);
                // A set that the screen no longer offers changes nothing.
                return invoke("options:set", {.row = "option:ANIMATION_RAIN", .text = "false"});
            case 3:
                CHECK(row_of(screen, "options:items", "option:ANIMATION_RAIN").columns.front().value
                      == get_options().get_option("ANIMATION_RAIN").getValueName());
                return act("QUIT");
            default:
                FAIL("the options screen outlived its answer");
                return act("QUIT");
        }
    });

    get_options().show(false);
    CHECK(step == 4);
    CHECK(get_option<bool>("ANIMATION_RAIN") == rain);
}

TEST_CASE(
    "the options screen is the same at any viewport size and has the same ids in another language",
    "[options_screen][engine_client_component]") {
    const auto guard = screen_guard{};
    auto reads = 0;
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        ++reads;
        check_invariance("options");
        return act("QUIT");
    });
    get_options().show(false);
    CHECK(reads == 1);
}

TEST_CASE(
    "the options screen of world creation has no tabs and says World options",
    "[options_screen][engine_client_component]") {
    const auto guard = screen_guard{};
    auto reads = 0;
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        ++reads;
        const auto screen = ec::top_screen(captured());
        CHECK(screen.title == "World options");
        CHECK_FALSE(has_part(screen, "options:pages"));
        CHECK_FALSE(std::ranges::contains(
            screen.affordances | std::views::transform(&ec::affordance::id), "options:page"));
        CHECK(has_part(screen, "options:items"));
        return act("QUIT");
    });
    CHECK(get_options().show(false, true) == "QUIT");
    CHECK(reads == 1);
}

TEST_CASE(
    "the keybindings screen picks an action row in a mode, filters and discards unsaved keys",
    "[keybindings_screen][engine_client_component]") {
    const auto guard = screen_guard{};
    auto ctxt = input_context{"KEYBINDING_TEST"};
    ctxt.register_action("UP");
    ctxt.register_action("DOWN");
    const auto before = inp_mngr.get_input_for_action("UP", "default");
    auto step = 0;
    auto field = std::string{};

    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto state = captured();
        const auto screen = ec::top_screen(state);
        if (screen.context != "HELP_KEYBINDINGS") {
            // The prompts of the screen are the generic ones, over it.
            const auto snapshot = game_client::current_interaction({.limit = 200});
            if (snapshot.field && snapshot.field->type == "key") {
                REQUIRE(step == 6);
                step = 7;
                return resolve(
                    {.input_id = snapshot.input_id,
                     .operation = game_client::interaction_operation::fill,
                     .target_id = snapshot.field->id,
                     .value = "F9",
                     .submit = true});
            }
            REQUIRE(step == 9);
            step = 10;
            return answer_no();
        }
        switch (step++) {
            case 0: {
                CHECK(screen.title == "Keybindings");
                CHECK(body<ec::panel_body>(part(screen, "keybindings:window")).title
                      == "Keybindings");
                CHECK(body<ec::panel_body>(part(screen, "keybindings:window")).stats.empty());
                // The legend is native text, and every action of the context is a row by its id.
                CHECK_FALSE(body<ec::text_body>(part(screen, "keybindings:legend")).markup.empty());
                const auto& up = row_of(screen, "keybindings:actions", "action:UP");
                CHECK(up.label == ctxt.get_action_name("UP"));
                CHECK(up.columns.at(0).key == "keys");
                CHECK(up.columns.at(1).key == "scope");
                CHECK(up.color == "c_light_gray");
                CHECK(affordance(screen, "ADD_GLOBAL").label == mode_name("ADD_GLOBAL"));
                CHECK(affordance(screen, "QUIT").role == "cancel");
                // Nothing is picked before a mode is chosen, and the filter is open.
                CHECK_FALSE(affordance(screen, "keybindings:pick").enabled);
                CHECK(affordance(screen, "keybindings:filter").enabled);
                CHECK_FALSE(
                    ec::resolve_invoke(
                        state, {.affordance_id = "keybindings:pick", .args = {.row = "action:UP"}})
                        .has_value());
                return invoke("keybindings:select", {.row = "action:DOWN"});
            }
            case 1:
                CHECK(screen.focus_row == "action:DOWN");
                CHECK(row_of(screen, "keybindings:actions", "action:DOWN").selected);
                return invoke("keybindings:filter", {.text = ctxt.get_action_name("DOWN")});
            case 2: {
                // The filter is the screen's, and the rows are what it lets through.
                CHECK(body<ec::panel_body>(part(screen, "keybindings:window")).filter
                      == ctxt.get_action_name("DOWN"));
                const auto& shown = rows(screen, "keybindings:actions");
                REQUIRE(shown.size() == 1);
                CHECK(shown.front().id == "action:DOWN");
                return invoke("keybindings:filter", {.text = ""});
            }
            case 3:
                CHECK(rows(screen, "keybindings:actions").size() == 2);
                return act("ADD_GLOBAL");
            case 4: {
                // The mode is a fact of the screen, and picking is on.
                const auto& stats = body<ec::panel_body>(part(screen, "keybindings:window")).stats;
                REQUIRE(stats.size() == 1);
                CHECK(stats.front().key == "mode");
                CHECK(stats.front().value == mode_name("ADD_GLOBAL"));
                CHECK(affordance(screen, "keybindings:pick").enabled);
                CHECK(affordance(screen, "keybindings:pick").label == stats.front().value);
                CHECK_FALSE(affordance(screen, "keybindings:filter").enabled);
                CHECK(
                    ec::resolve_invoke(
                        state,
                        {.affordance_id = "keybindings:pick", .args = {.row = "action:NOPE"}})
                        .error()
                    == ec::error::invalid_arguments);
                return invoke("keybindings:select", {.row = "action:UP"});
            }
            case 5:
                CHECK(screen.focus_row == "action:UP");
                return invoke("keybindings:pick", {.row = "action:UP"});
            case 6:
                FAIL("picking a row opens the key prompt before the screen reads again");
                return act("QUIT");
            case 7: {
                const auto& up = row_of(screen, "keybindings:actions", "action:UP");
                CHECK(up.columns.front().value.find("F9") != std::string::npos);
                CHECK(body<ec::panel_body>(part(screen, "keybindings:window")).stats.empty());
                ++step;
                return act("QUIT");
            }
            default:
                FAIL("the keybindings screen outlived its answer");
                return act("QUIT");
        }
    });

    static_cast<void>(ctxt.display_menu());
    CHECK(step == 10);
    CHECK(inp_mngr.get_input_for_action("UP", "default") == before);
}

TEST_CASE(
    "executing an action row of the keybindings screen returns the game action",
    "[keybindings_screen][engine_client_component]") {
    const auto guard = screen_guard{};
    auto ctxt = input_context{"KEYBINDING_TEST"};
    ctxt.register_action("pause");
    auto step = 0;

    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto screen = ec::top_screen(captured());
        switch (step++) {
            case 0:
                CHECK(affordance(screen, "EXECUTE").label == mode_name("EXECUTE"));
                return act("EXECUTE");
            case 1:
                CHECK(affordance(screen, "keybindings:pick").label == mode_name("EXECUTE"));
                return invoke("keybindings:pick", {.row = "action:pause"});
            default:
                FAIL("the keybindings screen outlived its execute");
                return act("QUIT");
        }
    });

    CHECK(ctxt.display_menu(true) == ACTION_PAUSE);
    CHECK(step == 2);
}

TEST_CASE(
    "the keybindings screen is the same at any viewport size and has the same ids in another language",
    "[keybindings_screen][engine_client_component]") {
    const auto guard = screen_guard{};
    auto ctxt = input_context{"KEYBINDING_TEST"};
    ctxt.register_action("UP");
    auto reads = 0;
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        ++reads;
        check_invariance("keybindings");
        return act("QUIT");
    });
    static_cast<void>(ctxt.display_menu());
    CHECK(reads == 1);
}

#endif
