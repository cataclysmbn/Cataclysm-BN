#if defined(CATA_MCP)

#    include "action.h"
#    include "cached_options.h"
#    include "cata_utility.h"
#    include "catch/catch.hpp"
#    include "client_command.h"
#    include "client_input.h"
#    include "client_interaction.h"
#    include "client_memory.h"
#    include "client_memory_scope.h"
#    include "client_screen_support.h"
#    include "color.h"
#    include "cursesdef.h"
#    include "cursesport.h"
#    include "engine_client_component.h"
#    include "engine_client_contract.h"
#    include "fstream_utils.h"
#    include "help.h"
#    include "input.h"
#    include "json.h"
#    include "morale.h"
#    include "morale_types.h"
#    include "output.h"
#    include "path_display.h"
#    include "path_info.h"
#    include "rng.h"
#    include "string_formatter.h"
#    include "string_utils.h"
#    include "translations.h"
#    include "ui_manager.h"

#    include <algorithm>
#    include <atomic>
#    include <expected>
#    include <filesystem>
#    include <nlohmann/json.hpp>
#    include <ranges>
#    include <sstream>
#    include <stdexcept>
#    include <string>
#    include <variant>
#    include <vector>

namespace {

struct text_widget_guard {
    const game_client::memory::scoped_state memory;
    restore_on_out_of_scope<bool> restore_test_mode{test_mode};
    restore_on_out_of_scope<int> restore_termx{TERMX};
    restore_on_out_of_scope<int> restore_termy{TERMY};
    restore_on_out_of_scope<int> restore_width{FULL_SCREEN_WIDTH};
    restore_on_out_of_scope<int> restore_height{FULL_SCREEN_HEIGHT};
    int old_timeout = inp_mngr.get_timeout();

    text_widget_guard() {
        test_mode = false;
        TERMX = 80;
        TERMY = 24;
        FULL_SCREEN_WIDTH = 80;
        FULL_SCREEN_HEIGHT = 24;
        game_client::memory::resize(TERMX, TERMY);
        catacurses::stdscr = catacurses::newwin(TERMY, TERMX, point_zero);
        catacurses::newscr = catacurses::newwin(TERMY, TERMX, point_zero);
    }

    ~text_widget_guard() { inp_mngr.set_timeout(old_timeout); }
};

auto expected_help_message(const std::string& message) -> std::string {
    if (message == "<GAME_DIRECTORIES>") { return resolved_game_paths(); }
    if (message == "<DRAW_NOTE_COLORS>") {
        auto text = std::string(_("Note colors: "));
        for (const auto& [index, name] : get_note_color_names()) {
            text += string_format("%s:%s, ", colorize(index, get_note_color(index)), _(name));
        }
        return text;
    }
    if (message == "<HELP_DRAW_DIRECTIONS>") {
        auto grid = std::string{
            "<LEFTUP_0>  <UP_0>  <RIGHTUP_0>   <LEFTUP_1>  <UP_1>  <RIGHTUP_1>\n"
            " \\ | /     \\ | /\n"
            "  \\|/       \\|/\n"
            "<LEFT_0>--<pause_0>--<RIGHT_0>   <LEFT_1>--<pause_1>--<RIGHT_1>\n"
            "  /|\\       /|\\\n"
            " / | \\     / | \\\n"
            "<LEFTDOWN_0>  <DOWN_0>  <RIGHTDOWN_0>   <LEFTDOWN_1>  <DOWN_1>  <RIGHTDOWN_1>"};
        for (const auto action :
             {ACTION_MOVE_FORTH_LEFT, ACTION_MOVE_FORTH, ACTION_MOVE_FORTH_RIGHT, ACTION_MOVE_LEFT,
              ACTION_PAUSE, ACTION_MOVE_RIGHT, ACTION_MOVE_BACK_LEFT, ACTION_MOVE_BACK,
              ACTION_MOVE_BACK_RIGHT}) {
            const auto keys = keys_bound_to(action);
            for (const auto index : std::views::iota(std::size_t{0}, std::size_t{2})) {
                const auto token = "<" + action_ident(action) + "_" + std::to_string(index) + ">";
                const auto binding =
                    index < keys.size()
                        ? string_format("<color_light_blue>%s</color>", keys[index])
                        : std::string{"<color_red>?</color>"};
                grid = replace_all(grid, token, binding);
            }
        }
        return grid;
    }
    return _(message);
}

namespace ec = engine_client;
using json = nlohmann::json;
using client_screen_test::invoke_event;
using client_screen_test::top_json;
using client_screen_test::top_screen_now;

auto resolve_action(const std::string& action) -> input_event {
    const auto result =
        game_client::resolve_input_command({.action = action}, game_client::memory::screen_size());
    REQUIRE(result.has_value());
    return *result;
}

auto live_state() -> ec::boundary_state {
    const auto state = ec::capture_boundary(
        {.epoch = "epoch:test",
         .ready =
             {.phase = "waiting_for_input",
              .game_ready = true,
              .accepts_interaction_commands = true,
              .accepts_registered_actions = true}});
    REQUIRE(state.has_value());
    return *state;
}

auto find_affordance(const ec::screen_view& top, const std::string& id) -> const ec::affordance* {
    const auto found = std::ranges::find(top.affordances, id, &ec::affordance::id);
    return found == top.affordances.end() ? nullptr : &*found;
}

/// The text screen on top: its one text component, and the registered ways out.
auto check_reader(const ec::screen_view& top) -> void {
    CHECK(top.context == "SCROLLABLE_TEXT");
    REQUIRE(top.root);
    CHECK(top.root->id == "text:body");
    CHECK(top.root->children.empty());
    CHECK(std::holds_alternative<ec::text_body>(top.root->body));
    const auto* const quit = find_affordance(top, "QUIT");
    REQUIRE(quit != nullptr);
    CHECK(quit->role == "cancel");
    CHECK(std::ranges::none_of(top.affordances, [](const ec::affordance& entry) {
        return entry.args.row || entry.args.count || entry.args.text;
    }));
}

auto reader_text(const ec::screen_view& top) -> const std::string& {
    REQUIRE(top.root);
    REQUIRE(std::holds_alternative<ec::text_body>(top.root->body));
    return std::get<ec::text_body>(top.root->body).markup;
}

auto reader_window() -> catacurses::window {
    return catacurses::newwin(FULL_SCREEN_HEIGHT, FULL_SCREEN_WIDTH, point_zero);
}

auto native_text_row(
    const game_client::screen_snapshot& screen, const int row, const int window_width = -1)
    -> std::string {
    REQUIRE(row >= 1);
    REQUIRE(row < screen.height - 1);
    const auto width = window_width < 0 ? screen.width : window_width;
    REQUIRE(width > 2);
    REQUIRE(width <= screen.width);
    auto result = std::string{};
    const auto offset = static_cast<std::size_t>(row * screen.width + 1);
    for (const auto& cell : screen.cells | std::views::drop(offset) | std::views::take(width - 2)) {
        result += cell.text;
    }
    return result;
}

} // namespace

TEST_CASE("text_widget_guard leaves a borrowed compositor intact", "[client][mcp]") {
    const auto outer = game_client::memory::scoped_state{};
    game_client::memory::resize(7, 3);
    catacurses::stdscr = catacurses::newwin(3, 7, point_zero);
    catacurses::mvwprintw(catacurses::stdscr, point_zero, "x");
    catacurses::wrefresh(catacurses::stdscr);
    const auto borrowed = catacurses::stdscr;
    {
        const auto guard = text_widget_guard{};
        CHECK(catacurses::stdscr.get<cata_cursesport::WINDOW>() != nullptr);
    }
    CHECK(game_client::memory::screen_size() == point(7, 3));
    CHECK(game_client::memory::snapshot().cells[0].text == "x");
    CHECK(catacurses::stdscr.get<cata_cursesport::WINDOW>()
          == borrowed.get<cata_cursesport::WINDOW>());
    CHECK(catacurses::stdscr.get<cata_cursesport::WINDOW>()->width == 7);
}

TEST_CASE(
    "read-only text exposes full content and native paging reaches the final lines",
    "[client][interaction][text][mcp]") {
    const auto guard = text_widget_guard{};
    auto text = std::string{};
    for (const auto index : std::views::iota(0, 47)) {
        text += "Visible line " + std::to_string(index) + "\n";
    }
    text += "<color_light_blue>Final text https://example.org/help</color>";
    const auto title = std::string("Read-only document");
    auto shape = json{};
    auto reads = 0;
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        REQUIRE(++reads <= 8);
        const auto top = top_screen_now();
        check_reader(top);
        CHECK(top.title == title);
        CHECK(reader_text(top) == text);
        if (shape.is_null()) { shape = top_json(); }
        CHECK(top_json() == shape);
        const auto invented =
            ec::resolve_invoke(live_state(), {.affordance_id = "invented-choice"});
        REQUIRE_FALSE(invented);
        CHECK(invented.error() == ec::error::unknown_affordance);
        const auto native_screen = game_client::memory::snapshot();
        const auto& screen = native_screen.text;
        if (reads == 1) {
            CHECK(screen.find("Visible line 0") != std::string::npos);
            // Actual raw human scroll binding, not a semantic choice or a screen macro.
            const auto active = game_client::active_input_context();
            REQUIRE(active.context);
            const auto& bindings =
                inp_mngr.get_input_for_action("PAGE_DOWN", active.context->category_name());
            REQUIRE_FALSE(bindings.empty());
            return bindings.front();
        }
        if (reads == 2) { return resolve_action("PAGE_DOWN"); }
        if (reads == 3 || reads == 4) {
            CHECK(native_text_row(native_screen, 1).starts_with("Visible line 44 "));
            CHECK(screen.find("Final text https://example.org/help") != std::string::npos);
            if (reads == 3) { return resolve_action("PAGE_DOWN"); }
            return resolve_action("PAGE_UP");
        }
        if (reads == 5) {
            CHECK(native_text_row(native_screen, 1).starts_with("Visible line 22 "));
            return resolve_action("UP");
        }
        if (reads == 6) {
            CHECK(native_text_row(native_screen, 1).starts_with("Visible line 21 "));
            CHECK(native_text_row(native_screen, 2).starts_with("Visible line 22 "));
            return resolve_action("DOWN");
        }
        if (reads == 7) {
            CHECK(native_text_row(native_screen, 1).starts_with("Visible line 22 "));
            CHECK(native_text_row(native_screen, 2).starts_with("Visible line 23 "));
            return resolve_action("PAGE_UP");
        }
        CHECK(screen.find("Visible line 0") != std::string::npos);
        return invoke_event(top, "QUIT");
    });
    scrollable_text(reader_window, title, text);
    CHECK(reads == 8);
    CHECK_FALSE(game_client::current_interaction().structured);
}

TEST_CASE(
    "scrolled text reader resize reflows the native viewport without schema drift",
    "[client][interaction][text][mcp]") {
    const auto guard = text_widget_guard{};
    // Keep the composed surface fixed while the real reader window changes size.
    TERMX = 140;
    TERMY = 64;
    game_client::memory::resize(TERMX, TERMY);
    const auto background = background_pane{};
    auto viewport = point{80, 24};
    auto text = std::string{};
    for (const auto index : std::views::iota(0, 30)) {
        text += "Paragraph " + std::to_string(index) + ": " + std::string(64, 'x') + " "
              + std::string(40, 'y') + "\n";
    }
    text += "End of resizable document";
    const auto title = std::string{"Resizable reader"};
    const auto initial_lines = foldstring(text, viewport.x - 2);
    const auto initial_page_height = viewport.y - 2;
    auto shape = json{};
    auto window_creations = 0;
    auto reads = 0;
    const auto check_viewport = [&](const int first_line) {
        const auto screen = game_client::memory::snapshot();
        CHECK(screen.width == TERMX);
        CHECK(screen.height == TERMY);
        CAPTURE(reads, viewport, first_line);
        const auto lines = foldstring(text, viewport.x - 2);
        for (const auto row : std::views::iota(1, viewport.y - 1)) {
            const auto line = first_line + row - 1;
            auto expected = line < static_cast<int>(lines.size()) ? lines[line] : std::string{};
            expected.resize(viewport.x - 2, ' ');
            CHECK(native_text_row(screen, row, viewport.x) == expected);
        }
    };
    const auto resize_viewport = [&](const point size) {
        REQUIRE(size.x >= FULL_SCREEN_WIDTH);
        REQUIRE(size.y >= FULL_SCREEN_HEIGHT);
        viewport = size;
        // Exercise the active widget's installed resize callback and redraw, not
        // a reconstructed view or direct access to its private scroll state.
        ui_manager::screen_resized();
    };
    const auto check_unchanged = [&](const ec::screen_view& before) {
        const auto resized = top_screen_now();
        check_reader(resized);
        CHECK(resized.screen_id == before.screen_id);
        CHECK(top_json() == shape);
        CHECK(resized.title == title);
        CHECK(reader_text(resized) == text);
    };
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        REQUIRE(++reads <= 4);
        const auto top = top_screen_now();
        check_reader(top);
        CHECK(top.title == title);
        CHECK(reader_text(top) == text);
        if (shape.is_null()) { shape = top_json(); }
        CHECK(top_json() == shape);
        if (reads == 1) {
            CHECK(window_creations == 1);
            check_viewport(0);
            return resolve_action("PAGE_DOWN");
        }
        if (reads == 2) {
            check_viewport(initial_page_height);
            return resolve_action("PAGE_UP");
        }
        if (reads == 3) {
            check_viewport(0);
            resize_viewport({140, 32});
            CHECK(window_creations == 2);
            const auto wide_lines = foldstring(text, viewport.x - 2);
            REQUIRE(wide_lines.size() < initial_lines.size());
            REQUIRE(static_cast<int>(wide_lines.size()) - (viewport.y - 2) == 1);
            check_viewport(0);
            check_unchanged(top);
            return resolve_action("DOWN");
        }
        check_viewport(1);
        resize_viewport({80, 32});
        CHECK(window_creations == 3);
        CHECK(foldstring(text, viewport.x - 2) == initial_lines);
        check_viewport(1);
        check_unchanged(top);
        return invoke_event(top, "QUIT");
    });
    scrollable_text(
        [&]() {
            ++window_creations;
            return catacurses::newwin(viewport.y, viewport.x, point_zero);
        },
        title, text);
    CHECK(reads == 4);
    CHECK(window_creations == 3);
    CHECK_FALSE(game_client::active_input_context().context);
    CHECK_FALSE(game_client::current_interaction().structured);
}

#endif
