#if defined(CATA_MCP)

#    include "avatar.h"
#    include "catch/catch.hpp"
#    include "client_command.h"
#    include "client_input.h"
#    include "client_interaction.h"
#    include "client_memory.h"
#    include "client_memory_scope.h"
#    include "cursesdef.h"
#    include "engine_client_component.h"
#    include "engine_client_contract.h"
#    include "game.h"
#    include "input.h"
#    include "locale_fixture.h"
#    include "locale_scope.h"
#    include "mutation.h"
#    include "mutation_ui.h"
#    include "output.h"
#    include "player_helpers.h"
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
        clear_character(get_avatar(), false);
        get_avatar().set_mutation(trait_id("test_preview_trait"));
        get_avatar().set_mutation(trait_id("test_preview_active_trait"));
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

const auto passive_id = std::string{"mutation:test_preview_trait"};
const auto active_id = std::string{"mutation:test_preview_active_trait"};

auto captured() -> ec::boundary_state {
    const auto state = ec::capture_boundary(
        {.epoch = "epoch:mutations",
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
auto rows(const ec::screen_view& screen, const std::string& column) -> const std::vector<ec::row>& {
    return body<ec::list_body>(part(screen, "mutations:" + column + ":rows")).rows;
}
auto row_of(const ec::screen_view& screen, const std::string& column, const std::string& id)
    -> const ec::row& {
    const auto& all = rows(screen, column);
    const auto found = std::ranges::find(all, id, &ec::row::id);
    INFO(id);
    REQUIRE(found != all.end());
    return *found;
}
auto text_of(const ec::screen_view& screen, const std::string& id) -> const std::string& {
    return body<ec::text_body>(part(screen, id)).markup;
}
auto column_of(const ec::row& row, const std::string& key) -> const std::string& {
    const auto found = std::ranges::find(row.columns, key, &game_client::interaction_column::key);
    REQUIRE(found != row.columns.end());
    return found->value;
}
auto denial_of(const ec::screen_view& screen, const std::string& action_id) -> std::string {
    const auto found = std::ranges::find(screen.denials, action_id, &ec::denial::id);
    return found == screen.denials.end() ? std::string{} : found->reason;
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
    "the mutation screen lists its columns by identity and a picked active one is toggled",
    "[mutation_screen][engine_client_component]") {
    const auto guard = screen_guard{};
    auto& you = get_avatar();
    auto step = 0;
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto state = captured();
        const auto screen = ec::top_screen(state);
        REQUIRE(screen.context == "MUTATIONS");
        switch (step++) {
            case 0: {
                CHECK(text_of(screen, "mutations:help").find("Activating") != std::string::npos);
                const auto& passive = row_of(screen, "passive", passive_id);
                const auto& active = row_of(screen, "active", active_id);
                CHECK(column_of(passive, "kind") == "Passive");
                CHECK(column_of(active, "kind") == "Active");
                CHECK_FALSE(passive.toggled.has_value());
                CHECK(active.toggled == std::optional{true});
                CHECK(passive.color == string_from_color(c_light_cyan));
                CHECK(active.color == string_from_color(c_light_green));
                CHECK(column_of(active, "innate") == "No");
                CHECK(column_of(active, "cost") == "0");
                CHECK_FALSE(passive.description.empty());
                CHECK(passive.hotkey.size() == 1);
                CHECK(screen.focus_row == passive_id);
                CHECK(screen.focus_component == "mutations:passive:rows");
                CHECK_FALSE(has_part(screen, "mutations:description"));
                CHECK(
                    ec::resolve_invoke(
                        state,
                        {.affordance_id = "mutations:pick", .args = {.row = "mutation:none"}})
                        .error()
                    == ec::error::invalid_arguments);
                return act("TOGGLE_EXAMINE");
            }
            case 1:
                CHECK(text_of(screen, "mutations:help").find("Examining") != std::string::npos);
                return invoke("mutations:pick", {.row = active_id});
            case 2:
                // Examining only moves the cursor to the mutation and describes it.
                CHECK(screen.focus_row == active_id);
                CHECK(screen.focus_component == "mutations:active:rows");
                CHECK_FALSE(text_of(screen, "mutations:description").empty());
                return act("TOGGLE_EXAMINE");
            case 3:
                CHECK(you.my_mutations[trait_id("test_preview_active_trait")].powered);
                return invoke("mutations:pick", {.row = active_id});
            default:
                FAIL("the mutation screen outlived its confirm");
                return act("QUIT");
        }
    });
    show_mutations_ui(you);
    CHECK(step == 4);
    CHECK_FALSE(you.my_mutations[trait_id("test_preview_active_trait")].powered);
}

TEST_CASE(
    "the mutation screen says why a passive mutation cannot be activated and drops it when examining",
    "[mutation_screen][engine_client_component]") {
    const auto guard = screen_guard{};
    auto step = 0;
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto screen = ec::top_screen(captured());
        if (step++ == 0) {
            const auto& passive = row_of(screen, "passive", passive_id);
            CHECK_FALSE(passive.enabled);
            CHECK(passive.denial.find("cannot activate") != std::string::npos);
            CHECK(denial_of(screen, "CONFIRM") == passive.denial);
            CHECK(row_of(screen, "active", active_id).denial.empty());
            return act("TOGGLE_EXAMINE");
        }
        CHECK(row_of(screen, "passive", passive_id).enabled);
        CHECK(denial_of(screen, "CONFIRM").empty());
        return act("QUIT");
    });
    show_mutations_ui(get_avatar());
    CHECK(step == 2);
}

TEST_CASE(
    "the mutation screen moves its focus and its description with the native cursor",
    "[mutation_screen][engine_client_component]") {
    const auto guard = screen_guard{};
    auto step = 0;
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto screen = ec::top_screen(captured());
        switch (step++) {
            case 0:
                return act("TOGGLE_EXAMINE");
            case 1:
                return act("NEXT_TAB");
            case 2:
                CHECK(screen.focus_row == active_id);
                CHECK(text_of(screen, "mutations:description")
                          .find(trait_id("test_preview_active_trait")->desc())
                      != std::string::npos);
                return act("QUIT");
            default:
                FAIL("unexpected native read");
                return act("QUIT");
        }
    });
    show_mutations_ui(get_avatar());
    CHECK(step == 3);
}

TEST_CASE(
    "the mutation screen hides a sprite and reassigns a letter through the native handlers",
    "[mutation_screen][engine_client_component]") {
    const auto guard = screen_guard{};
    auto& you = get_avatar();
    auto step = 0;
    auto reassign = false;
    SECTION("hide") {}
    SECTION("reassign") { reassign = true; }
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        if (game_client::current_interaction().context != "MUTATIONS") {
            // The native one-key popup of the reassignment.
            return input_event{'q', input_event_t::keyboard};
        }
        const auto screen = ec::top_screen(captured());
        switch (step++) {
            case 0:
                return act(reassign ? "REASSIGN" : "TOGGLE_SPRITE");
            case 1:
                return invoke("mutations:pick", {.row = passive_id});
            case 2:
                if (reassign) {
                    CHECK(row_of(screen, "passive", passive_id).hotkey == "q");
                } else {
                    CHECK(column_of(row_of(screen, "passive", passive_id), "sprite") == "Hidden");
                }
                return act("QUIT");
            default:
                FAIL("unexpected native read");
                return act("QUIT");
        }
    });
    show_mutations_ui(you);
    CHECK(step == 3);
}

TEST_CASE(
    "the mutation screen is the same at any viewport size and has the same ids in another language",
    "[mutation_screen][engine_client_component]") {
    const auto guard = screen_guard{};
    const auto fixture_ko = locale_fixture::korean_catalogue{};
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
    show_mutations_ui(get_avatar());
    CHECK(reads == 1);
}

TEST_CASE(
    "the mutation screen publishes no window text and says when a column is empty",
    "[mutation_screen][engine_client_component]") {
    const auto guard = screen_guard{};
    get_avatar().unset_mutation(trait_id("test_preview_active_trait"));
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto screen = ec::top_screen(captured());
        CHECK(rows(screen, "active").empty());
        CHECK_FALSE(text_of(screen, "mutations:active:none").empty());
        CHECK_FALSE(has_part(screen, "mutations:passive:none"));
        for (const auto& row : rows(screen, "passive")) {
            CHECK(row.label.find("<color") == std::string::npos);
            CHECK(row.id.starts_with("mutation:"));
        }
        return act("QUIT");
    });
    show_mutations_ui(get_avatar());
}

#endif
