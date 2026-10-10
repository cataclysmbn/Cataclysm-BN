#if defined(CATA_MCP)

#    include "cata_utility.h"
#    include "catch/catch.hpp"
#    include "client_input.h"
#    include "client_memory.h"
#    include "client_memory_scope.h"
#    include "cursesdef.h"
#    include "cursesport.h"
#    include "engine_client_contract.h"
#    include "game.h"
#    include "input.h"
#    include "output.h"
#    include "state_helpers.h"

#    include <string>

namespace {

struct memory_screen_guard {
    const game_client::memory::scoped_state memory;
    const int termx = TERMX;
    const int termy = TERMY;

    ~memory_screen_guard() {
        TERMX = termx;
        TERMY = termy;
    }
};

} // namespace

TEST_CASE("memory_screen_guard leaves a borrowed compositor intact", "[client][mcp]") {
    const auto outer = game_client::memory::scoped_state{};
    game_client::memory::resize(7, 3);
    catacurses::stdscr = catacurses::newwin(3, 7, point_zero);
    catacurses::mvwprintw(catacurses::stdscr, point_zero, "x");
    catacurses::wrefresh(catacurses::stdscr);
    const auto borrowed = catacurses::stdscr;
    { const auto guard = memory_screen_guard{}; }
    CHECK(game_client::memory::screen_size() == point(7, 3));
    CHECK(game_client::memory::snapshot().cells[0].text == "x");
    CHECK(catacurses::stdscr.get<cata_cursesport::WINDOW>()
          == borrowed.get<cata_cursesport::WINDOW>());
    CHECK(catacurses::stdscr.get<cata_cursesport::WINDOW>()->width == 7);
}

TEST_CASE("generic memory client composes overlapping windows", "[client][mcp]") {
    auto guard = memory_screen_guard{};
    TERMX = 8;
    TERMY = 4;
    game_client::memory::resize(TERMX, TERMY);
    catacurses::stdscr = catacurses::newwin(TERMY, TERMX, point_zero);
    catacurses::newscr = catacurses::stdscr;

    catacurses::mvwprintw(catacurses::stdscr, point_zero, "base");
    catacurses::wrefresh(catacurses::stdscr);
    const auto overlay = catacurses::newwin(1, 3, point(1, 1));
    catacurses::mvwprintw(overlay, point_zero, "界");
    catacurses::wrefresh(overlay);

    const auto screen = game_client::memory::snapshot();
    CHECK(screen.width == 8);
    CHECK(screen.height == 4);
    CHECK(screen.cells[9].text == "界");
    CHECK(screen.cells[10].text.empty());
    CHECK(screen.cells[0].text == "b");
}

TEST_CASE("cursesport ignores writes to invalid windows", "[client][mcp]") {
    const auto invalid = catacurses::window{};
    CHECK_NOTHROW(catacurses::waddch(invalid, 'x'));

    const auto valid = catacurses::newwin(1, 1, point_zero);
    CHECK_NOTHROW(catacurses::waddch(valid, 'x'));
    const auto* const native = valid.get<cata_cursesport::WINDOW>();
    REQUIRE(native != nullptr);
    CHECK(native->line[0].chars[0].ch == "x");
}

TEST_CASE("generic memory client reports cursor and resize bounds", "[client][mcp]") {
    auto guard = memory_screen_guard{};
    TERMX = 6;
    TERMY = 3;
    game_client::memory::resize(TERMX, TERMY);
    catacurses::stdscr = catacurses::newwin(TERMY, TERMX, point_zero);
    catacurses::newscr = catacurses::stdscr;
    catacurses::wmove(catacurses::stdscr, point(3, 2));
    catacurses::curs_set(1);

    const auto screen = game_client::memory::snapshot();
    CHECK(screen.cursor.x == 3);
    CHECK(screen.cursor.y == 2);
    CHECK(screen.cursor_visible);

    game_client::memory::resize(3, 2);
    const auto resized = game_client::memory::snapshot();
    CHECK(resized.width == 3);
    CHECK(resized.height == 2);
    CHECK(resized.cursor.x < 3);
    CHECK(resized.cursor.y < 2);
}

TEST_CASE("generic memory client keeps source and virtual cursors separate", "[client][mcp]") {
    auto guard = memory_screen_guard{};
    TERMX = 8;
    TERMY = 4;
    game_client::memory::resize(TERMX, TERMY);
    catacurses::stdscr = catacurses::newwin(TERMY, TERMX, point_zero);
    catacurses::newscr = catacurses::newwin(TERMY, TERMX, point_zero);
    const auto overlay = catacurses::newwin(2, 3, point(2, 1));
    catacurses::wmove(catacurses::stdscr, point_zero);
    catacurses::wmove(overlay, point(1, 0));
    catacurses::wredrawln(overlay, 0, 1);
    const auto* const dirty_overlay = overlay.get<cata_cursesport::WINDOW>();
    REQUIRE(dirty_overlay != nullptr);
    CHECK(dirty_overlay->draw);
    CHECK(dirty_overlay->line[0].touched);
    catacurses::wnoutrefresh(overlay);

    const auto* const source = catacurses::stdscr.get<cata_cursesport::WINDOW>();
    const auto* const virtual_screen = catacurses::newscr.get<cata_cursesport::WINDOW>();
    REQUIRE(source != nullptr);
    REQUIRE(virtual_screen != nullptr);
    CHECK(source->cursor == point_zero);
    CHECK(virtual_screen->cursor == point(3, 1));
    CHECK(dirty_overlay->draw == false);

    game_client::memory::set_input_provider([](const int /*timeout*/) {
        return input_event('x', input_event_t::keyboard);
    });
    const auto event = inp_mngr.get_input_event();
    CHECK(event.get_first_input() == 'x');
    CHECK(virtual_screen->cursor == point(3, 1));
}

TEST_CASE("generic memory client projects offset coordinate input", "[client][mcp][input]") {
    auto guard = memory_screen_guard{};
    REQUIRE(g != nullptr);
    TERMX = 8;
    TERMY = 4;
    game_client::memory::resize(TERMX, TERMY);
    const auto restore_terrain = restore_on_out_of_scope<catacurses::window>(g->w_terrain);
    const auto restore_view = restore_on_out_of_scope<tripoint_bub_ms>(g->ter_view_p);
    const auto capture = catacurses::newwin(2, 4, point(2, 1));
    g->w_terrain = capture;
    g->ter_view_p = tripoint_bub_ms(10, 20, 0);

    auto context = input_context("MCP_COORDINATE_TEST");
    context.register_action("ANY_INPUT");
    context.register_action("COORDINATE");
    game_client::memory::set_input_provider([](const int /*timeout*/) {
        auto event = input_event(MOUSE_BUTTON_LEFT, input_event_t::mouse);
        event.mouse_pos = point(4, 2);
        return event;
    });
    CHECK(context.handle_input() == "ANY_INPUT");
    const auto projected = context.get_coordinates(capture);
    REQUIRE(projected.has_value());
    CHECK(projected->x() == 10);
    CHECK(projected->y() == 20);
    CHECK(g->w_terrain == capture);
}

TEST_CASE(
    "a travel click selects the clicked square through the real input context",
    "[client][mcp][input][engine_client_contract]") {
    auto guard = memory_screen_guard{};
    REQUIRE(g != nullptr);
    TERMX = 8;
    TERMY = 4;
    game_client::memory::resize(TERMX, TERMY);
    const auto restore_terrain = restore_on_out_of_scope<catacurses::window>(g->w_terrain);
    const auto restore_view = restore_on_out_of_scope<tripoint_bub_ms>(g->ter_view_p);
    const auto capture = catacurses::newwin(2, 4, point(2, 1));
    g->w_terrain = capture;
    g->ter_view_p = tripoint_bub_ms(10, 20, 0);
    const auto frame = engine_client::current_bubble_frame();
    const auto target = [&](const int x, const int y, const int z = 0) {
        return engine_client::travel_operation{
            .target = {.dim = frame.dim, .x = frame.x + x, .y = frame.y + y, .z = z}};
    };

    SECTION("the click projects back to the same bubble square") {
        auto context = input_context("MCP_TRAVEL_TEST");
        context.register_action("ANY_INPUT");
        context.register_action("COORDINATE");
        for (const auto& square : {point(8, 19), point(9, 20), point(10, 19), point(11, 20)}) {
            const auto click = engine_client::travel_click(target(square.x, square.y));
            REQUIRE(click);
            const auto resolved = game_client::resolve_input_command(*click, point(8, 4));
            REQUIRE(resolved);
            game_client::memory::set_input_provider([&](const int /*timeout*/) {
                return *resolved;
            });
            CHECK(context.handle_input() == "ANY_INPUT");
            const auto projected = context.get_coordinates(capture);
            REQUIRE(projected);
            CHECK(projected->xy() == tripoint_bub_ms(square.x, square.y, 0).xy());
        }
    }
    SECTION("a square outside the terrain window has no click") {
        CHECK_FALSE(engine_client::travel_click(target(7, 20)));
        CHECK_FALSE(engine_client::travel_click(target(12, 20)));
        CHECK_FALSE(engine_client::travel_click(target(10, 18)));
        CHECK_FALSE(engine_client::travel_click(target(10, 21)));
    }
    SECTION("another level or dimension has no click") {
        CHECK_FALSE(engine_client::travel_click(target(10, 20, 1)));
        auto elsewhere = target(10, 20);
        elsewhere.target.dim = frame.dim + "_elsewhere";
        CHECK_FALSE(engine_client::travel_click(elsewhere));
    }
}

TEST_CASE("memory events pass through the real input context", "[client][mcp][input]") {
    const auto guard = memory_screen_guard{};
    TERMX = 8;
    TERMY = 4;
    game_client::memory::resize(TERMX, TERMY);
    catacurses::stdscr = catacurses::newwin(TERMY, TERMX, point_zero);
    catacurses::newscr = catacurses::stdscr;
    auto context = input_context("MCP_TEST");
    context.register_action("ANY_INPUT");
    const auto original = game_client::active_input_context();
    auto observed = game_client::input_context_view{};
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        observed = game_client::active_input_context();
        return input_event('x', input_event_t::keyboard);
    });

    CHECK(context.handle_input() == "ANY_INPUT");
    CHECK(context.get_raw_input().get_first_input() == 'x');
    CHECK(observed.context == &context);
    CHECK(observed.category == "MCP_TEST");
    CHECK(game_client::active_input_context().context == original.context);
}

#endif // CATA_MCP
