#if defined(CATA_MCP)

#    include "avatar.h"
#    include "cata_utility.h"
#    include "catch/catch.hpp"
#    include "client_command.h"
#    include "client_input.h"
#    include "client_interaction.h"
#    include "client_memory.h"
#    include "client_memory_scope.h"
#    include "cursesdef.h"
#    include "cursesport.h"
#    include "debug_menu.h"
#    include "editor/editor_session.h"
#    include "editor/map_session.h"
#    include "editor/mapgen_levels.h"
#    include "editor/mapgen_preview.h"
#    include "editor/mapgen_source.h"
#    include "editor/recent_documents.h"
#    include "editor/special_grid.h"
#    include "engine_client_component.h"
#    include "engine_client_contract.h"
#    include "game.h"
#    include "game_constants.h"
#    include "gamemode.h"
#    include "input.h"
#    include "json_formatter.h"
#    include "map/map.h"
#    include "map/mapbuffer.h"
#    include "map/mapdata.h"
#    include "mapgen/mapgen_constructor.h"
#    include "mapgen/mapgen_functions.h"
#    include "options_helpers.h"
#    include "output.h"
#    include "overmap/omdata.h"
#    include "overmap/overmap_special.h"
#    include "overmap/overmapbuffer.h"
#    include "overmap/overmapbuffer_registry.h"
#    include "path_info.h"
#    include "player_activity.h"
#    include "point.h"
#    include "state_helpers.h"
#    include "translations.h"
#    include "type_id.h"
#    include "world.h"
#    include "worldfactory.h"

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
        // Each case starts the editor afresh, as a new game does.
        editor::reset_editor_session();
        test_mode = false;
        TERMX = 100;
        TERMY = 40;
        game_client::memory::resize(TERMX, TERMY);
        catacurses::stdscr = catacurses::newwin(TERMY, TERMX, point_zero);
        catacurses::newscr = catacurses::newwin(TERMY, TERMX, point_zero);
    }

    ~screen_guard() {
        game_client::memory::set_input_provider({});
        editor::reset_editor_session();
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
    std::filesystem::path path;
    std::string text;

    explicit mod_file(
        const nlohmann::ordered_json& extra = nlohmann::ordered_json::object(),
        const std::string& name = "floor.json")
        : path(std::filesystem::path(PATH_INFO::user_moddir()) / "editor_session_test" / name) {
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
/// The component `id` somewhere in `value`, null when it is not there.
auto find_wire(const nlohmann::json& value, const std::string& id) -> nlohmann::json {
    if (value.is_object() && value.contains("kind") && value.contains("id")
        && value["id"].is_string() && value["id"].get<std::string>() == id) {
        return value;
    }
    if (!value.is_structured()) { return nullptr; }
    for (const auto& child : value) {
        if (auto found = find_wire(child, id); !found.is_null()) { return found; }
    }
    return nullptr;
}
/// The component `id` as the boundary's wire carries it, null when it is not there.
auto wire_part(const ec::boundary_state& state, const std::string& id) -> nlohmann::json {
    return find_wire(nlohmann::json::parse(ec::serialize_interaction_boundary(state))["stack"], id);
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
/// The value of the row's column `key`, empty when it has none.
auto column_value(const ec::row& row, const std::string& key) -> std::string {
    const auto found = std::ranges::find(row.columns, key, &game_client::interaction_column::key);
    return found == row.columns.end() ? std::string{} : found->value;
}
/// What a legend row places first, by name.
auto placed_value(const ec::row& row) -> std::string {
    const auto found = std::ranges::find_if(row.columns, [](const auto& column) {
        return column.key != "use" && column.key != "id" && !column.value.empty();
    });
    return found == row.columns.end() ? std::string{} : found->value;
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
                CHECK(screen.role == "workspace");
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
                // The canvas says it is one, so a client gives it the room a drawing needs.
                CHECK(wire_part(state, "editor:canvas").value("role", "") == "canvas");
                // The legend resolves each symbol as the loader does, the first one picked.
                const auto& legend = body<ec::list_body>(part(screen, "editor:legend")).rows;
                REQUIRE(legend.size() == 2);
                CHECK(legend[0].id == "symbol:#");
                CHECK(column_value(legend[0], "terrain") == "plastered wooden wall");
                CHECK(legend[0].selected);
                CHECK(body<ec::list_body>(part(screen, "editor:diagnostics")).rows.empty());
                const auto& tools = body<ec::tabs_body>(part(screen, "editor:tools")).tabs;
                REQUIRE(tools.size() == 11);
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
                CHECK(body<ec::panel_body>(part(screen, "editor:document")).stats[1].value
                      == ". wooden floor");
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
    "the canvas drags as a trail for the pencil, as a span for shapes and not for the fill",
    "[editor_session][engine_client_component]") {
    const auto guard = screen_guard{};
    const auto file = mod_file{};
    auto step = 0;

    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto state = captured();
        const auto screen = ec::top_screen(state);
        const auto drag = [&] { return wire_part(state, "editor:canvas").value("drag", ""); };
        switch (step++) {
            case 0:
                return invoke("editor:open", {.text = file.path.string()});
            case 1:
                CHECK(drag() == "trail");
                CHECK(wire_part(state, "editor:canvas").value("drag_begin", "")
                      == "editor:stroke_begin");
                CHECK(
                    wire_part(state, "editor:canvas").value("drag_end", "") == "editor:stroke_end");
                return invoke("editor:tool", {.row = "tool:rect"});
            case 2:
                CHECK(drag() == "span");
                CHECK_FALSE(wire_part(state, "editor:canvas").contains("drag_begin"));
                // A stroke is offered only where the canvas names it.
                CHECK(std::ranges::none_of(screen.affordances, [](const auto& entry) {
                    return entry.id == "editor:stroke_begin" || entry.id == "editor:stroke_end";
                }));
                return invoke("editor:tool", {.row = "tool:fill"});
            case 3:
                CHECK(drag().empty());
                return invoke("editor:tool", {.row = "tool:pick"});
            case 4:
                CHECK(drag().empty());
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
    "a stroke begun with the pencil can end after the tool changed",
    "[editor_session][engine_client_component]") {
    const auto guard = screen_guard{};
    const auto file = mod_file{};
    auto step = 0;

    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto state = captured();
        const auto screen = ec::top_screen(state);
        switch (step++) {
            case 0:
                return invoke("editor:open", {.text = file.path.string()});
            case 1:
                return invoke("editor:stroke_begin");
            case 2:
                return invoke("editor:tool", {.row = "tool:rect"});
            case 3:
                // A hotkey mid-drag must not leave the stroke open, folding later edits into it.
                CHECK(affordance(screen, "editor:stroke_end").enabled);
                return invoke("editor:stroke_end");
            case 4:
                CHECK(std::ranges::none_of(screen.affordances, [](const auto& entry) {
                    return entry.id == "editor:stroke_end";
                }));
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
    "a pencil trail between stroke begin and end is one undo step",
    "[editor_session][engine_client_component]") {
    const auto guard = screen_guard{};
    const auto file = mod_file{};
    auto step = 0;

    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto state = captured();
        const auto screen = ec::top_screen(state);
        switch (step++) {
            case 0:
                return invoke("editor:open", {.text = file.path.string()});
            case 1:
                return invoke("editor:stroke_begin");
            case 2:
                return invoke("editor:paint", {.row = "square:1,1"});
            case 3:
                return invoke("editor:paint", {.row = "square:2,1"});
            case 4:
                return invoke("editor:paint", {.row = "square:3,1"});
            case 5:
                CHECK(walls(screen) == 3);
                return invoke("editor:stroke_end");
            case 6:
                // Outside a stroke each square is a step of its own.
                return invoke("editor:paint", {.row = "square:5,5"});
            case 7:
                return invoke("editor:paint", {.row = "square:6,5"});
            case 8:
                CHECK(walls(screen) == 5);
                return invoke("editor:undo");
            case 9:
                CHECK(walls(screen) == 4);
                return invoke("editor:undo");
            case 10:
                CHECK(walls(screen) == 3);
                return invoke("editor:undo");
            case 11:
                CHECK(walls(screen) == 0);
                CHECK_FALSE(affordance(screen, "editor:undo").enabled);
                return act("QUIT");
            default:
                FAIL("the editor outlived its exit");
                return act("QUIT");
        }
    });

    editor::run_editor_session();
    CHECK(step == 12);
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

namespace {

/// A mapgen whose floor is a parameter, with one wall at (3, 4) to tell its turns apart, and
/// groups of items at (3, 3).
auto previewed_extra() -> nlohmann::ordered_json {
    auto rows = nlohmann::ordered_json::array();
    for (auto row = 0; row < 24; ++row) {
        rows.push_back(row == 4 ? "...#" + std::string(20, '.') : std::string(24, '.'));
    }
    auto extra = nlohmann::ordered_json::parse(R"({
        "parameters": { "floor": { "type": "ter_str_id", "scope": "omt",
          "default": { "distribution": [ [ "t_floor", 1 ], [ "t_dirt", 1 ] ] } } },
        "terrain": { ".": { "param": "floor" }, "#": "t_wall" },
        "place_items": [ { "item": "hardware", "x": 3, "y": 3, "chance": 100, "repeat": 3 } ]
      })");
    extra["rows"] = rows;
    return extra;
}

auto preview_labels(const ec::screen_view& screen) -> std::vector<std::string> {
    return body<ec::grid_body>(part(screen, "editor:preview")).cells
         | std::views::transform(&ec::grid_cell::label) | std::ranges::to<std::vector>();
}

} // namespace

TEST_CASE(
    "the editor pins a mapgen parameter and the preview uses it",
    "[editor_session][engine_client_component]") {
    const auto guard = screen_guard{};
    const auto file = mod_file{previewed_extra()};
    const auto restore_mapgen = restore_on_out_of_scope<bool>(disable_mapgen);
    disable_mapgen = false;
    auto step = 0;
    const auto pinned = [](const ec::screen_view& screen) {
        const auto& fields = body<ec::form_body>(part(screen, "editor:pins")).fields;
        REQUIRE(fields.size() == 1);
        CHECK(fields.front().id == "floor");
        CHECK(fields.front().type == "select");
        return fields.front();
    };

    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto state = captured();
        const auto screen = ec::top_screen(state);
        switch (step++) {
            case 0:
                return invoke("editor:open", {.text = file.path.string()});
            case 1:
                // The parameters are offered once a preview is shown, with every value they take.
                CHECK_FALSE(ec::find_component(*screen.root, "editor:pins"));
                return invoke("editor:preview", {.count = 3});
            case 2: {
                const auto field = pinned(screen);
                CHECK(field.value.empty());
                CHECK(std::ranges::contains(field.options, "t_dirt", &ec::form_option::id));
                CHECK(std::ranges::contains(field.options, "t_floor", &ec::form_option::id));
                return invoke("editor:pin_set", {.row = "floor", .text = "t_dirt"});
            }
            case 3: {
                CHECK(pinned(screen).value == "t_dirt");
                const auto labels = preview_labels(screen);
                CHECK(std::ranges::contains(labels, "dirt"));
                CHECK_FALSE(std::ranges::contains(labels, "wooden floor"));
                return invoke("editor:pin_set", {.row = "floor", .text = "t_floor"});
            }
            case 4:
                CHECK(pinned(screen).value == "t_floor");
                CHECK_FALSE(std::ranges::contains(preview_labels(screen), "dirt"));
                // A parameter the mapgen lacks is refused with its name, and the pin stays.
                return invoke("editor:pin_set", {.row = "wall", .text = "t_wall"});
            case 5:
                CHECK(body<ec::text_body>(part(screen, "editor:notice")).markup.find("wall")
                      != std::string::npos);
                CHECK(pinned(screen).value == "t_floor");
                CHECK_FALSE(std::ranges::contains(preview_labels(screen), "dirt"));
                return invoke("editor:pin_set", {.row = "floor", .text = ""});
            case 6:
                CHECK(pinned(screen).value.empty());
                return act("QUIT");
            default:
                FAIL("the editor outlived its exit");
                return act("QUIT");
        }
    });

    editor::run_editor_session();
    CHECK(step == 7);
}

TEST_CASE(
    "the editor turns the preview to the mapgen's own rotation",
    "[editor_session][engine_client_component]") {
    const auto guard = screen_guard{};
    const auto file = mod_file{previewed_extra()};
    const auto restore_mapgen = restore_on_out_of_scope<bool>(disable_mapgen);
    disable_mapgen = false;
    auto step = 0;
    auto north = std::vector<std::string>{};
    const auto rotation = [](const ec::screen_view& screen) {
        const auto& rows = body<ec::list_body>(part(screen, "editor:rotation")).rows;
        CHECK((rows | std::views::transform(&ec::row::id) | std::ranges::to<std::vector>())
              == std::vector<std::string>{"turn:0", "turn:90", "turn:180", "turn:270"});
        return rows | std::views::filter(&ec::row::selected) | std::views::transform(&ec::row::id)
             | std::ranges::to<std::vector>();
    };
    // What the library generates, as the preview shows it.
    const auto generated = [&](const std::string& direction) {
        const auto made = editor::preview_mapgen(
            nlohmann::ordered_json::parse(file.text)[0].dump(),
            {.at = get_avatar().abs_omt_pos(),
             .seed = 3,
             .direction = direction,
             .pins = {{"floor", "t_floor"}}});
        REQUIRE(made);
        return made->squares | std::views::transform([](const editor::preview_square& square) {
                   return ter_str_id(square.ter).obj().name();
               })
             | std::ranges::to<std::vector>();
    };

    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto state = captured();
        const auto screen = ec::top_screen(state);
        switch (step++) {
            case 0:
                return invoke("editor:open", {.text = file.path.string()});
            case 1:
                return invoke("editor:preview", {.count = 3});
            case 2:
                return invoke("editor:pin_set", {.row = "floor", .text = "t_floor"});
            case 3:
                CHECK(rotation(screen) == std::vector<std::string>{"turn:0"});
                north = preview_labels(screen);
                CHECK(north == generated("north"));
                CHECK(north[4 * 24 + 3] == ter_str_id("t_wall").obj().name());
                CHECK(refused(state, "editor:turn", {.row = "turn:45"}));
                return invoke("editor:turn", {.row = "turn:90"});
            case 4:
                CHECK(rotation(screen) == std::vector<std::string>{"turn:90"});
                CHECK(preview_labels(screen) == generated("east"));
                CHECK(preview_labels(screen) != north);
                return invoke("editor:turn", {.row = "turn:180"});
            case 5:
                CHECK(preview_labels(screen) == generated("south"));
                return invoke("editor:turn", {.row = "turn:270"});
            case 6:
                CHECK(preview_labels(screen) == generated("west"));
                return invoke("editor:turn", {.row = "turn:0"});
            case 7:
                CHECK(preview_labels(screen) == north);
                return act("QUIT");
            default:
                FAIL("the editor outlived its exit");
                return act("QUIT");
        }
    });

    editor::run_editor_session();
    CHECK(step == 8);
}

TEST_CASE(
    "the editor re-rolls the preview with a seed it shows",
    "[editor_session][engine_client_component]") {
    const auto guard = screen_guard{};
    const auto file = mod_file{previewed_extra()};
    const auto restore_mapgen = restore_on_out_of_scope<bool>(disable_mapgen);
    disable_mapgen = false;
    auto step = 0;
    auto first = std::vector<std::string>{};
    const auto seed = [](const ec::screen_view& screen) {
        return body<ec::panel_body>(part(screen, "editor:document")).stats.back().value;
    };

    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto state = captured();
        const auto screen = ec::top_screen(state);
        switch (step++) {
            case 0:
                return invoke("editor:open", {.text = file.path.string()});
            case 1:
                CHECK_FALSE(has_affordance(screen, "editor:reroll"));
                return invoke("editor:preview", {.count = 5});
            case 2:
                CHECK(seed(screen) == "5");
                first = preview_labels(screen);
                REQUIRE(has_affordance(screen, "editor:reroll"));
                return invoke("editor:reroll");
            case 3:
                // A new seed, recorded in the document's stats.
                CHECK(seed(screen) != "5");
                return invoke("editor:preview", {.count = 5});
            case 4:
                CHECK(seed(screen) == "5");
                CHECK(preview_labels(screen) == first);
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
    "the editor lists what can spawn on a square of the mapgen",
    "[editor_session][engine_client_component]") {
    const auto guard = screen_guard{};
    const auto file = mod_file{previewed_extra()};
    const auto restore_mapgen = restore_on_out_of_scope<bool>(disable_mapgen);
    disable_mapgen = false;
    auto step = 0;
    auto first = std::vector<std::string>{};
    const auto rows = [](const ec::screen_view& screen) {
        return body<ec::list_body>(part(screen, "editor:loot")).rows;
    };
    const auto lines = [&](const ec::screen_view& screen) {
        return rows(screen) | std::views::transform([](const ec::row& row) {
                   return row.id + " " + row.columns[0].value + " " + row.columns[1].value + " "
                        + row.columns[2].value;
               })
             | std::ranges::to<std::vector>();
    };

    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto state = captured();
        const auto screen = ec::top_screen(state);
        switch (step++) {
            case 0:
                return invoke("editor:open", {.text = file.path.string()});
            case 1:
                CHECK_FALSE(ec::find_component(*screen.root, "editor:loot"));
                // It sits in the View menu and counts over the squares the select tool marked.
                CHECK(affordance(screen, "editor:loot").group == "View");
                CHECK_FALSE(affordance(screen, "editor:loot").args.row);
                CHECK_FALSE(affordance(screen, "editor:loot").enabled);
                CHECK(affordance(screen, "editor:loot").denial
                      == "Mark squares with the Select tool first.");
                return invoke("editor:preview", {.count = 5});
            case 2:
                return invoke("editor:tool", {.row = "tool:select"});
            case 3:
                return invoke("editor:paint", {.row = "square:3,3"});
            case 4:
                return invoke("editor:paint", {.row = "square:3,3"});
            case 5:
                CHECK(affordance(screen, "editor:loot").enabled);
                return invoke("editor:loot");
            case 6:
                first = lines(screen);
                REQUIRE_FALSE(first.empty());
                // The log says what the count found, wherever the list is drawn.
                REQUIRE(first.size() > 1);
                CHECK(body<ec::text_body>(part(screen, "editor:status")).markup
                      == "1 square marked. " + std::to_string(first.size())
                             + " kinds of items can spawn there.");
                for (const auto& row : rows(screen)) {
                    CHECK(itype_id(row.id.substr(std::string_view("loot:").size())).is_valid());
                    CHECK(row.columns.size() == 3);
                }
                // The same seed rolls the same list.
                return invoke("editor:loot");
            case 7:
                CHECK(lines(screen) == first);
                return invoke("editor:paint", {.row = "square:10,10"});
            case 8:
                // Marking other squares takes the count of the old ones away.
                CHECK_FALSE(ec::find_component(*screen.root, "editor:loot"));
                CHECK_FALSE(ec::find_component(*screen.root, "editor:status"));
                return invoke("editor:paint", {.row = "square:12,12"});
            case 9:
                CHECK_FALSE(ec::find_component(*screen.root, "editor:loot"));
                // Squares nothing spawns on have an empty list.
                return invoke("editor:loot");
            case 10:
                CHECK(rows(screen).empty());
                CHECK(body<ec::text_body>(part(screen, "editor:status")).markup
                      == "9 squares marked: nothing can spawn there.");
                return act("QUIT");
            default:
                FAIL("the editor outlived its exit");
                return act("QUIT");
        }
    });

    editor::run_editor_session();
    CHECK(step == 11);
}

TEST_CASE(
    "the editor's legend flags a symbol the rows never use and one they use but nothing maps",
    "[editor_session][engine_client_component]") {
    const auto guard = screen_guard{};
    auto rows = nlohmann::ordered_json::array({"z" + std::string(23, '.')});
    for (auto row = 1; row < 24; ++row) { rows.push_back(std::string(24, '.')); }
    const auto file = mod_file(
        {{"rows", rows}, {"fill_ter", "t_floor"}, {"furniture", {{"C", "f_chair"}}}});
    auto step = 0;
    const auto use = [](const ec::screen_view& screen, const std::string& id) {
        const auto& rows = body<ec::list_body>(part(screen, "editor:legend")).rows;
        const auto found = std::ranges::find(rows, id, &ec::row::id);
        REQUIRE(found != rows.end());
        const auto column =
            std::ranges::find(found->columns, "use", &game_client::interaction_column::key);
        return column == found->columns.end() ? std::string{} : column->value;
    };

    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto state = captured();
        const auto screen = ec::top_screen(state);
        switch (step++) {
            case 0:
                return invoke("editor:open", {.text = file.path.string()});
            case 1:
                CHECK(use(screen, "symbol:C") == "Unused");
                // The loader puts the entry's fill terrain where nothing maps a symbol.
                CHECK(use(screen, "symbol:z") == "Fill");
                CHECK(use(screen, "symbol:.").empty());
                CHECK(use(screen, "symbol:#") == "Unused");
                return act("QUIT");
            default:
                FAIL("the editor outlived its exit");
                return act("QUIT");
        }
    });

    editor::run_editor_session();
    CHECK(step == 2);
}

TEST_CASE(
    "the editor's legend names what a symbol places, its ids beside, and shows a space",
    "[editor_session][engine_client_component]") {
    const auto guard = screen_guard{};
    auto rows = nlohmann::ordered_json::array({" %" + std::string(22, '.')});
    for (auto row = 1; row < 24; ++row) { rows.push_back(std::string(24, '.')); }
    const auto file = mod_file(
        {{"rows", rows},
         {"fill_ter", "t_floor"},
         {"terrain",
          {{".", nlohmann::ordered_json::array({"t_dirt", "t_grass"})},
           {"#", nlohmann::ordered_json::parse(R"([["t_dirt", 3], "t_grass"])")},
           {"%", "t_region_groundcover_urban"}}},
         {"furniture", {{"C", "f_chair"}}},
         {"items", {{"#", {{"item", "SUS_tools"}, {"chance", 30}}}}}});
    auto step = 0;
    const auto row = [](const ec::screen_view& screen, const std::string& id) -> ec::row {
        const auto& rows = body<ec::list_body>(part(screen, "editor:legend")).rows;
        const auto found = std::ranges::find(rows, id, &ec::row::id);
        REQUIRE(found != rows.end());
        return *found;
    };
    const auto column = [&](const ec::screen_view& screen, const std::string& id,
                            const std::string& key) {
        const auto columns = row(screen, id).columns;
        const auto found = std::ranges::find(columns, key, &game_client::interaction_column::key);
        return found == columns.end() ? std::string("(none)") : found->label + "=" + found->value;
    };
    const auto stat = [](const ec::screen_view& screen, const std::string& key) {
        const auto& stats = body<ec::panel_body>(part(screen, "editor:document")).stats;
        const auto found = std::ranges::find(stats, key, &ec::panel_stat::key);
        return found == stats.end() ? std::string("(none)") : found->value;
    };

    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto screen = ec::top_screen(captured());
        switch (step++) {
            case 0:
                return invoke("editor:open", {.text = file.path.string()});
            case 1:
                CHECK(column(screen, "symbol:.", "terrain") == "Terrain=dirt, grass");
                CHECK(column(screen, "symbol:.", "id") == "ID=t_dirt, t_grass");
                CHECK(column(screen, "symbol:#", "terrain") == "Terrain=dirt x3, grass");
                CHECK(column(screen, "symbol:#", "items") == "Item groups=SUS_tools");
                CHECK(column(screen, "symbol:#", "id") == "ID=t_dirt, t_grass, SUS_tools");
                CHECK(column(screen, "symbol:C", "furniture") == "Furniture=chair");
                // Every row has the same columns: the flag first, the ids last.
                for (const auto& each : body<ec::list_body>(part(screen, "editor:legend")).rows) {
                    CHECK(
                        (each.columns | std::views::transform(&game_client::interaction_column::key)
                         | std::ranges::to<std::vector>())
                        == std::vector<std::string>{"use", "terrain", "furniture", "items", "id"});
                }
                // A region's stand-in reads as what the default region puts there most.
                CHECK(column(screen, "symbol:%", "terrain").starts_with("Terrain="));
                CHECK(column(screen, "symbol:%", "terrain").find("pseudo") == std::string::npos);
                // A space draws the fill terrain, and reads as a visible mark.
                CHECK(row(screen, "symbol: ").label == "\u2423");
                CHECK(column(screen, "symbol: ", "use") == "Use=Fill");
                CHECK(column(screen, "symbol: ", "terrain") == "Terrain=wooden floor");
                // What is drawn with reads by its mark and what it places.
                CHECK(stat(screen, "symbol") == "\u2423 wooden floor");
                CHECK(stat(screen, "inside") == "\u2423 wooden floor");
                return invoke("editor:symbol", {.row = "symbol:C"});
            case 2:
                CHECK(stat(screen, "symbol") == "C chair");
                return act("QUIT");
            default:
                FAIL("the editor outlived its exit");
                return act("QUIT");
        }
    });

    editor::run_editor_session();
    CHECK(step == 3);
}

TEST_CASE(
    "the editor tells apart entries of a file that have the same name",
    "[editor_session][engine_client_component]") {
    const auto guard = screen_guard{};
    auto step = 0;

    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto screen = ec::top_screen(captured());
        switch (step++) {
            case 0:
                return invoke("editor:open",
                              {.text = PATH_INFO::datadir() + "json/mapgen/house/2Story01.json"});
            case 1: {
                const auto labels =
                    body<ec::list_body>(part(screen, "editor:objects")).rows
                    | std::views::transform(&ec::row::label) | std::ranges::to<std::vector>();
                REQUIRE(labels.size() >= 2);
                CHECK(labels[0] == "2Story01_1 (1 of 2)");
                CHECK(labels[1] == "2Story01_1 (2 of 2)");
                return act("QUIT");
            }
            default:
                FAIL("the editor outlived its exit");
                return act("QUIT");
        }
    });

    editor::run_editor_session();
    CHECK(step == 2);
}

TEST_CASE(
    "the editor names an entry of a list of overmap terrains by those terrains",
    "[editor_session][engine_client_component]") {
    const auto guard = screen_guard{};
    auto step = 0;

    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto screen = ec::top_screen(captured());
        switch (step++) {
            case 0:
                return invoke(
                    "editor:open", {.text = PATH_INFO::datadir() + "json/mapgen/shelter.json"});
            case 1:
                CHECK(body<ec::list_body>(part(screen, "editor:objects")).rows.front().label
                      == "shelter");
                return invoke("editor:apply");
            case 2:
                CHECK(std::get<ec::text_body>(part(screen, "editor:status").body).markup
                      == "The game generates shelter with this entry now.");
                return act("QUIT");
            default:
                FAIL("the editor outlived its exit");
                return act("QUIT");
        }
    });

    editor::run_editor_session();
    CHECK(step == 3);
}

TEST_CASE(
    "the editor's catalog tells apart entries of the same name",
    "[editor_session][engine_client_component]") {
    const auto guard = screen_guard{};
    const auto file = mod_file{};
    auto step = 0;

    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto screen = ec::top_screen(captured());
        switch (step++) {
            case 0:
                return invoke("editor:open", {.text = file.path.string()});
            case 1:
                return invoke("editor:catalog_filter", {.text = "t_adobe_brick_floor"});
            case 2: {
                const auto labels =
                    body<ec::list_body>(part(screen, "editor:catalog")).rows
                    | std::views::transform(&ec::row::label) | std::ranges::to<std::vector>();
                CHECK(labels == std::vector<std::string>{"adobe floor", "adobe floor (noroof)"});
                return invoke("editor:catalog_filter", {.text = "t_grass"});
            }
            case 3:
                // A name only one entry has stays as it is.
                CHECK(body<ec::list_body>(part(screen, "editor:catalog")).rows.front().label
                      == "grass");
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
    "the editor refuses a symbol the rows would use without terrain under it",
    "[editor_session][engine_client_component]") {
    const auto guard = screen_guard{};
    const auto file = mod_file(nlohmann::ordered_json{{"furniture", {{"C", "f_chair"}}}});
    auto step = 0;
    const auto set = [](const std::string& id, const std::string& text) {
        return invoke("editor:mapping_set", {.row = id, .text = text});
    };
    const auto notice = [](const ec::screen_view& screen) {
        return body<ec::text_body>(part(screen, "editor:notice")).markup;
    };
    const auto bare = [](const ec::screen_view& screen) {
        return body<ec::list_body>(part(screen, "editor:diagnostics")).rows.empty()
            && saved(screen) == "Yes";
    };

    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto state = captured();
        const auto screen = ec::top_screen(state);
        switch (step++) {
            case 0:
                return invoke("editor:open", {.text = file.path.string()});
            case 1:
                return set("symbol", "x");
            case 2:
                return set("layer", "furniture");
            case 3:
                return set("value", "f_chair");
            case 4:
                return invoke("editor:mapping_apply");
            case 5:
                CHECK(notice(screen)
                      == "x has no terrain under it: map its terrain first, or set fill_ter.");
                CHECK(bare(screen));
                return set("symbol", ".");
            case 6:
                return set("layer", "terrain");
            case 7:
                return set("value", "");
            case 8:
                return invoke("editor:mapping_apply");
            case 9:
                CHECK(notice(screen)
                      == ". has no terrain under it: map its terrain first, or set fill_ter.");
                CHECK(bare(screen));
                return invoke("editor:symbol", {.row = "symbol:C"});
            case 10:
                return invoke("editor:paint", {.row = "square:2,3"});
            case 11:
                CHECK(notice(screen)
                      == "C has no terrain under it: map its terrain first, or set fill_ter.");
                CHECK(cell(screen, 2, 3).glyph == ".");
                CHECK(bare(screen));
                return act("QUIT");
            default:
                FAIL("the editor outlived its exit");
                return act("QUIT");
        }
    });

    editor::run_editor_session();
    CHECK(step == 12);
    CHECK(read_file(file.path) == file.text);
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
                CHECK(column_value(rows[2], "terrain") == "dirt, grass");
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
                CHECK(placed[0].columns[2].value == "monster: mon_zombie");
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
    "the editor adds a placement over the selection and deletes it, each one undo step",
    "[editor_session][engine_client_component]") {
    const auto guard = screen_guard{};
    const auto file = mod_file(nlohmann::ordered_json::parse(
        R"({"place_monster": [{"monster": "mon_zombie", "x": [3, 5], "y": 7}]})"));
    auto step = 0;
    const auto placed = [](const ec::screen_view& screen) {
        return body<ec::list_body>(part(screen, "editor:placements")).rows
             | std::views::transform(&ec::row::id) | std::ranges::to<std::vector>();
    };
    const auto field =
        [](const ec::screen_view& screen, const std::string& form, const std::string& id) {
            const auto& fields = body<ec::form_body>(part(screen, form)).fields;
            const auto found = std::ranges::find(fields, id, &ec::form_field::id);
            REQUIRE(found != fields.end());
            return *found;
        };
    const auto set = [](const std::string& id, const std::string& text) {
        return invoke("editor:place_set", {.row = id, .text = text});
    };

    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto state = captured();
        const auto screen = ec::top_screen(state);
        switch (step++) {
            case 0:
                return invoke("editor:open", {.text = file.path.string()});
            case 1:
                CHECK(field(screen, "editor:place", "kind").type == "select");
                CHECK(std::ranges::contains(
                    field(screen, "editor:place", "kind").options, std::string("place_items"),
                    &ec::form_option::id));
                CHECK_FALSE(has_affordance(screen, "editor:placement_delete"));
                return invoke("editor:tool", {.row = "tool:select"});
            case 2:
                return invoke("editor:paint", {.row = "square:2,3"});
            case 3:
                return invoke("editor:paint", {.row = "square:4,6"});
            case 4:
                // The selection is where a new placement lands.
                CHECK(field(screen, "editor:place", "x").value == "2-4");
                CHECK(field(screen, "editor:place", "y").value == "3-6");
                return set("kind", "place_items");
            case 5:
                return set("value", R"({"item": "no_such_group"})");
            case 6:
                return invoke("editor:place_apply");
            case 7:
                CHECK(
                    body<ec::text_body>(part(screen, "editor:notice")).markup.find("no_such_group")
                    != std::string::npos);
                CHECK(placed(screen).size() == 1);
                return set("value", R"({"item": "tools_common", "chance": 50})");
            case 8:
                return invoke("editor:place_apply");
            case 9:
                CHECK(placed(screen)
                      == std::vector<
                          std::string>{"placement:place_monster:0", "placement:place_items:0"});
                // The new placement is picked, so it moves and deletes like any other.
                CHECK(field(screen, "editor:move", "x").value == "2-4");
                CHECK(field(screen, "editor:move", "y").value == "3-6");
                CHECK(saved(screen) == "No");
                return invoke("editor:placement_delete");
            case 10:
                CHECK(placed(screen) == std::vector<std::string>{"placement:place_monster:0"});
                CHECK_FALSE(has_affordance(screen, "editor:placement_delete"));
                return invoke("editor:undo");
            case 11:
                CHECK(placed(screen).size() == 2);
                return invoke("editor:undo");
            case 12:
                CHECK(placed(screen).size() == 1);
                CHECK(saved(screen) == "Yes");
                return act("QUIT");
            default:
                FAIL("the editor outlived its exit");
                return act("QUIT");
        }
    });

    editor::run_editor_session();
    CHECK(step == 13);
    CHECK(read_file(file.path) == file.text);
}

TEST_CASE(
    "the editor cuts a selection with its placements and pastes it mirrored, each one undo step",
    "[editor_session][engine_client_component]") {
    const auto guard = screen_guard{};
    const auto file = mod_file(nlohmann::ordered_json::parse(
        R"({"place_monster": [{"monster": "mon_zombie", "x": [3, 5], "y": 7}]})"));
    auto step = 0;
    const auto placed = [](const ec::screen_view& screen) {
        return body<ec::list_body>(part(screen, "editor:placements")).rows;
    };
    const auto clipboard = [](const ec::screen_view& screen) {
        const auto& stats = body<ec::panel_body>(part(screen, "editor:document")).stats;
        const auto found = std::ranges::find(stats, "clipboard", &ec::panel_stat::key);
        REQUIRE(found != stats.end());
        return found->value;
    };

    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto state = captured();
        const auto screen = ec::top_screen(state);
        switch (step++) {
            case 0:
                return invoke("editor:open", {.text = file.path.string()});
            case 1:
                CHECK_FALSE(affordance(screen, "editor:cut").enabled);
                return invoke("editor:paint", {.row = "square:3,7"});
            case 2:
                return invoke("editor:tool", {.row = "tool:select"});
            case 3:
                return invoke("editor:paint", {.row = "square:2,6"});
            case 4:
                return invoke("editor:paint", {.row = "square:5,8"});
            case 5:
                return invoke("editor:inside", {.row = "symbol:."});
            case 6:
                return invoke("editor:cut");
            case 7:
                // What is cut leaves the inside symbol, and takes the placements wholly inside.
                CHECK(cell(screen, 3, 7).glyph == ".");
                CHECK(placed(screen).empty());
                CHECK(clipboard(screen) == "4x3");
                return invoke("editor:flip_x");
            case 8:
                return invoke("editor:tool", {.row = "tool:paste"});
            case 9:
                return invoke("editor:paint", {.row = "square:10,10"});
            case 10:
                CHECK(cell(screen, 12, 11).glyph == "#");
                REQUIRE(placed(screen).size() == 1);
                CHECK(placed(screen)[0].columns[0].value == "10-12");
                CHECK(placed(screen)[0].columns[1].value == "11");
                return invoke("editor:undo");
            case 11:
                CHECK(cell(screen, 12, 11).glyph == ".");
                CHECK(placed(screen).empty());
                return invoke("editor:undo");
            case 12:
                CHECK(cell(screen, 3, 7).glyph == "#");
                CHECK(placed(screen).size() == 1);
                return invoke("editor:undo");
            case 13:
                CHECK(saved(screen) == "Yes");
                return act("QUIT");
            default:
                FAIL("the editor outlived its exit");
                return act("QUIT");
        }
    });

    editor::run_editor_session();
    CHECK(step == 14);
    CHECK(read_file(file.path) == file.text);
}

namespace {
/// Rows of 24 floor squares with a wall at columns 1 to 3 of row 1, and a monster on the middle
/// one.
auto walled_rows() -> nlohmann::ordered_json {
    auto rows = nlohmann::ordered_json::array();
    for (auto row = 0; row < 24; ++row) {
        rows.push_back(
            row == 1 ? "." + std::string(3, '#') + std::string(20, '.') : std::string(24, '.'));
    }
    return {{"rows", rows},
            {"place_monster",
             nlohmann::ordered_json::parse(R"([{"monster": "mon_zombie", "x": 2, "y": 1}])")}};
}
auto object_of(const std::filesystem::path& path) -> nlohmann::ordered_json {
    return nlohmann::ordered_json::parse(read_file(path))[0]["object"];
}
auto placement_rows(const ec::screen_view& screen) {
    return body<ec::list_body>(part(screen, "editor:placements")).rows;
}
} // namespace

TEST_CASE(
    "a copy from one document pastes into another opened later, with its placements, as one undo",
    "[editor_session][engine_client_component]") {
    const auto guard = screen_guard{};
    const auto from = mod_file(walled_rows(), "a.json");
    const auto into = mod_file(nlohmann::ordered_json::object(), "b.json");
    auto step = 0;

    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto state = captured();
        const auto screen = ec::top_screen(state);
        switch (step++) {
            case 0:
                return invoke("editor:open", {.text = from.path.string()});
            case 1:
                return invoke("editor:tool", {.row = "tool:select"});
            case 2:
                return invoke("editor:paint", {.row = "square:1,1"});
            case 3:
                return invoke("editor:paint", {.row = "square:3,2"});
            case 4:
                return invoke("editor:copy");
            case 5:
                return invoke("editor:open", {.text = into.path.string()});
            case 6:
                // The other document has no walls and no placements; the clipboard stays.
                CHECK(walls(screen) == 0);
                CHECK(placement_rows(screen).empty());
                CHECK(affordance(screen, "editor:rotate").enabled);
                return invoke("editor:tool", {.row = "tool:paste"});
            case 7:
                return invoke("editor:paint", {.row = "square:10,10"});
            case 8:
                CHECK(walls(screen) == 3);
                CHECK(cell(screen, 10, 10).glyph == "#");
                CHECK(cell(screen, 12, 10).glyph == "#");
                REQUIRE(placement_rows(screen).size() == 1);
                CHECK(placement_rows(screen)[0].columns[0].value == "11");
                CHECK(placement_rows(screen)[0].columns[1].value == "10");
                return invoke("editor:undo");
            case 9:
                CHECK(walls(screen) == 0);
                CHECK(placement_rows(screen).empty());
                return act("QUIT");
            default:
                FAIL("the editor outlived its exit");
                return act("QUIT");
        }
    });

    editor::run_editor_session();
    CHECK(step == 10);
    CHECK(read_file(from.path) == from.text);
    CHECK(read_file(into.path) == into.text);
}

TEST_CASE(
    "a cut in one document stays cut there and pastes into another opened later",
    "[editor_session][engine_client_component]") {
    const auto guard = screen_guard{};
    const auto from = mod_file(walled_rows(), "a.json");
    const auto into = mod_file(nlohmann::ordered_json::object(), "b.json");
    auto step = 0;

    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto state = captured();
        const auto screen = ec::top_screen(state);
        switch (step++) {
            case 0:
                return invoke("editor:open", {.text = from.path.string()});
            case 1:
                return invoke("editor:tool", {.row = "tool:select"});
            case 2:
                return invoke("editor:paint", {.row = "square:1,1"});
            case 3:
                return invoke("editor:paint", {.row = "square:3,2"});
            case 4:
                return invoke("editor:inside", {.row = "symbol:."});
            case 5:
                return invoke("editor:cut");
            case 6:
                CHECK(walls(screen) == 0);
                CHECK(placement_rows(screen).empty());
                CHECK(
                    body<ec::list_body>(part(screen, "editor:history")).rows.back().label == "Cut");
                return invoke("editor:save");
            case 7:
                return invoke("editor:open", {.text = into.path.string()});
            case 8:
                return invoke("editor:tool", {.row = "tool:paste"});
            case 9:
                return invoke("editor:paint", {.row = "square:5,5"});
            case 10:
                CHECK(walls(screen) == 3);
                CHECK(cell(screen, 5, 5).glyph == "#");
                REQUIRE(placement_rows(screen).size() == 1);
                CHECK(placement_rows(screen)[0].columns[0].value == "6");
                CHECK(placement_rows(screen)[0].columns[1].value == "5");
                CHECK(body<ec::list_body>(part(screen, "editor:history")).rows.back().label
                      == "Paste");
                return act("QUIT");
            case 11:
                return answer("Leave without saving");
            default:
                FAIL("the editor outlived its exit");
                return act("QUIT");
        }
    });

    editor::run_editor_session();
    CHECK(step == 12);
    // The first document keeps the cut: its wall row is floor again and its monster is gone.
    const auto left = object_of(from.path);
    CHECK(left["rows"][1].get<std::string>() == std::string(24, '.'));
    CHECK_FALSE(left.contains("place_monster"));
    CHECK(read_file(into.path) == into.text);
}

TEST_CASE(
    "a paste brings the copied file's mapping of a symbol the target lacks, and keeps the target's "
    "own mapping, saying so",
    "[editor_session][engine_client_component]") {
    const auto guard = screen_guard{};
    const auto from = mod_file(walled_rows(), "a.json");
    const auto into =
        mod_file(nlohmann::ordered_json::parse(R"({"terrain": {".": "t_floor"}})"), "b.json");
    const auto other = mod_file(
        nlohmann::ordered_json::parse(R"({"terrain": {".": "t_floor", "#": "t_rock"}})"), "c.json");
    const auto notice = [](const ec::screen_view& screen) {
        return body<ec::text_body>(part(screen, "editor:notice")).markup;
    };
    auto step = 0;

    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto state = captured();
        const auto screen = ec::top_screen(state);
        switch (step++) {
            case 0:
                return invoke("editor:open", {.text = from.path.string()});
            case 1:
                return invoke("editor:tool", {.row = "tool:select"});
            case 2:
                return invoke("editor:paint", {.row = "square:1,1"});
            case 3:
                return invoke("editor:paint", {.row = "square:3,1"});
            case 4:
                return invoke("editor:copy");
            case 5:
                return invoke("editor:open", {.text = into.path.string()});
            case 6:
                return invoke("editor:tool", {.row = "tool:paste"});
            case 7:
                return invoke("editor:paint", {.row = "square:4,4"});
            case 8:
                CHECK(walls(screen) == 3);
                CHECK_FALSE(ec::find_component(*screen.root, "editor:notice"));
                CHECK(body<ec::list_body>(part(screen, "editor:diagnostics")).rows.empty());
                return invoke("editor:undo");
            case 9:
                // One undo takes back the squares and the mapping together.
                CHECK(walls(screen) == 0);
                CHECK(saved(screen) == "Yes");
                return invoke("editor:redo");
            case 10:
                return invoke("editor:save");
            case 11:
                return invoke("editor:open", {.text = other.path.string()});
            case 12:
                return invoke("editor:paint", {.row = "square:4,4"});
            case 13:
                CHECK(walls(screen) == 3);
                CHECK(notice(screen) == "Pasted symbols keep what this entry maps them to: #");
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
    CHECK(object_of(into.path)["terrain"]
          == nlohmann::ordered_json::parse(R"({".": "t_floor", "#": "t_wall"})"));
    CHECK(object_of(other.path)["terrain"]
          == nlohmann::ordered_json::parse(R"({".": "t_floor", "#": "t_rock"})"));
}

TEST_CASE(
    "a paste keeps the meaning of a symbol the target uses unmapped, and maps only symbols that "
    "land",
    "[editor_session][engine_client_component]") {
    const auto guard = screen_guard{};
    const auto from = mod_file(walled_rows(), "a.json");
    // Its squares are grass by `fill_ter`; `.` maps nothing.
    const auto into = mod_file(
        nlohmann::ordered_json::parse(R"({"fill_ter": "t_grass", "terrain": {"x": "t_floor"}})"),
        "b.json");
    const auto notice = [](const ec::screen_view& screen) {
        return body<ec::text_body>(part(screen, "editor:notice")).markup;
    };
    auto step = 0;

    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto screen = ec::top_screen(captured());
        switch (step++) {
            case 0:
                return invoke("editor:open", {.text = from.path.string()});
            case 1:
                return invoke("editor:tool", {.row = "tool:select"});
            case 2:
                return invoke("editor:paint", {.row = "square:0,1"});
            case 3:
                return invoke("editor:paint", {.row = "square:1,1"});
            case 4:
                return invoke("editor:copy");
            case 5:
                return invoke("editor:open", {.text = into.path.string()});
            case 6:
                return invoke("editor:tool", {.row = "tool:paste"});
            case 7:
                // Only the `.` lands; the wall falls past the last column.
                return invoke("editor:paint", {.row = "square:23,4"});
            case 8:
                CHECK(walls(screen) == 0);
                CHECK(notice(screen) == "Pasted symbols keep what this entry maps them to: .");
                return invoke("editor:save");
            case 9:
                return act("QUIT");
            default:
                FAIL("the editor outlived its exit");
                return act("QUIT");
        }
    });

    editor::run_editor_session();
    CHECK(step == 10);
    CHECK(object_of(into.path)["terrain"] == nlohmann::ordered_json::parse(R"({"x": "t_floor"})"));
}

TEST_CASE(
    "a paste refuses symbols whose copied mapping reads the copied entry's parameters",
    "[editor_session][engine_client_component]") {
    const auto guard = screen_guard{};
    auto object = walled_rows();
    object["parameters"] = nlohmann::ordered_json::parse(
        R"({"wall": {"type": "ter_str_id", "default": {"distribution": [["t_wall", 1]]}}})");
    object["terrain"] = nlohmann::ordered_json::parse(
        R"({".": "t_floor", "#": {"param": "wall"}})");
    const auto from = mod_file(object, "a.json");
    const auto into =
        mod_file(nlohmann::ordered_json::parse(R"({"terrain": {".": "t_floor"}})"), "b.json");
    const auto notice = [](const ec::screen_view& screen) {
        return body<ec::text_body>(part(screen, "editor:notice")).markup;
    };
    auto step = 0;

    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto screen = ec::top_screen(captured());
        switch (step++) {
            case 0:
                return invoke("editor:open", {.text = from.path.string()});
            case 1:
                return invoke("editor:tool", {.row = "tool:select"});
            case 2:
                return invoke("editor:paint", {.row = "square:1,1"});
            case 3:
                return invoke("editor:paint", {.row = "square:3,1"});
            case 4:
                return invoke("editor:copy");
            case 5:
                return invoke("editor:open", {.text = into.path.string()});
            case 6:
                return invoke("editor:tool", {.row = "tool:paste"});
            case 7:
                return invoke("editor:paint", {.row = "square:4,4"});
            case 8:
                CHECK(walls(screen) == 0);
                CHECK(saved(screen) == "Yes");
                CHECK(notice(screen)
                      == "These symbols read parameters of the entry they were copied from; map "
                         "them here first: #");
                return act("QUIT");
            default:
                FAIL("the editor outlived its exit");
                return act("QUIT");
        }
    });

    editor::run_editor_session();
    CHECK(step == 9);
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
                // The pager belongs to the list it turns.
                CHECK(std::ranges::contains(
                    part(screen, "editor:catalog").affordances,
                    std::string("editor:catalog_page")));
                return invoke("editor:catalog_filter", {.text = "t_dirt"});
            }
            case 2: {
                const auto& rows = entries(screen).rows;
                CHECK(entries(screen).filter == "t_dirt");
                CHECK(std::ranges::contains(
                    part(screen, "editor:catalog").affordances,
                    std::string("editor:catalog_filter")));
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
                CHECK(placed(screen)[2].value == "monster: mon_zombie, chance: 50");
                // A member set on a placement changes the placement, though the patch adds it.
                CHECK(body<ec::list_body>(part(screen, "editor:history")).rows.back().label
                      == "Change place monster");
                return invoke("editor:field_set", {.row = "value", .text = R"("fifty")"});
            case 12:
                return invoke("editor:field_apply");
            case 13:
                // What the loader refuses is not written.
                CHECK(ec::find_component(*screen.root, "editor:notice"));
                CHECK(placed(screen)[2].value == "monster: mon_zombie, chance: 50");
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
    "the editor opens a mapgen of an overmap terrain the game lacks and says so among the "
    "diagnostics",
    "[editor_session][engine_client_component]") {
    const auto guard = screen_guard{};
    const auto file = mod_file{};
    {
        auto entries = nlohmann::ordered_json::parse(file.text);
        entries[0]["om_terrain"] = "zz_no_such_terrain";
        auto out = std::ofstream(file.path, std::ios::binary | std::ios::trunc);
        out << entries.dump(2);
    }
    auto step = 0;

    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto screen = ec::top_screen(captured());
        switch (step++) {
            case 0:
                return invoke("editor:open", {.text = file.path.string()});
            case 1: {
                CHECK_FALSE(ec::find_component(*screen.root, "editor:notice"));
                CHECK(walls(screen) == 0);
                const auto& rows = body<ec::list_body>(part(screen, "editor:diagnostics")).rows;
                CHECK(std::ranges::any_of(rows, [](const ec::row& row) {
                    return row.label.contains("zz_no_such_terrain");
                }));
                return act("QUIT");
            }
            default:
                FAIL("the editor outlived its exit");
                return act("QUIT");
        }
    });

    editor::run_editor_session();
    CHECK(step == 2);
}

TEST_CASE(
    "the editor's tools and clipboard commands carry the keys image editors use",
    "[editor_session][engine_client_component]") {
    const auto guard = screen_guard{};
    const auto file = mod_file{};
    auto step = 0;

    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto state = captured();
        const auto screen = ec::top_screen(state);
        switch (step++) {
            case 0:
                return invoke("editor:open", {.text = file.path.string()});
            case 1: {
                const auto& tabs = body<ec::tabs_body>(part(screen, "editor:tools")).tabs;
                const auto keys =
                    tabs | std::views::transform([](const ec::tab_entry& tab) {
                        return std::pair{tab.id, tab.hotkey};
                    })
                    | std::ranges::to<std::map>();
                CHECK(
                    keys
                    == std::map<std::string, std::string>{
                        {"tool:point", "b"},
                        {"tool:line", "l"},
                        {"tool:rect", "r"},
                        {"tool:rect_filled", "u"},
                        {"tool:ellipse", "o"},
                        {"tool:room", "m"},
                        {"tool:fill", "f"},
                        {"tool:pick", "i"},
                        {"tool:select", "s"},
                        {"tool:paste", "v"},
                        {"tool:placements", "p"}});
                CHECK(wire_part(state, "editor:tools")["tabs"][0]["hotkey"] == "b");
                CHECK(affordance(screen, "editor:copy").keys == std::vector<std::string>{"CTRL+C"});
                CHECK(affordance(screen, "editor:cut").keys == std::vector<std::string>{"CTRL+X"});
                CHECK(affordance(screen, "editor:rotate").keys == std::vector<std::string>{"z"});
                CHECK(affordance(screen, "editor:flip_x").keys == std::vector<std::string>{"x"});
                CHECK(affordance(screen, "editor:flip_y").keys == std::vector<std::string>{"y"});
                CHECK(affordance(screen, "editor:test").keys == std::vector<std::string>{"F5"});
                CHECK(affordance(screen, "editor:stamp").keys == std::vector<std::string>{"F6"});
                CHECK(affordance(screen, "editor:apply").keys == std::vector<std::string>{"F7"});
                CHECK(affordance(screen, "editor:preview").keys == std::vector<std::string>{"F8"});
                return act("QUIT");
            }
            default:
                FAIL("the editor outlived its exit");
                return act("QUIT");
        }
    });

    editor::run_editor_session();
    CHECK(step == 2);
}

TEST_CASE(
    "the editor outlines each placement on the canvas, and dragging one moves or stretches it in "
    "one step",
    "[editor_session][engine_client_component]") {
    const auto guard = screen_guard{};
    const auto file = mod_file(nlohmann::ordered_json::parse(
        R"({"place_monster": [{"monster": "mon_zombie", "x": [3, 5], "y": 7}],
            "place_items": [{"item": "trash", "x": 10, "y": [2, 4], "chance": 50}]})"));
    const auto region = [](const ec::screen_view& screen, const std::string& id) {
        const auto& regions = body<ec::grid_body>(part(screen, "editor:canvas")).regions;
        const auto found = std::ranges::find(regions, id, &ec::grid_region::id);
        INFO(id);
        REQUIRE(found != regions.end());
        return *found;
    };
    const auto notice = [](const ec::screen_view& screen) {
        return body<ec::text_body>(part(screen, "editor:notice")).markup;
    };
    const auto monster = std::string("placement:place_monster:0");
    const auto items = std::string("placement:place_items:0");
    auto step = 0;

    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto state = captured();
        const auto screen = ec::top_screen(state);
        switch (step++) {
            case 0:
                return invoke("editor:open", {.text = file.path.string()});
            case 1: {
                // The squares a placement may land on, as the loader reads its ranges.
                CHECK(region(screen, monster).from == ec::grid_point{3, 7});
                CHECK(region(screen, monster).to == ec::grid_point{5, 7});
                CHECK(region(screen, monster).label == "place_monster");
                CHECK_FALSE(region(screen, monster).selected);
                CHECK(region(screen, items).from == ec::grid_point{10, 2});
                CHECK(region(screen, items).to == ec::grid_point{10, 4});
                const auto wire = wire_part(state, "editor:canvas");
                CHECK(wire["regions"][0]
                      == nlohmann::json::parse(
                          R"({"id": "placement:place_monster:0", "from": {"x": 3, "y": 7},
                              "to": {"x": 5, "y": 7}, "label": "place_monster"})"));
                // Drawing tools draw; only the placements tool drags placements.
                CHECK(body<ec::grid_body>(part(screen, "editor:canvas")).region_drag.empty());
                return invoke("editor:tool", {.row = "tool:placements"});
            }
            case 2:
                CHECK(body<ec::grid_body>(part(screen, "editor:canvas")).region_drag
                      == "editor:placement_span");
                CHECK(body<ec::grid_body>(part(screen, "editor:canvas")).drag.empty());
                CHECK(wire_part(state, "editor:canvas")["region_drag"] == "editor:placement_span");
                CHECK_FALSE(std::ranges::contains(
                    part(screen, "editor:canvas").affordances,
                    std::string("editor:placement_span")));
                CHECK(refused(state, "editor:placement_span", {.row = "placement:place_fields:0"}));
                // A click picks the placement over the square.
                return invoke("editor:paint", {.row = "square:4,7"});
            case 3:
                CHECK(region(screen, monster).selected);
                CHECK_FALSE(region(screen, items).selected);
                return invoke("editor:placement_span", {.row = monster, .text = "2,1,2,1"});
            case 4:
                // Moved: both corners went two east and one south.
                CHECK(region(screen, monster).from == ec::grid_point{5, 8});
                CHECK(region(screen, monster).to == ec::grid_point{7, 8});
                CHECK(saved(screen) == "No");
                return invoke("editor:placement_span", {.row = items, .text = "0,0,3,-1"});
            case 5:
                // Stretched by its bottom-right corner, and picked.
                CHECK(region(screen, items).from == ec::grid_point{10, 2});
                CHECK(region(screen, items).to == ec::grid_point{13, 3});
                CHECK(region(screen, items).selected);
                return invoke("editor:placement_span", {.row = monster, .text = "0,0,-3,0"});
            case 6:
                CHECK(notice(screen) == "A placement cannot end before it starts.");
                CHECK(region(screen, monster).to == ec::grid_point{7, 8});
                return invoke("editor:placement_span", {.row = monster, .text = "1,1"});
            case 7:
                CHECK(notice(screen)
                      == "A placement moves by four whole numbers: how far its first and last "
                         "squares go east and south.");
                return invoke(
                    "editor:placement_span", {.row = monster, .text = "2147483647,0,2147483647,0"});
            case 8:
                CHECK(notice(screen) == "A placement moves at most 1000 squares.");
                CHECK(region(screen, monster).to == ec::grid_point{7, 8});
                return invoke("editor:undo");
            case 9:
                CHECK(region(screen, items).to == ec::grid_point{10, 4});
                CHECK(region(screen, monster).to == ec::grid_point{7, 8});
                return invoke("editor:undo");
            case 10:
                CHECK(region(screen, monster).from == ec::grid_point{3, 7});
                CHECK(saved(screen) == "Yes");
                return act("QUIT");
            default:
                FAIL("the editor outlived its exit");
                return act("QUIT");
        }
    });

    editor::run_editor_session();
    CHECK(step == 11);
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
                CHECK(column_value(legend[0], "terrain") == "dirt");
                CHECK(body<ec::list_body>(part(screen, "editor:diagnostics")).rows.empty());
                CHECK(saved(screen) == "Yes");
                // The form starts over for the next file: what it named is not typed after.
                const auto& fields = body<ec::form_body>(part(screen, "editor:new")).fields;
                const auto value = [&](const std::string& id) {
                    return std::ranges::find(fields, id, &ec::form_field::id)->value;
                };
                CHECK(value("path").empty());
                CHECK(value("om_terrain").empty());
                return invoke("editor:new_set", {.row = "path", .text = made.string()});
            }
            case 9:
                return invoke("editor:new_set", {.row = "om_terrain", .text = "house_04"});
            case 10:
                return invoke("editor:new_apply");
            case 11:
                CHECK(notice(screen) == made.string() + " already exists.");
                return act("QUIT");
            default:
                FAIL("the editor outlived its exit");
                return act("QUIT");
        }
    });

    editor::run_editor_session();
    CHECK(step == 12);
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

TEST_CASE(
    "the editor closes a file it left without saving and cannot read again",
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
                std::filesystem::remove(file.path);
                return act("QUIT");
            case 3:
                return answer("Leave without saving");
            default:
                FAIL("the editor outlived its exit");
                return act("QUIT");
        }
    });
    editor::run_editor_session();
    CHECK(step == 4);

    step = 0;
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto screen = ec::top_screen(captured());
        CHECK(step++ == 0);
        // Nothing is open: the discarded edits did not come back.
        CHECK(body<ec::form_body>(part(screen, "editor:file")).fields.front().value.empty());
        CHECK_FALSE(has_affordance(screen, "editor:paint"));
        return act("QUIT");
    });
    editor::run_editor_session();
    CHECK(step == 1);
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

TEST_CASE(
    "the editor draws a room, with a brush and mirrored, and goes back to any step of its history",
    "[editor_session][engine_client_component]") {
    const auto guard = screen_guard{};
    const auto file = mod_file{};
    auto step = 0;
    const auto stat = [](const ec::screen_view& screen, const std::string& key) {
        const auto& stats = body<ec::panel_body>(part(screen, "editor:document")).stats;
        const auto found = std::ranges::find(stats, key, &ec::panel_stat::key);
        INFO(key);
        REQUIRE(found != stats.end());
        return found->value;
    };
    const auto history = [](const ec::screen_view& screen) {
        return body<ec::list_body>(part(screen, "editor:history")).rows;
    };

    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto state = captured();
        const auto screen = ec::top_screen(state);
        switch (step++) {
            case 0:
                return invoke("editor:open", {.text = file.path.string()});
            case 1: {
                const auto& tools = body<ec::tabs_body>(part(screen, "editor:tools")).tabs;
                REQUIRE(tools.size() == 11);
                CHECK(tools[5].id == "tool:room");
                CHECK(stat(screen, "inside") == "# plastered wooden wall");
                CHECK(stat(screen, "brush") == "1");
                CHECK(stat(screen, "mirror_x") == "Off");
                CHECK(affordance(screen, "editor:undo").keys == std::vector<std::string>{"CTRL+Z"});
                CHECK(affordance(screen, "editor:redo").keys == std::vector<std::string>{"CTRL+Y"});
                CHECK(affordance(screen, "editor:save").keys == std::vector<std::string>{"CTRL+S"});
                CHECK(refused(state, "editor:brush", {.count = 10}));
                CHECK(refused(state, "editor:brush", {.count = 0}));
                CHECK(history(screen).size() == 1);
                CHECK(history(screen)[0].selected);
                CHECK(history(screen)[0].description == "Saved");
                return invoke("editor:symbol", {.row = "symbol:."});
            }
            case 2:
                return invoke("editor:tool", {.row = "tool:room"});
            case 3:
                return invoke("editor:paint", {.row = "square:2,2"});
            case 4:
                return invoke("editor:paint", {.row = "square:6,5"});
            case 5:
                // The outline in the floor symbol, the inside in the wall symbol, in one step.
                CHECK(walls(screen) == 6);
                CHECK(cell(screen, 3, 3).glyph == "#");
                CHECK(cell(screen, 2, 2).glyph == ".");
                CHECK(history(screen).size() == 2);
                CHECK(history(screen)[1].selected);
                CHECK(history(screen)[0].description == "Saved");
                // A step says what it changed in words, not as a JSON pointer.
                CHECK(history(screen)[0].label == "Opened");
                CHECK(history(screen)[1].label == "Change rows");
                return invoke("editor:jump", {.row = "step:0"});
            case 6:
                CHECK(walls(screen) == 0);
                CHECK_FALSE(affordance(screen, "editor:undo").enabled);
                CHECK(affordance(screen, "editor:redo").enabled);
                CHECK(saved(screen) == "Yes");
                CHECK(history(screen).size() == 2);
                CHECK(history(screen)[0].selected);
                return invoke("editor:jump", {.row = "step:1"});
            case 7:
                CHECK(walls(screen) == 6);
                CHECK(saved(screen) == "No");
                return invoke("editor:symbol", {.row = "symbol:#"});
            case 8:
                return invoke("editor:tool", {.row = "tool:point"});
            case 9:
                return invoke("editor:brush", {.count = 2});
            case 10:
                CHECK(stat(screen, "brush") == "2");
                return invoke("editor:paint", {.row = "square:10,10"});
            case 11:
                CHECK(walls(screen) == 10);
                return invoke("editor:mirror_x");
            case 12:
                CHECK(stat(screen, "mirror_x") == "On");
                return invoke("editor:paint", {.row = "square:1,15"});
            case 13:
                // Two columns of two squares, and their mirror image across the middle column.
                CHECK(walls(screen) == 18);
                CHECK(cell(screen, 22, 15).glyph == "#");
                CHECK(cell(screen, 21, 16).glyph == "#");
                CHECK(history(screen).size() == 4);
                return invoke("editor:jump", {.row = "step:2"});
            case 14:
                CHECK(walls(screen) == 10);
                CHECK(history(screen).size() == 4);
                CHECK(history(screen)[2].selected);
                CHECK(refused(state, "editor:jump", {.row = "step:4"}));
                return invoke("editor:save");
            case 15:
                CHECK(history(screen)[2].description == "Saved");
                CHECK(history(screen)[0].description.empty());
                // An edit after going back drops the steps undone, and the saved state with them
                // once it lay among them.
                return invoke("editor:jump", {.row = "step:1"});
            case 16:
                return invoke("editor:paint", {.row = "square:0,0"});
            case 17:
                CHECK(history(screen).size() == 3);
                CHECK(std::ranges::none_of(history(screen), [](const auto& row) {
                    return row.description == "Saved";
                }));
                return act("QUIT");
            case 18:
                // Unsaved, so leaving asks.
                return answer("Leave without saving");
            default:
                FAIL("the editor outlived its exit");
                return act("QUIT");
        }
    });

    editor::run_editor_session();
    const auto rows = nlohmann::ordered_json::parse(read_file(file.path))[0]["object"]["rows"];
    CHECK(rows[10].get<std::string>() == std::string(10, '.') + "##" + std::string(12, '.'));
    // The file holds the state saved, at the second step, and not the edit after it.
    CHECK(rows[15].get<std::string>() == std::string(24, '.'));
    CHECK(rows[0].get<std::string>() == std::string(24, '.'));
}

TEST_CASE("a new game forgets that the editor waits to open again", "[editor_session]") {
    const auto guard = screen_guard{};
    game_client::memory::set_input_provider([](const int /*timeout*/) {
        return invoke("editor:advance", {.count = 5});
    });
    editor::run_editor_session();
    get_avatar().cancel_activity();
    editor::reset_editor_session();
    CHECK_FALSE(editor::reopens_after_waiting());
}

TEST_CASE(
    "the editor lets turns pass as the avatar waits and opens again after them",
    "[editor_session][engine_client_component]") {
    const auto guard = screen_guard{};
    auto step = 0;
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        switch (step++) {
            case 0:
                return invoke("editor:advance", {.count = 5});
            default:
                FAIL("the editor outlived its wait");
                return act("QUIT");
        }
    });
    editor::run_editor_session();
    CHECK(step == 1);
    const auto& waiting = get_avatar().activity;
    REQUIRE(waiting);
    CHECK(waiting->id() == activity_id("ACT_WAIT"));
    CHECK(waiting->get_moves_left() == to_moves<int>(5_turns));
    CHECK_FALSE(editor::reopens_after_waiting());

    get_avatar().cancel_activity();
    calendar::turn += 5_turns;
    CHECK(editor::reopens_after_waiting());
    CHECK_FALSE(editor::reopens_after_waiting());
    // Leaving at once forgets the message too.
    auto shown = std::string();
    auto visits = 0;
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto screen = ec::top_screen(captured());
        const auto* const found = ec::find_component(*screen.root, "editor:status");
        shown = found ? body<ec::text_body>(*found).markup : std::string();
        ++visits;
        return act("QUIT");
    });
    editor::run_editor_session();
    CHECK(shown == "5 turns passed.");
    editor::run_editor_session();
    CHECK(shown.empty());
    CHECK(visits == 2);

    game_client::memory::set_input_provider([](const int /*timeout*/) {
        return invoke("editor:advance", {.count = 5});
    });
    editor::run_editor_session();
    get_avatar().cancel_activity();
    calendar::turn += 5_turns;
    REQUIRE(editor::reopens_after_waiting());

    const auto waited = [](const ec::screen_view& screen) {
        const auto* const found = ec::find_component(*screen.root, "editor:status");
        return found ? body<ec::text_body>(*found).markup : std::string();
    };
    step = 0;
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto screen = ec::top_screen(captured());
        switch (step++) {
            case 0:
                CHECK(waited(screen) == "5 turns passed.");
                return invoke("editor:export_width", {.count = 1});
            case 1:
                CHECK(waited(screen).empty());
                return act("QUIT");
            default:
                FAIL("the editor outlived its exit");
                return act("QUIT");
        }
    });
    editor::run_editor_session();
    CHECK(step == 2);
    editor::reset_editor_session();
}

TEST_CASE(
    "the editor stamps pasted mapgen text into the world and takes it back",
    "[editor_session][engine_client_component]") {
    const auto guard = screen_guard{};
    auto step = 0;
    const auto omt = get_avatar().abs_omt_pos();
    const auto live = [&] {
        return get_map().ter(abs_to_bub(project_to<coords::ms>(omt) + tripoint_rel_ms(4, 5, 0)));
    };
    const auto before = live();
    auto walls = nlohmann::ordered_json{
        {"type", "mapgen"},
        {"method", "json"},
        {"om_terrain",
         get_overmapbuffer(get_map().get_bound_dimension()).ter(omt)->get_type_id().str()},
        {"object",
         {{"rows", std::vector<std::string>(24, std::string(24, '#'))},
          {"terrain", {{"#", "t_wall"}}}}}};
    const auto notice = [](const ec::screen_view& screen) {
        const auto* const found = ec::find_component(*screen.root, "editor:notice");
        return found ? body<ec::text_body>(*found).markup : std::string();
    };

    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto screen = ec::top_screen(captured());
        switch (step++) {
            case 0:
                return invoke("editor:import_set", {.row = "import", .text = "{ not json"});
            case 1:
                // Setting the text keeps it in the form and stamps nothing yet.
                CHECK(body<ec::form_body>(part(screen, "editor:import_form")).fields[0].value
                      == "{ not json");
                CHECK(live() == before);
                return invoke("editor:import");
            case 2: {
                // The refusal says what is wrong in words, not as the loader names its input.
                CHECK(notice(screen) == "The pasted text is not JSON.");
                CHECK(live() == before);
                auto methodless = walls;
                methodless.erase("method");
                return invoke("editor:import_set", {.row = "import", .text = methodless.dump()});
            }
            case 3:
                return invoke("editor:import");
            case 4:
                CHECK(notice(screen) == "The \"method\" field is missing.");
                CHECK(live() == before);
                return invoke("editor:import_set",
                              {.row = "import", .text = R"([{ "type": "palette", "id": "x" }])"});
            case 5:
                return invoke("editor:import");
            case 6:
                CHECK(notice(screen) == "The pasted text holds no mapgen entry.");
                // A file's array of entries, as an export writes it, stamps its mapgen.
                return invoke(
                    "editor:import_set",
                    {.row = "import", .text = nlohmann::ordered_json::array({walls}).dump()});
            case 7:
                return invoke("editor:import");
            case 8:
                CHECK(notice(screen).empty());
                CHECK(body<ec::text_body>(part(screen, "editor:status")).markup == "Stamped.");
                CHECK(live() == ter_id("t_wall"));
                CHECK(affordance(screen, "editor:unstamp").enabled);
                return invoke("editor:unstamp");
            case 9:
                CHECK(live() == before);
                return act("QUIT");
            default:
                FAIL("the editor outlived its exit");
                return act("QUIT");
        }
    });

    editor::run_editor_session();
    CHECK(step == 10);
    editor::reset_editor_session();
}

TEST_CASE(
    "the editor applies the entry to the game, stamps it into the world, takes it back and tests it",
    "[editor_session][engine_client_component]") {
    const auto guard = screen_guard{};
    const auto file = mod_file{};
    auto step = 0;
    const auto omt = get_avatar().abs_omt_pos();
    const auto live = [&](const int x, const int y) {
        return get_map().ter(abs_to_bub(project_to<coords::ms>(omt) + tripoint_rel_ms(x, y, 0)));
    };
    const auto before = live(2, 3);
    const auto stamped = [](const ec::screen_view& screen) {
        const auto& stats = body<ec::panel_body>(part(screen, "editor:document")).stats;
        return std::ranges::contains(stats, std::string("stamped"), &ec::panel_stat::key);
    };
    const auto status = [](const ec::screen_view& screen) {
        const auto* const found = ec::find_component(*screen.root, "editor:status");
        return found ? std::get<ec::text_body>(found->body).markup : std::string{};
    };

    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto state = captured();
        const auto screen = ec::top_screen(state);
        switch (step++) {
            case 0:
                return invoke("editor:open", {.text = file.path.string()});
            case 1:
                CHECK_FALSE(affordance(screen, "editor:unstamp").enabled);
                // Outside the editor's own world, testing needs a stamp: it never writes over the
                // player's map unasked.
                CHECK_FALSE(affordance(screen, "editor:test").enabled);
                // What is off says why.
                CHECK(affordance(screen, "editor:test").denial
                      == "Stamp the entry first: Test plays what was stamped.");
                CHECK(affordance(screen, "editor:unstamp").denial == "Nothing is stamped.");
                CHECK(affordance(screen, "editor:copy").denial
                      == "Mark squares with the Select tool first.");
                CHECK(affordance(screen, "editor:cut").denial
                      == "Mark squares with the Select tool first.");
                for (const auto* const id : {"editor:rotate", "editor:flip_x", "editor:flip_y"}) {
                    INFO(id);
                    CHECK(affordance(screen, id).denial == "Copy or cut squares first.");
                }
                CHECK_FALSE(stamped(screen));
                return invoke("editor:paint", {.row = "square:2,3"});
            case 2:
                return invoke("editor:apply");
            case 3:
                CHECK_FALSE(ec::find_component(*screen.root, "editor:notice"));
                // Each action of the World menu says what it did.
                CHECK(status(screen) == "The game generates house_04 with this entry now.");
                CHECK(live(2, 3) == before);
                return invoke("editor:stamp");
            case 4:
                CHECK_FALSE(ec::find_component(*screen.root, "editor:notice"));
                CHECK(status(screen) == "Stamped over the overmap square you stand on.");
                CHECK(live(2, 3) == ter_id("t_wall"));
                CHECK(live(3, 3) == ter_id("t_floor"));
                CHECK(stamped(screen));
                CHECK(affordance(screen, "editor:unstamp").enabled);
                return invoke("editor:unstamp");
            case 5:
                CHECK(live(2, 3) == before);
                CHECK(status(screen) == "Put back what the stamp replaced.");
                CHECK_FALSE(stamped(screen));
                return invoke("editor:stamp");
            case 6:
                // Stamping again replaces the stamp rather than stacking on it.
                CHECK(live(2, 3) == ter_id("t_wall"));
                return invoke("editor:test");
            default:
                FAIL("the editor outlived its test");
                return act("QUIT");
        }
    });

    editor::run_editor_session();
    // The avatar plays in the middle of the stamped square.
    CHECK(get_avatar().abs_omt_pos() == omt);
    CHECK(get_avatar().abs_pos() == project_to<coords::ms>(omt) + tripoint_rel_ms(SEEX, SEEY, 0));
    CHECK(step == 7);
    editor::reset_editor_session();
}

TEST_CASE(
    "the editor opens the mapgen of an overmap terrain, and any data file from its browser",
    "[editor_session][engine_client_component]") {
    const auto guard = screen_guard{};
    const auto restore_mapgen = restore_on_out_of_scope<bool>(disable_mapgen);
    disable_mapgen = false;
    auto step = 0;
    const auto path = [](const ec::screen_view& screen) {
        return body<ec::form_body>(part(screen, "editor:file")).fields.front().value;
    };
    const auto objects = [](const ec::screen_view& screen) {
        return body<ec::list_body>(part(screen, "editor:objects")).rows;
    };
    const auto files = [](const ec::screen_view& screen) {
        return body<ec::list_body>(part(screen, "editor:browser"));
    };

    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto state = captured();
        const auto screen = ec::top_screen(state);
        switch (step++) {
            case 0:
                // The browser lists the data files a page at a time, 50 of them.
                CHECK(files(screen).rows.size() == 50);
                CHECK(files(screen).total > 50);
                CHECK(files(screen).rows[0].id.starts_with("file:"));
                return invoke("editor:open_oter", {.text = "no_such_overmap_terrain"});
            case 1:
                CHECK(body<ec::text_body>(part(screen, "editor:notice"))
                          .markup.find("no_such_overmap_terrain")
                      != std::string::npos);
                CHECK_FALSE(has_affordance(screen, "editor:paint"));
                return invoke("editor:open_oter", {.text = "2StoryModern06_1_north"});
            case 2: {
                // Its file is open on the entry that makes it, even when the file holds many.
                CHECK(path(screen).ends_with(".json"));
                CHECK(path(screen).ends_with("mapgen/house/2storymodern06.json"));
                const auto entries = objects(screen);
                const auto selected = std::ranges::find_if(entries, &ec::row::selected);
                REQUIRE(selected != entries.end());
                CHECK(selected->label.find("2StoryModern06_1") != std::string::npos);
                return invoke("editor:browser_filter", {.text = "2STORYMODERN06.JSON"});
            }
            case 3:
                // The filter ignores case and matches anywhere in the path.
                REQUIRE(files(screen).rows.size() == 1);
                CHECK(files(screen).total == 1);
                CHECK(files(screen).filter == "2STORYMODERN06.JSON");
                CHECK(std::ranges::contains(
                    part(screen, "editor:browser").affordances,
                    std::string("editor:browser_filter")));
                CHECK(std::ranges::contains(
                    part(screen, "editor:browser").affordances,
                    std::string("editor:browser_page")));
                CHECK(files(screen).rows[0].label == "json/mapgen/house/2storymodern06.json");
                CHECK(refused(state, "editor:browser_page", {.count = 1}));
                return invoke("editor:browser_open", {.row = files(screen).rows[0].id});
            case 4:
                CHECK(path(screen).ends_with("mapgen/house/2storymodern06.json"));
                // The heading names the file as a modder finds it in the game's directory.
                CHECK(body<ec::panel_body>(part(screen, "editor:document")).title
                      == "data/json/mapgen/house/2storymodern06.json");
                return invoke("editor:browser_filter", {.text = "no such path at all"});
            case 5:
                CHECK(files(screen).rows.empty());
                return act("QUIT");
            default:
                FAIL("the editor outlived its exit");
                return act("QUIT");
        }
    });

    editor::run_editor_session();
    CHECK(step == 6);
}

TEST_CASE(
    "the editor opens the mapgen of an overmap terrain named by its type, building or name",
    "[editor_session][engine_client_component]") {
    const auto guard = screen_guard{};
    const auto restore_mapgen = restore_on_out_of_scope<bool>(disable_mapgen);
    disable_mapgen = false;
    auto step = 0;
    const auto path = [](const ec::screen_view& screen) {
        return body<ec::form_body>(part(screen, "editor:file")).fields.front().value;
    };
    const auto notice = [](const ec::screen_view& screen) {
        const auto* const found = ec::find_component(*screen.root, "editor:notice");
        return found ? body<ec::text_body>(*found).markup : std::string();
    };
    const auto status = [](const ec::screen_view& screen) {
        const auto* const found = ec::find_component(*screen.root, "editor:status");
        return found ? body<ec::text_body>(*found).markup : std::string();
    };

    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto screen = ec::top_screen(captured());
        switch (step++) {
            case 0:
                // A type without its direction, as the mapgen's om_terrain names it.
                return invoke("editor:open_oter", {.text = "2StoryModern06_1"});
            case 1:
                CHECK(notice(screen).empty());
                CHECK(path(screen).ends_with("mapgen/house/2storymodern06.json"));
                // A building opens the mapgen of its ground square.
                return invoke("editor:open_oter", {.text = "house_two_story_basement"});
            case 2:
                CHECK(notice(screen).empty());
                CHECK(path(screen).ends_with("mapgen/house/house_2story.json"));
                CHECK(status(screen).find("house_2story_base") != std::string::npos);
                // The name the overmap shows opens the first terrain of that name.
                return invoke("editor:open_oter", {.text = "house"});
            case 3:
                CHECK(notice(screen).empty());
                CHECK(path(screen).find("mapgen/") != std::string::npos);
                CHECK(status(screen).find("named house") != std::string::npos);
                return act("QUIT");
            default:
                FAIL("the editor outlived its exit");
                return act("QUIT");
        }
    });
    editor::run_editor_session();
    CHECK(step == 4);
    editor::reset_editor_session();
}

TEST_CASE(
    "the editor's browser lists only the files holding what it edits",
    "[editor_session][engine_client_component]") {
    const auto guard = screen_guard{};
    const auto file = mod_file{};
    {
        auto out = std::ofstream(file.path.parent_path() / "items.json", std::ios::binary);
        out << R"([{"type": "GENERIC", "id": "editor_session_test_item", "name": "thing"}])";
    }
    auto step = 0;
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto screen = ec::top_screen(captured());
        switch (step++) {
            case 0:
                return invoke("editor:browser_filter", {.text = "editor_session_test"});
            case 1: {
                const auto& rows = body<ec::list_body>(part(screen, "editor:browser")).rows;
                REQUIRE(rows.size() == 1);
                CHECK(rows[0].id == "file:" + file.path.generic_string());
                return act("QUIT");
            }
            default:
                FAIL("the editor outlived its exit");
                return act("QUIT");
        }
    });
    editor::run_editor_session();
    CHECK(step == 2);
}

TEST_CASE(
    "the editor creates a nested chunk file in the user mods and opens it",
    "[editor_session][engine_client_component]") {
    const auto guard = screen_guard{};
    const auto folder = mod_file{};
    const auto made = folder.path.parent_path() / "chunk.json";
    const auto notice = [](const ec::screen_view& screen) {
        return body<ec::text_body>(part(screen, "editor:notice")).markup;
    };
    auto step = 0;

    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto screen = ec::top_screen(captured());
        switch (step++) {
            case 0: {
                const auto& fields = body<ec::form_body>(part(screen, "editor:new")).fields;
                const auto kind = std::ranges::find(fields, "kind", &ec::form_field::id);
                REQUIRE(kind != fields.end());
                CHECK(kind->value == "mapgen");
                CHECK(kind->options.size() == 5);
                return invoke("editor:new_set", {.row = "path", .text = made.string()});
            }
            case 1:
                return invoke("editor:new_set", {.row = "om_terrain", .text = "editor_test_chunk"});
            case 2:
                return invoke("editor:new_set", {.row = "kind", .text = "vehicle"});
            case 3:
                return invoke("editor:new_apply");
            case 4:
                CHECK(notice(screen)
                      == "A new file holds a mapgen, a nested chunk, a palette, an overmap special "
                         "or a building.");
                CHECK_FALSE(std::filesystem::exists(made));
                return invoke("editor:new_set", {.row = "kind", .text = "nested"});
            case 5:
                return invoke("editor:new_set", {.row = "size", .text = "25"});
            case 6:
                return invoke("editor:new_apply");
            case 7:
                CHECK(notice(screen) == "A chunk is 1 to 24 squares wide.");
                CHECK_FALSE(std::filesystem::exists(made));
                return invoke("editor:new_set", {.row = "size", .text = "6"});
            case 8:
                return invoke("editor:new_apply");
            case 9: {
                CHECK_FALSE(ec::find_component(*screen.root, "editor:notice"));
                const auto& objects = body<ec::list_body>(part(screen, "editor:objects")).rows;
                REQUIRE(objects.size() == 1);
                CHECK(objects[0].label == "editor_test_chunk");
                const auto& canvas = body<ec::grid_body>(part(screen, "editor:canvas"));
                CHECK(canvas.cols == 6);
                CHECK(canvas.rows == 6);
                CHECK(body<ec::list_body>(part(screen, "editor:diagnostics")).rows.empty());
                return act("QUIT");
            }
            default:
                FAIL("the editor outlived its exit");
                return act("QUIT");
        }
    });

    editor::run_editor_session();
    CHECK(step == 10);
    const auto written = nlohmann::ordered_json::parse(read_file(made));
    CHECK(written[0]["nested_mapgen_id"] == "editor_test_chunk");
    CHECK_FALSE(written[0].contains("om_terrain"));
    CHECK(written[0]["object"]["mapgensize"] == nlohmann::ordered_json::array({6, 6}));
    CHECK(written[0]["object"]["rows"].size() == 6);
    CHECK(written[0]["object"]["rows"][5] == "......");
    CHECK(format_json(read_file(made)) == read_file(made));
}

TEST_CASE(
    "a mapgen reads the palettes of its own file as they are edited",
    "[editor_session][editor_palette]") {
    const auto guard = screen_guard{};
    auto rows = nlohmann::ordered_json::array();
    for (auto row = 0; row < 24; ++row) { rows.push_back(std::string(24, row == 0 ? 'x' : '.')); }
    const auto file = mod_file{{{"palettes", {"editor_session_test_palette"}}, {"rows", rows}}};
    {
        // The file's palette, which the game never loaded.
        auto entries = nlohmann::ordered_json::parse(file.text);
        entries.push_back(
            {{"type", "palette"},
             {"id", "editor_session_test_palette"},
             {"terrain", {{"x", "t_dirt"}}}});
        auto out = std::ofstream(file.path, std::ios::binary | std::ios::trunc);
        out << format_json(entries.dump()).value();
    }
    const auto line = [](const ec::screen_view& screen, const std::string& symbol) {
        const auto& rows = body<ec::list_body>(part(screen, "editor:legend")).rows;
        const auto found = std::ranges::find(rows, "symbol:" + symbol, &ec::row::id);
        REQUIRE(found != rows.end());
        return *found;
    };
    const auto messages = [](const ec::screen_view& screen) {
        return body<ec::list_body>(part(screen, "editor:diagnostics")).rows
             | std::views::transform(&ec::row::label) | std::ranges::to<std::vector>();
    };
    auto step = 0;

    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto screen = ec::top_screen(captured());
        switch (step++) {
            case 0:
                return invoke("editor:open", {.text = file.path.string()});
            case 1:
                CHECK(messages(screen).empty());
                CHECK(line(screen, "x").description == "From editor_session_test_palette");
                CHECK(column_value(line(screen, "x"), "terrain") == "dirt");
                return invoke("editor:object", {.row = "object:1"});
            case 2:
                return invoke("editor:mapping_set", {.row = "symbol", .text = "x"});
            case 3:
                return invoke("editor:mapping_set", {.row = "value", .text = "t_grass"});
            case 4:
                return invoke("editor:mapping_apply");
            case 5:
                return invoke("editor:object", {.row = "object:0"});
            case 6:
                // The mapgen reads the palette as edited, before it is saved.
                CHECK(column_value(line(screen, "x"), "terrain") == "grass");
                CHECK(messages(screen).empty());
                return act("QUIT");
            case 7:
                return answer("Leave without saving");
            default:
                FAIL("the editor outlived its exit");
                return act("QUIT");
        }
    });

    editor::run_editor_session();
    CHECK(step == 8);
    // The game's palettes are as it loaded them.
    CHECK_FALSE(palette_id("editor_session_test_palette").is_valid());
}

TEST_CASE(
    "the editor creates a palette file in the user mods and opens it on its legend",
    "[editor_session][editor_palette]") {
    const auto guard = screen_guard{};
    const auto folder = mod_file{};
    const auto made = folder.path.parent_path() / "palette.json";
    const auto notice = [](const ec::screen_view& screen) {
        return body<ec::text_body>(part(screen, "editor:notice")).markup;
    };
    const auto label = [](const ec::screen_view& screen, const std::string& id) {
        const auto& fields = body<ec::form_body>(part(screen, "editor:new")).fields;
        const auto found = std::ranges::find(fields, id, &ec::form_field::id);
        REQUIRE(found != fields.end());
        return found->label;
    };
    auto step = 0;

    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto screen = ec::top_screen(captured());
        switch (step++) {
            case 0:
                CHECK(label(screen, "om_terrain") == "Overmap terrain");
                return invoke("editor:new_set", {.row = "path", .text = made.string()});
            case 1:
                return invoke("editor:new_set", {.row = "kind", .text = "palette"});
            case 2:
                // The id field names what a palette is known by.
                CHECK(label(screen, "om_terrain") == "Palette id");
                return invoke("editor:new_set", {.row = "om_terrain", .text = "cabin_palette"});
            case 3:
                return invoke("editor:new_apply");
            case 4:
                CHECK(notice(screen) == "cabin_palette is a palette already.");
                CHECK_FALSE(std::filesystem::exists(made));
                return invoke(
                    "editor:new_set", {.row = "om_terrain", .text = "editor_test_palette"});
            case 5:
                return invoke("editor:new_apply");
            case 6: {
                CHECK_FALSE(ec::find_component(*screen.root, "editor:notice"));
                const auto& objects = body<ec::list_body>(part(screen, "editor:objects")).rows;
                REQUIRE(objects.size() == 1);
                CHECK(objects[0].label == "editor_test_palette");
                CHECK(body<ec::list_body>(part(screen, "editor:legend")).rows.front().id
                      == "symbol:.");
                CHECK(body<ec::list_body>(part(screen, "editor:diagnostics")).rows.empty());
                CHECK(screen.focus_component == "editor:legend");
                return act("QUIT");
            }
            default:
                FAIL("the editor outlived its exit");
                return act("QUIT");
        }
    });

    editor::run_editor_session();
    CHECK(step == 7);
    const auto written = nlohmann::ordered_json::parse(read_file(made));
    CHECK(
        written
        == nlohmann::ordered_json::parse(
            R"([{"type": "palette", "id": "editor_test_palette", "terrain": {".": "t_floor"}}])"));
    CHECK(format_json(read_file(made)) == read_file(made));
}

TEST_CASE(
    "the editor copies a selection, rotates it and pastes it as one undo step",
    "[editor_session][engine_client_component]") {
    const auto guard = screen_guard{};
    const auto file = mod_file{};
    auto step = 0;
    const auto clipboard = [](const ec::screen_view& screen) {
        const auto& stats = body<ec::panel_body>(part(screen, "editor:document")).stats;
        const auto found = std::ranges::find(stats, "clipboard", &ec::panel_stat::key);
        return found == stats.end() ? std::string{} : found->value;
    };

    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto state = captured();
        const auto screen = ec::top_screen(state);
        switch (step++) {
            case 0:
                return invoke("editor:open", {.text = file.path.string()});
            case 1:
                CHECK(clipboard(screen).empty());
                CHECK_FALSE(affordance(screen, "editor:copy").enabled);
                CHECK_FALSE(affordance(screen, "editor:rotate").enabled);
                return invoke("editor:tool", {.row = "tool:line"});
            case 2:
                return invoke("editor:paint", {.row = "square:1,1"});
            case 3:
                return invoke("editor:paint", {.row = "square:3,1"});
            case 4:
                CHECK(walls(screen) == 3);
                return invoke("editor:tool", {.row = "tool:select"});
            case 5:
                return invoke("editor:paint", {.row = "square:3,1"});
            case 6:
                return invoke("editor:paint", {.row = "square:1,2"});
            case 7:
                // The selection is the rectangle between the two squares, two rows of three.
                CHECK(cell(screen, 2, 2).selected);
                CHECK_FALSE(cell(screen, 4, 2).selected);
                CHECK(affordance(screen, "editor:copy").enabled);
                return invoke("editor:copy");
            case 8:
                CHECK(clipboard(screen) == "3x2");
                CHECK(affordance(screen, "editor:rotate").enabled);
                return invoke("editor:rotate");
            case 9:
                CHECK(clipboard(screen) == "2x3");
                return invoke("editor:tool", {.row = "tool:paste"});
            case 10:
                return invoke("editor:paint", {.row = "square:10,10"});
            case 11:
                // A quarter turn clockwise: the wall row of the top is the right column.
                CHECK(walls(screen) == 6);
                CHECK(cell(screen, 11, 10).glyph == "#");
                CHECK(cell(screen, 11, 11).glyph == "#");
                CHECK(cell(screen, 11, 12).glyph == "#");
                CHECK(cell(screen, 10, 10).glyph == ".");
                return invoke("editor:undo");
            case 12:
                CHECK(walls(screen) == 3);
                return invoke("editor:paint", {.row = "square:23,23"});
            case 13:
                // The part of the paste beyond the rows is dropped.
                CHECK(walls(screen) == 3);
                return invoke("editor:paint", {.row = "square:22,22"});
            case 14:
                CHECK(walls(screen) == 5);
                CHECK(cell(screen, 23, 22).glyph == "#");
                return act("QUIT");
            case 15:
                return answer("Leave without saving");
            default:
                FAIL("the editor outlived its exit");
                return act("QUIT");
        }
    });

    editor::run_editor_session();
    CHECK(step == 16);
}

TEST_CASE(
    "the editor exports the overmap squares around the avatar as one entry with its palette",
    "[editor_session][engine_client_component]") {
    const auto guard = screen_guard{};
    const auto folder = mod_file{};
    const auto made = folder.path.parent_path() / "export_region.json";
    auto step = 0;

    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto screen = ec::top_screen(captured());
        switch (step++) {
            case 0:
                return invoke("editor:export_width", {.count = 2});
            case 1:
                return invoke("editor:export_height", {.count = 3});
            case 2:
                return invoke("editor:export_palette", {.text = "lab_workspace_palette"});
            case 3:
                CHECK(body<ec::form_body>(part(screen, "editor:export_options")).fields[0].value
                      == "lab_workspace_palette");
                return invoke("editor:export", {.text = made.string()});
            case 4: {
                const auto& canvas = body<ec::grid_body>(part(screen, "editor:canvas"));
                CHECK(canvas.cols == 48);
                CHECK(canvas.rows == 72);
                return act("QUIT");
            }
            default:
                FAIL("the editor outlived its exit");
                return act("QUIT");
        }
    });

    editor::run_editor_session();
    CHECK(step == 5);
    const auto written = nlohmann::ordered_json::parse(read_file(made));
    CHECK(written[0]["object"]["palettes"]
          == nlohmann::ordered_json::array({"lab_workspace_palette"}));
    auto& overmap = get_overmapbuffer(get_map().get_bound_dimension());
    // A width of 2 starts at the avatar's square, a height of 3 one square north.
    const auto corner = get_avatar().abs_omt_pos() + point_rel_omt(0, -1);
    const auto& rows = written[0]["om_terrain"];
    REQUIRE(rows.size() == 3);
    CHECK(rows[0].size() == 2);
    CHECK(rows[0][0].get<std::string>() == overmap.ter(corner)->get_type_id().str());
    CHECK(rows[2][1].get<std::string>()
          == overmap.ter(corner + point_rel_omt(1, 2))->get_type_id().str());
}

TEST_CASE(
    "the editor exports the squares the player marks in the world as a nested chunk",
    "[editor_session][engine_client_component]") {
    const auto guard = screen_guard{};
    const auto folder = mod_file{};
    const auto made = folder.path.parent_path() / "marked.json";
    const auto export_label = [](const ec::screen_view& screen) {
        return affordance(screen, "editor:export").label;
    };
    auto step = 0;

    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto screen = ec::top_screen(captured());
        switch (step++) {
            case 0:
                CHECK(export_label(screen) == "Export mapgen here");
                return invoke("editor:export_area");
            // The game's own area selection: the first corner on the avatar, the second 2 east and
            // 1 south.
            case 1:
                return act("CONFIRM");
            case 2:
                return act("RIGHT");
            case 3:
                return act("RIGHT");
            case 4:
                return act("DOWN");
            case 5:
                return act("CONFIRM");
            case 6:
                CHECK(export_label(screen) == "Export 3x2 squares");
                return invoke("editor:export", {.text = made.string()});
            case 7: {
                const auto& canvas = body<ec::grid_body>(part(screen, "editor:canvas"));
                CHECK(canvas.cols == 3);
                CHECK(canvas.rows == 2);
                // The overmap square size goes back to the squares around the avatar.
                return invoke("editor:export_width", {.count = 1});
            }
            case 8:
                CHECK(export_label(screen) == "Export mapgen here");
                return act("QUIT");
            default:
                FAIL("the editor outlived its exit");
                return act("QUIT");
        }
    });

    editor::run_editor_session();
    CHECK(step == 9);
    const auto written = nlohmann::ordered_json::parse(read_file(made));
    CHECK(written[0]["nested_mapgen_id"] == "marked");
    CHECK(written[0]["object"]["mapgensize"] == nlohmann::ordered_json::array({3, 2}));
}

TEST_CASE(
    "the editor exports the overmap square under the avatar to a new file and opens it",
    "[editor_session][engine_client_component]") {
    const auto guard = screen_guard{};
    const auto folder = mod_file{};
    const auto made = folder.path.parent_path() / "export.json";
    const auto notice = [](const ec::screen_view& screen) {
        return body<ec::text_body>(part(screen, "editor:notice")).markup;
    };
    auto step = 0;

    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto screen = ec::top_screen(captured());
        switch (step++) {
            case 0:
                return invoke("editor:export", {.text = "/tmp/editor_export_test.json"});
            case 1:
                CHECK(notice(screen).starts_with("A new file goes in your mods"));
                CHECK_FALSE(std::filesystem::exists("/tmp/editor_export_test.json"));
                return invoke("editor:export", {.text = made.string()});
            case 2: {
                CHECK_FALSE(ec::find_component(*screen.root, "editor:notice"));
                CHECK(body<ec::list_body>(part(screen, "editor:objects")).rows.size() == 1);
                const auto& canvas = body<ec::grid_body>(part(screen, "editor:canvas"));
                CHECK(canvas.cols == 24);
                CHECK(canvas.rows == 24);
                CHECK(body<ec::list_body>(part(screen, "editor:diagnostics")).rows.empty());
                return invoke("editor:export", {.text = made.string()});
            }
            case 3:
                // A file that exists is not overwritten.
                CHECK(notice(screen).ends_with("already exists."));
                return act("QUIT");
            default:
                FAIL("the editor outlived its exit");
                return act("QUIT");
        }
    });

    editor::run_editor_session();
    CHECK(step == 4);
    const auto written = nlohmann::ordered_json::parse(read_file(made));
    CHECK(written[0]["type"] == "mapgen");
    CHECK(
        written[0]["om_terrain"].get<std::string>()
        == get_overmapbuffer(get_map().get_bound_dimension())
               .ter(get_avatar().abs_omt_pos())
               ->get_type_id()
               .str());
    CHECK(format_json(read_file(made)) == read_file(made));
}

TEST_CASE("game time does not pass while the map editor is open", "[editor_session]") {
    const auto guard = screen_guard{};
    const auto file = mod_file{};
    const auto animations = override_option("ANIMATIONS", "false");
    auto& you = get_avatar();
    you.set_mutation(trait_id("DEBUG_EDITOR"));
    you.set_moves(100);
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
    CHECK(you.get_moves() == 100);
}

TEST_CASE(
    "the debug menu's map editor opens the map editor, and its square editor stays beside it",
    "[editor_session]") {
    const auto guard = screen_guard{};
    get_avatar().set_mutation(trait_id("DEBUG_EDITOR"));
    auto step = 0;

    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto screen = ec::top_screen(captured());
        switch (step++) {
            case 0:
                return answer("Map…");
            case 1: {
                // The editor of the squares around the avatar is offered under its own name.
                const auto snapshot = game_client::current_interaction({.limit = 200});
                CHECK(std::ranges::contains(
                    snapshot.choices, std::string("Edit squares here"),
                    &game_client::interaction_choice::label));
                return answer("Map editor");
            }
            case 2:
                CHECK(screen.kind == "editor");
                return act("QUIT");
            default:
                FAIL("the editor outlived its exit");
                return act("QUIT");
        }
    });

    debug_menu::debug();
    CHECK(step == 3);
}

TEST_CASE(
    "the main-menu editor game is the whole view, tests in the world and closes to the main menu",
    "[editor_session][gamemode]") {
    // Put back after the screen guard, whose test mode keeps clearing the avatar from prompting.
    const auto origin = get_map().get_abs_sub();
    const auto name = get_avatar().name;
    const auto restore = on_out_of_scope([&] {
        g->uquit = QUIT_NO;
        g->load_map(origin);
        get_primary_overmapbuffer().clear();
        clear_all_state();
        get_avatar().name = name;
    });
    const auto guard = screen_guard{};
    const auto file = mod_file{};
    const auto animations = override_option("ANIMATIONS", "false");
    auto mode = get_special_game(special_game_type::EDITOR);
    REQUIRE(mode->init());
    get_avatar().set_moves(100);
    auto step = 0;

    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto screen = ec::top_screen(captured());
        switch (step++) {
            case 0:
                // The map was not asked for input first, and the editor is all there is to see.
                CHECK(screen.kind == "editor");
                CHECK(screen.role == "workspace");
                return invoke("editor:open", {.text = file.path.string()});
            case 1:
                // Testing needs no stamp first: it stamps the mapgen where the avatar is.
                CHECK(affordance(screen, "editor:test").enabled);
                return invoke("editor:test", {});
            default:
                FAIL("the editor outlived its test");
                return act("QUIT");
        }
    });
    handle_action(std::move(mode));
    CHECK(step == 2);
    CHECK(g->uquit == QUIT_NO);
    // The avatar plays the stamped mapgen.
    CHECK(get_map().ter(get_avatar().bub_pos()) == ter_str_id("t_floor"));

    // The editor comes back to the same document, and closing it ends the editor game.
    step = 0;
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto screen = ec::top_screen(captured());
        switch (step++) {
            case 0:
                CHECK(screen.role == "workspace");
                CHECK(body<ec::form_body>(part(screen, "editor:file")).fields.front().value
                      == file.path.generic_string());
                return act("QUIT");
            default:
                FAIL("the editor outlived its exit");
                return act("QUIT");
        }
    });
    handle_action(get_special_game(special_game_type::EDITOR));
    CHECK(step == 1);
    CHECK(g->uquit == QUIT_NOSAVED);
}

TEST_CASE(
    "the editor over a running game is the whole view too and closes to the game",
    "[editor_session][gamemode]") {
    const auto guard = screen_guard{};
    auto step = 0;
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto screen = ec::top_screen(captured());
        ++step;
        CHECK(screen.role == "workspace");
        return act("QUIT");
    });
    editor::run_editor_session();
    CHECK(step == 1);
    CHECK(g->uquit == QUIT_NO);
}

TEST_CASE(
    "the editor opens a palette, edits its legend and names what the loader says of it",
    "[editor_session][editor_palette]") {
    const auto guard = screen_guard{};
    const auto path =
        std::filesystem::path(PATH_INFO::user_moddir()) / "editor_session_test" / "palette.json";
    std::filesystem::create_directories(path.parent_path());
    const auto cleanup = on_out_of_scope([&] { std::filesystem::remove_all(path.parent_path()); });
    {
        auto out = std::ofstream(path, std::ios::binary | std::ios::trunc);
        out << format_json(
                   nlohmann::ordered_json::array(
                       {{{"type", "palette"},
                         {"id", "editor_session_test_palette"},
                         {"palettes", {"no_such_palette"}},
                         {"terrain", {{".", "t_floor"}}},
                         {"furniture", {{"c", "f_chair"}}}}})
                       .dump())
                   .value();
    }
    const auto ids = [](const ec::screen_view& screen, const std::string& list) {
        return body<ec::list_body>(part(screen, list)).rows | std::views::transform(&ec::row::id)
             | std::ranges::to<std::vector>();
    };
    const auto set = [](const std::string& id, const std::string& text) {
        return invoke("editor:mapping_set", {.row = id, .text = text});
    };
    auto step = 0;

    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto screen = ec::top_screen(captured());
        switch (step++) {
            case 0:
                return invoke("editor:open", {.text = path.string()});
            case 1: {
                const auto& objects = body<ec::list_body>(part(screen, "editor:objects")).rows;
                REQUIRE(objects.size() == 1);
                CHECK(objects[0].label == "editor_session_test_palette");
                CHECK(objects[0].selected);
                CHECK(ids(screen, "editor:legend")
                      == std::vector<std::string>{"symbol:.", "symbol:c"});
                // A palette has no rows to draw on.
                CHECK(ec::find_component(*screen.root, "editor:canvas") == nullptr);
                CHECK_FALSE(has_affordance(screen, "editor:paint"));
                // Nor the drawing tools' state, nor symbols the rows leave unused.
                const auto& stats = body<ec::panel_body>(part(screen, "editor:document")).stats;
                CHECK(std::ranges::to<std::vector<std::string>>(
                          stats | std::views::transform(&ec::panel_stat::key))
                      == std::vector<std::string>{"saved"});
                for (const auto& row : body<ec::list_body>(part(screen, "editor:legend")).rows) {
                    CHECK_FALSE(std::ranges::contains(
                        row.columns, std::string("use"), &game_client::interaction_column::key));
                }
                const auto& messages = body<ec::list_body>(part(screen, "editor:diagnostics")).rows;
                CHECK(std::ranges::any_of(messages, [](const ec::row& row) {
                    return row.label.find("no_such_palette") != std::string::npos
                        && row.description == "/palettes/0";
                }));
                return set("symbol", "x");
            }
            case 2:
                return set("value", "t_dirt");
            case 3:
                return invoke("editor:mapping_apply");
            case 4:
                CHECK(ids(screen, "editor:legend")
                      == std::vector<std::string>{"symbol:.", "symbol:c", "symbol:x"});
                return invoke("editor:save");
            case 5: {
                const auto written = nlohmann::ordered_json::parse(read_file(path));
                CHECK(written[0]["terrain"]
                      == nlohmann::ordered_json{{".", "t_floor"}, {"x", "t_dirt"}});
                CHECK_FALSE(written[0].contains("object"));
                return invoke("editor:undo");
            }
            case 6:
                CHECK(ids(screen, "editor:legend")
                      == std::vector<std::string>{"symbol:.", "symbol:c"});
                return invoke("editor:save");
            default:
                return act("QUIT");
        }
    });

    editor::run_editor_session();
    CHECK(step == 8);
}

TEST_CASE(
    "the editor extracts a mapgen's own legend to a palette of its file as one undo step",
    "[editor_session][editor_palette]") {
    const auto guard = screen_guard{};
    const auto file = mod_file{{{"furniture", {{"c", "f_chair"}}}}};
    const auto lines = [](const ec::screen_view& screen) {
        return body<ec::list_body>(part(screen, "editor:legend")).rows
             | std::views::transform([](const ec::row& row) {
                   return row.id + " " + placed_value(row) + " " + row.description;
               })
             | std::ranges::to<std::vector>();
    };
    const auto notice = [](const ec::screen_view& screen) {
        return body<ec::text_body>(part(screen, "editor:notice")).markup;
    };
    auto before = std::vector<std::string>{};
    auto step = 0;

    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto screen = ec::top_screen(captured());
        switch (step++) {
            case 0:
                return invoke("editor:open", {.text = file.path.string()});
            case 1:
                before = lines(screen);
                return invoke("editor:extract_palette", {.text = "cabin_palette"});
            case 2:
                CHECK(notice(screen) == "cabin_palette is a palette already.");
                return invoke("editor:extract_palette", {.text = "editor_session_test_extracted"});
            case 3: {
                // The symbols place what they did, now from the palette.
                CHECK(
                    lines(screen)
                    == std::vector<std::string>{
                        R"(symbol:# plastered wooden wall From editor_session_test_extracted)",
                        R"(symbol:. wooden floor From editor_session_test_extracted)",
                        R"(symbol:c chair From editor_session_test_extracted)"});
                CHECK(body<ec::list_body>(part(screen, "editor:diagnostics")).rows.empty());
                const auto& objects = body<ec::list_body>(part(screen, "editor:objects")).rows;
                REQUIRE(objects.size() == 2);
                CHECK(objects[0].selected);
                CHECK(objects[1].label == "editor_session_test_extracted");
                CHECK(body<ec::list_body>(part(screen, "editor:history")).rows.size() == 2);
                // The step is named for what the player did, not for its last edit.
                CHECK(body<ec::list_body>(part(screen, "editor:history")).rows.back().label
                      == "Extract palette");
                return invoke("editor:save");
            }
            case 4: {
                const auto written = nlohmann::ordered_json::parse(read_file(file.path));
                REQUIRE(written.size() == 2);
                CHECK(written[0]["object"]["palettes"]
                      == nlohmann::ordered_json::array({"editor_session_test_extracted"}));
                CHECK_FALSE(written[0]["object"].contains("terrain"));
                CHECK_FALSE(written[0]["object"].contains("furniture"));
                CHECK(
                    written[1]
                    == nlohmann::ordered_json{
                        {"type", "palette"},
                        {"id", "editor_session_test_extracted"},
                        {"terrain", {{".", "t_floor"}, {"#", "t_wall"}}},
                        {"furniture", {{"c", "f_chair"}}}});
                return invoke("editor:undo");
            }
            case 5:
                CHECK(lines(screen) == before);
                CHECK(body<ec::list_body>(part(screen, "editor:objects")).rows.size() == 1);
                CHECK(body<ec::text_body>(part(screen, "editor:status")).markup
                      == "Undid: Extract palette");
                return invoke("editor:save");
            default:
                return act("QUIT");
        }
    });

    editor::run_editor_session();
    CHECK(step == 7);
    CHECK(read_file(file.path) == file.text);
}

TEST_CASE(
    "a mapgen previews the nested chunks of its own file as they are edited",
    "[editor_session][editor_mapgen]") {
    const auto guard = screen_guard{};
    const auto file = mod_file(nlohmann::ordered_json::parse(
        R"({"place_nested": [{"chunks": ["editor_session_test_chunk"], "x": 2, "y": 6}]})"));
    {
        auto entries = nlohmann::ordered_json::parse(file.text);
        entries.push_back(nlohmann::ordered_json::parse(R"({
            "type": "mapgen",
            "method": "json",
            "nested_mapgen_id": "editor_session_test_chunk",
            "object": {"mapgensize": [1, 1], "rows": ["#"], "terrain": {"#": "t_wall"}}
          })"));
        auto out = std::ofstream(file.path, std::ios::binary | std::ios::trunc);
        out << format_json(entries.dump()).value();
    }
    const auto restore_mapgen = restore_on_out_of_scope<bool>(disable_mapgen);
    disable_mapgen = false;
    const auto shown = [](const ec::screen_view& screen, const int x, const int y) {
        const auto& preview = body<ec::grid_body>(part(screen, "editor:preview"));
        const auto found =
            std::ranges::find(preview.cells, ec::grid_point{x, y}, &ec::grid_cell::at);
        REQUIRE(found != preview.cells.end());
        REQUIRE_FALSE(found->looks.empty());
        return preview.looks.at(found->looks.front()).id;
    };
    const auto set = [](const std::string& id, const std::string& text) {
        return invoke("editor:mapping_set", {.row = id, .text = text});
    };
    auto step = 0;

    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto screen = ec::top_screen(captured());
        switch (step++) {
            case 0:
                return invoke("editor:open", {.text = file.path.string()});
            case 1:
                CHECK(body<ec::list_body>(part(screen, "editor:diagnostics")).rows.empty());
                return invoke("editor:preview", {.count = 1});
            case 2:
                CHECK(shown(screen, 2, 6) == "t_wall");
                CHECK(shown(screen, 3, 6) == "t_floor");
                return invoke("editor:object", {.row = "object:1"});
            case 3:
                return set("symbol", "#");
            case 4:
                return set("value", "t_dirt");
            case 5:
                return invoke("editor:mapping_apply");
            case 6:
                return invoke("editor:object", {.row = "object:0"});
            case 7:
                CHECK(body<ec::list_body>(part(screen, "editor:diagnostics")).rows.empty());
                CHECK(shown(screen, 2, 6) == "t_dirt");
                return invoke("editor:undo");
            case 8:
                return act("QUIT");
            default:
                return answer("Leave without saving");
        }
    });

    editor::run_editor_session();
    CHECK(step == 9);
}

TEST_CASE(
    "an applied mapgen places the nested chunks of its own file as they were applied",
    "[editor_session][editor_mapgen]") {
    const auto guard = screen_guard{};
    const auto file = mod_file(nlohmann::ordered_json::parse(
        R"({"place_nested": [{"chunks": ["editor_session_test_applied"], "x": 2, "y": 6}]})"));
    {
        auto entries = nlohmann::ordered_json::parse(file.text);
        entries.push_back(nlohmann::ordered_json::parse(R"({
            "type": "mapgen",
            "method": "json",
            "nested_mapgen_id": "editor_session_test_applied",
            "object": {"mapgensize": [1, 1], "rows": ["#"], "terrain": {"#": "t_wall"}}
          })"));
        auto out = std::ofstream(file.path, std::ios::binary | std::ios::trunc);
        out << format_json(entries.dump()).value();
    }
    const auto restore_mapgen = restore_on_out_of_scope<bool>(disable_mapgen);
    disable_mapgen = false;
    auto step = 0;

    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto screen = ec::top_screen(captured());
        switch (step++) {
            case 0:
                return invoke("editor:open", {.text = file.path.string()});
            case 1:
                return invoke("editor:apply");
            case 2:
                CHECK_FALSE(ec::find_component(*screen.root, "editor:notice"));
                return act("QUIT");
            default:
                return answer("Leave without saving");
        }
    });

    editor::run_editor_session();
    CHECK(step == 3);
    auto buffer = mapbuffer{};
    buffer.set_dimension_id(get_map().get_bound_dimension());
    auto constructor = mapgen_constructor(buffer);
    const auto picked = pick_mapgen_func("house_04");
    REQUIRE(picked);
    REQUIRE(
        constructor
            .generate(tripoint_abs_omt(60, 60, 0), calendar::turn,
                      {.use_selected_mapgen = true, .selected_mapgen = picked})
            .is_generated());
    CHECK(constructor.ter(point_omt_ms(2, 6)) == ter_id("t_wall"));
    CHECK(constructor.ter(point_omt_ms(3, 6)) == ter_id("t_floor"));
}

TEST_CASE(
    "the editor applies a nested chunk on each save and on each reload from disk",
    "[editor_session][editor_mapgen]") {
    const auto guard = screen_guard{};
    const auto name = std::string("editor_session_test_saved");
    const auto file = mod_file{};
    const auto write_chunk = [&](const std::string& terrain) {
        auto entries = nlohmann::ordered_json::parse(file.text);
        entries.push_back(
            {{"type", "mapgen"},
             {"method", "json"},
             {"nested_mapgen_id", name},
             {"object", {{"mapgensize", {1, 1}}, {"rows", {"#"}}, {"terrain", {{"#", terrain}}}}}});
        auto out = std::ofstream(file.path, std::ios::binary | std::ios::trunc);
        out << format_json(entries.dump()).value();
    };
    write_chunk("t_wall");
    const auto restore_mapgen = restore_on_out_of_scope<bool>(disable_mapgen);
    disable_mapgen = false;
    // What the game places for the chunk, outside the editor's stand-ins.
    const auto placed = [&] {
        auto rows = nlohmann::ordered_json::array();
        for (auto y = 0; y < 24; ++y) { rows.push_back(std::string(24, '.')); }
        const auto parent = nlohmann::ordered_json{
            {"type", "mapgen"},
            {"method", "json"},
            {"om_terrain", "house_04"},
            {"object",
             {{"rows", rows},
              {"terrain", {{".", "t_floor"}}},
              {"place_nested", {{{"chunks", {name}}, {"x", 0}, {"y", 0}}}}}}};
        const auto shown =
            editor::preview_mapgen(parent.dump(), {.at = get_avatar().abs_omt_pos()});
        REQUIRE(shown);
        return shown->squares.front().ter;
    };
    const auto stat = [](const ec::screen_view& screen, const std::string& key) {
        const auto& stats = body<ec::panel_body>(part(screen, "editor:document")).stats;
        const auto found = std::ranges::find(stats, key, &ec::panel_stat::key);
        return found == stats.end() ? std::string{} : found->value;
    };
    const auto set = [](const std::string& id, const std::string& text) {
        return invoke("editor:mapping_set", {.row = id, .text = text});
    };
    auto step = 0;

    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto screen = ec::top_screen(captured());
        switch (step++) {
            case 0:
                return invoke("editor:open", {.text = file.path.string()});
            case 1:
                return invoke("editor:object", {.row = "object:1"});
            case 2:
                CHECK(stat(screen, "apply_on_save") == "Off");
                return invoke("editor:apply");
            case 3:
                // A chunk is applied as the game's chunk of its id.
                CHECK_FALSE(ec::find_component(*screen.root, "editor:notice"));
                CHECK(placed() == "t_wall");
                return invoke("editor:apply_on_save");
            case 4:
                CHECK(stat(screen, "apply_on_save") == "On");
                return set("symbol", "#");
            case 5:
                return set("value", "t_dirt");
            case 6:
                return invoke("editor:mapping_apply");
            case 7:
                CHECK(placed() == "t_wall");
                return invoke("editor:save");
            case 8:
                CHECK(placed() == "t_dirt");
                write_chunk("t_rock_floor");
                return invoke("editor:object", {.row = "object:1"});
            case 9:
                CHECK(placed() == "t_rock_floor");
                return act("QUIT");
            default:
                return answer("Leave without saving");
        }
    });

    editor::run_editor_session();
    CHECK(step == 10);
}

TEST_CASE(
    "on reload the editor applies only the entry it showed and says why a chunk was not applied",
    "[editor_session][editor_mapgen]") {
    const auto guard = screen_guard{};
    const auto file = mod_file{};
    const auto write = [&](const std::optional<std::string>& row) {
        auto entries = nlohmann::ordered_json::parse(file.text);
        if (row) {
            entries.push_back(
                {{"type", "mapgen"},
                 {"method", "json"},
                 {"nested_mapgen_id", "editor_session_test_reloaded"},
                 {"object",
                  {{"mapgensize", {1, 1}}, {"rows", {*row}}, {"terrain", {{"#", "t_wall"}}}}}});
        }
        auto out = std::ofstream(file.path, std::ios::binary | std::ios::trunc);
        out << format_json(entries.dump()).value();
    };
    write("#");
    const auto notice = [](const ec::screen_view& screen) {
        const auto* const found = ec::find_component(*screen.root, "editor:notice");
        return found == nullptr ? std::string{} : body<ec::text_body>(*found).markup;
    };
    const auto reloaded = string_format("%s changed on disk and was reloaded.", file.path.string());
    // The game's own mapgens, so that the one picked is the one registered.
    const auto restore_mapgen = restore_on_out_of_scope<bool>(disable_mapgen);
    disable_mapgen = false;
    auto before = std::shared_ptr<mapgen_function>{};
    auto step = 0;

    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto screen = ec::top_screen(captured());
        switch (step++) {
            case 0:
                return invoke("editor:open", {.text = file.path.string()});
            case 1:
                return invoke("editor:object", {.row = "object:1"});
            case 2:
                return invoke("editor:apply_on_save");
            case 3:
                write("##");
                return invoke("editor:catalog_page", {.count = 0});
            case 4:
                // The chunk the loader refuses is not applied, and the notice says why.
                CHECK_FALSE(notice(screen).empty());
                CHECK(notice(screen) != reloaded);
                before = pick_mapgen_func("house_04");
                write(std::nullopt);
                return invoke("editor:catalog_page", {.count = 0});
            case 5:
                // The chunk is gone: the mapgen shown in its place is not applied.
                CHECK(notice(screen) == reloaded);
                CHECK(pick_mapgen_func("house_04") == before);
                return act("QUIT");
            default:
                return answer("Leave without saving");
        }
    });

    editor::run_editor_session();
    CHECK(step == 6);
}

TEST_CASE(
    "the editor applies and stamps a mapgen with the palettes of its own file",
    "[editor_session][editor_palette]") {
    const auto guard = screen_guard{};
    const auto file = mod_file{};
    const auto omt = get_avatar().abs_omt_pos();
    const auto live = [&](const int x, const int y) {
        return get_map().ter(abs_to_bub(project_to<coords::ms>(omt) + tripoint_rel_ms(x, y, 0)));
    };
    auto step = 0;

    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto screen = ec::top_screen(captured());
        switch (step++) {
            case 0:
                return invoke("editor:open", {.text = file.path.string()});
            case 1:
                return invoke("editor:extract_palette", {.text = "editor_session_test_extracted"});
            case 2:
                return invoke("editor:stamp");
            case 3: {
                CHECK_FALSE(ec::find_component(*screen.root, "editor:notice"));
                // The palette's floor, under what the square's map extras put on it.
                const auto squares = std::views::iota(0, SEEX * 2 * SEEY * 2);
                CHECK(
                    std::ranges::count_if(
                        squares,
                        [&](const int at) {
                            return live(at % (SEEX * 2), at / (SEEX * 2)) == ter_id("t_floor");
                        })
                    > SEEX * SEEY * 2);
                return invoke("editor:object", {.row = "object:1"});
            }
            case 4: {
                // The stamp stays in the world while the palette is open, and can be taken back.
                const auto& stats = body<ec::panel_body>(part(screen, "editor:document")).stats;
                CHECK(std::ranges::contains(stats, std::string("stamped"), &ec::panel_stat::key));
                CHECK(affordance(screen, "editor:unstamp").enabled);
                return invoke("editor:unstamp");
            }
            case 5:
                CHECK_FALSE(affordance(screen, "editor:unstamp").enabled);
                return invoke("editor:object", {.row = "object:0"});
            case 6:
                return invoke("editor:apply");
            case 7:
                CHECK_FALSE(ec::find_component(*screen.root, "editor:notice"));
                return invoke("editor:undo");
            case 8:
                return invoke("editor:save");
            default:
                return act("QUIT");
        }
    });

    editor::run_editor_session();
    CHECK(step == 10);
}

TEST_CASE(
    "the editor refuses to extract a legend the palette could not hold, and leaves no step",
    "[editor_session][editor_palette]") {
    const auto guard = screen_guard{};
    const auto notice = [](const ec::screen_view& screen) {
        return body<ec::text_body>(part(screen, "editor:notice")).markup;
    };
    const auto refused = [&](const nlohmann::ordered_json& extra, const std::string& told) {
        const auto file = mod_file{extra};
        auto step = 0;
        game_client::memory::set_input_provider([&](const int /*timeout*/) {
            const auto screen = ec::top_screen(captured());
            switch (step++) {
                case 0:
                    return invoke("editor:open", {.text = file.path.string()});
                case 1:
                    return invoke(
                        "editor:extract_palette", {.text = "editor_session_test_extracted"});
                case 2:
                    CHECK(notice(screen) == told);
                    CHECK(body<ec::list_body>(part(screen, "editor:objects")).rows.size() == 1);
                    CHECK_FALSE(affordance(screen, "editor:undo").enabled);
                    CHECK_FALSE(affordance(screen, "editor:redo").enabled);
                    return act("QUIT");
                default:
                    return act("QUIT");
            }
        });
        editor::run_editor_session();
        CHECK(step == 3);
        CHECK(read_file(file.path) == file.text);
    };
    SECTION("palettes that are not a list") {
        refused({{"palettes", "editor_session_test_palette"}},
                "The mapgen's palettes are not a list.");
    }
    SECTION("a mapping that reads the mapgen's parameters") {
        refused(
            {{"parameters",
              {{"floor",
                {{"type", "ter_str_id"}, {"default", {{"distribution", {{"t_floor", 1}}}}}}}}},
             {"terrain", {{".", {{"param", "floor"}}}}}},
            "The legend reads the mapgen's parameters, which a palette does not have.");
    }
}

TEST_CASE(
    "the editors remember what they open, newest first, once each, with the mods loaded",
    "[editor_session]") {
    const auto guard = screen_guard{};
    const auto file = mod_file{};
    const auto forget = [] { std::filesystem::remove(editor::recent_documents_file()); };
    forget();
    const auto cleanup = on_out_of_scope(forget);
    CHECK(editor::recent_documents().empty());
    auto step = 0;
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        switch (step++) {
            case 0:
            case 1:
                return invoke("editor:open", {.text = file.path.string()});
            default:
                return act("QUIT");
        }
    });
    editor::run_editor_session();
    CHECK(step == 3);
    const auto mods = world_generator->active_world->info->active_mod_order;
    CHECK(editor::recent_documents()
          == std::vector{editor::recent_document{
              .editor = "map", .name = file.path.generic_string(), .mods = mods}});
    for (auto index = 0; index < 12; ++index) {
        editor::remember_document({.editor = "vehicle", .name = string_format("v%d", index)});
    }
    const auto recent = editor::recent_documents();
    REQUIRE(recent.size() == editor::recent_limit);
    CHECK(recent.front().name == "v11");
    CHECK(recent.back().name == "v2");
}

TEST_CASE(
    "the editors' recent documents keep only the entries written in their shape",
    "[editor_session]") {
    const auto cleanup = on_out_of_scope([] {
        std::filesystem::remove(editor::recent_documents_file());
    });
    {
        auto out = std::ofstream(editor::recent_documents_file(), std::ios::binary);
        out << R"([{"editor": 1, "name": "x"}, "junk", {"editor": "map", "name": "a.json", "mods": "bn"},
            {"editor": "map", "name": "b.json", "mods": ["bn", 2]}])";
    }
    CHECK(
        editor::recent_documents()
        == std::vector<editor::recent_document>{
            {.editor = "map", .name = "a.json"},
            {.editor = "map", .name = "b.json", .mods = {mod_id("bn")}}});
}

TEST_CASE(
    "the map editor opens the document the main menu asked for at its first visit",
    "[editor_session]") {
    const auto guard = screen_guard{};
    const auto file = mod_file{};
    const auto cleanup = on_out_of_scope([] {
        std::filesystem::remove(editor::recent_documents_file());
    });
    editor::open_first({.editor = "map", .name = file.path.generic_string()});
    auto step = 0;
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto screen = ec::top_screen(captured());
        if (step++ == 0) {
            CHECK(body<ec::form_body>(part(screen, "editor:file")).fields.front().value
                  == file.path.generic_string());
            CHECK(body<ec::list_body>(part(screen, "editor:objects")).rows.size() == 1);
        }
        return act("QUIT");
    });
    editor::run_editor_session();
    CHECK(step == 1);
}

TEST_CASE(
    "a new game forgets the document the main menu asked the editor to open", "[editor_session]") {
    const auto guard = screen_guard{};
    const auto file = mod_file{};
    editor::open_first({.editor = "map", .name = file.path.generic_string()});
    editor::reset_editor_session();
    auto step = 0;
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto screen = ec::top_screen(captured());
        if (step++ == 0) { CHECK_FALSE(has_affordance(screen, "editor:save")); }
        return act("QUIT");
    });
    editor::run_editor_session();
    CHECK(step == 1);
}

/// The entries of a house's ground floor beside another house, its roof, open air on its west
/// half, and a nested chunk.
auto house_levels() -> nlohmann::ordered_json {
    auto ground = nlohmann::ordered_json::array();
    auto roof = nlohmann::ordered_json::array();
    for (auto row = 0; row < 24; ++row) {
        ground.push_back(std::string(48, '.'));
        roof.push_back(std::string(11, ' ') + "c" + std::string(12, 'r'));
    }
    return nlohmann::ordered_json::array(
        {{{"type", "mapgen"},
          {"method", "json"},
          {"om_terrain",
           nlohmann::ordered_json::array({nlohmann::ordered_json::array({"house_01", "house_04"})})},
          {"object", {{"rows", ground}, {"terrain", {{".", "t_floor"}}}}}},
         {{"type", "mapgen"},
          {"method", "json"},
          {"om_terrain", "house_01_roof"},
          {"object",
           {{"rows", roof},
            {"terrain", {{" ", "t_open_air"}, {"c", "t_open_air"}, {"r", "t_flat_roof"}}},
            {"furniture", {{"c", "f_chair"}}}}}},
         {{"type", "mapgen"},
          {"method", "json"},
          {"nested_mapgen_id", "editor_session_test_levels_chunk"},
          {"object", {{"mapgensize", {1, 1}}, {"rows", {"#"}}, {"terrain", {{"#", "t_wall"}}}}}}});
}

/// The entries of the two ground floor squares of a city building, the west of floor and the
/// east of wall.
auto building_halves() -> nlohmann::ordered_json {
    const auto half = [](const char* om_terrain, const char symbol, const char* terrain) {
        auto rows = nlohmann::ordered_json::array();
        for (auto row = 0; row < 24; ++row) { rows.push_back(std::string(24, symbol)); }
        return nlohmann::ordered_json{
            {"type", "mapgen"},
            {"method", "json"},
            {"om_terrain", om_terrain},
            {"object", {{"rows", rows}, {"terrain", {{std::string(1, symbol), terrain}}}}}};
    };
    return nlohmann::ordered_json::array(
        {half("urban_1_1", '.', "t_floor"), half("urban_1_2", '#', "t_wall")});
}

/// A mod file of `entries`; removed when the test ends.
struct levels_file {
    std::filesystem::path path =
        std::filesystem::path(PATH_INFO::user_moddir()) / "editor_session_test" / "levels.json";

    explicit levels_file(const nlohmann::ordered_json& entries = house_levels()) {
        std::filesystem::create_directories(path.parent_path());
        auto out = std::ofstream(path, std::ios::binary | std::ios::trunc);
        out << format_json(entries.dump()).value();
    }

    ~levels_file() { std::filesystem::remove_all(path.parent_path()); }
};

TEST_CASE(
    "the canvas of an overmap terrain's mapgen outlines each overmap square",
    "[editor_session][editor_levels]") {
    const auto guard = screen_guard{};
    const auto file = levels_file{};
    const auto block = [](const ec::boundary_state& state) {
        return wire_part(state, "editor:canvas").value("block", nlohmann::json());
    };
    auto step = 0;

    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto state = captured();
        const auto screen = ec::top_screen(state);
        switch (step++) {
            case 0:
                return invoke("editor:open", {.text = file.path.string()});
            case 1:
                CHECK(body<ec::grid_body>(part(screen, "editor:canvas")).cols == 48);
                CHECK(block(state) == nlohmann::json{{"x", 24}, {"y", 24}});
                return invoke("editor:object", {.row = "object:2"});
            case 2:
                CHECK(block(state).is_null());
                return act("QUIT");
            default:
                FAIL("the editor outlived its exit");
                return act("QUIT");
        }
    });

    editor::run_editor_session();
    CHECK(step == 3);
}

TEST_CASE(
    "the editor goes up and down the levels a building stacks in the file",
    "[editor_session][editor_levels]") {
    const auto guard = screen_guard{};
    const auto file = levels_file{};
    const auto level = [](const ec::screen_view& screen) {
        const auto& stats = body<ec::panel_body>(part(screen, "editor:document")).stats;
        const auto found = std::ranges::find(stats, "level", &ec::panel_stat::key);
        return found == stats.end() ? std::string("none") : found->value;
    };
    const auto selected = [](const ec::screen_view& screen) {
        const auto& rows = body<ec::list_body>(part(screen, "editor:objects")).rows;
        const auto found = std::ranges::find_if(rows, &ec::row::selected);
        return found == rows.end() ? std::string() : found->id;
    };
    auto step = 0;

    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto screen = ec::top_screen(captured());
        switch (step++) {
            case 0:
                return invoke("editor:open", {.text = file.path.string()});
            case 1:
                CHECK(level(screen) == "0");
                CHECK(affordance(screen, "editor:level_up").keys == std::vector<std::string>{"<"});
                CHECK(affordance(screen, "editor:level_up").enabled);
                CHECK(
                    affordance(screen, "editor:level_down").keys == std::vector<std::string>{">"});
                // house_01 has no level below it.
                CHECK_FALSE(affordance(screen, "editor:level_down").enabled);
                CHECK_FALSE(affordance(screen, "editor:level_down").denial.empty());
                return invoke("editor:level_up");
            case 2:
                CHECK(selected(screen) == "object:1");
                CHECK(level(screen) == "1");
                CHECK_FALSE(affordance(screen, "editor:level_up").enabled);
                return invoke("editor:level_down");
            case 3:
                CHECK(selected(screen) == "object:0");
                return invoke("editor:object", {.row = "object:2"});
            case 4:
                // A nested chunk stands on no level.
                CHECK(level(screen) == "none");
                CHECK_FALSE(affordance(screen, "editor:level_up").enabled);
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
    "the canvas shows the level below, faded, where its own squares are open air",
    "[editor_session][editor_levels]") {
    const auto guard = screen_guard{};
    const auto file = levels_file{};
    const auto look = [](const ec::screen_view& screen, const int x, const int y) {
        const auto& grid = body<ec::grid_body>(part(screen, "editor:canvas"));
        const auto& shown = cell(screen, x, y);
        return shown.looks.empty() ? std::string() : grid.looks.at(shown.looks.front()).id;
    };
    auto step = 0;

    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto state = captured();
        const auto screen = ec::top_screen(state);
        switch (step++) {
            case 0:
                return invoke("editor:open", {.text = file.path.string()});
            case 1:
                CHECK_FALSE(cell(screen, 0, 0).faded);
                return invoke("editor:level_up");
            case 2: {
                CHECK(cell(screen, 0, 0).faded);
                CHECK(cell(screen, 0, 0).glyph == ".");
                CHECK(look(screen, 0, 0) == "t_floor");
                const auto cells = wire_part(state, "editor:canvas")["cells"];
                CHECK(std::ranges::any_of(cells, [](const auto& entry) {
                    return entry.value("faded", false);
                }));
                CHECK_FALSE(cell(screen, 20, 0).faded);
                // Open air that holds furniture shows its own square.
                CHECK_FALSE(cell(screen, 11, 0).faded);
                CHECK(cell(screen, 11, 0).glyph == "c");
                CHECK(cell(screen, 20, 0).glyph == "r");
                return invoke("editor:symbol", {.row = "symbol:r"});
            }
            case 3:
                return invoke("editor:paint", {.row = "square:0,0"});
            case 4:
                // Drawing on a faded square draws on this level.
                CHECK_FALSE(cell(screen, 0, 0).faded);
                CHECK(cell(screen, 0, 0).glyph == "r");
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
}

TEST_CASE("a building stacks its roof one level above its ground floor", "[editor_levels]") {
    const auto index = editor::index_levels();
    CHECK(editor::terrain_level(index, "house_01") == 0);
    CHECK(editor::terrain_level(index, "house_01_roof") == 1);
    CHECK(editor::terrain_above(index, "house_01", 1) == "house_01_roof");
    CHECK(editor::terrain_above(index, "house_01_roof", -1) == "house_01");
    CHECK(editor::terrain_above(index, "house_01", -1) == std::nullopt);
    CHECK(editor::terrain_level(index, "no_such_terrain") == std::nullopt);
}

TEST_CASE("a terrain repeated up a tower leads past itself to the next one", "[editor_levels]") {
    const auto index = editor::level_index(
        {.layouts = {
             {{tripoint(0, 0, 0), {.name = "tower"}},
              {tripoint(0, 0, 1), {.name = "tower"}},
              {tripoint(0, 0, 2), {.name = "tower_roof"}}}}});
    CHECK(editor::terrain_level(index, "tower") == 0);
    CHECK(editor::terrain_above(index, "tower", 1) == "tower_roof");
    CHECK(editor::terrain_above(index, "tower_roof", -1) == "tower");
    CHECK(editor::terrain_above(index, "tower", -1) == std::nullopt);
}

TEST_CASE(
    "the level below an entry's square is another entry's even when the entry makes it too",
    "[editor_levels]") {
    const auto index = editor::level_index(
        {.layouts = {
             {{tripoint(0, 0, -1), {.name = "cellar"}}, {tripoint(0, 0, 0), {.name = "shop"}}}}});
    const auto entries = std::vector<editor::document_object>{
        {.value = nlohmann::ordered_json::parse(R"({"om_terrain": [["shop", "cellar"]]})")},
        {.value = nlohmann::ordered_json::parse(R"({"om_terrain": "cellar"})")}};
    const auto below = editor::stacked_squares(index, entries, 0, -1);
    REQUIRE(below.size() == 1);
    CHECK(below[0].origin == point_zero);
    CHECK(below[0].other.entry == 1);
}

TEST_CASE("an entry of several overmap terrains makes each of them", "[editor_levels]") {
    const auto entries = std::vector<editor::document_object>{
        {.value = nlohmann::ordered_json::parse(R"({"om_terrain": ["shed", "barn"]})")}};
    const auto made = editor::square_making(entries, "barn");
    REQUIRE(made.has_value());
    CHECK(made->entry == 0);
    CHECK(made->origin == point_zero);
}

TEST_CASE(
    "an entry's neighbours are the squares beside it on its level that other entries make",
    "[editor_levels]") {
    const auto index = editor::level_index(
        {.layouts = {
             {{tripoint(0, 0, 0), {.name = "shop_front"}},
              {tripoint(1, 0, 0), {.name = "shop_back"}},
              {tripoint(0, 1, 0), {.name = "shop_yard", .turned = true}},
              {tripoint(0, 0, 1), {.name = "shop_roof"}}}}});
    const auto entries = std::vector<editor::document_object>{
        {.value = nlohmann::ordered_json::parse(R"({"om_terrain": "shop_front"})")},
        {.value = nlohmann::ordered_json::parse(R"({"om_terrain": "shop_back"})")},
        {.value = nlohmann::ordered_json::parse(R"({"om_terrain": "shop_yard"})")},
        {.value = nlohmann::ordered_json::parse(R"({"om_terrain": "shop_roof"})")}};
    // The yard is turned from how its mapgen draws it, and the roof is a level up.
    const auto front = editor::neighbour_squares(index, entries, 0);
    REQUIRE(front.size() == 1);
    CHECK(front[0].origin == point(24, 0));
    CHECK(front[0].other.entry == 1);
    const auto back = editor::neighbour_squares(index, entries, 1);
    REQUIRE(back.size() == 1);
    CHECK(back[0].origin == point(-24, 0));
    CHECK(back[0].other.entry == 0);
}

TEST_CASE("a square beside two squares of an entry is one neighbour", "[editor_levels]") {
    const auto index = editor::level_index(
        {.layouts = {
             {{tripoint(0, 0, 0), {.name = "a"}},
              {tripoint(1, 0, 0), {.name = "b"}},
              {tripoint(0, 1, 0), {.name = "c"}},
              {tripoint(1, 1, 0), {.name = "d"}}}}});
    const auto entries = std::vector<editor::document_object>{
        {.value = nlohmann::ordered_json::parse(R"({"om_terrain": [["a", "b"], ["c"]]})")},
        {.value = nlohmann::ordered_json::parse(R"({"om_terrain": "d"})")}};
    const auto beside = editor::neighbour_squares(index, entries, 0);
    REQUIRE(beside.size() == 1);
    CHECK(beside[0].origin == point(24, 24));
}

TEST_CASE(
    "a square beside the canvas entry that draws nothing leaves the canvas in place",
    "[editor_session][editor_levels]") {
    const auto guard = screen_guard{};
    auto entries = building_halves();
    // urban_1_1, west of urban_1_2, is all its fill.
    entries[0]["object"] = {{"fill_ter", "t_floor"}};
    std::swap(entries[0], entries[1]);
    const auto file = levels_file(entries);
    auto step = 0;

    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto screen = ec::top_screen(captured());
        switch (step++) {
            case 0:
                return invoke("editor:open", {.text = file.path.string()});
            case 1:
                CHECK(cell(screen, 0, 0).id == "square:0,0");
                CHECK(body<ec::grid_body>(part(screen, "editor:canvas")).cols == 24);
                CHECK(part(screen, "editor:canvas").affordances
                      == std::vector<std::string>{"editor:paint"});
                return act("QUIT");
            default:
                FAIL("the editor outlived its exit");
                return act("QUIT");
        }
    });

    editor::run_editor_session();
    CHECK(step == 2);
}

TEST_CASE("an entry's own squares are none of its neighbours", "[editor_levels]") {
    const auto index = editor::level_index(
        {.layouts = {{{tripoint(0, 0, 0), {.name = "shop_front"}},
                      {tripoint(1, 0, 0), {.name = "shop_back"}}}}});
    const auto entries = std::vector<editor::document_object>{
        {.value = nlohmann::ordered_json::parse(R"({"om_terrain": [["shop_front", "shop_back"]]})")},
        {.value = nlohmann::ordered_json::parse(R"({"om_terrain": "shop_back"})")}};
    CHECK(editor::neighbour_squares(index, entries, 0).empty());
}

TEST_CASE(
    "the canvas shows the edges of the squares beside it that the file makes, faded",
    "[editor_session][editor_levels]") {
    const auto guard = screen_guard{};
    const auto file = levels_file(building_halves());
    const auto has_cell = [](const ec::screen_view& screen, const int x, const int y) {
        const auto& cells = body<ec::grid_body>(part(screen, "editor:canvas")).cells;
        return std::ranges::contains(cells, ec::grid_point{x, y}, &ec::grid_cell::at);
    };
    auto step = 0;

    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto screen = ec::top_screen(captured());
        switch (step++) {
            case 0:
                return invoke("editor:open", {.text = file.path.string()});
            case 1: {
                // urban_1_2 stands east of urban_1_1.
                CHECK(body<ec::grid_body>(part(screen, "editor:canvas")).cols == 28);
                CHECK_FALSE(cell(screen, 23, 0).faded);
                const auto& beside = cell(screen, 24, 0);
                CHECK(beside.faded);
                CHECK(beside.glyph == "#");
                CHECK(beside.id == "object:1");
                CHECK(beside.affordance == "editor:object");
                CHECK(cell(screen, 27, 23).faded);
                CHECK_FALSE(has_cell(screen, 28, 0));
                CHECK_FALSE(has_cell(screen, 24, 24));
                return invoke("editor:object", {.row = beside.id});
            }
            case 2:
                // urban_1_1 stands west of urban_1_2: the canvas starts an overmap square east,
                // where its outline falls.
                CHECK(cell(screen, 24, 0).id == "square:0,0");
                CHECK(cell(screen, 24, 0).glyph == "#");
                CHECK_FALSE(cell(screen, 24, 0).faded);
                CHECK(cell(screen, 20, 0).faded);
                CHECK(cell(screen, 20, 0).glyph == ".");
                CHECK_FALSE(has_cell(screen, 19, 0));
                return act("QUIT");
            default:
                FAIL("the editor outlived its exit");
                return act("QUIT");
        }
    });

    editor::run_editor_session();
    CHECK(step == 3);
}

auto special_levels() -> nlohmann::ordered_json {
    const auto square = [](const int x, const int y, const int z, const std::string& name) {
        return nlohmann::ordered_json{{"point", {x, y, z}}, {"overmap", name}};
    };
    return nlohmann::ordered_json::array(
        {{{"type", "overmap_special"},
          {"id", "editor_session_test_special"},
          {"overmaps",
           {square(0, 0, 0, "house_01_north"), square(1, 0, 0, "field"),
            square(0, 0, 1, "house_01_roof_north")}},
          {"locations", {"land"}},
          {"occurrences", {0, 1}}}});
}

TEST_CASE(
    "an overmap special opens on the overmap squares of one level, each opening its mapgen",
    "[editor_session][editor_special]") {
    const auto guard = screen_guard{};
    const auto restore_mapgen = restore_on_out_of_scope<bool>(disable_mapgen);
    disable_mapgen = false;
    const auto file = levels_file{special_levels()};
    const auto level = [](const ec::screen_view& screen) {
        const auto& stats = body<ec::panel_body>(part(screen, "editor:document")).stats;
        const auto found = std::ranges::find(stats, "level", &ec::panel_stat::key);
        return found == stats.end() ? std::string("none") : found->value;
    };
    const auto look = [](const ec::screen_view& screen, const int x, const int y) {
        const auto& grid = body<ec::grid_body>(part(screen, "editor:canvas"));
        const auto& shown = cell(screen, x, y);
        return shown.looks.empty()
                 ? std::string()
                 : grid.looks.at(shown.looks.front()).id.value_or("");
    };
    auto step = 0;

    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto screen = ec::top_screen(captured());
        switch (step++) {
            case 0:
                return invoke("editor:open", {.text = file.path.string()});
            case 1: {
                const auto& grid = body<ec::grid_body>(part(screen, "editor:canvas"));
                CHECK(grid.cols == 2);
                CHECK(grid.rows == 1);
                CHECK(level(screen) == "0");
                CHECK(look(screen, 0, 0) == "house_01");
                CHECK(look(screen, 1, 0) == "field");
                CHECK(cell(screen, 0, 0).glyph == oter_str_id("house_01_north")->get_symbol());
                CHECK(cell(screen, 0, 0).affordance == "editor:open_square");
                CHECK(cell(screen, 0, 0).id == "square:0,0");
                CHECK(affordance(screen, "editor:level_up").enabled);
                CHECK_FALSE(affordance(screen, "editor:level_down").enabled);
                return invoke("editor:level_up");
            }
            case 2:
                CHECK(level(screen) == "1");
                CHECK(look(screen, 0, 0) == "house_01_roof");
                // The special places nothing east of the roof.
                CHECK(body<ec::grid_body>(part(screen, "editor:canvas")).cells.size() == 1);
                CHECK_FALSE(affordance(screen, "editor:level_up").enabled);
                return invoke("editor:open_square", {.row = "square:0,0"});
            case 3: {
                const auto& entries = body<ec::list_body>(part(screen, "editor:objects")).rows;
                const auto selected = std::ranges::find_if(entries, &ec::row::selected);
                REQUIRE(selected != entries.end());
                CHECK(selected->label.find("house_01_roof") != std::string::npos);
                return act("QUIT");
            }
            default:
                FAIL("the editor outlived its exit");
                return act("QUIT");
        }
    });

    editor::run_editor_session();
    CHECK(step == 4);
}

TEST_CASE(
    "the editor reports a special's unknown terrain and a connection over its own square",
    "[editor_session][editor_special]") {
    const auto guard = screen_guard{};
    const auto restore_mapgen = restore_on_out_of_scope<bool>(disable_mapgen);
    disable_mapgen = false;
    auto entries = special_levels();
    entries[0]["overmaps"].push_back({{"point", {0, 1, 0}}, {"overmap", "no_such_terrain_north"}});
    entries[0]["connections"] = {{{"point", {1, 0, 0}}, {"connection", "local_road"}}};
    const auto file = levels_file{entries};
    const auto said = [](const ec::screen_view& screen) {
        return body<ec::list_body>(part(screen, "editor:diagnostics")).rows
             | std::views::transform(&ec::row::label) | std::ranges::to<std::vector>();
    };
    auto step = 0;

    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto screen = ec::top_screen(captured());
        switch (step++) {
            case 0:
                return invoke("editor:open", {.text = file.path.string()});
            case 1: {
                const auto messages = said(screen);
                CHECK(std::ranges::any_of(messages, [](const std::string& text) {
                    return text.find("no_such_terrain_north") != std::string::npos;
                }));
                CHECK(std::ranges::any_of(messages, [](const std::string& text) {
                    return text.find("overwrites terrain") != std::string::npos;
                }));

                return act("QUIT");
            }
            default:
                FAIL("the editor outlived its exit");
                return act("QUIT");
        }
    });

    editor::run_editor_session();
    CHECK(step == 2);
}

TEST_CASE("a special's square names its terrain and turn", "[editor_special]") {
    const auto squares = editor::special_squares(special_levels()[0]);
    REQUIRE(squares.size() == 3);
    CHECK(squares[0].at == tripoint_rel_omt(0, 0, 0));
    CHECK(squares[0].overmap == "house_01_north");
    CHECK(squares[2].at == tripoint_rel_omt(0, 0, 1));
}

TEST_CASE(
    "the editor draws a special's squares with an overmap terrain from the catalog and erases them",
    "[editor_session][editor_special]") {
    const auto guard = screen_guard{};
    const auto file = levels_file{special_levels()};
    const auto shown = [](const ec::screen_view& screen, const std::string& id) {
        const auto& cells = body<ec::grid_body>(part(screen, "editor:canvas")).cells;
        const auto found = std::ranges::find(cells, id, &ec::grid_cell::id);
        return found == cells.end() ? std::string("none") : found->label;
    };
    const auto stat = [](const ec::screen_view& screen, const std::string& key) {
        const auto& stats = body<ec::panel_body>(part(screen, "editor:document")).stats;
        const auto found = std::ranges::find(stats, key, &ec::panel_stat::key);
        return found == stats.end() ? std::string("none") : found->value;
    };
    const auto squares = [&] {
        return editor::special_squares(nlohmann::ordered_json::parse(read_file(file.path))[0]);
    };
    auto step = 0;

    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto screen = ec::top_screen(captured());
        switch (step++) {
            case 0:
                return invoke("editor:open", {.text = file.path.string()});
            case 1:
                // Drawing with nothing picked says what to pick.
                return invoke("editor:tool", {.row = "tool:point"});
            case 2:
                CHECK(cell(screen, 1, 1).affordance == "editor:paint");
                // One square of room around the special to draw new squares on.
                CHECK(body<ec::grid_body>(part(screen, "editor:canvas")).cols == 4);
                CHECK(shown(screen, "square:-1,0") == "");
                return invoke("editor:paint", {.row = "square:2,0"});
            case 3:
                CHECK(ec::find_component(*screen.root, "editor:notice"));
                return invoke("editor:catalog_filter", {.text = "forest_thick"});
            case 4:
                return invoke("editor:catalog_pick", {.row = "entry:forest_thick"});
            case 5:
                // The terrain is named as the game names it, its id telling it from the others.
                CHECK(stat(screen, "terrain")
                      == oter_str_id("forest_thick")->get_name() + " (forest_thick)");
                return invoke("editor:paint", {.row = "square:2,0"});
            case 6:
                CHECK(shown(screen, "square:2,0") == oter_str_id("forest_thick")->get_name());
                return invoke("editor:paint", {.row = "square:1,0"});
            case 7:
                CHECK(shown(screen, "square:1,0") == oter_str_id("forest_thick")->get_name());
                return invoke("editor:undo");
            case 8:
                CHECK(shown(screen, "square:1,0") == oter_str_id("field")->get_name());
                return invoke("editor:tool", {.row = "tool:erase"});
            case 9:
                return invoke("editor:paint", {.row = "square:1,0"});
            case 10:
                CHECK(shown(screen, "square:1,0") == "");
                return invoke("editor:save");
            case 11: {
                const auto saved = squares();
                CHECK(saved.size() == 3);
                CHECK(
                    std::ranges::contains(saved, "forest_thick", &editor::special_square::overmap));
                CHECK_FALSE(
                    std::ranges::contains(saved, "field", &editor::special_square::overmap));
                return invoke("editor:tool", {.row = "tool:open"});
            }
            case 12:
                CHECK(cell(screen, 0, 0).affordance == "editor:open_square");
                return act("QUIT");
            default:
                FAIL("the editor outlived its exit");
                return act("QUIT");
        }
    });

    editor::run_editor_session();
    CHECK(step == 13);
}

TEST_CASE("a special's square is set, added or removed by a patch", "[editor_special]") {
    const auto value = special_levels()[0];
    const auto apply = [&](const nlohmann::ordered_json& patch) {
        return editor::special_squares(value.patch(patch));
    };
    const auto set = editor::square_patch(value, tripoint_rel_omt(1, 0, 0), "forest");
    CHECK(apply(set)[1].overmap == "forest");
    const auto added = editor::square_patch(value, tripoint_rel_omt(-1, 2, 1), "forest");
    REQUIRE(apply(added).size() == 4);
    CHECK(apply(added)[3].at == tripoint_rel_omt(-1, 2, 1));
    const auto removed = editor::square_patch(value, tripoint_rel_omt(0, 0, 1), "");
    CHECK(apply(removed).size() == 2);
    CHECK(editor::square_patch(value, tripoint_rel_omt(1, 0, 0), "field").empty());
    CHECK(editor::square_patch(value, tripoint_rel_omt(5, 5, 0), "").empty());
}

TEST_CASE(
    "the editor lists a file's specials past values of other shapes and opens a special on the "
    "level nearest the ground",
    "[editor_session][editor_special]") {
    const auto guard = screen_guard{};
    auto entries = nlohmann::ordered_json::array(
        {"a string", 5, {{"type", "overmap_special"}, {"id", "odd"}, {"subtype", 5}}});
    auto cellar = special_levels()[0];
    cellar["overmaps"] =
        {{{"point", {0, 0, -2}}, {"overmap", "field"}},
         {{"point", {0, 0, 1}}, {"overmap", "forest"}}};
    entries.push_back(cellar);
    const auto file = levels_file{entries};
    const auto level = [](const ec::screen_view& screen) {
        const auto& stats = body<ec::panel_body>(part(screen, "editor:document")).stats;
        const auto found = std::ranges::find(stats, "level", &ec::panel_stat::key);
        return found == stats.end() ? std::string("none") : found->value;
    };
    auto step = 0;

    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto screen = ec::top_screen(captured());
        switch (step++) {
            case 0:
                return invoke("editor:open", {.text = file.path.string()});
            case 1:
                CHECK(body<ec::list_body>(part(screen, "editor:objects")).rows.size() == 2);
                return invoke("editor:object", {.row = "object:3"});
            case 2:
                CHECK(level(screen) == "1");
                CHECK(affordance(screen, "editor:level_down").enabled);
                return invoke("editor:level_down");
            case 3:
                // The levels between that place nothing are passed over.
                CHECK(level(screen) == "-2");
                CHECK_FALSE(affordance(screen, "editor:level_down").enabled);
                return invoke("editor:open", {.text = file.path.string()});
            case 4:
                return invoke("editor:object", {.row = "object:3"});
            case 5:
                CHECK(level(screen) == "1");
                return act("QUIT");
            default:
                FAIL("the editor outlived its exit");
                return act("QUIT");
        }
    });

    editor::run_editor_session();
    CHECK(step == 6);
}

TEST_CASE(
    "a special that copies another is not checked alone, and its squares look as the overmap's",
    "[editor_session][editor_special]") {
    const auto guard = screen_guard{};
    auto copied = special_levels()[0];
    copied.erase("overmaps");
    copied["copy-from"] = "Hunting Lodge";
    auto shaped = special_levels()[0];
    shaped["id"] = "editor_session_test_shaped";
    shaped["overmaps"] =
        {{{"point", {0, 0, 0}}, {"overmap", "road_ns"}},
         {{"point", {1, 0, 0}}, {"overmap", "vending_park_1_north"}}};
    const auto file = levels_file{nlohmann::ordered_json::array({copied, shaped})};
    const auto look = [](const ec::screen_view& screen, const std::string& id) {
        const auto& grid = body<ec::grid_body>(part(screen, "editor:canvas"));
        const auto found = std::ranges::find(grid.cells, id, &ec::grid_cell::id);
        REQUIRE(found != grid.cells.end());
        REQUIRE(found->looks.size() == 1);
        return grid.looks.at(found->looks.front());
    };
    auto step = 0;

    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto screen = ec::top_screen(captured());
        switch (step++) {
            case 0:
                return invoke("editor:open", {.text = file.path.string()});
            case 1:
                CHECK(body<ec::list_body>(part(screen, "editor:diagnostics")).rows.empty());
                return invoke("editor:object", {.row = "object:1"});
            case 2: {
                CHECK(look(screen, "square:0,0").subtile == "edge");
                CHECK(look(screen, "square:1,0").looks_like
                      == std::vector<std::string>{"small_storage_units"});
                return act("QUIT");
            }
            default:
                FAIL("the editor outlived its exit");
                return act("QUIT");
        }
    });

    editor::run_editor_session();
    CHECK(step == 3);
}

TEST_CASE("a special's square farther than an overmap reaches is left out", "[editor_special]") {
    auto value = special_levels()[0];
    value["overmaps"].push_back({{"point", {4000000000LL, 0, 0}}, {"overmap", "field"}});
    value["overmaps"].push_back({{"point", {0, -OMAPY, 0}}, {"overmap", "field"}});
    CHECK(editor::special_squares(value).size() == 3);
}

TEST_CASE(
    "the editor's connect tool joins a special to a road beside it and parts them again",
    "[editor_session][editor_special]") {
    const auto guard = screen_guard{};
    const auto file = levels_file{special_levels()};
    const auto connections = [&] {
        const auto saved = nlohmann::ordered_json::parse(read_file(file.path))[0];
        return saved.value("connections", nlohmann::ordered_json::array());
    };
    const auto shown = [](const ec::screen_view& screen, const std::string& id) {
        const auto& cells = body<ec::grid_body>(part(screen, "editor:canvas")).cells;
        const auto found = std::ranges::find(cells, id, &ec::grid_cell::id);
        REQUIRE(found != cells.end());
        return *found;
    };
    auto step = 0;

    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto screen = ec::top_screen(captured());
        switch (step++) {
            case 0:
                return invoke("editor:open", {.text = file.path.string()});
            case 1: {
                const auto& field =
                    body<ec::form_body>(part(screen, "editor:connection")).fields.front();
                CHECK(field.value == "local_road");
                // The connections to pick from are named by the terrain they lay.
                CHECK(field.type == "select");
                const auto road =
                    std::ranges::find(field.options, "local_road", &ec::form_option::id);
                REQUIRE(road != field.options.end());
                CHECK(road->label == _(oter_type_str_id("road")->name));
                return invoke("editor:tool", {.row = "tool:connect"});
            }
            case 2:
                return invoke("editor:paint", {.row = "square:-1,0"});
            case 3:
                CHECK(shown(screen, "square:-1,0").faded);
                CHECK(shown(screen, "square:-1,0").label
                      == _(oter_type_str_id("road")->name) + " connection");
                return invoke("editor:save");
            case 4: {
                const auto saved = connections();
                REQUIRE(saved.size() == 1);
                CHECK(saved[0]["point"] == nlohmann::ordered_json::array({-1, 0, 0}));
                CHECK(saved[0]["connection"] == "local_road");
                CHECK(saved[0]["from"] == nlohmann::ordered_json::array({0, 0, 0}));
                return invoke("editor:paint", {.row = "square:-1,0"});
            }
            case 5:
                CHECK_FALSE(shown(screen, "square:-1,0").faded);
                return invoke("editor:connection_set", {.text = "no_such_connection"});
            case 6:
                return invoke("editor:paint", {.row = "square:2,0"});
            case 7:
                // The loader names a connection the game does not have.
                CHECK(std::ranges::any_of(
                    body<ec::list_body>(part(screen, "editor:diagnostics")).rows,
                    [](const ec::row& row) {
                        return row.label.find("no_such_connection") != std::string::npos;
                    }));
                return invoke("editor:save");
            case 8:
                CHECK(connections().size() == 1);
                return act("QUIT");
            default:
                FAIL("the editor outlived its exit");
                return act("QUIT");
        }
    });

    editor::run_editor_session();
    CHECK(step == 9);
}

TEST_CASE("a special's connection is added beside a square or removed", "[editor_special]") {
    const auto value = special_levels()[0];
    const auto added = value.patch(
        editor::connection_patch(value, tripoint_rel_omt(0, 1, 0), "local_road"));
    const auto joined = editor::special_connections(added);
    REQUIRE(joined.size() == 1);
    CHECK(joined[0].at == tripoint_rel_omt(0, 1, 0));
    CHECK(added["connections"][0]["from"] == nlohmann::ordered_json::array({0, 0, 0}));
    // A square with no square of the special beside it has no side to join from.
    const auto alone = value.patch(
        editor::connection_patch(value, tripoint_rel_omt(5, 5, 0), "local_road"));
    CHECK_FALSE(alone["connections"][0].contains("from"));
    CHECK(editor::special_connections(
              added.patch(editor::connection_patch(added, tripoint_rel_omt(0, 1, 0), "local_road")))
              .empty());
}

TEST_CASE(
    "drawing a special stays inside an overmap, reaches new levels and leaves copies alone",
    "[editor_session][editor_special]") {
    const auto guard = screen_guard{};
    auto edge = special_levels()[0];
    edge["overmaps"].push_back({{"point", {OMAPX - 1, 0, 0}}, {"overmap", "field"}});
    auto copied = special_levels()[0];
    copied.erase("overmaps");
    copied["copy-from"] = "Hunting Lodge";
    auto mapgen = house_levels()[1];
    const auto file = levels_file{nlohmann::ordered_json::array({edge, copied, mapgen})};
    const auto level = [](const ec::screen_view& screen) {
        const auto& stats = body<ec::panel_body>(part(screen, "editor:document")).stats;
        const auto found = std::ranges::find(stats, "level", &ec::panel_stat::key);
        return found == stats.end() ? std::string("none") : found->value;
    };
    const auto has_cell = [](const ec::screen_view& screen, const std::string& id) {
        return std::ranges::contains(
            body<ec::grid_body>(part(screen, "editor:canvas")).cells, id, &ec::grid_cell::id);
    };
    const auto notice = [](const ec::screen_view& screen) {
        return ec::find_component(*screen.root, "editor:notice") != nullptr;
    };
    auto step = 0;

    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto screen = ec::top_screen(captured());
        switch (step++) {
            case 0:
                return invoke("editor:open", {.text = file.path.string()});
            case 1:
                return invoke("editor:catalog_filter", {.text = "forest_thick"});
            case 2:
                return invoke("editor:tool", {.row = "tool:point"});
            case 3:
                CHECK(has_cell(screen, string_format("square:%d,0", OMAPX - 1)));
                CHECK_FALSE(has_cell(screen, string_format("square:%d,0", OMAPX)));
                return invoke("editor:catalog_pick", {.row = "entry:forest_thick"});
            case 4:
                CHECK(std::ranges::contains(
                    body<ec::panel_body>(part(screen, "editor:document")).stats,
                    oter_str_id("forest_thick")->get_name() + " (forest_thick)",
                    &ec::panel_stat::value));
                return invoke("editor:paint", {.row = string_format("square:%d,0", OMAPX)});
            case 5:
                CHECK(notice(screen));
                CHECK_FALSE(
                    body<ec::panel_body>(part(screen, "editor:document")).stats.front().value
                    == "No");
                return invoke("editor:level_up");
            case 6:
                CHECK(level(screen) == "1");
                CHECK(affordance(screen, "editor:level_up").enabled);
                return invoke("editor:level_up");
            case 7:
                // A drawing tool goes on to a level the special has not reached yet.
                CHECK(level(screen) == "2");
                return invoke("editor:paint", {.row = "square:0,0"});
            case 8:
                CHECK(body<ec::panel_body>(part(screen, "editor:document")).stats.front().value
                      == "No");
                CHECK(has_cell(screen, "square:0,0"));
                return invoke("editor:catalog_filter", {.text = "zzz"});
            case 9:
                return invoke("editor:object", {.row = "object:1"});
            case 10:
                // The squares of a copy come from what it copies, which only the game's data has.
                return invoke("editor:paint", {.row = "square:0,0"});
            case 11:
                CHECK(notice(screen));
                return invoke("editor:object", {.row = "object:2"});
            case 12:
                // The mapgen's catalog does not keep the special's filter.
                CHECK(body<ec::list_body>(part(screen, "editor:catalog")).filter.empty());
                return act("QUIT");
            case 13:
                return answer("Leave without saving");
            default:
                FAIL("the editor outlived its exit");
                return act("QUIT");
        }
    });

    editor::run_editor_session();
    CHECK(step == 14);
}

TEST_CASE("a special's squares named twice are all erased and all set", "[editor_special]") {
    auto value = special_levels()[0];
    value["overmaps"].push_back({{"point", {1, 0, 0}}, {"overmap", "forest"}});
    const auto erased = value.patch(editor::square_patch(value, tripoint_rel_omt(1, 0, 0), ""));
    CHECK(editor::special_squares(erased).size() == 2);
    const auto set = editor::special_squares(
        value.patch(editor::square_patch(value, tripoint_rel_omt(1, 0, 0), "forest_thick")));
    CHECK(std::ranges::count(set, "forest_thick", &editor::special_square::overmap) == 2);
}

TEST_CASE(
    "the editor creates an overmap special or a building file and opens it on its square",
    "[editor_session][editor_special]") {
    const auto guard = screen_guard{};
    const auto folder = mod_file{};
    const auto made = folder.path.parent_path() / "special.json";
    const auto built = folder.path.parent_path() / "building.json";
    const auto notice = [](const ec::screen_view& screen) {
        return body<ec::text_body>(part(screen, "editor:notice")).markup;
    };
    const auto label = [](const ec::screen_view& screen, const std::string& id) {
        const auto& fields = body<ec::form_body>(part(screen, "editor:new")).fields;
        const auto found = std::ranges::find(fields, id, &ec::form_field::id);
        REQUIRE(found != fields.end());
        return found->label;
    };
    const auto value = [](const ec::screen_view& screen, const std::string& id) {
        const auto& fields = body<ec::form_body>(part(screen, "editor:new")).fields;
        const auto found = std::ranges::find(fields, id, &ec::form_field::id);
        REQUIRE(found != fields.end());
        return found->value;
    };
    auto step = 0;

    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto screen = ec::top_screen(captured());
        switch (step++) {
            case 0:
                return invoke("editor:new_set", {.row = "path", .text = made.string()});
            case 1:
                return invoke("editor:new_set", {.row = "kind", .text = "special"});
            case 2:
                CHECK(label(screen, "om_terrain") == "Special id");
                CHECK(label(screen, "fill") == "Overmap terrain");
                // A mapgen's terrain default gives way to an overmap terrain one.
                CHECK(value(screen, "fill") == "field");
                return invoke("editor:new_set", {.row = "om_terrain", .text = "Hunting Lodge"});
            case 3:
                return invoke("editor:new_apply");
            case 4:
                CHECK(notice(screen)
                      == "Hunting Lodge names an overmap special or building already.");
                return invoke(
                    "editor:new_set", {.row = "om_terrain", .text = "editor_session_test_new"});
            case 5:
                return invoke("editor:new_set", {.row = "fill", .text = "vending_park_1"});
            case 6:
                return invoke("editor:new_apply");
            case 7:
                // Overmap terrains with rotations are named with one.
                CHECK(
                    notice(screen)
                    == "vending_park_1 is no overmap terrain. Did you mean vending_park_1_north?");
                CHECK_FALSE(std::filesystem::exists(made));
                return invoke("editor:new_set", {.row = "fill", .text = "field"});
            case 8:
                return invoke("editor:new_apply");
            case 9: {
                CHECK_FALSE(ec::find_component(*screen.root, "editor:notice"));
                const auto& objects = body<ec::list_body>(part(screen, "editor:objects")).rows;
                REQUIRE(objects.size() == 1);
                CHECK(objects[0].label == "editor_session_test_new");
                CHECK(cell(screen, 0, 0).label == oter_str_id("field")->get_name());
                CHECK(body<ec::list_body>(part(screen, "editor:diagnostics")).rows.empty());
                return invoke("editor:new_set", {.row = "path", .text = built.string()});
            }
            case 10:
                return invoke("editor:new_set", {.row = "kind", .text = "building"});
            case 11:
                CHECK(label(screen, "om_terrain") == "Building id");
                // The form starts over after the special was made.
                CHECK(value(screen, "om_terrain").empty());
                return invoke(
                    "editor:new_set", {.row = "om_terrain", .text = "editor_session_test_new"});
            case 12:
                return invoke("editor:new_apply");
            case 13: {
                const auto written = nlohmann::ordered_json::parse(read_file(built))[0];
                CHECK(written["type"] == "city_building");
                CHECK(editor::special_squares(written).size() == 1);
                CHECK(body<ec::list_body>(part(screen, "editor:diagnostics")).rows.empty());
                return act("QUIT");
            }
            default:
                FAIL("the editor outlived its exit");
                return act("QUIT");
        }
    });

    editor::run_editor_session();
    CHECK(step == 14);
}

TEST_CASE(
    "the connect tool joins only beside a special, and the editor keeps the level drawn on",
    "[editor_session][editor_special]") {
    const auto guard = screen_guard{};
    const auto restore_mapgen = restore_on_out_of_scope<bool>(disable_mapgen);
    disable_mapgen = false;
    auto special = special_levels()[0];
    // The connection on a square names nothing, as a hand-written one may.
    special["connections"] =
        {{{"point", {0, 0, -1}}, {"connection", "local_road"}}, {{"point", {1, 0, 0}}}};
    // A mapgen comes first, so that the file reopens on an entry of another catalog.
    auto entries = house_levels();
    entries.push_back(special);
    const auto file = levels_file{entries};
    const auto special_row = "object:" + std::to_string(entries.size() - 1);
    const auto level = [](const ec::screen_view& screen) {
        const auto& stats = body<ec::panel_body>(part(screen, "editor:document")).stats;
        const auto found = std::ranges::find(stats, "level", &ec::panel_stat::key);
        return found == stats.end() ? std::string("none") : found->value;
    };
    const auto notice = [](const ec::screen_view& screen) {
        return ec::find_component(*screen.root, "editor:notice") != nullptr;
    };
    const auto saved = [](const ec::screen_view& screen) {
        return body<ec::panel_body>(part(screen, "editor:document")).stats.front().value;
    };
    auto step = 0;

    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto screen = ec::top_screen(captured());
        switch (step++) {
            case 0:
                return invoke("editor:open", {.text = file.path.string()});
            case 1:
                return invoke("editor:object", {.row = special_row});
            case 2:
                // A level with only a connection on it is a level of the special.
                CHECK(affordance(screen, "editor:level_down").enabled);
                return invoke("editor:tool", {.row = "tool:connect"});
            case 3:
                return invoke("editor:paint", {.row = "square:0,0"});
            case 4:
                CHECK(notice(screen));
                CHECK(saved(screen) == "Yes");
                return invoke("editor:connection_set", {.text = ""});
            case 5:
                return invoke("editor:paint", {.row = "square:2,0"});
            case 6:
                CHECK(notice(screen));
                CHECK(saved(screen) == "Yes");
                // A connection that sits on a square is taken away all the same.
                return invoke("editor:paint", {.row = "square:1,0"});
            case 7:
                CHECK_FALSE(notice(screen));
                CHECK(saved(screen) == "No");
                return invoke("editor:connection_set", {.text = "local_road"});
            case 8:
                return invoke("editor:level_up");
            case 9:
                return invoke("editor:level_up");
            case 10:
                CHECK(level(screen) == "2");
                return invoke("editor:paint", {.row = "square:1,0"});
            case 11:
                return invoke("editor:catalog_filter", {.text = "zzz"});
            case 12:
                return invoke("editor:save");
            case 13: {
                // The file changes on disk; the editor reads it again on the level it showed.
                auto out = std::ofstream(file.path, std::ios::binary | std::ios::app);
                out << "\n";
                return invoke("editor:tool", {.row = "tool:open"});
            }
            case 14:
                CHECK(level(screen) == "2");
                // The special's catalog keeps its filter over the reload.
                CHECK(body<ec::list_body>(part(screen, "editor:catalog")).filter == "zzz");
                return invoke("editor:level_down");
            case 15:
                return invoke("editor:level_down");
            case 16:
                CHECK(level(screen) == "0");
                return invoke("editor:open_square", {.row = "square:0,0"});
            case 17:
                // The mapgen's catalog does not keep the special's filter.
                CHECK(body<ec::list_body>(part(screen, "editor:catalog")).filter.empty());
                return act("QUIT");
            default:
                FAIL("the editor outlived its exit");
                return act("QUIT");
        }
    });

    editor::run_editor_session();
    CHECK(step == 18);
}

TEST_CASE(
    "the editor places the open special on new land near the avatar, and tests it there",
    "[editor_session][editor_special]") {
    // The test walks the avatar off its map; later cases start on theirs, once the screen is back.
    const auto walked_back = on_out_of_scope([] { clear_all_state(); });
    const auto guard = screen_guard{};
    const auto restore_mapgen = restore_on_out_of_scope<bool>(disable_mapgen);
    disable_mapgen = false;
    const auto file = levels_file{special_levels()};
    const auto omt = get_avatar().abs_omt_pos();
    auto step = 0;

    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto screen = ec::top_screen(captured());
        switch (step++) {
            case 0:
                return invoke("editor:open", {.text = file.path.string()});
            case 1:
                CHECK_FALSE(affordance(screen, "editor:test").enabled);
                CHECK(affordance(screen, "editor:test").denial
                      == "Place the special in the world first.");
                CHECK(affordance(screen, "editor:place_special").keys
                      == std::vector<std::string>{"F6"});
                CHECK(affordance(screen, "editor:test").keys == std::vector<std::string>{"F5"});
                return invoke("editor:place_special");
            case 2: {
                CHECK_FALSE(ec::find_component(*screen.root, "editor:notice"));
                // Placing says where the special went and how to get there.
                const auto* const said = ec::find_component(*screen.root, "editor:status");
                REQUIRE(said);
                CHECK(std::get<ec::text_body>(said->body).markup
                      == "Placed editor_session_test_special nearby; Test takes you there.");
                CHECK(affordance(screen, "editor:test").enabled);
                return invoke("editor:test");
            }
            default:
                FAIL("the editor outlived its test");
                return act("QUIT");
        }
    });

    editor::run_editor_session();
    CHECK(step == 3);
    const auto at = get_avatar().abs_omt_pos();
    auto& buffer = ACTIVE_OVERMAP_BUFFER;
    // Land the avatar already walked keeps what was made there.
    CHECK(at != omt);
    CHECK(buffer.ter(at) == oter_id("house_01_north"));
    CHECK(buffer.ter(at + tripoint_rel_omt(1, 0, 0)) == oter_id("field"));
    CHECK(buffer.ter(at + tripoint_rel_omt(0, 0, 1)) == oter_id("house_01_roof_north"));
    CHECK(buffer.overmap_special_at(at) == overmap_special_id("editor_session_test_special"));
    editor::reset_editor_session();
}

TEST_CASE("the editor tests only the special it placed", "[editor_session][editor_special]") {
    const auto guard = screen_guard{};
    auto entries = special_levels();
    auto other = entries[0];
    other["id"] = "editor_session_test_other";
    entries.push_back(other);
    const auto file = levels_file{entries};
    auto step = 0;

    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto screen = ec::top_screen(captured());
        switch (step++) {
            case 0:
                return invoke("editor:open", {.text = file.path.string()});
            case 1:
                return invoke("editor:place_special");
            case 2:
                CHECK(affordance(screen, "editor:test").enabled);
                return invoke("editor:object", {.row = "object:1"});
            case 3:
                // The other special was not placed, so there is nothing of it to test.
                CHECK_FALSE(affordance(screen, "editor:test").enabled);
                return invoke("editor:object", {.row = "object:0"});
            case 4:
                CHECK(affordance(screen, "editor:test").enabled);
                return act("QUIT");
            default:
                FAIL("the editor outlived its exit");
                return act("QUIT");
        }
    });

    editor::run_editor_session();
    CHECK(step == 5);
    editor::reset_editor_session();
}

TEST_CASE(
    "the editor shows entries without a string id, placed or not",
    "[editor_session][editor_special]") {
    const auto guard = screen_guard{};
    auto entries = special_levels();
    entries.push_back({{"type", "overmap_terrain"}, {"name", "unnamed terrain"}});
    const auto file = levels_file{entries};
    auto step = 0;

    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto screen = ec::top_screen(captured());
        switch (step++) {
            case 0:
                return invoke("editor:open", {.text = file.path.string()});
            case 1:
                return invoke("editor:place_special");
            case 2:
                return invoke("editor:field_set", {.row = "pointer", .text = "/id"});
            case 3:
                return invoke("editor:field_set", {.row = "value", .text = "5"});
            case 4:
                return invoke("editor:field_apply");
            case 5:
                // A special named by no string is refused, so the one placed stays open.
                CHECK(ec::find_component(*screen.root, "editor:notice"));
                CHECK(affordance(screen, "editor:test").enabled);
                return invoke("editor:object", {.row = "object:1"});
            case 6:
                CHECK(body<ec::list_body>(part(screen, "editor:objects")).rows.size() == 2);
                CHECK_FALSE(ec::find_component(*screen.root, "editor:terrains"));
                return act("QUIT");
            default:
                FAIL("the editor outlived its exit");
                return act("QUIT");
        }
    });

    editor::run_editor_session();
    CHECK(step == 7);
    editor::reset_editor_session();
}

TEST_CASE("the editor places no special that copies another", "[editor_session][editor_special]") {
    const auto guard = screen_guard{};
    auto entries = special_levels();
    entries[0]["copy-from"] = "Hunting Lodge";
    const auto file = levels_file{entries};
    auto step = 0;

    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto screen = ec::top_screen(captured());
        switch (step++) {
            case 0:
                return invoke("editor:open", {.text = file.path.string()});
            case 1:
                return invoke("editor:place_special");
            case 2:
                CHECK(body<ec::text_body>(part(screen, "editor:notice")).markup
                      == "A special that copies another is placed with the game's data only.");
                CHECK_FALSE(affordance(screen, "editor:test").enabled);
                return act("QUIT");
            default:
                FAIL("the editor outlived its exit");
                return act("QUIT");
        }
    });

    editor::run_editor_session();
    CHECK(step == 3);
    editor::reset_editor_session();
}

TEST_CASE(
    "the editor lists a built-in or Lua mapgen read-only with where it is declared and defined",
    "[editor_session][editor_mapgen]") {
    const auto guard = screen_guard{};
    const auto restore_mapgen = restore_on_out_of_scope<bool>(disable_mapgen);
    disable_mapgen = false;
    const auto rows = [](const ec::screen_view& screen) {
        return body<ec::list_body>(part(screen, "editor:code_mapgens")).rows;
    };
    auto step = 0;

    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto screen = ec::top_screen(captured());
        switch (step++) {
            case 0:
                return invoke("editor:open_oter", {.text = "lake_shore"});
            case 1: {
                CHECK_FALSE(ec::find_component(*screen.root, "editor:notice"));
                const auto shown = rows(screen);
                REQUIRE(shown.size() == 1);
                CHECK(shown[0].label == "Built-in mapgen lake_shore");
                CHECK(shown[0].description.find("overmap_terrain_waterbody.json:11")
                      != std::string::npos);
                CHECK_FALSE(has_affordance(screen, "editor:paint"));
                return invoke("editor:open_oter", {.text = "slimepit"});
            }
            case 2: {
                const auto shown = rows(screen);
                REQUIRE(shown.size() == 1);
                CHECK(shown[0].label == "Lua mapgen slimepit");
                // Where the JSON names it, and where Lua defines the function.
                CHECK(shown[0].description.find("mapgen/slimepit.json:2") != std::string::npos);
                CHECK(shown[0].description.find("preload.lua:43") != std::string::npos);
                return invoke(
                    "editor:open",
                    {.text =
                         (std::filesystem::path(PATH_INFO::datadir()) / "json/mapgen/slimepit.json")
                             .string()});
            }
            case 3: {
                // A file of code mapgens opens on them rather than on nothing.
                CHECK_FALSE(ec::find_component(*screen.root, "editor:notice"));
                const auto shown = rows(screen);
                REQUIRE(shown.size() == 1);
                CHECK(shown[0].label == "Lua mapgen slimepit");
                return invoke("editor:open_oter", {.text = "2StoryModern06_1_north"});
            }
            case 4:
                // A JSON mapgen leaves the list.
                CHECK_FALSE(ec::find_component(*screen.root, "editor:code_mapgens"));
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
    "the editor opens an overmap terrain entry, checks it alone and opens its terrains' mapgens",
    "[editor_session][editor_special]") {
    const auto guard = screen_guard{};
    const auto restore_mapgen = restore_on_out_of_scope<bool>(disable_mapgen);
    disable_mapgen = false;
    const auto file = levels_file{nlohmann::ordered_json::array(
        {{{"type", "overmap_terrain"},
          {"id", "editor_session_test_oter"},
          {"name", "test terrain"},
          {"color", "red"},
          {"flags", {"NO_SUCH_FLAG"}}}})};
    const auto data = std::filesystem::path(PATH_INFO::datadir()) / "json/overmap/overmap_terrain";
    const auto path = [](const ec::screen_view& screen) {
        return body<ec::form_body>(part(screen, "editor:file")).fields.front().value;
    };
    const auto rows = [](const ec::screen_view& screen, const std::string& id) {
        return body<ec::list_body>(part(screen, id)).rows;
    };
    auto step = 0;

    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto screen = ec::top_screen(captured());
        switch (step++) {
            case 0:
                return invoke("editor:open", {.text = file.path.string()});
            case 1: {
                CHECK_FALSE(ec::find_component(*screen.root, "editor:notice"));
                REQUIRE(rows(screen, "editor:objects").size() == 1);
                CHECK(rows(screen, "editor:objects")[0].label == "editor_session_test_oter");
                // What the loader says against the entry: no symbol, an unknown flag.
                const auto said = rows(screen, "editor:diagnostics");
                CHECK(std::ranges::any_of(said, [](const ec::row& row) {
                    return row.label.find("NO_SUCH_FLAG") != std::string::npos;
                }));
                CHECK_FALSE(has_affordance(screen, "editor:paint"));
                // A terrain the game does not know yet has no mapgen to open.
                CHECK_FALSE(ec::find_component(*screen.root, "editor:legend"));
                return invoke(
                    "editor:open", {.text = (data / "overmap_terrain_waterbody.json").string()});
            }
            case 2:
                // Checking an entry registers none of its mapgens again.
                CHECK(editor::code_mapgens_of("lake_shore").size() == 1);
                CHECK(rows(screen, "editor:diagnostics").empty());
                return invoke(
                    "editor:open", {.text = (data / "overmap_terrain_residential.json").string()});
            case 3: {
                const auto objects = rows(screen, "editor:objects");
                const auto houses = std::ranges::find_if(objects, [](const ec::row& row) {
                    return row.label.find("house_01") != std::string::npos;
                });
                REQUIRE(houses != objects.end());
                return invoke("editor:object", {.row = houses->id});
            }
            case 4: {
                const auto terrains = rows(screen, "editor:terrains");
                CHECK(
                    std::ranges::contains(terrains, std::string("terrain:house_01"), &ec::row::id));
                return invoke("editor:open_terrain", {.row = "terrain:house_01"});
            }
            case 5:
                CHECK(path(screen).ends_with("mapgen/house/house01.json"));
                return act("QUIT");
            default:
                FAIL("the editor outlived its exit");
                return act("QUIT");
        }
    });

    editor::run_editor_session();
    CHECK(step == 6);
}

namespace {
/// The slot of the part `id` of the screen's root split.
auto slot_of(const ec::screen_view& screen, const std::string& id) -> std::string {
    REQUIRE(screen.root);
    const auto found = std::ranges::find(screen.root->children, id, &ec::component::id);
    INFO(id);
    REQUIRE(found != screen.root->children.end());
    return found->slot;
}
} // namespace

TEST_CASE(
    "the map editor names what each part is for and the menu of each action", "[editor_session]") {
    const auto guard = screen_guard{};
    const auto file = mod_file{};
    auto step = 0;

    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto screen = ec::top_screen(captured());
        switch (step++) {
            case 0:
                // Nothing is open: the files to open are the page, with the form that makes one.
                CHECK(slot_of(screen, "editor:browser") == "canvas");
                CHECK(slot_of(screen, "editor:new") == "inspector");
                CHECK(slot_of(screen, "editor:file") == "outline");
                CHECK(part(screen, "editor:new").label == "New file");
                // A field is named by what it asks, not by the heading of its form.
                for (const auto* const form : {"editor:new", "editor:file"}) {
                    INFO(form);
                    CHECK(body<ec::form_body>(part(screen, form)).fields.front().label == "Path");
                }
                CHECK(affordance(screen, "editor:vehicle").group == "World");
                return invoke("editor:open", {.text = file.path.string()});
            case 1:
                CHECK(slot_of(screen, "editor:tools") == "canvas");
                CHECK(slot_of(screen, "editor:legend") == "palette");
                CHECK(slot_of(screen, "editor:catalog_kinds") == "palette");
                CHECK(slot_of(screen, "editor:objects") == "outline");
                CHECK(slot_of(screen, "editor:browser") == "outline");
                CHECK(slot_of(screen, "editor:field") == "inspector");
                CHECK(slot_of(screen, "editor:diagnostics") == "log");
                CHECK(slot_of(screen, "editor:history") == "log");
                CHECK(slot_of(screen, "editor:document") == "status");
                // The parts of the columns are headed by what they hold.
                CHECK(part(screen, "editor:legend").label == "Legend");
                CHECK(part(screen, "editor:objects").label == "Entries");
                CHECK(part(screen, "editor:browser").label == "Files");
                CHECK(part(screen, "editor:history").label == "History");
                CHECK(affordance(screen, "editor:save").group == "File");
                CHECK(affordance(screen, "editor:export").group == "File");
                {
                    // The File menu starts with saving, as editors have it.
                    const auto file =
                        screen.affordances | std::views::filter([](const ec::affordance& entry) {
                            return entry.group == "File";
                        })
                        | std::views::transform(&ec::affordance::id) | std::views::take(2)
                        | std::ranges::to<std::vector>();
                    CHECK(file == std::vector<std::string>{"editor:save", "editor:save_as"});
                }
                CHECK(affordance(screen, "editor:undo").group == "Edit");
                CHECK(affordance(screen, "editor:brush").group == "Edit");
                CHECK(affordance(screen, "editor:level_up").group == "View");
                CHECK(affordance(screen, "editor:preview").group == "View");
                CHECK(affordance(screen, "editor:test").group == "World");
                CHECK(affordance(screen, "editor:stamp").group == "World");
                {
                    // The menus come in the order editors have them.
                    auto menus = std::vector<std::string>{};
                    for (const auto& entry : screen.affordances) {
                        if (!entry.group.empty() && !std::ranges::contains(menus, entry.group)) {
                            menus.push_back(entry.group);
                        }
                    }
                    CHECK(menus == std::vector<std::string>{"File", "Edit", "View", "World"});
                }
                // What a part runs on its rows, and the drag's own steps, sit in no menu.
                CHECK(affordance(screen, "editor:paint").group.empty());
                CHECK(affordance(screen, "editor:stroke_begin").group.empty());
                CHECK(affordance(screen, "editor:catalog_pick").group.empty());
                return act("QUIT");
            default:
                FAIL("the editor outlived its exit");
                return act("QUIT");
        }
    });

    editor::run_editor_session();
    CHECK(step == 2);
}

TEST_CASE("the editor's legend and catalog show what they place", "[editor_session]") {
    const auto guard = screen_guard{};
    const auto file = mod_file{};
    const auto row_of =
        [](const ec::screen_view& screen, const std::string& list, const std::string& id) {
            const auto& rows = body<ec::list_body>(part(screen, list)).rows;
            const auto found = std::ranges::find(rows, id, &ec::row::id);
            INFO(id);
            REQUIRE(found != rows.end());
            return *found;
        };
    auto step = 0;
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto screen = ec::top_screen(captured());
        switch (step++) {
            case 0:
                return invoke("editor:open", {.text = file.path.string()});
            case 1: {
                const auto floor = row_of(screen, "editor:legend", "symbol:.");
                REQUIRE(floor.look);
                CHECK(floor.look->id == "t_floor");
                // An id the legend names alone reads as its name, not as JSON text.
                const auto terrain = std::ranges::
                    find(floor.columns, "terrain", &game_client::interaction_column::key);
                REQUIRE(terrain != floor.columns.end());
                CHECK(terrain->value == "wooden floor");
                const auto& terrains = body<ec::list_body>(part(screen, "editor:catalog")).rows;
                REQUIRE_FALSE(terrains.empty());
                CHECK(std::ranges::all_of(terrains, [](const ec::row& row) {
                    return row.look && "entry:" + row.look->id.value_or("") == row.id;
                }));
                return invoke("editor:catalog_kind", {.row = "catalog:furniture"});
            }
            case 2: {
                const auto& furniture = body<ec::list_body>(part(screen, "editor:catalog")).rows;
                REQUIRE_FALSE(furniture.empty());
                CHECK(std::ranges::all_of(furniture, [](const ec::row& row) {
                    return row.look && "entry:" + row.look->id.value_or("") == row.id;
                }));
                return act("QUIT");
            }
            default:
                FAIL("the editor outlived its exit");
                return act("QUIT");
        }
    });
    editor::run_editor_session();
    CHECK(step == 3);
}

TEST_CASE("the editor's start page offers the map files opened lately", "[editor_session]") {
    const auto guard = screen_guard{};
    const auto file = mod_file{};
    const auto forget = [] { std::filesystem::remove(editor::recent_documents_file()); };
    forget();
    const auto cleanup = on_out_of_scope([&] {
        forget();
        editor::reset_editor_session();
    });
    editor::remember_document({.editor = "vehicle", .name = "bicycle"});
    editor::remember_document({.editor = "map", .name = file.path.generic_string()});
    auto step = 0;
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto screen = ec::top_screen(captured());
        switch (step++) {
            case 0: {
                // Only the map editor's documents, newest first, beside the files to open.
                CHECK(slot_of(screen, "editor:recent") == "canvas");
                CHECK(part(screen, "editor:recent").label == "Recent");
                const auto& rows = body<ec::list_body>(part(screen, "editor:recent")).rows;
                REQUIRE(rows.size() == 1);
                CHECK(rows[0].id == "file:" + file.path.generic_string());
                return invoke("editor:recent_open", {.row = rows[0].id});
            }
            case 1:
                CHECK(body<ec::form_body>(part(screen, "editor:file")).fields.front().value
                      == file.path.generic_string());
                CHECK_FALSE(ec::find_component(*screen.root, "editor:recent"));
                return act("QUIT");
            default:
                FAIL("the editor outlived its exit");
                return act("QUIT");
        }
    });
    editor::run_editor_session();
    CHECK(step == 2);
}

TEST_CASE("the editor names an entry whose name is empty as unnamed", "[editor_session]") {
    CHECK(editor::map_editor::entry_name({{"om_terrain", nlohmann::ordered_json::array()}})
          == "(unnamed)");
    const auto nested_empty = nlohmann::ordered_json::array({nlohmann::ordered_json::array()});
    CHECK(editor::map_editor::entry_name({{"om_terrain", nested_empty}}) == "(unnamed)");
    CHECK(editor::map_editor::entry_name({{"om_terrain", {"a", "b"}}}) == "a, b");
}

namespace {
/// A mod file of a mapgen of an overmap terrain, a nested chunk and an update mapgen, in that
/// order, removed with its directory when it ends.
struct kinds_file {
    std::filesystem::path path =
        std::filesystem::path(PATH_INFO::user_moddir()) / "editor_session_test" / "kinds.json";

    kinds_file() {
        const auto rows = [](const int side) {
            return std::vector<std::string>(side, std::string(side, '.'));
        };
        const auto entries = nlohmann::ordered_json::array(
            {{{"type", "mapgen"},
              {"method", "json"},
              {"om_terrain", "house_04"},
              {"object", {{"rows", rows(24)}, {"terrain", {{".", "t_floor"}}}}}},
             {{"type", "mapgen"},
              {"method", "json"},
              {"nested_mapgen_id", "editor_test_chunk"},
              {"object",
               {{"mapgensize", {4, 4}},
                {"rows", rows(4)},
                {"terrain", {{".", "t_floor"}, {"#", "t_wall"}}}}}},
             {{"type", "mapgen"},
              {"method", "json"},
              {"update_mapgen_id", "editor_test_chunk"},
              {"object",
               {{"place_nested", {{{"chunks", {"editor_test_chunk"}}, {"x", 0}, {"y", 0}}}}}}}});
        std::filesystem::create_directories(path.parent_path());
        auto out = std::ofstream(path, std::ios::binary | std::ios::trunc);
        out << format_json(entries.dump()).value();
    }
    ~kinds_file() { std::filesystem::remove_all(path.parent_path()); }
    kinds_file(const kinds_file&) = delete;
    auto operator=(const kinds_file&) -> kinds_file& = delete;
};
} // namespace

TEST_CASE(
    "the editor turns away putting a nested chunk or an update mapgen in the world, saying why",
    "[editor_session][engine_client_component]") {
    const auto guard = screen_guard{};
    const auto file = kinds_file{};
    auto step = 0;
    const auto notice = [](const ec::screen_view& screen) {
        const auto* const found = ec::find_component(*screen.root, "editor:notice");
        return found ? body<ec::text_body>(*found).markup : std::string();
    };
    const auto off = [](const ec::screen_view& screen, const std::string& id) {
        const auto& found = affordance(screen, id);
        return !found.enabled && !found.denial.empty();
    };

    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto screen = ec::top_screen(captured());
        switch (step++) {
            case 0:
                return invoke("editor:open", {.text = file.path.string()});
            case 1:
                CHECK(affordance(screen, "editor:stamp").enabled);
                CHECK(affordance(screen, "editor:preview").enabled);
                return invoke("editor:preview", {.count = 1});
            case 2:
                CHECK(notice(screen).empty());
                return invoke("editor:object", {.row = "object:1"});
            case 3:
                // A chunk is placed by a mapgen: the game takes it, the world and a preview do not.
                CHECK(notice(screen).empty());
                CHECK(affordance(screen, "editor:apply").enabled);
                CHECK(off(screen, "editor:stamp"));
                CHECK(off(screen, "editor:preview"));
                CHECK(off(screen, "editor:test"));
                CHECK(affordance(screen, "editor:stamp").denial.find("chunk") != std::string::npos);
                return invoke("editor:object", {.row = "object:2"});
            case 4:
                CHECK(notice(screen).empty());
                CHECK(off(screen, "editor:apply"));
                CHECK(off(screen, "editor:stamp"));
                CHECK(off(screen, "editor:preview"));
                CHECK(off(screen, "editor:test"));
                CHECK(
                    affordance(screen, "editor:stamp").denial.find("update") != std::string::npos);
                return act("QUIT");
            default:
                FAIL("the editor outlived its exit");
                return act("QUIT");
        }
    });
    editor::run_editor_session();
    CHECK(step == 5);
    editor::reset_editor_session();
}

TEST_CASE(
    "the editor's entries say what kind each is and number only the same kind of a name",
    "[editor_session][engine_client_component]") {
    const auto guard = screen_guard{};
    const auto file = kinds_file{};
    auto step = 0;

    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto screen = ec::top_screen(captured());
        switch (step++) {
            case 0:
                return invoke("editor:open", {.text = file.path.string()});
            case 1: {
                const auto& rows = body<ec::list_body>(part(screen, "editor:objects")).rows;
                REQUIRE(rows.size() == 3);
                CHECK(rows[0].description == "Overmap terrain mapgen");
                // A chunk and the update mapgen of its name are two entries, not two of one.
                CHECK(rows[1].label == "editor_test_chunk");
                CHECK(rows[1].description == "Nested chunk");
                CHECK(rows[2].label == "editor_test_chunk");
                CHECK(rows[2].description == "Update mapgen");
                return act("QUIT");
            }
            default:
                FAIL("the editor outlived its exit");
                return act("QUIT");
        }
    });
    editor::run_editor_session();
    CHECK(step == 2);
    editor::reset_editor_session();
}

TEST_CASE(
    "the editor's undo and redo show the entry they changed and say what they did",
    "[editor_session][engine_client_component]") {
    const auto guard = screen_guard{};
    const auto file = kinds_file{};
    auto step = 0;
    const auto selected = [](const ec::screen_view& screen) {
        const auto& rows = body<ec::list_body>(part(screen, "editor:objects")).rows;
        const auto found = std::ranges::find_if(rows, &ec::row::selected);
        return found == rows.end() ? std::string() : found->id;
    };
    const auto status = [](const ec::screen_view& screen) {
        const auto* const found = ec::find_component(*screen.root, "editor:status");
        return found ? body<ec::text_body>(*found).markup : std::string();
    };

    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto screen = ec::top_screen(captured());
        switch (step++) {
            case 0:
                return invoke("editor:open", {.text = file.path.string()});
            case 1:
                return invoke("editor:object", {.row = "object:1"});
            case 2:
                return invoke("editor:symbol", {.row = "symbol:#"});
            case 3:
                return invoke("editor:paint", {.row = "square:1,1"});
            case 4:
                CHECK(affordance(screen, "editor:undo").enabled);
                return invoke("editor:object", {.row = "object:0"});
            case 5:
                return invoke("editor:undo");
            case 6:
                // The chunk painted is shown again, so the player sees what came back.
                CHECK(selected(screen) == "object:1");
                CHECK(status(screen).starts_with("Undid: "));
                return invoke("editor:object", {.row = "object:0"});
            case 7:
                return invoke("editor:redo");
            case 8:
                CHECK(selected(screen) == "object:1");
                CHECK(status(screen).starts_with("Redid: "));
                // Undone again, so that leaving asks nothing.
                return invoke("editor:undo");
            case 9:
                return act("QUIT");
            default:
                FAIL("the editor outlived its exit");
                return act("QUIT");
        }
    });
    editor::run_editor_session();
    CHECK(step == 10);
    editor::reset_editor_session();
}

TEST_CASE(
    "the editor's legend says a nested chunk's unmapped symbol keeps the map under it",
    "[editor_session][engine_client_component]") {
    const auto guard = screen_guard{};
    const auto path =
        std::filesystem::path(PATH_INFO::user_moddir()) / "editor_session_test" / "chunk.json";
    std::filesystem::create_directories(path.parent_path());
    {
        auto out = std::ofstream(path, std::ios::binary | std::ios::trunc);
        out << format_json(
                   nlohmann::ordered_json::array(
                       {{{"type", "mapgen"},
                         {"method", "json"},
                         {"nested_mapgen_id", "editor_test_chunk"},
                         {"object",
                          {{"mapgensize", {2, 2}},
                           {"rows", {"x.", ".."}},
                           {"terrain", {{".", "t_floor"}}}}}}})
                       .dump())
                   .value();
    }
    const auto removed = on_out_of_scope([&] { std::filesystem::remove_all(path.parent_path()); });
    auto step = 0;
    const auto use_of = [](const ec::screen_view& screen, const std::string& symbol) {
        const auto& rows = body<ec::list_body>(part(screen, "editor:legend")).rows;
        const auto row = std::ranges::find(rows, "symbol:" + symbol, &ec::row::id);
        REQUIRE(row != rows.end());
        const auto use = std::ranges::
            find(row->columns, std::string("use"), &game_client::interaction_column::key);
        return use == row->columns.end() ? std::string() : use->value;
    };

    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto screen = ec::top_screen(captured());
        switch (step++) {
            case 0:
                return invoke("editor:open", {.text = path.string()});
            case 1:
                // The game leaves the squares of such a symbol as the map around the chunk has
                // them.
                CHECK(use_of(screen, "x") == "Keeps the map");
                return act("QUIT");
            default:
                FAIL("the editor outlived its exit");
                return act("QUIT");
        }
    });
    editor::run_editor_session();
    CHECK(step == 2);
    editor::reset_editor_session();
}

TEST_CASE(
    "the editor turns away drawing on an entry without rows, saying why",
    "[editor_session][engine_client_component]") {
    const auto guard = screen_guard{};
    const auto file = kinds_file{};
    auto step = 0;
    const auto tab = [](const ec::screen_view& screen, const std::string& id) {
        const auto& tabs = body<ec::tabs_body>(part(screen, "editor:tools")).tabs;
        const auto found = std::ranges::find(tabs, id, &ec::tab_entry::id);
        REQUIRE(found != tabs.end());
        return *found;
    };

    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto screen = ec::top_screen(captured());
        switch (step++) {
            case 0:
                return invoke("editor:open", {.text = file.path.string()});
            case 1:
                CHECK(tab(screen, "tool:point").enabled);
                return invoke("editor:object", {.row = "object:2"});
            case 2:
                // The update mapgen only places chunks: there is nothing to draw, and it says so.
                CHECK_FALSE(tab(screen, "tool:point").enabled);
                CHECK(tab(screen, "tool:point").denial.find("rows") != std::string::npos);
                CHECK_FALSE(tab(screen, "tool:select").enabled);
                CHECK(tab(screen, "tool:placements").enabled);
                // The tool shown is one that works on it.
                CHECK(tab(screen, "tool:placements").selected);
                CHECK_FALSE(tab(screen, "tool:point").selected);
                return act("QUIT");
            default:
                FAIL("the editor outlived its exit");
                return act("QUIT");
        }
    });
    editor::run_editor_session();
    CHECK(step == 3);
    editor::reset_editor_session();
}

TEST_CASE(
    "the editor turns away applying an update mapgen on save, saying why",
    "[editor_session][engine_client_component]") {
    const auto guard = screen_guard{};
    const auto file = kinds_file{};
    auto step = 0;
    const auto notice = [](const ec::screen_view& screen) {
        const auto* const found = ec::find_component(*screen.root, "editor:notice");
        return found ? body<ec::text_body>(*found).markup : std::string();
    };

    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto screen = ec::top_screen(captured());
        switch (step++) {
            case 0:
                return invoke("editor:open", {.text = file.path.string()});
            case 1:
                CHECK(affordance(screen, "editor:apply_on_save").enabled);
                return invoke("editor:apply_on_save");
            case 2:
                return invoke("editor:object", {.row = "object:2"});
            case 3: {
                // The game runs an update mapgen on squares it made: saving it applies nothing.
                const auto& toggle = affordance(screen, "editor:apply_on_save");
                CHECK_FALSE(toggle.enabled);
                CHECK(toggle.denial == affordance(screen, "editor:apply").denial);
                return invoke("editor:save");
            }
            case 4:
                CHECK(notice(screen).empty());
                return act("QUIT");
            default:
                FAIL("the editor outlived its exit");
                return act("QUIT");
        }
    });
    editor::run_editor_session();
    CHECK(step == 5);
    editor::reset_editor_session();
}

TEST_CASE(
    "the editor says why it cannot save, undo or redo now",
    "[editor_session][engine_client_component]") {
    const auto guard = screen_guard{};
    const auto file = kinds_file{};
    auto step = 0;

    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto screen = ec::top_screen(captured());
        switch (step++) {
            case 0:
                return invoke("editor:open", {.text = file.path.string()});
            case 1:
                CHECK(affordance(screen, "editor:undo").denial == "Nothing to undo.");
                CHECK(affordance(screen, "editor:redo").denial == "Nothing to redo.");
                CHECK(affordance(screen, "editor:save").denial
                      == "Nothing changed since the file was saved.");
                return act("QUIT");
            default:
                FAIL("the editor outlived its exit");
                return act("QUIT");
        }
    });
    editor::run_editor_session();
    CHECK(step == 2);
    editor::reset_editor_session();
}

TEST_CASE(
    "the editor keeps a refusal until an action changes something or refuses anew",
    "[editor_session][engine_client_component]") {
    const auto guard = screen_guard{};
    const auto file = kinds_file{};
    auto step = 0;
    const auto notice = [](const ec::screen_view& screen) {
        const auto* const found = ec::find_component(*screen.root, "editor:notice");
        return found ? body<ec::text_body>(*found).markup : std::string();
    };
    const auto refused = std::string("cabin_palette is a palette already.");

    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto screen = ec::top_screen(captured());
        switch (step++) {
            case 0:
                return invoke("editor:open", {.text = file.path.string()});
            case 1:
                return invoke("editor:extract_palette", {.text = "cabin_palette"});
            case 2:
                CHECK(notice(screen) == refused);
                return invoke("editor:tool", {.row = "tool:select"});
            case 3:
                // Picking a tool changes nothing the refusal was about.
                CHECK(notice(screen) == refused);
                return invoke("editor:extract_palette", {.text = "editor_session_test_kept"});
            case 4:
                CHECK(notice(screen).empty());
                return invoke("editor:undo");
            case 5:
                return act("QUIT");
            default:
                FAIL("the editor outlived its exit");
                return act("QUIT");
        }
    });
    editor::run_editor_session();
    CHECK(step == 6);
    editor::reset_editor_session();
}

#endif // CATA_MCP
