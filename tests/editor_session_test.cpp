#if defined(CATA_MCP)

#    include "catch/catch.hpp"
#    include "client_command.h"
#    include "client_input.h"
#    include "client_interaction.h"
#    include "client_memory.h"
#    include "client_memory_scope.h"
#    include "cursesdef.h"
#    include "cursesport.h"
#    include "editor/editor_session.h"
#    include "engine_client_component.h"
#    include "engine_client_contract.h"
#    include "game_constants.h"
#    include "input.h"
#    include "json_formatter.h"
#    include "output.h"
#    include "path_info.h"
#    include "point.h"
#    include "state_helpers.h"

#    include <algorithm>
#    include <filesystem>
#    include <fstream>
#    include <nlohmann/json.hpp>
#    include <ranges>
#    include <sstream>
#    include <string>
#    include <variant>
#    include <vector>

namespace {
namespace ec = engine_client;

struct screen_guard {
    const game_client::memory::scoped_state memory;
    bool old_test_mode = test_mode;
    int old_termx = TERMX;
    int old_termy = TERMY;

    screen_guard() {
        clear_all_state();
        test_mode = false;
        TERMX = 100;
        TERMY = 40;
        game_client::memory::resize(TERMX, TERMY);
        catacurses::stdscr = catacurses::newwin(TERMY, TERMX, point_zero);
        catacurses::newscr = catacurses::newwin(TERMY, TERMX, point_zero);
    }

    ~screen_guard() {
        game_client::memory::set_input_provider({});
        TERMX = old_termx;
        TERMY = old_termy;
        test_mode = old_test_mode;
    }
};

auto read_file(const std::filesystem::path& path) -> std::string {
    auto in = std::ifstream(path, std::ios::binary);
    auto text = std::ostringstream{};
    text << in.rdbuf();
    return text.str();
}

/// A mod file of one 24x24 mapgen of floor with a wall symbol, removed when the test ends.
struct mod_file {
    std::filesystem::path path =
        std::filesystem::path(PATH_INFO::user_moddir()) / "editor_session_test" / "floor.json";
    std::string text;

    mod_file() {
        auto rows = nlohmann::ordered_json::array();
        for (auto row = 0; row < 24; ++row) { rows.push_back(std::string(24, '.')); }
        const auto entry = nlohmann::ordered_json::array(
            {{{"type", "mapgen"},
              {"method", "json"},
              {"om_terrain", "field"},
              {"object", {{"rows", rows}, {"terrain", {{".", "t_floor"}, {"#", "t_wall"}}}}}}});
        text = format_json(entry.dump()).value();
        std::filesystem::create_directories(path.parent_path());
        auto out = std::ofstream(path, std::ios::binary | std::ios::trunc);
        out << text;
    }

    ~mod_file() { std::filesystem::remove_all(path.parent_path()); }
};

auto captured() -> ec::boundary_state {
    const auto state = ec::capture_boundary(
        {.epoch = "epoch:editor",
         .ready =
             {.phase = "waiting_for_input",
              .game_ready = true,
              .accepts_interaction_commands = true,
              .accepts_registered_actions = true}});
    REQUIRE(state.has_value());
    return *state;
}

auto act(const std::string& action) -> input_event {
    auto input = game_client::input_command{};
    input.action = action;
    const auto result =
        game_client::resolve_input_command(input, game_client::memory::screen_size());
    REQUIRE(result.has_value());
    return *result;
}

auto invoke(const std::string& affordance, game_client::interaction_arguments args = {})
    -> input_event {
    auto event = input_event{};
    event.type = input_event_t::interaction;
    event.interaction = game_client::interaction_event{
        .operation = game_client::interaction_operation::invoke,
        .target_id = affordance,
        .arguments = std::move(args)};
    return event;
}

auto answer(const std::string& label) -> input_event {
    const auto snapshot = game_client::current_interaction({.limit = 200});
    const auto option =
        std::ranges::find(snapshot.choices, label, &game_client::interaction_choice::label);
    REQUIRE(option != snapshot.choices.end());
    auto input = game_client::input_command{};
    input.interaction = game_client::interaction_command{
        .input_id = snapshot.input_id,
        .operation = game_client::interaction_operation::choose,
        .target_id = option->id};
    const auto result =
        game_client::resolve_input_command(input, game_client::memory::screen_size());
    REQUIRE(result.has_value());
    return *result;
}

auto part(const ec::screen_view& screen, const std::string& id) -> const ec::component& {
    REQUIRE(screen.root);
    const auto* const found = ec::find_component(*screen.root, id);
    INFO(id);
    REQUIRE(found != nullptr);
    return *found;
}
template <typename Body> auto body(const ec::component& value) -> const Body& {
    REQUIRE(std::holds_alternative<Body>(value.body));
    return std::get<Body>(value.body);
}
auto affordance(const ec::screen_view& screen, const std::string& id) -> const ec::affordance& {
    const auto found = std::ranges::find(screen.affordances, id, &ec::affordance::id);
    INFO(id);
    REQUIRE(found != screen.affordances.end());
    return *found;
}
auto has_affordance(const ec::screen_view& screen, const std::string& id) -> bool {
    return std::ranges::contains(screen.affordances, id, &ec::affordance::id);
}
auto cell(const ec::screen_view& screen, const int x, const int y) -> const ec::grid_cell& {
    const auto& cells = body<ec::grid_body>(part(screen, "editor:canvas")).cells;
    const auto found = std::ranges::find(cells, ec::grid_point{x, y}, &ec::grid_cell::at);
    REQUIRE(found != cells.end());
    return *found;
}
auto walls(const ec::screen_view& screen) -> std::size_t {
    const auto& cells = body<ec::grid_body>(part(screen, "editor:canvas")).cells;
    return static_cast<std::size_t>(
        std::ranges::count(cells, std::string("#"), &ec::grid_cell::glyph));
}
auto saved(const ec::screen_view& screen) -> std::string {
    return body<ec::panel_body>(part(screen, "editor:document")).stats.front().value;
}
/// Whether invoking the affordance with the arguments is refused as invalid.
auto refused(
    const ec::boundary_state& state, const std::string& affordance, const ec::invoke_args& args)
    -> bool {
    return ec::resolve_invoke(state, {.affordance_id = affordance, .args = args}).error()
        == ec::error::invalid_arguments;
}

} // namespace

TEST_CASE(
    "the editor opens a mod's mapgen, draws, undoes and saves it in the data style",
    "[editor_session][engine_client_component]") {
    const auto guard = screen_guard{};
    const auto file = mod_file{};
    auto step = 0;

    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto state = captured();
        const auto screen = ec::top_screen(state);
        REQUIRE(screen.kind == "editor");
        switch (step++) {
            case 0:
                CHECK(screen.role == "screen");
                // Nothing is open: only opening and leaving are offered.
                CHECK(affordance(screen, "editor:open").args.text);
                CHECK(affordance(screen, "QUIT").role == "cancel");
                CHECK_FALSE(has_affordance(screen, "editor:paint"));
                CHECK_FALSE(has_affordance(screen, "editor:save"));
                return invoke("editor:open", {.text = "/etc/hostname.json"});
            case 1:
                // A file outside the data and the mods is refused, with the reason.
                CHECK(body<ec::text_body>(part(screen, "editor:notice"))
                          .markup.find("/etc/hostname.json")
                      != std::string::npos);
                CHECK_FALSE(has_affordance(screen, "editor:paint"));
                return invoke("editor:open", {.text = file.path.string()});
            case 2: {
                CHECK_FALSE(ec::find_component(*screen.root, "editor:notice"));
                const auto& objects = body<ec::list_body>(part(screen, "editor:objects")).rows;
                REQUIRE(objects.size() == 1);
                CHECK(objects[0].label == "field");
                CHECK(objects[0].selected);
                const auto& canvas = body<ec::grid_body>(part(screen, "editor:canvas"));
                CHECK(canvas.cols == 24);
                CHECK(canvas.rows == 24);
                CHECK(canvas.cells.size() == 576);
                CHECK(cell(screen, 2, 3).id == "square:2,3");
                CHECK(cell(screen, 2, 3).affordance == "editor:paint");
                // The legend resolves each symbol as the loader does, the first one picked.
                const auto& legend = body<ec::list_body>(part(screen, "editor:legend")).rows;
                REQUIRE(legend.size() == 2);
                CHECK(legend[0].id == "symbol:#");
                CHECK(legend[0].columns.front().value == "\"t_wall\"");
                CHECK(legend[0].selected);
                CHECK(body<ec::list_body>(part(screen, "editor:diagnostics")).rows.empty());
                const auto& tools = body<ec::tabs_body>(part(screen, "editor:tools")).tabs;
                REQUIRE(tools.size() == 7);
                CHECK(tools[0].id == "tool:point");
                CHECK(tools[0].selected);
                CHECK(saved(screen) == "Yes");
                CHECK_FALSE(affordance(screen, "editor:save").enabled);
                CHECK_FALSE(affordance(screen, "editor:undo").enabled);
                // Squares, symbols and tools are named by identity.
                CHECK(refused(state, "editor:paint", {.row = "square:24,0"}));
                CHECK(refused(state, "editor:symbol", {.row = "symbol:x"}));
                CHECK(refused(state, "editor:tool", {.row = "tool:spray"}));
                return invoke("editor:paint", {.row = "square:2,3"});
            }
            case 3:
                CHECK(cell(screen, 2, 3).glyph == "#");
                CHECK(walls(screen) == 1);
                CHECK(saved(screen) == "No");
                CHECK(affordance(screen, "editor:undo").enabled);
                CHECK(affordance(screen, "editor:save").enabled);
                return invoke("editor:tool", {.row = "tool:rect_filled"});
            case 4:
                CHECK(body<ec::tabs_body>(part(screen, "editor:tools")).tabs[3].selected);
                return invoke("editor:paint", {.row = "square:5,5"});
            case 5:
                // The first square of a rectangle is its anchor and draws nothing yet.
                CHECK(cell(screen, 5, 5).selected);
                CHECK(body<ec::grid_body>(part(screen, "editor:canvas")).cursor
                      == ec::grid_point{5, 5});
                CHECK(walls(screen) == 1);
                return invoke("editor:paint", {.row = "square:6,6"});
            case 6:
                CHECK(walls(screen) == 5);
                CHECK_FALSE(cell(screen, 5, 5).selected);
                return invoke("editor:undo");
            case 7:
                // The rectangle was one step.
                CHECK(walls(screen) == 1);
                CHECK(affordance(screen, "editor:redo").enabled);
                return invoke("editor:symbol", {.row = "symbol:."});
            case 8:
                CHECK(body<ec::panel_body>(part(screen, "editor:document")).stats[1].value == ".");
                return invoke("editor:save");
            case 9:
                CHECK(saved(screen) == "Yes");
                return act("QUIT");
            default:
                FAIL("the editor outlived its exit");
                return act("QUIT");
        }
    });

    editor::run_editor_session();
    CHECK(step == 10);
    const auto text = read_file(file.path);
    CHECK(format_json(text) == text);
    const auto rows = nlohmann::ordered_json::parse(text)[0]["object"]["rows"];
    CHECK(rows[3].get<std::string>() == "..#" + std::string(21, '.'));
    CHECK(rows[5].get<std::string>() == std::string(24, '.'));
}

TEST_CASE(
    "the editor asks before leaving unsaved changes and keeps the file when told to discard them",
    "[editor_session][engine_client_component]") {
    const auto guard = screen_guard{};
    const auto file = mod_file{};
    auto step = 0;

    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        switch (step++) {
            case 0:
                return invoke("editor:open", {.text = file.path.string()});
            case 1:
                return invoke("editor:paint", {.row = "square:0,0"});
            case 2:
                return act("QUIT");
            case 3:
                return answer("No");
            case 4:
                // Still in the editor, with the change.
                CHECK(cell(ec::top_screen(captured()), 0, 0).glyph == "#");
                return act("QUIT");
            case 5:
                return answer("Yes");
            default:
                FAIL("the editor outlived its exit");
                return act("QUIT");
        }
    });

    editor::run_editor_session();
    CHECK(step == 6);
    CHECK(read_file(file.path) == file.text);
}

#endif // CATA_MCP
