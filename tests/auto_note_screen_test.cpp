#if defined(CATA_MCP)

#    include "auto_note.h"
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
#    include "mapgen/map_extras.h"
#    include "options.h"
#    include "options_helpers.h"
#    include "output.h"
#    include "point.h"
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
        clear_all_state();
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

/// The session's auto note settings, put back after the test.
struct settings_guard {
    const auto_notes::auto_note_settings saved = get_auto_notes_settings();

    settings_guard() { get_auto_notes_settings() = auto_notes::auto_note_settings{}; }

    ~settings_guard() { get_auto_notes_settings() = saved; }
};

constexpr auto crater = "extra:mx_crater";
constexpr auto campers = "extra:mx_collegekids";
constexpr auto helicopter = "extra:mx_helicopter";

/// Three discovered map extras, the second with its note off.
void discover_three() {
    auto& settings = get_auto_notes_settings();
    settings.default_initialize();
    for (const auto* const extra : {"mx_crater", "mx_collegekids", "mx_helicopter"}) {
        settings.set_discovered(string_id<map_extra>{extra});
    }
    settings.set_auto_note_status(string_id<map_extra>{"mx_collegekids"}, false);
}

auto captured() -> ec::boundary_state {
    const auto state = ec::capture_boundary(
        {.epoch = "epoch:auto_note",
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

/// What a client sends to the popup on top: picks the option so labelled.
auto answer(const std::string& label) -> input_event {
    const auto snapshot = game_client::current_interaction({.limit = 200});
    const auto option =
        std::ranges::find(snapshot.choices, label, &game_client::interaction_choice::label);
    REQUIRE(option != snapshot.choices.end());
    auto input = game_client::input_command{};
    input.interaction = game_client::interaction_command{
        .input_id = snapshot.input_id,
        .operation = game_client::interaction_operation::choose,
        .target_id = option->id};
    const auto result =
        game_client::resolve_input_command(input, game_client::memory::screen_size());
    REQUIRE(result.has_value());
    return *result;
}

auto part(const ec::screen_view& screen, const std::string& id) -> const ec::component& {
    REQUIRE(screen.root);
    const auto* const found = ec::find_component(*screen.root, id);
    INFO(id);
    REQUIRE(found != nullptr);
    return *found;
}
template <typename Body> auto body(const ec::component& value) -> const Body& {
    REQUIRE(std::holds_alternative<Body>(value.body));
    return std::get<Body>(value.body);
}
auto rows(const ec::screen_view& screen) -> const std::vector<ec::row>& {
    return body<ec::list_body>(part(screen, "autonote:extras")).rows;
}
auto row_of(const ec::screen_view& screen, const std::string& id) -> const ec::row& {
    const auto& all = rows(screen);
    const auto found = std::ranges::find(all, id, &ec::row::id);
    INFO(id);
    REQUIRE(found != all.end());
    return *found;
}
auto has_affordance(const ec::screen_view& screen, const std::string& id) -> bool {
    return std::ranges::contains(screen.affordances, id, &ec::affordance::id);
}
auto affordance(const ec::screen_view& screen, const std::string& id) -> const ec::affordance& {
    const auto found = std::ranges::find(screen.affordances, id, &ec::affordance::id);
    INFO(id);
    REQUIRE(found != screen.affordances.end());
    return *found;
}
auto column_of(const ec::row& row, const std::string& key)
    -> const game_client::interaction_column& {
    const auto found = std::ranges::find(row.columns, key, &game_client::interaction_column::key);
    INFO(key);
    REQUIRE(found != row.columns.end());
    return *found;
}
/// Whether invoking the affordance with the arguments is refused as invalid.
auto refused(
    const ec::boundary_state& state, const std::string& affordance, const ec::invoke_args& args)
    -> bool {
    return ec::resolve_invoke(state, {.affordance_id = affordance, .args = args}).error()
        == ec::error::invalid_arguments;
}
auto notes_enabled(const char* const extra) -> bool {
    return get_auto_notes_settings().has_auto_note_enabled(string_id<map_extra>{extra});
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

} // namespace

TEST_CASE(
    "the auto notes manager lists the discovered map extras by id and toggles one by its row",
    "[auto_note_screen][engine_client_component]") {
    const auto guard = screen_guard{};
    const auto settings = settings_guard{};
    const auto option = override_option{"AUTO_NOTES", "true"};
    discover_three();
    auto gui = auto_notes::auto_note_manager_gui{};
    auto step = 0;

    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto state = captured();
        const auto screen = ec::top_screen(state);
        switch (step++) {
            case 0: {
                REQUIRE(screen.context == "AUTO_NOTES");
                CHECK(screen.role == "screen");
                CHECK(screen.title == " AUTO NOTES MANAGER ");
                // A row is a map extra by its data name, with its symbol and note state.
                const auto& all = rows(screen);
                REQUIRE(all.size() == 3);
                CHECK(all[0].id == crater);
                CHECK(all[1].id == campers);
                CHECK(all[2].id == helicopter);
                CHECK(all[0].label == "Crater");
                CHECK(all[0].selected);
                CHECK_FALSE(all[1].selected);
                CHECK(all[0].toggled == true);
                CHECK(all[1].toggled == false);
                CHECK(column_of(all[0], "symbol").value == "o");
                CHECK(column_of(all[0], "symbol").color == "c_red");
                CHECK(column_of(all[2], "symbol").color == "c_light_blue");
                CHECK(column_of(all[0], "enabled").value == "yes");
                CHECK(column_of(all[0], "enabled").color == "c_green");
                CHECK(column_of(all[1], "enabled").value == "no");
                CHECK(column_of(all[1], "enabled").color == "c_red");
                CHECK(screen.focus_component == "autonote:extras");
                CHECK(screen.focus_row == crater);
                // What the option says beside the list.
                const auto& stat = body<ec::panel_body>(part(screen, "autonote:status")).stats;
                REQUIRE(stat.size() == 1);
                CHECK(stat[0].label == "Auto notes enabled:");
                CHECK(stat[0].value == "True");
                CHECK(stat[0].tone == "good");
                CHECK(affordance(screen, "autonote:select").role == "select");
                CHECK(affordance(screen, "autonote:toggle").label
                      == affordance(screen, "CONFIRM").label);
                CHECK(affordance(screen, "SWITCH_AUTO_NOTE_OPTION").enabled);
                CHECK(affordance(screen, "ENABLE_MAPEXTRA_NOTE").enabled);
                CHECK(affordance(screen, "DISABLE_MAPEXTRA_NOTE").enabled);
                CHECK(affordance(screen, "QUIT").role == "cancel");
                // Rows are named by identity; position, an unknown extra and a missing argument
                // are refused.
                CHECK(refused(state, "autonote:select", {.row = "extra:0"}));
                CHECK(refused(state, "autonote:select", {.row = "extra:mx_nothing"}));
                CHECK(refused(state, "autonote:toggle", {.row = "entry:0"}));
                CHECK(refused(state, "autonote:toggle", {}));
                return invoke("autonote:select", {.row = campers});
            }
            case 1:
                // Selecting moves the native cursor and changes nothing else.
                CHECK(screen.focus_row == campers);
                CHECK(row_of(screen, campers).selected);
                CHECK_FALSE(row_of(screen, crater).selected);
                CHECK(row_of(screen, campers).toggled == false);
                return invoke("autonote:toggle", {.row = campers});
            case 2:
                CHECK(row_of(screen, campers).toggled == true);
                CHECK(column_of(row_of(screen, campers), "enabled").value == "yes");
                // The session's settings change only when the changes are saved.
                CHECK_FALSE(notes_enabled("mx_collegekids"));
                // The registered actions act at the cursor.
                return act("DISABLE_MAPEXTRA_NOTE");
            case 3:
                CHECK(row_of(screen, campers).toggled == false);
                return invoke("autonote:toggle", {.row = helicopter});
            case 4:
                CHECK(screen.focus_row == helicopter);
                CHECK(row_of(screen, helicopter).toggled == false);
                CHECK(row_of(screen, campers).toggled == false);
                return act("ENABLE_MAPEXTRA_NOTE");
            case 5:
                CHECK(row_of(screen, helicopter).toggled == true);
                return act("QUIT");
            case 6:
                // The save is asked before the settings take the changes.
                return answer("Yes");
            default:
                FAIL("the auto notes manager outlived its answer");
                return act("QUIT");
        }
    });

    gui.show();
    CHECK(step == 7);
    CHECK(gui.was_changed());
    CHECK(notes_enabled("mx_crater"));
    CHECK_FALSE(notes_enabled("mx_collegekids"));
    CHECK(notes_enabled("mx_helicopter"));
}

TEST_CASE(
    "the auto notes manager leaves the settings alone when the changes are refused",
    "[auto_note_screen][engine_client_component]") {
    const auto guard = screen_guard{};
    const auto settings = settings_guard{};
    discover_three();
    auto gui = auto_notes::auto_note_manager_gui{};
    auto step = 0;

    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        switch (step++) {
            case 0:
                return invoke("autonote:toggle", {.row = crater});
            case 1:
                return act("QUIT");
            case 2:
                return answer("No");
            default:
                FAIL("the auto notes manager outlived its answer");
                return act("QUIT");
        }
    });

    gui.show();
    CHECK(step == 3);
    CHECK(notes_enabled("mx_crater"));
}

TEST_CASE(
    "the auto notes manager switches the option and says why it lists nothing before a discovery",
    "[auto_note_screen][engine_client_component]") {
    const auto guard = screen_guard{};
    const auto settings = settings_guard{};
    const auto option = override_option{"AUTO_NOTES", "true"};
    const auto map_extras = override_option{"AUTO_NOTES_MAP_EXTRAS", "true"};
    get_auto_notes_settings().default_initialize();
    auto gui = auto_notes::auto_note_manager_gui{};
    auto step = 0;

    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto screen = ec::top_screen(captured());
        REQUIRE(screen.context == "AUTO_NOTES");
        switch (step++) {
            case 0:
                CHECK(rows(screen).empty());
                CHECK(body<ec::text_body>(part(screen, "autonote:notice")).markup
                      == "Discover more special encounters to populate this list");
                // With nothing to act on, only the option and the way out remain.
                CHECK(has_affordance(screen, "SWITCH_AUTO_NOTE_OPTION"));
                CHECK(has_affordance(screen, "QUIT"));
                CHECK_FALSE(has_affordance(screen, "autonote:select"));
                CHECK_FALSE(has_affordance(screen, "autonote:toggle"));
                CHECK_FALSE(has_affordance(screen, "ENABLE_MAPEXTRA_NOTE"));
                CHECK_FALSE(has_affordance(screen, "DISABLE_MAPEXTRA_NOTE"));
                CHECK(screen.focus_row.empty());
                CHECK(
                    body<ec::panel_body>(part(screen, "autonote:status")).stats[0].value == "True");
                return act("SWITCH_AUTO_NOTE_OPTION");
            case 1: {
                const auto& stat = body<ec::panel_body>(part(screen, "autonote:status")).stats;
                CHECK(stat[0].value == "False");
                CHECK(stat[0].tone == "bad");
                return act("QUIT");
            }
            default:
                FAIL("the auto notes manager outlived its answer");
                return act("QUIT");
        }
    });

    gui.show();
    CHECK(step == 2);
    CHECK_FALSE(gui.was_changed());
}

TEST_CASE(
    "the auto notes manager is the same at any viewport size and has the same ids in another language",
    "[auto_note_screen][engine_client_component]") {
    const auto guard = screen_guard{};
    const auto settings = settings_guard{};
    const auto option = override_option{"AUTO_NOTES", "true"};
    discover_three();
    auto gui = auto_notes::auto_note_manager_gui{};
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
        const auto fixture_ko = locale_fixture::korean_catalogue{};
        const auto korean = [] {
            const auto scope = localization::locale_scope(locale_fixture::korean);
            return published();
        }();
        auto english_ids = std::vector<std::string>{};
        auto korean_ids = std::vector<std::string>{};
        collect_ids(narrow, english_ids);
        collect_ids(korean, korean_ids);
        CHECK_FALSE(english_ids.empty());
        CHECK(english_ids == korean_ids);
        CHECK(without_words(narrow) == without_words(korean));
        return act("QUIT");
    });
    gui.show();
    CHECK(reads == 1);
}

#endif // CATA_MCP
