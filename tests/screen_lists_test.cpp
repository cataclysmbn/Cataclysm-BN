#if defined(CATA_MCP)

#    include "armor_layers.h"
#    include "avatar.h"
#    include "bionics.h"
#    include "bionics_ui.h"
#    include "catch/catch.hpp"
#    include "client_command.h"
#    include "client_input.h"
#    include "client_interaction.h"
#    include "client_memory.h"
#    include "client_memory_scope.h"
#    include "client_screen_support.h"
#    include "cursesdef.h"
#    include "cursesport.h"
#    include "game.h"
#    include "game_constants.h"
#    include "input.h"
#    include "item.h"
#    include "map/map.h"
#    include "map_helpers.h"
#    include "map_item_stack.h"
#    include "mission.h"
#    include "mutation_ui.h"
#    include "output.h"
#    include "player_helpers.h"
#    include "state_helpers.h"

#    include <algorithm>
#    include <functional>
#    include <memory>
#    include <string>
#    include <vector>

namespace {

/// Test-only member access reaches the item list, which only the `V` action of the main loop opens.
/// Explicit instantiation permits the member pointer without changing the game header.
struct list_items_tag {
    friend auto list_items_call(list_items_tag, const std::vector<map_item_stack>& stacks) -> void;
};
template <auto Member> struct list_items_access {
    friend auto list_items_call(list_items_tag, const std::vector<map_item_stack>& stacks) -> void {
        static_cast<void>((g.get()->*Member)(stacks));
    }
};
template struct list_items_access<&game::list_items>;

} // namespace

using namespace client_screen_test;

namespace {
auto column(const game_client::interaction_choice& choice, const std::string& label)
    -> std::string {
    const auto found =
        std::ranges::find(choice.columns, label, &game_client::interaction_column::label);
    REQUIRE(found != choice.columns.end());
    return found->value;
}
} // namespace

TEST_CASE(
    "the mission list switches tabs and makes the chosen mission active",
    "[client][interaction][mcp][screens]") {
    clear_all_state();
    clear_map();
    auto& you = get_avatar();
    clear_character(you, false);
    mission* assigned = nullptr;
    for (const mission_type& type : mission_type::get_all()) {
        if (type.goal == MGOAL_ASSASSINATE) {
            assigned = mission::reserve_new(type.id, you.getID());
            break;
        }
    }
    REQUIRE(assigned != nullptr);
    if (assigned->get_assigned_player_id() == you.getID()) {
        you.on_mission_assignment(*assigned);
    } else {
        assigned->assign(you);
    }
    REQUIRE(you.get_active_missions().size() == 1);
    const auto guard = screen_guard{};
    auto stage = 0;
    open_screen(
        [] { g->list_missions(); },
        [&](const auto& snapshot) {
            switch (stage++) {
                case 0: {
                    CHECK(snapshot.context == "MISSIONS");
                    CHECK(rows_with(snapshot, "tab:").size() == 3);
                    REQUIRE(rows_with(snapshot, "mission:").size() == 1);
                    const auto mission_row = row(snapshot, "mission:0");
                    CHECK(mission_row->label == assigned->name());
                    CHECK(mission_row->highlighted);
                    CHECK_FALSE(mission_row->description.empty());
                    return choose(snapshot, "tab:1");
                }
                case 1: {
                    // Nothing is completed yet: the tab says so and lists no mission.
                    CHECK(rows_with(snapshot, "mission:").empty());
                    CHECK(snapshot.message == "You haven't completed any missions!");
                    CHECK(row(snapshot, "tab:1")->selected);
                    return choose(snapshot, "tab:0");
                }
                case 2:
                    return choose(snapshot, "mission:0");
                default:
                    FAIL("unexpected extra screen " << snapshot.context);
            }
            return input_event{};
        });
    CHECK(stage == 3);
    CHECK(you.get_active_mission() == assigned);
}

TEST_CASE(
    "the mutation list shows passive and active traits and a chosen active one is toggled",
    "[client][interaction][mcp][screens]") {
    clear_all_state();
    clear_map();
    auto& you = get_avatar();
    clear_character(you, false);
    you.set_mutation(trait_id("test_preview_trait"));
    you.set_mutation(trait_id("test_preview_active_trait"));
    const auto guard = screen_guard{};
    auto stage = 0;
    open_screen(
        [&] { show_mutations_ui(you); },
        [&](const auto& snapshot) {
            switch (stage++) {
                case 0: {
                    CHECK(snapshot.context == "MUTATIONS");
                    CHECK(snapshot.message == "Activating");
                    const auto passive = row(snapshot, "mutation:test_preview_trait");
                    const auto active = row(snapshot, "mutation:test_preview_active_trait");
                    REQUIRE(passive != snapshot.choices.end());
                    REQUIRE(active != snapshot.choices.end());
                    CHECK(passive->columns.front().value == "Passive");
                    CHECK(active->columns.front().value == "Active");
                    CHECK(passive->highlighted);
                    CHECK(active->selected);
                    CHECK(passive->pane_id == "passive");
                    CHECK(active->pane_id == "active");
                    CHECK(column(*passive, "Color") == string_from_color(c_light_cyan));
                    CHECK(column(*active, "Color") == string_from_color(c_light_green));
                    CHECK(column(*active, "Innate") == "No");
                    CHECK(column(*active, "Cost") == "0");
                    CHECK(row(snapshot, "action:REASSIGN")->pane_id == "actions");
                    CHECK_FALSE(passive->description.empty());
                    return choose(snapshot, "action:TOGGLE_EXAMINE");
                }
                case 1: {
                    CHECK(snapshot.message == "Examining");
                    return choose(snapshot, "mutation:test_preview_active_trait");
                }
                case 2: {
                    // Examining only moves the cursor to the mutation and describes it.
                    CHECK(snapshot.message == "Examining");
                    CHECK(row(snapshot, "mutation:test_preview_active_trait")->highlighted);
                    return choose(snapshot, "action:TOGGLE_EXAMINE");
                }
                case 3:
                    return choose(snapshot, "mutation:test_preview_active_trait");
                default:
                    FAIL("unexpected extra screen " << snapshot.context);
            }
            return input_event{};
        });
    CHECK(stage == 4);
    CHECK_FALSE(you.my_mutations[trait_id("test_preview_active_trait")].powered);
}

TEST_CASE(
    "the bionics list publishes power, state and color per row and switches its tabs",
    "[client][interaction][mcp][screens]") {
    clear_all_state();
    clear_map();
    auto& you = get_avatar();
    clear_character(you, false);
    you.add_bionic(bionic_id("bio_flashlight"));
    you.add_bionic(bionic_id("bio_power_storage"));
    const auto active_id = game_client::opaque_interaction_id("bionic", {"bio_flashlight"});
    const auto passive_id = game_client::opaque_interaction_id("bionic", {"bio_power_storage"});
    const auto tab = [](const snapshot_t& snapshot, const bool active) {
        const auto found = std::ranges::find_if(snapshot.choices, [&](const auto& choice) {
            return !choice.pane_id && choice.label.starts_with(active ? "ACTIVE" : "PASSIVE");
        });
        REQUIRE(found != snapshot.choices.end());
        return found;
    };
    const auto guard = screen_guard{};
    auto stage = 0;
    open_screen(
        [&] { show_bionics_ui(you); },
        [&](const auto& snapshot) {
            switch (stage++) {
                case 0: {
                    CHECK(snapshot.context == "BIONICS");
                    CHECK(tab(snapshot, true)->selected);
                    CHECK_FALSE(tab(snapshot, false)->selected);
                    const auto flashlight = row(snapshot, active_id);
                    REQUIRE(flashlight != snapshot.choices.end());
                    CHECK(flashlight->pane_id == "active");
                    CHECK(column(*flashlight, "State") == "Off");
                    CHECK(column(*flashlight, "Power").find("act") != std::string::npos);
                    CHECK(column(*flashlight, "Color") == string_from_color(c_light_red));
                    CHECK(row(snapshot, passive_id) == snapshot.choices.end());
                    return choose(snapshot, tab(snapshot, false)->id);
                }
                case 1: {
                    CHECK(tab(snapshot, false)->selected);
                    CHECK(row(snapshot, active_id) == snapshot.choices.end());
                    const auto storage = row(snapshot, passive_id);
                    REQUIRE(storage != snapshot.choices.end());
                    CHECK(storage->pane_id == "passive");
                    CHECK(column(*storage, "State") == "Passive");
                    CHECK(column(*storage, "Color") == string_from_color(c_cyan));
                    return choose(snapshot, tab(snapshot, true)->id);
                }
                case 2:
                    CHECK(tab(snapshot, true)->selected);
                    CHECK(row(snapshot, active_id) != snapshot.choices.end());
                    return cancel(snapshot);
                default:
                    FAIL("unexpected extra screen " << snapshot.context);
            }
            return input_event{};
        });
    CHECK(stage == 3);
}

TEST_CASE(
    "the item list shows the items around and moves its cursor to a chosen one",
    "[client][interaction][mcp][screens]") {
    clear_all_state();
    clear_map();
    auto& you = get_avatar();
    clear_character(you, false);
    const auto lighter_pos = you.bub_pos() + tripoint_east;
    const auto knife_pos = you.bub_pos() + tripoint_east * 2;
    get_map().add_item(lighter_pos, item::spawn("lighter", calendar::turn));
    get_map().add_item(knife_pos, item::spawn("knife_combat", calendar::turn));
    const auto& lighter = get_map().i_at(lighter_pos).only_item();
    const auto& knife = get_map().i_at(knife_pos).only_item();
    const auto stacks = std::vector<map_item_stack>{
        map_item_stack(&lighter, tripoint_rel_ms(1, 0, 0)),
        map_item_stack(&knife, tripoint_rel_ms(2, 0, 0)),
    };
    const auto guard = screen_guard{};
    auto stage = 0;
    open_screen(
        [&] { list_items_call(list_items_tag{}, stacks); },
        [&](const auto& snapshot) {
            switch (stage++) {
                case 0: {
                    CHECK(snapshot.context == "LIST_ITEMS");
                    const auto items = rows_with(snapshot, "item:");
                    REQUIRE(items.size() == 2);
                    const auto nearest = row(snapshot, items.front());
                    CHECK(nearest->label.find("lighter") != std::string::npos);
                    CHECK(nearest->highlighted);
                    CHECK_FALSE(nearest->description.empty());
                    CHECK(nearest->columns.front().value == "1");
                    CHECK_FALSE(row(snapshot, items.back())->highlighted);
                    return choose(snapshot, items.back());
                }
                case 1: {
                    const auto items = rows_with(snapshot, "item:");
                    const auto farthest = row(snapshot, items.back());
                    CHECK(farthest->label.find("knife") != std::string::npos);
                    CHECK(farthest->highlighted);
                    CHECK_FALSE(row(snapshot, items.front())->highlighted);
                    CHECK(row(snapshot, "action:EXAMINE") != snapshot.choices.end());
                    return cancel(snapshot);
                }
                default:
                    FAIL("unexpected extra screen " << snapshot.context);
            }
            return input_event{};
        });
    CHECK(stage == 2);
}

TEST_CASE(
    "the armor layer list shows the worn items and a grabbed item moves with the cursor",
    "[client][interaction][mcp][screens]") {
    clear_all_state();
    clear_map();
    auto& you = get_avatar();
    clear_character(you, false);
    you.wear_item(item::spawn("tshirt"));
    you.wear_item(item::spawn("jeans"));
    REQUIRE(you.worn.size() == 2);
    you.moves = 100;
    const auto innermost = you.worn.front()->typeId();
    const auto guard = screen_guard{};
    auto stage = 0;
    open_screen(
        [&] { show_armor_layers_ui(you); },
        [&](const auto& snapshot) {
            switch (stage++) {
                case 0: {
                    CHECK(snapshot.context == "SORT_ARMOR");
                    CHECK(rows_with(snapshot, "tab:").size() > 2);
                    REQUIRE(rows_with(snapshot, "armor:").size() == 2);
                    CHECK(row(snapshot, "armor:0")->highlighted);
                    CHECK(row(snapshot, "armor:0")->columns.size() == 2);
                    return choose(snapshot, "armor:1");
                }
                case 1: {
                    CHECK(row(snapshot, "armor:1")->highlighted);
                    CHECK_FALSE(row(snapshot, "armor:0")->highlighted);
                    return choose(snapshot, "action:MOVE_ARMOR");
                }
                case 2: {
                    CHECK(row(snapshot, "armor:1")->selected);
                    return choose(snapshot, "action:UP");
                }
                case 3: {
                    // The grabbed item went one layer inward with the cursor.
                    CHECK(row(snapshot, "armor:0")->selected);
                    CHECK(row(snapshot, "armor:0")->highlighted);
                    return cancel(snapshot);
                }
                default:
                    FAIL("unexpected extra screen " << snapshot.context);
            }
            return input_event{};
        });
    CHECK(stage == 4);
    CHECK(you.worn.back()->typeId() == innermost);
}

#endif // CATA_MCP
