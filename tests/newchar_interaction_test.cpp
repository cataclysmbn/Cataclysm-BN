#if defined(CATA_MCP)

#    include "avatar.h"
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
#    include "game.h"
#    include "game_constants.h"
#    include "input.h"
#    include "locale_fixture.h"
#    include "locale_scope.h"
#    include "magic/magic.h"
#    include "newcharacter.h"
#    include "output.h"
#    include "player_helpers.h"
#    include "profession.h"
#    include "state_helpers.h"

#    include <algorithm>
#    include <array>
#    include <functional>
#    include <nlohmann/json.hpp>
#    include <ranges>
#    include <string>
#    include <variant>
#    include <vector>

namespace {
namespace ec = engine_client;
using json = nlohmann::json;

struct newchar_guard {
    const game_client::memory::scoped_state memory;
    bool old_test_mode = test_mode;
    int old_termx = TERMX;
    int old_termy = TERMY;
    int old_full_screen_width = FULL_SCREEN_WIDTH;
    int old_full_screen_height = FULL_SCREEN_HEIGHT;

    newchar_guard() {
        test_mode = false;
        TERMX = 100;
        TERMY = 40;
        FULL_SCREEN_WIDTH = 80;
        FULL_SCREEN_HEIGHT = 24;
        game_client::memory::resize(TERMX, TERMY);
        catacurses::stdscr = catacurses::newwin(TERMY, TERMX, point_zero);
        catacurses::newscr = catacurses::newwin(TERMY, TERMX, point_zero);
    }

    ~newchar_guard() {
        game_client::memory::set_input_provider({});
        TERMX = old_termx;
        TERMY = old_termy;
        FULL_SCREEN_WIDTH = old_full_screen_width;
        FULL_SCREEN_HEIGHT = old_full_screen_height;
        test_mode = old_test_mode;
    }
};

/// The avatar's name is changed by the creation; the harness checks it is left as it was found.
struct restore_name {
    avatar& you;
    std::string name = you.name;
    ~restore_name() { you.name = name; }
};

auto captured() -> ec::boundary_state {
    const auto state = ec::capture_boundary(
        {.epoch = "epoch:newchar",
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
auto go_to(const std::string& tab) -> input_event { return invoke("newchar:tab", {.row = tab}); }
auto pick(const std::string& tab, const std::string& row, const std::string& page = {})
    -> input_event {
    return invoke("newchar:" + tab + (page.empty() ? "" : ":" + page) + ":pick", {.row = row});
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
auto rows_of(const ec::screen_view& screen, const std::string& list)
    -> const std::vector<ec::row>& {
    return body<ec::list_body>(part(screen, list)).rows;
}
auto find_row(const ec::screen_view& screen, const std::string& list, const std::string& id)
    -> const ec::row* {
    const auto& all = rows_of(screen, list);
    const auto found = std::ranges::find(all, id, &ec::row::id);
    return found == all.end() ? nullptr : &*found;
}
auto row_of(const ec::screen_view& screen, const std::string& list, const std::string& id)
    -> const ec::row& {
    const auto* const found = find_row(screen, list, id);
    INFO(list << " " << id);
    REQUIRE(found != nullptr);
    return *found;
}
auto column_of(const ec::row& row, const std::string& key) -> const std::string& {
    const auto found = std::ranges::find(row.columns, key, &game_client::interaction_column::key);
    REQUIRE(found != row.columns.end());
    return found->value;
}
auto tab_ids(const ec::screen_view& screen) -> std::vector<std::string> {
    REQUIRE(screen.root);
    return body<ec::tabs_body>(*screen.root).tabs | std::views::transform(&ec::tab_entry::id)
         | std::ranges::to<std::vector>();
}
auto has_tab(const ec::screen_view& screen, const std::string& id) -> bool {
    return std::ranges::contains(tab_ids(screen), id);
}
auto selected_tab(const ec::screen_view& screen) -> std::string {
    REQUIRE(screen.root);
    const auto& tabs = body<ec::tabs_body>(*screen.root).tabs;
    const auto found = std::ranges::find(tabs, true, &ec::tab_entry::selected);
    REQUIRE(found != tabs.end());
    return found->id;
}

auto points_text(const ec::screen_view& screen) -> std::string {
    const auto& stats =
        body<ec::panel_body>(part(screen, "newchar:" + selected_tab(screen) + ":notes")).stats;
    const auto found = std::ranges::find(stats, "points_left", &ec::panel_stat::key);
    REQUIRE(found != stats.end());
    return found->label;
}

/// Runs the creation with a scripted client. Popups are answered here (a name into the field, yes
/// to a question, a plain message dismissed); `script` sees every creation screen.
auto create_with(
    avatar& you, std::vector<std::string>& popups,
    const std::function<auto(const ec::boundary_state&, const ec::screen_view&)->input_event>&
        script) -> bool {
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto snapshot = game_client::current_interaction();
        if (!snapshot.context.starts_with("NEW_CHAR_")) {
            auto command = game_client::interaction_command{.input_id = snapshot.input_id};
            if (snapshot.field) {
                command.operation = game_client::interaction_operation::fill;
                command.target_id = snapshot.field->id;
                command.value = "Ada";
                command.submit = true;
            } else {
                popups.push_back(snapshot.message);
                const auto yes = std::ranges::
                    find(snapshot.choices, "YES", &game_client::interaction_choice::description);
                command.operation =
                    yes != snapshot.choices.end()
                        ? game_client::interaction_operation::choose
                        : game_client::interaction_operation::cancel;
                if (yes != snapshot.choices.end()) { command.target_id = yes->id; }
            }
            auto input = game_client::input_command{};
            input.interaction = std::move(command);
            const auto result =
                game_client::resolve_input_command(input, game_client::memory::screen_size());
            REQUIRE(result.has_value());
            return *result;
        }
        const auto state = captured();
        return script(state, ec::top_screen(state));
    });
    return you.create(character_type::CUSTOM);
}

/// The first row of the trait pages that costs points to take and can be taken.
auto affordable_trait(const ec::screen_view& screen) -> const ec::row* {
    for (const auto* const page : {"advantage", "disadvantage", "neutral"}) {
        const auto list = std::string{"newchar:traits:"} + page + ":rows";
        if (!has_part(screen, list)) { continue; }
        for (const auto& row : rows_of(screen, list)) {
            if (row.enabled && !row.toggled.value_or(false) && column_of(row, "points") != "0"
                && column_of(row, "points").front() != '-') {
                return &row;
            }
        }
    }
    return nullptr;
}
auto trait_row(const ec::screen_view& screen, const std::string& id) -> const ec::row& {
    for (const auto* const page : {"advantage", "disadvantage", "neutral"}) {
        const auto list = std::string{"newchar:traits:"} + page + ":rows";
        if (has_part(screen, list) && find_row(screen, list, id) != nullptr) {
            return row_of(screen, list, id);
        }
    }
    FAIL("no trait row " << id);
    return rows_of(screen, "newchar:traits:advantage:rows").front();
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
    "a custom character is created through the tab affordances",
    "[newchar][engine_client_component]") {
    clear_all_state();
    const auto guard = newchar_guard{};
    auto& you = get_avatar();
    const auto restore = restore_name{you};
    auto popups = std::vector<std::string>{};
    auto trait_points = std::string{};
    auto picked_trait = std::string{};
    auto skill_id = std::string{};
    auto skill_level_before = std::string{};
    auto traits_before = 0;
    auto traits_after = 0;
    auto tabs_seen = std::vector<std::string>{};
    auto stage = 0;

    const auto created = create_with(you, popups, [&](const auto& state, const auto& screen) {
        tabs_seen.push_back(selected_tab(screen));
        CHECK(screen.title == "Character creation");
        switch (stage++) {
            case 0: {
                CHECK(selected_tab(screen) == "points");
                CHECK(tab_ids(screen).front() == "points");
                CHECK(tab_ids(screen).back() == "overview");
                CHECK(tab_ids(screen).size() >= 8);
                CHECK_FALSE(points_text(screen).empty());
                const auto& pools = rows_of(screen, "newchar:points:rows");
                CHECK(std::ranges::count(
                          pools, true, [](const auto& row) { return row.toggled.value_or(false); })
                      == 1);
                CHECK(std::ranges::all_of(pools, [](const auto& row) {
                    return row.id.starts_with("points:") && !row.description.empty();
                }));
                // Unknown tabs and rows of another tab are refused, not run.
                for (const auto& [affordance, row] :
                     std::vector<std::pair<std::string, std::string>>{
                         {"newchar:tab", "nowhere"},
                         {"newchar:tab", "tab:traits"},
                         {"newchar:points:pick", "points:none"},
                         {"newchar:points:pick", "trait:none"}}) {
                    INFO(affordance << " " << row);
                    CHECK(
                        ec::resolve_invoke(
                            state, {.affordance_id = affordance, .args = {.row = row}})
                            .error()
                        == ec::error::invalid_arguments);
                }
                CHECK(ec::resolve_invoke(state, {.affordance_id = "newchar:tab"}).error()
                      == ec::error::invalid_arguments);
                // The traits tab's pick is not offered on the points tab.
                CHECK(
                    ec::resolve_invoke(state, {.affordance_id = "newchar:traits:advantage:pick",
                                               .args = {.row = "trait:none"}})
                        .error()
                    == ec::error::unknown_affordance);
                return go_to("traits");
            }
            case 1: {
                // Scenario, profession and stats were stepped over; traits shows its rows.
                CHECK(selected_tab(screen) == "traits");
                trait_points = points_text(screen);
                const auto* const trait = affordable_trait(screen);
                REQUIRE(trait != nullptr);
                CHECK(trait->id.starts_with("trait:"));
                CHECK_FALSE(trait->description.empty());
                picked_trait = trait->id;
                traits_before = static_cast<int>(you.get_mutations().size());
                return pick("traits", picked_trait, "advantage");
            }
            case 2: {
                CHECK(trait_row(screen, picked_trait).toggled == std::optional{true});
                CHECK(points_text(screen) != trait_points);
                CHECK(screen.focus_row == picked_trait);
                traits_after = static_cast<int>(you.get_mutations().size());
                return go_to("skills");
            }
            case 3: {
                CHECK(selected_tab(screen) == "skills");
                const auto& all = rows_of(screen, "newchar:skills:rows");
                const auto skill = std::ranges::find_if(all, [](const auto& row) {
                    return row.id.starts_with("skill:") && row.enabled;
                });
                REQUIRE(skill != all.end());
                skill_id = skill->id;
                skill_level_before = column_of(*skill, "level");
                return pick("skills", skill_id);
            }
            case 4:
                // Picking a skill moves the cursor to it; raising it is the native action.
                CHECK(screen.focus_row == skill_id);
                CHECK(column_of(row_of(screen, "newchar:skills:rows", skill_id), "level")
                      == skill_level_before);
                return act("RIGHT");
            case 5:
                CHECK(column_of(row_of(screen, "newchar:skills:rows", skill_id), "level")
                      != skill_level_before);
                return go_to("overview");
            case 6:
                CHECK(selected_tab(screen) == "overview");
                CHECK(row_of(screen, "newchar:overview:fields", "name").columns.front().value
                      != "Ada");
                return pick("overview", "name");
            case 7:
                // The name was entered in the field popup; finish the character.
                CHECK(row_of(screen, "newchar:overview:fields", "name").columns.front().value
                      == "Ada");
                return act("NEXT_TAB");
            default:
                FAIL("unexpected extra character creation screen " << selected_tab(screen));
        }
        return act("QUIT");
    });

    CHECK(created);
    CHECK(you.name == "Ada");
    CHECK(traits_after == traits_before + 1);
    CHECK(stage == 8);
    CHECK(std::ranges::find(tabs_seen, "stats") == tabs_seen.end());
}

TEST_CASE(
    "the bionics tab lists the profession's bionics and refuses to remove a locked one",
    "[newchar][engine_client_component]") {
    clear_all_state();
    const auto guard = newchar_guard{};
    auto& you = get_avatar();
    const auto restore = restore_name{you};
    auto popups = std::vector<std::string>{};
    auto stage = 0;
    auto prepper_has_metabolics = false;
    auto points_before = std::string{};
    const auto created = create_with(you, popups, [&](const auto&, const auto& screen) {
        switch (stage++) {
            case 0:
                return go_to("profession");
            case 1: {
                const auto& prepper =
                    row_of(screen, "newchar:profession:list:rows", "profession:bionic_prepper");
                CHECK_FALSE(prepper.toggled.value_or(false));
                return pick("profession", "profession:bionic_prepper");
            }
            case 2:
                CHECK(row_of(screen, "newchar:profession:list:rows", "profession:bionic_prepper")
                          .toggled
                      == std::optional{true});
                return go_to("bionics");
            case 3: {
                const auto list = std::string{"newchar:bionics:advantage:rows"};
                const auto& bionic = row_of(screen, list, "bionic:bio_metabolics");
                prepper_has_metabolics = bionic.toggled.value_or(false);
                CHECK_FALSE(bionic.description.empty());
                points_before = points_text(screen);
                return pick("bionics", "bionic:bio_metabolics", "advantage");
            }
            case 4: {
                // A locked bionic stays installed and the points are untouched.
                const auto list = std::string{"newchar:bionics:advantage:rows"};
                CHECK(row_of(screen, list, "bionic:bio_metabolics").toggled == std::optional{true});
                CHECK(screen.focus_row == "bionic:bio_metabolics");
                CHECK(points_text(screen) == points_before);
                return go_to("overview");
            }
            case 5:
                return act("NEXT_TAB");
            default:
                FAIL("unexpected extra character creation screen");
        }
        return act("QUIT");
    });
    CHECK(created);
    CHECK(prepper_has_metabolics);
    CHECK(you.prof->ident() == profession_id("bionic_prepper"));
    CHECK(you.has_bionic(bionic_id("bio_metabolics")));
    REQUIRE(popups.size() >= 1);
    CHECK(popups.front().find("prevents you from removing") != std::string::npos);
}

TEST_CASE(
    "the magic tab appears with a spellcasting profession and learning a spell costs points",
    "[newchar][engine_client_component]") {
    clear_all_state();
    const auto guard = newchar_guard{};
    auto& you = get_avatar();
    const auto restore = restore_name{you};
    // The learned spell would otherwise outlive the test in the shared avatar.
    const auto forget = on_out_of_scope([&] {
        you.magic->forget_spell(spell_id("test_spell_apprentice"));
    });
    auto popups = std::vector<std::string>{};
    auto stage = 0;
    auto points_before = std::string{};
    const auto created = create_with(you, popups, [&](const auto&, const auto& screen) {
        switch (stage++) {
            case 0:
                // No profession chosen yet: the default one allows no spell, so there is no tab.
                CHECK_FALSE(has_tab(screen, "magic"));
                return go_to("profession");
            case 1:
                return pick("profession", "profession:test_spellcaster");
            case 2:
                CHECK(has_tab(screen, "magic"));
                return go_to("magic");
            case 3: {
                const auto& spell =
                    row_of(screen, "newchar:magic:rows", "spell:test_spell_apprentice");
                CHECK_FALSE(spell.toggled.value_or(false));
                CHECK_FALSE(spell.description.empty());
                points_before = points_text(screen);
                return pick("magic", spell.id);
            }
            case 4:
                CHECK(screen.focus_row == "spell:test_spell_apprentice");
                CHECK_FALSE(
                    row_of(screen, "newchar:magic:rows", "spell:test_spell_apprentice")
                        .toggled.value_or(false));
                return act("RIGHT");
            case 5:
                CHECK(row_of(screen, "newchar:magic:rows", "spell:test_spell_apprentice").toggled
                      == std::optional{true});
                CHECK(points_text(screen) != points_before);
                return go_to("overview");
            case 6:
                return act("NEXT_TAB");
            default:
                FAIL("unexpected extra character creation screen");
        }
        return act("QUIT");
    });
    CHECK(created);
    CHECK(you.magic->knows_spell(spell_id("test_spell_apprentice")));
}

TEST_CASE(
    "the character creation tabs are the same at any viewport size and have the same ids in another language",
    "[newchar][engine_client_component]") {
    clear_all_state();
    const auto guard = newchar_guard{};
    const auto fixture_ko = locale_fixture::korean_catalogue{};
    auto& you = get_avatar();
    const auto restore = restore_name{you};
    auto popups = std::vector<std::string>{};
    auto stage = 0;
    const auto tabs = std::array<std::string, 4>{"scenario", "profession", "skills", "overview"};
    create_with(you, popups, [&](const auto&, const auto&) {
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
        const auto next = static_cast<std::size_t>(stage++);
        return next < tabs.size() ? go_to(tabs[next]) : act("QUIT");
    });
    CHECK(stage == 5);
}

#endif // CATA_MCP
