#include "avatar.h"
#include "catch/catch.hpp"
#include "cursesdef.h"
#include "engine_client_component.h"
#include "game.h"
#include "input.h"
#include "item.h"
#include "map/map.h"
#include "options.h"
#include "output.h"
#include "pickup.h"
#include "pickup_screen.h"
#include "pickup_token.h"
#include "player_helpers.h"
#include "state_helpers.h"

#if defined(CATA_MCP)
#    include "locale_fixture.h"
#    include "locale_scope.h"
#endif

#include <algorithm>
#include <list>
#include <nlohmann/json.hpp>
#include <numeric>
#include <ranges>
#include <set>
#include <string>
#include <variant>
#include <vector>

namespace {
namespace ec = engine_client;
using json = nlohmann::json;

/// The things on the avatar's square, stacked as the pickup list stacks them, and a list over them.
struct ground {
    std::vector<std::list<item_stack::iterator>> stacks;
    std::vector<pickup_count> picks;
    std::vector<int> matches;
    std::string filter;
    std::string identity = "ground:60:60:0:-1";
    int cursor = 0;
    units::mass weight = 0_gram;
    units::volume volume = 0_ml;
    input_context ctxt{"PICKUP"};

    explicit ground(const std::vector<std::string>& types) {
        clear_all_state();
        auto& here = get_map();
        const auto center = tripoint_bub_ms{60, 60, 0};
        g->place_player(center);
        for (const auto& pos : here.points_in_radius(center, 2)) { here.i_clear(pos); }
        for (const auto& type : types) { here.add_item_or_charges(center, item::spawn(type)); }
        const auto nearby = pickup::nearby_items_for_pickup(center);
        stacks = pickup::flatten(pickup::stack_for_pickup_ui(nearby.items));
        picks.resize(stacks.size());
        for (const auto index : std::views::iota(std::size_t{0}, stacks.size())) {
            matches.push_back(static_cast<int>(index));
        }
        ctxt.register_action("CONFIRM");
        ctxt.register_action("QUIT", to_translation("Cancel"));
    }
    ground(const ground&) = delete;
    auto operator=(const ground&) -> ground& = delete;

    auto options() const -> pickup::screen_options {
        return {
            .stacked_here = stacks,
            .getitem = picks,
            .matches = matches,
            .filter = filter,
            .source = pickup::source_kind::ground,
            .source_identity = identity,
            .selected = cursor,
            .weight_predict = weight,
            .volume_predict = volume,
            .hotkeys = "abcdefghijklmnopqrstuvwxyz",
            .ctxt = ctxt,
        };
    }
    /// The index of the stack of `type`.
    auto stack_of(const std::string& type) const -> std::size_t {
        const auto found = std::ranges::find_if(stacks, [&](const auto& stack) {
            return (*stack.front())->typeId().str() == type;
        });
        REQUIRE(found != stacks.end());
        return static_cast<std::size_t>(std::distance(stacks.begin(), found));
    }
};

auto ready() -> ec::readiness {
    return {.phase = "waiting_for_input",
            .game_ready = true,
            .accepts_interaction_commands = true,
            .accepts_registered_actions = true};
}
auto boundary() -> ec::boundary_state {
    return {
        .id = "boundary:test",
        .epoch = "epoch:test",
        .ready = ready(),
        .actions = {{.id = "UP", .name = "Move up", .keys = {"k"}}},
        .interaction = std::nullopt,
        .frame = {.dim = "", .x = 100, .y = 200}};
}

/// The screen on top of the published stack while the pickup screen is open.
auto published(const ground& list) -> json {
    const auto screen = ec::screen_scope(
        list.ctxt, "test",
        {.view = [&](const ec::boundary_state&) { return pickup::screen_of(list.options()); }});
    // Each open scope has its own serial; the screen is compared by what it shows.
    auto top = json::parse(ec::serialize_interaction_boundary(boundary()))["stack"].back();
    top.erase("screen_id");
    return top;
}

template <typename Body> auto body(const ec::component& value) -> const Body& {
    REQUIRE(std::holds_alternative<Body>(value.body));
    return std::get<Body>(value.body);
}
auto rows_of(const ec::screen_view& view) -> const std::vector<ec::row>& {
    REQUIRE(view.root);
    return body<ec::list_body>(view.root->children.front()).rows;
}

/// Every string member removed, as a client sees the shape of a screen without its words.
auto without_words(json value) -> json {
    if (value.is_string()) { return ""; }
    if (!value.is_structured()) { return value; }
    for (auto& member : value) { member = without_words(std::move(member)); }
    return value;
}
/// Every id the screen names: its own, its components', rows' and affordances'.
void collect_ids(const json& value, std::vector<std::string>& into) {
    if (value.is_object()) {
        for (const auto& [name, member] : value.items()) {
            if ((name == "id" || name == "screen_id" || name == "affordance")
                && member.is_string()) {
                into.push_back(member.get<std::string>());
            }
            collect_ids(member, into);
        }
    } else if (value.is_array()) {
        for (const auto& member : value) { collect_ids(member, into); }
    }
}
auto ids_of(const json& value) -> std::vector<std::string> {
    auto result = std::vector<std::string>{};
    collect_ids(value, result);
    return result;
}
} // namespace

TEST_CASE("the pickup screen publishes a row per stack with its marks and counts", "[pickup]") {
    auto list = ground({"test_1kg_cube", "test_1kg_cube", "test_1kg_cube", "rock"});
    const auto cubes = list.stack_of("test_1kg_cube");
    const auto rock = list.stack_of("rock");
    REQUIRE((*list.stacks[cubes].front())->count_by_charges() == false);
    REQUIRE(list.stacks[cubes].size() == 3);
    list.picks[cubes] = {.pick = true, .count = 2};
    list.picks[rock] = {.pick = true};
    list.cursor = static_cast<int>(rock);

    const auto view = pickup::screen_of(list.options());
    const auto& rows = rows_of(view);
    REQUIRE(rows.size() == 2);
    const auto& counted = rows[cubes];
    CHECK(counted.mark->glyph == "#");
    CHECK(counted.selected);
    CHECK(counted.selected_count == 2U);
    CHECK(counted.max == 3U);
    CHECK(counted.minimum_count == 0U);
    CHECK(counted.look->id == "test_1kg_cube");
    const auto& whole = rows[rock];
    CHECK(whole.mark->glyph == "+");
    CHECK(whole.selected_count == 1U);
    CHECK(whole.look->id == "rock");
    CHECK(view.dock == get_option<std::string>("PICKUP_POSITION"));
    // The screen's title is the window's; the panel under it does not repeat it.
    CHECK_FALSE(view.title.empty());
    CHECK(body<ec::panel_body>(*view.root).title.empty());
    CHECK(view.focus_component == pickup::items_id);
    CHECK(view.focus_row == whole.id);
    // The native window describes the item under the cursor only.
    CHECK_FALSE(whole.description.empty());
    CHECK(counted.description.empty());

    list.picks[rock] = {};
    const auto unmarked = rows_of(pickup::screen_of(list.options()));
    CHECK(unmarked[rock].mark->glyph == "-");
    CHECK_FALSE(unmarked[rock].selected);
    CHECK(unmarked[rock].selected_count == 0U);
}

TEST_CASE(
    "the pickup screen's rows follow the filter and the capacity header says over", "[pickup]") {
    // The test avatar carries without limit while it has the debug storage.
    auto list = ground({"rock", "stick"});
    get_avatar().unset_mutation(trait_id("DEBUG_STORAGE"));
    const auto rock = list.stack_of("rock");
    list.matches = {static_cast<int>(rock)};
    list.filter = "rock";
    list.weight = get_avatar().weight_capacity() + 1_gram;

    const auto view = pickup::screen_of(list.options());
    REQUIRE(view.root);
    CHECK(rows_of(view).size() == 1);
    const auto& panel = body<ec::panel_body>(*view.root);
    CHECK(panel.filter == "rock");
    REQUIRE(panel.stats.size() == 2);
    CHECK(panel.stats[0].key == "weight");
    CHECK(panel.stats[0].tone == "bad");
    CHECK(panel.stats[1].key == "volume");
    CHECK(panel.stats[1].tone.empty());
    CHECK_FALSE(panel.stats[0].max.empty());
}

TEST_CASE("an empty cursor list publishes no focus row", "[pickup]") {
    auto list = ground({"rock"});
    list.matches.clear();
    list.cursor = 0;
    const auto view = pickup::screen_of(list.options());
    CHECK(rows_of(view).empty());
    CHECK(view.focus_row.empty());
}

TEST_CASE("a pickup row id comes from the items' own serials", "[pickup]") {
    auto list = ground({"rock", "stick", "stick"});
    const auto first = pickup::choice_ids(list.options());
    REQUIRE(first.size() == list.stacks.size());
    CHECK(std::set<std::string>(first.begin(), first.end()).size() == first.size());
    // The same list is named the same, whatever the marks and the cursor.
    list.picks[0] = {.pick = true};
    list.cursor = 1;
    CHECK(pickup::choice_ids(list.options()) == first);
    // Another source name changes nothing: the rows name the items, not where they lie.
    list.identity = "ground:61:60:0:-1";
    CHECK(pickup::choice_ids(list.options()) == first);
    // Reordering the rows moves each id with its stack.
    std::ranges::reverse(list.stacks);
    std::ranges::reverse(list.picks);
    list.matches.resize(list.stacks.size());
    std::ranges::iota(list.matches, 0);
    auto reordered = pickup::choice_ids(list.options());
    std::ranges::reverse(reordered);
    CHECK(reordered == first);
}

TEST_CASE(
    "the pickup screen is the same at any viewport size", "[pickup][engine_client_component]") {
    auto list = ground({"rock", "stick"});
    list.picks[0] = {.pick = true};
    const auto saved_x = TERMX;
    const auto saved_y = TERMY;
    TERMX = 80;
    TERMY = 24;
    const auto narrow = published(list);
    TERMX = 200;
    TERMY = 60;
    const auto wide = published(list);
    TERMX = saved_x;
    TERMY = saved_y;
    CHECK(narrow.dump() == wide.dump());
}

TEST_CASE(
    "the pickup affordances are the ones its list and its buttons run",
    "[pickup][engine_client_component]") {
    auto list = ground({"rock"});
    const auto view = pickup::screen_of(list.options());
    const auto find = [&](const std::string& id) -> const ec::affordance& {
        const auto found = std::ranges::find(view.affordances, id, &ec::affordance::id);
        REQUIRE(found != view.affordances.end());
        return *found;
    };
    CHECK(body<ec::list_body>(view.root->children.front()).activate == pickup::toggle_id);
    CHECK(ec::scope_of(find(pickup::toggle_id)) == ec::expect_scope::boundary);
    CHECK(find(pickup::count_id).args.count->min == 0);

    // A row the list holds can be marked; one it does not, and a count below the minimum, are
    // refused.
    const auto ids = pickup::choice_ids(list.options());
    const auto screen = ec::screen_scope(
        list.ctxt, "test",
        {.view = [&](const ec::boundary_state&) { return pickup::screen_of(list.options()); }});
    const auto state = boundary();
    const auto marked = ec::
        resolve_invoke(state, {.affordance_id = pickup::toggle_id, .args = {.row = ids.front()}});
    REQUIRE(marked);
    CHECK(std::holds_alternative<ec::invoke_operation>(*marked));
    CHECK(
        ec::resolve_invoke(
            state, {.affordance_id = pickup::toggle_id, .args = {.row = "pickup:nothing"}})
            .error()
        == ec::error::invalid_arguments);
    CHECK(
        ec::resolve_invoke(
            state, {.affordance_id = pickup::count_id, .args = {.row = ids.front(), .count = -1}})
            .error()
        == ec::error::invalid_arguments);
    CHECK(ec::resolve_invoke(
        state, {.affordance_id = pickup::count_id, .args = {.row = ids.front(), .count = 0}}));
}

#if defined(CATA_MCP)
TEST_CASE(
    "the pickup screen has the same ids and shape in another language",
    "[pickup][engine_client_component]") {
    const auto fixture = locale_fixture::korean_catalogue{};
    auto list = ground({"test_1kg_cube", "test_1kg_cube", "rock"});
    list.picks[0] = {.pick = true, .count = 1};
    const auto english = published(list);
    const auto korean = [&] {
        const auto scope = localization::locale_scope(locale_fixture::korean);
        return published(list);
    }();
    CHECK(ids_of(english) == ids_of(korean));
    CHECK(without_words(english) == without_words(korean));
    // The words are composed again in the language: the stack's name is the catalogue's.
    CHECK(english.dump().find("-ko") == std::string::npos);
    CHECK(korean.dump().find("TEST 1kg cubes-ko") != std::string::npos);
}
#endif
