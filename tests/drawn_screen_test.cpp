#if defined(CATA_MCP)

#    include "advanced_inv.h"
#    include "avatar.h"
#    include "cached_options.h"
#    include "calendar.h"
#    include "catacharset.h"
#    include "catch/catch.hpp"
#    include "client_memory.h"
#    include "client_memory_scope.h"
#    include "cursesdef.h"
#    include "engine_client_session.h"
#    include "engine_client_world.h"
#    include "game.h"
#    include "game_constants.h"
#    include "game_inventory.h"
#    include "game_session.h"
#    include "game_ui.h"
#    include "input.h"
#    include "item.h"
#    include "map/map.h"
#    include "map_helpers.h"
#    include "mcp_server.h"
#    include "options_helpers.h"
#    include "output.h"
#    include "player_helpers.h"
#    include "state_helpers.h"
#    include "ui_manager.h"
#    include "uistate.h"

#    include <algorithm>
#    include <functional>
#    include <memory>
#    include <nlohmann/json.hpp>
#    include <optional>
#    include <ranges>
#    include <sstream>
#    include <string>
#    include <string_view>
#    include <utility>
#    include <vector>

namespace {
using json = nlohmann::json;

constexpr auto test_pos = tripoint_bub_ms{60, 60, 0};

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

/// An invoke of the top screen's affordance.
struct invocation {
    std::string affordance;
    json args = json::object();
};

/// A protocol client on the real server and session, answering the native reads of the real
/// screens.
struct wire_client {
    engine_client::session authority;
    std::vector<bn::mcp::key_event> queued;
    std::ostringstream output;
    std::ostringstream errors;
    bn::mcp::server server{
        {
            .submit =
                [this](const auto& input) {
                    queued = input;
                    return true;
                },
            .has_input = [this] { return !queued.empty(); },
            .contract_session = &authority,
        },
    };
    bool greeted = false;
    int next_id = 10;

    wire_client() {
        authority.set_world_capture(
            &engine_client::world::capture_world, &engine_client::world::capture_patch);
    }

    /// The members of the response to the request `id` among the frames written so far.
    auto response(const int id) const -> json {
        auto lines = std::istringstream{output.str()};
        for (auto line = std::string{}; std::getline(lines, line);) {
            const auto frame = json::parse(line);
            if (frame.contains("id") && frame["id"] == id) { return frame; }
        }
        FAIL("no response to request " << id << ": " << output.str());
        return {};
    }

    auto request(const std::string& method, const json& params) -> std::string {
        return json{{"jsonrpc", "2.0"}, {"id", ++next_id}, {"method", method}, {"params", params}}
                   .dump()
             + "\n";
    }

    /// The top screen of the stack, as a connection reads it at this native read.
    auto top() -> json {
        REQUIRE(authority.publish_boundary());
        auto text = std::string{};
        if (!greeted) {
            text =
                request("bn.hello",
                        {{"versions", {"0.3"}}, {"client", {{"name", "test"}, {"version", "1"}}}});
            greeted = true;
        }
        text += request("bn.subscribe", json::object());
        output.str("");
        auto input = std::istringstream{text};
        for (auto lines = std::ranges::count(text, '\n'); lines > 0; --lines) {
            REQUIRE(server.poll_input(input, output, errors));
        }
        const auto header = response(next_id);
        REQUIRE(header.contains("result"));
        return header["result"]["interaction"]["stack"].back();
    }

    /// The expectation an invoke of `found` on `screen` carries, as its `expect` says.
    auto expect_of(const json& screen, const json& found) -> json {
        if (found["expect"] == "screen") { return {{"screen_id", screen["screen_id"]}}; }
        const auto current = authority.current();
        REQUIRE(current);
        return {{"boundary_id", current->value.interaction.id},
                {"revision", std::to_string(current->at.revision)}};
    }

    auto invoke_request(const json& screen, const invocation& call) -> std::string {
        const auto& offered = screen["affordances"];
        const auto found = std::ranges::find_if(offered, [&](const json& entry) {
            return entry["id"].get<std::string>() == call.affordance;
        });
        INFO("affordance " << call.affordance);
        REQUIRE(found != offered.end());
        return request(
            "bn.command.submit",
            {{"epoch", authority.epoch()},
             {"expect", expect_of(screen, *found)},
             {"operation",
              {{"kind", "invoke"}, {"affordance_id", call.affordance}, {"args", call.args}}}});
    }

    /// The input the native read takes from `call`, after the server has turned away each of
    /// `refused`.
    auto invoke(
        const json& screen, const invocation& call, const std::vector<invocation>& refused = {})
        -> input_event {
        auto text = std::string{};
        for (const auto& entry : refused) { text += invoke_request(screen, entry); }
        text += invoke_request(screen, call);
        output.str("");
        auto input = std::istringstream{text};
        REQUIRE(server.pump_until_input(input, output, errors));
        INFO(output.str());
        CHECK(
            std::ranges::count_if(
                std::views::split(output.str(), '\n'),
                [](const auto& line) {
                    return std::string_view{line}.find("\"stage\":\"rejected\"")
                        != std::string_view::npos;
                })
            == std::ssize(refused));
        REQUIRE(queued.size() == 1);
        const auto native =
            game_client::resolve_input_command(queued.front(), game_client::memory::screen_size());
        REQUIRE(native);
        auto delivered = authority.delivered(*native);
        queued.clear();
        return delivered;
    }
};

auto is_canvas(const json& screen) -> bool {
    return screen["root"].is_object() && screen["root"]["kind"] == "canvas";
}

/// The drawn rows of a canvas, as text without their colors.
auto rows_of(const json& screen) -> std::vector<std::string> {
    REQUIRE(is_canvas(screen));
    return screen["root"]["rows"] | std::views::transform([](const json& row) {
               auto text = std::string{};
               for (const auto& run : row) { text += run[0].get<std::string>(); }
               return text;
           })
         | std::ranges::to<std::vector>();
}

auto grid_contains(const json& screen, const std::string& text) -> bool {
    return std::ranges::any_of(rows_of(screen), [&](const std::string& row) {
        return row.find(text) != std::string::npos;
    });
}

/// The column of the terminal where `text` is first drawn.
auto column_of(const json& screen, const std::string& text) -> std::optional<int> {
    for (const auto& row : rows_of(screen)) {
        const auto at = row.find(text);
        if (at != std::string::npos) { return utf8_width(row.substr(0, at)); }
    }
    return std::nullopt;
}

auto affordance_of(const json& screen, const std::string& id) -> json {
    const auto& offered = screen["affordances"];
    const auto found = std::ranges::find_if(offered, [&](const json& entry) {
        return entry["id"].get<std::string>() == id;
    });
    REQUIRE(found != offered.end());
    return *found;
}

auto cube_count_on_ground() -> int {
    return std::ranges::count_if(get_map().i_at(test_pos), [](const auto* candidate) {
        return candidate->typeId() == itype_id("test_1kg_cube");
    });
}

auto cube_count_in_inventory() -> int { return get_avatar().amount_of(itype_id("test_1kg_cube")); }

struct aim_state {
    advanced_inv_save_state old_transfer_save = uistate.transfer_save;

    aim_state() {
        uistate.transfer_save = advanced_inv_save_state{};
        uistate.transfer_save.active_left = true;
        uistate.transfer_save.saved_area = AIM_INVENTORY;
        uistate.transfer_save.saved_area_right = AIM_CENTER;
        uistate.transfer_save.pane.area_idx = AIM_INVENTORY;
        uistate.transfer_save.pane_right.area_idx = AIM_CENTER;
    }
    ~aim_state() { uistate.transfer_save = old_transfer_save; }
};

} // namespace

TEST_CASE(
    "a client moves an item through the advanced inventory it reads as a drawn screen",
    "[drawn_screen][client][mcp]") {
    clear_all_state();
    clear_map();
    const auto state = aim_state{};
    auto& you = get_avatar();
    clear_character(you, false);
    const auto guard = screen_guard{};
    you.setpos(map_local_to_abs(get_map(), test_pos));
    you.set_moves(1000);
    you.worn.push_back(item::spawn("duffelbag"));
    you.i_add(item::spawn("test_1kg_cube", calendar::turn));
    REQUIRE(cube_count_in_inventory() == 1);
    REQUIRE(cube_count_on_ground() == 0);
    const auto no_autosave = override_option("AUTOSAVE", "false");
    const auto keep_bubble =
        override_option("REALITY_BUBBLE_SIZE", std::to_string(g_reality_bubble_size));
    const auto no_mobile = override_option("ACTIVITY_MOBILE_BUBBLE_SIZE", "0");
    const auto no_idle = override_option("ACTIVITY_IDLE_BUBBLE_SIZE", "0");
    const auto no_underground = override_option("UNDERGROUND_BUBBLE_SIZE", "0");
    const auto no_vehicle = override_option("VEHICLE_BUBBLE_SIZE", "0");
    const auto no_combat = override_option("COMBAT_BUBBLE_SIZE", "0");
    g->new_game = false;
    const auto running = game_session::running();
    game_session::set_running(true);

    auto client = wire_client{};
    const auto cube_name = item::spawn("test_1kg_cube", calendar::turn)->tname();
    auto reads = 0;
    auto before = std::vector<std::string>{};
    auto half = 0;
    auto closed = false;
    game_client::memory::set_input_provider([&](const int timeout) -> input_event {
        // The native loop polls for keys while an activity runs; no client has pressed one.
        if (timeout == 0) { return input_event{}; }
        const auto top = client.top();
        switch (reads++) {
            case 0: {
                CHECK(top["context"] == "ADVANCED_INVENTORY");
                CHECK(top["kind"] == "advanced_inventory");
                CHECK(top["role"] == "screen");
                CHECK(top["root"]["id"] == "canvas:screen");
                half = top["root"]["cols"].get<int>() / 2;
                const auto column = column_of(top, cube_name);
                REQUIRE(column);
                CHECK(*column < half);
                const auto move = affordance_of(top, "MOVE_ITEM_STACK");
                CHECK(move["label"].get<std::string>() != "MOVE_ITEM_STACK");
                CHECK_FALSE(move["keys"].empty());
                CHECK(move["expect"] == "screen");
                const auto wire = top.dump();
                CAPTURE(wire.size());
                CHECK(wire.size() < engine_client::maximum_inline_bytes);
                before = rows_of(top);
                return client.invoke(top, {.affordance = "MOVE_ITEM_STACK"});
            }
            case 1: {
                // The move runs as an activity, which reopens the screen where it was.
                CHECK(top["kind"] == "advanced_inventory");
                CHECK(cube_count_on_ground() == 1);
                CHECK(cube_count_in_inventory() == 0);
                CHECK(rows_of(top) != before);
                const auto column = column_of(top, cube_name);
                REQUIRE(column);
                CHECK(*column >= half);
                closed = true;
                you.set_moves(0);
                return client.invoke(top, {.affordance = "QUIT"});
            }
            default:
                FAIL("the screen should be closed at read " << reads);
                return input_event{};
        }
    });
    create_advanced_inv();
    for (auto turn = 0; !closed && turn < 50; ++turn) { CHECK_FALSE(g->do_turn()); }
    game_session::set_running(running);
    CHECK(reads == 2);
    CHECK(cube_count_on_ground() == 1);
    CHECK(cube_count_in_inventory() == 0);
}

TEST_CASE(
    "a screen with no code of its own is shown and run by its context",
    "[drawn_screen][client][mcp]") {
    clear_all_state();
    const auto guard = screen_guard{};
    const auto left = item::spawn("test_1kg_cube", calendar::turn);
    const auto right = item::spawn("test_platinum_bit", calendar::turn);
    auto client = wire_client{};
    auto reads = 0;
    game_client::memory::set_input_provider([&](const int /*timeout*/) -> input_event {
        const auto top = client.top();
        switch (reads++) {
            case 0: {
                CHECK(top["kind"] == "default");
                CHECK(grid_contains(top, left->tname()));
                CHECK(grid_contains(top, right->tname()));
                CHECK(affordance_of(top, "PAGE_DOWN")["role"] == "navigate");
                CHECK(affordance_of(top, "QUIT")["role"] == "cancel");
                CHECK(affordance_of(top, "bn:key")["args"].contains("text"));
                return client.invoke(top, {.affordance = "DOWN"});
            }
            case 1: {
                // A name that is no key is refused; a key takes the path of the native read, where
                // the context maps it to its action.
                return client
                    .invoke(top, {.affordance = "bn:key", .args = {{"text", "ESC"}}},
                            {{.affordance = "bn:key", .args = {{"text", "NO_SUCH_KEY"}}}});
            }
            default:
                FAIL("the screen should be closed at read " << reads);
                return input_event{};
        }
    });
    game_menus::inv::compare(*left, *right);
    CHECK(reads == 2);
}

TEST_CASE(
    "a drawn screen keeps its id between reads and shows only a small top window",
    "[drawn_screen][client][mcp]") {
    const auto guard = screen_guard{};
    auto window = catacurses::newwin(5, 20, point(10, 10));
    auto ui = ui_adaptor{};
    ui.position_from_window(window);
    ui.on_redraw([&](const ui_adaptor&) {
        mvwprintz(window, point_zero, c_light_gray, "Marked");
        catacurses::wnoutrefresh(window);
    });
    auto ctxt = input_context{"TEST_DRAWN_SCREEN"};
    ctxt.register_action("SELECT");
    ctxt.register_action("QUIT");
    auto client = wire_client{};
    auto reads = 0;
    auto id = json{};
    game_client::memory::set_input_provider([&](const int /*timeout*/) -> input_event {
        const auto top = client.top();
        switch (reads++) {
            case 0: {
                CHECK(top["kind"] == "test_drawn_screen");
                CHECK(top["root"]["cols"] == 20);
                CHECK(rows_of(top).size() == 5);
                CHECK(column_of(top, "Marked") == 0);
                id = top["screen_id"];
                return client.invoke(top, {.affordance = "SELECT"});
            }
            default:
                CHECK(top["screen_id"] == id);
                return client.invoke(top, {.affordance = "QUIT"});
        }
    });
    ui_manager::redraw();
    CHECK(ctxt.handle_input() == "SELECT");
    ui_manager::redraw();
    CHECK(ctxt.handle_input() == "QUIT");
}
TEST_CASE(
    "a drawn screen shows what its windows cover, not what the map and sidebar drew between them",
    "[drawn_screen][client][mcp]") {
    const auto guard = screen_guard{};
    auto behind = catacurses::newwin(TERMY, TERMX, point_zero);
    auto first = catacurses::newwin(3, 20, point(10, 10));
    auto second = catacurses::newwin(3, 20, point(40, 12));
    auto background = ui_adaptor{};
    background.set_background();
    background.position_from_window(behind);
    background.on_redraw([&](const ui_adaptor&) {
        mvwprintz(behind, point(30, 11), c_light_gray, "Behind");
        mvwprintz(behind, point(0, 0), c_light_gray, "Sidebar");
        catacurses::wnoutrefresh(behind);
    });
    auto ui_first = ui_adaptor{};
    ui_first.position_from_window(first);
    ui_first.on_redraw([&](const ui_adaptor&) {
        mvwprintz(first, point_zero, c_light_gray, "First");
        catacurses::wnoutrefresh(first);
    });
    auto ui_second = ui_adaptor{};
    ui_second.position_from_window(second);
    ui_second.on_redraw([&](const ui_adaptor&) {
        mvwprintz(second, point_zero, c_light_gray, "Second");
        catacurses::wnoutrefresh(second);
    });
    auto ctxt = input_context{"TEST_DRAWN_LAYERS"};
    ctxt.register_action("QUIT");
    auto client = wire_client{};
    game_client::memory::set_input_provider([&](const int /*timeout*/) -> input_event {
        const auto top = client.top();
        CHECK(top["root"]["cols"] == 50);
        CHECK(rows_of(top).size() == 5);
        CHECK(grid_contains(top, "First"));
        CHECK(grid_contains(top, "Second"));
        CHECK_FALSE(grid_contains(top, "Behind"));
        CHECK_FALSE(grid_contains(top, "Sidebar"));
        return client.invoke(top, {.affordance = "QUIT"});
    });
    ui_manager::redraw();
    CHECK(ctxt.handle_input() == "QUIT");
}

TEST_CASE(
    "the sidebar canvas keeps the cells a window above the game covers",
    "[drawn_screen][client][mcp]") {
    const auto guard = screen_guard{};
    auto behind = catacurses::newwin(TERMY, TERMX, point_zero);
    auto modal = catacurses::newwin(3, 30, point(70, 0));
    auto background = ui_adaptor{};
    background.set_background();
    background.position_from_window(behind);
    background.on_redraw([&](const ui_adaptor&) {
        mvwprintz(behind, point(80, 0), c_light_gray, "Sidebar");
        catacurses::wnoutrefresh(behind);
    });
    const auto sidebar = rectangle<point>{point(70, 0), point(100, 3)};
    const auto text_of = [](const game_client::interaction_canvas& canvas) {
        auto result = std::string{};
        for (const auto& row : canvas.rows) {
            for (const auto& run : row) { result += run.text; }
        }
        return result;
    };
    ui_manager::redraw();
    CHECK(text_of(game_client::terminal_canvas(sidebar)).contains("Sidebar"));
    auto ui_modal = ui_adaptor{};
    ui_modal.position_from_window(modal);
    ui_modal.on_redraw([&](const ui_adaptor&) {
        mvwprintz(modal, point_zero, c_light_gray, "Modal");
        catacurses::wnoutrefresh(modal);
    });
    ui_manager::redraw();
    const auto covered = text_of(game_client::terminal_canvas(sidebar));
    CHECK(covered.contains("Sidebar"));
    CHECK_FALSE(covered.contains("Modal"));
}

TEST_CASE(
    "the sidebar canvas holds no cell of a window that closed before the game redrew its sidebar",
    "[drawn_screen][client][mcp]") {
    const auto guard = screen_guard{};
    auto behind = catacurses::newwin(TERMY, TERMX, point_zero);
    auto modal = catacurses::newwin(3, 30, point(70, 0));
    auto background = ui_adaptor{};
    background.set_background();
    background.position_from_window(behind);
    background.on_redraw([&](const ui_adaptor&) {
        mvwprintz(behind, point(80, 0), c_light_gray, "Sidebar");
        catacurses::wnoutrefresh(behind);
    });
    const auto sidebar = rectangle<point>{point(70, 0), point(100, 3)};
    const auto text_of = [](const game_client::interaction_canvas& canvas) {
        auto result = std::string{};
        for (const auto& row : canvas.rows) {
            for (const auto& run : row) { result += run.text; }
        }
        return result;
    };
    ui_manager::redraw();
    {
        auto ui_modal = ui_adaptor{};
        ui_modal.position_from_window(modal);
        ui_modal.on_redraw([&](const ui_adaptor&) {
            mvwprintz(modal, point_zero, c_light_gray, "Modal");
            catacurses::wnoutrefresh(modal);
        });
        ui_manager::redraw();
        CHECK_FALSE(text_of(game_client::terminal_canvas(sidebar)).contains("Modal"));
    }
    // The window is gone and the game has not drawn over its cells yet: they are not the sidebar's.
    CHECK_FALSE(text_of(game_client::terminal_canvas(sidebar)).contains("Modal"));
}

TEST_CASE(
    "the sidebar canvas shows what the game redrew after a window above it closed",
    "[drawn_screen][client][mcp]") {
    const auto guard = screen_guard{};
    auto behind = catacurses::newwin(TERMY, TERMX, point_zero);
    auto modal = catacurses::newwin(3, 30, point(70, 0));
    auto line = std::string{"Before"};
    auto background = ui_adaptor{};
    background.set_background();
    background.position_from_window(behind);
    background.on_redraw([&](const ui_adaptor&) {
        werase(behind);
        mvwprintz(behind, point(70, 1), c_light_gray, line);
        catacurses::wnoutrefresh(behind);
    });
    const auto sidebar = rectangle<point>{point(70, 0), point(100, 3)};
    const auto text_of = [](const game_client::interaction_canvas& canvas) {
        auto result = std::string{};
        for (const auto& row : canvas.rows) {
            for (const auto& run : row) { result += run.text; }
        }
        return result;
    };
    ui_manager::redraw();
    {
        auto ui_modal = ui_adaptor{};
        ui_modal.position_from_window(modal);
        ui_modal.on_redraw([&](const ui_adaptor&) {
            mvwprintz(modal, point(0, 1), c_light_gray, "Modal");
            catacurses::wnoutrefresh(modal);
        });
        ui_manager::redraw();
        CHECK(text_of(game_client::terminal_canvas(sidebar)).contains("Before"));
    }
    line = "After";
    ui_manager::redraw();
    const auto redrawn = text_of(game_client::terminal_canvas(sidebar));
    CHECK(redrawn.contains("After"));
    CHECK_FALSE(redrawn.contains("Before"));
    CHECK_FALSE(redrawn.contains("Modal"));
}

TEST_CASE(
    "a client viewport smaller than the main menu needs still leaves a terminal it lays out in",
    "[drawn_screen]") {
    const auto scope = game_client::memory::scoped_state{};
    game_client::memory::initialize(80, 24);
    game_ui::resize_terrain_window({40, 25});
    CHECK(game_client::memory::screen_size() == game_client::memory::minimum_terminal);
}

#endif // CATA_MCP
