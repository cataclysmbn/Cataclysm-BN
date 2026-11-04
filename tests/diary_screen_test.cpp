#if defined(CATA_MCP)

#    include "avatar.h"
#    include "calendar.h"
#    include "catch/catch.hpp"
#    include "client_memory.h"
#    include "client_presentation.h"
#    include "client_screen_support.h"
#    include "diary.h"
#    include "engine_client_component.h"
#    include "item_serial.h"
#    include "locale_fixture.h"
#    include "locale_scope.h"
#    include "map_helpers.h"
#    include "page_serial.h"
#    include "player_helpers.h"
#    include "state_helpers.h"
#    include "string_editor_window.h"

#    include <algorithm>
#    include <functional>
#    include <nlohmann/json.hpp>
#    include <sstream>
#    include <string>
#    include <variant>
#    include <vector>

namespace {
namespace ec = engine_client;
using namespace client_screen_test;
using json = nlohmann::json;

constexpr auto pages_id = "diary:pages:list";
constexpr auto page_id = "diary:page";
constexpr auto editor_field_id = "editor:text";
constexpr auto editor_set_id = "editor:set";

auto part(const ec::screen_view& screen, const std::string& id) -> const ec::component& {
    REQUIRE(screen.root);
    const auto* const found = ec::find_component(*screen.root, id);
    INFO(id);
    REQUIRE(found != nullptr);
    return *found;
}
auto page_rows(const ec::screen_view& screen) -> const std::vector<ec::row>& {
    const auto& list = part(screen, pages_id);
    REQUIRE(std::holds_alternative<ec::list_body>(list.body));
    return std::get<ec::list_body>(list.body).rows;
}
auto ids_of(const std::vector<ec::row>& rows) -> std::vector<std::string> {
    auto ids = std::vector<std::string>{};
    for (const auto& row : rows) { ids.push_back(row.id); }
    return ids;
}
auto markup(const ec::screen_view& screen, const std::string& id) -> std::string {
    const auto& text = part(screen, id);
    REQUIRE(std::holds_alternative<ec::text_body>(text.body));
    return std::get<ec::text_body>(text.body).markup;
}
auto editor_field(const ec::screen_view& screen) -> const ec::form_field& {
    const auto& form = part(screen, "editor:form");
    REQUIRE(std::holds_alternative<ec::form_body>(form.body));
    const auto& fields = std::get<ec::form_body>(form.body).fields;
    REQUIRE(fields.size() == 1);
    return fields.front();
}
auto has_affordance(const ec::screen_view& screen, const std::string& id) -> bool {
    return std::ranges::contains(screen.affordances, id, &ec::affordance::id);
}

/// Whether invoking the affordance with the arguments is refused as invalid.
auto refused(const ec::screen_view& screen, const std::string& affordance, ec::invoke_args args)
    -> bool {
    const auto state = ec::capture_boundary(
        {.epoch = "epoch:test",
         .ready = {.phase = "waiting_for_input",
                   .accepts_interaction_commands = true,
                   .accepts_registered_actions = true}});
    REQUIRE(state.has_value());
    CHECK(screen.screen_id == ec::top_screen(*state).screen_id);
    const auto resolved =
        ec::resolve_invoke(*state, {.affordance_id = affordance, .args = std::move(args)});
    return !resolved.has_value() && resolved.error() == ec::error::invalid_arguments;
}

/// The diary of a fresh avatar named `Pin`, open under a screen guard.
struct diary_fixture {
    std::string old_name = get_avatar().name;
    // The state is cleared before the guard turns the test mode off, which the clearing needs.
    const bool cleared =
        (clear_all_state(), clear_map(), clear_character(get_avatar(), false), true);
    const screen_guard guard;
    const bool named =
        (get_avatar().name = "Pin",
         calendar::turn = calendar::start_of_cataclysm + 3_days + 5_hours, true);
    // The avatar's own diary keeps its pages from one test to the next.
    diary book;

    diary_fixture() = default;
    ~diary_fixture() {
        game_client::memory::set_input_provider({});
        get_avatar().name = old_name;
    }
    diary_fixture(const diary_fixture&) = delete;
    auto operator=(const diary_fixture&) -> diary_fixture& = delete;

    /// Opens the diary and answers each of its input reads with `script`, which gets the number
    /// of the read and the screen on top, until it quits.
    auto open(const std::function<auto(int, const ec::screen_view&)->input_event>& script) -> void {
        auto reads = 0;
        game_client::memory::set_input_provider([&](const int /*timeout*/) {
            REQUIRE(reads < max_screen_reads);
            return script(reads++, top_screen_now());
        });
        diary::show_diary_ui(&book);
    }
};

} // namespace

TEST_CASE(
    "the diary is the same at any viewport size and has the same ids in another language",
    "[diary_screen][engine_client_component]") {
    auto fixture = diary_fixture{};
    const auto fixture_ko = locale_fixture::korean_catalogue{{
        {"pages: %d", "pages: %d-ko"},
        {"Info", "Info-ko"},
        {"%s's Diary", "%s's Diary-ko"},
    }};
    auto reads = 0;
    fixture.open([&](const int read, const ec::screen_view& top) {
        if (read == 0) { return run_action("NEW_PAGE"); }
        ++reads;
        REQUIRE(top.root);
        const auto [narrow, wide] = at_both_sizes();
        CHECK(narrow.dump() == wide.dump());
        const auto korean = [] {
            const auto scope = localization::locale_scope(locale_fixture::korean);
            return top_json();
        }();
        CHECK(without_words(narrow) == without_words(korean));
        // The words really do differ, so equal ids are not equal text.
        CHECK(narrow.dump() != korean.dump());
        return run_action("QUIT");
    });
    CHECK(reads == 1);
}

TEST_CASE("the diary publishes no window text", "[diary_screen][engine_client_component]") {
    auto fixture = diary_fixture{};
    fixture.open([&](const int read, const ec::screen_view& top) {
        if (read == 0) { return run_action("NEW_PAGE"); }
        REQUIRE(top.root);
        CHECK(top.context == "DIARY");
        REQUIRE(page_rows(top).size() == 1);
        for (const auto& row : page_rows(top)) {
            CHECK(row.label.find("<color") == std::string::npos);
            CHECK(row.id.find(row.label) == std::string::npos);
        }
        CHECK(has_affordance(top, page_id));
        for (const auto* const action : {"NEW_PAGE", "CONFIRM", "DELETE PAGE", "EXPORT_DIARY"}) {
            CHECK(has_affordance(top, action));
        }
        return run_action("QUIT");
    });
}

TEST_CASE(
    "two pages made in the same turn have different ids that outlive the deletion of the page before them",
    "[diary_screen][diary_identity]") {
    auto fixture = diary_fixture{};
    auto before = std::vector<std::string>{};
    auto after = std::vector<std::string>{};
    fixture.open([&](const int read, const ec::screen_view& top) {
        switch (read) {
            case 0:
            case 1:
                return run_action("NEW_PAGE");
            case 2: {
                before = ids_of(page_rows(top));
                REQUIRE(before.size() == 2);
                CHECK(before.front() != before.back());
                // The pages are equal to the player: both were written in this turn.
                CHECK(page_rows(top).front().label == page_rows(top).back().label);
                return invoke_event(top, page_id, {.row = before.front()});
            }
            case 3:
                return run_action("DELETE PAGE");
            case 4: {
                REQUIRE(top.context == "YESNO");
                return run_action("YES");
            }
            case 5: {
                after = ids_of(page_rows(top));
                return run_action("QUIT");
            }
            default:
                FAIL("unexpected read " << read);
        }
        return input_event{};
    });
    REQUIRE(after.size() == 1);
    CHECK(after.front() == before.back());
}

TEST_CASE(
    "a page row opens its page and a row the diary does not hold is refused",
    "[diary_screen][diary_identity]") {
    auto fixture = diary_fixture{};
    fixture.open([&](const int read, const ec::screen_view& top) {
        switch (read) {
            case 0:
            case 1:
                return run_action("NEW_PAGE");
            case 2: {
                const auto ids = ids_of(page_rows(top));
                REQUIRE(ids.size() == 2);
                CHECK(page_rows(top).back().selected);
                CHECK_FALSE(page_rows(top).front().selected);
                CHECK(markup(top, "diary:changes:text").find("2/2") != std::string::npos);
                CHECK(refused(top, page_id, {.row = "page:0"}));
                CHECK(refused(top, page_id, {.row = ""}));
                CHECK(refused(top, page_id, {}));
                CHECK(refused(top, page_id, {.row = ids.front(), .text = "x"}));
                return invoke_event(top, page_id, {.row = ids.front()});
            }
            case 3: {
                CHECK(page_rows(top).front().selected);
                CHECK_FALSE(page_rows(top).back().selected);
                CHECK(markup(top, "diary:changes:text").find("1/2") != std::string::npos);
                return run_action("QUIT");
            }
            default:
                FAIL("unexpected read " << read);
        }
        return input_event{};
    });
}

TEST_CASE(
    "a diary page keeps its serial, gets none twice and starts the numbering again with the items'",
    "[diary_identity]") {
    game_client::reset_item_serials();
    auto first = diary_page{};
    auto second = diary_page{};
    const auto a = game_client::diary_page_serial(first);
    const auto b = game_client::diary_page_serial(second);
    CHECK(a == 1);
    CHECK(b == 2);
    CHECK(game_client::diary_page_serial(first) == a);
    CHECK(game_client::diary_page_row_id(second) == "page:2");
    // A page deleted makes its serial unused, not free: the next page takes a new one.
    game_client::forget_diary_page_serial(&first);
    auto third = diary_page{};
    CHECK(game_client::diary_page_serial(third) == 3);
    game_client::reset_item_serials();
    CHECK(game_client::diary_page_serial(second) == 1);
}

TEST_CASE("the serial of a diary page is not saved", "[diary_identity]") {
    auto fixture = diary_fixture{};
    fixture.open([&](const int read, const ec::screen_view& /*top*/) {
        return run_action(read == 0 ? "NEW_PAGE" : "QUIT");
    });
    auto saved = std::ostringstream{};
    fixture.book.serialize(saved);
    CHECK(saved.str().find("serial") == std::string::npos);
    CHECK(saved.str().find("page:") == std::string::npos);
}

TEST_CASE("the diary edits the text of a page through the text editor", "[diary_screen]") {
    auto fixture = diary_fixture{};
    fixture.open([&](const int read, const ec::screen_view& top) {
        switch (read) {
            case 0:
                return run_action("NEW_PAGE");
            case 1:
                return run_action("CONFIRM");
            case 2: {
                CHECK(top.context == "STRING_EDITOR");
                return invoke_event(top, editor_set_id, {.text = "Found a rifle.\nSafe here."});
            }
            case 3:
                return run_action("TEXT.CONFIRM");
            case 4: {
                CHECK(top.context == "DIARY");
                CHECK(markup(top, "diary:page:text") == "Found a rifle.\nSafe here.");
                return run_action("QUIT");
            }
            default:
                FAIL("unexpected read " << read);
        }
        return input_event{};
    });
}

TEST_CASE(
    "the text editor publishes its text, its cursor and that it takes several lines",
    "[text_editor_screen][engine_client_component]") {
    clear_all_state();
    const auto guard = screen_guard{};
    auto step = 0;
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto top = top_screen_now();
        CHECK(top.context == "STRING_EDITOR");
        const auto& field = editor_field(top);
        CHECK(field.id == editor_field_id);
        CHECK(field.type == "text");
        CHECK(field.multiline);
        CHECK_FALSE(field.printable);
        switch (step++) {
            case 0: {
                CHECK(field.value == "ab\ncd");
                // Natively the cursor starts after the last character.
                CHECK(field.cursor == 5);
                const auto wire = top_json().dump();
                CHECK(wire.find("\"multiline\":true") != std::string::npos);
                CHECK(wire.find("\"cursor\":5") != std::string::npos);
                return run_action("TEXT.LEFT");
            }
            case 1: {
                CHECK(field.cursor == 4);
                return run_action("TEXT.UP");
            }
            case 2: {
                CHECK(field.cursor == 1);
                return run_action("TEXT.QUIT");
            }
            default:
                FAIL("unexpected read");
        }
        return input_event{};
    });
    auto editor =
        string_editor_window{[] { return catacurses::newwin(10, 40, point_zero); }, "ab\ncd"};
    const auto [confirmed, text] = editor.query_string();
    CHECK_FALSE(confirmed);
    CHECK(text == "ab\ncd");
    CHECK(step == 3);
}

TEST_CASE(
    "the text editor takes a whole text, confirms it and puts the cursor after it",
    "[text_editor_screen]") {
    clear_all_state();
    const auto guard = screen_guard{};
    auto step = 0;
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto top = top_screen_now();
        switch (step++) {
            case 0: {
                CHECK(has_affordance(top, editor_set_id));
                CHECK(refused(top, editor_set_id, {}));
                CHECK(refused(top, editor_set_id, {.row = "x", .text = "y"}));
                return invoke_event(top, editor_set_id, {.text = "print(1)\nprint(2)"});
            }
            case 1: {
                CHECK(editor_field(top).value == "print(1)\nprint(2)");
                CHECK(editor_field(top).cursor == 17);
                return run_action("TEXT.CONFIRM");
            }
            default:
                FAIL("unexpected read");
        }
        return input_event{};
    });
    auto editor =
        string_editor_window{[] { return catacurses::newwin(10, 40, point_zero); }, "seed"};
    const auto [confirmed, text] = editor.query_string();
    CHECK(confirmed);
    CHECK(text == "print(1)\nprint(2)");
    CHECK(step == 2);
}

TEST_CASE(
    "the text editor offers every action of its context and the paste only where there is a clipboard",
    "[text_editor_screen]") {
    clear_all_state();
    const auto guard = screen_guard{};
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto top = top_screen_now();
        for (const auto* const action :
             {"TEXT.QUIT", "TEXT.CONFIRM", "TEXT.LEFT", "TEXT.RIGHT", "TEXT.UP", "TEXT.DOWN",
              "TEXT.CLEAR", "TEXT.BACKSPACE", "TEXT.HOME", "TEXT.END", "TEXT.PAGE_UP",
              "TEXT.PAGE_DOWN", "TEXT.DELETE", "TEXT.INPUT_FROM_FILE"}) {
            CHECK(has_affordance(top, action));
        }
        CHECK(
            has_affordance(top, "TEXT.PASTE") == game_client::presentation().clipboard_available());
        return run_action("TEXT.QUIT");
    });
    auto editor = string_editor_window{[] { return catacurses::newwin(10, 40, point_zero); }, ""};
    CHECK_FALSE(editor.query_string().first);
}

#endif
