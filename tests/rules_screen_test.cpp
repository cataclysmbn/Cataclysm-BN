#if defined(CATA_MCP)

#    include "auto_pickup.h"
#    include "avatar.h"
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
#    include "json.h"
#    include "locale_fixture.h"
#    include "locale_scope.h"
#    include "monstergenerator.h"
#    include "mtype.h"
#    include "options.h"
#    include "output.h"
#    include "path_info.h"
#    include "safemode_ui.h"
#    include "state_helpers.h"

#    include <algorithm>
#    include <filesystem>
#    include <fstream>
#    include <iterator>
#    include <map>
#    include <nlohmann/json.hpp>
#    include <ranges>
#    include <sstream>
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

/// The character's name for the screen, put back after it.
struct name_guard {
    const std::string saved = get_avatar().name;

    explicit name_guard(const std::string& name) { get_avatar().name = name; }

    ~name_guard() { get_avatar().name = saved; }
};

/// A file under the test profile written with `content` and removed after.
struct profile_file {
    const std::filesystem::path path;

    profile_file(const std::string& named, const std::string& content): path(named) {
        REQUIRE(named.starts_with(PATH_INFO::user_dir()));
        std::filesystem::create_directories(path.parent_path());
        auto out = std::ofstream{path};
        out << content;
    }

    ~profile_file() { std::filesystem::remove(path); }

    auto content() const -> std::string {
        auto in = std::ifstream{path};
        return {std::istreambuf_iterator<char>{in}, {}};
    }
};

constexpr auto pickup_rules =
    R"([{"rule":"*knife*","active":true,"exclude":false},)"
    R"({"rule":"","active":false,"exclude":false},)"
    R"({"rule":"sledge hammer","active":true,"exclude":true}])";
constexpr auto safemode_rules =
    R"([{"rule":"zombie","active":true,"whitelist":false,"attitude":0,"proximity":10,"category":0},)"
    R"({"rule":"*","active":true,"whitelist":true,"attitude":3,"proximity":0,"category":0},)"
    R"({"rule":"a loud*","active":false,"whitelist":true,"attitude":0,"proximity":5,"category":1}])";

/// What a client sends to the popup on top: leaves it for `cancel`, else picks the option so
/// labelled.
auto answer(const std::string& token) -> input_event {
    const auto snapshot = game_client::current_interaction({.limit = 200});
    auto input = game_client::input_command{};
    if (token == "cancel") {
        input.interaction = game_client::interaction_command{
            .input_id = snapshot.input_id, .operation = game_client::interaction_operation::cancel};
    } else {
        const auto option =
            std::ranges::find(snapshot.choices, token, &game_client::interaction_choice::label);
        REQUIRE(option != snapshot.choices.end());
        input.interaction = game_client::interaction_command{
            .input_id = snapshot.input_id,
            .operation = game_client::interaction_operation::choose,
            .target_id = option->id};
    }
    const auto result =
        game_client::resolve_input_command(input, game_client::memory::screen_size());
    REQUIRE(result.has_value());
    return *result;
}

/// The ids of the rows of the list, which a rule screen names by identity.
auto ids_of(const ec::screen_view& screen, const std::string& list) -> std::vector<std::string> {
    return rows(screen, list) | std::views::transform(&ec::row::id)
         | std::ranges::to<std::vector>();
}

/// Whether `id` is a rule's row id: `rule:` and the number the session gave the rule, never 0.
auto is_rule_id(const std::string& id) -> bool {
    const auto serial = id.substr(std::min<std::size_t>(5, id.size()));
    return id.starts_with("rule:") && !serial.empty() && serial != "0"
        && std::ranges::all_of(serial, [](const char c) { return c >= '0' && c <= '9'; });
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

TEST_CASE(
    "the auto pickup manager is driven by its rows and edits rules by id",
    "[rules_screen][autopickup_screen][engine_client_component]") {
    const auto guard = screen_guard{};
    const auto named = name_guard{"Tester"};
    const auto file = profile_file{PATH_INFO::autopickup(), pickup_rules};
    auto settings = auto_pickup::player_settings{};
    settings.load_global();
    auto ids = std::vector<std::string>{};
    auto copy = std::string{};
    auto step = 0;

    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto state = captured();
        if (step == 11) {
            // The question over the manager, which the screen below it leaves unanswered.
            const auto stack = ec::interaction_stack(state);
            REQUIRE(stack.size() >= 2);
            CHECK(stack[stack.size() - 2].context == "AUTO_PICKUP");
            CHECK(stack[stack.size() - 2].affordances.empty());
            ++step;
            return answer("No");
        }
        const auto screen = ec::top_screen(state);
        if (step == 9) {
            // The text entry of a rule is the popup over the manager.
            CHECK(screen.context != "AUTO_PICKUP");
            ++step;
            return answer("cancel");
        }
        REQUIRE(screen.context == "AUTO_PICKUP");
        switch (step++) {
            case 0: {
                CHECK(screen.title == " AUTO PICKUP MANAGER ");
                CHECK(screen.role == "screen");
                const auto& tabs = body<ec::tabs_body>(part(screen, "autopickup:tabs"));
                REQUIRE(tabs.tabs.size() == 2);
                CHECK(tabs.select == "autopickup:tab");
                CHECK(tabs.tabs[0].id == "global");
                CHECK(tabs.tabs[0].selected);
                CHECK(tabs.tabs[1].id == "character");
                CHECK_FALSE(tabs.tabs[1].selected);
                // A row is a rule by the number the session gave it, with its state.
                ids = ids_of(screen, "autopickup:rules");
                REQUIRE(ids.size() == 3);
                CHECK(std::ranges::all_of(ids, is_rule_id));
                CHECK(ids[0] != ids[1]);
                CHECK(ids[1] != ids[2]);
                const auto& all = rows(screen, "autopickup:rules");
                CHECK(all[0].label == "*knife*");
                CHECK(all[1].label == "<empty rule>");
                CHECK(all[2].label == "sledge hammer");
                CHECK(all[0].selected);
                CHECK(column_of(all[0], "i_e").value == "Include");
                CHECK(column_of(all[2], "i_e").value == "Exclude");
                CHECK(column_of(all[0], "active").value == "True");
                CHECK(column_of(all[1], "active").value == "False");
                CHECK(all[0].color == "c_white");
                CHECK(all[1].color == "c_light_gray");
                CHECK(screen.focus_component == "autopickup:rules");
                CHECK(screen.focus_row == ids[0]);
                const auto& stat = body<ec::panel_body>(part(screen, "autopickup:details")).stats;
                REQUIRE(stat.size() == 1);
                CHECK(stat[0].value == (get_option<bool>("AUTO_PICKUP") ? "True" : "False"));
                CHECK(body<ec::text_body>(part(screen, "autopickup:help"))
                          .markup.starts_with("* is used as a Wildcard."));
                CHECK(affordance(screen, "autopickup:select").role == "select");
                CHECK(affordance(screen, "autopickup:edit").label == "Rules");
                CHECK(affordance(screen, "autopickup:mode").label == "I/E");
                CHECK(affordance(screen, "ADD_RULE").enabled);
                CHECK(affordance(screen, "REMOVE_RULE").enabled);
                CHECK(affordance(screen, "TEST_RULE").enabled);
                CHECK(affordance(screen, "QUIT").role == "cancel");
                // Rules and tabs are named by identity; position, an unknown name and a missing or
                // extra argument are refused.
                CHECK(refused(state, "autopickup:select", {.row = "rule:0"}));
                CHECK(refused(state, "autopickup:select", {.row = "entry:0"}));
                CHECK(refused(state, "autopickup:edit", {}));
                CHECK(refused(state, "autopickup:mode", {.row = ids[0], .text = "x"}));
                CHECK(refused(state, "autopickup:tab", {.row = "middle"}));
                CHECK(refused(state, "autopickup:tab", {.row = ids[0]}));
                return invoke("autopickup:select", {.row = ids[2]});
            }
            case 1:
                CHECK(screen.focus_row == ids[2]);
                CHECK(row_of(screen, "autopickup:rules", ids[2]).selected);
                CHECK_FALSE(row_of(screen, "autopickup:rules", ids[0]).selected);
                return invoke("autopickup:mode", {.row = ids[2]});
            case 2:
                // The sledge hammer rule includes now, and nothing else changed.
                CHECK(column_of(row_of(screen, "autopickup:rules", ids[2]), "i_e").value
                      == "Include");
                CHECK(column_of(row_of(screen, "autopickup:rules", ids[0]), "i_e").value
                      == "Include");
                return invoke("autopickup:select", {.row = ids[1]});
            case 3:
                CHECK(screen.focus_row == ids[1]);
                return act("REMOVE_RULE");
            case 4:
                // The rules after the removed one keep their ids.
                CHECK(ids_of(screen, "autopickup:rules") == std::vector{ids[0], ids[2]});
                CHECK(refused(state, "autopickup:select", {.row = ids[1]}));
                CHECK(screen.focus_row == ids[2]);
                return act("COPY_RULE");
            case 5: {
                // A copy is a new rule with an id of its own.
                const auto now = ids_of(screen, "autopickup:rules");
                REQUIRE(now.size() == 3);
                copy = now[2];
                CHECK(is_rule_id(copy));
                CHECK(copy != ids[0]);
                CHECK(copy != ids[1]);
                CHECK(copy != ids[2]);
                CHECK(row_of(screen, "autopickup:rules", copy).label == "sledge hammer");
                CHECK(screen.focus_row == copy);
                return act("MOVE_RULE_DOWN");
            }
            case 6:
                // A moved rule keeps its id.
                CHECK(ids_of(screen, "autopickup:rules") == std::vector{ids[0], copy, ids[2]});
                CHECK(screen.focus_row == copy);
                return invoke("autopickup:tab", {.row = "character"});
            case 7: {
                const auto& tabs = body<ec::tabs_body>(part(screen, "autopickup:tabs"));
                CHECK(tabs.tabs[1].selected);
                CHECK_FALSE(tabs.tabs[0].selected);
                CHECK(rows(screen, "autopickup:rules").empty());
                // The rules of the other page are not rows of this one.
                CHECK(refused(state, "autopickup:select", {.row = ids[0]}));
                CHECK_FALSE(affordance(screen, "REMOVE_RULE").enabled);
                return invoke("autopickup:tab", {.row = "global"});
            }
            case 8:
                CHECK(ids_of(screen, "autopickup:rules") == std::vector{ids[0], copy, ids[2]});
                return invoke("autopickup:edit", {.row = ids[0]});
            case 10:
                // The text entry left changed nothing; leaving asks to save what was changed.
                CHECK(row_of(screen, "autopickup:rules", ids[0]).label == "*knife*");
                return act("QUIT");
            default:
                FAIL("the auto pickup manager outlived its answer");
                return act("QUIT");
        }
    });

    settings.show();
    CHECK(step == 12);
    // The answer was No, so the rules saved are the ones the manager was opened with.
    CHECK(json::parse(file.content()) == json::parse(pickup_rules));
}

TEST_CASE(
    "the auto pickup manager saves the rules it is left with and never their ids",
    "[rules_screen][autopickup_screen][engine_client_component]") {
    const auto guard = screen_guard{};
    const auto named = name_guard{""};
    const auto file = profile_file{PATH_INFO::autopickup(), pickup_rules};
    auto settings = auto_pickup::player_settings{};
    settings.load_global();
    auto first = std::string{};
    auto step = 0;

    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto state = captured();
        const auto screen = ec::top_screen(state);
        switch (step++) {
            case 0:
                REQUIRE(screen.context == "AUTO_PICKUP");
                first = ids_of(screen, "autopickup:rules").front();
                return invoke("autopickup:mode", {.row = first});
            case 1:
                CHECK(
                    column_of(row_of(screen, "autopickup:rules", first), "i_e").value == "Exclude");
                return act("QUIT");
            case 2:
                return answer("Yes");
            default:
                FAIL("the auto pickup manager outlived its answer");
                return act("QUIT");
        }
    });

    settings.show();
    CHECK(step == 3);
    // The rules as saved: the same three, the first now excluding, with the members the file always
    // had.
    const auto saved = json::parse(file.content());
    REQUIRE(saved.size() == 3);
    CHECK(saved[0] == json{{"rule", "*knife*"}, {"active", true}, {"exclude", true}});
    CHECK(saved[1] == json{{"rule", ""}, {"active", false}, {"exclude", false}});
    CHECK(saved[2] == json{{"rule", "sledge hammer"}, {"active", true}, {"exclude", true}});
}

TEST_CASE(
    "the auto pickup manager of a character has the tab of that character and lists the matches of a rule",
    "[rules_screen][autopickup_screen][engine_client_component]") {
    const auto guard = screen_guard{};
    const auto named = name_guard{"Tester"};
    auto settings = auto_pickup::npc_settings{};
    auto in = std::istringstream{pickup_rules};
    auto jsin = JsonIn{in};
    settings.deserialize(jsin);
    auto ids = std::vector<std::string>{};
    auto step = 0;

    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto state = captured();
        const auto screen = ec::top_screen(state);
        switch (step++) {
            case 0: {
                REQUIRE(screen.context == "AUTO_PICKUP");
                CHECK(screen.title == "Pickup rules for Bob");
                const auto& tabs = body<ec::tabs_body>(part(screen, "autopickup:tabs"));
                REQUIRE(tabs.tabs.size() == 1);
                CHECK(tabs.tabs[0].id == "npc");
                CHECK(tabs.tabs[0].label == "Bob");
                CHECK(tabs.tabs[0].selected);
                ids = ids_of(screen, "autopickup:rules");
                CHECK(ids.size() == 3);
                // Only the player's own manager switches the option and swaps pages.
                CHECK(std::ranges::
                          find(screen.affordances, "SWITCH_AUTO_PICKUP_OPTION", &ec::affordance::id)
                      == screen.affordances.end());
                return invoke("autopickup:select", {.row = ids[2]});
            }
            case 1:
                return act("TEST_RULE");
            case 2: {
                // The matches of the rule over the manager, each by the item it is.
                REQUIRE(screen.context == "AUTO_PICKUP_TEST");
                CHECK(screen.role == "dialog");
                CHECK(screen.title == "1 item matches: sledge hammer");
                const auto& all = rows(screen, "autopickup_test:matches");
                REQUIRE(all.size() == 1);
                CHECK(all[0].id == "item:hammer_sledge");
                CHECK(all[0].label == "sledge hammer");
                CHECK(all[0].selected);
                CHECK(body<ec::text_body>(part(screen, "autopickup_test:note")).markup
                      == "Won't display content or suffix matches");
                CHECK(affordance(screen, "autopickup_test:select").role == "select");
                CHECK(refused(state, "autopickup_test:select", {.row = "item:nothing"}));
                CHECK(refused(state, "autopickup_test:select", {.row = ids[0]}));
                CHECK(affordance(screen, "QUIT").role == "cancel");
                return invoke("autopickup_test:select", {.row = "item:hammer_sledge"});
            }
            case 3:
                CHECK(screen.focus_row == "item:hammer_sledge");
                return act("QUIT");
            case 4:
                REQUIRE(screen.context == "AUTO_PICKUP");
                return act("QUIT");
            default:
                FAIL("the auto pickup manager outlived its answer");
                return act("QUIT");
        }
    });

    settings.show("Bob");
    CHECK(step == 5);
}

TEST_CASE(
    "the auto pickup manager is the same at any viewport size and has the same ids in another language",
    "[rules_screen][autopickup_screen][engine_client_component]") {
    const auto guard = screen_guard{};
    const auto named = name_guard{"Tester"};
    const auto file = profile_file{PATH_INFO::autopickup(), pickup_rules};
    auto settings = auto_pickup::player_settings{};
    settings.load_global();
    auto reads = 0;
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        switch (reads++) {
            case 0:
                check_invariance("autopickup");
                return act("TEST_RULE");
            case 1:
                check_invariance("autopickup test");
                return act("QUIT");
            default:
                return act("QUIT");
        }
    });
    settings.show();
    CHECK(reads == 3);
}

TEST_CASE(
    "the safe mode manager is driven by its rows and edits rules by id",
    "[rules_screen][safemode_screen][engine_client_component]") {
    const auto guard = screen_guard{};
    const auto named = name_guard{"Tester"};
    auto rules = safemode{};
    auto in = std::istringstream{safemode_rules};
    auto jsin = JsonIn{in};
    rules.deserialize(jsin);
    auto ids = std::vector<std::string>{};
    auto step = 0;

    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto state = captured();
        const auto stack = ec::interaction_stack(state);
        const auto screen = ec::top_screen(state);
        // The question over the manager, which the screen below it leaves unanswered.
        if (step == 15) {
            REQUIRE(stack.size() >= 2);
            CHECK(stack[stack.size() - 2].context == "SAFEMODE");
            CHECK(stack[stack.size() - 2].affordances.empty());
            ++step;
            return answer("No");
        }
        // The text entry or the distance of a rule is the popup over the manager.
        if (step == 10 || step == 12) {
            CHECK(screen.context != "SAFEMODE");
            ++step;
            return answer("cancel");
        }
        REQUIRE(screen.context == "SAFEMODE");
        switch (step++) {
            case 0: {
                CHECK(screen.title == " SAFE MODE MANAGER ");
                CHECK(screen.role == "screen");
                const auto& tabs = body<ec::tabs_body>(part(screen, "safemode:tabs"));
                REQUIRE(tabs.tabs.size() == 2);
                CHECK(tabs.select == "safemode:tab");
                CHECK(tabs.tabs[0].id == "global");
                CHECK(tabs.tabs[0].label == "Global");
                CHECK(tabs.tabs[0].selected);
                CHECK(tabs.tabs[1].id == "character");
                CHECK(tabs.tabs[1].label == "Character");
                ids = ids_of(screen, "safemode:rules");
                REQUIRE(ids.size() == 3);
                CHECK(std::ranges::all_of(ids, is_rule_id));
                const auto& all = rows(screen, "safemode:rules");
                CHECK(all[0].label == "zombie");
                CHECK(all[0].selected);
                CHECK(column_of(all[0], "attitude").value == "Hostile");
                CHECK(column_of(all[1], "attitude").value == "Any");
                CHECK(column_of(all[2], "attitude").value == "---");
                CHECK(column_of(all[0], "dist").value == "10");
                CHECK(column_of(all[1], "dist").value == "---");
                CHECK(column_of(all[2], "dist").value == "5");
                CHECK(column_of(all[0], "whitelist").value == "Blacklist");
                CHECK(column_of(all[1], "whitelist").value == "Whitelist");
                CHECK(column_of(all[0], "category").value == "Hostile");
                CHECK(column_of(all[2], "category").value == "Sound");
                CHECK(column_of(all[0], "active").value == "True");
                CHECK(column_of(all[2], "active").value == "False");
                CHECK(all[2].color == "c_light_gray");
                CHECK(screen.focus_row == ids[0]);
                const auto& stat = body<ec::panel_body>(part(screen, "safemode:details")).stats;
                REQUIRE(stat.size() == 1);
                CHECK(stat[0].value == (get_option<bool>("SAFEMODE") ? "True" : "False"));
                // The help beside the list is the one of the kind of the rule under the cursor.
                CHECK(body<ec::text_body>(part(screen, "safemode:help"))
                          .markup.contains("human          matches every NPC"));
                CHECK(affordance(screen, "safemode:select").role == "select");
                CHECK(affordance(screen, "safemode:edit").label == "Rules");
                CHECK(affordance(screen, "safemode:attitude").label == "Attitude");
                CHECK(affordance(screen, "safemode:dist").label == "Dist");
                CHECK(affordance(screen, "safemode:list").label == "B/W");
                CHECK(affordance(screen, "safemode:category").label == "Cat");
                CHECK(affordance(screen, "TEST_RULE").enabled);
                CHECK(affordance(screen, "ADD_DEFAULT_RULESET").enabled);
                CHECK(refused(state, "safemode:select", {.row = "rule:0"}));
                CHECK(refused(state, "safemode:attitude", {.row = "entry:0"}));
                CHECK(refused(state, "safemode:list", {}));
                CHECK(refused(state, "safemode:tab", {.row = "middle"}));
                return invoke("safemode:attitude", {.row = ids[0]});
            }
            case 1: {
                // The attitude moved on from Hostile to Neutral, and the cursor is on the rule.
                const auto& first = row_of(screen, "safemode:rules", ids[0]);
                CHECK(column_of(first, "attitude").value == "Neutral");
                CHECK(first.selected);
                return invoke("safemode:list", {.row = ids[0]});
            }
            case 2: {
                // A whitelisted hostile rule has no distance.
                const auto& first = row_of(screen, "safemode:rules", ids[0]);
                CHECK(column_of(first, "whitelist").value == "Whitelist");
                CHECK(column_of(first, "dist").value == "---");
                return invoke("safemode:category", {.row = ids[0]});
            }
            case 3: {
                // A sound rule has no attitude, and has its distance.
                const auto& first = row_of(screen, "safemode:rules", ids[0]);
                CHECK(column_of(first, "category").value == "Sound");
                CHECK(column_of(first, "attitude").value == "---");
                CHECK(column_of(first, "dist").value == "10");
                CHECK(body<ec::text_body>(part(screen, "safemode:help"))
                          .markup.contains("footsteps      matches the sound name exactly"));
                return invoke("safemode:select", {.row = ids[2]});
            }
            case 4:
                CHECK(screen.focus_row == ids[2]);
                return act("ENABLE_RULE");
            case 5:
                CHECK(
                    column_of(row_of(screen, "safemode:rules", ids[2]), "active").value == "True");
                CHECK(row_of(screen, "safemode:rules", ids[2]).color == "c_white");
                return act("COPY_RULE");
            case 6: {
                // A copy is a new rule with an id of its own, the others keep theirs.
                const auto now = ids_of(screen, "safemode:rules");
                REQUIRE(now.size() == 4);
                CHECK(std::vector(now.begin(), now.begin() + 3) == ids);
                CHECK(is_rule_id(now[3]));
                CHECK(std::ranges::find(ids, now[3]) == ids.end());
                CHECK(row_of(screen, "safemode:rules", now[3]).label == "a loud*");
                return act("REMOVE_RULE");
            }
            case 7:
                CHECK(ids_of(screen, "safemode:rules") == ids);
                return invoke("safemode:select", {.row = ids[1]});
            case 8:
                // The default ruleset adds two rules, each with an id of its own.
                return act("ADD_DEFAULT_RULESET");
            case 9: {
                const auto now = ids_of(screen, "safemode:rules");
                REQUIRE(now.size() == 5);
                CHECK(std::vector(now.begin(), now.begin() + 3) == ids);
                CHECK(is_rule_id(now[3]));
                CHECK(is_rule_id(now[4]));
                CHECK(now[3] != now[4]);
                return invoke("safemode:edit", {.row = ids[0]});
            }
            case 11:
                // The text entry left; its answer is the distance of the sound rule.
                return invoke("safemode:dist", {.row = ids[0]});
            case 13: {
                // The distance asked for and not answered is the option's.
                CHECK(column_of(row_of(screen, "safemode:rules", ids[0]), "dist").value
                      == std::to_string(get_option<int>("SAFEMODEPROXIMITY")));
                return invoke("safemode:tab", {.row = "character"});
            }
            case 14: {
                const auto& tabs = body<ec::tabs_body>(part(screen, "safemode:tabs"));
                CHECK(tabs.tabs[1].selected);
                CHECK(rows(screen, "safemode:rules").empty());
                CHECK(refused(state, "safemode:select", {.row = ids[0]}));
                return act("QUIT");
            }
            default:
                FAIL("the safe mode manager outlived its answer");
                return act("QUIT");
        }
    });

    rules.show();
    CHECK(step == 16);
}

TEST_CASE(
    "the safe mode manager lists the monsters a rule matches",
    "[rules_screen][safemode_screen][engine_client_component]") {
    const auto guard = screen_guard{};
    const auto named = name_guard{"Tester"};
    auto rules = safemode{};
    auto in = std::istringstream{safemode_rules};
    auto jsin = JsonIn{in};
    rules.deserialize(jsin);
    auto ids = std::vector<std::string>{};
    auto step = 0;

    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto state = captured();
        const auto screen = ec::top_screen(state);
        switch (step++) {
            case 0:
                REQUIRE(screen.context == "SAFEMODE");
                ids = ids_of(screen, "safemode:rules");
                return invoke("safemode:select", {.row = ids[0]});
            case 1:
                return act("TEST_RULE");
            case 2: {
                // The matches of the rule over the manager, each by the monster it is.
                REQUIRE(screen.context == "SAFEMODE_TEST");
                CHECK(screen.role == "dialog");
                const auto expected =
                    MonsterGenerator::generator().get_all_mtypes()
                    | std::views::filter([](const mtype& type) { return type.nname() == "zombie"; })
                    | std::views::transform([](const mtype& type) {
                          return "monster:" + type.id.str();
                      })
                    | std::ranges::to<std::vector>();
                REQUIRE_FALSE(expected.empty());
                CHECK(screen.title == std::to_string(expected.size()) + " monsters match: zombie");
                CHECK(ids_of(screen, "safemode_test:matches") == expected);
                CHECK(row_of(screen, "safemode_test:matches", expected[0]).label == "zombie");
                CHECK(row_of(screen, "safemode_test:matches", expected[0]).selected);
                CHECK(body<ec::text_body>(part(screen, "safemode_test:note")).markup
                      == "Lists monsters regardless of their attitude.");
                CHECK(refused(state, "safemode_test:select", {.row = "monster:nothing"}));
                CHECK(refused(state, "safemode_test:select", {.row = ids[0]}));
                return invoke("safemode_test:select", {.row = expected.back()});
            }
            case 3:
                CHECK(screen.focus_row.starts_with("monster:"));
                return act("QUIT");
            case 4:
                REQUIRE(screen.context == "SAFEMODE");
                return act("QUIT");
            default:
                FAIL("the safe mode manager outlived its answer");
                return act("QUIT");
        }
    });

    rules.show();
    CHECK(step == 5);
}

TEST_CASE(
    "the safe mode manager saves the rules it is left with and never their ids",
    "[rules_screen][safemode_screen][engine_client_component]") {
    const auto guard = screen_guard{};
    const auto named = name_guard{""};
    auto rules = safemode{};
    auto in = std::istringstream{safemode_rules};
    auto jsin = JsonIn{in};
    rules.deserialize(jsin);
    auto first = std::string{};
    auto step = 0;

    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto screen = ec::top_screen(captured());
        switch (step++) {
            case 0:
                REQUIRE(screen.context == "SAFEMODE");
                first = ids_of(screen, "safemode:rules").front();
                return invoke("safemode:category", {.row = first});
            case 1:
                return act("QUIT");
            case 2:
                return answer("Yes");
            default:
                FAIL("the safe mode manager outlived its answer");
                return act("QUIT");
        }
    });

    const auto file = profile_file{PATH_INFO::safemode(), "[]"};
    rules.show();
    CHECK(step == 3);
    // The rules as saved: the same three, the first now a sound rule, with the members the file
    // always had.
    const auto saved = json::parse(file.content());
    REQUIRE(saved.size() == 3);
    CHECK(
        saved[0]
        == json{
            {"rule", "zombie"},
            {"active", true},
            {"whitelist", false},
            {"attitude", 0},
            {"proximity", 10},
            {"category", 1}});
    CHECK(saved[2].size() == 6);
}

TEST_CASE(
    "the safe mode manager says why it lists no rules and what cannot be done without a character",
    "[rules_screen][safemode_screen][engine_client_component]") {
    const auto guard = screen_guard{};
    const auto named = name_guard{""};
    auto rules = safemode{};
    auto step = 0;

    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto state = captured();
        const auto screen = ec::top_screen(state);
        REQUIRE(screen.context == "SAFEMODE");
        switch (step++) {
            case 0:
                CHECK(rows(screen, "safemode:rules").empty());
                CHECK(
                    body<ec::text_body>(part(screen, "safemode:notice")).markup
                    == "Safe Mode manager currently inactive.\n"
                       "Default rules are used.  Add a rule to activate.\n"
                       "Press ~ to add a default ruleset to get started.");
                CHECK_FALSE(affordance(screen, "TEST_RULE").enabled);
                CHECK_FALSE(affordance(screen, "REMOVE_RULE").enabled);
                CHECK(affordance(screen, "ADD_RULE").enabled);
                return invoke("safemode:tab", {.row = "character"});
            case 1:
                CHECK(body<ec::text_body>(part(screen, "safemode:notice")).markup
                      == "Please load a character first to use this page!");
                CHECK_FALSE(affordance(screen, "ADD_RULE").enabled);
                CHECK_FALSE(affordance(screen, "ADD_DEFAULT_RULESET").enabled);
                return act("QUIT");
            default:
                FAIL("the safe mode manager outlived its answer");
                return act("QUIT");
        }
    });

    rules.show();
    CHECK(step == 2);
}

TEST_CASE(
    "the safe mode manager cannot test or swap a rule without a character",
    "[rules_screen][safemode_screen][engine_client_component]") {
    const auto guard = screen_guard{};
    const auto named = name_guard{""};
    auto rules = safemode{};
    auto in = std::istringstream{safemode_rules};
    auto jsin = JsonIn{in};
    rules.deserialize(jsin);
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto screen = ec::top_screen(captured());
        REQUIRE(screen.context == "SAFEMODE");
        CHECK_FALSE(affordance(screen, "TEST_RULE").enabled);
        CHECK(affordance(screen, "TEST_RULE").denial
              == "No monsters loaded.  Please start a game first.");
        CHECK_FALSE(affordance(screen, "SWAP_RULE_GLOBAL_CHAR").enabled);
        CHECK(affordance(screen, "SWAP_RULE_GLOBAL_CHAR").denial
              == "Please load a character first to use this page!");
        CHECK(affordance(screen, "REMOVE_RULE").enabled);
        return act("QUIT");
    });
    rules.show();
}

TEST_CASE(
    "the safe mode manager is the same at any viewport size and has the same ids in another language",
    "[rules_screen][safemode_screen][engine_client_component]") {
    const auto guard = screen_guard{};
    const auto named = name_guard{"Tester"};
    auto rules = safemode{};
    auto in = std::istringstream{safemode_rules};
    auto jsin = JsonIn{in};
    rules.deserialize(jsin);
    auto reads = 0;
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        switch (reads++) {
            case 0:
                check_invariance("safemode");
                return act("TEST_RULE");
            case 1:
                check_invariance("safemode test");
                return act("QUIT");
            default:
                return act("QUIT");
        }
    });
    rules.show();
    CHECK(reads == 3);
}

#endif // CATA_MCP
