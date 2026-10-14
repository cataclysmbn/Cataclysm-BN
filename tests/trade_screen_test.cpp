#if defined(CATA_MCP)

#    include "avatar.h"
#    include "calendar.h"
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
#    include "faction.h"
#    include "game.h"
#    include "input.h"
#    include "item.h"
#    include "locale_fixture.h"
#    include "locale_scope.h"
#    include "map/map.h"
#    include "map_helpers.h"
#    include "npc.h"
#    include "npctrade.h"
#    include "output.h"
#    include "path_info.h"
#    include "player_helpers.h"
#    include "replay/replay.h"
#    include "rng.h"
#    include "state_helpers.h"
#    include "string_utils.h"
#    include "trade_screen.h"
#    include "trade_win.h"
#    include "ui.h"

#    include <atomic>
#    include <filesystem>
#    include <iterator>
#    include <nlohmann/json.hpp>
#    include <ranges>
#    include <set>
#    include <stdexcept>
#    include <string>
#    include <vector>

namespace {
namespace ec = engine_client;
namespace ts = trade_screen;

constexpr auto trade_test_pos = tripoint_bub_ms{60, 60, 0};

auto resize_terminal(const int columns, const int lines) -> void {
    TERMX = columns;
    TERMY = lines;
    game_client::memory::resize(TERMX, TERMY);
    catacurses::stdscr = catacurses::newwin(TERMY, TERMX, point_zero);
    catacurses::newscr = catacurses::newwin(TERMY, TERMX, point_zero);
}

struct screen_guard {
    const game_client::memory::scoped_state memory;
    bool old_test_mode = test_mode;
    int old_termx = TERMX;
    int old_termy = TERMY;
    int old_full_screen_width = FULL_SCREEN_WIDTH;
    int old_full_screen_height = FULL_SCREEN_HEIGHT;

    screen_guard() {
        test_mode = false;
        FULL_SCREEN_WIDTH = 80;
        FULL_SCREEN_HEIGHT = 24;
        resize_terminal(100, 40);
    }

    ~screen_guard() {
        TERMX = old_termx;
        TERMY = old_termy;
        FULL_SCREEN_WIDTH = old_full_screen_width;
        FULL_SCREEN_HEIGHT = old_full_screen_height;
        test_mode = old_test_mode;
    }
};

struct trade_fixture {
    time_point old_turn = calendar::turn;
    cata_default_random_engine old_rng = rng_get_engine();
    npc* trader = nullptr;

    trade_fixture() {
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

    ~trade_fixture() {
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

auto stack_json() -> nlohmann::json {
    return nlohmann::json::parse(ec::serialize_interaction_boundary(ts::live_state()))["stack"];
}

auto keys_of(const ec::row& row) -> std::vector<std::string> {
    return row.columns | std::views::transform(&game_client::interaction_column::key)
         | std::ranges::to<std::vector>();
}

auto ids_of(const nlohmann::json& value) -> std::vector<std::string> {
    auto result = std::vector<std::string>{};
    const auto walk = [&](const auto& self, const nlohmann::json& node) -> void {
        if (node.is_object()) {
            for (const auto& [key, member] : node.items()) {
                if ((key == "id" || key == "screen_id") && member.is_string()) {
                    result.push_back(member.get<std::string>());
                }
                self(self, member);
            }
        } else if (node.is_array()) {
            for (const auto& member : node) { self(self, member); }
        }
    };
    walk(walk, value);
    return result;
}

/// The value without any string member: what is left is shape, numbers and flags.
auto without_text(const nlohmann::json& value) -> nlohmann::json {
    if (value.is_object()) {
        auto result = nlohmann::json::object();
        for (const auto& [key, member] : value.items()) {
            if (!member.is_string()) { result[key] = without_text(member); }
        }
        return result;
    }
    if (value.is_array()) {
        auto result = nlohmann::json::array();
        for (const auto& member : value) { result.push_back(without_text(member)); }
        return result;
    }
    return value;
}

} // namespace

TEST_CASE(
    "NPC trade publishes both panes with their headers, category rows and price columns",
    "[client][interaction][npc_trade][engine_client_trade][mcp]") {
    const auto shopkeeper = GENERATE(true, false);
    const auto guard = screen_guard{};
    const auto fixture = trade_fixture{};
    fixture.trader->mission = shopkeeper ? NPC_MISSION_SHOPKEEP : NPC_MISSION_NULL;
    fixture.add_npc_item(itype_id("test_platinum_bit"), 10);
    fixture.add_npc_item(itype_id("test_halligan"));
    std::ranges::for_each(std::views::iota(0, 3), [&](const auto /*index*/) {
        fixture.add_player_item(itype_id("test_pipe"));
    });
    auto& foreign = fixture.add_player_item(itype_id("test_fire_ax"));
    foreign.set_owner(*fixture.trader);
    const auto original_pipes = get_avatar().amount_of(itype_id("test_pipe"));
    const auto original_charges = fixture.trader->charges_of(itype_id("test_platinum_bit"));
    auto state = npc_trading::trade_state{};
    npc_trading::setup_trade_state(state, 0, *fixture.trader);
    auto reads = 0;

    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        ++reads;
        const auto screen = ts::top();
        REQUIRE(screen);
        const auto& view = *screen;
        CHECK(view.role == "screen");
        CHECK(view.title.starts_with("Trading with"));
        CHECK(view.title.find(fixture.trader->disp_name()) != std::string::npos);

        // The header: the trade's balance, as the native line colors it, and the category mode.
        const auto& balance = ts::body<ec::text_body>(ts::part(view, "trade:balance"));
        CHECK(balance.markup.starts_with("Credit"));
        CHECK(balance.tone
              == (npc_trading::npc_will_accept_trade(state, *fixture.trader) ? "good" : "bad"));
        const auto& head = ts::body<ec::panel_body>(ts::part(view, "trade:head"));
        REQUIRE(head.stats.size() == 1);
        CHECK(head.stats.front().key == "category_mode");
        CHECK(head.stats.front().value == "OFF");

        // Each pane: whose it is, what it filters by, and carried against capacity.
        const auto& npc_pane = ts::body<ec::panel_body>(ts::part(view, "pane:npc"));
        const auto& you_pane = ts::body<ec::panel_body>(ts::part(view, "pane:you"));
        CHECK(npc_pane.title.starts_with("Inventory:"));
        CHECK(npc_pane.title.find(fixture.trader->name) != std::string::npos);
        CHECK(you_pane.title == "Inventory: You");
        CHECK(npc_pane.filter.empty());
        // The shopkeeper carries whatever it is given, so the native pane prints no capacity for
        // it.
        CHECK(npc_pane.stats.empty() == shopkeeper);
        REQUIRE(you_pane.stats.size() == 2);
        CHECK(you_pane.stats[0].key == "weight");
        CHECK(you_pane.stats[1].key == "volume");
        for (const auto& stat : you_pane.stats) {
            CHECK_FALSE(stat.max.empty());
            CHECK_FALSE(stat.unit.empty());
            CHECK((stat.tone == "good" || stat.tone == "bad"));
        }

        // Rows: a heading over each category, then the items with the native columns.
        const auto& theirs = ts::rows(view, "list:npc");
        REQUIRE_FALSE(theirs.empty());
        CHECK(theirs.front().header);
        CHECK_FALSE(theirs.front().selectable);
        CHECK(theirs.front().label == to_upper_case(theirs.front().label));
        const auto* const bits = ts::item_row(view, "list:npc", "test_platinum_bit");
        const auto* const halligan = ts::item_row(view, "list:npc", "test_halligan");
        const auto* const pipes = ts::item_row(view, "list:you", "test_pipe");
        REQUIRE(bits != nullptr);
        REQUIRE(halligan != nullptr);
        REQUIRE(pipes != nullptr);
        CHECK(ts::item_row(view, "list:you", "test_fire_ax") == nullptr);
        CHECK(bits->max == 10);
        CHECK(halligan->max == 1);
        CHECK(pipes->max == 3);
        CHECK(bits->selected_count == 0);
        CHECK(bits->minimum_count == 0);
        CHECK(bits->mark->glyph == "-");
        // The letters that pick a row belong to the window's page, so no row carries one.
        CHECK(bits->hotkey.empty());
        CHECK(keys_of(*bits) == std::vector<std::string>{"qty", "weight", "vol", "price"});
        CHECK(bits->columns[0].value == "10");
        CHECK(halligan->columns[0].value.empty());
        CHECK_FALSE(bits->columns[3].value.empty());
        CHECK(bits->look->kind == "item");

        // Ids come from what the items are: none holds the item's name, and no two are alike.
        auto listed = std::vector<std::string>{};
        for (const auto* const pane_rows : {&theirs, &ts::rows(view, "list:you")}) {
            std::ranges::transform(*pane_rows, std::back_inserter(listed), &ec::row::id);
        }
        CHECK(std::set<std::string>(listed.begin(), listed.end()).size() == listed.size());
        CHECK(bits->id.find("platinum") == std::string::npos);

        // Only the row the cursor is on carries its description; the focus names that row.
        CHECK(view.focus_component == "list:npc");
        const auto focused = std::ranges::find(theirs, view.focus_row, &ec::row::id);
        REQUIRE(focused != theirs.end());
        CHECK_FALSE(focused->description.empty());
        CHECK(std::ranges::
                  count_if(theirs, [](const ec::row& row) { return !row.description.empty(); })
              == 1);
        CHECK(std::ranges::none_of(ts::rows(view, "list:you"), [](const ec::row& row) {
            return !row.description.empty();
        }));

        // What a client may do: the affordances of the screen, then the context's registered
        // actions.
        const auto& count = ts::affordance(view, "trade:set_count");
        CHECK(count.args.row->component_id == "trade:panes");
        CHECK(count.args.count->min == 0);
        CHECK(ec::scope_of(count) == ec::expect_scope::boundary);
        const auto& filter = ts::affordance(view, "trade:filter");
        CHECK(filter.args.text);
        CHECK(filter.args.pane->component_id == "trade:panes");
        CHECK(ts::affordance(view, "trade:confirm").role == "confirm");
        CHECK(ts::affordance(view, "trade:cancel").role == "cancel");
        CHECK(ec::scope_of(ts::affordance(view, "trade:cancel")) == ec::expect_scope::screen);
        CHECK(ts::affordance(view, "UP").keys == std::vector<std::string>{"k"});
        CHECK(ts::body<ec::list_body>(ts::part(view, "list:npc")).filterable);

        // Both panes' rows are the rows a count may name; nothing else is.
        const auto named = ec::row_ids(ts::part(view, "trade:panes"));
        REQUIRE(named);
        CHECK(std::ranges::contains(*named, bits->id));
        CHECK(std::ranges::contains(*named, pipes->id));
        CHECK_FALSE(std::ranges::contains(*named, theirs.front().id));
        CHECK(ts::resolve("trade:set_count", {.row = bits->id, .count = 3}));
        CHECK(ts::resolve("trade:set_count", {.row = pipes->id, .count = 0}));
        CHECK(ts::resolve("trade:filter", {.text = "pipe", .pane = "pane:npc"}));
        const auto refused = [](const std::string& affordance, ec::invoke_args args) {
            const auto result = ts::resolve(affordance, std::move(args));
            REQUIRE_FALSE(result);
            return result.error();
        };
        CHECK(refused("trade:set_count", {.row = "trade-item:unlisted", .count = 1})
              == ec::error::invalid_arguments);
        // A category heading is no item, so a count may not name it.
        CHECK(refused("trade:set_count", {.row = theirs.front().id, .count = 1})
              == ec::error::invalid_arguments);
        CHECK(refused("trade:set_count", {.row = bits->id, .count = -1})
              == ec::error::invalid_arguments);
        CHECK(refused("trade:set_count", {.row = bits->id}) == ec::error::invalid_arguments);
        CHECK(refused("trade:filter", {.text = "pipe", .pane = "pane:nobody"})
              == ec::error::invalid_arguments);
        CHECK(refused("trade:filter", {.text = "pipe"}) == ec::error::invalid_arguments);
        return ts::invoke("trade:cancel");
    });

    CHECK_FALSE(
        trading_window(state).perform_trade(*fixture.trader, npc_trading::deal_kind::trade));
    CHECK(reads == 1);
    CHECK(get_avatar().amount_of(itype_id("test_pipe")) == original_pipes);
    CHECK(fixture.trader->charges_of(itype_id("test_platinum_bit")) == original_charges);
}

TEST_CASE(
    "NPC trade counts in native units, clamps to what is there and leaves unlisted rows alone",
    "[client][interaction][npc_trade][engine_client_trade][mcp]") {
    const auto guard = screen_guard{};
    const auto fixture = trade_fixture{};
    fixture.trader->mission = NPC_MISSION_SHOPKEEP;
    fixture.add_npc_item(itype_id("test_platinum_bit"), 10);
    std::ranges::for_each(std::views::iota(0, 3), [&](const auto /*index*/) {
        fixture.add_player_item(itype_id("test_pipe"));
    });
    const auto original_pipes = get_avatar().amount_of(itype_id("test_pipe"));
    const auto original_charges = fixture.trader->charges_of(itype_id("test_platinum_bit"));
    const auto original_debt = fixture.trader->op_of_u.owed;
    auto state = npc_trading::trade_state{};
    npc_trading::setup_trade_state(state, 0, *fixture.trader);
    auto step = 0;
    auto bits_id = std::string{};
    auto pipes_id = std::string{};
    auto balance_before = std::string{};

    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto screen = ts::top();
        REQUIRE(screen);
        const auto* const bits = ts::item_row(*screen, "list:npc", "test_platinum_bit");
        const auto* const pipes = ts::item_row(*screen, "list:you", "test_pipe");
        REQUIRE(bits != nullptr);
        REQUIRE(pipes != nullptr);
        const auto balance = ts::body<ec::text_body>(ts::part(*screen, "trade:balance")).markup;
        if (step == 0) {
            bits_id = bits->id;
            pipes_id = pipes->id;
            balance_before = balance;
            CHECK(bits->selected_count == 0);
            step = 1;
            return ts::set_count(bits_id, 3);
        }
        if (step == 1) {
            // The row keeps its id from one publication to the next.
            CHECK(bits->id == bits_id);
            CHECK(bits->selected);
            CHECK(bits->selected_count == 3);
            CHECK(bits->mark->glyph == "#");
            CHECK(balance != balance_before);
            step = 2;
            return ts::set_count(pipes_id, 2);
        }
        if (step == 2) {
            // Both panes keep their picks, and the cursor went to the pane of the last row.
            CHECK(bits->selected_count == 3);
            CHECK(pipes->selected_count == 2);
            CHECK(screen->focus_component == "list:you");
            CHECK(screen->focus_row == pipes_id);
            step = 3;
            return ts::set_count(bits_id, 99);
        }
        if (step == 3) {
            // More than is there is all that is there, as natively.
            CHECK(bits->selected_count == 10);
            CHECK(bits->mark->glyph == "+");
            CHECK(screen->focus_component == "list:npc");
            step = 4;
            return ts::set_count(bits_id, 0);
        }
        if (step == 4) {
            CHECK_FALSE(bits->selected);
            CHECK(bits->selected_count == 0);
            CHECK(bits->mark->glyph == "-");
            CHECK(pipes->selected_count == 2);
            step = 5;
            // A row the window does not list changes nothing.
            return ts::set_count("trade-item:unlisted", 1);
        }
        REQUIRE(step == 5);
        CHECK(bits->selected_count == 0);
        CHECK(pipes->selected_count == 2);
        step = 6;
        return ts::invoke("trade:cancel");
    });

    CHECK_FALSE(
        trading_window(state).perform_trade(*fixture.trader, npc_trading::deal_kind::trade));
    CHECK(step == 6);
    CHECK(get_avatar().amount_of(itype_id("test_pipe")) == original_pipes);
    CHECK(fixture.trader->charges_of(itype_id("test_platinum_bit")) == original_charges);
    CHECK(fixture.trader->op_of_u.owed == original_debt);
}

TEST_CASE(
    "NPC trade filters each pane by its own text",
    "[client][interaction][npc_trade][engine_client_trade][mcp]") {
    const auto guard = screen_guard{};
    const auto fixture = trade_fixture{};
    fixture.trader->mission = NPC_MISSION_SHOPKEEP;
    fixture.add_npc_item(itype_id("test_platinum_bit"), 10);
    fixture.add_npc_item(itype_id("test_halligan"));
    fixture.add_player_item(itype_id("test_pipe"));
    auto state = npc_trading::trade_state{};
    npc_trading::setup_trade_state(state, 0, *fixture.trader);
    constexpr auto nothing = "__no_trade_match__";
    auto step = 0;
    auto hidden_id = std::string{};

    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto screen = ts::top();
        REQUIRE(screen);
        const auto* const bits = ts::item_row(*screen, "list:npc", "test_platinum_bit");
        const auto* const halligan = ts::item_row(*screen, "list:npc", "test_halligan");
        const auto* const pipes = ts::item_row(*screen, "list:you", "test_pipe");
        const auto filter_of = [&](const std::string& pane) {
            return ts::body<ec::panel_body>(ts::part(*screen, pane)).filter;
        };
        if (step == 0) {
            REQUIRE(bits != nullptr);
            hidden_id = bits->id;
            step = 1;
            return ts::filter("pane:npc", nothing);
        }
        if (step == 1) {
            // The NPC's pane lists nothing, the player's is as it was, and the cursor stays where
            // it was.
            CHECK(bits == nullptr);
            CHECK(halligan == nullptr);
            CHECK(ts::rows(*screen, "list:npc").empty());
            CHECK(pipes != nullptr);
            CHECK(filter_of("pane:npc") == nothing);
            CHECK(filter_of("pane:you").empty());
            CHECK(screen->focus_component == "list:npc");
            // A row the filter hides is no row a count may name; a pane that is none is no pane.
            const auto hidden = ts::resolve("trade:set_count", {.row = hidden_id, .count = 1});
            REQUIRE_FALSE(hidden);
            CHECK(hidden.error() == ec::error::invalid_arguments);
            step = 2;
            return ts::filter("pane:nobody", nothing);
        }
        if (step == 2) {
            // The invoke of an unknown pane changed nothing.
            CHECK(filter_of("pane:npc") == nothing);
            CHECK(filter_of("pane:you").empty());
            step = 3;
            return ts::filter("pane:you", nothing);
        }
        if (step == 3) {
            CHECK(pipes == nullptr);
            CHECK(filter_of("pane:you") == nothing);
            step = 4;
            return ts::filter("pane:npc", "");
        }
        if (step == 4) {
            CHECK(bits != nullptr);
            CHECK(halligan != nullptr);
            CHECK(pipes == nullptr);
            CHECK(filter_of("pane:npc").empty());
            step = 5;
            return ts::filter("pane:you", "");
        }
        REQUIRE(step == 5);
        CHECK(pipes != nullptr);
        CHECK(filter_of("pane:you").empty());
        step = 6;
        return ts::invoke("trade:cancel");
    });

    CHECK_FALSE(
        trading_window(state).perform_trade(*fixture.trader, npc_trading::deal_kind::trade));
    CHECK(step == 6);
}

TEST_CASE(
    "NPC trade in category mode highlights the category of the row a count is set on",
    "[client][interaction][npc_trade][engine_client_trade][mcp]") {
    const auto guard = screen_guard{};
    const auto fixture = trade_fixture{};
    fixture.trader->mission = NPC_MISSION_SHOPKEEP;
    fixture.add_npc_item(itype_id("test_platinum_bit"), 10);
    fixture.add_npc_item(itype_id("test_halligan"));
    fixture.add_npc_item(itype_id("test_pipe"));
    auto state = npc_trading::trade_state{};
    npc_trading::setup_trade_state(state, 0, *fixture.trader);
    // The heading that governs a row: the last one above it.
    const auto heading_of = [](const std::vector<ec::row>& all, const std::string& id) {
        const ec::row* heading = nullptr;
        for (const auto& entry : all) {
            if (entry.header) { heading = &entry; }
            if (entry.id == id) { return heading; }
        }
        return static_cast<const ec::row*>(nullptr);
    };
    auto pending = std::vector<std::string>{};
    auto checked = std::size_t{0};
    auto started = false;

    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto screen = ts::top();
        REQUIRE(screen);
        if (!started) {
            started = true;
            // The rows to count, last row first.
            const auto& all = ts::rows(*screen, "list:npc");
            std::ranges::for_each(all | std::views::reverse, [&](const ec::row& entry) {
                if (!entry.header) { pending.push_back(entry.id); }
            });
            const auto toggle = inp_mngr.get_input_for_action("CATEGORY_SELECTION", "NPC_TRADE");
            REQUIRE_FALSE(toggle.empty());
            return toggle.front();
        }
        const auto& head = ts::body<ec::panel_body>(ts::part(*screen, "trade:head"));
        REQUIRE_FALSE(head.stats.empty());
        CHECK(head.stats.front().value == "ON");
        const auto& all = ts::rows(*screen, "list:npc");
        if (checked > 0) {
            const auto* const heading = heading_of(all, pending[checked - 1]);
            REQUIRE(heading != nullptr);
            CHECK(heading->selected);
            CHECK(std::ranges::count_if(
                      all, [](const ec::row& entry) { return entry.header && entry.selected; })
                  == 1);
        }
        if (checked == pending.size()) { return ts::invoke("trade:cancel"); }
        return ts::set_count(pending[checked++], 1);
    });

    CHECK_FALSE(
        trading_window(state).perform_trade(*fixture.trader, npc_trading::deal_kind::trade));
    CHECK(checked == pending.size());
    CHECK(checked == 3);
}

TEST_CASE(
    "NPC trade confirmed through its affordance exchanges items and updates priced debt",
    "[client][interaction][npc_trade][engine_client_trade][mcp]") {
    const auto guard = screen_guard{};
    const auto fixture = trade_fixture{};
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
        if (const auto screen = ts::top()) {
            if (step == 0) {
                const auto* const target = ts::item_row(*screen, "list:npc", "test_pipe");
                REQUIRE(target != nullptr);
                step = 1;
                return ts::set_count(target->id, 1);
            }
            REQUIRE(step == 1);
            CHECK(get_avatar().amount_of(itype_id("test_pipe")) == original_player_count);
            CHECK(fixture.trader->amount_of(itype_id("test_pipe")) == original_npc_count);
            CHECK(fixture.trader->op_of_u.owed == original_debt);
            step = 2;
            return ts::invoke("trade:confirm");
        }
        // The window is under the question it asked, and offers nothing while it is covered.
        const auto stack = ec::interaction_stack(ts::live_state());
        REQUIRE(stack.size() == 3);
        CHECK(stack[1].context == "NPC_TRADE");
        CHECK(stack[1].affordances.empty());
        REQUIRE(ec::top_screen(ts::captured()).context == "YESNO");
        REQUIRE(step == 2);
        step = 3;
        return ts::answer("YES");
    });

    CHECK(npc_trading::trade(*fixture.trader, 0, npc_trading::deal_kind::trade));
    CHECK(step == 3);
    CHECK(get_avatar().amount_of(itype_id("test_pipe")) == original_player_count + 1);
    CHECK(fixture.trader->amount_of(itype_id("test_pipe")) == original_npc_count - 1);
    CHECK(fixture.trader->op_of_u.owed == original_debt - unit_price);
    CHECK(sold.is_owned_by(get_avatar()));
}

TEST_CASE(
    "NPC trade preserves native credit and capacity denials",
    "[client][interaction][npc_trade][engine_client_trade][mcp]") {
    const auto guard = screen_guard{};

    SECTION("insufficient credit") {
        const auto fixture = trade_fixture{};
        fixture.trader->personality.altruism = 0;
        auto& candidate = fixture.add_npc_item(itype_id("test_pipe"));
        auto state = npc_trading::trade_state{};
        state.theirs.emplace_back(std::vector<item*>{&candidate}, 1000, 1);
        const auto original_debt = fixture.trader->op_of_u.owed;
        auto step = 0;
        game_client::memory::set_input_provider([&](const int /*timeout*/) {
            if (const auto screen = ts::top()) {
                if (step == 0) {
                    const auto* const target = ts::item_row(*screen, "list:npc", "test_pipe");
                    REQUIRE(target != nullptr);
                    step = 1;
                    return ts::set_count(target->id, 1);
                }
                if (step == 1) {
                    step = 2;
                    return ts::invoke("trade:confirm");
                }
                REQUIRE(step == 3);
                step = 4;
                return ts::invoke("trade:cancel");
            }
            REQUIRE(ec::top_screen(ts::captured()).context == "POPUP_WAIT");
            REQUIRE(step == 2);
            CHECK(ts::popup_message().find("offer") != std::string::npos);
            step = 3;
            return ts::answer();
        });

        CHECK_FALSE(
            trading_window(state).perform_trade(*fixture.trader, npc_trading::deal_kind::trade));
        CHECK(step == 4);
        CHECK(fixture.trader->amount_of(itype_id("test_pipe")) == 1);
        CHECK(get_avatar().amount_of(itype_id("test_pipe")) == 0);
        CHECK(fixture.trader->op_of_u.owed == original_debt);
    }

    SECTION("NPC carry capacity") {
        const auto fixture = trade_fixture{};
        auto& candidate = fixture.add_player_item(itype_id("test_overweight_cube"));
        auto state = npc_trading::trade_state{};
        state.yours.emplace_back(std::vector<item*>{&candidate}, 1000, 1);
        auto step = 0;
        game_client::memory::set_input_provider([&](const int /*timeout*/) {
            if (const auto screen = ts::top()) {
                if (step == 0) {
                    const auto* const target =
                        ts::item_row(*screen, "list:you", "test_overweight_cube");
                    REQUIRE(target != nullptr);
                    step = 1;
                    return ts::set_count(target->id, 1);
                }
                if (step == 1) {
                    step = 2;
                    return ts::invoke("trade:confirm");
                }
                REQUIRE(step == 3);
                step = 4;
                return ts::invoke("trade:cancel");
            }
            REQUIRE(ec::top_screen(ts::captured()).context == "POPUP_WAIT");
            REQUIRE(step == 2);
            CHECK(ts::popup_message().find("carry") != std::string::npos);
            step = 3;
            return ts::answer();
        });

        CHECK_FALSE(
            trading_window(state).perform_trade(*fixture.trader, npc_trading::deal_kind::trade));
        CHECK(step == 4);
        CHECK(get_avatar().amount_of(itype_id("test_overweight_cube")) == 1);
        CHECK(fixture.trader->amount_of(itype_id("test_overweight_cube")) == 0);
    }
}

TEST_CASE(
    "NPC trade publishes the same stack at every window size",
    "[client][interaction][npc_trade][engine_client_trade][mcp]") {
    const auto guard = screen_guard{};
    const auto fixture = trade_fixture{};
    fixture.trader->mission = NPC_MISSION_SHOPKEEP;
    fixture.add_npc_item(itype_id("test_platinum_bit"), 10);
    fixture.add_npc_item(itype_id("test_halligan"));
    std::ranges::for_each(std::views::iota(0, 3), [&](const auto /*index*/) {
        fixture.add_player_item(itype_id("test_pipe"));
    });
    auto state = npc_trading::trade_state{};
    npc_trading::setup_trade_state(state, 0, *fixture.trader);
    auto wires = std::vector<nlohmann::json>{};
    resize_terminal(80, 24);

    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        wires.push_back(stack_json());
        if (wires.size() > 1) { return ts::invoke("trade:cancel"); }
        // The window is laid out again for the larger terminal before the next read.
        resize_terminal(200, 60);
        return ts::filter("pane:npc", "");
    });

    CHECK_FALSE(
        trading_window(state).perform_trade(*fixture.trader, npc_trading::deal_kind::trade));
    REQUIRE(wires.size() == 2);
    CHECK(wires[0].back()["root"]["kind"] == "panel");
    CHECK(wires[0] == wires[1]);
}

TEST_CASE(
    "NPC trade has the same ids and shape in every language",
    "[client][interaction][npc_trade][engine_client_trade][engine_client_locale][mcp]") {
    const auto catalogue = locale_fixture::korean_catalogue{};
    const auto guard = screen_guard{};
    const auto fixture = trade_fixture{};
    fixture.trader->mission = NPC_MISSION_SHOPKEEP;
    fixture.add_npc_item(itype_id("test_platinum_bit"), 10);
    fixture.add_player_item(itype_id("test_pipe"));
    auto state = npc_trading::trade_state{};
    npc_trading::setup_trade_state(state, 0, *fixture.trader);
    auto reads = 0;

    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        ++reads;
        const auto english = stack_json();
        auto korean = nlohmann::json{};
        {
            const auto scope = localization::locale_scope{locale_fixture::korean};
            korean = stack_json();
        }
        // The words differ, and the oracle is the catalogue: the title is composed from its word.
        CHECK(english.back()["title"] != korean.back()["title"]);
        CHECK(korean.back()["title"].get<std::string>().starts_with(
            locale_fixture::in_korean("Trading with")));
        // What a recorded input names is the same, and so is everything but the words.
        CHECK(ids_of(english) == ids_of(korean));
        CHECK(without_text(english) == without_text(korean));
        return ts::invoke("trade:cancel");
    });

    CHECK_FALSE(
        trading_window(state).perform_trade(*fixture.trader, npc_trading::deal_kind::trade));
    CHECK(reads == 1);
}

TEST_CASE(
    "NPC trade title follows the kind the caller names, not the wording of its language",
    "[client][interaction][npc_trade][engine_client_trade][engine_client_locale][mcp]") {
    const auto catalogue = locale_fixture::korean_catalogue{};
    const auto guard = screen_guard{};
    const auto fixture = trade_fixture{};
    const auto [kind, word] =
        GENERATE(std::pair{npc_trading::deal_kind::pay, std::string{"Paying"}},
                 std::pair{npc_trading::deal_kind::reward, std::string{"Accepting a reward from"}});
    INFO(word);
    auto state = npc_trading::trade_state{};
    npc_trading::setup_trade_state(state, 0, *fixture.trader);
    auto reads = 0;

    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        ++reads;
        const auto english = stack_json();
        auto korean = nlohmann::json{};
        {
            const auto scope = localization::locale_scope{locale_fixture::korean};
            korean = stack_json();
        }
        CHECK(english.back()["title"].get<std::string>().starts_with(word));
        CHECK(
            korean.back()["title"].get<std::string>().starts_with(locale_fixture::in_korean(word)));
        return ts::invoke("trade:cancel");
    });

    CHECK_FALSE(trading_window(state).perform_trade(*fixture.trader, kind));
    CHECK(reads == 1);
}

TEST_CASE(
    "NPC trade item popup is a screen of its own and closes by an invoke",
    "[client][interaction][npc_trade][engine_client_trade][mcp]") {
    const auto guard = screen_guard{};
    const auto fixture = trade_fixture{};
    fixture.trader->mission = NPC_MISSION_SHOPKEEP;
    fixture.add_npc_item(itype_id("test_platinum_bit"), 10);
    auto state = npc_trading::trade_state{};
    npc_trading::setup_trade_state(state, 0, *fixture.trader);
    auto step = 0;

    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        if (step == 0) {
            REQUIRE(ts::top());
            step = 1;
            auto command = game_client::input_command{};
            command.action = "EXAMINE";
            const auto examine =
                game_client::resolve_input_command(command, game_client::memory::screen_size());
            REQUIRE(examine.has_value());
            return *examine;
        }
        if (step == 1) {
            // The window is under the popup, which is published as a screen of its own.
            const auto captured = ts::captured();
            const auto stack = ec::interaction_stack(captured);
            REQUIRE(stack.size() == 3);
            CHECK(stack[1].affordances.empty());
            const auto& popup = stack.back();
            CHECK_FALSE(popup.screen_id.starts_with("screen:read:"));
            CHECK(popup.screen_id != stack[1].screen_id);
            CHECK(popup.role == "dialog");
            REQUIRE(popup.root);
            CHECK(ts::part(popup, "text:info").id == "text:info");
            const auto operation = ec::resolve_invoke(captured, {.affordance_id = "QUIT"});
            REQUIRE(operation.has_value());
            const auto* const registered = std::get_if<ec::registered_action>(&*operation);
            REQUIRE(registered != nullptr);
            auto command = game_client::input_command{};
            command.action = registered->id;
            command.input_id = game_client::current_input_id();
            const auto closed =
                game_client::resolve_input_command(command, game_client::memory::screen_size());
            REQUIRE(closed.has_value());
            step = 2;
            return *closed;
        }
        // The popup is closed: the window is the top screen again.
        REQUIRE(step == 2);
        CHECK(ec::interaction_stack(ts::captured()).size() == 2);
        step = 3;
        return ts::invoke("trade:cancel");
    });

    CHECK_FALSE(
        trading_window(state).perform_trade(*fixture.trader, npc_trading::deal_kind::trade));
    CHECK(step == 3);
}

TEST_CASE(
    "replay repeats an NPC trade made through its affordances",
    "[client][interaction][npc_trade][engine_client_trade][replay][mcp]") {
    const auto guard = screen_guard{};
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

    {
        const auto fixture = trade_fixture{};
        fixture.add_npc_item(itype_id("test_pipe"));
        fixture.trader->op_of_u.owed = 100000;
        replay::configure_recording(path.string(), {.rng_seed = 41});
        replay::start();
        auto step = 0;
        game_client::memory::set_input_provider([&](const int /*timeout*/) {
            if (const auto screen = ts::top()) {
                if (step == 0) {
                    const auto* const target = ts::item_row(*screen, "list:npc", "test_pipe");
                    REQUIRE(target != nullptr);
                    step = 1;
                    return ts::set_count(target->id, 1);
                }
                REQUIRE(step == 1);
                step = 2;
                return ts::invoke("trade:confirm");
            }
            REQUIRE(ec::top_screen(ts::captured()).context == "YESNO");
            REQUIRE(step == 2);
            step = 3;
            return ts::answer("YES");
        });
        CHECK(npc_trading::trade(*fixture.trader, 0, npc_trading::deal_kind::trade));
        CHECK(step == 3);
        CHECK(get_avatar().amount_of(itype_id("test_pipe")) == 1);
        CHECK(fixture.trader->amount_of(itype_id("test_pipe")) == 0);
        replay::finish();
    }

    {
        const auto fixture = trade_fixture{};
        fixture.add_npc_item(itype_id("test_pipe"));
        fixture.trader->op_of_u.owed = 100000;
        game_client::memory::set_input_provider([](const int /*timeout*/) -> input_event {
            throw std::runtime_error("NPC trade playback must not read live input");
        });
        replay::configure_playback(path.string(), {.rng_seed = 41});
        replay::start();
        CHECK(npc_trading::trade(*fixture.trader, 0, npc_trading::deal_kind::trade));
        CHECK_THROWS_AS(replay::next_input_event(), replay::completed);
        CHECK(get_avatar().amount_of(itype_id("test_pipe")) == 1);
        CHECK(fixture.trader->amount_of(itype_id("test_pipe")) == 0);
        replay::finish();
    }
}

#endif
