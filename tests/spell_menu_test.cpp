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
#    include "engine_client_component.h"
#    include "engine_client_contract.h"
#    include "input.h"
#    include "locale_fixture.h"
#    include "locale_scope.h"
#    include "magic/magic.h"
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
        {.epoch = "epoch:spells",
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

/// The input a client's invoke of `affordance` becomes.
auto invoke(const ec::boundary_state& state, ec::invoke_operation request) -> input_event {
    const auto operation = ec::resolve_invoke(state, std::move(request));
    REQUIRE(operation.has_value());
    REQUIRE(std::holds_alternative<ec::semantic_operation>(*operation));
    auto input = game_client::input_command{};
    input.interaction = std::get<ec::semantic_operation>(*operation).command;
    input.interaction->input_id = game_client::current_interaction().input_id;
    const auto result =
        game_client::resolve_input_command(input, game_client::memory::screen_size());
    REQUIRE(result.has_value());
    return *result;
}

auto rows(const ec::screen_view& screen) -> const std::vector<ec::row>& {
    REQUIRE(screen.root);
    const auto* const found = ec::find_component(*screen.root, "list:choices");
    REQUIRE(found != nullptr);
    REQUIRE(std::holds_alternative<ec::list_body>(found->body));
    return std::get<ec::list_body>(found->body).rows;
}
auto spell_rows(const ec::screen_view& screen) -> std::vector<ec::row> {
    return rows(screen)
         | std::views::filter([](const auto& row) { return row.id.starts_with("spell:"); })
         | std::ranges::to<std::vector>();
}
auto column_of(const ec::row& row, const std::string& key) -> std::string {
    const auto found = std::ranges::find(row.columns, key, &game_client::interaction_column::key);
    return found == row.columns.end() ? std::string{} : found->value;
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

struct knowing_guard {
    avatar& you = get_avatar();
    ~knowing_guard() { you.magic = pimpl<known_magic>(); }
};

} // namespace

TEST_CASE(
    "the spell menu lists a spell by its identity with its figures and keys",
    "[spell_menu][engine_client_component]") {
    clear_all_state();
    const auto guard = screen_guard{};
    const auto knowing = knowing_guard{};
    auto& you = get_avatar();
    you.magic->learn_spell(spell_id("test_spell_pew"), you, true);
    auto step = 0;
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto state = captured();
        const auto screen = ec::top_screen(state);
        const auto spells = spell_rows(screen);
        REQUIRE(spells.size() == 1);
        const auto& pew = spells.front();
        switch (step++) {
            case 0:
                CHECK(pew.id == "spell:test_spell_pew");
                CHECK(column_of(pew, "spell_level")
                      == std::to_string(
                          you.magic->get_spell(spell_id("test_spell_pew")).get_level()));
                CHECK_FALSE(column_of(pew, "failure_chance").empty());
                CHECK_FALSE(column_of(pew, "casting_cost").empty());
                CHECK_FALSE(column_of(pew, "casting_time").empty());
                CHECK_FALSE(column_of(pew, "range").empty());
                CHECK(pew.description.find("Casting Time") != std::string::npos);
                CHECK_FALSE(pew.favorite);
                CHECK(std::ranges::any_of(rows(screen), [](const auto& row) {
                    return row.label == "Assign spell hotkey" && row.id.starts_with("action:key:");
                }));
                // A row is named by what it is: its place in the list names nothing.
                CHECK(
                    ec::resolve_invoke(
                        state,
                        {.affordance_id = "bn:choose:list:choices", .args = {.row = "entry:0"}})
                        .error()
                    == ec::error::invalid_arguments);
                return act("TOGGLE_FAVORITE");
            case 1:
                CHECK(pew.id == "spell:test_spell_pew");
                CHECK(pew.favorite);
                return invoke(
                    state, {.affordance_id = "bn:choose:list:choices", .args = {.row = pew.id}});
            default:
                FAIL("the spell menu outlived its choice");
                return act("QUIT");
        }
    });
    const auto selected = you.magic->select_spell(you);
    CHECK(step == 2);
    CHECK(selected == 0);
}

TEST_CASE(
    "the spell menu details do not change between reads for any known spell",
    "[spell_menu][engine_client_component]") {
    clear_all_state();
    const auto guard = screen_guard{};
    const auto knowing = knowing_guard{};
    auto& you = get_avatar();
    std::ranges::for_each(spell_type::get_all(), [&](const auto& type) {
        you.magic->learn_spell(type.id, you, true);
    });
    auto reads = 0;
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        ++reads;
        const auto first = spell_rows(ec::top_screen(captured()));
        const auto second = spell_rows(ec::top_screen(captured()));
        REQUIRE(first.size() == second.size());
        CHECK(first.size() > 1);
        for (const auto i : std::views::iota(std::size_t{}, first.size())) {
            INFO(first[i].id);
            CHECK(first[i].id.starts_with("spell:"));
            CHECK(first[i].description == second[i].description);
        }
        return act("QUIT");
    });
    you.magic->select_spell(you);
    CHECK(reads == 1);
}

TEST_CASE(
    "the spell menu is the same at any viewport size and has the same ids in another language",
    "[spell_menu][engine_client_component]") {
    clear_all_state();
    const auto guard = screen_guard{};
    const auto knowing = knowing_guard{};
    const auto fixture_ko = locale_fixture::korean_catalogue{};
    auto& you = get_avatar();
    you.magic->learn_spell(spell_id("test_spell_pew"), you, true);
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
    you.magic->select_spell(you);
    CHECK(reads == 1);
}

#endif
