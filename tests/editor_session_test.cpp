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
#    include "editor/editor_session.h"
#    include "engine_client_component.h"
#    include "engine_client_contract.h"
#    include "game.h"
#    include "game_constants.h"
#    include "gamemode.h"
#    include "input.h"
#    include "json_formatter.h"
#    include "map/map.h"
#    include "options_helpers.h"
#    include "output.h"
#    include "path_info.h"
#    include "point.h"
#    include "state_helpers.h"
#    include "type_id.h"

#    include <algorithm>
#    include <filesystem>
#    include <fstream>
#    include <memory>
#    include <nlohmann/json.hpp>
#    include <ranges>
#    include <sstream>
#    include <string>
#    include <utility>
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

    explicit mod_file(const nlohmann::ordered_json& extra = nlohmann::ordered_json::object()) {
        auto rows = nlohmann::ordered_json::array();
        for (auto row = 0; row < 24; ++row) { rows.push_back(std::string(24, '.')); }
        const auto entry = nlohmann::ordered_json::array(
            {{{"type", "mapgen"},
              {"method", "json"},
              {"om_terrain", "house_04"},
              {"object", {{"rows", rows}, {"terrain", {{".", "t_floor"}, {"#", "t_wall"}}}}}}});
        auto with = entry;
        with[0]["object"].update(extra);
        text = format_json(with.dump()).value();
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
                CHECK(screen.title == "Map Editor");
                // Nothing is open: only opening and leaving are offered.
                CHECK(affordance(screen, "editor:open").args.text->field_id == "path");
                CHECK(
                    body<ec::form_body>(part(screen, "editor:file")).fields.front().value.empty());
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
                CHECK(body<ec::form_body>(part(screen, "editor:file")).fields.front().value
                      == file.path.generic_string());
                const auto& objects = body<ec::list_body>(part(screen, "editor:objects")).rows;
                REQUIRE(objects.size() == 1);
                CHECK(objects[0].label == "house_04");
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
            case 3: {
                CHECK(cell(screen, 2, 3).glyph == "#");
                CHECK(walls(screen) == 1);
                // A client that draws sprites reads what each square shows from the grid's looks.
                const auto& canvas = body<ec::grid_body>(part(screen, "editor:canvas"));
                REQUIRE(cell(screen, 2, 3).looks.size() == 1);
                CHECK(canvas.looks.at(cell(screen, 2, 3).looks.front()).id == "t_wall");
                CHECK(canvas.looks.at(cell(screen, 2, 3).looks.front()).kind == "terrain");
                CHECK(canvas.looks.size() == 2);
                CHECK(saved(screen) == "No");
                CHECK(affordance(screen, "editor:undo").enabled);
                CHECK(affordance(screen, "editor:save").enabled);
                return invoke("editor:tool", {.row = "tool:rect_filled"});
            }
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
    "the editor asks to save, discard or stay before leaving unsaved changes",
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
                return answer("Stay");
            case 4:
                // Still in the editor, with the change.
                CHECK(cell(ec::top_screen(captured()), 0, 0).glyph == "#");
                return act("QUIT");
            case 5:
                return answer("Leave without saving");
            default:
                FAIL("the editor outlived its exit");
                return act("QUIT");
        }
    });

    editor::run_editor_session();
    CHECK(step == 6);
    CHECK(read_file(file.path) == file.text);

    // Saving on the way out writes the change, unless the file changed on disk meanwhile.
    step = 0;
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        switch (step++) {
            case 0:
                return invoke("editor:open", {.text = file.path.string()});
            case 1:
                return invoke("editor:paint", {.row = "square:0,0"});
            case 2: {
                auto out = std::ofstream(file.path, std::ios::binary | std::ios::trunc);
                out << file.text << "\n";
                return act("QUIT");
            }
            case 3:
                return answer("Save and leave");
            case 4:
                CHECK(body<ec::text_body>(part(ec::top_screen(captured()), "editor:notice")).markup
                      == "The file changed on disk since it was opened; nothing was saved.");
                {
                    auto out = std::ofstream(file.path, std::ios::binary | std::ios::trunc);
                    out << file.text;
                }
                return act("QUIT");
            case 5:
                return answer("Save and leave");
            default:
                FAIL("the editor outlived its exit");
                return act("QUIT");
        }
    });

    editor::run_editor_session();
    CHECK(step == 6);
    CHECK(read_file(file.path).find("\"#") != std::string::npos);
}

TEST_CASE(
    "the editor previews the entry with a seed and keeps the preview in step with each drawing",
    "[editor_session][engine_client_component]") {
    const auto guard = screen_guard{};
    const auto file = mod_file{};
    const auto restore_mapgen = restore_on_out_of_scope<bool>(disable_mapgen);
    disable_mapgen = false;
    auto step = 0;
    const auto preview = [](const ec::screen_view& screen) -> const ec::grid_body& {
        return body<ec::grid_body>(part(screen, "editor:preview"));
    };
    const auto square = [&](const ec::screen_view& screen, const int x, const int y) {
        const auto& cells = preview(screen).cells;
        const auto found = std::ranges::find(cells, ec::grid_point{x, y}, &ec::grid_cell::at);
        REQUIRE(found != cells.end());
        return *found;
    };

    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto state = captured();
        const auto screen = ec::top_screen(state);
        switch (step++) {
            case 0:
                return invoke("editor:open", {.text = file.path.string()});
            case 1:
                CHECK_FALSE(ec::find_component(*screen.root, "editor:preview"));
                CHECK(affordance(screen, "editor:preview").args.count->initial == 1);
                CHECK(refused(state, "editor:preview", {.count = 0}));
                return invoke("editor:preview", {.count = 7});
            case 2:
                // The game's own generation: the floor the rows name, with its name.
                CHECK(preview(screen).cols == 24);
                CHECK(preview(screen).rows == 24);
                CHECK(square(screen, 3, 4).label == "wooden floor");
                REQUIRE_FALSE(square(screen, 3, 4).looks.empty());
                CHECK(preview(screen).looks.at(square(screen, 3, 4).looks.front()).id == "t_floor");
                CHECK(body<ec::panel_body>(part(screen, "editor:document")).stats.back().value
                      == "7");
                CHECK(affordance(screen, "editor:preview").args.count->initial == 7);
                return invoke("editor:paint", {.row = "square:3,4"});
            case 3:
                CHECK(square(screen, 3, 4).label != "wooden floor");
                CHECK(square(screen, 3, 5).label == "wooden floor");
                return invoke("editor:undo");
            case 4:
                CHECK(square(screen, 3, 4).label == "wooden floor");
                return act("QUIT");
            default:
                FAIL("the editor outlived its exit");
                return act("QUIT");
        }
    });

    editor::run_editor_session();
    CHECK(step == 5);
}

TEST_CASE(
    "the editor's legend form adds, changes and removes a mapping as undo steps",
    "[editor_session][engine_client_component]") {
    const auto guard = screen_guard{};
    const auto file = mod_file{};
    auto step = 0;
    const auto legend = [](const ec::screen_view& screen) {
        return body<ec::list_body>(part(screen, "editor:legend")).rows
             | std::views::transform(&ec::row::id) | std::ranges::to<std::vector>();
    };
    const auto field = [](const ec::screen_view& screen, const std::string& id) {
        const auto& fields = body<ec::form_body>(part(screen, "editor:mapping")).fields;
        const auto found = std::ranges::find(fields, id, &ec::form_field::id);
        REQUIRE(found != fields.end());
        return *found;
    };
    const auto set = [](const std::string& id, const std::string& text) {
        return invoke("editor:mapping_set", {.row = id, .text = text});
    };

    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto state = captured();
        const auto screen = ec::top_screen(state);
        switch (step++) {
            case 0:
                return invoke("editor:open", {.text = file.path.string()});
            case 1:
                // The form starts on the symbol drawn with, on terrain.
                CHECK(field(screen, "symbol").value == "#");
                CHECK(field(screen, "layer").value == "terrain");
                CHECK(field(screen, "layer").type == "select");
                CHECK(std::ranges::contains(
                    field(screen, "layer").options, std::string("furniture"),
                    &ec::form_option::id));
                CHECK(refused(state, "editor:mapping_set", {.row = "colour", .text = "x"}));
                return set("symbol", "x");
            case 2:
                return set("value", "t_dirt");
            case 3:
                CHECK(field(screen, "value").value == "t_dirt");
                return invoke("editor:mapping_apply");
            case 4:
                // A bare id is written as a string.
                CHECK(
                    legend(screen) == std::vector<std::string>{"symbol:#", "symbol:.", "symbol:x"});
                CHECK(saved(screen) == "No");
                return set("value", R"(["t_dirt", "t_grass"])");
            case 5:
                return invoke("editor:mapping_apply");
            case 6: {
                const auto& rows = body<ec::list_body>(part(screen, "editor:legend")).rows;
                CHECK(rows[2].columns.front().value == R"(["t_dirt","t_grass"])");
                return set("layer", "terrains");
            }
            case 7:
                return invoke("editor:mapping_apply");
            case 8:
                // A member the loader does not read is refused before it is written.
                CHECK(body<ec::text_body>(part(screen, "editor:notice")).markup
                      == "terrains maps no symbols.");
                return set("layer", "terrain");
            case 9:
                return set("value", "");
            case 10:
                return invoke("editor:mapping_apply");
            case 11:
                CHECK(legend(screen) == std::vector<std::string>{"symbol:#", "symbol:."});
                return invoke("editor:undo");
            case 12:
                CHECK(legend(screen).size() == 3);
                return invoke("editor:undo");
            case 13:
                return invoke("editor:undo");
            case 14:
                CHECK(saved(screen) == "Yes");
                return act("QUIT");
            default:
                FAIL("the editor outlived its exit");
                return act("QUIT");
        }
    });

    editor::run_editor_session();
    CHECK(step == 15);
    CHECK(read_file(file.path) == file.text);
}

TEST_CASE(
    "the editor lists placements and shows the square a placement or a diagnostic names",
    "[editor_session][engine_client_component]") {
    const auto guard = screen_guard{};
    const auto file = mod_file(nlohmann::ordered_json::parse(R"({"place_monster": [
        {"monster": "mon_zombie", "x": [3, 5], "y": 7},
        {"monster": "mon_zombie", "x": 30, "y": 2}]})"));
    auto step = 0;
    const auto cursor = [](const ec::screen_view& screen) {
        return body<ec::grid_body>(part(screen, "editor:canvas")).cursor;
    };

    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto state = captured();
        const auto screen = ec::top_screen(state);
        switch (step++) {
            case 0:
                return invoke("editor:open", {.text = file.path.string()});
            case 1: {
                const auto& placed = body<ec::list_body>(part(screen, "editor:placements")).rows;
                REQUIRE(placed.size() == 2);
                CHECK(placed[0].id == "placement:place_monster:0");
                CHECK(placed[0].columns[0].value == "3-5");
                CHECK(placed[0].columns[1].value == "7");
                CHECK(placed[0].columns[2].value == R"({"monster":"mon_zombie"})");
                // The loader drops the second: it names it, and it can be shown.
                const auto& messages = body<ec::list_body>(part(screen, "editor:diagnostics")).rows;
                REQUIRE(messages.size() == 1);
                CHECK(messages[0].description.starts_with("/object/place_monster/1"));
                CHECK(messages[0].selectable);
                CHECK_FALSE(cursor(screen));
                CHECK(refused(state, "editor:placement", {.row = "placement:place_items:0"}));
                return invoke("editor:placement", {.row = "placement:place_monster:0"});
            }
            case 2:
                CHECK(cursor(screen) == ec::grid_point{3, 7});
                CHECK(screen.focus_row == "square:3,7");
                return invoke("editor:diagnostic", {.row = "message:0"});
            case 3:
                // A square outside the rows is shown at the nearest edge.
                CHECK(cursor(screen) == ec::grid_point{23, 2});
                return act("QUIT");
            default:
                FAIL("the editor outlived its exit");
                return act("QUIT");
        }
    });

    editor::run_editor_session();
    CHECK(step == 4);
}

TEST_CASE(
    "the editor's catalog finds what a legend can name and puts it in the legend form",
    "[editor_session][engine_client_component]") {
    const auto guard = screen_guard{};
    const auto file = mod_file{};
    auto step = 0;
    const auto field = [](const ec::screen_view& screen, const std::string& id) {
        const auto& fields = body<ec::form_body>(part(screen, "editor:mapping")).fields;
        const auto found = std::ranges::find(fields, id, &ec::form_field::id);
        REQUIRE(found != fields.end());
        return found->value;
    };
    const auto entries = [](const ec::screen_view& screen) -> const ec::list_body& {
        return body<ec::list_body>(part(screen, "editor:catalog"));
    };

    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto screen = ec::top_screen(captured());
        switch (step++) {
            case 0:
                return invoke("editor:open", {.text = file.path.string()});
            case 1: {
                // Terrain first, a page at a time, by id.
                const auto& kinds = body<ec::tabs_body>(part(screen, "editor:catalog_kinds")).tabs;
                REQUIRE(kinds.size() == 11);
                CHECK(kinds[0].id == "catalog:terrain");
                CHECK(kinds[0].selected);
                CHECK(entries(screen).rows.size() == 50);
                CHECK(entries(screen).total > 50);
                CHECK(entries(screen).filterable);
                CHECK(affordance(screen, "editor:catalog_page").args.count->max > 0);
                return invoke("editor:catalog_filter", {.text = "t_dirt"});
            }
            case 2: {
                const auto& rows = entries(screen).rows;
                CHECK(std::ranges::contains(rows, std::string("entry:t_dirt"), &ec::row::id));
                CHECK(std::ranges::all_of(rows, [](const ec::row& entry) {
                    return entry.columns[0].value.find("dirt") != std::string::npos
                        || entry.label.find("dirt") != std::string::npos;
                }));
                return invoke("editor:catalog_pick", {.row = "entry:t_dirt"});
            }
            case 3:
                CHECK(field(screen, "layer") == "terrain");
                CHECK(field(screen, "value") == "t_dirt");
                return invoke("editor:catalog_kind", {.row = "catalog:monster_group"});
            case 4:
                // A new kind starts on its first page, with the filter kept.
                CHECK(entries(screen).offset == 0);
                return invoke("editor:catalog_filter", {.text = "GROUP_ZOMBIE"});
            case 5:
                return invoke("editor:catalog_pick", {.row = "entry:GROUP_ZOMBIE"});
            case 6:
                // A group is named inside the mapping its layer reads.
                CHECK(field(screen, "layer") == "monsters");
                CHECK(field(screen, "value") == R"({"monster":"GROUP_ZOMBIE"})");
                return invoke("editor:catalog_kind", {.row = "catalog:terrain"});
            case 7:
                return invoke("editor:catalog_filter", {.text = ""});
            case 8:
                return invoke("editor:catalog_page", {.count = 1});
            case 9:
                CHECK(entries(screen).offset == 50);
                return act("QUIT");
            default:
                FAIL("the editor outlived its exit");
                return act("QUIT");
        }
    });

    editor::run_editor_session();
    CHECK(step == 10);
}

TEST_CASE(
    "the editor moves a picked placement and sets any field the loader accepts",
    "[editor_session][engine_client_component]") {
    const auto guard = screen_guard{};
    const auto file = mod_file(nlohmann::ordered_json::parse(
        R"({"place_monster": [{"monster": "mon_zombie", "x": [3, 5], "y": 7}]})"));
    auto step = 0;
    const auto field =
        [](const ec::screen_view& screen, const std::string& form, const std::string& id) {
            const auto& fields = body<ec::form_body>(part(screen, form)).fields;
            const auto found = std::ranges::find(fields, id, &ec::form_field::id);
            REQUIRE(found != fields.end());
            return found->value;
        };
    const auto placed = [](const ec::screen_view& screen) {
        return body<ec::list_body>(part(screen, "editor:placements")).rows.front().columns;
    };

    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto screen = ec::top_screen(captured());
        switch (step++) {
            case 0:
                return invoke("editor:open", {.text = file.path.string()});
            case 1:
                // Nothing to move until a placement is picked.
                CHECK_FALSE(ec::find_component(*screen.root, "editor:move"));
                CHECK(field(screen, "editor:field", "pointer").empty());
                return invoke("editor:placement", {.row = "placement:place_monster:0"});
            case 2:
                CHECK(field(screen, "editor:move", "x") == "3-5");
                CHECK(field(screen, "editor:move", "y") == "7");
                CHECK(field(screen, "editor:field", "pointer") == "/object/place_monster/0");
                CHECK(field(screen, "editor:field", "value")
                      == R"({"monster":"mon_zombie","x":[3,5],"y":7})");
                return invoke("editor:move_set", {.row = "x", .text = "10"});
            case 3:
                return invoke("editor:move_apply");
            case 4:
                // One square is written as a number, and the cursor follows.
                CHECK(placed(screen)[0].value == "10");
                CHECK(body<ec::grid_body>(part(screen, "editor:canvas")).cursor
                      == ec::grid_point{10, 7});
                return invoke("editor:move_set", {.row = "y", .text = "9-2"});
            case 5:
                return invoke("editor:move_apply");
            case 6:
                CHECK(body<ec::text_body>(part(screen, "editor:notice")).markup
                      == "A place is a number or two joined by a dash, the smaller first.");
                return invoke("editor:move_set", {.row = "y", .text = "7x"});
            case 7:
                return invoke("editor:move_apply");
            case 8:
                CHECK(body<ec::text_body>(part(screen, "editor:notice")).markup
                      == "A place is a number or two joined by a dash, the smaller first.");
                CHECK(placed(screen)[1].value == "7");
                return invoke("editor:field_set",
                              {.row = "pointer", .text = "/object/place_monster/0/chance"});
            case 9:
                return invoke("editor:field_set", {.row = "value", .text = "50"});
            case 10:
                return invoke("editor:field_apply");
            case 11:
                CHECK(placed(screen)[2].value == R"({"monster":"mon_zombie","chance":50})");
                return invoke("editor:field_set", {.row = "value", .text = R"("fifty")"});
            case 12:
                return invoke("editor:field_apply");
            case 13:
                // What the loader refuses is not written.
                CHECK(ec::find_component(*screen.root, "editor:notice"));
                CHECK(placed(screen)[2].value == R"({"monster":"mon_zombie","chance":50})");
                return invoke("editor:save");
            case 14:
                return act("QUIT");
            default:
                FAIL("the editor outlived its exit");
                return act("QUIT");
        }
    });

    editor::run_editor_session();
    CHECK(step == 15);
    const auto saved = nlohmann::ordered_json::parse(read_file(file.path));
    CHECK(saved[0]["object"]["place_monster"][0]
          == nlohmann::ordered_json::parse(
              R"({"monster": "mon_zombie", "x": 10, "y": 7, "chance": 50})"));
}

TEST_CASE(
    "the editor reloads a file changed on disk unless it is edited, and then says so",
    "[editor_session][engine_client_component]") {
    const auto guard = screen_guard{};
    const auto file = mod_file{};
    const auto row = std::string(24, '.');
    // The file as another program writes it: its first row `with` instead.
    const auto write_outside = [&](const char with) {
        auto text = file.text;
        text.replace(text.find(row), row.size(), std::string(24, with));
        auto out = std::ofstream(file.path, std::ios::binary | std::ios::trunc);
        out << text;
    };
    const auto notice = [](const ec::screen_view& screen) {
        return body<ec::text_body>(part(screen, "editor:notice")).markup;
    };
    auto step = 0;

    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto screen = ec::top_screen(captured());
        switch (step++) {
            case 0:
                return invoke("editor:open", {.text = file.path.string()});
            case 1:
                CHECK(walls(screen) == 0);
                write_outside('#');
                return invoke("editor:symbol", {.row = "symbol:#"});
            case 2:
                CHECK(walls(screen) == 24);
                CHECK(notice(screen) == file.path.string() + " changed on disk and was reloaded.");
                CHECK(saved(screen) == "Yes");
                return invoke("editor:paint", {.row = "square:5,5"});
            case 3:
                CHECK(walls(screen) == 25);
                CHECK_FALSE(ec::find_component(*screen.root, "editor:notice"));
                write_outside('.');
                return invoke("editor:symbol", {.row = "symbol:#"});
            case 4:
                // An edited document keeps its edit, and the change on disk is not overwritten
                // unasked.
                CHECK(walls(screen) == 25);
                CHECK(
                    notice(screen)
                    == file.path.string() + " changed on disk; saving would overwrite that change.");
                return invoke("editor:save");
            case 5:
                CHECK(saved(screen) == "No");
                return act("QUIT");
            case 6:
                return answer("Leave without saving");
            default:
                FAIL("the editor outlived its exit");
                return act("QUIT");
        }
    });

    editor::run_editor_session();
    CHECK(step == 7);
    CHECK(read_file(file.path) == file.text);
}

TEST_CASE(
    "the editor creates a mapgen file in the user mods and opens it",
    "[editor_session][engine_client_component]") {
    const auto guard = screen_guard{};
    // Removes the folder the new file goes in.
    const auto folder = mod_file{};
    const auto made = folder.path.parent_path() / "new.json";
    const auto notice = [](const ec::screen_view& screen) {
        return body<ec::text_body>(part(screen, "editor:notice")).markup;
    };
    auto step = 0;

    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto screen = ec::top_screen(captured());
        switch (step++) {
            case 0:
                CHECK(affordance(screen, "editor:new_apply").enabled);
                return invoke("editor:new_set", {.row = "path", .text = "/tmp/new.json"});
            case 1:
                return invoke("editor:new_set", {.row = "om_terrain", .text = "house_04"});
            case 2:
                return invoke("editor:new_apply");
            case 3:
                // Only the user mods take a new file.
                CHECK(notice(screen).starts_with("A new file goes in your mods"));
                return invoke("editor:new_set", {.row = "path", .text = made.string()});
            case 4:
                return invoke("editor:new_set", {.row = "fill", .text = "t_no_such_terrain"});
            case 5:
                return invoke("editor:new_apply");
            case 6:
                CHECK(notice(screen) == "t_no_such_terrain is no terrain.");
                CHECK_FALSE(std::filesystem::exists(made));
                return invoke("editor:new_set", {.row = "fill", .text = "t_dirt"});
            case 7:
                return invoke("editor:new_apply");
            case 8: {
                CHECK_FALSE(ec::find_component(*screen.root, "editor:notice"));
                CHECK(body<ec::form_body>(part(screen, "editor:file")).fields.front().value
                      == made.generic_string());
                const auto& objects = body<ec::list_body>(part(screen, "editor:objects")).rows;
                REQUIRE(objects.size() == 1);
                CHECK(objects[0].label == "house_04");
                CHECK(body<ec::grid_body>(part(screen, "editor:canvas")).cells.size() == 576);
                CHECK(cell(screen, 23, 23).glyph == ".");
                const auto& legend = body<ec::list_body>(part(screen, "editor:legend")).rows;
                REQUIRE(legend.size() == 1);
                CHECK(legend[0].columns.front().value == "\"t_dirt\"");
                CHECK(body<ec::list_body>(part(screen, "editor:diagnostics")).rows.empty());
                CHECK(saved(screen) == "Yes");
                return invoke("editor:new_apply");
            }
            case 9:
                CHECK(notice(screen) == made.string() + " already exists.");
                return act("QUIT");
            default:
                FAIL("the editor outlived its exit");
                return act("QUIT");
        }
    });

    editor::run_editor_session();
    CHECK(step == 10);
    const auto written = nlohmann::ordered_json::parse(read_file(made));
    CHECK(written[0]["om_terrain"] == "house_04");
    CHECK(written[0]["object"]["terrain"]["."] == "t_dirt");
}

TEST_CASE(
    "the editor saves a document as a new file in the user mods, which it edits from then on",
    "[editor_session][engine_client_component]") {
    const auto guard = screen_guard{};
    const auto file = mod_file{};
    const auto copy = file.path.parent_path() / "copy.json";
    const auto notice = [](const ec::screen_view& screen) {
        return body<ec::text_body>(part(screen, "editor:notice")).markup;
    };
    auto step = 0;

    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto screen = ec::top_screen(captured());
        switch (step++) {
            case 0:
                CHECK_FALSE(has_affordance(screen, "editor:save_as"));
                return invoke("editor:open", {.text = file.path.string()});
            case 1:
                CHECK(affordance(screen, "editor:save_as").args.text->field_id == "path");
                return invoke("editor:paint", {.row = "square:0,0"});
            case 2:
                return invoke("editor:save_as", {.text = "/tmp/editor_session_copy.json"});
            case 3:
                CHECK(notice(screen).starts_with("Save as writes a .json file in your mods"));
                CHECK(saved(screen) == "No");
                return invoke("editor:save_as", {.text = copy.string()});
            case 4:
                CHECK_FALSE(ec::find_component(*screen.root, "editor:notice"));
                CHECK(saved(screen) == "Yes");
                CHECK(body<ec::form_body>(part(screen, "editor:file")).fields.front().value
                      == copy.generic_string());
                return invoke("editor:save_as", {.text = file.path.string()});
            case 5:
                CHECK(notice(screen) == "That file already exists; nothing was saved.");
                return invoke("editor:paint", {.row = "square:1,0"});
            case 6:
                return invoke("editor:save");
            case 7:
                CHECK(saved(screen) == "Yes");
                return act("QUIT");
            default:
                FAIL("the editor outlived its exit");
                return act("QUIT");
        }
    });

    editor::run_editor_session();
    CHECK(step == 8);
    // The file it came from is untouched; the copy has both strokes.
    CHECK(read_file(file.path) == file.text);
    const auto rows = nlohmann::ordered_json::parse(read_file(copy))[0]["object"]["rows"];
    CHECK(rows[0].get<std::string>().starts_with("##."));
}

TEST_CASE(
    "the editor comes back to the document and view it was left with",
    "[editor_session][engine_client_component]") {
    const auto guard = screen_guard{};
    const auto file = mod_file{};
    auto step = 0;

    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        switch (step++) {
            case 0:
                return invoke("editor:open", {.text = file.path.string()});
            case 1:
                return invoke("editor:tool", {.row = "tool:rect"});
            case 2:
                return invoke("editor:paint", {.row = "square:2,3"});
            default:
                return act("QUIT");
        }
    });
    editor::run_editor_session();
    REQUIRE(step == 4);

    step = 0;
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto screen = ec::top_screen(captured());
        switch (step++) {
            case 0:
                CHECK(body<ec::form_body>(part(screen, "editor:file")).fields.front().value
                      == file.path.generic_string());
                CHECK(body<ec::grid_body>(part(screen, "editor:canvas")).cursor
                      == ec::grid_point{2, 3});
                // The rectangle tool is still picked: the second corner draws it.
                return invoke("editor:paint", {.row = "square:5,5"});
            case 1:
                CHECK(cell(screen, 5, 5).glyph == "#");
                return act("QUIT");
            case 2:
                return answer("Leave without saving");
            default:
                FAIL("the editor outlived its exit");
                return act("QUIT");
        }
    });
    editor::run_editor_session();
    CHECK(step == 3);

    // Leaving without saving put the file back as it is on disk; the file stays open.
    step = 0;
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto screen = ec::top_screen(captured());
        switch (step++) {
            case 0:
                CHECK(body<ec::form_body>(part(screen, "editor:file")).fields.front().value
                      == file.path.generic_string());
                CHECK(saved(screen) == "Yes");
                CHECK(cell(screen, 5, 5).glyph == ".");
                return act("QUIT");
            default:
                FAIL("the editor outlived its exit");
                return act("QUIT");
        }
    });
    editor::run_editor_session();
    CHECK(step == 1);
    CHECK(read_file(file.path) == file.text);
}

namespace {

/// `game::handle_action` and `game::gamemode` are private; the explicit instantiation reaches them
/// without changing the game's headers, as client_interaction_test.cpp does.
struct handle_action_member {
    using type = auto (game::*)() -> bool;
    friend auto native_member(handle_action_member) -> type;
};
template <typename Tag, typename Tag::type Member> struct native_test_member {
    friend auto native_member(Tag /*tag*/) -> typename Tag::type { return Member; }
};
struct mode_member {
    using type = std::unique_ptr<special_game> game::*;
    friend auto native_member(mode_member) -> type;
};
template struct native_test_member<handle_action_member, &game::handle_action>;
template struct native_test_member<mode_member, &game::gamemode>;

auto game_mode() -> std::unique_ptr<special_game>& {
    return g.get()->*native_member(mode_member{});
} // *NOPAD*

/// One turn of the native loop's input handling under the game mode given.
auto handle_action(std::unique_ptr<special_game> mode) -> bool {
    auto& current = game_mode();
    auto kept = std::exchange(current, std::move(mode));
    const auto restore = on_out_of_scope([&] { current = std::move(kept); });
    return (g.get()->*native_member(handle_action_member{}))();
}

} // namespace

TEST_CASE("game time does not pass while the map editor is open", "[editor_session]") {
    const auto guard = screen_guard{};
    const auto file = mod_file{};
    const auto animations = override_option("ANIMATIONS", "false");
    auto& you = get_avatar();
    you.set_mutation(trait_id("DEBUG_EDITOR"));
    you.set_moves(100);
    const auto before = calendar::turn;
    auto step = 0;

    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto screen = ec::top_screen(captured());
        switch (step++) {
            case 0:
                CHECK(screen.role == "map");
                return act("editor");
            case 1:
                CHECK(screen.kind == "editor");
                return invoke("editor:open", {.text = file.path.string()});
            case 2:
                return invoke("editor:paint", {.row = "square:0,0"});
            case 3:
                return invoke("editor:save");
            case 4:
                return act("QUIT");
            default:
                FAIL("the editor outlived its exit");
                return act("QUIT");
        }
    });
    handle_action(std::make_unique<special_game>());
    CHECK(step == 5);
    CHECK(calendar::turn == before);
    CHECK(you.get_moves() == 100);
}

#endif // CATA_MCP
