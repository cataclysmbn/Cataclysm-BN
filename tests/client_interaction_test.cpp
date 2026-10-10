#if defined(CATA_MCP)

#    include "action.h"
#    include "advanced_inv.h"
#    include "avatar.h"
#    include "cached_options.h"
#    include "calendar.h"
#    include "cata_utility.h"
#    include "catch/catch.hpp"
#    include "character_functions.h"
#    include "client_command.h"
#    include "client_input.h"
#    include "client_interaction.h"
#    include "client_memory.h"
#    include "client_memory_scope.h"
#    include "client_presentation.h"
#    include "client_presentation_scope.h"
#    include "construction.h"
#    include "crafting.h"
#    include "cursesdef.h"
#    include "cursesport.h"
#    include "drop_token.h"
#    include "faction.h"
#    include "game.h"
#    include "input.h"
#    include "inventory_ui.h"
#    include "item.h"
#    include "map/map.h"
#    include "map_helpers.h"
#    include "monster.h"
#    include "npc.h"
#    include "npctrade.h"
#    include "options_helpers.h"
#    include "output.h"
#    include "path_info.h"
#    include "pickup.h"
#    include "player_activity.h"
#    include "player_helpers.h"
#    include "popup.h"
#    include "ranged.h"
#    include "recipe.h"
#    include "replay/replay.h"
#    include "rng.h"
#    include "state_helpers.h"
#    include "string_input_popup.h"
#    include "trade_win.h"
#    include "ui.h"
#    include "uistate.h"
#    include "vehicle/veh_type.h"
#    include "vehicle/vehicle.h"

#    include <atomic>
#    include <filesystem>
#    include <functional>
#    include <set>
#    include <stdexcept>
#    include <string>
#    include <vector>

namespace {

struct interaction_test_guard {
    const game_client::memory::scoped_state memory;
    bool old_test_mode = test_mode;
    int old_termx = TERMX;
    int old_termy = TERMY;
    int old_full_screen_width = FULL_SCREEN_WIDTH;
    int old_full_screen_height = FULL_SCREEN_HEIGHT;

    interaction_test_guard() {
        test_mode = false;
        TERMX = 100;
        TERMY = 40;
        FULL_SCREEN_WIDTH = 80;
        FULL_SCREEN_HEIGHT = 24;
        game_client::memory::resize(TERMX, TERMY);
        catacurses::stdscr = catacurses::newwin(TERMY, TERMX, point_zero);
        catacurses::newscr = catacurses::newwin(TERMY, TERMX, point_zero);
    }

    ~interaction_test_guard() {
        TERMX = old_termx;
        TERMY = old_termy;
        FULL_SCREEN_WIDTH = old_full_screen_width;
        FULL_SCREEN_HEIGHT = old_full_screen_height;
        test_mode = old_test_mode;
    }
};

auto resolve(game_client::interaction_command command) -> input_event {
    auto input = game_client::input_command{};
    input.interaction = std::move(command);
    const auto result =
        game_client::resolve_input_command(input, game_client::memory::screen_size());
    REQUIRE(result.has_value());
    return *result;
}

/// The crafting menu lists its tabs ahead of the recipes; most checks only look at the recipes.
auto recipe_rows(game_client::interaction_snapshot snapshot) -> game_client::interaction_snapshot {
    std::erase_if(snapshot.choices, [](const auto& choice) {
        return choice.id.starts_with("tab:") || choice.id.starts_with("subtab:");
    });
    return snapshot;
}

auto resolve_action(const std::string& action) -> input_event {
    auto input = game_client::input_command{};
    input.action = action;
    const auto result =
        game_client::resolve_input_command(input, game_client::memory::screen_size());
    REQUIRE(result.has_value());
    return *result;
}

class denied_inventory_preset final: public inventory_selector_preset {
public:
    auto get_denial(const item* candidate) const -> std::string override {
        return candidate->typeId() == itype_id("bandages") ? "Denied by test preset" : "";
    }
};

auto add_crafting_fixture_item(avatar& you, const itype_id& id, const int count) -> void {
    auto first = item::spawn(id, calendar::turn);
    if (first->count_by_charges()) {
        first->charges = count;
        you.i_add(std::move(first));
        return;
    }
    you.i_add(std::move(first));
    std::ranges::for_each(std::views::iota(1, count), [&](const auto /*index*/) {
        you.i_add(item::spawn(id, calendar::turn));
    });
}

auto prepare_crafting_fixture(const bool learn_pipe = true) -> void {
    auto& you = get_avatar();
    const auto& soldering_iron = recipe_id("test_soldering_iron").obj();
    you.learn_recipe(&soldering_iron);
    you.set_skill_level(soldering_iron.skill_used, 2);
    if (learn_pipe) {
        const auto& pipe = recipe_id("test_pipe").obj();
        you.learn_recipe(&pipe);
        you.set_skill_level(pipe.skill_used, 1);
    }
    add_crafting_fixture_item(you, itype_id("test_screwdriver"), 1);
    add_crafting_fixture_item(you, itype_id("knife_steak"), 1);
    add_crafting_fixture_item(you, itype_id("e_scrap"), 4);
    add_crafting_fixture_item(you, itype_id("copper"), 2);
    add_crafting_fixture_item(you, itype_id("scrap"), 2);
    add_crafting_fixture_item(you, itype_id("duct_tape"), 20);
    add_crafting_fixture_item(you, itype_id("cable"), 10);
    you.invalidate_crafting_inventory();
}

auto finish_crafting_activity(avatar& you) -> void {
    auto turns = 0;
    while (you.activity && you.activity->id() == activity_id("ACT_CRAFT") && turns++ < 2000) {
        you.set_moves(100000);
        you.activity->do_turn(you);
    }
    REQUIRE(turns < 2000);
}

/// Reset state that the general map test helpers do not clear.
auto clear_construction_fixture(const tripoint_bub_ms& destination) -> void {
    auto& you = get_avatar();
    if (you.activity && you.activity->id() == activity_id("ACT_BUILD")) {
        you.activity->set_to_null();
    }
    get_map().partial_con_remove(destination);
    get_map().trap_set(destination, trap_id("tr_null"));
    get_map().furn_set(destination, furn_id("f_null"));
}

auto finish_construction_activity(avatar& you) -> void {
    auto turns = 0;
    while (you.activity && you.activity->id() == activity_id("ACT_BUILD") && turns++ < 2000) {
        you.set_moves(100000);
        you.activity->do_turn(you);
    }
    REQUIRE(turns < 2000);
}

constexpr auto pickup_test_pos = tripoint_bub_ms{60, 60, 0};

auto clear_pickup_fixture_state() -> void {
    const auto previous_test_mode = test_mode;
    test_mode = true;
    clear_all_state();
    clear_character(get_avatar(), false);
    test_mode = previous_test_mode;
}

struct pickup_fixture_guard {
    explicit pickup_fixture_guard(const bool with_storage = true) {
        clear_pickup_fixture_state();
        auto& you = get_avatar();
        you.setpos(map_local_to_abs(get_map(), pickup_test_pos));
        you.set_moves(1000);
        if (with_storage) { you.worn.push_back(item::spawn("duffelbag")); }
    }

    ~pickup_fixture_guard() { clear_pickup_fixture_state(); }
};

auto add_ground_item(const itype_id& id, const int charges = -1) -> item& { // *NOPAD*
    auto added = item::spawn(id, calendar::turn, charges);
    auto& result = *added;
    get_map().add_item(pickup_test_pos, std::move(added));
    return result;
}

auto ground_item_count(const itype_id& id) -> int {
    return std::ranges::count_if(get_map().i_at(pickup_test_pos), [&](const auto* candidate) {
        return candidate->typeId() == id;
    });
}

auto ground_charges(const itype_id& id) -> int {
    auto result = 0;
    for (const auto* candidate : get_map().i_at(pickup_test_pos)) {
        if (candidate->typeId() == id) { result += candidate->charges; }
    }
    return result;
}

auto finish_pickup_activity(avatar& you) -> void {
    REQUIRE(you.activity);
    REQUIRE(you.activity->id() == activity_id("ACT_PICKUP"));
    process_activity(you);
    REQUIRE_FALSE(you.activity);
}

constexpr auto advanced_inventory_test_pos = tripoint_bub_ms{60, 60, 0};

struct advanced_inventory_fixture_guard {
    advanced_inv_save_state old_transfer_save = uistate.transfer_save;
    int old_container_location = uistate.adv_inv_container_location;
    int old_container_index = uistate.adv_inv_container_index;
    itype_id old_container_type = uistate.adv_inv_container_type;
    itype_id old_container_content_type = uistate.adv_inv_container_content_type;
    bool old_container_in_vehicle = uistate.adv_inv_container_in_vehicle;
    time_point old_turn = calendar::turn;

    advanced_inventory_fixture_guard() {
        const auto previous_test_mode = test_mode;
        test_mode = true;
        clear_all_state();
        clear_character(get_avatar(), false);
        test_mode = previous_test_mode;
        calendar::turn = calendar::turn_zero + 12_hours;
        auto& you = get_avatar();
        you.setpos(map_local_to_abs(get_map(), advanced_inventory_test_pos));
        you.set_moves(1000);
        you.worn.push_back(item::spawn("duffelbag"));
        uistate.transfer_save = advanced_inv_save_state{};
        uistate.transfer_save.active_left = true;
        uistate.transfer_save.saved_area = AIM_INVENTORY;
        uistate.transfer_save.saved_area_right = AIM_CENTER;
        uistate.transfer_save.pane.area_idx = AIM_INVENTORY;
        uistate.transfer_save.pane_right.area_idx = AIM_CENTER;
        uistate.transfer_save.pane.sort_idx = SORTBY_NAME;
        uistate.transfer_save.pane_right.sort_idx = SORTBY_NAME;
        uistate.adv_inv_container_location = -1;
        uistate.adv_inv_container_index = 0;
        uistate.adv_inv_container_type = itype_id::NULL_ID();
        uistate.adv_inv_container_content_type = itype_id::NULL_ID();
        uistate.adv_inv_container_in_vehicle = false;
    }

    ~advanced_inventory_fixture_guard() {
        const auto previous_test_mode = test_mode;
        test_mode = true;
        clear_all_state();
        clear_character(get_avatar(), false);
        test_mode = previous_test_mode;
        uistate.transfer_save = old_transfer_save;
        uistate.adv_inv_container_location = old_container_location;
        uistate.adv_inv_container_index = old_container_index;
        uistate.adv_inv_container_type = old_container_type;
        uistate.adv_inv_container_content_type = old_container_content_type;
        uistate.adv_inv_container_in_vehicle = old_container_in_vehicle;
        calendar::turn = old_turn;
    }
};

constexpr auto trade_test_pos = tripoint_bub_ms{60, 60, 0};

struct trade_fixture_guard {
    time_point old_turn = calendar::turn;
    cata_default_random_engine old_rng = rng_get_engine();
    npc* trader = nullptr;

    trade_fixture_guard() {
        const auto previous_test_mode = test_mode;
        test_mode = true;
        clear_all_state();
        clear_character(get_avatar(), false);
        calendar::turn = calendar::turn_zero + 12_hours;
        auto& you = get_avatar();
        you.setpos(map_local_to_abs(get_map(), trade_test_pos));
        you.set_moves(1000);
        g->faction_manager_ptr->create_if_needed();
        trader = &spawn_npc(trade_test_pos + tripoint_east, "test_talker");
        clear_character(*trader, false);
        trader->name = "Semantic trade fixture";
        trader->set_fac(faction_id("tacoma_commune"));
        trader->mission = NPC_MISSION_NULL;
        trader->op_of_u = npc_opinion{};
        auto player_storage = item::spawn("duffelbag", calendar::turn);
        player_storage->set_owner(you);
        you.worn.push_back(std::move(player_storage));
        auto npc_storage = item::spawn("duffelbag", calendar::turn);
        npc_storage->set_owner(*trader);
        trader->worn.push_back(std::move(npc_storage));
        test_mode = previous_test_mode;
    }

    ~trade_fixture_guard() {
        const auto previous_test_mode = test_mode;
        test_mode = true;
        clear_all_state();
        clear_character(get_avatar(), false);
        test_mode = previous_test_mode;
        calendar::turn = old_turn;
        rng_get_engine() = old_rng;
    }

    auto add_player_item(const itype_id& type, const int charges = -1) const -> item& { // *NOPAD*
        auto added = item::spawn(type, calendar::turn, charges);
        added->set_owner(get_avatar());
        return get_avatar().i_add(std::move(added));
    }

    auto add_npc_item(const itype_id& type, const int charges = -1) const -> item& { // *NOPAD*
        auto added = item::spawn(type, calendar::turn, charges);
        added->set_owner(*trader);
        return trader->i_add(std::move(added));
    }
};

auto find_choice_type(
    const game_client::interaction_snapshot& snapshot, const std::string& type,
    const std::string& role) {
    const auto pane = std::ranges::find(snapshot.panes, role, &game_client::interaction_pane::role);
    REQUIRE(pane != snapshot.panes.end());
    return std::ranges::find_if(snapshot.choices, [&](const auto& choice) {
        return choice.pane_id == pane->id
            && std::ranges::any_of(choice.columns, [&](const auto& column) {
                   return column.label == "Type" && column.value == type;
               });
    });
}

class counting_callback final: public uilist_callback {
public:
    auto select(uilist* /*menu*/) -> void override { ++selections; }
    int selections = 0;
};

} // namespace

TEST_CASE("interaction_test_guard leaves a borrowed compositor intact", "[client][mcp]") {
    const auto outer = game_client::memory::scoped_state{};
    game_client::memory::resize(7, 3);
    catacurses::stdscr = catacurses::newwin(3, 7, point_zero);
    catacurses::mvwprintw(catacurses::stdscr, point_zero, "x");
    catacurses::wrefresh(catacurses::stdscr);
    const auto borrowed = catacurses::stdscr;
    {
        const auto guard = interaction_test_guard{};
        CHECK(catacurses::stdscr.get<cata_cursesport::WINDOW>() != nullptr);
    }
    CHECK(game_client::memory::screen_size() == point(7, 3));
    CHECK(game_client::memory::snapshot().cells[0].text == "x");
    CHECK(catacurses::stdscr.get<cata_cursesport::WINDOW>()
          == borrowed.get<cata_cursesport::WINDOW>());
    CHECK(catacurses::stdscr.get<cata_cursesport::WINDOW>()->width == 7);
}

TEST_CASE(
    "NPC trade exposes filtered player and NPC selections without exchanging items",
    "[client][interaction][npc_trade][mcp]") {
    const auto guard = interaction_test_guard{};
    const auto fixture = trade_fixture_guard{};
    fixture.trader->mission = NPC_MISSION_SHOPKEEP;
    fixture.add_npc_item(itype_id("test_platinum_bit"), 10);
    fixture.add_npc_item(itype_id("test_halligan"));
    std::ranges::for_each(std::views::iota(0, 3), [&](const auto /*index*/) {
        fixture.add_player_item(itype_id("test_pipe"));
    });
    auto& foreign = fixture.add_player_item(itype_id("test_fire_ax"));
    foreign.set_owner(*fixture.trader);
    const auto original_player_pipes = get_avatar().amount_of(itype_id("test_pipe"));
    const auto original_npc_charges = fixture.trader->charges_of(itype_id("test_platinum_bit"));
    const auto original_debt = fixture.trader->op_of_u.owed;
    auto state = npc_trading::trade_state{};
    npc_trading::setup_trade_state(state, 0, *fixture.trader);
    REQUIRE_FALSE(state.theirs.empty());
    REQUIRE_FALSE(state.yours.empty());
    auto hidden_id = std::string{};
    auto initial_input_id = std::uint64_t{0};
    auto step = 0;

    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto snapshot = game_client::current_interaction({.limit = 200});
        if (snapshot.context != "NPC_TRADE") {
            REQUIRE(snapshot.kind == game_client::interaction_kind::field);
            REQUIRE(step == 4);
            step = 5;
            return resolve({
                .input_id = snapshot.input_id,
                .operation = game_client::interaction_operation::fill,
                .target_id = snapshot.field->id,
                .value = "__no_trade_match__",
                .submit = true,
            });
        }
        REQUIRE(snapshot.kind == game_client::interaction_kind::inventory);
        REQUIRE(snapshot.panes.size() == 2);
        CHECK(snapshot.allow_set_count);
        CHECK(snapshot.allow_cancel);
        CHECK(std::ranges::count(snapshot.panes, "npc", &game_client::interaction_pane::role) == 1);
        CHECK(std::ranges::count(snapshot.panes, "player", &game_client::interaction_pane::role)
              == 1);
        CHECK(snapshot.message.find("acceptable") != std::string::npos);
        CHECK(find_choice_type(snapshot, "test_fire_ax", "player") == snapshot.choices.end());
        if (step == 0) {
            const auto charges = find_choice_type(snapshot, "test_platinum_bit", "npc");
            const auto pipes = find_choice_type(snapshot, "test_pipe", "player");
            REQUIRE(charges != snapshot.choices.end());
            REQUIRE(pipes != snapshot.choices.end());
            CHECK(charges->available_count == 10);
            CHECK(charges->selected_count == 0);
            CHECK(charges->minimum_count == 0);
            CHECK(pipes->available_count == 3);
            CHECK(pipes->selected_count == 0);
            CHECK(charges->storage_kind == "inventory");
            CHECK(pipes->storage_kind == "inventory");
            CHECK(charges->pane_id != pipes->pane_id);
            CHECK(charges->area_id != pipes->area_id);
            CHECK(std::ranges::any_of(charges->columns, [](const auto& column) {
                return column.label == "Unit price" && !column.value.empty();
            }));
            const auto too_many = game_client::resolve_interaction_command({
                .input_id = snapshot.input_id,
                .operation = game_client::interaction_operation::set_count,
                .target_id = charges->id,
                .count = 11,
            });
            REQUIRE_FALSE(too_many);
            CHECK(too_many.error().starts_with("invalid:"));
            hidden_id = charges->id;
            initial_input_id = snapshot.input_id;
            step = 1;
            return resolve({
                .input_id = snapshot.input_id,
                .operation = game_client::interaction_operation::set_count,
                .target_id = charges->id,
                .count = 3,
            });
        }
        if (step == 1) {
            const auto charges = find_choice_type(snapshot, "test_platinum_bit", "npc");
            const auto pipes = find_choice_type(snapshot, "test_pipe", "player");
            REQUIRE(charges != snapshot.choices.end());
            REQUIRE(pipes != snapshot.choices.end());
            CHECK(charges->selected);
            CHECK(charges->selected_count == 3);
            step = 2;
            return resolve({
                .input_id = snapshot.input_id,
                .operation = game_client::interaction_operation::set_count,
                .target_id = pipes->id,
                .count = 2,
            });
        }
        if (step == 2) {
            const auto charges = find_choice_type(snapshot, "test_platinum_bit", "npc");
            const auto pipes = find_choice_type(snapshot, "test_pipe", "player");
            REQUIRE(charges != snapshot.choices.end());
            REQUIRE(pipes != snapshot.choices.end());
            CHECK(charges->selected_count == 3);
            CHECK(pipes->selected_count == 2);
            CHECK(pipes->selected);
            step = 3;
            return resolve({
                .input_id = snapshot.input_id,
                .operation = game_client::interaction_operation::set_count,
                .target_id = charges->id,
                .count = 0,
            });
        }
        if (step == 3) {
            const auto charges = find_choice_type(snapshot, "test_platinum_bit", "npc");
            REQUIRE(charges != snapshot.choices.end());
            CHECK_FALSE(charges->selected);
            CHECK(charges->selected_count == 0);
            step = 4;
            return resolve_action("FILTER");
        }
        if (step == 5) {
            CHECK(find_choice_type(snapshot, "test_platinum_bit", "npc") == snapshot.choices.end());
            CHECK(find_choice_type(snapshot, "test_pipe", "player") != snapshot.choices.end());
            CHECK(std::ranges::none_of(snapshot.choices, [&](const auto& choice) {
                return choice.id == hidden_id;
            }));
            const auto hidden = game_client::resolve_interaction_command({
                .input_id = snapshot.input_id,
                .operation = game_client::interaction_operation::choose,
                .target_id = hidden_id,
            });
            REQUIRE_FALSE(hidden);
            CHECK(hidden.error().starts_with("invalid:"));
            const auto stale = game_client::resolve_interaction_command({
                .input_id = initial_input_id,
                .operation = game_client::interaction_operation::choose,
                .target_id = hidden_id,
            });
            REQUIRE_FALSE(stale);
            CHECK(stale.error().starts_with("stale:"));
            step = 6;
            return resolve_action("RESET_FILTER");
        }
        if (step == 6) {
            const auto singleton = find_choice_type(snapshot, "test_halligan", "npc");
            REQUIRE(singleton != snapshot.choices.end());
            CHECK(singleton->available_count == 1);
            CHECK_FALSE(singleton->selected);
            step = 7;
            return resolve({
                .input_id = snapshot.input_id,
                .operation = game_client::interaction_operation::choose,
                .target_id = singleton->id,
            });
        }
        if (step == 7) {
            const auto singleton = find_choice_type(snapshot, "test_halligan", "npc");
            REQUIRE(singleton != snapshot.choices.end());
            CHECK(singleton->selected_count == 1);
            step = 8;
            return resolve({
                .input_id = snapshot.input_id,
                .operation = game_client::interaction_operation::choose,
                .target_id = singleton->id,
            });
        }
        REQUIRE(step == 8);
        const auto singleton = find_choice_type(snapshot, "test_halligan", "npc");
        REQUIRE(singleton != snapshot.choices.end());
        CHECK(singleton->selected_count == 0);
        step = 9;
        return resolve({
            .input_id = snapshot.input_id,
            .operation = game_client::interaction_operation::cancel,
        });
    });

    const auto traded = trading_window(state).perform_trade(*fixture.trader, "Trade");
    CHECK_FALSE(traded);
    CHECK(step == 9);
    CHECK(get_avatar().amount_of(itype_id("test_pipe")) == original_player_pipes);
    CHECK(fixture.trader->charges_of(itype_id("test_platinum_bit")) == original_npc_charges);
    CHECK(fixture.trader->op_of_u.owed == original_debt);
}

TEST_CASE(
    "NPC trade semantic choose preserves the native multi-count quantity field",
    "[client][interaction][npc_trade][mcp]") {
    const auto guard = interaction_test_guard{};
    const auto fixture = trade_fixture_guard{};
    auto& candidate = fixture.add_npc_item(itype_id("test_platinum_bit"), 10);
    auto state = npc_trading::trade_state{};
    state.theirs.emplace_back(std::vector<item*>{&candidate}, 100, 1);
    auto step = 0;

    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto snapshot = game_client::current_interaction({.limit = 200});
        if (snapshot.context != "NPC_TRADE") {
            REQUIRE(step == 1);
            REQUIRE(snapshot.kind == game_client::interaction_kind::field);
            step = 2;
            return resolve({
                .input_id = snapshot.input_id,
                .operation = game_client::interaction_operation::fill,
                .target_id = snapshot.field->id,
                .value = "3",
                .submit = true,
            });
        }
        const auto target = find_choice_type(snapshot, "test_platinum_bit", "npc");
        REQUIRE(target != snapshot.choices.end());
        if (step == 0) {
            CHECK(target->selected_count == 0);
            step = 1;
            return resolve({
                .input_id = snapshot.input_id,
                .operation = game_client::interaction_operation::choose,
                .target_id = target->id,
            });
        }
        if (step == 2) {
            CHECK(target->selected_count == 3);
            step = 3;
            return resolve({
                .input_id = snapshot.input_id,
                .operation = game_client::interaction_operation::choose,
                .target_id = target->id,
            });
        }
        REQUIRE(step == 3);
        CHECK(target->selected_count == 0);
        step = 4;
        return resolve({
            .input_id = snapshot.input_id,
            .operation = game_client::interaction_operation::cancel,
        });
    });

    CHECK_FALSE(trading_window(state).perform_trade(*fixture.trader, "Trade"));
    CHECK(step == 4);
    CHECK(fixture.trader->charges_of(itype_id("test_platinum_bit")) == 10);
    CHECK(get_avatar().charges_of(itype_id("test_platinum_bit")) == 0);
}

TEST_CASE(
    "NPC trade explicit native confirmation exchanges items and updates priced debt",
    "[client][interaction][npc_trade][mcp]") {
    const auto guard = interaction_test_guard{};
    const auto fixture = trade_fixture_guard{};
    auto& sold = fixture.add_npc_item(itype_id("test_pipe"));
    fixture.trader->op_of_u.owed = 100000;
    auto preview = npc_trading::trade_state{};
    npc_trading::setup_trade_state(preview, 0, *fixture.trader);
    const auto priced = std::ranges::find_if(preview.theirs, [](const auto& candidate) {
        return candidate.locs.front()->typeId() == itype_id("test_pipe");
    });
    REQUIRE(priced != preview.theirs.end());
    const auto unit_price = static_cast<int>(priced->price);
    const auto original_debt = fixture.trader->op_of_u.owed;
    const auto original_player_count = get_avatar().amount_of(itype_id("test_pipe"));
    const auto original_npc_count = fixture.trader->amount_of(itype_id("test_pipe"));
    auto step = 0;

    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto snapshot = game_client::current_interaction({.limit = 200});
        if (snapshot.context == "NPC_TRADE") {
            if (step == 0) {
                const auto target = find_choice_type(snapshot, "test_pipe", "npc");
                REQUIRE(target != snapshot.choices.end());
                step = 1;
                return resolve({
                    .input_id = snapshot.input_id,
                    .operation = game_client::interaction_operation::set_count,
                    .target_id = target->id,
                    .count = 1,
                });
            }
            REQUIRE(step == 1);
            CHECK(get_avatar().amount_of(itype_id("test_pipe")) == original_player_count);
            CHECK(fixture.trader->amount_of(itype_id("test_pipe")) == original_npc_count);
            CHECK(fixture.trader->op_of_u.owed == original_debt);
            step = 2;
            return resolve_action("CONFIRM");
        }
        REQUIRE(snapshot.context == "YESNO");
        REQUIRE(step == 2);
        const auto yes = std::ranges::
            find(snapshot.choices, "YES", &game_client::interaction_choice::description);
        REQUIRE(yes != snapshot.choices.end());
        step = 3;
        return resolve({
            .input_id = snapshot.input_id,
            .operation = game_client::interaction_operation::choose,
            .target_id = yes->id,
        });
    });

    CHECK(npc_trading::trade(*fixture.trader, 0, "Trade"));
    CHECK(step == 3);
    CHECK(get_avatar().amount_of(itype_id("test_pipe")) == original_player_count + 1);
    CHECK(fixture.trader->amount_of(itype_id("test_pipe")) == original_npc_count - 1);
    CHECK(fixture.trader->op_of_u.owed == original_debt - unit_price);
    CHECK(sold.is_owned_by(get_avatar()));
}

TEST_CASE(
    "NPC trade preserves native credit and capacity denials",
    "[client][interaction][npc_trade][mcp]") {
    const auto guard = interaction_test_guard{};

    SECTION("insufficient credit") {
        const auto fixture = trade_fixture_guard{};
        fixture.trader->personality.altruism = 0;
        auto& candidate = fixture.add_npc_item(itype_id("test_pipe"));
        auto state = npc_trading::trade_state{};
        state.theirs.emplace_back(std::vector<item*>{&candidate}, 1000, 1);
        const auto original_debt = fixture.trader->op_of_u.owed;
        auto step = 0;
        game_client::memory::set_input_provider([&](const int /*timeout*/) {
            const auto snapshot = game_client::current_interaction({.limit = 200});
            if (snapshot.context == "NPC_TRADE") {
                if (step == 0) {
                    const auto target = find_choice_type(snapshot, "test_pipe", "npc");
                    REQUIRE(target != snapshot.choices.end());
                    step = 1;
                    return resolve({
                        .input_id = snapshot.input_id,
                        .operation = game_client::interaction_operation::set_count,
                        .target_id = target->id,
                        .count = 1,
                    });
                }
                if (step == 1) {
                    step = 2;
                    return resolve_action("CONFIRM");
                }
                REQUIRE(step == 3);
                step = 4;
                return resolve({
                    .input_id = snapshot.input_id,
                    .operation = game_client::interaction_operation::cancel,
                });
            }
            REQUIRE(snapshot.context == "POPUP_WAIT");
            REQUIRE(step == 2);
            CHECK(snapshot.message.find("offer") != std::string::npos);
            step = 3;
            return resolve({
                .input_id = snapshot.input_id,
                .operation = game_client::interaction_operation::choose,
                .target_id = snapshot.choices.front().id,
            });
        });

        CHECK_FALSE(trading_window(state).perform_trade(*fixture.trader, "Trade"));
        CHECK(step == 4);
        CHECK(fixture.trader->amount_of(itype_id("test_pipe")) == 1);
        CHECK(get_avatar().amount_of(itype_id("test_pipe")) == 0);
        CHECK(fixture.trader->op_of_u.owed == original_debt);
    }

    SECTION("NPC carry capacity") {
        const auto fixture = trade_fixture_guard{};
        auto& candidate = fixture.add_player_item(itype_id("test_overweight_cube"));
        auto state = npc_trading::trade_state{};
        state.yours.emplace_back(std::vector<item*>{&candidate}, 1000, 1);
        auto step = 0;
        game_client::memory::set_input_provider([&](const int /*timeout*/) {
            const auto snapshot = game_client::current_interaction({.limit = 200});
            if (snapshot.context == "NPC_TRADE") {
                if (step == 0) {
                    const auto target =
                        find_choice_type(snapshot, "test_overweight_cube", "player");
                    REQUIRE(target != snapshot.choices.end());
                    step = 1;
                    return resolve({
                        .input_id = snapshot.input_id,
                        .operation = game_client::interaction_operation::set_count,
                        .target_id = target->id,
                        .count = 1,
                    });
                }
                if (step == 1) {
                    step = 2;
                    return resolve_action("CONFIRM");
                }
                REQUIRE(step == 3);
                step = 4;
                return resolve({
                    .input_id = snapshot.input_id,
                    .operation = game_client::interaction_operation::cancel,
                });
            }
            REQUIRE(snapshot.context == "POPUP_WAIT");
            REQUIRE(step == 2);
            CHECK(snapshot.message.find("carry") != std::string::npos);
            step = 3;
            return resolve({
                .input_id = snapshot.input_id,
                .operation = game_client::interaction_operation::choose,
                .target_id = snapshot.choices.front().id,
            });
        });

        CHECK_FALSE(trading_window(state).perform_trade(*fixture.trader, "Trade"));
        CHECK(step == 4);
        CHECK(get_avatar().amount_of(itype_id("test_overweight_cube")) == 1);
        CHECK(fixture.trader->amount_of(itype_id("test_overweight_cube")) == 0);
    }
}

TEST_CASE(
    "semantic replay repeats a native NPC trade transaction",
    "[client][interaction][npc_trade][replay][mcp]") {
    const auto guard = interaction_test_guard{};
    static auto counter = std::atomic_uint64_t{0};
    const auto directory =
        std::filesystem::path(PATH_INFO::user_dir())
        / ("semantic-npc-trade-" + std::to_string(counter.fetch_add(1)));
    REQUIRE(std::filesystem::create_directory(directory));
    const auto cleanup_replay = on_out_of_scope([&] {
        replay::stop();
        std::filesystem::remove_all(directory);
    });
    const auto path = directory / "input.jsonl";

    const auto run_record_provider = [](int& step) {
        return [&step](const int /*timeout*/) {
            const auto snapshot = game_client::current_interaction({.limit = 200});
            if (snapshot.context == "NPC_TRADE") {
                if (step == 0) {
                    const auto target = find_choice_type(snapshot, "test_pipe", "npc");
                    REQUIRE(target != snapshot.choices.end());
                    step = 1;
                    return resolve({
                        .input_id = snapshot.input_id,
                        .operation = game_client::interaction_operation::set_count,
                        .target_id = target->id,
                        .count = 1,
                    });
                }
                REQUIRE(step == 1);
                step = 2;
                return resolve_action("CONFIRM");
            }
            REQUIRE(snapshot.context == "YESNO");
            REQUIRE(step == 2);
            const auto yes = std::ranges::
                find(snapshot.choices, "YES", &game_client::interaction_choice::description);
            REQUIRE(yes != snapshot.choices.end());
            step = 3;
            return resolve({
                .input_id = snapshot.input_id,
                .operation = game_client::interaction_operation::choose,
                .target_id = yes->id,
            });
        };
    };

    {
        const auto fixture = trade_fixture_guard{};
        fixture.add_npc_item(itype_id("test_pipe"));
        fixture.trader->op_of_u.owed = 100000;
        replay::configure_recording(path.string(), {.rng_seed = 41});
        replay::start();
        auto step = 0;
        game_client::memory::set_input_provider(run_record_provider(step));
        CHECK(npc_trading::trade(*fixture.trader, 0, "Trade"));
        CHECK(step == 3);
        CHECK(get_avatar().amount_of(itype_id("test_pipe")) == 1);
        CHECK(fixture.trader->amount_of(itype_id("test_pipe")) == 0);
        replay::finish();
    }

    {
        const auto fixture = trade_fixture_guard{};
        fixture.add_npc_item(itype_id("test_pipe"));
        fixture.trader->op_of_u.owed = 100000;
        game_client::memory::set_input_provider([](const int /*timeout*/) -> input_event {
            throw std::runtime_error("NPC trade playback must not read live input");
        });
        replay::configure_playback(path.string(), {.rng_seed = 41});
        replay::start();
        CHECK(npc_trading::trade(*fixture.trader, 0, "Trade"));
        CHECK_THROWS_AS(replay::next_input_event(), replay::completed);
        CHECK(get_avatar().amount_of(itype_id("test_pipe")) == 1);
        CHECK(fixture.trader->amount_of(itype_id("test_pipe")) == 0);
        replay::finish();
    }

    {
        const auto fixture = trade_fixture_guard{};
        fixture.add_npc_item(itype_id("test_pipe"));
        fixture.add_npc_item(itype_id("test_halligan"));
        fixture.trader->op_of_u.owed = 100000;
        const auto original_debt = fixture.trader->op_of_u.owed;
        game_client::memory::set_input_provider([](const int /*timeout*/) -> input_event {
            throw std::runtime_error("changed NPC trade playback must not read live input");
        });
        replay::configure_playback(path.string(), {.rng_seed = 41});
        replay::start();
        CHECK_THROWS_WITH(npc_trading::trade(*fixture.trader, 0, "Trade"),
                          Catch::Matchers::Contains("schema changed"));
        CHECK(get_avatar().amount_of(itype_id("test_pipe")) == 0);
        CHECK(fixture.trader->amount_of(itype_id("test_pipe")) == 1);
        CHECK(fixture.trader->op_of_u.owed == original_debt);
        replay::stop();
    }
}

TEST_CASE(
    "advanced inventory exposes both live panes and semantic focus without transfer",
    "[client][interaction][advanced_inventory][mcp]") {
    const auto guard = interaction_test_guard{};
    const auto fixture = advanced_inventory_fixture_guard{};
    auto& you = get_avatar();
    std::ranges::for_each(std::views::iota(0, 3), [&](const auto /*index*/) {
        you.i_add(item::spawn("test_1kg_cube", calendar::turn));
    });
    auto charges = item::spawn("test_platinum_bit", calendar::turn, 10);
    get_map().add_item(advanced_inventory_test_pos, std::move(charges));
    auto focused_id = std::string{};
    auto initial_schema = std::string{};
    auto reads = 0;

    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto snapshot = game_client::current_interaction({.limit = 200});
        REQUIRE(snapshot.context == "ADVANCED_INVENTORY");
        REQUIRE(snapshot.kind == game_client::interaction_kind::inventory);
        REQUIRE(snapshot.panes.size() == 2);
        CHECK_FALSE(snapshot.allow_set_count);
        CHECK(snapshot.allow_cancel);
        CHECK(std::ranges::count(snapshot.panes, "source", &game_client::interaction_pane::role)
              == 1);
        CHECK(
            std::ranges::count(snapshot.panes, "destination", &game_client::interaction_pane::role)
            == 1);
        if (reads++ == 0) {
            const auto inventory = find_choice_type(snapshot, "test_1kg_cube", "source");
            const auto ground = find_choice_type(snapshot, "test_platinum_bit", "destination");
            REQUIRE(inventory != snapshot.choices.end());
            REQUIRE(ground != snapshot.choices.end());
            CHECK(inventory->available_count == 3);
            CHECK(ground->available_count == 10);
            CHECK(inventory->storage_kind == "inventory");
            CHECK(ground->storage_kind == "ground");
            const auto source =
                std::ranges::find(snapshot.panes, "source", &game_client::interaction_pane::role);
            REQUIRE(source != snapshot.panes.end());
            CHECK(source->storage_kind == "inventory");
            const auto destination = std::ranges::
                find(snapshot.panes, "destination", &game_client::interaction_pane::role);
            REQUIRE(destination != snapshot.panes.end());
            CHECK(destination->storage_kind == "ground");
            CHECK(inventory->area_id != ground->area_id);
            CHECK_FALSE(inventory->selected_count);
            CHECK_FALSE(inventory->minimum_count);
            CHECK_FALSE(ground->selected_count);
            CHECK_FALSE(ground->minimum_count);
            CHECK(inventory->highlighted);
            CHECK_FALSE(ground->highlighted);
            initial_schema = snapshot.schema_id;
            focused_id = ground->id;
            return resolve({
                .input_id = snapshot.input_id,
                .operation = game_client::interaction_operation::choose,
                .target_id = focused_id,
            });
        }
        const auto focused =
            std::ranges::find(snapshot.choices, focused_id, &game_client::interaction_choice::id);
        REQUIRE(focused != snapshot.choices.end());
        CHECK(focused->highlighted);
        CHECK(snapshot.schema_id != initial_schema);
        CHECK(you.amount_of(itype_id("test_1kg_cube")) == 3);
        CHECK(ground_charges(itype_id("test_platinum_bit")) == 10);
        CHECK_FALSE(you.activity);
        return resolve({
            .input_id = snapshot.input_id,
            .operation = game_client::interaction_operation::cancel,
        });
    });

    create_advanced_inv();
    CHECK(reads == 2);
    CHECK(you.amount_of(itype_id("test_1kg_cube")) == 3);
    CHECK(ground_charges(itype_id("test_platinum_bit")) == 10);
    CHECK_FALSE(you.activity);
}

TEST_CASE(
    "advanced inventory distinguishes inventory and mixed ground cargo storage",
    "[client][interaction][advanced_inventory][mcp]") {
    const auto guard = interaction_test_guard{};
    const auto fixture = advanced_inventory_fixture_guard{};
    auto& here = get_map();
    const auto cart_pos = advanced_inventory_test_pos + tripoint_east;
    auto* const cart = here.add_vehicle(vproto_id("shopping_cart"), cart_pos, 0_degrees, 0, 0);
    REQUIRE(cart != nullptr);
    const auto cargo = cart->part_with_feature(tripoint_mnt_veh::zero(), "CARGO", true);
    REQUIRE(cargo >= 0);
    cart->get_items(cargo).clear();
    REQUIRE_FALSE(cart->add_item(cargo, item::spawn("test_1kg_cube", calendar::turn)));
    here.add_item(cart_pos, item::spawn("test_1kg_cube", calendar::turn));
    uistate.transfer_save.saved_area = AIM_ALL;
    uistate.transfer_save.pane.area_idx = AIM_ALL;
    auto reads = 0;

    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto snapshot = game_client::current_interaction({.limit = 200});
        REQUIRE(snapshot.context == "ADVANCED_INVENTORY");
        const auto source =
            std::ranges::find(snapshot.panes, "source", &game_client::interaction_pane::role);
        REQUIRE(source != snapshot.panes.end());
        CHECK(source->storage_kind == "mixed");
        const auto ground = std::ranges::find_if(snapshot.choices, [&](const auto& choice) {
            return choice.pane_id == source->id && choice.storage_kind == "ground"
                && std::ranges::any_of(choice.columns, [](const auto& column) {
                       return column.label == "Type" && column.value == "test_1kg_cube";
                   });
        });
        const auto cargo_item = std::ranges::find_if(snapshot.choices, [&](const auto& choice) {
            return choice.pane_id == source->id && choice.storage_kind == "cargo"
                && std::ranges::any_of(choice.columns, [](const auto& column) {
                       return column.label == "Type" && column.value == "test_1kg_cube";
                   });
        });
        REQUIRE(ground != snapshot.choices.end());
        REQUIRE(cargo_item != snapshot.choices.end());
        REQUIRE(ground->area_id);
        REQUIRE(cargo_item->area_id);
        CHECK(*ground->area_id != *cargo_item->area_id);
        ++reads;
        return resolve({
            .input_id = snapshot.input_id,
            .operation = game_client::interaction_operation::cancel,
        });
    });

    create_advanced_inv();
    CHECK(reads == 1);
    CHECK_FALSE(get_avatar().activity);
    CHECK(get_avatar().backlog.empty());
    CHECK(
        std::ranges::count_if(
            here.i_at(cart_pos),
            [](const auto* candidate) { return candidate->typeId() == itype_id("test_1kg_cube"); })
        == 1);
    CHECK(cart->get_items(cargo).size() == 1);
}

TEST_CASE(
    "advanced inventory filters one pane and rejects removed and stale identities",
    "[client][interaction][advanced_inventory][mcp]") {
    const auto guard = interaction_test_guard{};
    const auto fixture = advanced_inventory_fixture_guard{};
    auto& you = get_avatar();
    you.i_add(item::spawn("test_1kg_cube", calendar::turn));
    auto charges = item::spawn("test_platinum_bit", calendar::turn, 10);
    get_map().add_item(advanced_inventory_test_pos, std::move(charges));
    auto removed_id = std::string{};
    auto old_input_id = std::uint64_t{0};
    auto step = 0;

    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto snapshot = game_client::current_interaction({.limit = 200});
        if (snapshot.context != "ADVANCED_INVENTORY") {
            REQUIRE(snapshot.kind == game_client::interaction_kind::field);
            REQUIRE(step == 1);
            step = 2;
            return resolve({
                .input_id = snapshot.input_id,
                .operation = game_client::interaction_operation::fill,
                .target_id = snapshot.field->id,
                .value = "__no_advanced_inventory_match__",
                .submit = true,
            });
        }
        if (step == 0) {
            const auto inventory = find_choice_type(snapshot, "test_1kg_cube", "source");
            REQUIRE(inventory != snapshot.choices.end());
            removed_id = inventory->id;
            old_input_id = snapshot.input_id;
            step = 1;
            return resolve_action("FILTER");
        }
        REQUIRE(step == 2);
        const auto source =
            std::ranges::find(snapshot.panes, "source", &game_client::interaction_pane::role);
        REQUIRE(source != snapshot.panes.end());
        CHECK(source->filter == "__no_advanced_inventory_match__");
        CHECK(find_choice_type(snapshot, "test_platinum_bit", "destination")
              != snapshot.choices.end());
        CHECK(std::ranges::none_of(snapshot.choices, [&](const auto& choice) {
            return choice.id == removed_id;
        }));
        const auto removed = game_client::resolve_interaction_command({
            .input_id = snapshot.input_id,
            .operation = game_client::interaction_operation::choose,
            .target_id = removed_id,
        });
        REQUIRE_FALSE(removed);
        CHECK(removed.error().starts_with("invalid:"));
        const auto stale = game_client::resolve_interaction_command({
            .input_id = old_input_id,
            .operation = game_client::interaction_operation::choose,
            .target_id = removed_id,
        });
        REQUIRE_FALSE(stale);
        CHECK(stale.error().starts_with("stale:"));
        step = 3;
        return resolve({
            .input_id = snapshot.input_id,
            .operation = game_client::interaction_operation::cancel,
        });
    });

    create_advanced_inv();
    CHECK(step == 3);
    CHECK(you.amount_of(itype_id("test_1kg_cube")) == 1);
    CHECK(ground_charges(itype_id("test_platinum_bit")) == 10);
    CHECK_FALSE(you.activity);
}

TEST_CASE(
    "advanced inventory native variable moves transfer exact stacks and charges",
    "[client][interaction][advanced_inventory][replay][mcp]") {
    const auto guard = interaction_test_guard{};

    SECTION("two of three inventory items move to center ground") {
        const auto fixture = advanced_inventory_fixture_guard{};
        auto& you = get_avatar();
        std::ranges::for_each(std::views::iota(0, 3), [&](const auto /*index*/) {
            you.i_add(item::spawn("test_1kg_cube", calendar::turn));
        });
        auto step = 0;
        game_client::memory::set_input_provider([&](const int /*timeout*/) {
            const auto snapshot = game_client::current_interaction({.limit = 200});
            if (snapshot.context != "ADVANCED_INVENTORY") {
                REQUIRE(step == 2);
                REQUIRE(snapshot.kind == game_client::interaction_kind::field);
                step = 3;
                return resolve({
                    .input_id = snapshot.input_id,
                    .operation = game_client::interaction_operation::fill,
                    .target_id = snapshot.field->id,
                    .value = "2",
                    .submit = true,
                });
            }
            if (step == 0) {
                const auto target = find_choice_type(snapshot, "test_1kg_cube", "source");
                REQUIRE(target != snapshot.choices.end());
                step = 1;
                return resolve({
                    .input_id = snapshot.input_id,
                    .operation = game_client::interaction_operation::choose,
                    .target_id = target->id,
                });
            }
            if (step == 1) {
                CHECK(
                    std::ranges::
                        count(snapshot.choices, true, &game_client::interaction_choice::highlighted)
                    == 1);
                step = 2;
                return resolve_action("MOVE_VARIABLE_ITEM");
            }
            REQUIRE(step == 3);
            step = 4;
            return resolve({
                .input_id = snapshot.input_id,
                .operation = game_client::interaction_operation::cancel,
            });
        });

        create_advanced_inv();
        REQUIRE(you.activity);
        CHECK(you.activity->id() == activity_id("ACT_DROP"));
        process_activity(you);
        CHECK(step == 4);
        CHECK_FALSE(you.activity);
        CHECK(you.backlog.empty());
        CHECK(you.amount_of(itype_id("test_1kg_cube")) == 1);
        CHECK(ground_item_count(itype_id("test_1kg_cube")) == 2);
    }

    SECTION("three of ten ground charges move to inventory") {
        const auto fixture = advanced_inventory_fixture_guard{};
        auto& you = get_avatar();
        auto charges = item::spawn("test_platinum_bit", calendar::turn, 10);
        get_map().add_item(advanced_inventory_test_pos, std::move(charges));
        auto step = 0;
        game_client::memory::set_input_provider([&](const int /*timeout*/) {
            const auto snapshot = game_client::current_interaction({.limit = 200});
            if (snapshot.context != "ADVANCED_INVENTORY") {
                REQUIRE(step == 2);
                REQUIRE(snapshot.kind == game_client::interaction_kind::field);
                step = 3;
                return resolve({
                    .input_id = snapshot.input_id,
                    .operation = game_client::interaction_operation::fill,
                    .target_id = snapshot.field->id,
                    .value = "3",
                    .submit = true,
                });
            }
            if (step == 0) {
                const auto target = find_choice_type(snapshot, "test_platinum_bit", "destination");
                REQUIRE(target != snapshot.choices.end());
                step = 1;
                return resolve({
                    .input_id = snapshot.input_id,
                    .operation = game_client::interaction_operation::choose,
                    .target_id = target->id,
                });
            }
            if (step == 1) {
                const auto source = std::ranges::
                    find(snapshot.panes, "source", &game_client::interaction_pane::role);
                REQUIRE(source != snapshot.panes.end());
                const auto target = find_choice_type(snapshot, "test_platinum_bit", "source");
                REQUIRE(target != snapshot.choices.end());
                CHECK(target->area_id == source->area_id);
                step = 2;
                return resolve_action("MOVE_VARIABLE_ITEM");
            }
            REQUIRE(step == 3);
            step = 4;
            return resolve({
                .input_id = snapshot.input_id,
                .operation = game_client::interaction_operation::cancel,
            });
        });

        create_advanced_inv();
        REQUIRE(you.activity);
        CHECK(you.activity->id() == activity_id("ACT_PICKUP"));
        process_activity(you);
        CHECK(step == 4);
        CHECK_FALSE(you.activity);
        CHECK(you.backlog.empty());
        CHECK(you.charges_of(itype_id("test_platinum_bit")) == 3);
        CHECK(ground_charges(itype_id("test_platinum_bit")) == 7);
    }
}

TEST_CASE(
    "advanced inventory quantity and screen cancellation preserve live state",
    "[client][interaction][advanced_inventory][mcp]") {
    const auto guard = interaction_test_guard{};
    const auto fixture = advanced_inventory_fixture_guard{};
    auto& you = get_avatar();
    std::ranges::for_each(std::views::iota(0, 3), [&](const auto /*index*/) {
        you.i_add(item::spawn("test_1kg_cube", calendar::turn));
    });
    auto step = 0;
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto snapshot = game_client::current_interaction({.limit = 200});
        if (snapshot.context != "ADVANCED_INVENTORY") {
            REQUIRE(step == 2);
            step = 3;
            return resolve({
                .input_id = snapshot.input_id,
                .operation = game_client::interaction_operation::cancel,
            });
        }
        if (step == 0) {
            const auto target = find_choice_type(snapshot, "test_1kg_cube", "source");
            REQUIRE(target != snapshot.choices.end());
            step = 1;
            return resolve({
                .input_id = snapshot.input_id,
                .operation = game_client::interaction_operation::choose,
                .target_id = target->id,
            });
        }
        if (step == 1) {
            step = 2;
            return resolve_action("MOVE_VARIABLE_ITEM");
        }
        REQUIRE(step == 3);
        step = 4;
        return resolve({
            .input_id = snapshot.input_id,
            .operation = game_client::interaction_operation::cancel,
        });
    });

    create_advanced_inv();
    CHECK(step == 4);
    CHECK_FALSE(you.activity);
    CHECK(you.backlog.empty());
    CHECK(you.amount_of(itype_id("test_1kg_cube")) == 3);
    CHECK(ground_item_count(itype_id("test_1kg_cube")) == 0);
}

TEST_CASE(
    "advanced inventory preserves native overweight and loose-liquid denials",
    "[client][interaction][advanced_inventory][mcp]") {
    const auto guard = interaction_test_guard{};

    const auto run_denial = [&](const itype_id& type, const int charges) {
        const auto fixture = advanced_inventory_fixture_guard{};
        auto& you = get_avatar();
        auto candidate = item::spawn(type, calendar::turn, charges);
        get_map().add_item(advanced_inventory_test_pos, std::move(candidate));
        auto step = 0;
        game_client::memory::set_input_provider([&](const int /*timeout*/) {
            const auto snapshot = game_client::current_interaction({.limit = 200});
            if (snapshot.context == "POPUP_WAIT") {
                REQUIRE(step == 2);
                CHECK_FALSE(snapshot.message.empty());
                REQUIRE(snapshot.choices.size() == 1);
                step = 3;
                return resolve({
                    .input_id = snapshot.input_id,
                    .operation = game_client::interaction_operation::choose,
                    .target_id = snapshot.choices.front().id,
                });
            }
            REQUIRE(snapshot.context == "ADVANCED_INVENTORY");
            if (step == 0) {
                const auto target = find_choice_type(snapshot, type.str(), "destination");
                REQUIRE(target != snapshot.choices.end());
                step = 1;
                return resolve({
                    .input_id = snapshot.input_id,
                    .operation = game_client::interaction_operation::choose,
                    .target_id = target->id,
                });
            }
            if (step == 1) {
                step = 2;
                return resolve_action("MOVE_SINGLE_ITEM");
            }
            REQUIRE(step == 3);
            step = 4;
            return resolve({
                .input_id = snapshot.input_id,
                .operation = game_client::interaction_operation::cancel,
            });
        });

        create_advanced_inv();
        CHECK(step == 4);
        CHECK_FALSE(you.activity);
        CHECK(you.backlog.empty());
        CHECK(you.amount_of(type) == 0);
        CHECK(ground_item_count(type) == 1);
    };

    SECTION("overweight destination") { run_denial(itype_id("test_overweight_cube"), -1); }
    SECTION("loose liquid") { run_denial(itype_id("water_clean"), 10); }
}

TEST_CASE(
    "advanced inventory native container transfer rejects incompatible liquids unchanged",
    "[client][interaction][advanced_inventory][mcp]") {
    const auto guard = interaction_test_guard{};
    const auto fixture = advanced_inventory_fixture_guard{};
    auto& you = get_avatar();
    auto destination_owner = item::spawn("test_jug_plastic", calendar::turn);
    destination_owner->fill_with(item::spawn("bleach", calendar::turn, 5), 5);
    auto& destination = you.i_add(std::move(destination_owner));
    auto source_owner = item::spawn("test_waterskin", calendar::turn);
    source_owner->fill_with(item::spawn("water_clean", calendar::turn, 10), 10);
    auto& source = you.i_add(std::move(source_owner));
    REQUIRE_FALSE(destination.is_container_empty());
    REQUIRE_FALSE(source.is_container_empty());
    const auto destination_charges = destination.contents.front().charges;
    const auto source_charges = source.contents.front().charges;
    uistate.transfer_save.saved_area_right = AIM_INVENTORY;
    uistate.transfer_save.pane_right.area_idx = AIM_INVENTORY;
    auto step = 0;

    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto snapshot = game_client::current_interaction({.limit = 200});
        if (snapshot.context == "POPUP_WAIT") {
            REQUIRE(step == 4);
            CHECK_FALSE(snapshot.message.empty());
            REQUIRE(snapshot.choices.size() == 1);
            step = 5;
            return resolve({
                .input_id = snapshot.input_id,
                .operation = game_client::interaction_operation::choose,
                .target_id = snapshot.choices.front().id,
            });
        }
        REQUIRE(snapshot.context == "ADVANCED_INVENTORY");
        if (step == 0) {
            const auto target = find_choice_type(snapshot, "test_jug_plastic", "source");
            REQUIRE(target != snapshot.choices.end());
            step = 1;
            return resolve({
                .input_id = snapshot.input_id,
                .operation = game_client::interaction_operation::choose,
                .target_id = target->id,
            });
        }
        if (step == 1) {
            step = 2;
            return resolve_action("ITEMS_CONTAINER");
        }
        if (step == 2) {
            const auto target = find_choice_type(snapshot, "test_waterskin", "destination");
            REQUIRE(target != snapshot.choices.end());
            step = 3;
            return resolve({
                .input_id = snapshot.input_id,
                .operation = game_client::interaction_operation::choose,
                .target_id = target->id,
            });
        }
        if (step == 3) {
            step = 4;
            return resolve_action("MOVE_SINGLE_ITEM");
        }
        REQUIRE(step == 5);
        step = 6;
        return resolve({
            .input_id = snapshot.input_id,
            .operation = game_client::interaction_operation::cancel,
        });
    });

    create_advanced_inv();
    CHECK(step == 6);
    CHECK_FALSE(you.activity);
    CHECK(you.backlog.empty());
    REQUIRE_FALSE(destination.is_container_empty());
    REQUIRE_FALSE(source.is_container_empty());
    CHECK(destination.contents.front().typeId() == itype_id("bleach"));
    CHECK(destination.contents.front().charges == destination_charges);
    CHECK(source.contents.front().typeId() == itype_id("water_clean"));
    CHECK(source.contents.front().charges == source_charges);
    CHECK(you.amount_of(itype_id("test_jug_plastic")) == 1);
    CHECK(you.amount_of(itype_id("test_waterskin")) == 1);
}

TEST_CASE(
    "advanced inventory native pickup honors theft refusal without moving owned item",
    "[client][interaction][advanced_inventory][mcp]") {
    const auto guard = interaction_test_guard{};
    const auto force_lowercase = override_option("FORCE_CAPITAL_YN", "false");
    const auto fixture = advanced_inventory_fixture_guard{};
    auto& you = get_avatar();
    g->faction_manager_ptr->create_if_needed();
    auto& owner = spawn_npc(advanced_inventory_test_pos + tripoint_north, "test_talker");
    owner.set_fac(faction_id("free_merchants"));
    auto owned_item = item::spawn("test_1kg_cube", calendar::turn);
    owned_item->set_owner(owner);
    const auto owner_id = owned_item->get_owner();
    get_map().add_item(advanced_inventory_test_pos, std::move(owned_item));
    you.set_value("THIEF_MODE", "THIEF_ASK");
    you.set_value("THIEF_MODE_KEEP", "NO");
    auto step = 0;

    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto snapshot = game_client::current_interaction({.limit = 200});
        if (snapshot.context == "YES_NO_ALWAYS_NEVER") {
            REQUIRE(step == 2);
            CHECK(snapshot.message.find("stealing") != std::string::npos);
            const auto refuse = std::ranges::
                find(snapshot.choices, "NO", &game_client::interaction_choice::description);
            REQUIRE(refuse != snapshot.choices.end());
            step = 3;
            return resolve({
                .input_id = snapshot.input_id,
                .operation = game_client::interaction_operation::choose,
                .target_id = refuse->id,
            });
        }
        REQUIRE(snapshot.context == "ADVANCED_INVENTORY");
        if (step == 0) {
            const auto target = find_choice_type(snapshot, "test_1kg_cube", "destination");
            REQUIRE(target != snapshot.choices.end());
            step = 1;
            return resolve({
                .input_id = snapshot.input_id,
                .operation = game_client::interaction_operation::choose,
                .target_id = target->id,
            });
        }
        if (step == 1) {
            step = 2;
            return resolve_action("MOVE_SINGLE_ITEM");
        }
        REQUIRE(step == 3);
        step = 4;
        return resolve({
            .input_id = snapshot.input_id,
            .operation = game_client::interaction_operation::cancel,
        });
    });

    create_advanced_inv();
    REQUIRE(you.activity);
    CHECK(you.activity->id() == activity_id("ACT_PICKUP"));
    process_activity(you);
    CHECK(step == 4);
    CHECK_FALSE(you.activity);
    CHECK(you.backlog.empty());
    CHECK(you.amount_of(itype_id("test_1kg_cube")) == 0);
    REQUIRE(ground_item_count(itype_id("test_1kg_cube")) == 1);
    auto ground_items = get_map().i_at(advanced_inventory_test_pos);
    const auto ground_item = std::ranges::find_if(ground_items, [](const auto* candidate) {
        return candidate->typeId() == itype_id("test_1kg_cube");
    });
    REQUIRE(ground_item != ground_items.end());
    CHECK((*ground_item)->get_owner() == owner_id);
    CHECK(you.get_value("THIEF_MODE") == "THIEF_ASK");
}

TEST_CASE(
    "semantic replay repeats an advanced inventory native variable transfer",
    "[client][interaction][advanced_inventory][replay][mcp]") {
    const auto guard = interaction_test_guard{};
    static auto counter = std::atomic_uint64_t{0};
    const auto directory =
        std::filesystem::path(PATH_INFO::user_dir())
        / ("semantic-advanced-inventory-" + std::to_string(counter.fetch_add(1)));
    REQUIRE(std::filesystem::create_directory(directory));
    const auto cleanup_replay = on_out_of_scope([&] {
        replay::stop();
        std::filesystem::remove_all(directory);
    });
    const auto path = directory / "input.jsonl";

    {
        const auto fixture = advanced_inventory_fixture_guard{};
        auto& recorded = get_avatar();
        std::ranges::for_each(std::views::iota(0, 3), [&](const auto /*index*/) {
            recorded.i_add(item::spawn("test_1kg_cube", calendar::turn));
        });
        replay::configure_recording(path.string(), {.rng_seed = 37});
        replay::start();
        auto step = 0;
        game_client::memory::set_input_provider([&](const int /*timeout*/) {
            const auto snapshot = game_client::current_interaction({.limit = 200});
            if (snapshot.context != "ADVANCED_INVENTORY") {
                step = 3;
                return resolve({
                    .input_id = snapshot.input_id,
                    .operation = game_client::interaction_operation::fill,
                    .target_id = snapshot.field->id,
                    .value = "2",
                    .submit = true,
                });
            }
            if (step == 0) {
                const auto target = find_choice_type(snapshot, "test_1kg_cube", "source");
                REQUIRE(target != snapshot.choices.end());
                step = 1;
                return resolve({
                    .input_id = snapshot.input_id,
                    .operation = game_client::interaction_operation::choose,
                    .target_id = target->id,
                });
            }
            if (step == 1) {
                step = 2;
                return resolve_action("MOVE_VARIABLE_ITEM");
            }
            REQUIRE(step == 3);
            step = 4;
            return resolve({
                .input_id = snapshot.input_id,
                .operation = game_client::interaction_operation::cancel,
            });
        });
        create_advanced_inv();
        REQUIRE(recorded.activity);
        process_activity(recorded);
        CHECK(step == 4);
        CHECK(recorded.amount_of(itype_id("test_1kg_cube")) == 1);
        CHECK(ground_item_count(itype_id("test_1kg_cube")) == 2);
        replay::finish();
    }

    {
        const auto fixture = advanced_inventory_fixture_guard{};
        auto& played = get_avatar();
        std::ranges::for_each(std::views::iota(0, 3), [&](const auto /*index*/) {
            played.i_add(item::spawn("test_1kg_cube", calendar::turn));
        });
        game_client::memory::set_input_provider([](const int /*timeout*/) -> input_event {
            throw std::runtime_error("advanced inventory playback must not read live input");
        });
        replay::configure_playback(path.string(), {.rng_seed = 37});
        replay::start();
        create_advanced_inv();
        REQUIRE(played.activity);
        CHECK(played.activity->id() == activity_id("ACT_DROP"));
        process_activity(played);
        CHECK_THROWS_AS(replay::next_input_event(), replay::completed);
        CHECK_FALSE(played.activity);
        CHECK(played.backlog.empty());
        CHECK(played.amount_of(itype_id("test_1kg_cube")) == 1);
        CHECK(ground_item_count(itype_id("test_1kg_cube")) == 2);
        replay::finish();
    }

    {
        const auto fixture = advanced_inventory_fixture_guard{};
        auto& changed = get_avatar();
        std::ranges::for_each(std::views::iota(0, 2), [&](const auto /*index*/) {
            changed.i_add(item::spawn("test_1kg_cube", calendar::turn));
        });
        game_client::memory::set_input_provider([](const int /*timeout*/) -> input_event {
            throw std::runtime_error(
                "changed advanced inventory playback must not read live input");
        });
        replay::configure_playback(path.string(), {.rng_seed = 37});
        replay::start();
        CHECK_THROWS_WITH(create_advanced_inv(), Catch::Matchers::Contains("schema changed"));
        CHECK_FALSE(changed.activity);
        CHECK(changed.backlog.empty());
        CHECK(changed.amount_of(itype_id("test_1kg_cube")) == 2);
        CHECK(ground_item_count(itype_id("test_1kg_cube")) == 0);
        replay::stop();
    }
}

TEST_CASE("real uilist exposes filtered effective choices", "[client][interaction][mcp]") {
    const auto guard = interaction_test_guard{};
    auto menu = uilist{};
    menu.title = "Live list";
    menu.allow_disabled = false;
    menu.entries.emplace_back(10, true, MENU_AUTOASSIGN, "show enabled", "first description");
    menu.entries.emplace_back(20, false, MENU_AUTOASSIGN, "show disabled", "blocked");
    menu.entries.emplace_back(30, true, MENU_AUTOASSIGN, "hidden", "filtered");
    menu.set_filter("show");
    auto callback = counting_callback{};
    menu.callback = &callback;
    auto observed = game_client::interaction_snapshot{};
    auto stale_error = std::string{};
    auto disabled_error = std::string{};
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        observed = game_client::current_interaction();
        REQUIRE(observed.structured);
        REQUIRE(observed.choices.size() == 2);
        const auto disabled = std::ranges::
            find(observed.choices, false, &game_client::interaction_choice::selectable);
        REQUIRE(disabled != observed.choices.end());
        const auto disabled_result = game_client::resolve_interaction_command({
            .input_id = observed.input_id,
            .operation = game_client::interaction_operation::choose,
            .target_id = disabled->id,
        });
        REQUIRE_FALSE(disabled_result);
        disabled_error = disabled_result.error();
        const auto stale = game_client::resolve_interaction_command({
            .input_id = observed.input_id + 1,
            .operation = game_client::interaction_operation::choose,
            .target_id = observed.choices.front().id,
        });
        REQUIRE_FALSE(stale);
        stale_error = stale.error();
        return resolve({
            .input_id = observed.input_id,
            .operation = game_client::interaction_operation::choose,
            .target_id = observed.choices.front().id,
        });
    });

    menu.query();

    CHECK(menu.ret == 10);
    CHECK(observed.choices.front().description == "first description");
    CHECK(disabled_error.starts_with("disabled:"));
    CHECK(stale_error.starts_with("stale:"));
    CHECK(callback.selections > 0);
}

TEST_CASE(
    "real query popup supports semantic choice acknowledgement and cancel",
    "[client][interaction][mcp]") {
    const auto guard = interaction_test_guard{};
    SECTION("option") {
        game_client::memory::set_input_provider([&](const int /*timeout*/) {
            const auto snapshot = game_client::current_interaction();
            REQUIRE(snapshot.choices.size() == 2);
            const auto choice = std::ranges::
                find(snapshot.choices, "YES", &game_client::interaction_choice::description);
            REQUIRE(choice != snapshot.choices.end());
            return resolve({
                .input_id = snapshot.input_id,
                .operation = game_client::interaction_operation::choose,
                .target_id = choice->id,
            });
        });
        const auto result =
            query_popup()
                .context("YESNO")
                .message("%s", "Continue?")
                .option("YES")
                .option("NO")
                .query();
        CHECK(result.action == "YES");
    }
    SECTION("acknowledgement") {
        game_client::memory::set_input_provider([&](const int /*timeout*/) {
            const auto snapshot = game_client::current_interaction();
            REQUIRE(snapshot.choices.size() == 1);
            CHECK(snapshot.choices.front().description == "ANY_INPUT");
            CHECK(snapshot.choices.front().label == "Press any key");
            return resolve({
                .input_id = snapshot.input_id,
                .operation = game_client::interaction_operation::choose,
                .target_id = snapshot.choices.front().id,
            });
        });
        const auto result = query_popup().message("%s", "Notice").allow_anykey(true).query();
        CHECK(result.action == "ANY_INPUT");
    }
    SECTION("cancel") {
        game_client::memory::set_input_provider([&](const int /*timeout*/) {
            const auto snapshot = game_client::current_interaction();
            REQUIRE(snapshot.allow_cancel);
            return resolve({
                .input_id = snapshot.input_id,
                .operation = game_client::interaction_operation::cancel,
            });
        });
        const auto result =
            query_popup()
                .context("YESNO")
                .message("%s", "Cancel?")
                .option("YES")
                .allow_cancel(true)
                .query();
        CHECK(result.action == "QUIT");
    }
}

TEST_CASE(
    "real string popup validates replacement fields and submit state",
    "[client][interaction][mcp]") {
    const auto guard = interaction_test_guard{};
    auto reads = 0;
    auto numeric_error = std::string{};
    auto length_error = std::string{};
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto snapshot = game_client::current_interaction();
        REQUIRE(snapshot.field);
        CHECK(snapshot.field->type == "integer");
        CHECK(snapshot.field->max_length == 4);
        if (reads++ == 0) {
            const auto invalid_numeric = game_client::resolve_interaction_command({
                .input_id = snapshot.input_id,
                .operation = game_client::interaction_operation::fill,
                .target_id = snapshot.field->id,
                .value = "1x",
                .submit = true,
            });
            REQUIRE_FALSE(invalid_numeric);
            numeric_error = invalid_numeric.error();
            const auto invalid_length = game_client::resolve_interaction_command({
                .input_id = snapshot.input_id,
                .operation = game_client::interaction_operation::fill,
                .target_id = snapshot.field->id,
                .value = "12345",
                .submit = true,
            });
            REQUIRE_FALSE(invalid_length);
            length_error = invalid_length.error();
            for (const auto* wide : {"\xED\x95\x9C", "\xEF\xBC\x91"}) {
                const auto invalid_unicode = game_client::resolve_interaction_command({
                    .input_id = snapshot.input_id,
                    .operation = game_client::interaction_operation::fill,
                    .target_id = snapshot.field->id,
                    .value = wide,
                    .submit = true,
                });
                REQUIRE_FALSE(invalid_unicode);
                CHECK(invalid_unicode.error().starts_with("invalid:"));
            }
            return resolve({
                .input_id = snapshot.input_id,
                .operation = game_client::interaction_operation::fill,
                .target_id = snapshot.field->id,
                .value = "-12",
                .submit = false,
            });
        }
        CHECK(snapshot.field->value == "-12");
        return resolve({
            .input_id = snapshot.input_id,
            .operation = game_client::interaction_operation::fill,
            .target_id = snapshot.field->id,
            .value = "42",
            .submit = true,
        });
    });
    auto popup = string_input_popup{};
    const auto value =
        popup.title("Number").text("0").max_length(4).only_digits(true).query_string();
    CHECK(value == "42");
    CHECK(popup.confirmed());
    CHECK_FALSE(popup.canceled());
    CHECK(reads == 2);
    CHECK(numeric_error.starts_with("invalid:"));
    CHECK(length_error.starts_with("invalid:"));

    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto snapshot = game_client::current_interaction();
        return resolve({
            .input_id = snapshot.input_id,
            .operation = game_client::interaction_operation::cancel,
        });
    });
    auto canceled = string_input_popup{};
    CHECK(canceled.text("unchanged").query_string().empty());
    CHECK(canceled.canceled());
}

TEST_CASE(
    "real inventory picker exposes and selects live avatar items",
    "[client][interaction][inventory][mcp]") {
    clear_all_state();
    const auto cleanup = on_out_of_scope([]() { clear_all_state(); });
    const auto guard = interaction_test_guard{};
    auto& you = get_avatar();
    auto& expected = you.i_add(item::spawn("test_platinum_bit", calendar::turn, 7));
    auto selector = inventory_pick_selector(you);
    selector.add_character_items(you);
    auto observed = game_client::interaction_snapshot{};
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        observed = game_client::current_interaction();
        REQUIRE(observed.kind == game_client::interaction_kind::inventory);
        REQUIRE(observed.choices.size() == 1);
        const auto& choice = observed.choices.front();
        CHECK(choice.selectable);
        CHECK_FALSE(choice.selected);
        CHECK(choice.highlighted);
        CHECK(choice.selected_count == 0);
        CHECK(choice.available_count == 7);
        REQUIRE_FALSE(choice.columns.empty());
        CHECK(choice.columns.front().value == "inventory");
        CHECK(choice.description.size() > choice.label.size());
        return resolve({
            .input_id = observed.input_id,
            .operation = game_client::interaction_operation::choose,
            .target_id = choice.id,
        });
    });

    CHECK(selector.execute() == &expected);
    CHECK_FALSE(observed.choices.front().id.contains("0x"));
}

TEST_CASE(
    "real inventory multiselect accepts exact quantity and zero",
    "[client][interaction][inventory][mcp]") {
    clear_all_state();
    const auto cleanup = on_out_of_scope([]() { clear_all_state(); });
    const auto guard = interaction_test_guard{};
    auto& you = get_avatar();
    you.i_add(item::spawn("test_platinum_bit", calendar::turn, 7));
    auto selector = inventory_drop_selector(you);
    selector.add_character_items(you);
    auto reads = 0;
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto snapshot = game_client::current_interaction();
        REQUIRE(snapshot.choices.size() == 1);
        const auto& choice = snapshot.choices.front();
        REQUIRE(snapshot.allow_set_count);
        if (reads++ == 0) {
            CHECK(choice.selected_count == 0);
            return resolve({
                .input_id = snapshot.input_id,
                .operation = game_client::interaction_operation::set_count,
                .target_id = choice.id,
                .count = 0,
            });
        }
        if (reads == 2) {
            CHECK(choice.selected_count == 0);
            const auto too_many = game_client::resolve_interaction_command({
                .input_id = snapshot.input_id,
                .operation = game_client::interaction_operation::set_count,
                .target_id = choice.id,
                .count = 8,
            });
            REQUIRE_FALSE(too_many);
            CHECK(too_many.error().starts_with("invalid:"));
            return resolve({
                .input_id = snapshot.input_id,
                .operation = game_client::interaction_operation::set_count,
                .target_id = choice.id,
                .count = 3,
            });
        }
        CHECK(choice.selected_count == 3);
        return resolve_action("CONFIRM");
    });

    const auto dropped = selector.execute();
    REQUIRE(dropped.size() == 1);
    CHECK(dropped.front().count == 3);
}

TEST_CASE(
    "inventory selection and navigation highlights remain independent",
    "[client][interaction][inventory][mcp]") {
    clear_all_state();
    const auto cleanup = on_out_of_scope([]() { clear_all_state(); });
    const auto guard = interaction_test_guard{};
    auto& you = get_avatar();
    auto items = std::vector<detached_ptr<item>>{};
    for (const auto index : std::views::iota(0, 3)) {
        items.push_back(item::spawn("test_platinum_bit", calendar::turn, index + 1));
    }
    auto selector = inventory_drop_selector(you);
    for (const auto& candidate : items) {
        selector.add_item(selector.own_inv_column, candidate.get());
    }
    auto reads = 0;
    auto selected_id = std::string{};
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto snapshot = game_client::current_interaction();
        REQUIRE(snapshot.choices.size() == 3);
        if (reads++ == 0) {
            const auto highlighted = std::ranges::
                find(snapshot.choices, true, &game_client::interaction_choice::highlighted);
            REQUIRE(highlighted != snapshot.choices.end());
            CHECK_FALSE(highlighted->selected);
            CHECK(highlighted->selected_count == 0);
            selected_id = highlighted->id;
            return resolve({
                .input_id = snapshot.input_id,
                .operation = game_client::interaction_operation::set_count,
                .target_id = selected_id,
                .count = 1,
            });
        }
        const auto selected =
            std::ranges::find(snapshot.choices, selected_id, &game_client::interaction_choice::id);
        REQUIRE(selected != snapshot.choices.end());
        if (reads == 2) {
            CHECK(selected->selected);
            CHECK(selected->highlighted);
            CHECK(selected->selected_count == 1);
            auto moved = false;
            for (const auto& candidate : items) {
                REQUIRE(selector.select(candidate.get()));
                const auto updated = game_client::current_interaction();
                const auto original = std::ranges::
                    find(updated.choices, selected_id, &game_client::interaction_choice::id);
                REQUIRE(original != updated.choices.end());
                if (!original->highlighted) {
                    moved = true;
                    break;
                }
            }
            REQUIRE(moved);
            const auto moved_snapshot = game_client::current_interaction();
            const auto persisted = std::ranges::
                find(moved_snapshot.choices, selected_id, &game_client::interaction_choice::id);
            REQUIRE(persisted != moved_snapshot.choices.end());
            CHECK(persisted->selected);
            CHECK_FALSE(persisted->highlighted);
            CHECK(persisted->selected_count == 1);
            CHECK(std::ranges::count(
                      moved_snapshot.choices, true, &game_client::interaction_choice::highlighted)
                  == 1);
            return resolve({
                .input_id = moved_snapshot.input_id,
                .operation = game_client::interaction_operation::set_count,
                .target_id = selected_id,
                .count = 0,
            });
        }
        if (reads == 3) {
            CHECK_FALSE(selected->selected);
            CHECK(selected->highlighted);
            CHECK(selected->selected_count == 0);
            return resolve_action("CATEGORY_SELECTION");
        }
        CHECK(std::ranges::none_of(snapshot.choices, &game_client::interaction_choice::selected));
        CHECK(
            std::ranges::count(snapshot.choices, true, &game_client::interaction_choice::highlighted)
            == 3);
        return resolve({
            .input_id = snapshot.input_id,
            .operation = game_client::interaction_operation::cancel,
        });
    });

    CHECK(selector.execute().empty());
    CHECK(reads == 4);
}

TEST_CASE(
    "bounded interaction pages control late real inventory entries and reset safely",
    "[client][interaction][inventory][mcp]") {
    clear_all_state();
    const auto cleanup = on_out_of_scope([]() { clear_all_state(); });
    const auto guard = interaction_test_guard{};
    auto& you = get_avatar();
    auto items = std::vector<item*>{};
    items.reserve(102);
    for (const auto index : std::views::iota(0, 101)) {
        auto candidate = item::spawn("test_platinum_bit", calendar::turn, index == 100 ? 7 : 1);
        candidate->set_var("interaction_paging_index", index);
        items.push_back(&you.i_add(std::move(candidate), false));
    }
    items.push_back(&you.i_add(item::spawn("bandages"), false));

    const auto preset = denied_inventory_preset{};
    auto selector = inventory_drop_selector(you, preset);
    selector.add_character_items(you);
    auto page = game_client::interaction_page_state{};
    auto selected_id = std::string{};
    auto reads = 0;
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        auto snapshot = game_client::paginated_interaction(page);
        if (reads++ == 0) {
            CHECK(snapshot.choice_offset == 0);
            CHECK(snapshot.choices.size() == 100);
            CHECK(snapshot.choice_total == 102);
            const auto full = game_client::current_interaction({.limit = 200});
            const auto disabled = std::ranges::
                find(full.choices, false, &game_client::interaction_choice::selectable);
            REQUIRE(disabled != full.choices.end());
            const auto disabled_result = game_client::resolve_interaction_command({
                .input_id = snapshot.input_id,
                .operation = game_client::interaction_operation::set_count,
                .target_id = disabled->id,
                .count = 1,
            });
            REQUIRE_FALSE(disabled_result);
            CHECK(disabled_result.error().starts_with("disabled:"));
            const auto stale_result = game_client::resolve_interaction_command({
                .input_id = snapshot.input_id + 1,
                .operation = game_client::interaction_operation::set_count,
                .target_id = full.choices.front().id,
                .count = 1,
            });
            REQUIRE_FALSE(stale_result);
            CHECK(stale_result.error().starts_with("stale:"));
            CHECK(std::ranges::none_of(
                game_client::current_interaction({.limit = 200}).choices,
                &game_client::interaction_choice::selected));

            page.offset = 100;
            snapshot = game_client::paginated_interaction(page);
            CHECK(snapshot.choice_offset == 100);
            REQUIRE(snapshot.choices.size() == 2);
            const auto late = std::ranges::
                find(snapshot.choices, true, &game_client::interaction_choice::selectable);
            REQUIRE(late != snapshot.choices.end());
            selected_id = late->id;
            return resolve({
                .input_id = snapshot.input_id,
                .operation = game_client::interaction_operation::set_count,
                .target_id = selected_id,
                .count = 1,
            });
        }

        const auto selected =
            std::ranges::find(snapshot.choices, selected_id, &game_client::interaction_choice::id);
        REQUIRE(selected != snapshot.choices.end());
        CHECK(snapshot.choice_offset == 100);
        if (reads == 2) {
            CHECK(selected->selected);
            CHECK(selected->selected_count == 1);
            return resolve({
                .input_id = snapshot.input_id,
                .operation = game_client::interaction_operation::set_count,
                .target_id = selected_id,
                .count = 0,
            });
        }

        CHECK_FALSE(selected->selected);
        CHECK(selected->selected_count == 0);
        selector.set_filter("__interaction_paging_no_match__");
        const auto filtered = game_client::paginated_interaction(page);
        CHECK(filtered.choice_offset == 0);
        CHECK(filtered.choice_total == 0);
        CHECK(filtered.choices.empty());
        const auto removed = game_client::resolve_interaction_command({
            .input_id = snapshot.input_id,
            .operation = game_client::interaction_operation::set_count,
            .target_id = selected_id,
            .count = 1,
        });
        REQUIRE_FALSE(removed);
        CHECK(removed.error().starts_with("invalid:"));

        selector.set_filter("");
        const auto restored = game_client::paginated_interaction(page);
        CHECK(restored.choice_offset == 0);
        CHECK(restored.choice_total == 102);
        selector.clear_items();
        selector.add_item(selector.own_inv_column, items.back());
        const auto shrunk = game_client::paginated_interaction(page);
        CHECK(shrunk.choice_offset == 0);
        CHECK(shrunk.choice_total == 1);
        CHECK(shrunk.choices.size() == 1);
        selector.add_item(selector.own_inv_column, items.front());
        selector.set_filter(selector.get_filter());
        const auto reordered = game_client::paginated_interaction(page);
        CHECK(reordered.choice_offset == 0);
        CHECK(reordered.choice_total == 2);
        CHECK(reordered.choices.size() == 2);
        return resolve({
            .input_id = reordered.input_id,
            .operation = game_client::interaction_operation::cancel,
        });
    });

    CHECK(selector.execute().empty());
    CHECK(reads == 3);
}

TEST_CASE(
    "semantic ground pickup transfers exact noncharge and charge quantities",
    "[client][interaction][pickup][mcp]") {
    const auto guard = interaction_test_guard{};

    SECTION("two of three identical items") {
        const auto fixture = pickup_fixture_guard{};
        const auto id = itype_id("test_screwdriver");
        std::ranges::for_each(std::views::iota(0, 3), [&](const auto /*index*/) {
            add_ground_item(id);
        });
        auto reads = 0;
        game_client::memory::set_input_provider([&](const int /*timeout*/) {
            const auto snapshot = game_client::current_interaction();
            REQUIRE(snapshot.context == "PICKUP");
            REQUIRE(snapshot.kind == game_client::interaction_kind::inventory);
            REQUIRE(snapshot.choices.size() == 1);
            const auto& choice = snapshot.choices.front();
            CHECK(choice.available_count == 3);
            CHECK(choice.minimum_count == 0);
            CHECK(choice.columns[0].value == id.str());
            if (reads++ == 0) {
                return resolve({
                    .input_id = snapshot.input_id,
                    .operation = game_client::interaction_operation::set_count,
                    .target_id = choice.id,
                    .count = 2,
                });
            }
            CHECK(choice.selected);
            CHECK(choice.selected_count == 2);
            return resolve_action("CONFIRM");
        });

        pickup::pick_up(pickup_test_pos, 0, pickup::from_ground);
        finish_pickup_activity(get_avatar());

        CHECK(get_avatar().amount_of(id) == 2);
        CHECK(ground_item_count(id) == 1);
    }

    SECTION("three of ten charges") {
        const auto fixture = pickup_fixture_guard{};
        const auto id = itype_id("test_platinum_bit");
        add_ground_item(id, 10);
        auto reads = 0;
        game_client::memory::set_input_provider([&](const int /*timeout*/) {
            const auto snapshot = game_client::current_interaction();
            REQUIRE(snapshot.choices.size() == 1);
            const auto& choice = snapshot.choices.front();
            CHECK(choice.available_count == 10);
            if (reads++ == 0) {
                return resolve({
                    .input_id = snapshot.input_id,
                    .operation = game_client::interaction_operation::set_count,
                    .target_id = choice.id,
                    .count = 3,
                });
            }
            CHECK(choice.selected_count == 3);
            return resolve_action("CONFIRM");
        });

        pickup::pick_up(pickup_test_pos, 0, pickup::from_ground);
        finish_pickup_activity(get_avatar());

        CHECK(get_avatar().charges_of(id) == 3);
        CHECK(ground_charges(id) == 7);
    }
}

TEST_CASE(
    "semantic ground pickup preserves whole zero filter and stale validation",
    "[client][interaction][pickup][mcp]") {
    const auto guard = interaction_test_guard{};

    SECTION("available count normalizes to native whole stack") {
        const auto fixture = pickup_fixture_guard{};
        const auto id = itype_id("test_screwdriver");
        std::ranges::for_each(std::views::iota(0, 2), [&](const auto /*index*/) {
            add_ground_item(id);
        });
        auto reads = 0;
        game_client::memory::set_input_provider([&](const int /*timeout*/) {
            const auto snapshot = game_client::current_interaction();
            if (reads++ == 0) {
                const auto too_many = game_client::resolve_interaction_command({
                    .input_id = snapshot.input_id,
                    .operation = game_client::interaction_operation::set_count,
                    .target_id = snapshot.choices.front().id,
                    .count = 3,
                });
                REQUIRE_FALSE(too_many);
                return resolve({
                    .input_id = snapshot.input_id,
                    .operation = game_client::interaction_operation::set_count,
                    .target_id = snapshot.choices.front().id,
                    .count = 2,
                });
            }
            CHECK(snapshot.choices.front().selected_count == 2);
            return resolve_action("CONFIRM");
        });
        pickup::pick_up(pickup_test_pos, 0, pickup::from_ground);
        finish_pickup_activity(get_avatar());
        CHECK(get_avatar().amount_of(id) == 2);
        CHECK(ground_item_count(id) == 0);
    }

    SECTION("zero unselects and cancel changes neither world nor activity") {
        const auto fixture = pickup_fixture_guard{};
        const auto id = itype_id("test_screwdriver");
        add_ground_item(id);
        auto reads = 0;
        game_client::memory::set_input_provider([&](const int /*timeout*/) {
            const auto snapshot = game_client::current_interaction();
            const auto& choice = snapshot.choices.front();
            if (reads == 0) {
                ++reads;
                return resolve({
                    .input_id = snapshot.input_id,
                    .operation = game_client::interaction_operation::choose,
                    .target_id = choice.id,
                });
            }
            if (reads == 1) {
                ++reads;
                CHECK(choice.selected);
                return resolve({
                    .input_id = snapshot.input_id,
                    .operation = game_client::interaction_operation::set_count,
                    .target_id = choice.id,
                    .count = 0,
                });
            }
            CHECK_FALSE(choice.selected);
            CHECK(choice.selected_count == 0);
            return resolve({
                .input_id = snapshot.input_id,
                .operation = game_client::interaction_operation::cancel,
            });
        });
        pickup::pick_up(pickup_test_pos, 0, pickup::from_ground);
        CHECK_FALSE(get_avatar().activity);
        CHECK(get_avatar().amount_of(id) == 0);
        CHECK(ground_item_count(id) == 1);
    }

    SECTION("filter hides but preserves an existing selection through native confirmation") {
        const auto fixture = pickup_fixture_guard{};
        const auto hidden_id = itype_id("test_screwdriver");
        const auto visible_id = itype_id("hammer");
        add_ground_item(hidden_id);
        add_ground_item(visible_id);
        auto hidden_choice_id = std::string{};
        auto step = 0;
        game_client::memory::set_input_provider([&](const int /*timeout*/) {
            const auto snapshot = game_client::current_interaction();
            if (snapshot.kind == game_client::interaction_kind::field) {
                return resolve({
                    .input_id = snapshot.input_id,
                    .operation = game_client::interaction_operation::fill,
                    .target_id = snapshot.field->id,
                    .value = "hammer",
                    .submit = true,
                });
            }
            REQUIRE(snapshot.context == "PICKUP");
            if (step == 0) {
                const auto hidden = std::ranges::find_if(snapshot.choices, [&](const auto& choice) {
                    return choice.columns.front().value == hidden_id.str();
                });
                REQUIRE(hidden != snapshot.choices.end());
                hidden_choice_id = hidden->id;
                ++step;
                return resolve({
                    .input_id = snapshot.input_id,
                    .operation = game_client::interaction_operation::choose,
                    .target_id = hidden->id,
                });
            }
            if (step == 1) {
                CHECK(std::ranges::any_of(snapshot.choices, [&](const auto& choice) {
                    return choice.id == hidden_choice_id && choice.selected;
                }));
                ++step;
                return resolve_action("FILTER");
            }
            REQUIRE(snapshot.choices.size() == 1);
            const auto& visible = snapshot.choices.front();
            CHECK(visible.columns.front().value == visible_id.str());
            CHECK(visible.id != hidden_choice_id);
            CHECK(visible.label.find("screwdriver") == std::string::npos);
            CHECK(visible.description.find("screwdriver") == std::string::npos);
            const auto stale_hidden = game_client::resolve_interaction_command({
                .input_id = snapshot.input_id,
                .operation = game_client::interaction_operation::choose,
                .target_id = hidden_choice_id,
            });
            REQUIRE_FALSE(stale_hidden);
            if (step == 2) {
                ++step;
                return resolve({
                    .input_id = snapshot.input_id,
                    .operation = game_client::interaction_operation::set_count,
                    .target_id = visible.id,
                    .count = 1,
                });
            }
            CHECK(visible.selected);
            CHECK(visible.selected_count == 1);
            return resolve_action("CONFIRM");
        });
        pickup::pick_up(pickup_test_pos, 0, pickup::from_ground);
        finish_pickup_activity(get_avatar());
        CHECK(get_avatar().amount_of(hidden_id) == 1);
        CHECK(get_avatar().amount_of(visible_id) == 1);
        CHECK(ground_item_count(hidden_id) == 0);
        CHECK(ground_item_count(visible_id) == 0);
    }

    SECTION("filter only publishes matches and rejects the removed opaque id") {
        const auto fixture = pickup_fixture_guard{};
        add_ground_item(itype_id("test_screwdriver"));
        add_ground_item(itype_id("bandages"));
        auto old_id = std::string{};
        auto old_schema = std::string{};
        auto reads = 0;
        game_client::memory::set_input_provider([&](const int /*timeout*/) {
            const auto snapshot = game_client::current_interaction();
            if (snapshot.context == "PICKUP" && reads++ == 0) {
                REQUIRE(snapshot.choices.size() == 2);
                old_id = snapshot.choices.front().id;
                old_schema = snapshot.schema_id;
                return resolve_action("FILTER");
            }
            if (snapshot.kind == game_client::interaction_kind::field) {
                return resolve({
                    .input_id = snapshot.input_id,
                    .operation = game_client::interaction_operation::fill,
                    .target_id = snapshot.field->id,
                    .value = "bandage",
                    .submit = true,
                });
            }
            REQUIRE(snapshot.context == "PICKUP");
            REQUIRE(snapshot.choices.size() == 1);
            CHECK(snapshot.choices.front().columns.front().value == "bandages");
            CHECK(snapshot.schema_id != old_schema);
            const auto removed = game_client::resolve_interaction_command({
                .input_id = snapshot.input_id,
                .operation = game_client::interaction_operation::choose,
                .target_id = old_id,
            });
            REQUIRE_FALSE(removed);
            const auto stale_boundary = game_client::resolve_interaction_command({
                .input_id = snapshot.input_id + 1,
                .operation = game_client::interaction_operation::choose,
                .target_id = snapshot.choices.front().id,
            });
            REQUIRE_FALSE(stale_boundary);
            return resolve({
                .input_id = snapshot.input_id,
                .operation = game_client::interaction_operation::cancel,
            });
        });
        pickup::pick_up(pickup_test_pos, 0, pickup::from_ground);
        CHECK_FALSE(get_avatar().activity);
        CHECK(ground_item_count(itype_id("test_screwdriver")) == 1);
        CHECK(ground_item_count(itype_id("bandages")) == 1);
    }
}

TEST_CASE(
    "semantic ground pickup preserves native capacity refusal",
    "[client][interaction][pickup][mcp]") {
    const auto guard = interaction_test_guard{};
    const auto fixture = pickup_fixture_guard{false};
    const auto id = itype_id("test_overweight_cube");
    auto& you = get_avatar();
    auto pickup_item = item::spawn(id);
    REQUIRE(pickup_item);
    REQUIRE_FALSE(you.can_pick_weight(pickup_item->weight(), false));
    get_map().add_item(pickup_test_pos, std::move(pickup_item));
    auto saw_capacity_prompt = false;
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto snapshot = game_client::current_interaction();
        if (snapshot.context == "PICKUP") {
            if (!snapshot.choices.front().selected) {
                return resolve({
                    .input_id = snapshot.input_id,
                    .operation = game_client::interaction_operation::choose,
                    .target_id = snapshot.choices.front().id,
                });
            }
            return resolve_action("CONFIRM");
        }
        REQUIRE(snapshot.context == "UILIST");
        CHECK(snapshot.message.find("heavy") != std::string::npos);
        saw_capacity_prompt = true;
        return resolve({
            .input_id = snapshot.input_id,
            .operation = game_client::interaction_operation::cancel,
        });
    });

    pickup::pick_up(pickup_test_pos, 0, pickup::from_ground);
    finish_pickup_activity(you);

    CHECK(saw_capacity_prompt);
    CHECK(you.amount_of(id) == 0);
    CHECK(ground_item_count(id) == 1);
}

TEST_CASE(
    "semantic ground pickup keeps native nesting wear wield and select all paths",
    "[client][interaction][pickup][mcp]") {
    const auto guard = interaction_test_guard{};

    const auto add_nested_items = [] {
        auto& parent = add_ground_item(itype_id("backpack"));
        auto& first_child = add_ground_item(itype_id("test_screwdriver"));
        auto& second_child = add_ground_item(itype_id("bandages"));
        auto& tokens = drop_token::get_provider();
        *parent.drop_token = tokens.make_next(calendar::turn);
        *first_child.drop_token = tokens.make_next(calendar::turn);
        first_child.drop_token->parent_number = parent.drop_token->drop_number;
        *second_child.drop_token = tokens.make_next(calendar::turn);
        second_child.drop_token->parent_number = parent.drop_token->drop_number;
    };

    SECTION("choosing a parent propagates to multiple children") {
        const auto fixture = pickup_fixture_guard{};
        add_nested_items();
        auto reads = 0;
        game_client::memory::set_input_provider([&](const int /*timeout*/) {
            const auto snapshot = game_client::current_interaction();
            REQUIRE(snapshot.choices.size() == 3);
            const auto parent = std::ranges::find_if(snapshot.choices, [](const auto& choice) {
                return choice.columns.front().value == "backpack";
            });
            REQUIRE(parent != snapshot.choices.end());
            if (reads++ == 0) {
                CHECK(parent->columns[2].value.find("2 child stacks") != std::string::npos);
                return resolve({
                    .input_id = snapshot.input_id,
                    .operation = game_client::interaction_operation::choose,
                    .target_id = parent->id,
                });
            }
            CHECK(
                std::ranges::all_of(snapshot.choices, &game_client::interaction_choice::selected));
            return resolve_action("CONFIRM");
        });
        pickup::pick_up(pickup_test_pos, 0, pickup::from_ground);
        finish_pickup_activity(get_avatar());
        CHECK(get_map().i_at(pickup_test_pos).empty());
        CHECK(get_avatar().amount_of(itype_id("backpack")) == 1);
        CHECK(get_avatar().amount_of(itype_id("test_screwdriver")) == 1);
        CHECK(get_avatar().amount_of(itype_id("bandages")) == 1);
    }

    SECTION("choosing only a child leaves its parent and sibling") {
        const auto fixture = pickup_fixture_guard{};
        add_nested_items();
        auto reads = 0;
        game_client::memory::set_input_provider([&](const int /*timeout*/) {
            const auto snapshot = game_client::current_interaction();
            const auto child = std::ranges::find_if(snapshot.choices, [](const auto& choice) {
                return choice.columns.front().value == "test_screwdriver";
            });
            REQUIRE(child != snapshot.choices.end());
            if (reads++ == 0) {
                CHECK(child->columns[2].value.starts_with("child of pickup:"));
                return resolve({
                    .input_id = snapshot.input_id,
                    .operation = game_client::interaction_operation::choose,
                    .target_id = child->id,
                });
            }
            CHECK(child->selected);
            CHECK(std::ranges::
                      count(snapshot.choices, true, &game_client::interaction_choice::selected)
                  == 1);
            return resolve_action("CONFIRM");
        });
        pickup::pick_up(pickup_test_pos, 0, pickup::from_ground);
        finish_pickup_activity(get_avatar());
        CHECK(get_avatar().amount_of(itype_id("test_screwdriver")) == 1);
        CHECK(ground_item_count(itype_id("backpack")) == 1);
        CHECK(ground_item_count(itype_id("bandages")) == 1);
    }

    SECTION("native filtered select all picks only the visible subset") {
        const auto fixture = pickup_fixture_guard{};
        add_ground_item(itype_id("test_screwdriver"));
        add_ground_item(itype_id("bandages"));
        auto step = 0;
        game_client::memory::set_input_provider([&](const int /*timeout*/) {
            const auto snapshot = game_client::current_interaction();
            if (step++ == 0) { return resolve_action("FILTER"); }
            if (snapshot.kind == game_client::interaction_kind::field) {
                return resolve({
                    .input_id = snapshot.input_id,
                    .operation = game_client::interaction_operation::fill,
                    .target_id = snapshot.field->id,
                    .value = "bandage",
                    .submit = true,
                });
            }
            REQUIRE(snapshot.choices.size() == 1);
            if (!snapshot.choices.front().selected) { return resolve_action("SELECT_ALL"); }
            return resolve_action("CONFIRM");
        });
        pickup::pick_up(pickup_test_pos, 0, pickup::from_ground);
        finish_pickup_activity(get_avatar());
        CHECK(get_avatar().amount_of(itype_id("bandages")) == 1);
        CHECK(ground_item_count(itype_id("test_screwdriver")) == 1);
    }

    SECTION("native wear and wield keep their checks and transfer paths") {
        const auto fixture = pickup_fixture_guard{};
        add_ground_item(itype_id("backpack"));
        auto step = 0;
        game_client::memory::set_input_provider([&](const int /*timeout*/) {
            const auto snapshot = game_client::current_interaction();
            if (step++ == 0) {
                return resolve({
                    .input_id = snapshot.input_id,
                    .operation = game_client::interaction_operation::choose,
                    .target_id = snapshot.choices.front().id,
                });
            }
            return resolve_action("WEAR");
        });
        pickup::pick_up(pickup_test_pos, 0, pickup::from_ground);
        CHECK_FALSE(get_avatar().activity);
        CHECK(std::ranges::any_of(get_avatar().worn, [](const auto& worn) {
            return worn->typeId() == itype_id("backpack");
        }));
        CHECK(ground_item_count(itype_id("backpack")) == 0);

        add_ground_item(itype_id("machete"));
        step = 0;
        game_client::memory::set_input_provider([&](const int /*timeout*/) {
            const auto snapshot = game_client::current_interaction();
            if (step++ == 0) {
                return resolve({
                    .input_id = snapshot.input_id,
                    .operation = game_client::interaction_operation::choose,
                    .target_id = snapshot.choices.front().id,
                });
            }
            return resolve_action("WIELD");
        });
        pickup::pick_up(pickup_test_pos, 0, pickup::from_ground);
        CHECK(get_avatar().primary_weapon().typeId() == itype_id("machete"));
        CHECK(ground_item_count(itype_id("machete")) == 0);
    }

    SECTION("native wear denial leaves an unavailable item on the ground") {
        const auto fixture = pickup_fixture_guard{};
        add_ground_item(itype_id("machete"));
        auto reads = 0;
        game_client::memory::set_input_provider([&](const int /*timeout*/) {
            const auto snapshot = game_client::current_interaction();
            REQUIRE(snapshot.choices.size() == 1);
            CHECK(snapshot.choices.front().columns[3].value != "available");
            if (reads++ == 0) { return resolve_action("WEAR"); }
            return resolve({
                .input_id = snapshot.input_id,
                .operation = game_client::interaction_operation::cancel,
            });
        });
        pickup::pick_up(pickup_test_pos, 0, pickup::from_ground);
        CHECK_FALSE(get_avatar().activity);
        CHECK(ground_item_count(itype_id("machete")) == 1);
    }
}

TEST_CASE(
    "semantic replay repeats real partial ground pickup transfer",
    "[client][interaction][pickup][replay][mcp]") {
    const auto guard = interaction_test_guard{};
    static auto counter = std::atomic_uint64_t{0};
    const auto directory =
        std::filesystem::path(PATH_INFO::user_dir())
        / ("semantic-pickup-replay-" + std::to_string(counter.fetch_add(1)));
    REQUIRE(std::filesystem::create_directory(directory));
    const auto cleanup_replay = on_out_of_scope([&] {
        replay::stop();
        std::filesystem::remove_all(directory);
        clear_pickup_fixture_state();
    });
    const auto path = directory / "input.jsonl";
    const auto id = itype_id("test_screwdriver");
    const auto setup = [&] {
        clear_pickup_fixture_state();
        auto& you = get_avatar();
        you.setpos(map_local_to_abs(get_map(), pickup_test_pos));
        you.set_moves(1000);
        you.worn.push_back(item::spawn("duffelbag"));
        std::ranges::for_each(std::views::iota(0, 3), [&](const auto /*index*/) {
            add_ground_item(id);
        });
    };

    setup();
    replay::configure_recording(path.string(), {.rng_seed = 424242});
    replay::start();
    auto reads = 0;
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto snapshot = game_client::current_interaction();
        if (reads++ == 0) {
            return resolve({
                .input_id = snapshot.input_id,
                .operation = game_client::interaction_operation::set_count,
                .target_id = snapshot.choices.front().id,
                .count = 2,
            });
        }
        return resolve_action("CONFIRM");
    });
    pickup::pick_up(pickup_test_pos, 0, pickup::from_ground);
    finish_pickup_activity(get_avatar());
    replay::finish();
    REQUIRE(get_avatar().amount_of(id) == 2);
    REQUIRE(ground_item_count(id) == 1);

    setup();
    game_client::memory::set_input_provider([](const int /*timeout*/) -> input_event {
        throw std::runtime_error("playback must not read live input");
    });
    replay::configure_playback(path.string(), {.rng_seed = 424242});
    replay::start();
    pickup::pick_up(pickup_test_pos, 0, pickup::from_ground);
    finish_pickup_activity(get_avatar());
    CHECK(get_avatar().amount_of(id) == 2);
    CHECK(ground_item_count(id) == 1);
    CHECK_THROWS_AS(replay::next_input_event(), replay::completed);
    replay::finish();
}

TEST_CASE(
    "semantic replay restores real inventory quantity selection",
    "[client][interaction][inventory][replay][mcp]") {
    clear_all_state();
    const auto cleanup_state = on_out_of_scope([]() { clear_all_state(); });
    const auto guard = interaction_test_guard{};
    static auto counter = std::atomic_uint64_t{0};
    const auto directory =
        std::filesystem::path(PATH_INFO::user_dir())
        / ("semantic-inventory-replay-" + std::to_string(counter.fetch_add(1)));
    REQUIRE(std::filesystem::create_directory(directory));
    const auto cleanup_replay = on_out_of_scope([&] {
        replay::stop();
        std::filesystem::remove_all(directory);
    });
    const auto path = directory / "input.jsonl";
    auto& you = get_avatar();
    you.i_add(item::spawn("test_platinum_bit", calendar::turn, 7));

    replay::configure_recording(path.string(), {.rng_seed = 23});
    replay::start();
    auto reads = 0;
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto snapshot = game_client::current_interaction();
        if (reads++ == 0) {
            return resolve({
                .input_id = snapshot.input_id,
                .operation = game_client::interaction_operation::set_count,
                .target_id = snapshot.choices.front().id,
                .count = 2,
            });
        }
        return resolve_action("CONFIRM");
    });
    auto recorded_selector = inventory_drop_selector(you);
    recorded_selector.add_character_items(you);
    const auto recorded = recorded_selector.execute();
    replay::finish();
    REQUIRE(recorded.size() == 1);
    CHECK(recorded.front().count == 2);

    game_client::memory::set_input_provider([](const int /*timeout*/) -> input_event {
        throw std::runtime_error("playback must not read live input");
    });
    replay::configure_playback(path.string(), {.rng_seed = 23});
    replay::start();
    auto playback_selector = inventory_drop_selector(you);
    playback_selector.add_character_items(you);
    const auto playback = playback_selector.execute();
    REQUIRE(playback.size() == 1);
    CHECK(playback.front().count == recorded.front().count);
    CHECK(&*playback.front().loc == &*recorded.front().loc);
    CHECK_THROWS_AS(replay::next_input_event(), replay::completed);
    replay::finish();
}

TEST_CASE(
    "inventory semantic ids reject filtered removed and denied entries",
    "[client][interaction][inventory][mcp]") {
    clear_all_state();
    const auto cleanup = on_out_of_scope([]() { clear_all_state(); });
    const auto guard = interaction_test_guard{};
    auto& you = get_avatar();

    SECTION("filter invalidates the old opaque id") {
        auto bandages = item::spawn("bandages");
        auto heroin = item::spawn("heroin");
        auto selector = inventory_pick_selector(you);
        selector.add_item(selector.own_inv_column, bandages.get());
        selector.add_item(selector.own_inv_column, heroin.get());
        game_client::memory::set_input_provider([&](const int /*timeout*/) {
            const auto before = game_client::current_interaction();
            REQUIRE(before.choices.size() == 2);
            const auto old_id = before.choices.back().id;
            selector.set_filter("bandage");
            const auto stale = game_client::resolve_interaction_command({
                .input_id = before.input_id,
                .operation = game_client::interaction_operation::choose,
                .target_id = old_id,
            });
            REQUIRE_FALSE(stale);
            const auto after = game_client::current_interaction();
            REQUIRE(after.choices.size() == 1);
            return resolve({
                .input_id = after.input_id,
                .operation = game_client::interaction_operation::choose,
                .target_id = after.choices.front().id,
            });
        });
        CHECK(selector.execute() == bandages.get());
    }

    SECTION("reorder invalidates the old row-bound opaque id") {
        auto heroin = item::spawn("heroin");
        auto bandages = item::spawn("bandages");
        auto selector = inventory_pick_selector(you);
        selector.add_item(selector.own_inv_column, heroin.get());
        game_client::memory::set_input_provider([&](const int /*timeout*/) {
            const auto before = game_client::current_interaction();
            REQUIRE(before.choices.size() == 1);
            selector.add_item(selector.own_inv_column, bandages.get());
            selector.set_filter(selector.get_filter());
            const auto stale = game_client::resolve_interaction_command({
                .input_id = before.input_id,
                .operation = game_client::interaction_operation::choose,
                .target_id = before.choices.front().id,
            });
            REQUIRE_FALSE(stale);
            return resolve({
                .input_id = game_client::current_interaction().input_id,
                .operation = game_client::interaction_operation::cancel,
            });
        });
        CHECK(selector.execute() == nullptr);
    }

    SECTION("removal invalidates the old opaque id") {
        auto bandages = item::spawn("bandages");
        auto selector = inventory_pick_selector(you);
        selector.add_item(selector.own_inv_column, bandages.get());
        game_client::memory::set_input_provider([&](const int /*timeout*/) {
            const auto before = game_client::current_interaction();
            REQUIRE(before.choices.size() == 1);
            selector.remove_item(bandages.get());
            const auto stale = game_client::resolve_interaction_command({
                .input_id = before.input_id,
                .operation = game_client::interaction_operation::choose,
                .target_id = before.choices.front().id,
            });
            REQUIRE_FALSE(stale);
            return resolve({
                .input_id = game_client::current_interaction().input_id,
                .operation = game_client::interaction_operation::cancel,
            });
        });
        CHECK(selector.execute() == nullptr);
    }

    SECTION("denial is published and cannot be selected") {
        auto bandages = item::spawn("bandages");
        const auto preset = denied_inventory_preset{};
        auto selector = inventory_pick_selector(you, preset);
        selector.add_item(selector.own_inv_column, bandages.get());
        game_client::memory::set_input_provider([&](const int /*timeout*/) {
            const auto snapshot = game_client::current_interaction();
            REQUIRE(snapshot.choices.size() == 1);
            CHECK_FALSE(snapshot.choices.front().selectable);
            CHECK(snapshot.choices.front().denial == "Denied by test preset");
            const auto denied = game_client::resolve_interaction_command({
                .input_id = snapshot.input_id,
                .operation = game_client::interaction_operation::choose,
                .target_id = snapshot.choices.front().id,
            });
            REQUIRE_FALSE(denied);
            CHECK(denied.error().starts_with("disabled:"));
            return resolve({
                .input_id = snapshot.input_id,
                .operation = game_client::interaction_operation::cancel,
            });
        });
        CHECK(selector.execute() == nullptr);
    }
}

TEST_CASE(
    "inventory count popup restores the outer structured selector",
    "[client][interaction][inventory][mcp]") {
    clear_all_state();
    const auto cleanup = on_out_of_scope([]() { clear_all_state(); });
    const auto guard = interaction_test_guard{};
    auto& you = get_avatar();
    you.i_add(item::spawn("test_platinum_bit", calendar::turn, 7));
    auto selector = inventory_drop_selector(you);
    selector.add_character_items(you);
    auto reads = 0;
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto snapshot = game_client::current_interaction();
        if (reads++ == 0) {
            REQUIRE(snapshot.kind == game_client::interaction_kind::inventory);
            auto input = input_event('2', input_event_t::keyboard);
            return input;
        }
        if (reads == 2) {
            REQUIRE(snapshot.kind == game_client::interaction_kind::field);
            return resolve({
                .input_id = snapshot.input_id,
                .operation = game_client::interaction_operation::fill,
                .target_id = snapshot.field->id,
                .value = "2",
                .submit = true,
            });
        }
        REQUIRE(snapshot.kind == game_client::interaction_kind::inventory);
        CHECK(snapshot.choices.front().selected_count == 2);
        return resolve_action("CONFIRM");
    });
    const auto dropped = selector.execute();
    REQUIRE(dropped.size() == 1);
    CHECK(dropped.front().count == 2);
}

TEST_CASE(
    "real target UI accepts semantic cursor movement and rejects range overflow",
    "[client][interaction][target][mcp]") {
    clear_all_state();
    const auto cleanup = on_out_of_scope([]() { clear_all_state(); });
    const auto guard = interaction_test_guard{};
    build_test_map(ter_id("t_floor"));
    auto& you = get_avatar();
    const auto source = you.bub_pos();
    const auto visible = source + tripoint(2, 0, 0);
    const auto barrier = source + tripoint(4, 0, 0);
    const auto hidden = source + tripoint(5, 0, 0);
    spawn_test_monster("mon_zombie", visible);
    spawn_test_monster("mon_zombie", hidden);
    REQUIRE(get_map().ter_set(barrier, ter_id("t_wall")));
    get_map().invalidate_map_cache(0);
    get_map().build_map_cache(0, true);
    get_map().update_visibility_cache(0);
    REQUIRE(you.sees(visible));
    REQUIRE_FALSE(you.sees(hidden));
    auto rock = item::spawn("rock");
    auto reads = 0;
    auto target_position = game_client::interaction_position{};
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto snapshot = game_client::current_interaction();
        REQUIRE(snapshot.kind == game_client::interaction_kind::target);
        REQUIRE(snapshot.target);
        if (reads++ == 0) {
            const auto expected_source = game_client::
                interaction_position{.x = source.x(), .y = source.y(), .z = source.z()};
            CHECK(snapshot.target->source == expected_source);
            const auto visible_candidate = std::ranges::find(
                snapshot.target->candidates,
                game_client::
                    interaction_position{.x = visible.x(), .y = visible.y(), .z = visible.z()},
                &game_client::interaction_target_candidate::position);
            CHECK(visible_candidate != snapshot.target->candidates.end());
            const auto hidden_candidate = std::ranges::find(
                snapshot.target->candidates,
                game_client::interaction_position{.x = hidden.x(), .y = hidden.y(), .z = hidden.z()},
                &game_client::interaction_target_candidate::position);
            CHECK(hidden_candidate == snapshot.target->candidates.end());
            target_position = {
                .x = source.x() + 1,
                .y = source.y(),
                .z = source.z(),
            };
            const auto outside = game_client::resolve_interaction_command({
                .input_id = snapshot.input_id,
                .operation = game_client::interaction_operation::set_target,
                .position =
                    game_client::interaction_position{
                        .x = source.x() + snapshot.target->range + 1,
                        .y = source.y(),
                        .z = source.z(),
                    },
            });
            REQUIRE_FALSE(outside);
            CHECK(outside.error().contains("outside the active range"));
            return resolve({
                .input_id = snapshot.input_id,
                .operation = game_client::interaction_operation::set_target,
                .position = target_position,
            });
        }
        CHECK(snapshot.target->cursor == target_position);
        return resolve_action("FIRE");
    });

    const auto trajectory = target_handler::mode_throw(you, *rock, false);
    REQUIRE_FALSE(trajectory.empty());
    CHECK(trajectory.back() == source + tripoint_east);
}

TEST_CASE(
    "semantic targeting retains the normal neutral attack confirmation",
    "[client][interaction][target][mcp]") {
    clear_all_state();
    const auto cleanup = on_out_of_scope([]() { clear_all_state(); });
    const auto guard = interaction_test_guard{};
    build_test_map(ter_id("t_floor"));
    auto& you = get_avatar();
    const auto npc_position = you.bub_pos() + tripoint_east;
    auto& bystander = spawn_npc(npc_position, "test_talker");
    bystander.set_attitude(NPCATT_NULL);
    get_map().invalidate_map_cache(0);
    get_map().build_map_cache(0, true);
    get_map().update_visibility_cache(0);
    REQUIRE(you.sees(bystander));
    auto rock = item::spawn("rock");
    auto reads = 0;
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto snapshot = game_client::current_interaction();
        if (reads++ == 0) {
            REQUIRE(snapshot.target);
            const auto expected_position = game_client::interaction_position{
                .x = npc_position.x(), .y = npc_position.y(), .z = npc_position.z()};
            const auto candidate = std::ranges::
                find(snapshot.target->candidates, expected_position,
                     &game_client::interaction_target_candidate::position);
            REQUIRE(candidate != snapshot.target->candidates.end());
            return resolve({
                .input_id = snapshot.input_id,
                .operation = game_client::interaction_operation::set_target,
                .target_id = candidate->id,
                .position = candidate->position,
            });
        }
        if (reads == 2) {
            REQUIRE(snapshot.kind == game_client::interaction_kind::target);
            return resolve_action("FIRE");
        }
        if (reads == 3) {
            REQUIRE(snapshot.kind == game_client::interaction_kind::choices);
            CHECK(snapshot.message.contains("Really attack"));
            const auto no = std::ranges::
                find(snapshot.choices, "NO", &game_client::interaction_choice::description);
            REQUIRE(no != snapshot.choices.end());
            return resolve({
                .input_id = snapshot.input_id,
                .operation = game_client::interaction_operation::choose,
                .target_id = no->id,
            });
        }
        REQUIRE(snapshot.kind == game_client::interaction_kind::target);
        return resolve({
            .input_id = snapshot.input_id,
            .operation = game_client::interaction_operation::cancel,
        });
    });

    CHECK(target_handler::mode_throw(you, *rock, false).empty());
}

TEST_CASE(
    "semantic replay re-enters the same real string widget", "[client][interaction][replay][mcp]") {
    const auto guard = interaction_test_guard{};
    static auto counter = std::atomic_uint64_t{0};
    const auto directory =
        std::filesystem::path(PATH_INFO::user_dir())
        / ("semantic-replay-" + std::to_string(counter.fetch_add(1)));
    REQUIRE(std::filesystem::create_directory(directory));
    const auto cleanup = on_out_of_scope([&] {
        replay::stop();
        std::filesystem::remove_all(directory);
    });
    const auto path = directory / "input.jsonl";

    replay::configure_recording(path.string(), {.rng_seed = 17});
    replay::start();
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto snapshot = game_client::current_interaction();
        REQUIRE(snapshot.field);
        return resolve({
            .input_id = snapshot.input_id,
            .operation = game_client::interaction_operation::fill,
            .target_id = snapshot.field->id,
            .value = "recorded value",
            .submit = true,
        });
    });
    auto recorded_popup = string_input_popup{};
    CHECK(recorded_popup.title("Replay field").max_length(30).query_string() == "recorded value");
    replay::finish();

    game_client::memory::set_input_provider([](const int /*timeout*/) -> input_event {
        throw std::runtime_error("playback must not read live input");
    });
    replay::configure_playback(path.string(), {.rng_seed = 17});
    replay::start();
    auto playback_popup = string_input_popup{};
    CHECK(playback_popup.title("Replay field").max_length(30).query_string() == "recorded value");
    CHECK_THROWS_AS(replay::next_input_event(), replay::completed);
    replay::finish();
}

namespace {

class fixed_clipboard final: public game_client::render_service {
public:
    std::string text;
    auto clipboard_available() const -> bool override { return true; }
    auto clipboard_text() -> std::string override { return text; }
};

} // namespace

TEST_CASE(
    "replay inserts the recorded clipboard text instead of the live clipboard",
    "[client][replay][mcp]") {
    const auto guard = interaction_test_guard{};
    static auto counter = std::atomic_uint64_t{0};
    const auto directory =
        std::filesystem::path(PATH_INFO::user_dir())
        / ("clipboard-replay-" + std::to_string(counter.fetch_add(1)));
    REQUIRE(std::filesystem::create_directory(directory));
    const auto cleanup = on_out_of_scope([&] {
        replay::stop();
        std::filesystem::remove_all(directory);
    });
    const auto path = directory / "input.jsonl";
    auto clipboard = std::make_unique<fixed_clipboard>();
    auto& live_clipboard = *clipboard;
    const auto presentation = game_client::presentation_scope(std::move(clipboard));

    auto presses = 0;
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        return resolve_action(presses++ == 0 ? "TEXT.PASTE" : "TEXT.CONFIRM");
    });
    live_clipboard.text = "Alice";
    replay::configure_recording(path.string(), {.rng_seed = 23});
    replay::start();
    auto recorded_popup = string_input_popup{};
    CHECK(recorded_popup.title("Paste").max_length(30).query_string() == "Alice");
    replay::finish();

    live_clipboard.text = "Bob";
    game_client::memory::set_input_provider([](const int /*timeout*/) -> input_event {
        throw std::runtime_error("playback must not read live input");
    });
    replay::configure_playback(path.string(), {.rng_seed = 23});
    replay::start();
    auto playback_popup = string_input_popup{};
    CHECK(playback_popup.title("Paste").max_length(30).query_string() == "Alice");
    CHECK_THROWS_AS(replay::next_input_event(), replay::completed);
    replay::finish();
}

TEST_CASE(
    "semantic replay rejects a different real uilist in the same context",
    "[client][interaction][replay][mcp]") {
    const auto guard = interaction_test_guard{};
    static auto counter = std::atomic_uint64_t{0};
    const auto directory =
        std::filesystem::path(PATH_INFO::user_dir())
        / ("semantic-list-replay-" + std::to_string(counter.fetch_add(1)));
    REQUIRE(std::filesystem::create_directory(directory));
    const auto cleanup = on_out_of_scope([&] {
        replay::stop();
        std::filesystem::remove_all(directory);
    });
    const auto path = directory / "input.jsonl";

    replay::configure_recording(path.string(), {.rng_seed = 19});
    replay::start();
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto snapshot = game_client::current_interaction();
        return resolve({
            .input_id = snapshot.input_id,
            .operation = game_client::interaction_operation::choose,
            .target_id = snapshot.choices.front().id,
        });
    });
    {
        auto recorded = uilist{};
        recorded.entries.emplace_back(1, true, MENU_AUTOASSIGN, "Recorded choice");
        recorded.query();
    }
    replay::finish();

    replay::configure_playback(path.string(), {.rng_seed = 19});
    replay::start();
    {
        auto different = uilist{};
        different.entries.emplace_back(1, true, MENU_AUTOASSIGN, "Different choice");
        CHECK_THROWS_WITH(different.query(), Catch::Matchers::Contains("schema changed"));
    }
}

TEST_CASE(
    "common direction choice exposes only native relative vectors",
    "[client][interaction][direction][mcp]") {
    clear_all_state();
    const auto cleanup = on_out_of_scope([]() { clear_all_state(); });
    const auto guard = interaction_test_guard{};

    const auto choose = [&](const std::string& label, const bool allow_vertical) {
        game_client::memory::set_input_provider([&](const int /*timeout*/) {
            const auto snapshot = game_client::current_interaction();
            REQUIRE(snapshot.context == "DEFAULTMODE");
            REQUIRE(snapshot.kind == game_client::interaction_kind::choices);
            CHECK(snapshot.allow_cancel);
            CHECK(snapshot.choices.size() == (allow_vertical ? 11 : 9));
            const auto choice =
                std::ranges::find(snapshot.choices, label, &game_client::interaction_choice::label);
            REQUIRE(choice != snapshot.choices.end());
            return resolve({
                .input_id = snapshot.input_id,
                .operation = game_client::interaction_operation::choose,
                .target_id = choice->id,
            });
        });
        return choose_direction("Test direction", allow_vertical);
    };

    const auto cases = std::vector<std::pair<std::string, tripoint_rel_ms>>{
        {"North", tripoint_rel_ms::north()}, {"North East", tripoint_rel_ms::north_east()},
        {"East", tripoint_rel_ms::east()},   {"South East", tripoint_rel_ms::south_east()},
        {"South", tripoint_rel_ms::south()}, {"South West", tripoint_rel_ms::south_west()},
        {"West", tripoint_rel_ms::west()},   {"North West", tripoint_rel_ms::north_west()},
        {"Here", tripoint_rel_ms::zero()},   {"Above", tripoint_rel_ms::above()},
        {"Below", tripoint_rel_ms::below()},
    };
    for (const auto& [label, expected] : cases) {
        const auto selected = choose(label, expected.z() != 0);
        REQUIRE(selected);
        CHECK(*selected == expected);
    }
    CHECK(get_avatar().facing == FD_LEFT);

    auto stale_error = std::string{};
    auto absent_error = std::string{};
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto snapshot = game_client::current_interaction();
        CHECK(std::ranges::none_of(snapshot.choices, [](const auto& choice) {
            return choice.label == "Above";
        }));
        const auto stale = game_client::resolve_interaction_command({
            .input_id = snapshot.input_id + 1,
            .operation = game_client::interaction_operation::choose,
            .target_id = snapshot.choices.front().id,
        });
        REQUIRE_FALSE(stale);
        stale_error = stale.error();
        const auto absent = game_client::resolve_interaction_command({
            .input_id = snapshot.input_id,
            .operation = game_client::interaction_operation::choose,
            .target_id = game_client::opaque_interaction_id("direction", {"0", "0", "1"}),
        });
        REQUIRE_FALSE(absent);
        absent_error = absent.error();
        return resolve({
            .input_id = snapshot.input_id,
            .operation = game_client::interaction_operation::cancel,
        });
    });
    CHECK_FALSE(choose_direction("Cancel direction"));
    CHECK(stale_error.starts_with("stale:"));
    CHECK(absent_error.contains("not in the active interaction"));
}

TEST_CASE(
    "semantic construction choice preserves native placement and resource handling",
    "[client][interaction][construction][mcp]") {
    clear_all_state();
    const auto cleanup = on_out_of_scope([]() { clear_all_state(); });
    const auto guard = interaction_test_guard{};
    const auto restore_turn = restore_on_out_of_scope<decltype(calendar::turn)>(calendar::turn);
    const auto restore_filter = restore_on_out_of_scope<decltype(uistate.construction_filter)>(
        uistate.construction_filter);
    const auto restore_tab = restore_on_out_of_scope<decltype(uistate.construction_tab)>(
        uistate.construction_tab);
    const auto restore_last = restore_on_out_of_scope<decltype(uistate.last_construction)>(
        uistate.last_construction);
    calendar::turn = calendar::turn_zero + 12_hours;
    g->reset_light_level();
    uistate.construction_filter.clear();
    build_test_map(ter_id("t_floor"));
    auto& you = get_avatar();
    add_crafting_fixture_item(you, itype_id("e_scrap"), 2);
    const auto origin = you.bub_pos();
    const auto destination = origin + tripoint_east;
    clear_construction_fixture(destination);
    const auto cleanup_construction = on_out_of_scope(
        std::bind_front(clear_construction_fixture, destination));
    get_map().invalidate_map_cache(you.bub_pos().z());
    get_map().build_map_cache(you.bub_pos().z());
    get_map().update_visibility_cache(you.bub_pos().z());
    REQUIRE(character_funcs::can_see_fine_details(you));
    auto step = 0;

    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto snapshot = game_client::current_interaction();
        if (snapshot.context == "CONSTRUCTION") {
            if (step == 0) {
                step = 1;
                return resolve_action("FILTER");
            }
            REQUIRE(step == 2);
            const auto choice = std::ranges::
                find(snapshot.choices, "TEST semantic construction",
                     &game_client::interaction_choice::label);
            REQUIRE(choice != snapshot.choices.end());
            CHECK(choice->enabled);
            CHECK(choice->selectable);
            step = 3;
            return resolve({
                .input_id = snapshot.input_id,
                .operation = game_client::interaction_operation::choose,
                .target_id = choice->id,
            });
        }
        if (snapshot.context != "DEFAULTMODE") {
            REQUIRE(step == 1);
            REQUIRE(snapshot.field);
            step = 2;
            return resolve({
                .input_id = snapshot.input_id,
                .operation = game_client::interaction_operation::fill,
                .target_id = snapshot.field->id,
                .value = "TEST semantic construction",
                .submit = true,
            });
        }
        REQUIRE(step == 3);
        const auto east =
            std::ranges::find(snapshot.choices, "East", &game_client::interaction_choice::label);
        REQUIRE(east != snapshot.choices.end());
        step = 4;
        return resolve({
            .input_id = snapshot.input_id,
            .operation = game_client::interaction_operation::choose,
            .target_id = east->id,
        });
    });

    CHECK_FALSE(construction_menu(false));
    CHECK(step == 4);
    CHECK(you.amount_of(itype_id("e_scrap")) == 0);
    CHECK(get_map().partial_con_at(destination) != nullptr);
    REQUIRE(you.activity);
    CHECK(you.activity->id() == activity_id("ACT_BUILD"));
    finish_construction_activity(you);
    CHECK_FALSE(you.activity);
    CHECK(get_map().partial_con_at(destination) == nullptr);
    CHECK(get_map().furn(destination) == furn_id("f_test_semantic_construction"));
}

TEST_CASE(
    "semantic construction darkness denial retains resources without placement",
    "[client][interaction][construction][mcp]") {
    const auto restore_light = on_out_of_scope([]() {
        g->reset_light_level();
        get_map().invalidate_map_cache(get_avatar().bub_pos().z());
    });
    const auto restore_turn = restore_on_out_of_scope<decltype(calendar::turn)>(calendar::turn);
    clear_all_state();
    const auto cleanup = on_out_of_scope([]() { clear_all_state(); });
    const auto guard = interaction_test_guard{};
    const auto restore_filter = restore_on_out_of_scope<decltype(uistate.construction_filter)>(
        uistate.construction_filter);
    const auto restore_tab = restore_on_out_of_scope<decltype(uistate.construction_tab)>(
        uistate.construction_tab);
    const auto restore_last = restore_on_out_of_scope<decltype(uistate.last_construction)>(
        uistate.last_construction);
    calendar::turn = calendar::turn_zero;
    g->reset_light_level();
    uistate.construction_filter = "TEST semantic construction";
    build_test_map(ter_id("t_floor"));
    auto& you = get_avatar();
    const auto destination = you.bub_pos() + tripoint_east;
    clear_construction_fixture(destination);
    const auto cleanup_construction = on_out_of_scope(
        std::bind_front(clear_construction_fixture, destination));
    add_crafting_fixture_item(you, itype_id("e_scrap"), 2);
    get_map().invalidate_map_cache(you.bub_pos().z());
    get_map().build_map_cache(you.bub_pos().z());
    get_map().update_visibility_cache(you.bub_pos().z());
    REQUIRE_FALSE(character_funcs::can_see_fine_details(you));
    REQUIRE_FALSE(you.has_trait(trait_id("DEBUG_HS")));
    const auto& con = construction_str_id("constr_test_semantic_construction").obj();
    REQUIRE_FALSE(con.dark_craftable);
    REQUIRE(can_construct(con, destination));
    REQUIRE(player_can_build(you, you.crafting_inventory(), con));
    const auto materials_before = you.charges_of(itype_id("e_scrap"));
    REQUIRE(materials_before == 2);
    auto reads = 0;
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        ++reads;
        const auto snapshot = game_client::current_interaction();
        REQUIRE(snapshot.context == "CONSTRUCTION");
        REQUIRE(reads == 1);
        const auto choice = std::ranges::
            find(snapshot.choices, "TEST semantic construction",
                 &game_client::interaction_choice::label);
        REQUIRE(choice != snapshot.choices.end());
        REQUIRE(choice->enabled);
        return resolve({
            .input_id = snapshot.input_id,
            .operation = game_client::interaction_operation::choose,
            .target_id = choice->id,
        });
    });
    CHECK_FALSE(construction_menu(false));
    CHECK(reads == 1);
    CHECK(you.charges_of(itype_id("e_scrap")) == materials_before);
    CHECK_FALSE(you.activity);
    CHECK(get_map().partial_con_at(destination) == nullptr);
    CHECK(get_map().furn(destination) == furn_id("f_null"));
}

TEST_CASE(
    "semantic construction denial and blueprint choice do not bypass native rules",
    "[client][interaction][construction][mcp]") {
    clear_all_state();
    const auto cleanup = on_out_of_scope([]() { clear_all_state(); });
    const auto guard = interaction_test_guard{};
    const auto restore_filter = restore_on_out_of_scope<decltype(uistate.construction_filter)>(
        uistate.construction_filter);
    const auto restore_tab = restore_on_out_of_scope<decltype(uistate.construction_tab)>(
        uistate.construction_tab);
    const auto restore_last = restore_on_out_of_scope<decltype(uistate.last_construction)>(
        uistate.last_construction);
    uistate.construction_filter.clear();
    build_test_map(ter_id("t_floor"));
    auto& you = get_avatar();
    const auto material_before = you.amount_of(itype_id("test_1kg_cube"));

    SECTION("native denial") {
        auto step = 0;
        game_client::memory::set_input_provider([&](const int /*timeout*/) {
            const auto snapshot = game_client::current_interaction();
            if (snapshot.context == "CONSTRUCTION") {
                if (step == 0) {
                    step = 1;
                    return resolve_action("FILTER");
                }
                if (step == 2 && snapshot.choices.empty()) {
                    return resolve_action("TOGGLE_UNAVAILABLE_CONSTRUCTIONS");
                }
                if (step == 2) {
                    REQUIRE(snapshot.choices.size() == 1);
                    const auto& choice = snapshot.choices.front();
                    CHECK_FALSE(choice.enabled);
                    CHECK(choice.selectable);
                    CHECK_FALSE(choice.denial.empty());
                    step = 3;
                    return resolve({
                        .input_id = snapshot.input_id,
                        .operation = game_client::interaction_operation::choose,
                        .target_id = choice.id,
                    });
                }
                REQUIRE(step == 4);
                step = 5;
                return resolve({
                    .input_id = snapshot.input_id,
                    .operation = game_client::interaction_operation::cancel,
                });
            }
            if (step == 1) {
                REQUIRE(snapshot.field);
                step = 2;
                return resolve({
                    .input_id = snapshot.input_id,
                    .operation = game_client::interaction_operation::fill,
                    .target_id = snapshot.field->id,
                    .value = "TEST semantic unavailable construction",
                    .submit = true,
                });
            }
            REQUIRE(step == 3);
            CHECK(snapshot.message.contains("can't build"));
            step = 4;
            return resolve({
                .input_id = snapshot.input_id,
                .operation = game_client::interaction_operation::choose,
                .target_id = snapshot.choices.front().id,
            });
        });
        CHECK_FALSE(construction_menu(false));
        CHECK(step == 5);
        CHECK_FALSE(you.activity);
        CHECK(you.amount_of(itype_id("test_1kg_cube")) == material_before);
    }

    SECTION("blueprint ignores material availability") {
        uistate.construction_filter = "TEST semantic unavailable construction";
        game_client::memory::set_input_provider([&](const int /*timeout*/) {
            const auto snapshot = game_client::current_interaction();
            if (snapshot.choices.empty()) {
                return resolve_action("TOGGLE_UNAVAILABLE_CONSTRUCTIONS");
            }
            REQUIRE(snapshot.context == "CONSTRUCTION");
            REQUIRE(snapshot.choices.size() == 1);
            CHECK(snapshot.choices.front().enabled);
            return resolve({
                .input_id = snapshot.input_id,
                .operation = game_client::interaction_operation::choose,
                .target_id = snapshot.choices.front().id,
            });
        });
        const auto result = construction_menu(true);
        REQUIRE(result);
        CHECK(result->obj().id
              == construction_str_id("constr_test_semantic_construction_unavailable"));
        CHECK_FALSE(you.activity);
        CHECK(you.amount_of(itype_id("test_1kg_cube")) == material_before);
    }
}

TEST_CASE(
    "semantic replay repeats native construction placement",
    "[client][interaction][construction][replay][mcp]") {
    clear_all_state();
    const auto cleanup_state = on_out_of_scope([]() { clear_all_state(); });
    const auto guard = interaction_test_guard{};
    const auto restore_turn = restore_on_out_of_scope<decltype(calendar::turn)>(calendar::turn);
    const auto restore_filter = restore_on_out_of_scope<decltype(uistate.construction_filter)>(
        uistate.construction_filter);
    const auto restore_tab = restore_on_out_of_scope<decltype(uistate.construction_tab)>(
        uistate.construction_tab);
    const auto restore_last = restore_on_out_of_scope<decltype(uistate.last_construction)>(
        uistate.last_construction);
    static auto counter = std::atomic_uint64_t{0};
    const auto directory =
        std::filesystem::path(PATH_INFO::user_dir())
        / ("semantic-construction-replay-" + std::to_string(counter.fetch_add(1)));
    REQUIRE(std::filesystem::create_directory(directory));
    const auto cleanup_replay = on_out_of_scope([&] {
        replay::stop();
        std::filesystem::remove_all(directory);
    });
    const auto path = directory / "input.jsonl";

    const auto prepare = [&]() {
        calendar::turn = calendar::turn_zero + 12_hours;
        g->reset_light_level();
        uistate.construction_filter = "TEST semantic construction";
        build_test_map(ter_id("t_floor"));
        auto& you = get_avatar();
        const auto destination = you.bub_pos() + tripoint_east;
        clear_construction_fixture(destination);
        get_map().invalidate_map_cache(you.bub_pos().z());
        get_map().build_map_cache(you.bub_pos().z());
        get_map().update_visibility_cache(you.bub_pos().z());
        REQUIRE(character_funcs::can_see_fine_details(you));
        add_crafting_fixture_item(you, itype_id("e_scrap"), 2);
        return destination;
    };
    auto recorded_reads = 0;
    const auto semantic_provider = [&](const int /*timeout*/) {
        ++recorded_reads;
        const auto snapshot = game_client::current_interaction();
        if (snapshot.context == "CONSTRUCTION") {
            const auto choice = std::ranges::
                find(snapshot.choices, "TEST semantic construction",
                     &game_client::interaction_choice::label);
            REQUIRE(choice != snapshot.choices.end());
            return resolve({
                .input_id = snapshot.input_id,
                .operation = game_client::interaction_operation::choose,
                .target_id = choice->id,
            });
        }
        REQUIRE(snapshot.context == "DEFAULTMODE");
        const auto east =
            std::ranges::find(snapshot.choices, "East", &game_client::interaction_choice::label);
        REQUIRE(east != snapshot.choices.end());
        return resolve({
            .input_id = snapshot.input_id,
            .operation = game_client::interaction_operation::choose,
            .target_id = east->id,
        });
    };

    const auto recorded_destination = prepare();
    const auto cleanup_construction = on_out_of_scope(
        std::bind_front(clear_construction_fixture, recorded_destination));
    replay::configure_recording(path.string(), {.rng_seed = 37});
    replay::start();
    game_client::memory::set_input_provider(semantic_provider);
    CHECK_FALSE(construction_menu(false));
    CHECK(recorded_reads == 2);
    REQUIRE(get_avatar().activity);
    CHECK(get_avatar().activity->id() == activity_id("ACT_BUILD"));
    CHECK(get_avatar().amount_of(itype_id("e_scrap")) == 0);
    CHECK(get_map().partial_con_at(recorded_destination) != nullptr);
    finish_construction_activity(get_avatar());
    CHECK_FALSE(get_avatar().activity);
    CHECK(get_map().partial_con_at(recorded_destination) == nullptr);
    CHECK(get_map().furn(recorded_destination) == furn_id("f_test_semantic_construction"));
    replay::finish();

    game_client::memory::set_input_provider([](const int /*timeout*/) {
        return resolve_action("YES");
    });
    clear_all_state();
    const auto played_destination = prepare();
    game_client::memory::set_input_provider([](const int /*timeout*/) -> input_event {
        throw std::runtime_error("construction playback must not read live input");
    });
    replay::configure_playback(path.string(), {.rng_seed = 37});
    replay::start();
    CHECK_FALSE(construction_menu(false));
    REQUIRE(get_avatar().activity);
    CHECK(get_avatar().activity->id() == activity_id("ACT_BUILD"));
    CHECK(get_avatar().amount_of(itype_id("e_scrap")) == 0);
    CHECK(get_map().partial_con_at(played_destination) != nullptr);
    finish_construction_activity(get_avatar());
    CHECK_FALSE(get_avatar().activity);
    CHECK(get_map().partial_con_at(played_destination) == nullptr);
    CHECK(get_map().furn(played_destination) == furn_id("f_test_semantic_construction"));
    CHECK_THROWS_AS(replay::next_input_event(), replay::completed);
    replay::finish();
}

TEST_CASE(
    "semantic crafting batch entry preserves native selection bookkeeping",
    "[client][interaction][crafting][mcp]") {
    clear_all_state();
    const auto cleanup = on_out_of_scope([]() { clear_all_state(); });
    const auto guard = interaction_test_guard{};
    const auto highlight_unread = override_option("HIGHLIGHT_UNREAD_RECIPES", "true");
    const auto restore_read = restore_on_out_of_scope<decltype(uistate.read_recipes)>(
        uistate.read_recipes);
    const auto restore_expanded = restore_on_out_of_scope<decltype(uistate.expanded_recipes)>(
        uistate.expanded_recipes);
    uistate.read_recipes.clear();
    auto& you = get_avatar();
    const auto& pipe = recipe_id("test_pipe").obj();
    prepare_crafting_fixture();

    auto step = 0;
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto snapshot = recipe_rows(game_client::current_interaction({.limit = 100}));
        if (snapshot.context != "CRAFTING") {
            REQUIRE(step == 1);
            step = 2;
            return resolve({
                .input_id = snapshot.input_id,
                .operation = game_client::interaction_operation::fill,
                .target_id = snapshot.field->id,
                .value = "TEST",
                .submit = true,
            });
        }
        if (step == 0) {
            step = 1;
            return resolve_action("FILTER");
        }
        if (step == 2) {
            const auto target = std::ranges::find_if(snapshot.choices, [&](const auto& choice) {
                return choice.label.find(pipe.result_name()) != std::string::npos;
            });
            REQUIRE(target != snapshot.choices.end());
            CHECK(std::distance(snapshot.choices.begin(), target) > 0);
            CHECK(uistate.read_recipes.count(pipe.ident()) == 0);
            step = 3;
            return resolve({
                .input_id = snapshot.input_id,
                .operation = game_client::interaction_operation::set_count,
                .target_id = target->id,
                .count = 3,
            });
        }
        if (step == 3) {
            const auto selected = std::ranges::
                find(snapshot.choices, true, &game_client::interaction_choice::highlighted);
            REQUIRE(selected != snapshot.choices.end());
            CHECK(selected->selected_count == 3);
            CHECK(uistate.read_recipes.count(pipe.ident()) == 1);
            const auto five =
                std::ranges::find(snapshot.choices, std::uint64_t{5}, [](const auto& choice) {
                    return choice.selected_count.value_or(0);
                });
            REQUIRE(five != snapshot.choices.end());
            step = 4;
            return resolve({
                .input_id = snapshot.input_id,
                .operation = game_client::interaction_operation::set_count,
                .target_id = five->id,
                .count = 5,
            });
        }
        if (step == 4) {
            const auto selected = std::ranges::
                find(snapshot.choices, true, &game_client::interaction_choice::highlighted);
            REQUIRE(selected != snapshot.choices.end());
            CHECK(selected->selected_count == 5);
            CHECK(snapshot.message.contains("Batch"));
            step = 5;
            return resolve_action("CYCLE_BATCH");
        }
        REQUIRE(step == 5);
        const auto selected = std::ranges::
            find(snapshot.choices, true, &game_client::interaction_choice::highlighted);
        REQUIRE(selected != snapshot.choices.end());
        CHECK(selected->label.find(pipe.result_name()) != std::string::npos);
        CHECK_FALSE(snapshot.message.contains("Batch"));
        step = 6;
        return resolve({
            .input_id = snapshot.input_id,
            .operation = game_client::interaction_operation::cancel,
        });
    });

    you.craft(you.bub_pos());
    CHECK(step == 6);
    CHECK_FALSE(you.activity);
    CHECK(uistate.read_recipes.count(pipe.ident()) == 1);
}

TEST_CASE(
    "semantic crafting publishes tabs, search and recipe details",
    "[client][interaction][crafting][mcp]") {
    clear_all_state();
    const auto cleanup = on_out_of_scope([]() { clear_all_state(); });
    const auto guard = interaction_test_guard{};
    const auto restore_read = restore_on_out_of_scope<decltype(uistate.read_recipes)>(
        uistate.read_recipes);
    const auto restore_expanded = restore_on_out_of_scope<decltype(uistate.expanded_recipes)>(
        uistate.expanded_recipes);
    auto& you = get_avatar();
    const auto& pipe = recipe_id("test_pipe").obj();
    prepare_crafting_fixture();

    const auto choose =
        [](const game_client::interaction_snapshot& snapshot, const std::string& id) {
            REQUIRE(
                std::ranges::contains(snapshot.choices, id, &game_client::interaction_choice::id));
            return resolve({
                .input_id = snapshot.input_id,
                .operation = game_client::interaction_operation::choose,
                .target_id = id,
            });
        };
    const auto selected =
        [](const game_client::interaction_snapshot& snapshot, const std::string& prefix) {
            return snapshot.choices | std::views::filter([&](const auto& choice) {
                       return choice.selected && choice.id.starts_with(prefix);
                   })
                 | std::views::transform(&game_client::interaction_choice::id)
                 | std::ranges::to<std::vector>();
        };
    const auto pipe_row = [&](const game_client::interaction_snapshot& snapshot) {
        return std::ranges::find_if(snapshot.choices, [&](const auto& choice) {
            return choice.id.starts_with("recipe:")
                && choice.label.find(pipe.result_name()) != std::string::npos;
        });
    };

    auto step = 0;
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto snapshot = game_client::current_interaction({.limit = 200});
        REQUIRE(snapshot.context == "CRAFTING");
        const auto recipes = recipe_rows(snapshot).choices;
        switch (step++) {
            case 0: {
                // Tab rows have no pane; exactly one is selected; the search is a field.
                CHECK(
                    std::ranges::count_if(
                        snapshot.choices,
                        [](const auto& choice) { return choice.id.starts_with("tab:"); })
                    > 1);
                CHECK(std::ranges::none_of(snapshot.choices, [](const auto& choice) {
                    return choice.id.starts_with("tab:") && choice.pane_id;
                }));
                CHECK(selected(snapshot, "tab:").size() == 1);
                REQUIRE(snapshot.field);
                CHECK(snapshot.field->id == "filter");
                CHECK(snapshot.field->value.empty());
                CHECK_FALSE(snapshot.field->description.empty());
                CHECK_FALSE(snapshot.field->description.contains("<color"));
                return choose(snapshot, "tab:CC_ELECTRONIC");
            }
            case 1: {
                CHECK(selected(snapshot, "tab:") == std::vector<std::string>{"tab:CC_ELECTRONIC"});
                CHECK(std::ranges::all_of(recipes, [](const auto& choice) {
                    return choice.pane_id == "CC_ELECTRONIC";
                }));
                // Details come with the row, without color markup.
                const auto row = pipe_row(snapshot);
                REQUIRE(row != snapshot.choices.end());
                CHECK(row->description.contains("Time to complete"));
                CHECK_FALSE(row->description.contains("<color"));
                return choose(snapshot, "subtab:CSC_ELECTRONIC_TOOLS");
            }
            case 2: {
                CHECK(selected(snapshot, "subtab:")
                      == std::vector<std::string>{"subtab:CSC_ELECTRONIC_TOOLS"});
                CHECK(pipe_row(snapshot) != snapshot.choices.end());
                return resolve({
                    .input_id = snapshot.input_id,
                    .operation = game_client::interaction_operation::fill,
                    .target_id = "filter",
                    .value = "no recipe is named like this",
                    .submit = true,
                });
            }
            case 3: {
                // A search lists across the tab and freezes the sub-tabs, as native does.
                CHECK(recipes.empty());
                CHECK(snapshot.field->value == "no recipe is named like this");
                CHECK(std::ranges::none_of(snapshot.choices, [](const auto& choice) {
                    return choice.id.starts_with("subtab:");
                }));
                return resolve({
                    .input_id = snapshot.input_id,
                    .operation = game_client::interaction_operation::fill,
                    .target_id = "filter",
                    .value = "",
                    .submit = true,
                });
            }
            default: {
                CHECK(pipe_row(snapshot) != snapshot.choices.end());
                return resolve({
                    .input_id = snapshot.input_id,
                    .operation = game_client::interaction_operation::cancel,
                });
            }
        }
    });

    you.craft(you.bub_pos());
    CHECK(step == 5);
    CHECK_FALSE(you.activity);
}

TEST_CASE(
    "semantic crafting cancel leaves normal and batch selections untouched",
    "[client][interaction][crafting][mcp]") {
    clear_all_state();
    const auto cleanup = on_out_of_scope([]() { clear_all_state(); });
    const auto guard = interaction_test_guard{};
    const auto restore_read = restore_on_out_of_scope<decltype(uistate.read_recipes)>(
        uistate.read_recipes);
    const auto restore_expanded = restore_on_out_of_scope<decltype(uistate.expanded_recipes)>(
        uistate.expanded_recipes);
    auto& you = get_avatar();
    const auto& recipe = recipe_id("test_soldering_iron").obj();
    prepare_crafting_fixture();
    auto& here = get_map();
    here.add_item_or_charges(you.bub_pos(), item::spawn("test_rock"));
    const auto e_scrap_before = you.amount_of(itype_id("e_scrap"));
    const auto ground_before = here.i_at(you.bub_pos()).size();

    const auto run_cancel = [&](const bool batch) {
        auto step = 0;
        game_client::memory::set_input_provider([&](const int /*timeout*/) {
            const auto snapshot = recipe_rows(game_client::current_interaction({.limit = 100}));
            if (snapshot.context != "CRAFTING") {
                REQUIRE(step == 1);
                step = 2;
                return resolve({
                    .input_id = snapshot.input_id,
                    .operation = game_client::interaction_operation::fill,
                    .target_id = snapshot.field->id,
                    .value = recipe.result_name(),
                    .submit = true,
                });
            }
            if (step == 0) {
                step = 1;
                return resolve_action("FILTER");
            }
            if (batch && step == 2) {
                REQUIRE(snapshot.choices.size() == 1);
                step = 3;
                return resolve({
                    .input_id = snapshot.input_id,
                    .operation = game_client::interaction_operation::set_count,
                    .target_id = snapshot.choices.front().id,
                    .count = 2,
                });
            }
            CHECK(step == (batch ? 3 : 2));
            step = 4;
            return resolve({
                .input_id = snapshot.input_id,
                .operation = game_client::interaction_operation::cancel,
            });
        });
        you.craft(you.bub_pos());
        CHECK(step == 4);
    };

    SECTION("normal") { run_cancel(false); }
    SECTION("batch") { run_cancel(true); }

    CHECK_FALSE(you.activity);
    CHECK(you.amount_of(itype_id("e_scrap")) == e_scrap_before);
    CHECK(you.amount_of(itype_id("test_soldering_iron")) == 0);
    CHECK(here.i_at(you.bub_pos()).size() == ground_before);
}

TEST_CASE(
    "semantic unavailable crafting choice follows native denial without mutation",
    "[client][interaction][crafting][mcp]") {
    clear_all_state();
    const auto cleanup = on_out_of_scope([]() { clear_all_state(); });
    const auto guard = interaction_test_guard{};
    const auto restore_read = restore_on_out_of_scope<decltype(uistate.read_recipes)>(
        uistate.read_recipes);
    const auto restore_expanded = restore_on_out_of_scope<decltype(uistate.expanded_recipes)>(
        uistate.expanded_recipes);
    auto& you = get_avatar();
    add_crafting_fixture_item(you, itype_id("copper"), 3);
    auto& here = get_map();
    here.add_item_or_charges(you.bub_pos(), item::spawn("test_rock"));
    const auto material_before = you.amount_of(itype_id("copper"));
    const auto ground_before = here.i_at(you.bub_pos()).size();
    const auto& unavailable = recipe_id("test_pipe").obj();

    auto step = 0;
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto snapshot = recipe_rows(game_client::current_interaction({.limit = 100}));
        if (snapshot.context == "CRAFTING") {
            if (step == 0) {
                step = 1;
                return resolve_action("TOGGLE_UNAVAILABLE");
            }
            if (step == 1) {
                step = 2;
                return resolve_action("FILTER");
            }
            if (step == 3) {
                REQUIRE(snapshot.choices.size() == 1);
                const auto& choice = snapshot.choices.front();
                CHECK(choice.label.find(unavailable.result_name()) != std::string::npos);
                CHECK_FALSE(choice.enabled);
                CHECK(choice.selectable);
                CHECK_FALSE(choice.denial.empty());
                step = 4;
                return resolve({
                    .input_id = snapshot.input_id,
                    .operation = game_client::interaction_operation::choose,
                    .target_id = choice.id,
                });
            }
            REQUIRE(step == 5);
            step = 6;
            return resolve({
                .input_id = snapshot.input_id,
                .operation = game_client::interaction_operation::cancel,
            });
        }
        if (step == 2) {
            REQUIRE(snapshot.field);
            step = 3;
            return resolve({
                .input_id = snapshot.input_id,
                .operation = game_client::interaction_operation::fill,
                .target_id = snapshot.field->id,
                .value = unavailable.result_name(),
                .submit = true,
            });
        }
        REQUIRE(step == 4);
        CHECK(snapshot.message.contains("You can't do that"));
        REQUIRE(snapshot.choices.size() == 1);
        step = 5;
        return resolve({
            .input_id = snapshot.input_id,
            .operation = game_client::interaction_operation::choose,
            .target_id = snapshot.choices.front().id,
        });
    });

    you.craft(you.bub_pos());
    CHECK(step == 6);
    CHECK_FALSE(you.activity);
    CHECK(you.amount_of(itype_id("copper")) == material_before);
    CHECK(you.amount_of(itype_id("test_pipe")) == 0);
    CHECK(here.i_at(you.bub_pos()).size() == ground_before);
}

TEST_CASE(
    "semantic nested crafting choice expands and rejects quantities",
    "[client][interaction][crafting][mcp]") {
    clear_all_state();
    const auto cleanup = on_out_of_scope([]() { clear_all_state(); });
    const auto guard = interaction_test_guard{};
    const auto nested_categories = override_option("ENABLE_NESTED_CATEGORIES", "true");
    const auto restore_read = restore_on_out_of_scope<decltype(uistate.read_recipes)>(
        uistate.read_recipes);
    const auto restore_expanded = restore_on_out_of_scope<decltype(uistate.expanded_recipes)>(
        uistate.expanded_recipes);
    uistate.expanded_recipes.clear();
    auto& you = get_avatar();
    prepare_crafting_fixture();
    const auto& nested = recipe_id("test_semantic_recipes").obj();

    auto visited = std::set<std::string>{};
    auto choice_count_before = std::size_t{0};
    auto show_unavailable = false;
    auto expanded = false;
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto snapshot = game_client::current_interaction({.limit = 200});
        REQUIRE(snapshot.context == "CRAFTING");
        if (!expanded) {
            const auto category = std::ranges::find_if(snapshot.choices, [&](const auto& choice) {
                return choice.label.find(nested.result_name()) != std::string::npos;
            });
            if (category == snapshot.choices.end()) {
                if (!show_unavailable) {
                    show_unavailable = true;
                    return resolve_action("TOGGLE_UNAVAILABLE");
                }
                return resolve_action(
                    visited.insert(snapshot.message).second ? "RIGHT" : "NEXT_TAB");
            }
            CHECK_FALSE(category->selected_count);
            CHECK_FALSE(category->available_count);
            const auto rejected = game_client::resolve_interaction_command({
                .input_id = snapshot.input_id,
                .operation = game_client::interaction_operation::set_count,
                .target_id = category->id,
                .count = 2,
            });
            REQUIRE_FALSE(rejected);
            CHECK(rejected.error().starts_with("invalid:"));
            choice_count_before = snapshot.choices.size();
            expanded = true;
            return resolve({
                .input_id = snapshot.input_id,
                .operation = game_client::interaction_operation::choose,
                .target_id = category->id,
            });
        }
        CHECK(uistate.expanded_recipes.count(nested.ident()) == 1);
        CHECK(snapshot.choices.size() >= choice_count_before + 2);
        CHECK(
            std::ranges::count_if(
                snapshot.choices,
                [&](const auto& choice) {
                    return choice.label.find("TEST soldering iron") != std::string::npos
                        || choice.label.find("TEST pipe") != std::string::npos;
                })
            == 2);
        return resolve({
            .input_id = snapshot.input_id,
            .operation = game_client::interaction_operation::cancel,
        });
    });

    you.craft(you.bub_pos());
    CHECK(expanded);
    CHECK_FALSE(you.activity);
}

TEST_CASE(
    "crafting rejects filtered and stale semantic recipe identities without mutation",
    "[client][interaction][crafting][mcp]") {
    clear_all_state();
    const auto cleanup = on_out_of_scope([]() { clear_all_state(); });
    const auto guard = interaction_test_guard{};
    const auto restore_read = restore_on_out_of_scope<decltype(uistate.read_recipes)>(
        uistate.read_recipes);
    const auto restore_expanded = restore_on_out_of_scope<decltype(uistate.expanded_recipes)>(
        uistate.expanded_recipes);
    auto& you = get_avatar();
    prepare_crafting_fixture();
    const auto material_before = you.amount_of(itype_id("e_scrap"));
    auto old_id = std::string{};
    auto old_input_id = std::uint64_t{0};
    auto step = 0;

    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto snapshot = recipe_rows(game_client::current_interaction({.limit = 100}));
        if (snapshot.context != "CRAFTING") {
            REQUIRE(snapshot.field);
            if (step == 1) {
                step = 2;
                return resolve({
                    .input_id = snapshot.input_id,
                    .operation = game_client::interaction_operation::fill,
                    .target_id = snapshot.field->id,
                    .value = "TEST",
                    .submit = true,
                });
            }
            REQUIRE(step == 3);
            step = 4;
            return resolve({
                .input_id = snapshot.input_id,
                .operation = game_client::interaction_operation::fill,
                .target_id = snapshot.field->id,
                .value = "__no_semantic_recipe__",
                .submit = true,
            });
        }
        if (step == 0) {
            step = 1;
            return resolve_action("FILTER");
        }
        if (step == 2) {
            REQUIRE(snapshot.choices.size() == 2);
            old_id = snapshot.choices.back().id;
            old_input_id = snapshot.input_id;
            step = 3;
            return resolve_action("FILTER");
        }
        REQUIRE(step == 4);
        CHECK(snapshot.choices.empty());
        const auto removed = game_client::resolve_interaction_command({
            .input_id = snapshot.input_id,
            .operation = game_client::interaction_operation::choose,
            .target_id = old_id,
        });
        REQUIRE_FALSE(removed);
        CHECK(removed.error().starts_with("invalid:"));
        const auto stale = game_client::resolve_interaction_command({
            .input_id = old_input_id,
            .operation = game_client::interaction_operation::choose,
            .target_id = old_id,
        });
        REQUIRE_FALSE(stale);
        CHECK(stale.error().starts_with("stale:"));
        step = 5;
        return resolve({
            .input_id = snapshot.input_id,
            .operation = game_client::interaction_operation::cancel,
        });
    });

    you.craft(you.bub_pos());
    CHECK(step == 5);
    CHECK_FALSE(you.activity);
    CHECK(you.amount_of(itype_id("e_scrap")) == material_before);
    CHECK(you.amount_of(itype_id("test_pipe")) == 0);
    CHECK(you.amount_of(itype_id("test_soldering_iron")) == 0);
}

TEST_CASE(
    "crafting replay rejects a changed real recipe schema without mutation",
    "[client][interaction][crafting][replay][mcp]") {
    clear_all_state();
    const auto cleanup_state = on_out_of_scope([]() { clear_all_state(); });
    const auto guard = interaction_test_guard{};
    const auto restore_read = restore_on_out_of_scope<decltype(uistate.read_recipes)>(
        uistate.read_recipes);
    const auto restore_expanded = restore_on_out_of_scope<decltype(uistate.expanded_recipes)>(
        uistate.expanded_recipes);
    const auto restore_hidden = restore_on_out_of_scope<decltype(uistate.hidden_recipes)>(
        uistate.hidden_recipes);
    static auto counter = std::atomic_uint64_t{0};
    const auto directory =
        std::filesystem::path(PATH_INFO::user_dir())
        / ("semantic-crafting-schema-" + std::to_string(counter.fetch_add(1)));
    REQUIRE(std::filesystem::create_directory(directory));
    const auto cleanup_replay = on_out_of_scope([&] {
        replay::stop();
        std::filesystem::remove_all(directory);
    });
    const auto path = directory / "input.jsonl";
    prepare_crafting_fixture();

    replay::configure_recording(path.string(), {.rng_seed = 31});
    replay::start();
    auto reads = 0;
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto snapshot = recipe_rows(game_client::current_interaction({.limit = 100}));
        if (snapshot.context != "CRAFTING") {
            return resolve({
                .input_id = snapshot.input_id,
                .operation = game_client::interaction_operation::fill,
                .target_id = snapshot.field->id,
                .value = "TEST",
                .submit = true,
            });
        }
        if (reads++ == 0) { return resolve_action("FILTER"); }
        REQUIRE(snapshot.choices.size() == 2);
        return resolve({
            .input_id = snapshot.input_id,
            .operation = game_client::interaction_operation::cancel,
        });
    });
    auto& recorded = get_avatar();
    recorded.craft(recorded.bub_pos());
    CHECK_FALSE(recorded.activity);
    replay::finish();

    game_client::memory::set_input_provider([](const int /*timeout*/) {
        return resolve_action("YES");
    });
    clear_all_state();
    prepare_crafting_fixture(false);
    uistate.hidden_recipes.insert(recipe_id("test_pipe"));
    const auto material_before = get_avatar().amount_of(itype_id("e_scrap"));
    game_client::memory::set_input_provider([](const int /*timeout*/) -> input_event {
        throw std::runtime_error("crafting schema playback must not read live input");
    });
    replay::configure_playback(path.string(), {.rng_seed = 31});
    replay::start();
    CHECK_THROWS_WITH(
        get_avatar().craft(get_avatar().bub_pos()), Catch::Matchers::Contains("schema changed"));
    CHECK_FALSE(get_avatar().activity);
    CHECK(get_avatar().amount_of(itype_id("e_scrap")) == material_before);
    CHECK(get_avatar().amount_of(itype_id("test_pipe")) == 0);
}

TEST_CASE(
    "real crafting selector filters and starts a batch through semantic input",
    "[client][interaction][crafting][mcp]") {
    clear_all_state();
    const auto cleanup = on_out_of_scope([]() { clear_all_state(); });
    const auto guard = interaction_test_guard{};
    const auto restore_read = restore_on_out_of_scope<decltype(uistate.read_recipes)>(
        uistate.read_recipes);
    const auto restore_expanded = restore_on_out_of_scope<decltype(uistate.expanded_recipes)>(
        uistate.expanded_recipes);
    auto& you = get_avatar();
    const auto& recipe = recipe_id("test_soldering_iron").obj();
    prepare_crafting_fixture();

    auto crafting_reads = 0;
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto snapshot = recipe_rows(game_client::current_interaction({.limit = 100}));
        if (snapshot.context == "CRAFTING") {
            if (crafting_reads++ == 0) { return resolve_action("FILTER"); }
            const auto target =
                crafting_reads == 2
                    ? std::ranges::find_if(
                          snapshot.choices,
                          [&](const auto& choice) {
                              return choice.label.find(recipe.result_name()) != std::string::npos;
                          })
                    : std::ranges::find(
                          snapshot.choices, true, &game_client::interaction_choice::highlighted);
            REQUIRE(target != snapshot.choices.end());
            if (crafting_reads == 2) {
                CHECK(snapshot.choices.size() == 1);
                CHECK(target->enabled);
                CHECK(target->selectable);
                CHECK(target->minimum_count == 1);
                CHECK(target->available_count == 50);
                for (const auto count : {std::uint64_t{1}, std::uint64_t{50}}) {
                    CHECK(game_client::resolve_interaction_command({
                        .input_id = snapshot.input_id,
                        .operation = game_client::interaction_operation::set_count,
                        .target_id = target->id,
                        .count = count,
                    }));
                }
                for (const auto count : {std::uint64_t{0}, std::uint64_t{51}}) {
                    const auto rejected = game_client::resolve_interaction_command({
                        .input_id = snapshot.input_id,
                        .operation = game_client::interaction_operation::set_count,
                        .target_id = target->id,
                        .count = count,
                    });
                    REQUIRE_FALSE(rejected);
                    CHECK(rejected.error().starts_with("invalid:"));
                }
                return resolve({
                    .input_id = snapshot.input_id,
                    .operation = game_client::interaction_operation::set_count,
                    .target_id = target->id,
                    .count = 2,
                });
            }
            CHECK(snapshot.choices.size() == 50);
            CHECK(target->selected_count == 2);
            CHECK(target->highlighted);
            return resolve({
                .input_id = snapshot.input_id,
                .operation = game_client::interaction_operation::choose,
                .target_id = target->id,
            });
        }
        REQUIRE(snapshot.kind == game_client::interaction_kind::field);
        return resolve({
            .input_id = snapshot.input_id,
            .operation = game_client::interaction_operation::fill,
            .target_id = snapshot.field->id,
            .value = recipe.result_name(),
            .submit = true,
        });
    });

    you.craft(you.bub_pos());
    REQUIRE(you.activity);
    REQUIRE(you.activity->id() == activity_id("ACT_CRAFT"));
    CHECK(crafting_reads == 3);
    CHECK_FALSE(you.has_amount(itype_id("e_scrap"), 1));

    CHECK(you.amount_of(itype_id("e_scrap")) == 0);
    CHECK(you.amount_of(itype_id("copper")) == 0);
    CHECK(you.amount_of(itype_id("scrap")) == 0);
    CHECK(you.charges_of(itype_id("duct_tape")) == 0);
    CHECK(you.amount_of(itype_id("cable")) == 0);
    finish_crafting_activity(you);
    REQUIRE_FALSE(you.activity);
    you.invalidate_crafting_inventory();
    CHECK(you.amount_of(itype_id("test_soldering_iron")) == 2);
}

TEST_CASE(
    "semantic replay restores the real crafting filter and batch selection",
    "[client][interaction][crafting][replay][mcp]") {
    clear_all_state();
    const auto cleanup_state = on_out_of_scope([]() { clear_all_state(); });
    const auto guard = interaction_test_guard{};
    const auto restore_read = restore_on_out_of_scope<decltype(uistate.read_recipes)>(
        uistate.read_recipes);
    const auto restore_expanded = restore_on_out_of_scope<decltype(uistate.expanded_recipes)>(
        uistate.expanded_recipes);
    static auto counter = std::atomic_uint64_t{0};
    const auto directory =
        std::filesystem::path(PATH_INFO::user_dir())
        / ("semantic-crafting-replay-" + std::to_string(counter.fetch_add(1)));
    REQUIRE(std::filesystem::create_directory(directory));
    const auto cleanup_replay = on_out_of_scope([&] {
        replay::stop();
        std::filesystem::remove_all(directory);
    });
    const auto path = directory / "input.jsonl";
    const auto& recipe = recipe_id("test_soldering_iron").obj();
    prepare_crafting_fixture();

    replay::configure_recording(path.string(), {.rng_seed = 29});
    replay::start();
    auto reads = 0;
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto snapshot = game_client::current_interaction({.limit = 100});
        if (snapshot.context != "CRAFTING") {
            return resolve({
                .input_id = snapshot.input_id,
                .operation = game_client::interaction_operation::fill,
                .target_id = snapshot.field->id,
                .value = recipe.result_name(),
                .submit = true,
            });
        }
        if (reads++ == 0) { return resolve_action("FILTER"); }
        const auto target =
            reads == 2
                ? std::ranges::find_if(
                      snapshot.choices,
                      [&](const auto& choice) {
                          return choice.label.find(recipe.result_name()) != std::string::npos;
                      })
                : std::ranges::
                      find(snapshot.choices, true, &game_client::interaction_choice::highlighted);
        REQUIRE(target != snapshot.choices.end());
        return reads == 2
                 ? resolve({
                       .input_id = snapshot.input_id,
                       .operation = game_client::interaction_operation::set_count,
                       .target_id = target->id,
                       .count = 2,
                   })
                 : resolve({
                       .input_id = snapshot.input_id,
                       .operation = game_client::interaction_operation::choose,
                       .target_id = target->id,
                   });
    });
    auto& recorded = get_avatar();
    recorded.craft(recorded.bub_pos());
    REQUIRE(recorded.activity);
    REQUIRE(recorded.activity->id() == activity_id("ACT_CRAFT"));
    CHECK(recorded.amount_of(itype_id("e_scrap")) == 0);
    finish_crafting_activity(recorded);
    REQUIRE_FALSE(recorded.activity);
    CHECK(recorded.amount_of(itype_id("test_soldering_iron")) == 2);
    replay::finish();

    game_client::memory::set_input_provider([](const int /*timeout*/) {
        return resolve_action("YES");
    });
    clear_all_state();
    prepare_crafting_fixture();
    game_client::memory::set_input_provider([](const int /*timeout*/) -> input_event {
        throw std::runtime_error("crafting playback must not read live input");
    });
    replay::configure_playback(path.string(), {.rng_seed = 29});
    replay::start();
    auto& played = get_avatar();
    played.craft(played.bub_pos());
    REQUIRE(played.activity);
    REQUIRE(played.activity->id() == activity_id("ACT_CRAFT"));
    CHECK(played.amount_of(itype_id("e_scrap")) == 0);
    CHECK_THROWS_AS(replay::next_input_event(), replay::completed);
    finish_crafting_activity(played);
    REQUIRE_FALSE(played.activity);
    CHECK(played.amount_of(itype_id("test_soldering_iron")) == 2);
    CHECK(played.amount_of(itype_id("copper")) == 0);
    CHECK(played.amount_of(itype_id("scrap")) == 0);
    CHECK(played.charges_of(itype_id("duct_tape")) == 0);
    CHECK(played.amount_of(itype_id("cable")) == 0);
    replay::finish();
}

TEST_CASE("semantic count bounds include an optional minimum", "[client][interaction][mcp]") {
    const auto context = input_context("COUNT_BOUNDS");
    const auto input = game_client::input_context_scope(context, "COUNT_BOUNDS");
    auto minimum = std::optional<std::uint64_t>{};
    const auto interaction = game_client::interaction_scope(context, [&] {
        return game_client::interaction_snapshot{
            .kind = game_client::interaction_kind::choices,
            .choices = {{
                .id = "bounded",
                .label = "Bounded",
                .selected_count = minimum.value_or(0),
                .minimum_count = minimum,
                .available_count = 50,
            }},
            .allow_set_count = true,
        };
    });

    const auto inventory = game_client::current_interaction();
    CHECK(game_client::resolve_interaction_command({
        .input_id = inventory.input_id,
        .operation = game_client::interaction_operation::set_count,
        .target_id = "bounded",
        .count = 0,
    }));
    minimum = 1;
    const auto crafting = game_client::current_interaction();
    CHECK(crafting.schema_id != inventory.schema_id);
    for (const auto count : {std::uint64_t{1}, std::uint64_t{50}}) {
        CHECK(game_client::resolve_interaction_command({
            .input_id = crafting.input_id,
            .operation = game_client::interaction_operation::set_count,
            .target_id = "bounded",
            .count = count,
        }));
    }
    for (const auto count : {std::uint64_t{0}, std::uint64_t{51}}) {
        const auto result = game_client::resolve_interaction_command({
            .input_id = crafting.input_id,
            .operation = game_client::interaction_operation::set_count,
            .target_id = "bounded",
            .count = count,
        });
        REQUIRE_FALSE(result);
        CHECK(result.error().starts_with("invalid:"));
    }
}

TEST_CASE(
    "interaction providers do not leak into nested raw contexts", "[client][interaction][mcp]") {
    const auto outer = input_context("OUTER_INTERACTION");
    const auto inner = input_context("INNER_RAW");
    const auto outer_input = game_client::input_context_scope(outer, "OUTER_INTERACTION");
    const auto outer_interaction = game_client::interaction_scope(outer, [] {
        return game_client::interaction_snapshot{
            .kind = game_client::interaction_kind::choices,
            .choices = {{.id = "outer", .label = "Outer"}},
        };
    });
    CHECK(game_client::current_interaction().structured);

    REQUIRE_THROWS_AS(
        [&] {
            const auto inner_input = game_client::input_context_scope(inner, "INNER_RAW");
            CHECK_FALSE(game_client::current_interaction().structured);
            throw std::runtime_error("nested failure");
        }(),
        std::runtime_error);

    CHECK(game_client::current_interaction().structured);
    CHECK(game_client::current_interaction().choices.front().id == "outer");
}

TEST_CASE(
    "look around publishes its cursor and leaves on semantic cancel",
    "[client][interaction][mcp]") {
    const auto guard = interaction_test_guard{};
    clear_map();
    auto& you = get_avatar();
    clear_character(you, false);
    auto reads = 0;
    auto cursor = tripoint_bub_ms{};
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto snapshot = game_client::current_interaction();
        REQUIRE(snapshot.target);
        CHECK(snapshot.kind == game_client::interaction_kind::target);
        CHECK(snapshot.allow_cancel);
        cursor = {snapshot.target->cursor.x, snapshot.target->cursor.y, snapshot.target->cursor.z};
        if (reads++ == 0) { return resolve_action("RIGHT"); }
        return resolve({
            .input_id = snapshot.input_id,
            .operation = game_client::interaction_operation::cancel,
        });
    });
    const auto result = g->look_around();
    CHECK_FALSE(result);
    REQUIRE(reads == 2);
    CHECK(cursor == tripoint_bub_ms(you.bub_pos().x() + 1, you.bub_pos().y(), you.bub_pos().z()));
}

#endif // CATA_MCP
