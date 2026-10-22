#if defined(CATA_MCP)

#    include "advanced_inv.h"
#    include "avatar.h"
#    include "cached_options.h"
#    include "calendar.h"
#    include "cata_utility.h"
#    include "catch/catch.hpp"
#    include "client_input.h"
#    include "client_interaction.h"
#    include "client_memory.h"
#    include "client_memory_scope.h"
#    include "cursesdef.h"
#    include "cursesport.h"
#    include "engine_client_component.h"
#    include "engine_client_contract.h"
#    include "faction.h"
#    include "game.h"
#    include "game_constants.h"
#    include "input.h"
#    include "item.h"
#    include "map/map.h"
#    include "map_helpers.h"
#    include "npc.h"
#    include "options_helpers.h"
#    include "output.h"
#    include "path_info.h"
#    include "player_activity.h"
#    include "player_helpers.h"
#    include "replay/replay.h"
#    include "state_helpers.h"
#    include "string_formatter.h"
#    include "ui.h"
#    include "uistate.h"
#    include "units_utility.h"
#    include "vehicle/veh_type.h"
#    include "vehicle/vehicle.h"

#    include <atomic>
#    include <expected>
#    include <filesystem>
#    include <functional>
#    include <optional>
#    include <ranges>
#    include <stdexcept>
#    include <string>
#    include <utility>
#    include <variant>
#    include <vector>

namespace ec = engine_client;

namespace {

constexpr auto aim_test_pos = tripoint_bub_ms{60, 60, 0};

struct screen_guard {
    const game_client::memory::scoped_state memory;
    bool old_test_mode = test_mode;
    int old_termx = TERMX;
    int old_termy = TERMY;
    int old_full_screen_width = FULL_SCREEN_WIDTH;
    int old_full_screen_height = FULL_SCREEN_HEIGHT;

    explicit screen_guard(const int rows = 40) {
        test_mode = false;
        TERMX = 100;
        TERMY = rows;
        FULL_SCREEN_WIDTH = 80;
        FULL_SCREEN_HEIGHT = 24;
        game_client::memory::resize(TERMX, TERMY);
        catacurses::stdscr = catacurses::newwin(TERMY, TERMX, point_zero);
        catacurses::newscr = catacurses::newwin(TERMY, TERMX, point_zero);
    }

    ~screen_guard() {
        TERMX = old_termx;
        TERMY = old_termy;
        FULL_SCREEN_WIDTH = old_full_screen_width;
        FULL_SCREEN_HEIGHT = old_full_screen_height;
        test_mode = old_test_mode;
    }
};

struct aim_fixture {
    advanced_inv_save_state old_transfer_save = uistate.transfer_save;
    int old_container_location = uistate.adv_inv_container_location;
    int old_container_index = uistate.adv_inv_container_index;
    itype_id old_container_type = uistate.adv_inv_container_type;
    itype_id old_container_content_type = uistate.adv_inv_container_content_type;
    bool old_container_in_vehicle = uistate.adv_inv_container_in_vehicle;
    time_point old_turn = calendar::turn;

    aim_fixture() {
        const auto previous_test_mode = test_mode;
        test_mode = true;
        clear_all_state();
        clear_character(get_avatar(), false);
        test_mode = previous_test_mode;
        calendar::turn = calendar::turn_zero + 12_hours;
        auto& you = get_avatar();
        you.setpos(map_local_to_abs(get_map(), aim_test_pos));
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

    ~aim_fixture() {
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

auto ground_item_count(const itype_id& id) -> int {
    return std::ranges::count_if(get_map().i_at(aim_test_pos), [&](const auto* candidate) {
        return candidate->typeId() == id;
    });
}

auto ground_charges(const itype_id& id) -> int {
    auto result = 0;
    for (const auto* candidate : get_map().i_at(aim_test_pos)) {
        if (candidate->typeId() == id) { result += candidate->charges; }
    }
    return result;
}

auto add_to_inventory(const itype_id& id, const int count) -> void {
    std::ranges::for_each(std::views::iota(0, count), [&](const auto /*index*/) {
        get_avatar().i_add(item::spawn(id, calendar::turn));
    });
}

auto resolve(game_client::interaction_command command) -> input_event {
    auto input = game_client::input_command{};
    input.interaction = std::move(command);
    const auto result =
        game_client::resolve_input_command(input, game_client::memory::screen_size());
    REQUIRE(result.has_value());
    return *result;
}

auto resolve_action(const std::string& action) -> input_event {
    auto input = game_client::input_command{};
    input.action = action;
    const auto result =
        game_client::resolve_input_command(input, game_client::memory::screen_size());
    REQUIRE(result.has_value());
    return *result;
}

class test_authority final: public ec::command_authority {
public:
    auto current_permissions() const -> ec::command_permissions override {
        return {.accepts_interaction_commands = true, .accepts_registered_actions = true};
    }
};

auto capture() -> ec::boundary_state {
    const auto value = ec::capture_boundary(
        {.epoch = "epoch:test",
         .ready = {.phase = "waiting_for_input",
                   .accepts_interaction_commands = true,
                   .accepts_registered_actions = true}});
    REQUIRE(value);
    return *value;
}

/// The request a client sends for an affordance of `top`, expecting what its scope says.
auto request_of(
    const ec::boundary_state& state, const ec::screen_view& top, const std::string& affordance,
    ec::invoke_args args) -> ec::command_request {
    const auto found = std::ranges::find(top.affordances, affordance, &ec::affordance::id);
    REQUIRE(found != top.affordances.end());
    return {
        .epoch = "epoch:test",
        .expect = ec::scope_of(*found) == ec::expect_scope::screen
                    ? ec::expectation{.screen_id = top.screen_id}
                    : ec::expectation{.revision = 1, .boundary_id = state.id},
        .operation = ec::invoke_operation{.affordance_id = affordance, .args = std::move(args)}};
}

/// What the lifecycle makes of `request` against the live boundary: the input the screen reads, or
/// why not.
auto input_of(ec::command_request request) -> std::expected<input_event, ec::error> {
    const auto authority = test_authority{};
    auto lifecycle = ec::command_lifecycle{"epoch:test", authority};
    const auto submitted = lifecycle.submit(std::move(request));
    if (!submitted) { return std::unexpected(submitted.error()); }
    return lifecycle
        .validate({.epoch = "epoch:test", .sequence = 1, .revision = 1}, capture(), point{80, 24});
}

auto aim_input(const std::string& affordance, ec::invoke_args args = {})
    -> std::expected<input_event, ec::error> {
    const auto state = capture();
    return input_of(request_of(state, ec::top_screen(state), affordance, std::move(args)));
}

auto aim_event(const std::string& affordance, ec::invoke_args args = {}) -> input_event {
    const auto event = aim_input(affordance, std::move(args));
    REQUIRE(event);
    return *event;
}

auto list_rows(const ec::screen_view& top, const std::string& id) -> std::vector<ec::row> {
    REQUIRE(top.root);
    const auto* const found = ec::find_component(*top.root, id);
    REQUIRE(found != nullptr);
    return std::get<ec::list_body>(found->body).rows;
}

auto panel_of(const ec::screen_view& top, const std::string& id) -> ec::panel_body {
    REQUIRE(top.root);
    const auto* const found = ec::find_component(*top.root, id);
    REQUIRE(found != nullptr);
    return std::get<ec::panel_body>(found->body);
}

auto stat_of(const ec::panel_body& panel, const std::string& key) -> ec::panel_stat {
    const auto found = std::ranges::find(panel.stats, key, &ec::panel_stat::key);
    REQUIRE(found != panel.stats.end());
    return *found;
}

auto ids_of(const std::vector<ec::row>& rows) -> std::vector<std::string> {
    return rows | std::views::transform(&ec::row::id) | std::ranges::to<std::vector>();
}

/// The cells of a pane's area grid.
auto area_cells(const ec::screen_view& top, const std::string& id) -> std::vector<ec::grid_cell> {
    REQUIRE(top.root);
    const auto* const found = ec::find_component(*top.root, id);
    REQUIRE(found != nullptr);
    REQUIRE(std::holds_alternative<ec::grid_body>(found->body));
    return std::get<ec::grid_body>(found->body).cells;
}

auto ids_of(const std::vector<ec::grid_cell>& cells) -> std::vector<std::string> {
    return cells | std::views::transform(&ec::grid_cell::id) | std::ranges::to<std::vector>();
}

/// The id of the one row of `list` that stands for `type`.
auto row_of(const ec::screen_view& top, const std::string& list, const std::string& type)
    -> std::string {
    const auto rows = list_rows(top, list);
    const auto found = std::ranges::find_if(rows, [&](const ec::row& row) {
        return row.look && row.look->id == type;
    });
    REQUIRE(found != rows.end());
    return found->id;
}

/// What a screen does at each of its reads, one step per read; the screen is closed after the last.
struct script {
    using step = std::function<auto(const ec::screen_view&)->input_event>;
    std::vector<step> steps;
    /// The messages of the popups the screen raised, which are dismissed.
    std::vector<std::string> popups;
    /// The input for a read that is neither the screen nor a popup.
    std::function<auto(const game_client::interaction_snapshot&)->input_event> other;
    std::size_t reads = 0;
};

auto run_script(script& plan) -> void {
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto snapshot = game_client::current_interaction({.limit = 200});
        if (snapshot.context == "POPUP_WAIT") {
            plan.popups.push_back(snapshot.message);
            REQUIRE(snapshot.choices.size() == 1);
            return resolve({
                .input_id = snapshot.input_id,
                .operation = game_client::interaction_operation::choose,
                .target_id = snapshot.choices.front().id,
            });
        }
        const auto top = ec::top_screen(capture());
        if (top.context != "ADVANCED_INVENTORY") {
            REQUIRE(plan.other);
            return plan.other(snapshot);
        }
        if (plan.reads < plan.steps.size()) { return plan.steps[plan.reads++](top); }
        return aim_event("aim:close");
    });
    create_advanced_inv();
}

auto invoke_step(
    const std::string& affordance,
    std::function<auto(const ec::screen_view&)->ec::invoke_args> args = {}) -> script::step {
    return [=](const ec::screen_view& top) {
        return aim_event(affordance, args ? args(top) : ec::invoke_args{});
    };
}

auto key_action(const std::string& id) -> script::step {
    return [=](const ec::screen_view& /*top*/) { return resolve_action(id); };
}

/// Moves the rows of `types` from the pane `from` to the pane `to`, with the counts given.
auto move_rows(
    const std::string& from, const std::string& to,
    const std::vector<std::pair<std::string, std::optional<std::uint64_t>>>& types)
    -> script::step {
    return invoke_step("aim:move", [=](const ec::screen_view& top) {
        auto rows = std::vector<ec::invoke_row>{};
        for (const auto& [type, count] : types) {
            rows.push_back({.id = row_of(top, from + ":items", type), .count = count});
        }
        return ec::invoke_args{.rows = std::move(rows), .pane = to};
    });
}

const auto area_names = std::vector<std::string>{
    "area:INVENTORY", "area:SW",  "area:S",         "area:SE",  "area:W",     "area:CENTER",
    "area:E",         "area:NW",  "area:N",         "area:NE",  "area:ABOVE", "area:BELOW",
    "area:DRAGGED",   "area:ALL", "area:CONTAINER", "area:WORN"};

auto fixture_with_cart() -> vehicle* {
    auto& here = get_map();
    const auto cart_pos = aim_test_pos + tripoint_east;
    auto* const cart = here.add_vehicle(vproto_id("shopping_cart"), cart_pos, 0_degrees, 0, 0);
    REQUIRE(cart != nullptr);
    const auto cargo = cart->part_with_feature(tripoint_mnt_veh::zero(), "CARGO", true);
    REQUIRE(cargo >= 0);
    cart->get_items(cargo).clear();
    REQUIRE_FALSE(cart->add_item(cargo, item::spawn("test_1kg_cube", calendar::turn)));
    here.add_item(cart_pos, item::spawn("test_1kg_cube", calendar::turn));
    uistate.transfer_save.saved_area = AIM_ALL;
    uistate.transfer_save.pane.area_idx = AIM_ALL;
    return cart;
}

} // namespace

TEST_CASE(
    "advanced inventory publishes both panes, the areas and the rows by identity",
    "[advanced_inventory][client][mcp]") {
    const auto guard = screen_guard{};
    const auto fixture = aim_fixture{};
    auto& you = get_avatar();
    add_to_inventory(itype_id("test_1kg_cube"), 3);
    get_map().add_item(aim_test_pos, item::spawn("test_platinum_bit", calendar::turn, 10));
    auto plan = script{};
    plan.steps.push_back([&](const ec::screen_view& top) {
        REQUIRE(top.root);
        CHECK(top.role == "screen");
        CHECK(ec::component_kind(*top.root) == "split");

        const auto areas = area_cells(top, "aim:pane:left:areas");
        CHECK(ids_of(areas) == area_names);
        CHECK(std::ranges::count(areas, true, &ec::grid_cell::selected) == 1);
        CHECK(areas.front().selected);
        const auto area = [&](const std::string& id) {
            const auto found = std::ranges::find(areas, id, &ec::grid_cell::id);
            REQUIRE(found != areas.end());
            return *found;
        };
        // Each area sits where the native header prints its key, and picking it shows it.
        CHECK(area("area:INVENTORY").at == ec::grid_point{1, 1});
        CHECK(area("area:INVENTORY").glyph == "I");
        CHECK(area("area:INVENTORY").label == "Inventory");
        CHECK(area("area:N").at == ec::grid_point{4, 0});
        CHECK(area("area:N").glyph == "8");
        CHECK(area("area:SW").at == ec::grid_point{3, 2});
        CHECK(area("area:ALL").at == ec::grid_point{0, 2});
        CHECK(area("area:BELOW").at == ec::grid_point{6, 1});
        CHECK(std::ranges::all_of(areas, [](const ec::grid_cell& cell) {
            return cell.affordance == "aim:set_area";
        }));
        // With no container or grabbed vehicle around those areas cannot take items.
        CHECK_FALSE(area("area:CONTAINER").enabled);
        CHECK_FALSE(area("area:CONTAINER").denial.empty());
        CHECK_FALSE(area("area:DRAGGED").enabled);
        CHECK(area("area:CENTER").enabled);
        CHECK(area("area:CENTER").denial.empty());

        const auto& panes = *ec::find_component(*top.root, "aim:panes");
        CHECK(std::get<ec::split_body>(panes.body).active == "aim:pane:left");
        const auto left = list_rows(top, "aim:pane:left:items");
        REQUIRE(left.size() == 1);
        CHECK(left.front().id.starts_with("stack:"));
        CHECK(left.front().max == 3);
        CHECK(left.front().look.has_value());
        CHECK_FALSE(left.front().description.empty());
        const auto amount = std::ranges::
            find(left.front().columns, "amount", &game_client::interaction_column::key);
        REQUIRE(amount != left.front().columns.end());
        CHECK(amount->value == "3");
        const auto right = list_rows(top, "aim:pane:right:items");
        REQUIRE(right.size() == 1);
        CHECK(right.front().id.starts_with("item:"));
        CHECK(right.front().max == 10);
        // Only the pane the player works in takes counts.
        const auto selection = [&](const std::string& id) {
            return std::get<ec::list_body>(ec::find_component(*top.root, id)->body).selection;
        };
        CHECK(selection("aim:pane:left:items") == "count");
        CHECK(selection("aim:pane:right:items") == "none");
        CHECK(top.focus_component == "aim:pane:left:items");
        CHECK(top.focus_row == left.front().id);

        const auto affordance = [&](const std::string& id) {
            const auto found = std::ranges::find(top.affordances, id, &ec::affordance::id);
            REQUIRE(found != top.affordances.end());
            return *found;
        };
        CHECK(affordance("aim:move").enabled);
        CHECK(affordance("aim:move").args.rows->counts);
        CHECK(affordance("aim:move").args.rows->component_id == "aim:pane:left:items");
        CHECK(affordance("aim:close").role == "cancel");
        CHECK(std::ranges::count(top.affordances, "cancel", &ec::affordance::role) == 1);
        for (const auto* const id : {"aim:set_area", "aim:toggle_vehicle", "aim:filter"}) {
            CHECK(affordance(id).enabled);
        }
        // The registered actions the screen's own affordances perform are not offered beside them.
        for (const auto* const id : {"QUIT", "FILTER", "TOGGLE_VEH", "MOVE_VARIABLE_ITEM"}) {
            CAPTURE(id);
            CHECK(std::ranges::find(top.affordances, id, &ec::affordance::id)
                  == top.affordances.end());
        }
        // Looking at the screen changes nothing.
        CHECK(you.amount_of(itype_id("test_1kg_cube")) == 3);
        return aim_event("aim:close");
    });

    run_script(plan);

    CHECK(plan.reads == 1);
    CHECK(plan.popups.empty());
    CHECK_FALSE(you.activity);
    CHECK(you.amount_of(itype_id("test_1kg_cube")) == 3);
    CHECK(ground_charges(itype_id("test_platinum_bit")) == 10);
}

TEST_CASE(
    "advanced inventory rows and their ids are the same at any window size",
    "[advanced_inventory][client][mcp]") {
    const auto fixture = aim_fixture{};
    add_to_inventory(itype_id("test_1kg_cube"), 2);
    for (const auto* const type :
         {"knife_steak", "test_screwdriver", "e_scrap", "copper", "scrap"}) {
        add_to_inventory(itype_id(type), 1);
    }
    uistate.transfer_save.pane.sort_idx = SORTBY_CATEGORY;
    auto seen = std::vector<std::vector<std::string>>{};
    for (const auto rows : {40, 12}) {
        CAPTURE(rows);
        // The shorter window pages the pane every few rows and puts a category header on each page.
        const auto guard = screen_guard{rows};
        auto plan = script{};
        plan.steps.push_back([&](const ec::screen_view& top) {
            const auto listed = list_rows(top, "aim:pane:left:items");
            seen.push_back(ids_of(listed));
            CHECK(std::ranges::count(listed, true, &ec::row::header) >= 2);
            CHECK(std::ranges::none_of(listed, [](const ec::row& row) {
                return row.header && row.selectable;
            }));
            for (const auto& id : seen.back()) {
                CAPTURE(id);
                // Ids come from identity: no translated name, no spaces.
                CHECK(std::ranges::all_of(id, [](const char c) {
                    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')
                        || (c >= '0' && c <= '9') || c == '_' || c == ':';
                }));
            }
            return aim_event("aim:close");
        });
        run_script(plan);
        REQUIRE(plan.reads == 1);
    }
    REQUIRE(seen.size() == 2);
    CHECK(seen.front() == seen.back());
}

TEST_CASE(
    "advanced inventory tells ground storage from vehicle cargo by id",
    "[advanced_inventory][client][mcp]") {
    const auto guard = screen_guard{};
    const auto fixture = aim_fixture{};
    auto* const cart = fixture_with_cart();
    auto plan = script{};
    plan.steps.push_back([&](const ec::screen_view& top) {
        const auto rows = list_rows(top, "aim:pane:left:items");
        REQUIRE(rows.size() == 2);
        const auto ids = ids_of(rows);
        CHECK(ids.front() != ids.back());
        // A pane that shows several squares names where each stack is.
        for (const auto& row : rows) {
            const auto source =
                std::ranges::find(row.columns, "source", &game_client::interaction_column::key);
            REQUIRE(source != row.columns.end());
            CHECK(source->value == "East");
        }
        return aim_event("aim:close");
    });

    run_script(plan);

    CHECK_FALSE(get_avatar().activity);
    CHECK(ground_item_count(itype_id("test_1kg_cube")) == 0);
    CHECK(cart->get_items(cart->part_with_feature(tripoint_mnt_veh::zero(), "CARGO", true)).size()
          == 1);
}

TEST_CASE(
    "advanced inventory shows the area an invoke picks and refuses one it cannot take",
    "[advanced_inventory][client][mcp]") {
    const auto guard = screen_guard{};
    const auto fixture = aim_fixture{};
    add_to_inventory(itype_id("test_1kg_cube"), 1);
    auto plan = script{};
    plan.steps.push_back(invoke_step("aim:set_area", [](const ec::screen_view&) {
        return ec::invoke_args{.row = "area:E", .pane = "aim:pane:left"};
    }));
    plan.steps.push_back([&](const ec::screen_view& top) {
        const auto areas = area_cells(top, "aim:pane:left:areas");
        const auto selected = std::ranges::find(areas, true, &ec::grid_cell::selected);
        REQUIRE(selected != areas.end());
        CHECK(selected->id == "area:E");
        // The area shows the ground, not a vehicle's cargo, so it keeps its key.
        CHECK(selected->glyph == "6");
        CHECK(panel_of(top, "aim:pane:left").subtitle.starts_with("East"));
        CHECK(list_rows(top, "aim:pane:left:items").empty());
        // An area the pane cannot put items in is no row to show: the invoke is refused and the
        // pane stays where it was.
        const auto areas_now = area_cells(top, "aim:pane:left:areas");
        const auto container = std::ranges::find(areas_now, "area:CONTAINER", &ec::grid_cell::id);
        REQUIRE(container != areas_now.end());
        CHECK_FALSE(container->enabled);
        const auto refused =
            aim_input("aim:set_area", {.row = "area:CONTAINER", .pane = "aim:pane:left"});
        REQUIRE_FALSE(refused);
        CHECK(refused.error() == ec::error::invalid_arguments);
        return aim_event("aim:close");
    });

    run_script(plan);

    CHECK(plan.reads == 2);
    CHECK(plan.popups.empty());
    CHECK_FALSE(get_avatar().activity);
}

TEST_CASE(
    "advanced inventory filters one pane and refuses removed and stale rows",
    "[advanced_inventory][client][mcp]") {
    const auto guard = screen_guard{};
    const auto fixture = aim_fixture{};
    add_to_inventory(itype_id("test_1kg_cube"), 1);
    get_map().add_item(aim_test_pos, item::spawn("test_platinum_bit", calendar::turn, 10));
    auto removed = std::string{};
    auto old_state = std::optional<ec::boundary_state>{};
    auto plan = script{};
    plan.steps.push_back([&](const ec::screen_view& top) {
        removed = row_of(top, "aim:pane:left:items", "test_1kg_cube");
        old_state = capture();
        return aim_event(
            "aim:filter", {.text = "__no_advanced_inventory_match__", .pane = "aim:pane:left"});
    });
    plan.steps.push_back([&](const ec::screen_view& top) {
        CHECK(panel_of(top, "aim:pane:left").filter == "__no_advanced_inventory_match__");
        CHECK(panel_of(top, "aim:pane:right").filter.empty());
        CHECK(list_rows(top, "aim:pane:left:items").empty());
        CHECK(list_rows(top, "aim:pane:right:items").size() == 1);
        const auto state = capture();
        const auto args = ec::invoke_args{
            .rows = std::vector<ec::invoke_row>{{.id = removed}}, .pane = "aim:pane:right"};
        // The row the filter removed is not the list's any more.
        const auto gone = input_of(request_of(state, top, "aim:move", args));
        REQUIRE_FALSE(gone);
        CHECK(gone.error() == ec::error::invalid_arguments);
        // A command read at an earlier boundary is stale.
        auto earlier = request_of(state, top, "aim:filter", {.text = "", .pane = "aim:pane:left"});
        earlier.expect.boundary_id = old_state->id;
        const auto stale = input_of(earlier);
        REQUIRE_FALSE(stale);
        CHECK(stale.error() == ec::error::stale_boundary);
        return aim_event("aim:filter", {.text = "", .pane = "aim:pane:left"});
    });
    plan.steps.push_back([&](const ec::screen_view& top) {
        CHECK(list_rows(top, "aim:pane:left:items").size() == 1);
        return aim_event("aim:close");
    });

    run_script(plan);

    CHECK(plan.reads == 3);
    CHECK(get_avatar().amount_of(itype_id("test_1kg_cube")) == 1);
    CHECK(ground_charges(itype_id("test_platinum_bit")) == 10);
    CHECK_FALSE(get_avatar().activity);
}

TEST_CASE(
    "advanced inventory refuses arguments that do not fit the affordance",
    "[advanced_inventory][client][mcp]") {
    const auto guard = screen_guard{};
    const auto fixture = aim_fixture{};
    add_to_inventory(itype_id("test_1kg_cube"), 3);
    auto plan = script{};
    plan.steps.push_back([&](const ec::screen_view& top) {
        const auto cube = row_of(top, "aim:pane:left:items", "test_1kg_cube");
        const auto refused = [&](const std::string& affordance, ec::invoke_args args) {
            const auto event = aim_input(affordance, std::move(args));
            REQUIRE_FALSE(event);
            return event.error();
        };
        const auto invalid = ec::error::invalid_arguments;
        // A row of no list, a pane of no panel, a count the affordance does not take, a pane to
        // filter missing.
        CHECK(refused("aim:move", {.rows = std::vector<ec::invoke_row>{{.id = "nowhere"}},
                                   .pane = "aim:pane:right"})
              == invalid);
        CHECK(
            refused("aim:move",
                    {.rows = std::vector<ec::invoke_row>{{.id = cube}}, .pane = "aim:pane:middle"})
            == invalid);
        CHECK(refused("aim:move", {.rows = std::vector<ec::invoke_row>{{.id = cube}}}) == invalid);
        CHECK(refused("aim:move", {.rows = std::vector<ec::invoke_row>{}, .pane = "aim:pane:right"})
              == invalid);
        // The right pane's rows are not the source's.
        CHECK(refused("aim:move", {.rows = std::vector<ec::invoke_row>{{.id = "item:0"}},
                                   .pane = "aim:pane:right"})
              == invalid);
        // An area is no item, and an item is no area.
        CHECK(refused("aim:move", {.rows = std::vector<ec::invoke_row>{{.id = "area:E"}},
                                   .pane = "aim:pane:right"})
              == invalid);
        CHECK(refused("aim:set_area", {.row = cube, .pane = "aim:pane:left"}) == invalid);
        // An area the pane cannot put items in is not shown.
        const auto areas = area_cells(top, "aim:pane:left:areas");
        const auto denied = std::ranges::find_if(areas, [](const ec::grid_cell& cell) {
            return !cell.enabled;
        });
        REQUIRE(denied != areas.end());
        CHECK(refused("aim:set_area", {.row = denied->id, .pane = "aim:pane:left"}) == invalid);
        CHECK(refused("aim:set_area", {.row = "nowhere", .pane = "aim:pane:left"}) == invalid);
        CHECK(refused("aim:set_area", {.row = "area:E"}) == invalid);
        CHECK(refused("aim:filter", {.pane = "aim:pane:left"}) == invalid);
        CHECK(refused("aim:toggle_vehicle", {.row = "area:E", .pane = "aim:pane:left"}) == invalid);
        return aim_event("aim:close");
    });

    run_script(plan);

    CHECK(plan.reads == 1);
    CHECK_FALSE(get_avatar().activity);
}

TEST_CASE(
    "advanced inventory panes publish the header totals, capacities and sort mode",
    "[advanced_inventory][client][mcp]") {
    const auto guard = screen_guard{};
    const auto fixture = aim_fixture{};
    auto& you = get_avatar();
    add_to_inventory(itype_id("test_1kg_cube"), 1);
    get_map().add_item(aim_test_pos, item::spawn("test_platinum_bit", calendar::turn, 10));
    const auto weight = [](const units::mass value) {
        return string_format("%.1f", convert_weight(value));
    };

    SECTION("carried weight against the capacity, ground without one") {
        auto plan = script{};
        plan.steps.push_back([&](const ec::screen_view& top) {
            const auto carried = panel_of(top, "aim:pane:left");
            CHECK(stat_of(carried, "weight").value == weight(you.weight_carried()));
            CHECK(stat_of(carried, "weight").max == weight(you.weight_capacity()));
            CHECK(stat_of(carried, "weight").unit == weight_units());
            CHECK(stat_of(carried, "weight").tone.empty());
            CHECK(stat_of(carried, "volume").value == format_volume(you.volume_carried()));
            CHECK(stat_of(carried, "volume").max == format_volume(you.volume_capacity()));
            CHECK(stat_of(carried, "volume").unit == volume_units_abbr());
            CHECK(stat_of(carried, "sort").value == "name");
            const auto ground = panel_of(top, "aim:pane:right");
            CHECK(stat_of(ground, "weight").max.empty());
            CHECK_FALSE(stat_of(ground, "weight").value.empty());
            CHECK_FALSE(stat_of(ground, "volume").max.empty());
            return aim_event("aim:close");
        });
        run_script(plan);
        CHECK(plan.reads == 1);
    }
    SECTION("an overloaded inventory is bad") {
        you.i_add(item::spawn("test_overweight_cube", calendar::turn));
        REQUIRE(you.weight_carried() > you.weight_capacity());
        auto plan = script{};
        plan.steps.push_back([&](const ec::screen_view& top) {
            CHECK(stat_of(panel_of(top, "aim:pane:left"), "weight").tone == "bad");
            CHECK(stat_of(panel_of(top, "aim:pane:right"), "weight").tone.empty());
            return aim_event("aim:close");
        });
        run_script(plan);
        CHECK(plan.reads == 1);
    }
}

TEST_CASE(
    "advanced inventory moves the exact stacks and charges an invoke names",
    "[advanced_inventory][client][replay][mcp]") {
    const auto guard = screen_guard{};
    const auto fixture = aim_fixture{};
    auto& you = get_avatar();
    auto plan = script{};

    SECTION("two of three inventory items move to the ground") {
        add_to_inventory(itype_id("test_1kg_cube"), 3);
        plan.steps.push_back(move_rows("aim:pane:left", "aim:pane:right", {{"test_1kg_cube", 2}}));
        run_script(plan);
        REQUIRE(you.activity);
        CHECK(you.activity->id() == activity_id("ACT_DROP"));
        process_activity(you);
        CHECK_FALSE(you.activity);
        CHECK(you.backlog.empty());
        CHECK(you.amount_of(itype_id("test_1kg_cube")) == 1);
        CHECK(ground_item_count(itype_id("test_1kg_cube")) == 2);
    }
    SECTION("three of ten ground charges move to the inventory") {
        get_map().add_item(aim_test_pos, item::spawn("test_platinum_bit", calendar::turn, 10));
        plan.steps.push_back(key_action("TOGGLE_TAB"));
        plan.steps.push_back(
            move_rows("aim:pane:right", "aim:pane:left", {{"test_platinum_bit", 3}}));
        run_script(plan);
        REQUIRE(you.activity);
        CHECK(you.activity->id() == activity_id("ACT_PICKUP"));
        process_activity(you);
        CHECK_FALSE(you.activity);
        CHECK(you.backlog.empty());
        CHECK(you.charges_of(itype_id("test_platinum_bit")) == 3);
        CHECK(ground_charges(itype_id("test_platinum_bit")) == 7);
    }
    SECTION("no count moves the whole stack") {
        add_to_inventory(itype_id("test_1kg_cube"), 3);
        plan.steps.push_back(
            move_rows("aim:pane:left", "aim:pane:right", {{"test_1kg_cube", std::nullopt}}));
        run_script(plan);
        REQUIRE(you.activity);
        process_activity(you);
        CHECK(you.amount_of(itype_id("test_1kg_cube")) == 0);
        CHECK(ground_item_count(itype_id("test_1kg_cube")) == 3);
    }
    CHECK(plan.popups.empty());
}

TEST_CASE(
    "advanced inventory carries several rows in one activity",
    "[advanced_inventory][client][mcp]") {
    const auto guard = screen_guard{};
    const auto fixture = aim_fixture{};
    auto& you = get_avatar();
    add_to_inventory(itype_id("test_1kg_cube"), 3);
    add_to_inventory(itype_id("knife_steak"), 1);
    auto plan = script{};
    plan.steps.push_back(move_rows(
        "aim:pane:left", "aim:pane:right", {{"test_1kg_cube", 2}, {"knife_steak", std::nullopt}}));

    run_script(plan);

    REQUIRE(you.activity);
    CHECK(you.activity->id() == activity_id("ACT_DROP"));
    process_activity(you);
    CHECK_FALSE(you.activity);
    CHECK(you.amount_of(itype_id("test_1kg_cube")) == 1);
    CHECK(you.amount_of(itype_id("knife_steak")) == 0);
    CHECK(ground_item_count(itype_id("test_1kg_cube")) == 2);
    CHECK(ground_item_count(itype_id("knife_steak")) == 1);
    CHECK(plan.popups.empty());
}

TEST_CASE(
    "advanced inventory moves nothing when one of the rows cannot go",
    "[advanced_inventory][client][mcp]") {
    const auto guard = screen_guard{};
    const auto fixture = aim_fixture{};
    auto& you = get_avatar();
    for (const auto* const type : {"test_1kg_cube", "test_overweight_cube"}) {
        get_map().add_item(aim_test_pos, item::spawn(type, calendar::turn));
    }
    auto plan = script{};
    plan.steps.push_back(key_action("TOGGLE_TAB"));
    plan.steps.push_back(
        move_rows("aim:pane:right", "aim:pane:left",
                  {{"test_1kg_cube", std::nullopt}, {"test_overweight_cube", std::nullopt}}));

    run_script(plan);

    // The first row fits on its own; the second is too heavy, so neither moves.
    REQUIRE(plan.popups.size() == 1);
    CHECK_FALSE(plan.popups.front().empty());
    CHECK_FALSE(you.activity);
    CHECK(you.backlog.empty());
    CHECK(you.amount_of(itype_id("test_1kg_cube")) == 0);
    CHECK(ground_item_count(itype_id("test_1kg_cube")) == 1);
    CHECK(ground_item_count(itype_id("test_overweight_cube")) == 1);
}

TEST_CASE(
    "advanced inventory ignores a move whose counts or panes do not hold",
    "[advanced_inventory][client][mcp]") {
    const auto guard = screen_guard{};
    const auto fixture = aim_fixture{};
    auto& you = get_avatar();
    add_to_inventory(itype_id("test_1kg_cube"), 3);
    auto plan = script{};
    // Zero, more than the row has, a row twice, and the source as the destination.
    plan.steps.push_back(move_rows("aim:pane:left", "aim:pane:right", {{"test_1kg_cube", 0}}));
    plan.steps.push_back(move_rows("aim:pane:left", "aim:pane:right", {{"test_1kg_cube", 4}}));
    plan.steps.push_back(
        move_rows("aim:pane:left", "aim:pane:right", {{"test_1kg_cube", 1}, {"test_1kg_cube", 1}}));
    plan.steps.push_back(move_rows("aim:pane:left", "aim:pane:left", {{"test_1kg_cube", 1}}));

    run_script(plan);

    CHECK(plan.reads == 4);
    CHECK(plan.popups.empty());
    CHECK_FALSE(you.activity);
    CHECK(you.backlog.empty());
    CHECK(you.amount_of(itype_id("test_1kg_cube")) == 3);
    CHECK(ground_item_count(itype_id("test_1kg_cube")) == 0);
}

TEST_CASE(
    "advanced inventory takes an area id only as an area and an item id only as an item",
    "[advanced_inventory][client][mcp]") {
    const auto guard = screen_guard{};
    const auto fixture = aim_fixture{};
    auto& you = get_avatar();
    add_to_inventory(itype_id("test_1kg_cube"), 3);
    auto plan = script{};
    plan.steps.push_back([&](const ec::screen_view& top) {
        // Both panes list the same area ids; each of the lists holds each id once.
        const auto left = ids_of(area_cells(top, "aim:pane:left:areas"));
        CHECK(left == ids_of(area_cells(top, "aim:pane:right:areas")));
        CHECK(left == area_names);
        // An area id is no item of the source, and an item id is no area to show.
        const auto state = capture();
        const auto as_item = input_of(request_of(
            state, top, "aim:move",
            {.rows = std::vector<ec::invoke_row>{{.id = "area:E", .count = 1}},
             .pane = "aim:pane:right"}));
        REQUIRE_FALSE(as_item);
        CHECK(as_item.error() == ec::error::invalid_arguments);
        const auto as_area = input_of(request_of(
            state, top, "aim:set_area",
            {.row = row_of(top, "aim:pane:left:items", "test_1kg_cube"), .pane = "aim:pane:left"}));
        REQUIRE_FALSE(as_area);
        CHECK(as_area.error() == ec::error::invalid_arguments);
        // An id of no list is refused by the shape.
        const auto nowhere = input_of(request_of(
            state, top, "aim:set_area", {.row = "area:NOWHERE", .pane = "aim:pane:left"}));
        REQUIRE_FALSE(nowhere);
        CHECK(nowhere.error() == ec::error::invalid_arguments);
        return aim_event("aim:close");
    });

    run_script(plan);

    CHECK(plan.reads == 1);
    CHECK(plan.popups.empty());
    CHECK_FALSE(you.activity);
    CHECK(you.amount_of(itype_id("test_1kg_cube")) == 3);
    CHECK(ground_item_count(itype_id("test_1kg_cube")) == 0);
}

TEST_CASE(
    "advanced inventory carries rows from the ground and the cargo of one square as two places",
    "[advanced_inventory][client][mcp]") {
    const auto guard = screen_guard{};
    const auto fixture = aim_fixture{};
    auto* const cart = fixture_with_cart();
    auto& you = get_avatar();
    auto plan = script{};
    plan.steps.push_back(invoke_step("aim:move", [](const ec::screen_view& top) {
        const auto ids = ids_of(list_rows(top, "aim:pane:left:items"));
        REQUIRE(ids.size() == 2);
        return ec::invoke_args{
            .rows = std::vector<ec::invoke_row>{{.id = ids.front()}, {.id = ids.back()}},
            .pane = "aim:pane:right"};
    }));

    run_script(plan);

    CHECK(plan.popups.empty());
    REQUIRE(you.activity);
    for (auto turns = 0; turns < 4 && you.activity; ++turns) { process_activity(you); }
    CHECK_FALSE(you.activity);
    CHECK(ground_item_count(itype_id("test_1kg_cube")) == 2);
    CHECK(
        cart->get_items(cart->part_with_feature(tripoint_mnt_veh::zero(), "CARGO", true)).empty());
}

TEST_CASE(
    "advanced inventory keeps its native overweight and loose-liquid denials",
    "[advanced_inventory][client][mcp]") {
    const auto guard = screen_guard{};
    const auto run_denial = [&](const itype_id& type, const int charges) {
        const auto fixture = aim_fixture{};
        auto& you = get_avatar();
        get_map().add_item(aim_test_pos, item::spawn(type, calendar::turn, charges));
        auto plan = script{};
        plan.steps.push_back(key_action("TOGGLE_TAB"));
        plan.steps.push_back(move_rows("aim:pane:right", "aim:pane:left", {{type.str(), 1}}));
        run_script(plan);
        CHECK(plan.popups.size() == 1);
        CHECK_FALSE(you.activity);
        CHECK(you.backlog.empty());
        CHECK(you.amount_of(type) == 0);
        CHECK(ground_item_count(type) == 1);
    };

    SECTION("overweight destination") { run_denial(itype_id("test_overweight_cube"), -1); }
    SECTION("loose liquid") { run_denial(itype_id("water_clean"), 10); }
}

TEST_CASE(
    "advanced inventory container transfer rejects incompatible liquids unchanged",
    "[advanced_inventory][client][mcp]") {
    const auto guard = screen_guard{};
    const auto fixture = aim_fixture{};
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
    auto plan = script{};
    plan.steps.push_back([&](const ec::screen_view& top) {
        // The jug sorts first, so it is the item the left pane's cursor is on.
        const auto rows = list_rows(top, "aim:pane:left:items");
        const auto jug = std::ranges::find(rows, top.focus_row, &ec::row::id);
        REQUIRE(jug != rows.end());
        REQUIRE(jug->look.has_value());
        REQUIRE(jug->look->id == "test_jug_plastic");
        return aim_event("aim:set_area", {.row = "area:CONTAINER", .pane = "aim:pane:left"});
    });
    plan.steps.push_back(key_action("TOGGLE_TAB"));
    plan.steps.push_back(move_rows("aim:pane:right", "aim:pane:left", {{"test_waterskin", 1}}));

    run_script(plan);

    CHECK(plan.popups.size() == 1);
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
    "advanced inventory pickup honors theft refusal without moving the owned item",
    "[advanced_inventory][client][mcp]") {
    const auto guard = screen_guard{};
    const auto force_lowercase = override_option("FORCE_CAPITAL_YN", "false");
    const auto fixture = aim_fixture{};
    auto& you = get_avatar();
    g->faction_manager_ptr->create_if_needed();
    auto& owner = spawn_npc(aim_test_pos + tripoint_north, "test_talker");
    owner.set_fac(faction_id("free_merchants"));
    auto owned_item = item::spawn("test_1kg_cube", calendar::turn);
    owned_item->set_owner(owner);
    const auto owner_id = owned_item->get_owner();
    get_map().add_item(aim_test_pos, std::move(owned_item));
    you.set_value("THIEF_MODE", "THIEF_ASK");
    you.set_value("THIEF_MODE_KEEP", "NO");
    auto asked = 0;
    auto plan = script{};
    plan.steps.push_back(key_action("TOGGLE_TAB"));
    plan.steps.push_back(move_rows("aim:pane:right", "aim:pane:left", {{"test_1kg_cube", 1}}));
    plan.other = [&](const game_client::interaction_snapshot& snapshot) {
        REQUIRE(snapshot.context == "YES_NO_ALWAYS_NEVER");
        CHECK(snapshot.message.find("stealing") != std::string::npos);
        const auto refuse = std::ranges::
            find(snapshot.choices, "NO", &game_client::interaction_choice::description);
        REQUIRE(refuse != snapshot.choices.end());
        ++asked;
        return resolve({
            .input_id = snapshot.input_id,
            .operation = game_client::interaction_operation::choose,
            .target_id = refuse->id,
        });
    };

    run_script(plan);
    REQUIRE(you.activity);
    CHECK(you.activity->id() == activity_id("ACT_PICKUP"));
    process_activity(you);

    CHECK(asked == 1);
    CHECK_FALSE(you.activity);
    CHECK(you.backlog.empty());
    CHECK(you.amount_of(itype_id("test_1kg_cube")) == 0);
    REQUIRE(ground_item_count(itype_id("test_1kg_cube")) == 1);
    auto ground_items = get_map().i_at(aim_test_pos);
    const auto ground_item = std::ranges::find_if(ground_items, [](const auto* candidate) {
        return candidate->typeId() == itype_id("test_1kg_cube");
    });
    REQUIRE(ground_item != ground_items.end());
    CHECK((*ground_item)->get_owner() == owner_id);
    CHECK(you.get_value("THIEF_MODE") == "THIEF_ASK");
}

TEST_CASE(
    "semantic replay repeats an advanced inventory move and refuses to invent one",
    "[advanced_inventory][client][replay][mcp]") {
    const auto guard = screen_guard{};
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
        const auto fixture = aim_fixture{};
        auto& recorded = get_avatar();
        add_to_inventory(itype_id("test_1kg_cube"), 3);
        replay::configure_recording(path.string(), {.rng_seed = 37});
        replay::start();
        auto plan = script{};
        plan.steps.push_back(move_rows("aim:pane:left", "aim:pane:right", {{"test_1kg_cube", 2}}));
        run_script(plan);
        REQUIRE(recorded.activity);
        process_activity(recorded);
        CHECK(recorded.amount_of(itype_id("test_1kg_cube")) == 1);
        CHECK(ground_item_count(itype_id("test_1kg_cube")) == 2);
        replay::finish();
    }

    {
        const auto fixture = aim_fixture{};
        auto& played = get_avatar();
        add_to_inventory(itype_id("test_1kg_cube"), 3);
        game_client::memory::set_input_provider([](const int /*timeout*/) -> input_event {
            throw std::runtime_error("advanced inventory playback must not read live input");
        });
        replay::configure_playback(path.string(), {.rng_seed = 37});
        replay::start();
        create_advanced_inv();
        REQUIRE(played.activity);
        CHECK(played.activity->id() == activity_id("ACT_DROP"));
        process_activity(played);
        CHECK_FALSE(played.activity);
        CHECK(played.backlog.empty());
        CHECK(played.amount_of(itype_id("test_1kg_cube")) == 1);
        CHECK(ground_item_count(itype_id("test_1kg_cube")) == 2);
        replay::finish();
    }

    {
        // The recorded row is not there: the invoke names nothing the screen has, so nothing moves.
        const auto fixture = aim_fixture{};
        auto& changed = get_avatar();
        game_client::memory::set_input_provider([](const int /*timeout*/) -> input_event {
            throw std::runtime_error(
                "changed advanced inventory playback must not read live input");
        });
        replay::configure_playback(path.string(), {.rng_seed = 37});
        replay::start();
        // The screen ignores the invoke and leaves when the recording has no input left to give.
        CHECK_NOTHROW(create_advanced_inv());
        CHECK_FALSE(changed.activity);
        CHECK(changed.backlog.empty());
        CHECK(ground_item_count(itype_id("test_1kg_cube")) == 0);
        replay::stop();
    }
}

#endif
