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
#    include "color.h"
#    include "cursesdef.h"
#    include "cursesport.h"
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
#    include <filesystem>
#    include <ranges>
#    include <sstream>
#    include <stdexcept>
#    include <string>
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

auto resolve_semantic(game_client::interaction_command command) -> input_event {
    auto input = game_client::input_command{};
    input.interaction = std::move(command);
    const auto result =
        game_client::resolve_input_command(input, game_client::memory::screen_size());
    REQUIRE(result.has_value());
    return *result;
}

auto resolve_action(const std::string& action) -> input_event {
    const auto result =
        game_client::resolve_input_command({.action = action}, game_client::memory::screen_size());
    REQUIRE(result.has_value());
    return *result;
}

auto cancel(const game_client::interaction_snapshot& snapshot) -> input_event {
    return resolve_semantic(
        {.input_id = snapshot.input_id, .operation = game_client::interaction_operation::cancel});
}

auto check_reader(const game_client::interaction_snapshot& snapshot) -> void {
    CHECK(snapshot.context == "SCROLLABLE_TEXT");
    CHECK(snapshot.kind == game_client::interaction_kind::custom);
    CHECK(snapshot.structured);
    CHECK_FALSE(snapshot.actions_only);
    CHECK(snapshot.allow_cancel);
    CHECK(snapshot.choices.empty());
    CHECK(snapshot.choice_total == 0);
    CHECK_FALSE(snapshot.allow_set_count);
    CHECK_FALSE(snapshot.field);
    CHECK_FALSE(snapshot.target);
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
    auto menu_schema = std::string{};
    auto previous_input = std::uint64_t{0};
    auto reads = 0;

    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        REQUIRE(++reads <= static_cast<int>(2 * topics.size() + 1));
        const auto snapshot = game_client::current_interaction({.limit = 200});
        CHECK(snapshot.input_id > previous_input);
        previous_input = snapshot.input_id;
        if (in_reader) {
            check_reader(snapshot);
            CHECK(snapshot.title == _(" HELP "));
            const auto& topic = topics[topic_index];
            CHECK(snapshot.message == expected_topic_text(topic));
            CHECK(snapshot.message.find("<press_") == std::string::npos);
            CHECK(snapshot.message.find("<HELP_DRAW_DIRECTIONS>") == std::string::npos);
            CHECK(snapshot.message.find("<DRAW_NOTE_COLORS>") == std::string::npos);
            ++topic_index;
            in_reader = false;
            return cancel(snapshot);
        }
        CHECK(snapshot.kind == game_client::interaction_kind::choices);
        CHECK(snapshot.title == _(" HELP "));
        CHECK(snapshot.structured);
        CHECK(snapshot.allow_cancel);
        REQUIRE(snapshot.choice_total == topics.size());
        REQUIRE(snapshot.choices.size() == topics.size());
        if (menu_schema.empty()) { menu_schema = snapshot.schema_id; }
        CHECK(snapshot.schema_id == menu_schema);
        for (auto index = std::size_t{0}; index < topics.size(); ++index) {
            CHECK(snapshot.choices[index].label
                  == remove_color_tags(shortcut_text(c_light_blue, _(topics[index].name))));
        }
        const auto page = game_client::current_interaction({.offset = 1, .limit = 1});
        REQUIRE(page.choices.size() == 1);
        CHECK(page.choice_total == topics.size());
        CHECK(page.choices.front().id == snapshot.choices[1].id);
        CHECK(page.schema_id == snapshot.schema_id);
        CHECK(page.input_id == snapshot.input_id);
        if (topic_index == topics.size()) { return cancel(snapshot); }
        in_reader = true;
        return resolve_semantic(
            {.input_id = snapshot.input_id,
             .operation = game_client::interaction_operation::choose,
             .target_id = snapshot.choices[topic_index].id});
    });
    widget.display_help();
    CHECK(topic_index == topics.size());
    CHECK(reads == static_cast<int>(2 * topics.size() + 1));
    CHECK_FALSE(game_client::current_interaction().structured);
}

TEST_CASE(
    "help semantic validation rejects stale unknown and changed-schema input",
    "[client][interaction][help][text][mcp]") {
    const auto guard = text_widget_guard{};
    auto widget = help{};
    widget.load();
    auto reads = 0;
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        REQUIRE(++reads == 1);
        const auto snapshot = game_client::current_interaction();
        REQUIRE_FALSE(snapshot.choices.empty());
        auto command = game_client::interaction_command{
            .input_id = snapshot.input_id - 1,
            .operation = game_client::interaction_operation::choose,
            .target_id = snapshot.choices.front().id,
        };
        const auto stale = game_client::resolve_interaction_command(command);
        REQUIRE_FALSE(stale);
        CHECK(stale.error().starts_with("stale:"));
        command.input_id = snapshot.input_id;
        command.target_id = "unknown-help-topic";
        CHECK_FALSE(game_client::resolve_interaction_command(command));
        const auto active = game_client::active_input_context();
        REQUIRE(active.context);
        const auto event = game_client::interaction_event{
            .operation = game_client::interaction_operation::choose,
            .schema_id = "changed-schema",
            .target_id = snapshot.choices.front().id,
        };
        CHECK_FALSE(game_client::validate_interaction_event(*active.context, event));
        const auto unchanged = game_client::current_interaction();
        CHECK(unchanged.input_id == snapshot.input_id);
        CHECK(unchanged.schema_id == snapshot.schema_id);
        CHECK(unchanged.choices.front().id == snapshot.choices.front().id);
        return cancel(snapshot);
    });
    widget.display_help();
}

TEST_CASE(
    "raw loaded help hotkeys and semantic choices share the same native content",
    "[client][interaction][help][text][mcp]") {
    const auto guard = text_widget_guard{};
    const auto topics = read_topics();
    for (auto index = std::size_t{0}; index < topics.size(); ++index) {
        CAPTURE(index, topics[index].name);
        auto semantic_text = std::string{};
        auto semantic_title = std::string{};
        for (const auto raw : {false, true}) {
            auto widget = help{};
            widget.load();
            auto reads = 0;
            game_client::memory::set_input_provider([&](const int /*timeout*/) {
                const auto snapshot = game_client::current_interaction({.limit = 200});
                REQUIRE(++reads <= 3);
                if (reads == 1) {
                    REQUIRE(index < snapshot.choices.size());
                    if (raw) {
                        const auto keys = get_hotkeys(topics[index].name);
                        REQUIRE_FALSE(keys.empty());
                        const auto result = game_client::resolve_input_command(
                            {.key = keys.front()}, game_client::memory::screen_size());
                        REQUIRE(result);
                        return *result;
                    }
                    return resolve_semantic(
                        {.input_id = snapshot.input_id,
                         .operation = game_client::interaction_operation::choose,
                         .target_id = snapshot.choices[index].id});
                }
                if (reads == 2) {
                    check_reader(snapshot);
                    if (raw) {
                        CHECK(snapshot.message == semantic_text);
                        CHECK(snapshot.title == semantic_title);
                    } else {
                        semantic_text = snapshot.message;
                        semantic_title = snapshot.title;
                    }
                    return resolve_action("CONFIRM");
                }
                CHECK(snapshot.kind == game_client::interaction_kind::choices);
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
    auto schema = std::string{};
    auto reads = 0;
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        REQUIRE(++reads <= 8);
        const auto snapshot = game_client::current_interaction();
        check_reader(snapshot);
        CHECK(snapshot.title == title);
        CHECK(snapshot.message == text);
        if (schema.empty()) { schema = snapshot.schema_id; }
        CHECK(snapshot.schema_id == schema);
        const auto choose = game_client::resolve_interaction_command(
            {.input_id = snapshot.input_id,
             .operation = game_client::interaction_operation::choose,
             .target_id = "invented-choice"});
        CHECK_FALSE(choose);
        const auto count = game_client::resolve_interaction_command(
            {.input_id = snapshot.input_id,
             .operation = game_client::interaction_operation::set_count,
             .target_id = "invented-choice",
             .count = 1});
        CHECK_FALSE(count);
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
        return cancel(snapshot);
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
    auto schema = std::string{};
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
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        REQUIRE(++reads <= 4);
        const auto snapshot = game_client::current_interaction();
        check_reader(snapshot);
        CHECK(snapshot.title == title);
        CHECK(snapshot.message == text);
        if (schema.empty()) { schema = snapshot.schema_id; }
        CHECK(snapshot.schema_id == schema);
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
            const auto resized = game_client::current_interaction();
            check_reader(resized);
            CHECK(resized.input_id == snapshot.input_id);
            CHECK(resized.schema_id == schema);
            CHECK(resized.title == title);
            CHECK(resized.message == text);
            return resolve_action("DOWN");
        }
        check_viewport(1);
        resize_viewport({80, 32});
        CHECK(window_creations == 3);
        CHECK(foldstring(text, viewport.x - 2) == initial_lines);
        check_viewport(1);
        const auto narrowed = game_client::current_interaction();
        check_reader(narrowed);
        CHECK(narrowed.input_id == snapshot.input_id);
        CHECK(narrowed.schema_id == schema);
        CHECK(narrowed.title == title);
        CHECK(narrowed.message == text);
        resize_viewport({80, 64});
        CHECK(window_creations == 4);
        REQUIRE(static_cast<int>(initial_lines.size()) < viewport.y - 2);
        check_viewport(0);
        const auto resized = game_client::current_interaction();
        check_reader(resized);
        CHECK(resized.input_id == snapshot.input_id);
        CHECK(resized.schema_id == schema);
        CHECK(resized.title == title);
        CHECK(resized.message == text);
        return cancel(resized);
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
    auto menu_schema = std::string{};
    auto topic_id = std::string{};
    auto reader_schema = std::string{};
    auto previous_input = std::uint64_t{0};
    auto reads = 0;
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        REQUIRE(++reads <= 7);
        const auto snapshot = game_client::current_interaction();
        CHECK(snapshot.input_id > previous_input);
        previous_input = snapshot.input_id;
        if (reads == 1) {
            REQUIRE(snapshot.choices.size() > 1);
            menu_schema = snapshot.schema_id;
            topic_id = snapshot.choices[1].id;
            return resolve_semantic(
                {.input_id = snapshot.input_id,
                 .operation = game_client::interaction_operation::choose,
                 .target_id = topic_id});
        }
        if (reads == 2) {
            check_reader(snapshot);
            CHECK(snapshot.message == expected_text);
            reader_schema = snapshot.schema_id;
            return resolve_action("HELP_KEYBINDINGS");
        }
        if (reads == 3) {
            CHECK(snapshot.context == "HELP_KEYBINDINGS");
            CHECK(snapshot.kind == game_client::interaction_kind::field);
            CHECK(snapshot.structured);
            CHECK(snapshot.message != expected_text);
            return resolve_action("ADD_LOCAL");
        }
        if (reads == 4) {
            CHECK(snapshot.context == "HELP_KEYBINDINGS");
            CHECK_FALSE(snapshot.structured);
            CHECK(snapshot.actions_only);
            CHECK(snapshot.message.empty());
            CHECK(snapshot.choices.empty());
            CHECK_FALSE(snapshot.field);
            CHECK_FALSE(snapshot.allow_cancel);
            CHECK_FALSE(game_client::resolve_interaction_command(
                {.input_id = snapshot.input_id,
                 .operation = game_client::interaction_operation::cancel}));
            return resolve_action("QUIT");
        }
        if (reads == 5) {
            CHECK(snapshot.context == "HELP_KEYBINDINGS");
            CHECK(snapshot.kind == game_client::interaction_kind::field);
            return resolve_action("QUIT");
        }
        if (reads == 6) {
            check_reader(snapshot);
            CHECK(snapshot.schema_id == reader_schema);
            CHECK(snapshot.message == expected_text);
            return cancel(snapshot);
        }
        CHECK(snapshot.kind == game_client::interaction_kind::choices);
        CHECK(snapshot.schema_id == menu_schema);
        REQUIRE(snapshot.choices.size() > 1);
        CHECK(snapshot.choices[1].id == topic_id);
        return cancel(snapshot);
    });
    auto widget = help{};
    widget.load();
    widget.display_help();
    CHECK(reads == 7);
    CHECK_FALSE(game_client::current_interaction().structured);
}

TEST_CASE(
    "failed nested help reader releases widget providers and input contexts",
    "[client][interaction][help][text][mcp]") {
    const auto guard = text_widget_guard{};
    auto reads = 0;
    game_client::memory::set_input_provider([&](const int /*timeout*/) -> input_event {
        REQUIRE(++reads <= 2);
        const auto snapshot = game_client::current_interaction();
        if (reads == 1) {
            REQUIRE_FALSE(snapshot.choices.empty());
            return resolve_semantic(
                {.input_id = snapshot.input_id,
                 .operation = game_client::interaction_operation::choose,
                 .target_id = snapshot.choices.front().id});
        }
        check_reader(snapshot);
        throw std::runtime_error("test reader input failure");
    });
    auto widget = help{};
    widget.load();
    CHECK_THROWS_WITH(widget.display_help(), "test reader input failure");
    CHECK(reads == 2);
    CHECK_FALSE(game_client::active_input_context().context);
    CHECK_FALSE(game_client::current_interaction().structured);

    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto snapshot = game_client::current_interaction();
        CHECK(snapshot.kind == game_client::interaction_kind::choices);
        return cancel(snapshot);
    });
    widget.display_help();
    CHECK_FALSE(game_client::current_interaction().structured);
}

TEST_CASE(
    "semantic help selection and reader close replay without live input",
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
        const auto snapshot = game_client::current_interaction();
        if (reads == 1) {
            REQUIRE(snapshot.choices.size() > 1);
            return resolve_semantic(
                {.input_id = snapshot.input_id,
                 .operation = game_client::interaction_operation::choose,
                 .target_id = snapshot.choices[1].id});
        }
        if (reads == 2) {
            check_reader(snapshot);
            return resolve_action("PAGE_DOWN");
        }
        if (reads == 3) { check_reader(snapshot); }
        if (reads == 4) { CHECK(snapshot.kind == game_client::interaction_kind::choices); }
        return cancel(snapshot);
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
        } else {
            REQUIRE(event->interaction);
            CHECK(event->interaction->operation
                  == (index == 0 ? game_client::interaction_operation::choose
                                 : game_client::interaction_operation::cancel));
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
    "text reader replay rejects changed authorized text at the real widget boundary",
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
        const auto snapshot = game_client::current_interaction();
        check_reader(snapshot);
        return cancel(snapshot);
    });
    scrollable_text(reader_window, "Document", "Original authorized text");
    replay::finish();
    game_client::memory::set_input_provider([](const int /*timeout*/) -> input_event {
        throw std::runtime_error("reader replay must not read live input");
    });
    replay::configure_playback(path.string(), {.rng_seed = 37});
    replay::start();
    CHECK_THROWS_WITH(scrollable_text(reader_window, "Document", "Changed authorized text"),
                      Catch::Matchers::Contains("schema changed"));
    replay::stop();
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
    const auto exit_action = std::string(GENERATE("semantic", "CONFIRM", "QUIT"));
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
    auto reads = 0;
    auto schema = std::string{};
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        REQUIRE(++reads <= 3);
        const auto before = serialize_morale();
        const auto rng_before = rng_get_engine();
        const auto current = game_client::current_interaction();
        REQUIRE(current.structured);
        CHECK_FALSE(current.actions_only);
        CHECK(current.context == "MORALE");
        CHECK(current.kind == game_client::interaction_kind::custom);
        CHECK(current.title == _("Morale"));
        CHECK(current.allow_cancel);
        CHECK(current.choices.empty());
        CHECK_FALSE(current.field);
        CHECK_FALSE(current.target);
        CHECK(current.message.find(std::string(_("Focus trends towards:")) + " 93")
              != std::string::npos);
        CHECK((current.message.find(_("Pain level:")) != std::string::npos) == modifiers);
        CHECK((current.message.find(_("Fatigue Morale Cap:")) != std::string::npos) == modifiers);
        if (modifiers) {
            CHECK(current.message.find(std::string(_("Pain level:")) + " -4") != std::string::npos);
            CHECK(current.message.find(std::string(_("Fatigue Morale Cap:")) + " 7")
                  != std::string::npos);
        }
        if (populated) {
            CHECK(current.message.find(MORALE_GAME->describe() + " 100%") != std::string::npos);
            CHECK(current.message.find(MORALE_WET->describe() + " 100%") != std::string::npos);
            CHECK(current.message.find(std::string(_("Total positive morale")) + " +12")
                  != std::string::npos);
            CHECK(current.message.find(std::string(_("Total negative morale")) + " -7")
                  != std::string::npos);
            CHECK(
                current.message.find(std::string(_("Total morale:")) + " +5") != std::string::npos);
        } else {
            CHECK(current.message.find(_("Nothing affects your morale")) != std::string::npos);
            CHECK(
                current.message.find(std::string(_("Total morale:")) + " -") != std::string::npos);
        }
        const auto repeated = game_client::current_interaction();
        CHECK(game_client::serialize_interaction(repeated)
              == game_client::serialize_interaction(current));
        CHECK(serialize_morale() == before);
        CHECK(rng_get_engine() == rng_before);
        CHECK_FALSE(
            game_client::resolve_interaction_command(
                {
                    .input_id = current.input_id + 1,
                    .operation = game_client::interaction_operation::cancel,
                })
                .has_value());
        CHECK_FALSE(
            game_client::resolve_interaction_command(
                {
                    .input_id = current.input_id,
                    .operation = game_client::interaction_operation::choose,
                    .target_id = "missing",
                })
                .has_value());
        if (schema.empty()) { schema = current.schema_id; }
        CHECK(current.schema_id == schema);
        if (reads == 1) { return resolve_action("DOWN"); }
        if (reads == 2) { return resolve_action("UP"); }
        return exit_action == "semantic" ? cancel(current) : resolve_action(exit_action);
    });
    morale.display(93, modifiers ? 4 : 0, modifiers ? 7 : 0);
    CHECK(reads == 3);
    CHECK_FALSE(game_client::current_interaction().structured);
}

TEST_CASE(
    "morale semantic replay keeps native close and rejects changed values",
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
        const auto current = game_client::current_interaction();
        REQUIRE(current.structured);
        return cancel(current);
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
    CHECK_THROWS_WITH(morale.display(94, 0, 0), Catch::Matchers::Contains("schema changed"));
    replay::stop();
    CHECK_FALSE(game_client::active_input_context().context);
    CHECK_FALSE(game_client::current_interaction().structured);
}

#endif
