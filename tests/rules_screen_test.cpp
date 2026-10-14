#if defined(CATA_MCP)

#    include "catch/catch.hpp"
#    include "client_command.h"
#    include "client_input.h"
#    include "client_interaction.h"
#    include "client_memory.h"
#    include "client_memory_scope.h"
#    include "color.h"
#    include "cursesdef.h"
#    include "cursesport.h"
#    include "distraction_manager.h"
#    include "engine_client_component.h"
#    include "engine_client_contract.h"
#    include "game_constants.h"
#    include "input.h"
#    include "locale_fixture.h"
#    include "locale_scope.h"
#    include "output.h"
#    include "path_info.h"
#    include "state_helpers.h"

#    include <algorithm>
#    include <filesystem>
#    include <fstream>
#    include <map>
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

/// Every distraction enabled, whatever the profile holds; the profile's own are put back.
struct distractions_guard {
    const std::map<distraction_type, bool> saved = get_distraction_manager().distractions;

    distractions_guard() {
        for (auto i = 0; i < static_cast<int>(distraction_type::num_distraction_type); ++i) {
            get_distraction_manager().distractions[static_cast<distraction_type>(i)] = false;
        }
    }

    ~distractions_guard() { get_distraction_manager().distractions = saved; }
};

/// A colour manager of its own, so the profile's colours stay as they are, with black's normal
/// colour set.
auto colors_with_a_custom() -> color_manager {
    const auto custom = std::filesystem::path{PATH_INFO::user_dir()} / "rules_screen_colors.json";
    {
        auto out = std::ofstream{custom};
        out << R"([{"name":"c_black","custom":"c_red","invertcustom":""}])";
    }
    auto colors = color_manager{};
    colors.load_default();
    colors.load_custom(custom.string());
    std::filesystem::remove(custom);
    return colors;
}

auto captured() -> ec::boundary_state {
    const auto state = ec::capture_boundary(
        {.epoch = "epoch:rules",
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
auto answer_no() -> input_event {
    const auto snapshot = game_client::current_interaction({.limit = 200});
    const auto no =
        std::ranges::find(snapshot.choices, "No", &game_client::interaction_choice::label);
    REQUIRE(no != snapshot.choices.end());
    auto input = game_client::input_command{};
    input.interaction = game_client::interaction_command{
        .input_id = snapshot.input_id,
        .operation = game_client::interaction_operation::choose,
        .target_id = no->id};
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

constexpr auto first_distraction = "distraction:Noise";
constexpr auto second_distraction = "distraction:Pain";

} // namespace

TEST_CASE(
    "the distraction manager is driven by its rows and switches a distraction by id",
    "[rules_screen][distractions_screen][engine_client_component]") {
    const auto guard = screen_guard{};
    const auto distractions = distractions_guard{};
    auto step = 0;

    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto state = captured();
        const auto screen = ec::top_screen(state);
        REQUIRE(screen.context == "DISTRACTION_MANAGER");
        switch (step++) {
            case 0: {
                CHECK(screen.title == "Distractions manager");
                CHECK(screen.role == "screen");
                // A row is a distraction by its token, with its state as a switch and a status.
                const auto& all = rows(screen, "distractions:items");
                REQUIRE(all.size()
                        == static_cast<std::size_t>(distraction_type::num_distraction_type) - 1);
                CHECK(all.front().id == first_distraction);
                CHECK(all.front().label == "Noise");
                CHECK(all.front().selected);
                CHECK(all.front().toggled == true);
                CHECK(column_of(all.front(), "status").value == "Enabled");
                CHECK(column_of(all.front(), "status").color == "c_light_green");
                CHECK(row_of(screen, "distractions:items", "distraction:Hostile Far").label
                      == "Hostile Spotted");
                CHECK(body<ec::text_body>(part(screen, "distractions:description")).markup
                      == "Interrupts you if you hear a noise.");
                CHECK(screen.focus_component == "distractions:items");
                CHECK(screen.focus_row == first_distraction);
                CHECK(affordance(screen, "distractions:select").role == "select");
                CHECK(affordance(screen, "distractions:toggle").label
                      == affordance(screen, "CONFIRM").label);
                CHECK(affordance(screen, "QUIT").role == "cancel");
                CHECK(affordance(screen, "HELP_KEYBINDINGS").enabled);
                // Rows are named by identity; an unknown one, or none, is refused.
                CHECK(refused(state, "distractions:select", {.row = "distraction:Nothing"}));
                CHECK(refused(state, "distractions:toggle", {.row = "entry:0"}));
                CHECK(refused(state, "distractions:toggle", {}));
                return invoke("distractions:select", {.row = second_distraction});
            }
            case 1: {
                // Selecting moves the native cursor and changes nothing else.
                CHECK(screen.focus_row == second_distraction);
                CHECK(row_of(screen, "distractions:items", second_distraction).selected);
                CHECK_FALSE(row_of(screen, "distractions:items", first_distraction).selected);
                CHECK(row_of(screen, "distractions:items", second_distraction).toggled == true);
                CHECK(body<ec::text_body>(part(screen, "distractions:description")).markup
                      == "Interrupts you if you feel pain.");
                CHECK_FALSE(get_distraction_manager().distractions.at(distraction_type::pain));
                return invoke("distractions:toggle", {.row = second_distraction});
            }
            case 2: {
                const auto& pain = row_of(screen, "distractions:items", second_distraction);
                CHECK(pain.toggled == false);
                CHECK(column_of(pain, "status").value == "Disabled");
                CHECK(column_of(pain, "status").color == "c_red");
                CHECK(get_distraction_manager().distractions.at(distraction_type::pain));
                CHECK(row_of(screen, "distractions:items", first_distraction).toggled == true);
                // The registered action does the same at the cursor.
                return act("CONFIRM");
            }
            case 3:
                CHECK(row_of(screen, "distractions:items", second_distraction).toggled == true);
                CHECK_FALSE(get_distraction_manager().distractions.at(distraction_type::pain));
                return act("QUIT");
            default:
                FAIL("the distraction manager outlived its answer");
                return act("QUIT");
        }
    });

    get_distraction_manager().show();
    CHECK(step == 4);
}

TEST_CASE(
    "the distraction manager is the same at any viewport size and has the same ids in another language",
    "[rules_screen][distractions_screen][engine_client_component]") {
    const auto guard = screen_guard{};
    const auto distractions = distractions_guard{};
    auto reads = 0;
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        ++reads;
        check_invariance("distractions");
        return act("QUIT");
    });
    get_distraction_manager().show();
    CHECK(reads == 1);
}

TEST_CASE(
    "the colour manager picks a column and a colour by id and sets a custom colour",
    "[rules_screen][colors_screen][engine_client_component]") {
    const auto guard = screen_guard{};
    auto colors = colors_with_a_custom();
    auto step = 0;

    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto state = captured();
        if (step == 7) {
            // The question over the colour screen, which the screen below it leaves unanswered.
            const auto stack = ec::interaction_stack(state);
            REQUIRE(stack.size() >= 2);
            CHECK(stack[stack.size() - 2].context == "COLORS");
            CHECK(stack[stack.size() - 2].affordances.empty());
            ++step;
            return answer_no();
        }
        const auto screen = ec::top_screen(state);
        REQUIRE(screen.context == "COLORS");
        switch (step++) {
            case 0: {
                CHECK(screen.title == " COLOR MANAGER ");
                CHECK(screen.role == "screen");
                const auto& columns = body<ec::tabs_body>(part(screen, "colors:columns"));
                REQUIRE(columns.tabs.size() == 2);
                CHECK(columns.select == "colors:column");
                CHECK(columns.tabs[0].id == "normal");
                CHECK(columns.tabs[0].selected);
                CHECK(columns.tabs[1].id == "invert");
                CHECK_FALSE(columns.tabs[1].selected);
                // A row is a colour by its name, with the colour it is and the customs set on it.
                const auto& all = rows(screen, "colors:items");
                CHECK(all.size() > 100);
                CHECK(all.front().id == "color:c_black");
                CHECK(all.front().label == "c_black");
                CHECK(all.front().selected);
                CHECK(column_of(all.front(), "normal").value == "default");
                CHECK(column_of(all.front(), "normal").color == "c_black");
                CHECK(column_of(all.front(), "normal_custom").value == "c_red");
                CHECK(column_of(all.front(), "normal_custom").color == "c_red");
                CHECK(column_of(all.front(), "invert").color == "c_black_white");
                CHECK(column_of(all.front(), "invert_custom").value.empty());
                CHECK(screen.focus_row == "color:c_black");
                // The editor is the colour under the cursor's, with every colour on offer.
                const auto& field =
                    body<ec::form_body>(part(screen, "colors:editor")).fields.front();
                CHECK(field.id == "color:c_black");
                CHECK(field.type == "select");
                CHECK(field.value == "c_red");
                CHECK(field.options.size() == all.size());
                CHECK(affordance(screen, "colors:select").role == "select");
                CHECK(affordance(screen, "colors:column").enabled);
                CHECK(affordance(screen, "REMOVE_CUSTOM").enabled);
                CHECK(affordance(screen, "LOAD_TEMPLATE").enabled);
                CHECK(affordance(screen, "QUIT").role == "cancel");
                // Names are the identity of rows and columns; position and unknown names are
                // refused.
                CHECK(refused(state, "colors:select", {.row = "color:nothing"}));
                CHECK(refused(state, "colors:select", {.row = "entry:0"}));
                CHECK(refused(state, "colors:column", {.row = "middle"}));
                CHECK(refused(state, "colors:set", {.row = "color:c_blue", .text = "c_red"}));
                CHECK(
                    refused(state, "colors:set", {.row = "color:c_black", .text = "not_a_color"}));
                CHECK(refused(state, "colors:set", {.row = "color:c_black", .text = ""}));
                return invoke("colors:column", {.row = "invert"});
            }
            case 1: {
                const auto& columns = body<ec::tabs_body>(part(screen, "colors:columns"));
                CHECK(columns.tabs[1].selected);
                CHECK_FALSE(columns.tabs[0].selected);
                return invoke("colors:select", {.row = "color:c_blue"});
            }
            case 2: {
                CHECK(screen.focus_row == "color:c_blue");
                CHECK(row_of(screen, "colors:items", "color:c_blue").selected);
                CHECK_FALSE(row_of(screen, "colors:items", "color:c_black").selected);
                CHECK(body<ec::form_body>(part(screen, "colors:editor")).fields.front().id
                      == "color:c_blue");
                return invoke("colors:set", {.row = "color:c_blue", .text = "c_light_green"});
            }
            case 3: {
                // The custom colour is on the invert column of the row under the cursor, in its own
                // colour.
                const auto& blue = row_of(screen, "colors:items", "color:c_blue");
                CHECK(column_of(blue, "invert_custom").value == "c_light_green");
                CHECK(column_of(blue, "invert_custom").color == "c_light_green");
                CHECK(column_of(blue, "normal_custom").value.empty());
                CHECK(body<ec::form_body>(part(screen, "colors:editor")).fields.front().value
                      == "c_light_green");
                return invoke("colors:column", {.row = "normal"});
            }
            case 4:
                // The other column has none yet.
                CHECK(
                    body<ec::form_body>(part(screen, "colors:editor")).fields.front().value.empty());
                return invoke("colors:column", {.row = "invert"});
            case 5:
                // The registered action clears the custom colour of the cursor's column.
                return act("REMOVE_CUSTOM");
            case 6:
                CHECK(column_of(row_of(screen, "colors:items", "color:c_blue"), "invert_custom")
                          .value.empty());
                return act("QUIT");
            default:
                FAIL("the colour manager outlived its answer");
                return act("QUIT");
        }
    });

    colors.show_gui();
    CHECK(step == 8);
}

TEST_CASE(
    "the colour manager is the same at any viewport size and has the same ids in another language",
    "[rules_screen][colors_screen][engine_client_component]") {
    const auto guard = screen_guard{};
    auto colors = colors_with_a_custom();
    auto reads = 0;
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        ++reads;
        check_invariance("colors");
        return act("QUIT");
    });
    colors.show_gui();
    CHECK(reads == 1);
}

#endif // CATA_MCP
