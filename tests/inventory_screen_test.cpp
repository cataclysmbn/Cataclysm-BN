#include "avatar.h"
#include "catch/catch.hpp"
#include "cursesdef.h"
#include "engine_client_component.h"
#include "inventory_ui.h"
#include "item.h"
#include "item_category.h"
#include "output.h"
#include "player_helpers.h"

#if defined(CATA_MCP)
#    include "locale_fixture.h"
#    include "locale_scope.h"
#endif

#include <algorithm>
#include <nlohmann/json.hpp>
#include <ranges>
#include <set>
#include <string>
#include <variant>
#include <vector>

namespace {
namespace ec = engine_client;
using json = nlohmann::json;

/// A selector with the parts the screen reads opened up.
struct plain_selector: inventory_selector {
    using inventory_selector::interaction_row;
    using inventory_selector::interaction_rows;
    using inventory_selector::interaction_view;
    using inventory_selector::inventory_selector;
    using inventory_selector::open_screen;
};
struct counting_selector: inventory_multiselector {
    using inventory_multiselector::apply_semantic_count;
    using inventory_multiselector::interaction_view;
    using inventory_multiselector::inventory_multiselector;
    using inventory_multiselector::open_screen;
};

auto boundary() -> ec::boundary_state {
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
template <typename Body> auto body(const ec::component& value) -> const Body& {
    REQUIRE(std::holds_alternative<Body>(value.body));
    return std::get<Body>(value.body);
}
/// The lists of the columns, outermost panel first.
auto lists_of(const ec::screen_view& view) -> std::vector<const ec::component*> {
    REQUIRE(view.root);
    const auto& split = view.root->children.front();
    auto result = std::vector<const ec::component*>{};
    for (const auto& panel : split.children) { result.push_back(&panel.children.front()); }
    return result;
}
/// The top screen as published, without its serial: each open scope has its own.
auto top_screen() -> json {
    auto top = json::parse(ec::serialize_interaction_boundary(boundary()))["stack"].back();
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

TEST_CASE(
    "the inventory selector publishes a panel per column with a heading for each category",
    "[inventory][engine_client_component]") {
    clear_avatar();
    auto& dummy = get_avatar();
    auto bandages = item::spawn("bandages");
    auto heroin = item::spawn("heroin");
    auto rock = item::spawn("rock");

    auto selector = plain_selector(dummy);
    selector.add_item(selector.own_inv_column, bandages.get());
    selector.add_item(selector.own_inv_column, heroin.get());
    selector.add_item(selector.map_column, rock.get());

    const auto view = selector.interaction_view();
    const auto lists = lists_of(view);
    REQUIRE(lists.size() == 2);
    CHECK(lists[0]->id == "list:inventory");
    CHECK(lists[1]->id == "list:nearby");
    const auto& carried = body<ec::list_body>(*lists[0]).rows;
    // A heading, then the items under it; the heading is no entry.
    REQUIRE(carried.size() >= 2);
    CHECK(carried.front().header);
    CHECK_FALSE(carried.front().selectable);
    CHECK(carried.front().label == bandages->get_category().name());
    const auto first_item = std::ranges::find_if(carried, [](const auto& row) {
        return !row.header;
    });
    REQUIRE(first_item != carried.end());
    CHECK(first_item->look);
    CHECK(first_item->selectable);
    CHECK_FALSE(first_item->mark);
    CHECK(body<ec::list_body>(*lists[0]).activate == "list:inventory:choose");
    CHECK(body<ec::list_body>(*lists[0]).selection == "single");
    // The cursor row is the first item of the active column and the only one described.
    CHECK(view.focus_component == "list:inventory");
    CHECK(view.focus_row == first_item->id);
    CHECK_FALSE(first_item->description.empty());
    CHECK(std::ranges::count_if(carried, [](const auto& row) { return !row.description.empty(); })
          == 1);
    const auto& panel = *view.root;
    // The screen's title is the window's; the panel under it does not repeat it.
    CHECK_FALSE(view.title.empty());
    CHECK(body<ec::panel_body>(panel).title.empty());

    // Every row is named by the column it is in and what it is, not by its text or its place on the
    // screen.
    const auto rows = selector.interaction_rows();
    auto ids = std::set<std::string>{};
    for (const auto& row : rows) { ids.insert(row.id); }
    CHECK(ids.size() == rows.size());
}

TEST_CASE(
    "an empty inventory selector says so and offers nothing of its own",
    "[inventory][engine_client_component]") {
    clear_avatar();
    auto selector = plain_selector(get_avatar());
    const auto view = selector.interaction_view();
    REQUIRE(view.root);
    CHECK(std::holds_alternative<ec::text_body>(view.root->children.front().body));
    CHECK(view.focus_row.empty());
    CHECK(view.affordances.empty());
}

TEST_CASE(
    "a multiselector's marks and counts are the entries' own",
    "[inventory][engine_client_component]") {
    clear_avatar();
    auto& dummy = get_avatar();
    auto first = item::spawn("bandages");
    auto second = item::spawn("bandages");
    auto selector = counting_selector(dummy, default_preset, "ITEMS");
    selector.add_item(selector.own_inv_column, first.get());
    selector.add_item(selector.own_inv_column, second.get());
    REQUIRE(selector.select(first.get()));

    const auto find_row = [&](const ec::screen_view& view) -> const ec::row& {
        const auto& rows =
            body<ec::list_body>(view.root->children.front().children.front().children.front()).rows;
        const auto found = std::ranges::find_if(rows, [](const auto& row) { return !row.header; });
        REQUIRE(found != rows.end());
        return *found;
    };
    const auto before = selector.interaction_view();
    CHECK(find_row(before).mark->glyph == "-");
    CHECK(find_row(before).selected_count == 0U);
    CHECK(find_row(before).max.has_value());
    const auto count =
        std::ranges::find(before.affordances, "list:inventory:count", &ec::affordance::id);
    REQUIRE(count != before.affordances.end());
    CHECK(count->args.count->min == 0);

    auto& entry = const_cast<inventory_entry&>(selector.get_selected());
    selector.apply_semantic_count(entry, 1);
    const auto after = selector.interaction_view();
    CHECK(find_row(after).selected);
    CHECK(find_row(after).selected_count == 1U);
    CHECK(find_row(after).mark->glyph != "-");
}

TEST_CASE(
    "the inventory selector is the same at any viewport size",
    "[inventory][engine_client_component]") {
    clear_avatar();
    auto bandages = item::spawn("bandages");
    auto rock = item::spawn("rock");
    auto selector = plain_selector(get_avatar());
    selector.add_item(selector.own_inv_column, bandages.get());
    selector.add_item(selector.map_column, rock.get());
    const auto published = [&] {
        const auto screen = selector.open_screen();
        return top_screen();
    };
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
    CHECK(narrow["root"]["kind"] == "panel");
}

#if defined(CATA_MCP)
TEST_CASE(
    "the inventory selector has the same ids and shape in another language",
    "[inventory][engine_client_component]") {
    const auto fixture = locale_fixture::korean_catalogue{};
    clear_avatar();
    auto cube = item::spawn("test_1kg_cube");
    auto selector = plain_selector(get_avatar());
    selector.add_item(selector.own_inv_column, cube.get());
    const auto published = [&] {
        const auto screen = selector.open_screen();
        return top_screen();
    };
    const auto english = published();
    const auto korean = [&] {
        const auto scope = localization::locale_scope(locale_fixture::korean);
        return published();
    }();
    CHECK(ids_of(english) == ids_of(korean));
    CHECK(without_words(english) == without_words(korean));
    CHECK(english.dump().find("-ko") == std::string::npos);
    CHECK(korean.dump().find("TEST 1kg cube-ko") != std::string::npos);
}
#endif
