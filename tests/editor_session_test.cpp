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
#    include "editor/editor_session.h"
#    include "editor/mapgen_levels.h"
#    include "editor/mapgen_preview.h"
#    include "editor/recent_documents.h"
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
#    include "path_info.h"
#    include "player_activity.h"
#    include "point.h"
#    include "state_helpers.h"
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
                // The canvas says it is one, so a client gives it the room a drawing needs.
                CHECK(wire_part(state, "editor:canvas").value("role", "") == "canvas");
                // The legend resolves each symbol as the loader does, the first one picked.
                const auto& legend = body<ec::list_body>(part(screen, "editor:legend")).rows;
                REQUIRE(legend.size() == 2);
                CHECK(legend[0].id == "symbol:#");
                CHECK(legend[0].columns.front().value == "\"t_wall\"");
                CHECK(legend[0].selected);
                CHECK(body<ec::list_body>(part(screen, "editor:diagnostics")).rows.empty());
                const auto& tools = body<ec::tabs_body>(part(screen, "editor:tools")).tabs;
                REQUIRE(tools.size() == 10);
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
                CHECK(affordance(screen, "editor:loot").args.row->component_id == "editor:canvas");
                return invoke("editor:preview", {.count = 5});
            case 2:
                return invoke("editor:loot", {.row = "square:3,3"});
            case 3:
                first = lines(screen);
                REQUIRE_FALSE(first.empty());
                for (const auto& row : rows(screen)) {
                    CHECK(itype_id(row.id.substr(std::string_view("loot:").size())).is_valid());
                    CHECK(row.columns.size() == 3);
                }
                // The same seed rolls the same list.
                return invoke("editor:loot", {.row = "square:3,3"});
            case 4:
                CHECK(lines(screen) == first);
                // A square nothing spawns on has an empty list.
                return invoke("editor:loot", {.row = "square:10,10"});
            case 5:
                CHECK(rows(screen).empty());
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
                CHECK(use(screen, "symbol:z") == "Undefined");
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
                REQUIRE(tools.size() == 10);
                CHECK(tools[5].id == "tool:room");
                CHECK(stat(screen, "inside") == "#");
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
            case 2:
                CHECK_FALSE(notice(screen).empty());
                CHECK(live() == before);
                // A file's array of entries, as an export writes it, stamps its mapgen.
                return invoke(
                    "editor:import_set",
                    {.row = "import", .text = nlohmann::ordered_json::array({walls}).dump()});
            case 3:
                return invoke("editor:import");
            case 4:
                CHECK(notice(screen).empty());
                CHECK(body<ec::text_body>(part(screen, "editor:status")).markup == "Stamped.");
                CHECK(live() == ter_id("t_wall"));
                CHECK(affordance(screen, "editor:unstamp").enabled);
                return invoke("editor:unstamp");
            case 5:
                CHECK(live() == before);
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

    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto state = captured();
        const auto screen = ec::top_screen(state);
        switch (step++) {
            case 0:
                return invoke("editor:open", {.text = file.path.string()});
            case 1:
                CHECK_FALSE(affordance(screen, "editor:unstamp").enabled);
                CHECK_FALSE(affordance(screen, "editor:test").enabled);
                CHECK_FALSE(stamped(screen));
                return invoke("editor:paint", {.row = "square:2,3"});
            case 2:
                return invoke("editor:apply");
            case 3:
                CHECK_FALSE(ec::find_component(*screen.root, "editor:notice"));
                CHECK(live(2, 3) == before);
                return invoke("editor:stamp");
            case 4:
                CHECK_FALSE(ec::find_component(*screen.root, "editor:notice"));
                CHECK(live(2, 3) == ter_id("t_wall"));
                CHECK(live(3, 3) == ter_id("t_floor"));
                CHECK(stamped(screen));
                CHECK(affordance(screen, "editor:unstamp").enabled);
                return invoke("editor:unstamp");
            case 5:
                CHECK(live(2, 3) == before);
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
                CHECK(files(screen).total > 1000);
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
                CHECK(files(screen).rows[0].label == "json/mapgen/house/2storymodern06.json");
                CHECK(refused(state, "editor:browser_page", {.count = 1}));
                return invoke("editor:browser_open", {.row = files(screen).rows[0].id});
            case 4:
                CHECK(path(screen).ends_with("mapgen/house/2storymodern06.json"));
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
                CHECK(kind->options.size() == 3);
                return invoke("editor:new_set", {.row = "path", .text = made.string()});
            }
            case 1:
                return invoke("editor:new_set", {.row = "om_terrain", .text = "editor_test_chunk"});
            case 2:
                return invoke("editor:new_set", {.row = "kind", .text = "special"});
            case 3:
                return invoke("editor:new_apply");
            case 4:
                CHECK(notice(screen) == "A new file holds a mapgen, a nested chunk or a palette.");
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
                CHECK(line(screen, "x").columns.front().value == R"("t_dirt")");
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
                CHECK(line(screen, "x").columns.front().value == R"("t_grass")");
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
    "the main-menu editor game opens the editor first, leaves to the map and comes back",
    "[editor_session][gamemode]") {
    // Put back after the screen guard, whose test mode keeps clearing the avatar from prompting.
    const auto origin = get_map().get_abs_sub();
    const auto name = get_avatar().name;
    const auto restore = on_out_of_scope([&] {
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
    const auto at = get_avatar().abs_pos();
    auto step = 0;

    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto screen = ec::top_screen(captured());
        switch (step++) {
            case 0:
                // The map was not asked for input first.
                CHECK(screen.kind == "editor");
                return invoke("editor:open", {.text = file.path.string()});
            default:
                return act("QUIT");
        }
    });
    handle_action(std::move(mode));
    CHECK(step == 2);

    // Left, the editor shows the map where the avatar stands; the editor action comes back to the
    // same document.
    step = 0;
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto screen = ec::top_screen(captured());
        switch (step++) {
            case 0:
                CHECK(screen.role == "map");
                return act("editor");
            case 1:
                CHECK(body<ec::form_body>(part(screen, "editor:file")).fields.front().value
                      == file.path.generic_string());
                return act("QUIT");
            default:
                FAIL("the editor outlived its exit");
                return act("QUIT");
        }
    });
    handle_action(std::make_unique<special_game>());
    CHECK(step == 2);
    CHECK(get_avatar().get_moves() == 100);
    CHECK(get_avatar().abs_pos() == at);
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
                   return row.id + " " + row.columns.front().value + " " + row.description;
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
                        R"(symbol:# "t_wall" From editor_session_test_extracted)",
                        R"(symbol:. "t_floor" From editor_session_test_extracted)",
                        R"(symbol:c "f_chair" From editor_session_test_extracted)"});
                CHECK(body<ec::list_body>(part(screen, "editor:diagnostics")).rows.empty());
                const auto& objects = body<ec::list_body>(part(screen, "editor:objects")).rows;
                REQUIRE(objects.size() == 2);
                CHECK(objects[0].selected);
                CHECK(objects[1].label == "editor_session_test_extracted");
                CHECK(body<ec::list_body>(part(screen, "editor:history")).rows.size() == 2);
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

/// A mod file of a house's ground floor beside another house, its roof, open air on its west half,
/// and a nested chunk; removed when the test ends.
struct levels_file {
    std::filesystem::path path =
        std::filesystem::path(PATH_INFO::user_moddir()) / "editor_session_test" / "levels.json";

    levels_file() {
        auto ground = nlohmann::ordered_json::array();
        auto roof = nlohmann::ordered_json::array();
        for (auto row = 0; row < 24; ++row) {
            ground.push_back(std::string(48, '.'));
            roof.push_back(std::string(11, ' ') + "c" + std::string(12, 'r'));
        }
        const auto entries = nlohmann::ordered_json::array(
            {{{"type", "mapgen"},
              {"method", "json"},
              {"om_terrain",
               nlohmann::ordered_json::array(
                   {nlohmann::ordered_json::array({"house_01", "house_04"})})},
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
              {"object",
               {{"mapgensize", {1, 1}}, {"rows", {"#"}}, {"terrain", {{"#", "t_wall"}}}}}}});
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
        {.columns = {{{0, "tower"}, {1, "tower"}, {2, "tower_roof"}}}});
    CHECK(editor::terrain_level(index, "tower") == 0);
    CHECK(editor::terrain_above(index, "tower", 1) == "tower_roof");
    CHECK(editor::terrain_above(index, "tower_roof", -1) == "tower");
    CHECK(editor::terrain_above(index, "tower", -1) == std::nullopt);
}

TEST_CASE(
    "the level below an entry's square is another entry's even when the entry makes it too",
    "[editor_levels]") {
    const auto index = editor::level_index({.columns = {{{-1, "cellar"}, {0, "shop"}}}});
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

#endif // CATA_MCP
