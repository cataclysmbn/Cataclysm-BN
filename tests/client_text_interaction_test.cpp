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
#    include "engine_client_requirements.h"
#    include "fstream_utils.h"
#    include "help.h"
#    include "input.h"
#    include "json.h"
#    include "morale.h"
#    include "morale_types.h"
#    include "output.h"
#    include "path_display.h"
#    include "path_info.h"
#    include "replay/replay.h"
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

struct loaded_topic {
    int order = 0;
    std::string name;
    std::vector<std::string> messages;
};

auto read_topics() -> std::vector<loaded_topic> {
    auto result = std::vector<loaded_topic>{};
    REQUIRE(read_from_file_json(PATH_INFO::help(), [&](JsonIn& json) {
        json.start_array();
        while (!json.end_array()) {
            auto object = json.get_object();
            object.get_string("type");
            auto topic = loaded_topic{
                .order = object.get_int("order"),
                .name = object.get_string("name"),
            };
            object.read("messages", topic.messages);
            result.push_back(std::move(topic));
        }
    }));
    std::ranges::sort(result, {}, &loaded_topic::order);
    return result;
}

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

auto expected_topic_text(const loaded_topic& topic) -> std::string {
    auto text = std::string{};
    for (const auto& message : topic.messages) {
        auto paragraph = expected_help_message(message);
        auto start = paragraph.find("<press_");
        while (start != std::string::npos) {
            const auto end = paragraph.find('>', start);
            REQUIRE(end != std::string::npos);
            const auto token = paragraph.substr(start, end - start + 1);
            const auto action = paragraph.substr(start + 7, end - start - 7);
            const auto replacement =
                "<color_light_blue>" + press_x(look_up_action(action), "", "") + "</color>";
            paragraph.replace(start, token.size(), replacement);
            start = paragraph.find("<press_", start + replacement.size());
        }
        if (!text.empty()) { text += "\n\n"; }
        text += paragraph;
    }
    return text;
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

/// What the lifecycle makes of an invoke of `affordance` on the screen on top that expects
/// `expect`.
auto resolution(
    const std::string& affordance, const ec::expectation& expect, ec::invoke_args args = {})
    -> std::expected<input_event, ec::error> {
    class authority final: public ec::command_authority {
    public:
        auto current_permissions() const -> ec::command_permissions override {
            return {.accepts_interaction_commands = true, .accepts_registered_actions = true};
        }
    };
    const auto state = live_state();
    const auto owner = authority{};
    auto lifecycle = ec::command_lifecycle{"epoch:test", owner};
    const auto submitted = lifecycle.submit(
        {.epoch = "epoch:test",
         .expect = expect,
         .operation = ec::invoke_operation{.affordance_id = affordance, .args = std::move(args)}});
    if (!submitted) { return std::unexpected(submitted.error()); }
    return lifecycle
        .validate({.epoch = "epoch:test", .sequence = 1, .revision = 1}, state, point{80, 24});
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
        return entry.args.row || entry.args.rows || entry.args.count || entry.args.text;
    }));
}

auto reader_text(const ec::screen_view& top) -> const std::string& {
    REQUIRE(top.root);
    REQUIRE(std::holds_alternative<ec::text_body>(top.root->body));
    return std::get<ec::text_body>(top.root->body).markup;
}

auto topic_rows(const ec::screen_view& top) -> const std::vector<ec::row>& {
    REQUIRE(top.root);
    const auto* const list = ec::find_component(*top.root, "help:topics");
    REQUIRE(list != nullptr);
    REQUIRE(std::holds_alternative<ec::list_body>(list->body));
    return std::get<ec::list_body>(list->body).rows;
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
    "loaded help topics enter the native reader with complete localized content",
    "[client][interaction][help][text][mcp]") {
    const auto guard = text_widget_guard{};
    const auto topics = read_topics();
    REQUIRE(topics.size() > 1);
    auto widget = help{};
    widget.load();
    auto topic_index = std::size_t{0};
    auto in_reader = false;
    auto menu_shape = json{};
    auto menu_screen = std::string{};
    auto reads = 0;

    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        REQUIRE(++reads <= static_cast<int>(2 * topics.size() + 1));
        const auto top = top_screen_now();
        if (in_reader) {
            check_reader(top);
            CHECK(top.title == _(" HELP "));
            CHECK(top.screen_id != menu_screen);
            const auto& topic = topics[topic_index];
            CHECK(reader_text(top) == expected_topic_text(topic));
            CHECK(reader_text(top).find("<press_") == std::string::npos);
            CHECK(reader_text(top).find("<HELP_DRAW_DIRECTIONS>") == std::string::npos);
            CHECK(reader_text(top).find("<DRAW_NOTE_COLORS>") == std::string::npos);
            ++topic_index;
            in_reader = false;
            return invoke_event(top, "QUIT");
        }
        CHECK(top.title == _(" HELP "));
        const auto& rows = topic_rows(top);
        REQUIRE(rows.size() == topics.size());
        if (menu_screen.empty()) { menu_screen = top.screen_id; }
        CHECK(top.screen_id == menu_screen);
        if (menu_shape.is_null()) { menu_shape = top_json(); }
        CHECK(top_json() == menu_shape);
        for (auto index = std::size_t{0}; index < topics.size(); ++index) {
            CHECK(rows[index].id == "topic:" + std::to_string(topics[index].order));
            CHECK(rows[index].label
                  == remove_color_tags(shortcut_text(c_light_blue, _(topics[index].name))));
        }
        const auto state = live_state();
        const auto page = ec::read_rows(
            {.epoch = "epoch:test",
             .boundary_id = state.id,
             .component_id = "help:topics",
             .offset = 1,
             .limit = 1},
            state);
        REQUIRE(page.has_value());
        REQUIRE(page->rows.size() == 1);
        CHECK(page->total == topics.size());
        CHECK(page->rows.front().id == rows[1].id);
        if (topic_index == topics.size()) { return invoke_event(top, "QUIT"); }
        in_reader = true;
        return invoke_event(top, "help:topic", {.row = rows[topic_index].id});
    });
    widget.display_help();
    CHECK(topic_index == topics.size());
    CHECK(reads == static_cast<int>(2 * topics.size() + 1));
    CHECK_FALSE(game_client::current_interaction().structured);
}

TEST_CASE(
    "help invokes are rejected when the screen is stale, the topic is unknown or the expectation has the wrong form",
    "[client][interaction][help][text][mcp]") {
    const auto guard = text_widget_guard{};
    auto widget = help{};
    widget.load();
    auto reads = 0;
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        REQUIRE(++reads == 1);
        const auto top = top_screen_now();
        const auto state = live_state();
        const auto first = topic_rows(top).front().id;
        const auto boundary = ec::expectation{.revision = 1, .boundary_id = state.id};
        CHECK(resolution("help:topic", boundary, {.row = first}).has_value());
        const auto unknown = resolution("help:topic", boundary, {.row = "unknown-help-topic"});
        REQUIRE_FALSE(unknown);
        CHECK(unknown.error() == ec::error::invalid_arguments);
        // A topic is picked by a row, so its invoke expects the boundary, not the screen.
        const auto wrong_form =
            resolution("help:topic", {.screen_id = top.screen_id}, {.row = first});
        REQUIRE_FALSE(wrong_form);
        CHECK(wrong_form.error() == ec::error::invalid_arguments);
        const auto stale_boundary = resolution(
            "help:topic", {.revision = 1, .boundary_id = "boundary:old"}, {.row = first});
        REQUIRE_FALSE(stale_boundary);
        CHECK(stale_boundary.error() == ec::error::stale_boundary);
        // A way out expects the screen it is shown on.
        const auto stale = resolution("QUIT", {.screen_id = "screen:old"});
        REQUIRE_FALSE(stale);
        CHECK(stale.error() == ec::error::stale_screen);
        const auto missing = resolution("help:nothing", boundary);
        REQUIRE_FALSE(missing);
        CHECK(missing.error() == ec::error::unknown_affordance);
        CHECK(resolution("QUIT", {.screen_id = top.screen_id}).has_value());
        CHECK(top_screen_now().screen_id == top.screen_id);
        CHECK(topic_rows(top_screen_now()).front().id == first);
        return invoke_event(top, "QUIT");
    });
    widget.display_help();
    CHECK(reads == 1);
}

TEST_CASE(
    "raw loaded help hotkeys and invoked topics share the same native content",
    "[client][interaction][help][text][mcp]") {
    const auto guard = text_widget_guard{};
    const auto topics = read_topics();
    for (auto index = std::size_t{0}; index < topics.size(); ++index) {
        CAPTURE(index, topics[index].name);
        auto invoked_text = std::string{};
        auto invoked_title = std::string{};
        for (const auto raw : {false, true}) {
            auto widget = help{};
            widget.load();
            auto reads = 0;
            game_client::memory::set_input_provider([&](const int /*timeout*/) {
                const auto top = top_screen_now();
                REQUIRE(++reads <= 3);
                if (reads == 1) {
                    const auto& rows = topic_rows(top);
                    REQUIRE(index < rows.size());
                    if (raw) {
                        const auto keys = get_hotkeys(topics[index].name);
                        REQUIRE_FALSE(keys.empty());
                        CHECK(rows[index].hotkey == keys.front());
                        const auto result = game_client::resolve_input_command(
                            {.key = keys.front()}, game_client::memory::screen_size());
                        REQUIRE(result);
                        return *result;
                    }
                    return invoke_event(top, "help:topic", {.row = rows[index].id});
                }
                if (reads == 2) {
                    check_reader(top);
                    if (raw) {
                        CHECK(reader_text(top) == invoked_text);
                        CHECK(top.title == invoked_title);
                    } else {
                        invoked_text = reader_text(top);
                        invoked_title = top.title;
                    }
                    return resolve_action("CONFIRM");
                }
                CHECK(topic_rows(top).size() == topics.size());
                return resolve_action("QUIT");
            });
            widget.display_help();
            CHECK(reads == 3);
        }
    }
}

TEST_CASE(
    "read-only text exposes full content and native paging retains the final lines",
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
            CHECK(screen.find("Visible line 26") != std::string::npos);
            CHECK(screen.find("Final text https://example.org/help") != std::string::npos);
            if (reads == 3) { return resolve_action("PAGE_DOWN"); }
            return resolve_action("PAGE_UP");
        }
        if (reads == 5) {
            CHECK(native_text_row(native_screen, 1).starts_with("Visible line 4 "));
            return resolve_action("UP");
        }
        if (reads == 6) {
            CHECK(native_text_row(native_screen, 1).starts_with("Visible line 3 "));
            CHECK(native_text_row(native_screen, 2).starts_with("Visible line 4 "));
            return resolve_action("DOWN");
        }
        if (reads == 7) {
            CHECK(native_text_row(native_screen, 1).starts_with("Visible line 4 "));
            CHECK(native_text_row(native_screen, 2).starts_with("Visible line 5 "));
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
    "scrolled text reader resize reflows and clamps the native viewport without schema drift",
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
    const auto initial_last_offset = static_cast<int>(initial_lines.size()) - initial_page_height;
    REQUIRE(initial_last_offset > initial_page_height);
    REQUIRE(initial_last_offset < 2 * initial_page_height);
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
            return resolve_action("PAGE_DOWN");
        }
        if (reads == 3) {
            check_viewport(initial_last_offset);
            resize_viewport({140, 32});
            CHECK(window_creations == 2);
            const auto wide_lines = foldstring(text, viewport.x - 2);
            REQUIRE(wide_lines.size() < initial_lines.size());
            REQUIRE(static_cast<int>(wide_lines.size()) - (viewport.y - 2) == 1);
            check_viewport(1);
            check_unchanged(top);
            return resolve_action("DOWN");
        }
        check_viewport(1);
        resize_viewport({80, 32});
        CHECK(window_creations == 3);
        CHECK(foldstring(text, viewport.x - 2) == initial_lines);
        check_viewport(1);
        check_unchanged(top);
        resize_viewport({80, 64});
        CHECK(window_creations == 4);
        REQUIRE(static_cast<int>(initial_lines.size()) < viewport.y - 2);
        check_viewport(0);
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
    CHECK(window_creations == 4);
    CHECK_FALSE(game_client::active_input_context().context);
    CHECK_FALSE(game_client::current_interaction().structured);
}

TEST_CASE(
    "help reader nested keybindings fallback restores the reader and topic menu",
    "[client][interaction][help][text][mcp]") {
    const auto guard = text_widget_guard{};
    const auto topics = read_topics();
    REQUIRE(topics.size() > 1);
    const auto expected_text = expected_topic_text(topics[1]);
    auto menu_screen = std::string{};
    auto topic_id = std::string{};
    auto reader_screen = std::string{};
    auto previous_input = std::uint64_t{0};
    auto reads = 0;
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        REQUIRE(++reads <= 8);
        if (reads == 1) {
            const auto top = top_screen_now();
            REQUIRE(topic_rows(top).size() > 1);
            menu_screen = top.screen_id;
            topic_id = topic_rows(top)[1].id;
            return invoke_event(top, "help:topic", {.row = topic_id});
        }
        if (reads == 2) {
            const auto top = top_screen_now();
            check_reader(top);
            CHECK(reader_text(top) == expected_text);
            reader_screen = top.screen_id;
            return resolve_action("HELP_KEYBINDINGS");
        }
        if (reads == 7) {
            const auto top = top_screen_now();
            check_reader(top);
            CHECK(top.screen_id == reader_screen);
            CHECK(reader_text(top) == expected_text);
            return invoke_event(top, "QUIT");
        }
        if (reads == 8) {
            const auto top = top_screen_now();
            CHECK(top.screen_id == menu_screen);
            REQUIRE(topic_rows(top).size() > 1);
            CHECK(topic_rows(top)[1].id == topic_id);
            return invoke_event(top, "QUIT");
        }
        // The keybindings screen is a screen of the generic path.
        const auto snapshot = game_client::current_interaction();
        CHECK(snapshot.input_id > previous_input);
        previous_input = snapshot.input_id;
        if (reads == 3) {
            CHECK(snapshot.context == "HELP_KEYBINDINGS");
            CHECK(snapshot.kind == game_client::interaction_kind::field);
            CHECK(snapshot.structured);
            CHECK(snapshot.message != expected_text);
            // The first row is the cursor an engine client acts on; Down moves it.
            CHECK(snapshot.focus_choice_id == "mode:add_local");
            return resolve_action("DOWN");
        }
        if (reads == 4) {
            CHECK(snapshot.kind == game_client::interaction_kind::field);
            CHECK(snapshot.focus_choice_id == "mode:add_global");
            return resolve_action("ADD_LOCAL");
        }
        if (reads == 5) {
            CHECK(snapshot.context == "HELP_KEYBINDINGS");
            CHECK(snapshot.structured);
            CHECK(snapshot.kind == game_client::interaction_kind::choices);
            CHECK(snapshot.title == "Keybindings");
            CHECK(std::ranges::any_of(snapshot.choices, [](const auto& choice) {
                return choice.id == "mode:add_local" && choice.selected;
            }));
            CHECK_FALSE(snapshot.field);
            CHECK(snapshot.allow_cancel);
            return resolve_action("QUIT");
        }
        CHECK(snapshot.context == "HELP_KEYBINDINGS");
        CHECK(snapshot.kind == game_client::interaction_kind::field);
        return resolve_action("QUIT");
    });
    auto widget = help{};
    widget.load();
    widget.display_help();
    CHECK(reads == 8);
    CHECK_FALSE(game_client::current_interaction().structured);
}

TEST_CASE(
    "failed nested help reader releases widget providers and input contexts",
    "[client][interaction][help][text][mcp]") {
    const auto guard = text_widget_guard{};
    auto reads = 0;
    game_client::memory::set_input_provider([&](const int /*timeout*/) -> input_event {
        REQUIRE(++reads <= 2);
        const auto top = top_screen_now();
        if (reads == 1) {
            REQUIRE_FALSE(topic_rows(top).empty());
            return invoke_event(top, "help:topic", {.row = topic_rows(top).front().id});
        }
        check_reader(top);
        throw std::runtime_error("test reader input failure");
    });
    auto widget = help{};
    widget.load();
    CHECK_THROWS_WITH(widget.display_help(), "test reader input failure");
    CHECK(reads == 2);
    CHECK_FALSE(game_client::active_input_context().context);
    CHECK_FALSE(game_client::current_interaction().structured);

    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto top = top_screen_now();
        CHECK(top.title == _(" HELP "));
        CHECK_FALSE(topic_rows(top).empty());
        return invoke_event(top, "QUIT");
    });
    widget.display_help();
    CHECK_FALSE(game_client::current_interaction().structured);
}

TEST_CASE(
    "an invoked help topic and a closed reader replay without live input",
    "[client][interaction][help][text][replay][mcp]") {
    const auto guard = text_widget_guard{};
    static auto counter = std::atomic_uint64_t{0};
    const auto directory =
        std::filesystem::path(PATH_INFO::user_dir())
        / ("help-text-replay-" + std::to_string(counter.fetch_add(1)));
    REQUIRE(std::filesystem::create_directory(directory));
    const auto cleanup = on_out_of_scope([&] {
        replay::stop();
        std::filesystem::remove_all(directory);
    });
    const auto path = directory / "input.jsonl";
    replay::configure_recording(path.string(), {.rng_seed = 37});
    replay::start();
    auto boundaries = std::vector<replay::input_boundary_metadata>{};
    auto reads = 0;
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        REQUIRE(++reads <= 4);
        const auto active = game_client::active_input_context();
        REQUIRE(active.context);
        boundaries.push_back(
            {.context = active.context->category_name(),
             .actions = active.context->get_registered_actions_copy(),
             .timeout_ms = active.timeout_ms});
        const auto top = top_screen_now();
        if (reads == 1) {
            REQUIRE(topic_rows(top).size() > 1);
            return invoke_event(top, "help:topic", {.row = topic_rows(top)[1].id});
        }
        if (reads == 2) {
            check_reader(top);
            return resolve_action("PAGE_DOWN");
        }
        if (reads == 3) { check_reader(top); }
        if (reads == 4) { CHECK_FALSE(topic_rows(top).empty()); }
        return invoke_event(top, "QUIT");
    });
    auto recorded = help{};
    recorded.load();
    recorded.display_help();
    REQUIRE(reads == 4);
    replay::finish();
    REQUIRE(boundaries.size() == 4);
    CHECK(boundaries[0].context == "default");
    CHECK(boundaries[1].context == "SCROLLABLE_TEXT");
    CHECK(boundaries[2].context == "SCROLLABLE_TEXT");
    CHECK(boundaries[3] == boundaries[0]);
    CHECK(boundaries[2] == boundaries[1]);
    replay::configure_playback(path.string(), {.rng_seed = 37});
    replay::start();
    for (const auto index : std::views::iota(std::size_t{0}, boundaries.size())) {
        const auto event = replay::next_input_event(boundaries[index]);
        REQUIRE(event);
        if (index == 1) {
            CHECK(event->type == input_event_t::keyboard);
        } else if (index == 0) {
            REQUIRE(event->interaction);
            CHECK(event->interaction->operation == game_client::interaction_operation::invoke);
            CHECK(event->interaction->target_id == "help:topic");
        } else {
            CHECK(event->type != input_event_t::interaction);
        }
    }
    CHECK_THROWS_AS(replay::next_input_event(), replay::completed);
    replay::finish();

    game_client::memory::set_input_provider([](const int /*timeout*/) -> input_event {
        throw std::runtime_error("help/text replay must not request repeated live input");
    });
    replay::configure_playback(path.string(), {.rng_seed = 37});
    replay::start();
    auto played = help{};
    played.load();
    played.display_help();
    CHECK_THROWS_AS(replay::next_input_event(), replay::completed);
    replay::finish();
    CHECK_FALSE(game_client::current_interaction().structured);
}

TEST_CASE(
    "text reader replay keeps the recording when only the authorized text changes",
    "[client][interaction][text][replay][mcp]") {
    const auto guard = text_widget_guard{};
    static auto counter = std::atomic_uint64_t{0};
    const auto directory =
        std::filesystem::path(PATH_INFO::user_dir())
        / ("reader-schema-replay-" + std::to_string(counter.fetch_add(1)));
    REQUIRE(std::filesystem::create_directory(directory));
    const auto cleanup = on_out_of_scope([&] {
        replay::stop();
        std::filesystem::remove_all(directory);
    });
    const auto path = directory / "input.jsonl";
    replay::configure_recording(path.string(), {.rng_seed = 37});
    replay::start();
    game_client::memory::set_input_provider([](const int /*timeout*/) {
        const auto top = top_screen_now();
        check_reader(top);
        return invoke_event(top, "QUIT");
    });
    scrollable_text(reader_window, "Document", "Original authorized text");
    replay::finish();
    game_client::memory::set_input_provider([](const int /*timeout*/) -> input_event {
        throw std::runtime_error("reader replay must not read live input");
    });
    replay::configure_playback(path.string(), {.rng_seed = 37});
    replay::start();
    scrollable_text(reader_window, "Document", "Changed authorized text");
    CHECK_THROWS_AS(replay::next_input_event(), replay::completed);
    replay::finish();
    CHECK_FALSE(game_client::active_input_context().context);
    CHECK_FALSE(game_client::current_interaction().structured);
    replay::configure_playback(path.string(), {.rng_seed = 37});
    replay::start();
    scrollable_text(reader_window, "Document", "Original authorized text");
    CHECK_THROWS_AS(replay::next_input_event(), replay::completed);
    replay::finish();
}

TEST_CASE(
    "morale viewer publishes native values without query effects",
    "[client][interaction][morale][morale_view][mcp]") {
    const auto populated = GENERATE(false, true);
    const auto modifiers = GENERATE(false, true);
    const auto exit_action = std::string(GENERATE("invoke", "CONFIRM", "QUIT"));
    const auto guard = text_widget_guard{};
    const auto restore_rng = restore_on_out_of_scope<cata_default_random_engine>(rng_get_engine());
    auto morale = player_morale{};
    if (populated) {
        morale.add(MORALE_GAME, 12, 12);
        morale.add(MORALE_WET, -7, -7);
    }
    const auto serialize_morale = [&]() {
        auto output = std::ostringstream{};
        auto json = JsonOut(output);
        json.start_object();
        morale.store(json);
        json.end_object();
        return output.str();
    };
    const auto stat =
        [](const ec::screen_view& top, const std::string& key) -> const ec::panel_stat* {
        const auto* const totals = ec::find_component(*top.root, "morale:totals");
        REQUIRE(totals != nullptr);
        const auto& stats = std::get<ec::panel_body>(totals->body).stats;
        const auto found = std::ranges::find(stats, key, &ec::panel_stat::key);
        return found == stats.end() ? nullptr : &*found;
    };
    const auto row_value = [](const ec::screen_view& top, const std::string& id) {
        const auto* const list = ec::find_component(*top.root, "morale:points");
        REQUIRE(list != nullptr);
        const auto& rows = std::get<ec::list_body>(list->body).rows;
        const auto found = std::ranges::find(rows, id, &ec::row::id);
        REQUIRE(found != rows.end());
        return std::pair{found->label, found->columns.at(0).value};
    };
    auto reads = 0;
    auto shape = json{};
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        REQUIRE(++reads <= 3);
        const auto before = serialize_morale();
        const auto rng_before = rng_get_engine();
        const auto top = top_screen_now();
        CHECK(top.context == "MORALE");
        CHECK(top.title == _("Morale"));
        REQUIRE(stat(top, "focus") != nullptr);
        CHECK(stat(top, "focus")->label == ec::caption(_("Focus trends towards:")));
        CHECK(stat(top, "focus")->value == "93");
        CHECK((stat(top, "pain") != nullptr) == modifiers);
        CHECK((stat(top, "fatigue_cap") != nullptr) == modifiers);
        if (modifiers) {
            CHECK(stat(top, "pain")->label == ec::caption(_("Pain level:")));
            CHECK(stat(top, "pain")->value == "-4");
            CHECK(stat(top, "fatigue_cap")->label == ec::caption(_("Fatigue Morale Cap:")));
            CHECK(stat(top, "fatigue_cap")->value == "7");
        }
        REQUIRE(stat(top, "level") != nullptr);
        if (populated) {
            CHECK(row_value(top, "morale:positive:morale_game")
                  == std::pair{MORALE_GAME->describe(), std::string{"100%"}});
            CHECK(row_value(top, "morale:negative:morale_wet")
                  == std::pair{MORALE_WET->describe(), std::string{"100%"}});
            CHECK(row_value(top, "morale:positive").first == _("Total positive morale"));
            CHECK(row_value(top, "morale:positive").second == "+12");
            CHECK(row_value(top, "morale:negative").second == "-7");
            CHECK(stat(top, "level")->value == "+5");
        } else {
            const auto* const none = ec::find_component(*top.root, "morale:none");
            REQUIRE(none != nullptr);
            CHECK(std::get<ec::text_body>(none->body).markup == _("Nothing affects your morale"));
            CHECK(stat(top, "level")->value == "-");
        }
        if (shape.is_null()) { shape = top_json(); }
        CHECK(top_json() == shape);
        CHECK(serialize_morale() == before);
        CHECK(rng_get_engine() == rng_before);
        const auto invented = ec::resolve_invoke(live_state(), {.affordance_id = "morale:missing"});
        REQUIRE_FALSE(invented);
        CHECK(invented.error() == ec::error::unknown_affordance);
        const auto stale = resolution("QUIT", {.screen_id = "screen:old"});
        REQUIRE_FALSE(stale);
        CHECK(stale.error() == ec::error::stale_screen);
        if (reads == 1) { return resolve_action("DOWN"); }
        if (reads == 2) { return resolve_action("UP"); }
        return exit_action == "invoke" ? invoke_event(top, "QUIT") : resolve_action(exit_action);
    });
    morale.display(93, modifiers ? 4 : 0, modifiers ? 7 : 0);
    CHECK(reads == 3);
    CHECK_FALSE(game_client::current_interaction().structured);
}

TEST_CASE(
    "morale replay keeps the native close and accepts changed values",
    "[client][interaction][morale][morale_view][replay][mcp]") {
    const auto guard = text_widget_guard{};
    const auto directory = std::filesystem::path(PATH_INFO::user_dir()) / "morale-view-replay";
    REQUIRE(std::filesystem::create_directory(directory));
    const auto cleanup = on_out_of_scope([&]() {
        replay::stop();
        std::filesystem::remove_all(directory);
    });
    const auto path = directory / "inputs.jsonl";
    auto morale = player_morale{};
    morale.add(MORALE_GAME, 12, 12);
    replay::configure_recording(path.string(), {.rng_seed = 37});
    replay::start();
    game_client::memory::set_input_provider([](const int /*timeout*/) {
        const auto top = top_screen_now();
        REQUIRE(top.root);
        return invoke_event(top, "QUIT");
    });
    morale.display(93, 0, 0);
    replay::finish();
    game_client::memory::set_input_provider([](const int /*timeout*/) -> input_event {
        throw std::runtime_error("morale replay must not use live input");
    });
    replay::configure_playback(path.string(), {.rng_seed = 37});
    replay::start();
    morale.display(93, 0, 0);
    CHECK_THROWS_AS(replay::next_input_event(), replay::completed);
    replay::finish();
    replay::configure_playback(path.string(), {.rng_seed = 37});
    replay::start();
    morale.display(94, 0, 0);
    CHECK_THROWS_AS(replay::next_input_event(), replay::completed);
    replay::finish();
    CHECK_FALSE(game_client::active_input_context().context);
    CHECK_FALSE(game_client::current_interaction().structured);
}

#endif
