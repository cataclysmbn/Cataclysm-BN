#if defined(CATA_MCP)

#    include "avatar.h"
#    include "cata_utility.h"
#    include "catch/catch.hpp"
#    include "client_input.h"
#    include "client_interaction.h"
#    include "client_memory.h"
#    include "client_memory_scope.h"
#    include "command_refusal.h"
#    include "cursesdef.h"
#    include "cursesport.h"
#    include "editor/editor_access.h"
#    include "editor/editor_session.h"
#    include "editor/recent_documents.h"
#    include "editor/vehicle_design.h"
#    include "editor/vehicle_session.h"
#    include "editor_heading_names.h"
#    include "engine_client_component.h"
#    include "engine_client_contract.h"
#    include "json_formatter.h"
#    include "locale_fixture.h"
#    include "locale_scope.h"
#    include "map/map.h"
#    include "map_helpers.h"
#    include "mod_manager.h"
#    include "output.h"
#    include "path_info.h"
#    include "point.h"
#    include "state_helpers.h"
#    include "string_formatter.h"
#    include "vehicle/veh_type.h"
#    include "vehicle/vehicle.h"
#    include "vehicle/vpart_position.h"

#    include <algorithm>
#    include <array>
#    include <chrono>
#    include <filesystem>
#    include <fstream>
#    include <functional>
#    include <nlohmann/json.hpp>
#    include <ranges>
#    include <set>
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
        editor::reset_vehicle_editor();
        test_mode = false;
        TERMX = 100;
        TERMY = 40;
        game_client::memory::resize(TERMX, TERMY);
        catacurses::stdscr = catacurses::newwin(TERMY, TERMX, point_zero);
        catacurses::newscr = catacurses::newwin(TERMY, TERMX, point_zero);
    }

    ~screen_guard() {
        game_client::memory::set_input_provider({});
        editor::reset_vehicle_editor();
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

auto captured() -> ec::boundary_state {
    const auto state = ec::capture_boundary(
        {.epoch = "epoch:vehicle_editor",
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
auto has_affordance(const ec::screen_view& screen, const std::string& id) -> bool {
    return std::ranges::contains(screen.affordances, id, &ec::affordance::id);
}
auto affordance(const ec::screen_view& screen, const std::string& id) -> const ec::affordance& {
    const auto found = std::ranges::find(screen.affordances, id, &ec::affordance::id);
    INFO(id);
    REQUIRE(found != screen.affordances.end());
    return *found;
}
auto rows(const ec::screen_view& screen, const std::string& id) -> const std::vector<ec::row>& {
    return body<ec::list_body>(part(screen, id)).rows;
}
auto row(const ec::screen_view& screen, const std::string& list, const std::string& id)
    -> const ec::row& {
    const auto& all = rows(screen, list);
    const auto found = std::ranges::find(all, id, &ec::row::id);
    INFO(id);
    REQUIRE(found != all.end());
    return *found;
}
auto cell(const ec::screen_view& screen, const std::string& id) -> const ec::grid_cell& {
    const auto& cells = body<ec::grid_body>(part(screen, "vehicle:grid")).cells;
    const auto found = std::ranges::find(cells, id, &ec::grid_cell::id);
    INFO(id);
    REQUIRE(found != cells.end());
    return *found;
}
/// Picking the row `row` of the affordance `id` as a client does, through the engine's reading of
/// it at the boundary just captured.
auto pick(const std::string& id, const std::string& row) -> input_event {
    const auto resolved =
        ec::resolve_invoke(captured(), {.affordance_id = id, .args = {.row = row}});
    INFO(row);
    REQUIRE(resolved.has_value());
    REQUIRE(std::holds_alternative<ec::invoke_operation>(*resolved));
    return invoke(id, {.row = std::get<ec::invoke_operation>(*resolved).args.row});
}
/// The canvas as a client reads it from the boundary just captured.
auto wire_grid() -> nlohmann::json {
    auto found = nlohmann::json{};
    const auto walk_node = [&](this const auto& self, const nlohmann::json& node) -> void {
        if (node.is_object() && node.value("id", "") == "vehicle:grid") {
            found = node;
        } else if (node.is_structured()) {
            for (const auto& value : node) { self(value); }
        }
    };
    walk_node(nlohmann::json::parse(ec::serialize_interaction_boundary(captured())));
    return found;
}
/// Whether the canvas `grid` lists square `x`,`y` in a run of cells, rather than leaving it to
/// its blank cell.
auto wire_listed(const nlohmann::json& grid, const int x, const int y) -> bool {
    return std::ranges::any_of(grid.at("cells"), [&](const nlohmann::json& entry) {
        const auto from = entry.at("x").get<int>();
        return entry.at("y") == y && from <= x && x < from + entry.value("n", 1);
    });
}
/// What a client reads at square `x`,`y` of the canvas `grid`: the style of the run there, else
/// the blank one.
auto wire_square(const nlohmann::json& grid, const int x, const int y) -> nlohmann::json {
    const auto& cells = grid.at("cells");
    const auto run = std::ranges::find_if(cells, [&](const nlohmann::json& entry) {
        const auto from = entry.at("x").get<int>();
        return entry.at("y") == y && from <= x && x < from + entry.value("n", 1);
    });
    return grid.at("styles").at(
        run == cells.end() ? grid.at("blank").get<int>() : run->at("style").get<int>());
}
auto stat(const ec::screen_view& screen, const std::string& key) -> const ec::panel_stat& {
    const auto& stats = body<ec::panel_body>(part(screen, "vehicle:document")).stats;
    const auto found = std::ranges::find(stats, key, &ec::panel_stat::key);
    INFO(key);
    REQUIRE(found != stats.end());
    return *found;
}
/// What the game said building the open prototype, as the problems list has it.
auto messages(const ec::screen_view& screen) -> std::vector<std::string> {
    const auto* const found = ec::find_component(*screen.root, "vehicle:problems");
    if (found == nullptr) { return {}; }
    return body<ec::list_body>(*found).rows | std::views::transform(&ec::row::label)
         | std::ranges::to<std::vector>();
}
auto refused(
    const ec::boundary_state& state, const std::string& affordance, const ec::invoke_args& args)
    -> bool {
    return ec::resolve_invoke(state, {.affordance_id = affordance, .args = args}).error()
        == ec::error::invalid_arguments;
}

} // namespace

TEST_CASE(
    "the vehicle editor opens a prototype, installs a part, undoes, and saves it as a mod file",
    "[editor_vehicle][engine_client_component]") {
    const auto guard = screen_guard{};
    const auto source = editor::prototype_file("bicycle");
    REQUIRE(source.has_value());
    const auto before = read_file(*source);
    const auto saved_as =
        std::filesystem::path(PATH_INFO::user_moddir()) / "vehicle_editor_test" / "bicycle.json";
    std::filesystem::remove_all(saved_as.parent_path());
    std::filesystem::create_directories(saved_as.parent_path());
    const auto cleanup = on_out_of_scope([&] {
        std::filesystem::remove_all(saved_as.parent_path());
    });
    auto step = 0;

    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto state = captured();
        const auto screen = ec::top_screen(state);
        REQUIRE(screen.kind == "vehicle_editor");
        switch (step++) {
            case 0:
                CHECK(screen.title == "Vehicle Editor");
                // Nothing is open: the prototypes are listed, nothing to edit yet.
                CHECK(body<ec::list_body>(part(screen, "vehicle:prototypes")).total > 300);
                CHECK_FALSE(has_affordance(screen, "vehicle:choose"));
                CHECK(affordance(screen, "QUIT").role == "cancel");
                return invoke("vehicle:filter", {.text = "bicycle"});
            case 1:
                CHECK(row(screen, "vehicle:prototypes", "prototype:bicycle").label == "Bicycle");
                CHECK(body<ec::list_body>(part(screen, "vehicle:prototypes")).filter == "bicycle");
                CHECK(std::ranges::contains(
                    part(screen, "vehicle:prototypes").affordances, std::string("vehicle:filter")));
                CHECK(std::ranges::contains(
                    part(screen, "vehicle:prototypes").affordances, std::string("vehicle:page")));
                return invoke("vehicle:open", {.row = "prototype:bicycle"});
            case 2: {
                CHECK(body<ec::panel_body>(part(screen, "vehicle:document")).title == "Bicycle");
                // The file as a modder finds it in the game's directory.
                CHECK(stat(screen, "file").value.starts_with("data/json/vehicles/"));
                CHECK(stat(screen, "saved").value == "Yes");
                CHECK(
                    std::stoi(
                        std::ranges::find(
                            body<ec::panel_body>(part(screen, "vehicle:performance")).stats, "mass",
                            &ec::panel_stat::key)
                            ->value)
                    > 0);
                const auto& grid = body<ec::grid_body>(part(screen, "vehicle:grid"));
                // Room to build around its three mounts in a row, the vehicle in the middle.
                CHECK(grid.cols == 41);
                CHECK(grid.rows == 31);
                CHECK(grid.role == "canvas");
                CHECK(cell(screen, "mount:-1,0").at == ec::grid_point{19, 15});
                REQUIRE(cell(screen, "mount:1,0").looks.size() == 3);
                CHECK(grid.looks.at(cell(screen, "mount:1,0").looks.back()).kind == "vehicle_part");
                CHECK(cell(screen, "mount:2,0").looks.empty());
                CHECK(cell(screen, "mount:0,0").selected);
                CHECK(messages(screen).empty());
                CHECK_FALSE(affordance(screen, "vehicle:undo").enabled);
                CHECK(affordance(screen, "vehicle:undo").denial == "Nothing to undo.");
                CHECK(affordance(screen, "vehicle:redo").denial == "Nothing to redo.");
                CHECK(affordance(screen, "vehicle:save").denial
                      == "Nothing changed since the file was saved.");
                CHECK(
                    affordance(screen, "vehicle:undo").keys == std::vector<std::string>{"CTRL+Z"});
                CHECK(
                    affordance(screen, "vehicle:redo").keys == std::vector<std::string>{"CTRL+Y"});
                CHECK(
                    affordance(screen, "vehicle:save").keys == std::vector<std::string>{"CTRL+S"});
                CHECK(refused(state, "vehicle:mount", {.row = "mount:21,0"}));
                // The parts are a palette, no tab of their own.
                CHECK(refused(state, "vehicle:tool", {.row = "tool:install"}));
                return invoke("vehicle:mount", {.row = "mount:1,0"});
            }
            case 3:
                CHECK(cell(screen, "mount:1,0").selected);
                CHECK(rows(screen, "vehicle:stack").size() == 3);
                return invoke("vehicle:part_filter", {.text = "frame_vertical"});
            case 4:
                CHECK(
                    body<ec::list_body>(part(screen, "vehicle:parts")).filter == "frame_vertical");
                CHECK(std::ranges::contains(
                    part(screen, "vehicle:parts").affordances, std::string("vehicle:part_filter")));
                CHECK(std::ranges::contains(
                    part(screen, "vehicle:parts").affordances, std::string("vehicle:part_page")));
                // A second frame is not mountable here, as the game says, and why.
                CHECK_FALSE(
                    row(screen, "vehicle:parts", "vpart:xlframe_vertical").description.empty());
                return invoke("vehicle:part_filter", {.text = "headlight"});
            case 5:
                CHECK(row(screen, "vehicle:parts", "vpart:headlight").description.empty());
                return invoke("vehicle:choose", {.row = "vpart:headlight"});
            case 6:
                return invoke("vehicle:mount", {.row = "mount:1,0"});
            case 7:
                CHECK(rows(screen, "vehicle:stack").size() == 4);
                CHECK(rows(screen, "vehicle:stack").back().label == "headlight");
                CHECK(stat(screen, "saved").value == "No");
                CHECK(affordance(screen, "vehicle:undo").enabled);
                return invoke("vehicle:undo");
            case 8:
                CHECK(rows(screen, "vehicle:stack").size() == 3);
                CHECK(stat(screen, "saved").value == "Yes");
                return invoke("vehicle:redo");
            case 9:
                CHECK(rows(screen, "vehicle:stack").size() == 4);
                // The raw form of the JSON is a tab of its own, out of the way, and so is what
                // sets it.
                CHECK_FALSE(has_affordance(screen, "vehicle:field_set"));
                return invoke("vehicle:tool", {.row = "tool:json"});
            case 10:
                return invoke("vehicle:field_set", {.row = "pointer", .text = "/name"});
            case 11:
                CHECK(body<ec::form_body>(part(screen, "vehicle:field")).fields[1].value
                      == "Bicycle");
                return invoke("vehicle:field_set", {.row = "value", .text = "Lit bicycle"});
            case 12:
                return invoke("vehicle:field_apply");
            case 13:
                CHECK(
                    body<ec::panel_body>(part(screen, "vehicle:document")).title == "Lit bicycle");
                return invoke("vehicle:tool", {.row = "tool:mount"});
            case 14:
                return invoke("vehicle:pick", {.row = rows(screen, "vehicle:stack").front().id});
            case 15:
                return invoke("vehicle:remove");
            case 16:
                // The frame holds the wheel: the game's reason, and nothing removed.
                CHECK_FALSE(body<ec::text_body>(part(screen, "vehicle:notice")).markup.empty());
                CHECK(rows(screen, "vehicle:stack").size() == 4);
                return invoke("vehicle:save_as", {.text = "/tmp/bicycle.json"});
            case 17:
                CHECK(body<ec::text_body>(part(screen, "vehicle:notice")).markup.find("your mods")
                      != std::string::npos);
                return invoke("vehicle:save_as", {.text = saved_as.string()});
            case 18:
                CHECK(stat(screen, "saved").value == "Yes");
                return act("QUIT");
            default:
                FAIL("the vehicle editor outlived its exit");
                return act("QUIT");
        }
    });

    editor::run_vehicle_editor();
    CHECK(step == 19);
    // The game's own file is as it was; the copy holds the change in the data style.
    CHECK(read_file(*source) == before);
    const auto text = read_file(saved_as);
    CHECK(format_json(text) == text);
    const auto saved = nlohmann::ordered_json::parse(text);
    const auto bicycle = std::ranges::find(saved, "bicycle", [](const auto& entry) {
        return entry.value("id", "");
    });
    REQUIRE(bicycle != saved.end());
    CHECK((*bicycle)["name"] == "Lit bicycle");
    CHECK((*bicycle)["parts"][1]["parts"].back() == "headlight");
}

TEST_CASE(
    "the map editor opens the vehicle editor, and the main menu's vehicle editor opens it first",
    "[editor_vehicle][engine_client_component]") {
    const auto guard = screen_guard{};
    editor::reset_editor_session();
    auto kinds = std::vector<std::string>{};
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto screen = ec::top_screen(captured());
        kinds.push_back(screen.kind);
        switch (kinds.size()) {
            case 1:
                return invoke("editor:vehicle");
            case 2:
            case 3:
                return act("QUIT");
            default:
                FAIL("the editor outlived its exit");
                return act("QUIT");
        }
    });
    editor::run_editor_session();
    CHECK(kinds == std::vector<std::string>{"editor", "vehicle_editor", "editor"});

    kinds.clear();
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        kinds.push_back(ec::top_screen(captured()).kind);
        return act("QUIT");
    });
    editor::open_vehicle_editor_first();
    editor::run_editor_session();
    CHECK(kinds == std::vector<std::string>{"vehicle_editor"});
    editor::reset_editor_session();
}

TEST_CASE(
    "the vehicle editor spawns the design it shows beside the avatar",
    "[editor_vehicle][engine_client_component]") {
    const auto guard = screen_guard{};
    clear_map();
    const auto original = vproto_id("bicycle")->blueprint->part_count();
    const auto restore = on_out_of_scope([] {
        const auto path = editor::prototype_file("bicycle");
        const auto json = nlohmann::ordered_json::parse(read_file(*path));
        const auto found = std::ranges::find(json, "bicycle", [](const auto& entry) {
            return entry.value("id", "");
        });
        REQUIRE(editor::apply_design(found->dump()).has_value());
        clear_vehicles();
    });
    const auto at = get_avatar().bub_pos() + tripoint_rel_ms(8, 0, 0);
    auto step = 0;
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto state = captured();
        const auto screen = ec::top_screen(state);
        switch (step++) {
            case 0:
                return invoke("vehicle:filter", {.text = "bicycle"});
            case 1:
                return invoke("vehicle:open", {.row = "prototype:bicycle"});
            case 2:
                return invoke("vehicle:tool", {.row = "tool:spawn"});
            case 3: {
                const auto& fields = body<ec::form_body>(part(screen, "vehicle:spawn_form")).fields;
                REQUIRE(fields.size() == 3);
                CHECK(fields[1].value == "100");
                CHECK(refused(state, "vehicle:spawn_set", {.row = "speed", .text = "1"}));
                return invoke("vehicle:spawn_set", {.row = "fuel", .text = "101"});
            }
            case 4:
                CHECK(body<ec::text_body>(part(screen, "vehicle:notice")).markup
                      == "That value is out of range.");
                return invoke("vehicle:spawn_set", {.row = "fuel", .text = "0"});
            case 5:
                CHECK(
                    body<ec::form_body>(part(screen, "vehicle:spawn_form")).fields[1].value == "0");
                return invoke("vehicle:part_filter", {.text = "headlight"});
            case 6:
                return invoke("vehicle:choose", {.row = "vpart:headlight"});
            case 7:
                return invoke("vehicle:mount", {.row = "mount:1,0"});
            case 8:
                return invoke("vehicle:spawn");
            case 9:
                CHECK_FALSE(ec::find_component(*screen.root, "vehicle:notice"));
                // A second one goes beside the first, which is in its way.
                return invoke("vehicle:spawn");
            case 10:
                CHECK_FALSE(ec::find_component(*screen.root, "vehicle:notice"));
                CHECK(get_map().get_vehicles().size() == 2);
                return invoke("vehicle:undo");
            case 11:
                return act("QUIT");
            default:
                FAIL("the vehicle editor outlived its exit");
                return act("QUIT");
        }
    });
    editor::run_vehicle_editor();
    CHECK(step == 12);
    const auto spawned = get_map().veh_at(at);
    REQUIRE(spawned);
    // The design with the light, which the game now builds for the prototype.
    CHECK(spawned->vehicle().part_count() == original + 1);
    CHECK(vproto_id("bicycle")->blueprint->part_count() == original + 1);
}

TEST_CASE(
    "the vehicle editor shows the design turned to the facing it spawns with",
    "[editor_vehicle][engine_client_component]") {
    const auto guard = screen_guard{};
    const auto turned = [](const ec::screen_view& screen) {
        return body<ec::grid_body>(part(screen, "vehicle:turned"));
    };
    auto step = 0;
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto screen = ec::top_screen(captured());
        switch (step++) {
            case 0:
                return invoke("vehicle:filter", {.text = "bicycle"});
            case 1:
                return invoke("vehicle:open", {.row = "prototype:bicycle"});
            case 2:
                // Only the spawn tab shows it.
                CHECK_FALSE(ec::find_component(*screen.root, "vehicle:turned"));
                return invoke("vehicle:tool", {.row = "tool:spawn"});
            case 3: {
                // Facing east, the three mounts lie in a row as on the mount grid.
                const auto grid = turned(screen);
                CHECK(grid.cols == 3);
                CHECK(grid.rows == 1);
                CHECK(grid.cells.size() == 3);
                CHECK(std::ranges::all_of(grid.cells, [](const ec::grid_cell& cell) {
                    return !cell.looks.empty() && cell.affordance.empty();
                }));
                return invoke("vehicle:spawn_set", {.row = "facing", .text = "90"});
            }
            case 4: {
                // Facing south, they stand in a column.
                const auto grid = turned(screen);
                CHECK(grid.cols == 1);
                CHECK(grid.rows == 3);
                CHECK(grid.cells.size() == 3);
                return invoke("vehicle:spawn_set", {.row = "facing", .text = "45"});
            }
            case 5: {
                const auto grid = turned(screen);
                CHECK(grid.cols == 3);
                CHECK(grid.rows == 3);
                CHECK(grid.cells.size() == 3);
                return act("QUIT");
            }
            default:
                FAIL("the vehicle editor outlived its exit");
                return act("QUIT");
        }
    });
    editor::run_vehicle_editor();
    CHECK(step == 6);
}

TEST_CASE(
    "the vehicle editor's turned design covers the squares the spawned vehicle covers",
    "[editor_vehicle][engine_client_component]") {
    const auto guard = screen_guard{};
    clear_map();
    const auto cleanup = on_out_of_scope([] { clear_vehicles(); });
    const auto at = get_avatar().bub_pos() + tripoint_rel_ms(8, 0, 0);
    auto shown = std::set<std::pair<int, int>>{};
    auto step = 0;
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto screen = ec::top_screen(captured());
        switch (step++) {
            case 0:
                return invoke("vehicle:filter", {.text = "car"});
            case 1:
                return invoke("vehicle:open", {.row = "prototype:car"});
            case 2:
                return invoke("vehicle:tool", {.row = "tool:spawn"});
            case 3:
                return invoke("vehicle:spawn_set", {.row = "facing", .text = "30"});
            case 4:
                for (const auto& cell : body<ec::grid_body>(part(screen, "vehicle:turned")).cells) {
                    shown.emplace(cell.at.x, cell.at.y);
                }
                return invoke("vehicle:spawn");
            case 5:
                CHECK_FALSE(ec::find_component(*screen.root, "vehicle:notice"));
                return act("QUIT");
            default:
                FAIL("the vehicle editor outlived its exit");
                return act("QUIT");
        }
    });
    editor::run_vehicle_editor();
    REQUIRE(step == 6);
    const auto spawned = get_map().veh_at(at);
    REQUIRE(spawned);
    const auto& veh = spawned->vehicle();
    auto covered = std::vector<point>{};
    for (auto index = 0; index < veh.part_count(); ++index) {
        covered.push_back(veh.bub_part_location(index).xy().raw());
    }
    const auto min_x = std::ranges::min(covered | std::views::transform(&point::x));
    const auto min_y = std::ranges::min(covered | std::views::transform(&point::y));
    const auto squares =
        covered
        | std::views::transform([&](const point& p) { return std::pair{p.x - min_x, p.y - min_y}; })
        | std::ranges::to<std::set>();
    CHECK(shown == squares);
}

TEST_CASE(
    "the vehicle editor exports a world vehicle's design to a new mod file and opens it",
    "[editor_vehicle][engine_client_component]") {
    const auto guard = screen_guard{};
    clear_map();
    const auto cleanup_vehicles = on_out_of_scope([] { clear_vehicles(); });
    REQUIRE(editor::spawn_design(
        vproto_id("car"), get_avatar().bub_pos() + tripoint_rel_ms(8, 0, 0), {}));
    const auto file =
        std::filesystem::path(PATH_INFO::user_moddir()) / "vehicle_editor_export" / "my_car.json";
    std::filesystem::remove_all(file.parent_path());
    const auto cleanup = on_out_of_scope([&] { std::filesystem::remove_all(file.parent_path()); });
    auto step = 0;
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto screen = ec::top_screen(captured());
        switch (step++) {
            case 0:
                CHECK(ec::find_component(*screen.root, "vehicle:world") == nullptr);
                CHECK_FALSE(has_affordance(screen, "vehicle:export"));
                return invoke("vehicle:source", {.row = "source:world"});
            case 1: {
                const auto& world = rows(screen, "vehicle:world");
                REQUIRE(world.size() == 1);
                CHECK(world[0].label == "Car");
                CHECK(world[0].columns[0].value == "8 squares");
                return invoke("vehicle:export", {.row = "world:0", .text = "/tmp/my_car.json"});
            }
            case 2:
                CHECK(body<ec::text_body>(part(screen, "vehicle:notice")).markup.find("your mods")
                      != std::string::npos);
                return invoke("vehicle:export", {.row = "world:0", .text = file.string()});
            case 3:
                CHECK(body<ec::panel_body>(part(screen, "vehicle:document")).title == "Car");
                CHECK(stat(screen, "saved").value == "Yes");
                CHECK(messages(screen).empty());
                return invoke("vehicle:export", {.row = "world:0", .text = file.string()});
            case 4:
                CHECK(body<ec::text_body>(part(screen, "vehicle:notice")).markup
                      == "That file already exists; nothing was saved.");
                return act("QUIT");
            default:
                FAIL("the vehicle editor outlived its exit");
                return act("QUIT");
        }
    });
    editor::run_vehicle_editor();
    CHECK(step == 5);
    const auto text = read_file(file);
    CHECK(format_json(text) == text);
    CHECK(nlohmann::ordered_json::parse(text)[0]["id"] == "my_car");
}

TEST_CASE(
    "the vehicle editor copies a world vehicle with its state and pastes it south of the avatar",
    "[editor_vehicle][engine_client_component]") {
    const auto guard = screen_guard{};
    clear_map();
    const auto cleanup_vehicles = on_out_of_scope([] { clear_vehicles(); });
    auto* const source = editor::
        spawn_design(vproto_id("car"), get_avatar().bub_pos() + tripoint_rel_ms(8, 12, 0), {});
    REQUIRE(source != nullptr);
    source->name = "Copied car";
    const auto copied = std::filesystem::path(PATH_INFO::user_dir()) / "vehicle_copy.json";
    std::filesystem::remove(copied);
    const auto cleanup = on_out_of_scope([&] { std::filesystem::remove(copied); });
    auto step = 0;
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto screen = ec::top_screen(captured());
        switch (step++) {
            case 0:
                return invoke("vehicle:source", {.row = "source:world"});
            case 1:
                return invoke("vehicle:paste");
            case 2:
                CHECK(body<ec::text_body>(part(screen, "vehicle:notice")).markup
                      == "No vehicle is copied.");
                return invoke("vehicle:copy", {.row = "world:0"});
            case 3:
                CHECK(rows(screen, "vehicle:world").size() == 1);
                return invoke("vehicle:paste");
            case 4: {
                const auto& world = rows(screen, "vehicle:world");
                REQUIRE(world.size() == 2);
                CHECK(world[1].label == "Copied car");
                return invoke("vehicle:paste");
            }
            case 5:
                // The second copy goes beside the first, which is in its way.
                CHECK_FALSE(ec::find_component(*screen.root, "vehicle:notice"));
                CHECK(rows(screen, "vehicle:world").size() == 3);
                return act("QUIT");
            default:
                FAIL("the vehicle editor outlived its exit");
                return act("QUIT");
        }
    });
    editor::run_vehicle_editor();
    CHECK(step == 6);
    const auto pasted = get_map().veh_at(get_avatar().bub_pos() + tripoint_rel_ms(0, 8, 0));
    REQUIRE(pasted);
    CHECK(pasted->vehicle().name == "Copied car");
    CHECK(pasted->vehicle().part_count() == source->part_count());
}

TEST_CASE(
    "the vehicle editor edits a world vehicle in place, each change one undo step",
    "[editor_vehicle][engine_client_component]") {
    const auto guard = screen_guard{};
    clear_map();
    const auto cleanup_vehicles = on_out_of_scope([] { clear_vehicles(); });
    const auto at = get_avatar().bub_pos() + tripoint_rel_ms(8, 0, 0);
    REQUIRE(editor::spawn_design(vproto_id("bicycle"), at, {}));
    const auto title = [](const ec::screen_view& screen) {
        return body<ec::panel_body>(part(screen, "vehicle:live")).title;
    };
    const auto labels = [](const ec::screen_view& screen) {
        return rows(screen, "vehicle:stack") | std::views::transform(&ec::row::label)
             | std::ranges::to<std::vector>();
    };
    const auto light = [](const ec::screen_view& screen) {
        const auto& stack = rows(screen, "vehicle:stack");
        const auto found = std::ranges::find(stack, "headlight", &ec::row::label);
        REQUIRE(found != stack.end());
        return *found;
    };
    auto plain = std::vector<std::string>{};
    auto step = 0;
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto screen = ec::top_screen(captured());
        switch (step++) {
            case 0:
                CHECK_FALSE(has_affordance(screen, "vehicle:done"));
                return invoke("vehicle:source", {.row = "source:world"});
            case 1:
                return invoke("vehicle:edit", {.row = "world:0"});
            case 2:
                CHECK(title(screen) == "Bicycle");
                CHECK_FALSE(affordance(screen, "vehicle:undo").enabled);
                CHECK(affordance(screen, "vehicle:undo").denial == "Nothing to undo.");
                CHECK(affordance(screen, "vehicle:redo").denial == "Nothing to redo.");
                CHECK(
                    affordance(screen, "vehicle:undo").keys == std::vector<std::string>{"CTRL+Z"});
                CHECK(
                    affordance(screen, "vehicle:redo").keys == std::vector<std::string>{"CTRL+Y"});
                return invoke("vehicle:mount", {.row = "mount:1,0"});
            case 3:
                plain = labels(screen);
                return invoke("vehicle:choose", {.row = "vpart:headlight"});
            case 4:
                return invoke("vehicle:mount", {.row = "mount:1,0"});
            case 5:
                CHECK(labels(screen).size() == plain.size() + 1);
                CHECK(affordance(screen, "vehicle:undo").enabled);
                // The part just installed is picked.
                CHECK(light(screen).selected);
                CHECK(light(screen).look.has_value());
                return invoke("vehicle:tool", {.row = "tool:vehicle"});
            case 6:
                return invoke("vehicle:live_set", {.row = "name", .text = "Mine"});
            case 7:
                CHECK(title(screen) == "Mine");
                return invoke("vehicle:tool", {.row = "tool:part"});
            case 8:
                return invoke("vehicle:live_set", {.row = "health", .text = "50"});
            case 9:
                return invoke("vehicle:choose", {.row = "vpart:xlframe_vertical"});
            case 10:
                return invoke("vehicle:mount", {.row = "mount:1,0"});
            case 11:
                CHECK(body<ec::text_body>(part(screen, "vehicle:notice")).markup.find("frame")
                      != std::string::npos);
                // A refused install leaves the mount's parts in view.
                CHECK(light(screen).columns[0].value == "50");
                return invoke("vehicle:undo");
            case 12:
                CHECK(light(screen).columns[0].value == "100");
                return invoke("vehicle:undo");
            case 13:
                CHECK(title(screen) == "Bicycle");
                return invoke("vehicle:undo");
            case 14:
                CHECK(labels(screen) == plain);
                CHECK(rows(screen, "vehicle:world").size() == 1);
                CHECK(affordance(screen, "vehicle:redo").enabled);
                return invoke("vehicle:redo");
            case 15:
                CHECK(labels(screen).size() == plain.size() + 1);
                CHECK(
                    affordance(screen, "vehicle:done").keys == std::vector<std::string>{"CTRL+D"});
                return invoke("vehicle:done");
            case 16:
                CHECK(ec::find_component(*screen.root, "vehicle:live") == nullptr);
                return act("QUIT");
            default:
                FAIL("the vehicle editor outlived its exit");
                return act("QUIT");
        }
    });
    editor::run_vehicle_editor();
    CHECK(step == 17);
    const auto edited = get_map().veh_at(at);
    REQUIRE(edited);
    CHECK(edited->vehicle().name == "Bicycle");
}

TEST_CASE(
    "the vehicle editor moves a part within its mount's install order",
    "[editor_vehicle][engine_client_component]") {
    const auto guard = screen_guard{};
    const auto labels = [](const ec::screen_view& screen) {
        return rows(screen, "vehicle:stack") | std::views::transform(&ec::row::label)
             | std::ranges::to<std::vector>();
    };
    auto before = std::vector<std::string>{};
    auto step = 0;
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto screen = ec::top_screen(captured());
        switch (step++) {
            case 0:
                return invoke("vehicle:filter", {.text = "bicycle"});
            case 1:
                return invoke("vehicle:open", {.row = "prototype:bicycle"});
            case 2:
                before = labels(screen);
                REQUIRE(before.size() == 4);
                return invoke("vehicle:pick", {.row = "part:2"});
            case 3:
                return invoke("vehicle:earlier");
            case 4:
                CHECK(labels(screen)
                      == std::vector<std::string>{before[0], before[2], before[1], before[3]});
                return invoke("vehicle:pick", {.row = "part:3"});
            case 5:
                CHECK_FALSE(affordance(screen, "vehicle:later").enabled);
                return invoke("vehicle:later");
            case 6:
                CHECK(body<ec::text_body>(part(screen, "vehicle:notice")).markup
                      == "The part is already at that end of its mount.");
                return invoke("vehicle:undo");
            case 7:
                CHECK(labels(screen) == before);
                return act("QUIT");
            default:
                FAIL("the vehicle editor outlived its exit");
                return act("QUIT");
        }
    });
    editor::run_vehicle_editor();
    CHECK(step == 8);
}

TEST_CASE(
    "the vehicle editor sets a prototype's name, id and a tank's fuel through their own fields",
    "[editor_vehicle][engine_client_component]") {
    const auto guard = screen_guard{};
    const auto field =
        [](const ec::screen_view& screen, const char* const form, const std::string& id) {
            const auto& fields = body<ec::form_body>(part(screen, form)).fields;
            const auto found = std::ranges::find(fields, id, &ec::form_field::id);
            INFO(id);
            REQUIRE(found != fields.end());
            return found->value;
        };
    const auto tank = [](const ec::screen_view& screen) {
        const auto& stack = rows(screen, "vehicle:stack");
        const auto found = std::ranges::find(stack, vpart_id("tank")->name(), &ec::row::label);
        REQUIRE(found != stack.end());
        return found->id;
    };
    auto step = 0;
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto state = captured();
        const auto screen = ec::top_screen(state);
        switch (step++) {
            case 0:
                return invoke("vehicle:filter", {.text = "car"});
            case 1:
                return invoke("vehicle:open", {.row = "prototype:car"});
            case 2:
                return invoke("vehicle:mount", {.row = "mount:-2,-1"});
            case 3:
                // Every part on a mount names the mount's cell.
                CHECK(cell(screen, "mount:-2,-1").label.find(", ") != std::string::npos);
                // Their names, without the health bar the map draws before them.
                CHECK(cell(screen, "mount:-2,-1").label.find('|') == std::string::npos);
                return invoke("vehicle:pick", {.row = tank(screen)});
            case 4:
                return invoke("vehicle:tool", {.row = "tool:part"});
            case 5:
                CHECK(field(screen, "vehicle:part_form", "fuel") == "gasoline");
                CHECK(refused(state, "vehicle:part_set", {.row = "ammo", .text = "5"}));
                return invoke("vehicle:part_set", {.row = "fuel", .text = "diesel"});
            case 6:
                CHECK(field(screen, "vehicle:part_form", "fuel") == "diesel");
                CHECK(stat(screen, "saved").value == "No");
                return invoke("vehicle:part_set", {.row = "fuel", .text = ""});
            case 7:
                CHECK(field(screen, "vehicle:part_form", "fuel").empty());
                return invoke("vehicle:tool", {.row = "tool:json"});
            case 8:
                // A part left with its id alone is named by it again.
                CHECK(field(screen, "vehicle:field", "value") == "tank");
                return invoke("vehicle:tool", {.row = "tool:vehicle"});
            case 9:
                CHECK(field(screen, "vehicle:props", "name") == "Car");
                CHECK(field(screen, "vehicle:props", "id") == "car");
                return invoke("vehicle:prop_set", {.row = "name", .text = "My car"});
            case 10:
                CHECK(body<ec::panel_body>(part(screen, "vehicle:document")).title == "My car");
                return invoke("vehicle:prop_set", {.row = "id", .text = ""});
            case 11:
                CHECK(body<ec::text_body>(part(screen, "vehicle:notice")).markup
                      == "A prototype keeps its name and id.");
                CHECK(field(screen, "vehicle:props", "id") == "car");
                return invoke("vehicle:prop_set", {.row = "id", .text = "my_car"});
            case 12:
                CHECK(field(screen, "vehicle:props", "id") == "my_car");
                return invoke("vehicle:undo");
            case 13:
                CHECK(field(screen, "vehicle:props", "id") == "car");
                return invoke("vehicle:undo");
            case 14:
                CHECK(body<ec::panel_body>(part(screen, "vehicle:document")).title == "Car");
                return invoke("vehicle:undo");
            case 15:
                return invoke("vehicle:undo");
            case 16:
                CHECK(stat(screen, "saved").value == "Yes");
                return act("QUIT");
            default:
                FAIL("the vehicle editor outlived its exit");
                return act("QUIT");
        }
    });
    editor::run_vehicle_editor();
    CHECK(step == 17);
}

TEST_CASE(
    "the vehicle editor sets a turret's ammunition and a prototype's flags and colours, refusing "
    "what the game would not load",
    "[editor_vehicle][engine_client_component]") {
    const auto guard = screen_guard{};
    const auto field =
        [](const ec::screen_view& screen, const char* const form, const std::string& id) {
            const auto& fields = body<ec::form_body>(part(screen, form)).fields;
            const auto found = std::ranges::find(fields, id, &ec::form_field::id);
            INFO(id);
            REQUIRE(found != fields.end());
            return found->value;
        };
    const auto notice = [](const ec::screen_view& screen) {
        return body<ec::text_body>(part(screen, "vehicle:notice")).markup;
    };
    const auto turret = [](const ec::screen_view& screen) {
        const auto& stack = rows(screen, "vehicle:stack");
        // The fourth part the prototype lists, `/parts/0/parts/3`.
        const auto found = std::ranges::find(stack, std::string("part:3"), &ec::row::id);
        REQUIRE(found != stack.end());
        return found->id;
    };
    auto step = 0;
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto screen = ec::top_screen(captured());
        switch (step++) {
            case 0:
                return invoke("vehicle:filter", {.text = "armored"});
            case 1:
                return invoke("vehicle:open", {.row = "prototype:armored_car"});
            case 2:
                return invoke("vehicle:mount", {.row = "mount:-3,1"});
            case 3:
                return invoke("vehicle:pick", {.row = turret(screen)});
            case 4:
                return invoke("vehicle:tool", {.row = "tool:part"});
            case 5:
                CHECK(field(screen, "vehicle:part_form", "ammo_types") == "556");
                CHECK(field(screen, "vehicle:part_form", "ammo_min") == "3");
                CHECK(field(screen, "vehicle:part_form", "ammo_max") == "30");
                return invoke("vehicle:part_set", {.row = "ammo_types", .text = "556, 9mm"});
            case 6:
                // The machine gun does not fire pistol rounds.
                CHECK_FALSE(notice(screen).empty());
                CHECK(field(screen, "vehicle:part_form", "ammo_types") == "556");
                return invoke(
                    "vehicle:part_set", {.row = "ammo_types", .text = "556, 556_incendiary"});
            case 7:
                CHECK(field(screen, "vehicle:part_form", "ammo_types") == "556, 556_incendiary");
                return invoke("vehicle:part_set", {.row = "ammo_min", .text = "40"});
            case 8:
                CHECK(notice(screen) == "A turret cannot load fewer rounds at most than at least.");
                CHECK(field(screen, "vehicle:part_form", "ammo_min") == "3");
                return invoke("vehicle:part_set", {.row = "ammo_max", .text = "10"});
            case 9:
                CHECK(field(screen, "vehicle:part_form", "ammo_max") == "10");
                return invoke("vehicle:tool", {.row = "tool:vehicle"});
            case 10:
                CHECK(field(screen, "vehicle:props", "color_palette") == "military_standard");
                CHECK(field(screen, "vehicle:props", "flags").empty());
                return invoke("vehicle:prop_set", {.row = "flags", .text = "VEHICLE_LOCKED"});
            case 11:
                CHECK(field(screen, "vehicle:props", "flags") == "VEHICLE_LOCKED");
                return invoke("vehicle:prop_set",
                              {.row = "flags", .text = "VEHICLE_LOCKED, VEHICLE_NO_HOTWIRE"});
            case 12:
                CHECK(field(screen, "vehicle:props", "flags")
                      == "VEHICLE_LOCKED, VEHICLE_NO_HOTWIRE");
                return invoke(
                    "vehicle:prop_set", {.row = "color_palette", .text = "no_such_palette"});
            case 13:
                CHECK_FALSE(notice(screen).empty());
                CHECK(field(screen, "vehicle:props", "color_palette") == "military_standard");
                return invoke("vehicle:prop_set", {.row = "color_palette", .text = ""});
            case 14:
                CHECK(field(screen, "vehicle:props", "color_palette").empty());
                return invoke("vehicle:prop_set", {.row = "flags", .text = ""});
            case 15:
                CHECK(field(screen, "vehicle:props", "flags").empty());
                return invoke("vehicle:undo");
            case 16:
            case 17:
            case 18:
            case 19:
            case 20:
                return invoke("vehicle:undo");
            case 21:
                CHECK(stat(screen, "saved").value == "Yes");
                return act("QUIT");
            default:
                FAIL("the vehicle editor outlived its exit");
                return act("QUIT");
        }
    });
    editor::run_vehicle_editor();
    CHECK(step == 22);
}

TEST_CASE(
    "the vehicle editor adds, weighs and removes a mount's item spawns",
    "[editor_vehicle][engine_client_component]") {
    const auto guard = screen_guard{};
    auto before = std::size_t{0};
    auto step = 0;
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto screen = ec::top_screen(captured());
        switch (step++) {
            case 0:
                return invoke("vehicle:filter", {.text = "car"});
            case 1:
                return invoke("vehicle:open", {.row = "prototype:car"});
            case 2:
                return invoke("vehicle:mount", {.row = "mount:-2,0"});
            case 3:
                return invoke("vehicle:tool", {.row = "tool:cargo"});
            case 4:
                before = rows(screen, "vehicle:items").size();
                return invoke("vehicle:item_add", {.text = "no_such_item"});
            case 5:
                CHECK(body<ec::text_body>(part(screen, "vehicle:notice")).markup
                      == "No item or item group has that id.");
                CHECK(rows(screen, "vehicle:items").size() == before);
                return invoke("vehicle:item_add", {.text = "rock"});
            case 6: {
                const auto& items = rows(screen, "vehicle:items");
                REQUIRE(items.size() == before + 1);
                CHECK(items.back().label == "rock");
                CHECK(items.back().columns[0].value == "100");
                return invoke("vehicle:item_chance", {.row = items.back().id, .count = 40});
            }
            case 7:
                CHECK(rows(screen, "vehicle:items").back().columns[0].value == "40");
                return invoke("vehicle:item_add", {.text = "car_misc"});
            case 8:
                CHECK(rows(screen, "vehicle:items").back().label == "car_misc");
                CHECK(messages(screen).empty());
                return invoke(
                    "vehicle:item_remove", {.row = rows(screen, "vehicle:items").back().id});
            case 9:
                CHECK(rows(screen, "vehicle:items").size() == before + 1);
                return invoke("vehicle:undo");
            case 10:
                CHECK(rows(screen, "vehicle:items").size() == before + 2);
                return invoke("vehicle:undo");
            case 11:
                return invoke("vehicle:undo");
            case 12:
                return invoke("vehicle:undo");
            case 13:
                CHECK(rows(screen, "vehicle:items").size() == before);
                CHECK(stat(screen, "saved").value == "Yes");
                return act("QUIT");
            default:
                FAIL("the vehicle editor outlived its exit");
                return act("QUIT");
        }
    });
    editor::run_vehicle_editor();
    CHECK(step == 14);
}

TEST_CASE(
    "the vehicle editor opens the prototype the main menu asked for and remembers it",
    "[editor_vehicle][engine_client_component]") {
    const auto guard = screen_guard{};
    const auto forget = [] { std::filesystem::remove(editor::recent_documents_file()); };
    forget();
    const auto cleanup = on_out_of_scope([&] {
        forget();
        editor::reset_editor_session();
        editor::reset_vehicle_editor();
    });
    editor::open_first({.editor = "vehicle", .name = "bicycle"});
    auto kinds = std::vector<std::string>{};
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto screen = ec::top_screen(captured());
        kinds.push_back(screen.kind);
        if (kinds.size() == 1) {
            CHECK(body<ec::panel_body>(part(screen, "vehicle:document")).title == "Bicycle");
        }
        return act("QUIT");
    });
    editor::run_editor_session();
    // It leaves with the vehicle editor, as the main menu's vehicle editor does.
    CHECK(kinds == std::vector<std::string>{"vehicle_editor"});
    const auto recent = editor::recent_documents();
    REQUIRE_FALSE(recent.empty());
    CHECK(recent.front().editor == "vehicle");
    CHECK(recent.front().name == "bicycle");
}

TEST_CASE(
    "the vehicle editor reloads a file changed on disk unless it is edited, and then says so",
    "[editor_vehicle][engine_client_component]") {
    const auto guard = screen_guard{};
    const auto saved_as =
        std::filesystem::path(PATH_INFO::user_moddir()) / "vehicle_editor_disk" / "bicycle.json";
    std::filesystem::remove_all(saved_as.parent_path());
    const auto cleanup = on_out_of_scope([&] {
        std::filesystem::remove_all(saved_as.parent_path());
    });
    // The file as another program writes it: the prototype named `name` instead.
    const auto write_outside = [&](const std::string& name) {
        auto value = nlohmann::ordered_json::parse(read_file(saved_as));
        for (auto& entry : value) {
            if (entry.value("id", "") == "bicycle") { entry["name"] = name; }
        }
        auto out = std::ofstream(saved_as, std::ios::binary | std::ios::trunc);
        out << value.dump(2);
    };
    const auto title = [](const ec::screen_view& screen) {
        return body<ec::panel_body>(part(screen, "vehicle:document")).title;
    };
    const auto notice = [](const ec::screen_view& screen) {
        return body<ec::text_body>(part(screen, "vehicle:notice")).markup;
    };
    auto step = 0;

    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto screen = ec::top_screen(captured());
        switch (step++) {
            case 0:
                return invoke("vehicle:filter", {.text = "bicycle"});
            case 1:
                return invoke("vehicle:open", {.row = "prototype:bicycle"});
            case 2:
                return invoke("vehicle:save_as", {.text = saved_as.string()});
            case 3:
                CHECK(stat(screen, "saved").value == "Yes");
                write_outside("Outside bicycle");
                return invoke("vehicle:tool", {.row = "tool:vehicle"});
            case 4:
                CHECK(title(screen) == "Outside bicycle");
                CHECK(notice(screen) == saved_as.string() + " changed on disk and was reloaded.");
                CHECK(stat(screen, "saved").value == "Yes");
                return invoke("vehicle:prop_set", {.row = "name", .text = "Edited bicycle"});
            case 5:
                CHECK(title(screen) == "Edited bicycle");
                CHECK_FALSE(ec::find_component(*screen.root, "vehicle:notice"));
                write_outside("Other bicycle");
                return invoke("vehicle:tool", {.row = "tool:mount"});
            case 6:
                // An edited document keeps its edit, and the change on disk is not overwritten
                // unasked.
                CHECK(title(screen) == "Edited bicycle");
                CHECK(
                    notice(screen)
                    == saved_as.string() + " changed on disk; saving would overwrite that change.");
                return invoke("vehicle:undo");
            case 7: {
                // The file no longer defines the prototype: the clean document is closed.
                auto out = std::ofstream(saved_as, std::ios::binary | std::ios::trunc);
                out << "[]";
                return invoke("vehicle:tool", {.row = "tool:mount"});
            }
            case 8:
                CHECK(notice(screen)
                      == saved_as.string() + " changed on disk and no longer defines bicycle.");
                CHECK_FALSE(ec::find_component(*screen.root, "vehicle:document"));
                return invoke("vehicle:filter", {.text = "bicycle"});
            case 9:
                // Filtering changes nothing, so the notice stays.
                CHECK(notice(screen)
                      == saved_as.string() + " changed on disk and no longer defines bicycle.");
                return act("QUIT");
            default:
                FAIL("the vehicle editor outlived its exit");
                return act("QUIT");
        }
    });
    editor::run_vehicle_editor();
    CHECK(step == 10);
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
    "the vehicle editor names what each part is for and the menu of each action",
    "[editor_vehicle]") {
    const auto guard = screen_guard{};
    auto step = 0;
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto screen = ec::top_screen(captured());
        switch (step++) {
            case 0:
                // Nothing is open: the prototypes to open are the page.
                CHECK(slot_of(screen, "vehicle:sources") == "canvas");
                CHECK(affordance(screen, "vehicle:paste").group == "World");
                return invoke("vehicle:open", {.row = "prototype:bicycle"});
            case 1:
                CHECK(slot_of(screen, "vehicle:sources") == "outline");
                CHECK(slot_of(screen, "vehicle:grid") == "canvas");
                CHECK(slot_of(screen, "vehicle:tools") == "inspector");
                CHECK(part(screen, "vehicle:sources").label == "Vehicles");
                CHECK(ec::top_screen(captured()).role == "workspace");
                CHECK(affordance(screen, "vehicle:save").group == "File");
                CHECK(affordance(screen, "vehicle:save_as").group == "File");
                CHECK(affordance(screen, "vehicle:undo").group == "Edit");
                CHECK(affordance(screen, "vehicle:spawn").group == "World");
                // What a part runs on its rows sits in no menu.
                CHECK(affordance(screen, "vehicle:mount").group.empty());
                return act("QUIT");
            default:
                FAIL("the vehicle editor outlived its exit");
                return act("QUIT");
        }
    });
    editor::run_vehicle_editor();
    CHECK(step == 2);
}

TEST_CASE("the vehicle editor's parts show what they are", "[editor_vehicle]") {
    const auto guard = screen_guard{};
    auto step = 0;
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto screen = ec::top_screen(captured());
        switch (step++) {
            case 0:
                return invoke("vehicle:open", {.row = "prototype:bicycle"});
            case 1:
                return invoke("vehicle:mount", {.row = "mount:0,0"});
            case 2: {
                const auto& rows = body<ec::list_body>(part(screen, "vehicle:parts")).rows;
                REQUIRE_FALSE(rows.empty());
                // Every part, under headings that are no part.
                CHECK(std::ranges::all_of(rows, [](const ec::row& row) {
                    return row.header
                        || (row.look && "vpart:" + row.look->id.value_or("") == row.id);
                }));
                // So do the parts already on the mount.
                const auto& stack = body<ec::list_body>(part(screen, "vehicle:stack")).rows;
                REQUIRE_FALSE(stack.empty());
                CHECK(std::ranges::all_of(stack, [](const ec::row& row) {
                    return row.look.has_value();
                }));
                return act("QUIT");
            }
            default:
                FAIL("the vehicle editor outlived its exit");
                return act("QUIT");
        }
    });
    editor::run_vehicle_editor();
    CHECK(step == 3);
}

TEST_CASE(
    "the vehicle editor keeps its parts in a palette beside the mount it installs on",
    "[editor_vehicle]") {
    const auto guard = screen_guard{};
    const auto tabs = [](const ec::screen_view& screen) {
        return body<ec::tabs_body>(part(screen, "vehicle:tools")).tabs
             | std::views::transform(&ec::tab_entry::id) | std::ranges::to<std::vector>();
    };
    auto step = 0;
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto screen = ec::top_screen(captured());
        switch (step++) {
            case 0:
                return invoke("vehicle:open", {.row = "prototype:bicycle"});
            case 1:
                CHECK(slot_of(screen, "vehicle:part_groups") == "palette");
                CHECK(part(screen, "vehicle:part_groups").label == "Parts");
                CHECK_FALSE(std::ranges::contains(tabs(screen), std::string("tool:install")));
                return invoke("vehicle:part_filter", {.text = "headlight"});
            case 2:
                return invoke("vehicle:choose", {.row = "vpart:headlight"});
            case 3:
                return invoke("vehicle:mount", {.row = "mount:1,0"});
            case 4:
                // The palette and the mount's stack stay, ready for the next part.
                CHECK(has_affordance(screen, "vehicle:choose"));
                CHECK(body<ec::tabs_body>(part(screen, "vehicle:tools")).tabs.front().selected);
                CHECK(std::ranges::contains(
                    body<ec::list_body>(part(screen, "vehicle:stack")).rows
                        | std::views::transform(&ec::row::label),
                    std::string("headlight")));
                return invoke("vehicle:undo");
            default:
                return act("QUIT");
        }
    });
    editor::run_vehicle_editor();
    CHECK(step >= 6);
}

namespace {
/// Whether a part of the screen offers `entry`: a part's own affordance, a row's or a cell's.
auto offered_by_part(const ec::component& value, const ec::affordance& entry) -> bool {
    const auto by_cell = [&] {
        const auto* const grid = std::get_if<ec::grid_body>(&value.body);
        return grid && std::ranges::contains(grid->cells, entry.id, &ec::grid_cell::affordance);
    };
    return std::ranges::contains(value.affordances, entry.id) || by_cell()
        || (entry.args.row && entry.args.row->component_id == value.id)
        || std::ranges::any_of(value.children, [&](const ec::component& child) {
               return offered_by_part(child, entry);
           });
}
auto labels(const ec::screen_view& screen, const std::string& id) -> std::vector<std::string> {
    return rows(screen, id) | std::views::transform(&ec::row::label)
         | std::ranges::to<std::vector>();
}
auto notice_of(const ec::screen_view& screen) -> std::string {
    const auto* const found = ec::find_component(*screen.root, "vehicle:notice");
    return found ? body<ec::text_body>(*found).markup : std::string{};
}
} // namespace

TEST_CASE(
    "the vehicle editor keeps the file on the status line, the vehicle's performance in the "
    "inspector and its canvas tools in the tools column",
    "[editor_vehicle]") {
    const auto guard = screen_guard{};
    auto step = 0;
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto screen = ec::top_screen(captured());
        switch (step++) {
            case 0:
                return invoke("vehicle:open", {.row = "prototype:bicycle"});
            case 1: {
                CHECK(slot_of(screen, "vehicle:document") == "status");
                CHECK(body<ec::panel_body>(part(screen, "vehicle:document")).title == "Bicycle");
                CHECK(stat(screen, "file").value.starts_with("data/json/vehicles/"));
                CHECK(stat(screen, "saved").value == "Yes");
                CHECK(stat(screen, "mount").value == "0,0");
                // The status line holds no performance figures; the inspector does.
                CHECK_FALSE(std::ranges::contains(
                    body<ec::panel_body>(part(screen, "vehicle:document")).stats, "mass",
                    &ec::panel_stat::key));
                CHECK(slot_of(screen, "vehicle:grid") == "canvas");
                CHECK(slot_of(screen, "vehicle:performance") == "inspector");
                // The figures come under the design they follow from.
                CHECK(std::ranges::find(screen.root->children, "vehicle:tools", &ec::component::id)
                      < std::ranges::
                          find(screen.root->children, "vehicle:performance", &ec::component::id));
                CHECK(std::ranges::contains(
                    body<ec::panel_body>(part(screen, "vehicle:performance")).stats, "mass",
                    &ec::panel_stat::key));
                CHECK(slot_of(screen, "vehicle:canvas_tools") == "tools");
                const auto& tools = rows(screen, "vehicle:canvas_tools");
                CHECK(
                    (tools | std::views::transform(&ec::row::id) | std::ranges::to<std::vector>())
                    == std::vector<std::string>{
                        "canvas_tool:select", "canvas_tool:install", "canvas_tool:line",
                        "canvas_tool:rect", "canvas_tool:rect_filled", "canvas_tool:fill",
                        "canvas_tool:remove", "canvas_tool:pick", "canvas_tool:paste",
                        "canvas_tool:mirror_sides", "canvas_tool:mirror_ends"});
                CHECK(tools.front().selected);
                CHECK(
                    (tools | std::views::transform(&ec::row::hotkey)
                     | std::ranges::to<std::vector>())
                    == std::vector<
                        std::string>{"s", "b", "l", "r", "u", "f", "e", "i", "v", "", ""});
                // The design tabs say what they hold.
                CHECK(
                    (body<ec::tabs_body>(part(screen, "vehicle:tools")).tabs
                     | std::views::transform(&ec::tab_entry::label)
                     | std::ranges::to<std::vector>())
                    == std::vector<std::string>{
                        "Parts here", "Part settings", "Name and flags", "Items here", "Test spawn",
                        "Raw JSON"});
                // Every action that names no row is in a menu, and the menus are in order.
                for (const auto& entry : screen.affordances) {
                    if (!entry.role.empty()) { continue; }
                    INFO(entry.id);
                    CHECK((!entry.group.empty() || offered_by_part(*screen.root, entry)));
                }
                auto menus = std::vector<std::string>{};
                for (const auto& entry : screen.affordances) {
                    if (!entry.group.empty() && !std::ranges::contains(menus, entry.group)) {
                        menus.push_back(entry.group);
                    }
                }
                CHECK(menus == std::vector<std::string>{"File", "Edit", "View", "World"});
                CHECK(affordance(screen, "vehicle:new").keys == std::vector<std::string>{"CTRL+N"});
                CHECK(
                    affordance(screen, "vehicle:save_as").keys == std::vector<std::string>{"F12"});
                CHECK(affordance(screen, "vehicle:show:mount").group == "View");
                CHECK(
                    affordance(screen, "vehicle:show:json").keys == std::vector<std::string>{"6"});
                CHECK(affordance(screen, "vehicle:tool_install").group == "Edit");
                CHECK(affordance(screen, "vehicle:tool_install").keys
                      == std::vector<std::string>{"b"});
                CHECK(affordance(screen, "vehicle:spawn").keys == std::vector<std::string>{"F5"});
                // The top part of the mount is picked, and Remove names it.
                const auto& stack = rows(screen, "vehicle:stack");
                REQUIRE_FALSE(stack.empty());
                CHECK(stack.back().selected);
                CHECK(affordance(screen, "vehicle:remove").label == "Remove " + stack.back().label);
                CHECK(affordance(screen, "vehicle:remove").keys
                      == std::vector<std::string>{"DELETE"});
                CHECK(affordance(screen, "vehicle:remove").group == "Edit");
                CHECK(std::ranges::contains(
                    part(screen, "vehicle:stack").affordances, std::string("vehicle:remove")));
                return invoke("vehicle:mount", {.row = "mount:1,0"});
            }
            case 2:
                // The select tool only moves the cursor.
                CHECK(stat(screen, "mount").value == "1,0");
                CHECK(stat(screen, "saved").value == "Yes");
                return act("QUIT");
            default:
                FAIL("the vehicle editor outlived its exit");
                return act("QUIT");
        }
    });
    editor::run_vehicle_editor();
    CHECK(step == 3);
}

TEST_CASE(
    "the vehicle editor installs the chosen part where the grid is clicked and says why a mount "
    "refuses it",
    "[editor_vehicle]") {
    const auto guard = screen_guard{};
    auto step = 0;
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto screen = ec::top_screen(captured());
        switch (step++) {
            case 0:
                return invoke("vehicle:open", {.row = "prototype:bicycle"});
            case 1:
                return invoke("vehicle:part_filter", {.text = "headlight"});
            case 2:
                // Choosing a part from the palette installs nothing yet.
                CHECK(body<ec::list_body>(part(screen, "vehicle:parts")).activate
                      == "vehicle:choose");
                return invoke("vehicle:choose", {.row = "vpart:headlight"});
            case 3:
                CHECK(stat(screen, "saved").value == "Yes");
                CHECK(row(screen, "vehicle:canvas_tools", "canvas_tool:install").selected);
                CHECK(row(screen, "vehicle:canvas_tools", "canvas_tool:install").label == "Pencil");
                CHECK(row(screen, "vehicle:parts", "vpart:headlight").selected);
                // East of the rear wheel nothing holds a headlight up.
                return invoke("vehicle:mount", {.row = "mount:2,0"});
            case 4:
                CHECK(notice_of(screen).find("frame") != std::string::npos);
                CHECK(stat(screen, "saved").value == "Yes");
                CHECK(rows(screen, "vehicle:stack").empty());
                return invoke("vehicle:mount", {.row = "mount:1,0"});
            case 5:
                CHECK(notice_of(screen).empty());
                CHECK(labels(screen, "vehicle:stack").back() == "headlight");
                CHECK(rows(screen, "vehicle:stack").back().selected);
                CHECK(affordance(screen, "vehicle:remove").label == "Remove headlight");
                // A second headlight on the same mount: the game's rule, in words.
                return invoke("vehicle:mount", {.row = "mount:1,0"});
            case 6:
                CHECK(notice_of(screen) == "This mount already has headlight.");
                CHECK(std::ranges::count(labels(screen, "vehicle:stack"), "headlight") == 1);
                return invoke("vehicle:tool_remove");
            case 7:
                CHECK(row(screen, "vehicle:canvas_tools", "canvas_tool:remove").selected);
                return invoke("vehicle:mount", {.row = "mount:1,0"});
            case 8:
                // The remove tool takes the mount's top part off.
                CHECK_FALSE(std::ranges::contains(labels(screen, "vehicle:stack"), "headlight"));
                CHECK(affordance(screen, "vehicle:undo").enabled);
                return invoke("vehicle:undo");
            case 9:
                CHECK(labels(screen, "vehicle:stack").back() == "headlight");
                return invoke("vehicle:undo");
            case 10:
                CHECK(stat(screen, "saved").value == "Yes");
                return act("QUIT");
            default:
                FAIL("the vehicle editor outlived its exit");
                return act("QUIT");
        }
    });
    editor::run_vehicle_editor();
    CHECK(step == 11);
}

TEST_CASE(
    "the vehicle editor removes and reorders the picked part, which its buttons name",
    "[editor_vehicle]") {
    const auto guard = screen_guard{};
    auto step = 0;
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto screen = ec::top_screen(captured());
        switch (step++) {
            case 0:
                return invoke("vehicle:open", {.row = "prototype:bicycle"});
            case 1:
                return invoke("vehicle:mount", {.row = "mount:1,0"});
            case 2:
                // The frame holds the rest: picking it and removing it is refused, saying why.
                return invoke("vehicle:pick", {.row = rows(screen, "vehicle:stack").front().id});
            case 3:
                CHECK(rows(screen, "vehicle:stack").front().selected);
                CHECK(affordance(screen, "vehicle:remove").label
                      == "Remove " + rows(screen, "vehicle:stack").front().label);
                CHECK(affordance(screen, "vehicle:earlier").label
                      == "Move " + rows(screen, "vehicle:stack").front().label + " up");
                CHECK_FALSE(affordance(screen, "vehicle:earlier").enabled);
                return invoke("vehicle:remove");
            case 4:
                CHECK_FALSE(notice_of(screen).empty());
                CHECK(stat(screen, "saved").value == "Yes");
                return invoke("vehicle:later");
            case 5: {
                // The moved part stays picked.
                const auto& stack = rows(screen, "vehicle:stack");
                CHECK(stack[1].selected);
                CHECK(affordance(screen, "vehicle:earlier").enabled);
                return invoke("vehicle:undo");
            }
            case 6:
                return act("QUIT");
            default:
                FAIL("the vehicle editor outlived its exit");
                return act("QUIT");
        }
    });
    editor::run_vehicle_editor();
    CHECK(step == 7);
}

TEST_CASE(
    "the vehicle editor starts a new vehicle in a file of the player's mods", "[editor_vehicle]") {
    const auto guard = screen_guard{};
    const auto made =
        std::filesystem::path(PATH_INFO::user_moddir()) / "vehicle_editor_new" / "go_cart.json";
    std::filesystem::remove_all(made.parent_path());
    const auto cleanup = on_out_of_scope([&] { std::filesystem::remove_all(made.parent_path()); });
    auto step = 0;
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto screen = ec::top_screen(captured());
        switch (step++) {
            case 0:
                CHECK(affordance(screen, "vehicle:new").group == "File");
                return invoke("vehicle:new", {.text = "/tmp/go_cart.json"});
            case 1:
                CHECK(notice_of(screen).find("your mods") != std::string::npos);
                return invoke("vehicle:new", {.text = made.string()});
            case 2:
                CHECK(body<ec::panel_body>(part(screen, "vehicle:document")).title == "go_cart");
                // Nothing is written until the player saves.
                CHECK(stat(screen, "saved").value == "No");
                CHECK_FALSE(std::filesystem::exists(made));
                return invoke("vehicle:part_filter", {.text = "headlight"});
            case 3:
                return invoke("vehicle:choose", {.row = "vpart:headlight"});
            case 4:
                return invoke("vehicle:mount", {.row = "mount:1,0"});
            case 5:
                // Beside the frame it starts with, a vehicle grows by another frame first.
                CHECK(notice_of(screen).find("frame") != std::string::npos);
                return invoke("vehicle:part_filter", {.text = "xlframe_vertical"});
            case 6:
                return invoke("vehicle:choose", {.row = "vpart:xlframe_vertical"});
            case 7:
                return invoke("vehicle:mount", {.row = "mount:1,0"});
            case 8:
                CHECK(notice_of(screen).empty());
                CHECK(rows(screen, "vehicle:stack").size() == 1);
                return invoke("vehicle:save");
            case 9:
                CHECK(stat(screen, "saved").value == "Yes");
                // A file that is there already is not overwritten.
                return invoke("vehicle:new", {.text = made.string()});
            case 10:
                CHECK(notice_of(screen) == "That file already exists; nothing was saved.");
                // A file name alone is a file of that name in the player's mods.
                return invoke("vehicle:new", {.text = "vehicle_editor_new/hand_cart.json"});
            case 11:
                CHECK(body<ec::panel_body>(part(screen, "vehicle:document")).title == "hand_cart");
                CHECK_FALSE(std::filesystem::exists(made.parent_path() / "hand_cart.json"));
                return invoke("vehicle:save_as", {.text = "vehicle_editor_new/barrow"});
            case 12:
                CHECK(std::filesystem::exists(made.parent_path() / "barrow.json"));
                return act("QUIT");
            default:
                FAIL("the vehicle editor outlived its exit");
                return act("QUIT");
        }
    });
    editor::run_vehicle_editor();
    CHECK(step == 13);
    const auto saved = nlohmann::ordered_json::parse(read_file(made));
    REQUIRE(saved.size() == 1);
    CHECK(saved[0]["id"] == "go_cart");
    CHECK(saved[0]["parts"].dump().find("xlframe_vertical") != std::string::npos);
}

TEST_CASE("the vehicle editor turns the spawn facing a quarter at a time", "[editor_vehicle]") {
    const auto guard = screen_guard{};
    const auto facing = [](const ec::screen_view& screen) {
        return body<ec::form_body>(part(screen, "vehicle:spawn_form")).fields.front().value;
    };
    auto step = 0;
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto screen = ec::top_screen(captured());
        switch (step++) {
            case 0:
                return invoke("vehicle:open", {.row = "prototype:bicycle"});
            case 1:
                CHECK(affordance(screen, "vehicle:turn_cw").keys == std::vector<std::string>{"t"});
                CHECK(affordance(screen, "vehicle:turn_ccw").keys == std::vector<std::string>{"T"});
                return invoke("vehicle:turn_cw");
            case 2:
                // The facing shows at once.
                CHECK(body<ec::tabs_body>(part(screen, "vehicle:tools")).tabs[4].selected);
                CHECK(facing(screen) == "90");
                return invoke("vehicle:turn_ccw");
            case 3:
                CHECK(facing(screen) == "0");
                return invoke("vehicle:turn_ccw");
            case 4:
                CHECK(facing(screen) == "270");
                return act("QUIT");
            default:
                FAIL("the vehicle editor outlived its exit");
                return act("QUIT");
        }
    });
    editor::run_vehicle_editor();
    CHECK(step == 5);
}

TEST_CASE(
    "the vehicle editor lists what the game says of a prototype it cannot build as it is",
    "[editor_vehicle]") {
    const auto guard = screen_guard{};
    auto step = 0;
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto screen = ec::top_screen(captured());
        switch (step++) {
            case 0:
                return invoke("vehicle:open", {.row = "prototype:bicycle"});
            case 1:
                CHECK(ec::find_component(*screen.root, "vehicle:problems") == nullptr);
                return invoke("vehicle:show:json");
            case 2:
                return invoke("vehicle:field_set", {.row = "pointer", .text = "/parts/0/parts/0"});
            case 3:
                return invoke("vehicle:field_set", {.row = "value", .text = "no_such_part"});
            case 4:
                return invoke("vehicle:field_apply");
            case 5:
                CHECK(slot_of(screen, "vehicle:problems") == "log");
                CHECK(part(screen, "vehicle:problems").label == "Problems");
                CHECK_FALSE(messages(screen).empty());
                return invoke("vehicle:undo");
            case 6:
                CHECK(messages(screen).empty());
                return act("QUIT");
            default:
                FAIL("the vehicle editor outlived its exit");
                return act("QUIT");
        }
    });
    editor::run_vehicle_editor();
    CHECK(step == 7);
}

TEST_CASE(
    "the vehicle editor lists its steps in a History the player goes back and forth in",
    "[editor_vehicle]") {
    const auto guard = screen_guard{};
    const auto labels = [](const ec::screen_view& screen) {
        return body<ec::list_body>(part(screen, "vehicle:history")).rows
             | std::views::transform(&ec::row::label) | std::ranges::to<std::vector>();
    };
    const auto at_step = [](const ec::screen_view& screen) {
        const auto& all = body<ec::list_body>(part(screen, "vehicle:history")).rows;
        return std::ranges::find(all, true, &ec::row::selected)->id;
    };
    const auto has_light = [](const ec::screen_view& screen) {
        return std::ranges::contains(
            body<ec::list_body>(part(screen, "vehicle:stack")).rows
                | std::views::transform(&ec::row::label),
            std::string("headlight"));
    };
    auto step = 0;
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto screen = ec::top_screen(captured());
        switch (step++) {
            case 0:
                return invoke("vehicle:open", {.row = "prototype:bicycle"});
            case 1:
                CHECK(slot_of(screen, "vehicle:history") == "log");
                CHECK(part(screen, "vehicle:history").label == "History");
                CHECK(labels(screen) == std::vector<std::string>{"Opened"});
                return invoke("vehicle:choose", {.row = "vpart:headlight"});
            case 2:
                return invoke("vehicle:mount", {.row = "mount:1,0"});
            case 3:
                return invoke("vehicle:tool", {.row = "tool:vehicle"});
            case 4:
                return invoke("vehicle:prop_set", {.row = "name", .text = "Lit bicycle"});
            case 5:
                CHECK(labels(screen)
                      == std::vector<
                          std::string>{"Opened", "Install headlight at 1,0", "Change name"});
                CHECK(row(screen, "vehicle:history", "step:0").description == "Saved");
                CHECK(at_step(screen) == "step:2");
                return invoke("vehicle:jump", {.row = "step:0"});
            case 6:
                CHECK(at_step(screen) == "step:0");
                // The later steps stay to go back to.
                CHECK(labels(screen).size() == 3);
                CHECK(stat(screen, "saved").value == "Yes");
                return invoke("vehicle:jump", {.row = "step:1"});
            case 7:
                CHECK(at_step(screen) == "step:1");
                return invoke("vehicle:tool", {.row = "tool:mount"});
            case 8:
                CHECK(has_light(screen));
                return invoke("vehicle:jump", {.row = "step:0"});
            case 9:
                return act("QUIT");
            default:
                FAIL("the vehicle editor outlived its exit");
                return act("QUIT");
        }
    });
    editor::run_vehicle_editor();
    CHECK(step == 10);
}

TEST_CASE(
    "the vehicle editor lists the steps of a world vehicle edited in place in its History",
    "[editor_vehicle]") {
    const auto guard = screen_guard{};
    clear_map();
    const auto cleanup_vehicles = on_out_of_scope([] { clear_vehicles(); });
    const auto at = get_avatar().bub_pos() + tripoint_rel_ms(8, 0, 0);
    REQUIRE(editor::spawn_design(vproto_id("bicycle"), at, {}));
    const auto labels = [](const ec::screen_view& screen) {
        return body<ec::list_body>(part(screen, "vehicle:history")).rows
             | std::views::transform(&ec::row::label) | std::ranges::to<std::vector>();
    };
    const auto title = [](const ec::screen_view& screen) {
        return body<ec::panel_body>(part(screen, "vehicle:live")).title;
    };
    auto step = 0;
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto screen = ec::top_screen(captured());
        switch (step++) {
            case 0:
                return invoke("vehicle:source", {.row = "source:world"});
            case 1:
                return invoke("vehicle:edit", {.row = "world:0"});
            case 2:
                CHECK(slot_of(screen, "vehicle:history") == "log");
                CHECK(labels(screen) == std::vector<std::string>{"Opened"});
                return invoke("vehicle:choose", {.row = "vpart:headlight"});
            case 3:
                return invoke("vehicle:mount", {.row = "mount:1,0"});
            case 4:
                return invoke("vehicle:tool", {.row = "tool:vehicle"});
            case 5:
                return invoke("vehicle:live_set", {.row = "name", .text = "Mine"});
            case 6:
                CHECK(labels(screen)
                      == std::vector<
                          std::string>{"Opened", "Install headlight at 1,0", "Rename to Mine"});
                CHECK(row(screen, "vehicle:history", "step:2").selected);
                return invoke("vehicle:jump", {.row = "step:0"});
            case 7:
                CHECK(title(screen) == "Bicycle");
                CHECK(row(screen, "vehicle:history", "step:0").selected);
                CHECK(labels(screen).size() == 3);
                return invoke("vehicle:jump", {.row = "step:2"});
            case 8:
                CHECK(title(screen) == "Mine");
                return act("QUIT");
            default:
                FAIL("the vehicle editor outlived its exit");
                return act("QUIT");
        }
    });
    editor::run_vehicle_editor();
    CHECK(step == 9);
}

TEST_CASE(
    "the vehicle editor keeps its grid in place while the player picks and installs at its edge",
    "[editor_vehicle]") {
    const auto guard = screen_guard{};
    const auto grid = [](const ec::screen_view& screen) {
        return body<ec::grid_body>(part(screen, "vehicle:grid"));
    };
    const auto place = [&](const ec::screen_view& screen) {
        const auto& at = cell(screen, "mount:0,0").at;
        return std::array{at.x, at.y, grid(screen).cols, grid(screen).rows};
    };
    /// The north-west cell of the grid.
    const auto corner = [&](const ec::screen_view& screen) {
        const auto& cells = grid(screen).cells;
        return std::
            ranges::
                find_if(cells, [](const ec::grid_cell& entry) {
                    return entry.at.x == 0 && entry.at.y == 0;
                })->id;
    };
    const auto west = [&](const ec::screen_view& screen) {
        const auto& cells = grid(screen).cells;
        const auto& zero = cell(screen, "mount:-1,0").at;
        return std::
            ranges::
                find_if(cells, [&](const ec::grid_cell& entry) {
                    return entry.at.x == zero.x - 1 && entry.at.y == zero.y;
                })->id;
    };
    auto first = std::array<int, 4>{};
    auto step = 0;
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto screen = ec::top_screen(captured());
        switch (step++) {
            case 0:
                return invoke("vehicle:open", {.row = "prototype:bicycle"});
            case 1:
                first = place(screen);
                return invoke("vehicle:mount", {.row = corner(screen)});
            case 2:
                // The cells stay under the pointer.
                CHECK(place(screen) == first);
                return invoke("vehicle:choose", {.row = "vpart:xlframe_vertical"});
            case 3:
                return invoke("vehicle:mount", {.row = west(screen)});
            case 4:
                CHECK(cell(screen, "mount:-2,0").label.find("frame") != std::string::npos);
                CHECK(place(screen) == first);
                return invoke("vehicle:undo");
            case 5:
                // Nor does taking the part away move them.
                CHECK(place(screen) == first);
                return act("QUIT");
            default:
                FAIL("the vehicle editor outlived its exit");
                return act("QUIT");
        }
    });
    editor::run_vehicle_editor();
    CHECK(step == 6);
}

TEST_CASE(
    "the vehicle editor's grid shows every part on a mount, the shown one on top",
    "[editor_vehicle]") {
    const auto guard = screen_guard{};
    const auto ids = [](const ec::screen_view& screen, const std::string& id) {
        const auto& grid = body<ec::grid_body>(part(screen, "vehicle:grid"));
        return cell(screen, id).looks | std::views::transform([&](const int index) {
                   return grid.looks[index].id.value_or("");
               })
             | std::ranges::to<std::vector>();
    };
    auto step = 0;
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto screen = ec::top_screen(captured());
        switch (step++) {
            case 0:
                return invoke("vehicle:open", {.row = "prototype:bicycle"});
            case 1: {
                const auto front = ids(screen, "mount:1,0");
                CHECK(std::ranges::is_permutation(
                    front,
                    std::vector<std::string>{
                        "xlframe_vertical", "wheel_mount_light_steerable", "wheel_bicycle"}));
                // The frame is under the rest, and the part the game shows is on top.
                CHECK(front.front() == "xlframe_vertical");
                CHECK(front.back() == "wheel_bicycle");
                return invoke("vehicle:choose", {.row = "vpart:headlight"});
            }
            case 2:
                return invoke("vehicle:mount", {.row = "mount:1,0"});
            case 3:
                CHECK(std::ranges::contains(ids(screen, "mount:1,0"), std::string("headlight")));
                CHECK(ids(screen, "mount:1,0").size() == 4);
                return invoke("vehicle:undo");
            case 4:
                return act("QUIT");
            default:
                FAIL("the vehicle editor outlived its exit");
                return act("QUIT");
        }
    });
    editor::run_vehicle_editor();
    CHECK(step == 5);
}

TEST_CASE(
    "the vehicle editor sets nothing of a part on a mount that has none", "[editor_vehicle]") {
    const auto guard = screen_guard{};
    const auto settings = [](const ec::screen_view& screen) {
        return body<ec::tabs_body>(part(screen, "vehicle:tools")).tabs[1];
    };
    auto step = 0;
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto screen = ec::top_screen(captured());
        switch (step++) {
            case 0:
                return invoke("vehicle:filter", {.text = "armored"});
            case 1:
                return invoke("vehicle:open", {.row = "prototype:armored_car"});
            case 2:
                return invoke("vehicle:mount", {.row = "mount:-3,1"});
            case 3:
                return invoke("vehicle:pick", {.row = "part:3"});
            case 4:
                CHECK(settings(screen).enabled);
                // An empty mount of the grid.
                return invoke(
                    "vehicle:mount",
                    {.row = std::ranges::find_if(
                                body<ec::grid_body>(part(screen, "vehicle:grid")).cells,
                                [](const ec::grid_cell& entry) { return entry.label.empty(); })
                                ->id});
            case 5:
                CHECK_FALSE(settings(screen).enabled);
                return invoke("vehicle:show:part");
            case 6:
                CHECK(ec::find_component(*screen.root, "vehicle:part_form") == nullptr);
                return act("QUIT");
            default:
                FAIL("the vehicle editor outlived its exit");
                return act("QUIT");
        }
    });
    editor::run_vehicle_editor();
    CHECK(step == 7);
}

TEST_CASE(
    "the vehicle editor ignores what acts on a vehicle while none is open", "[editor_vehicle]") {
    const auto guard = screen_guard{};
    auto step = 0;
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto screen = ec::top_screen(captured());
        switch (step++) {
            case 0:
                return invoke("vehicle:mount", {.row = "mount:0,0"});
            case 1:
                return invoke("vehicle:remove");
            case 2:
                CHECK(ec::find_component(*screen.root, "vehicle:grid") == nullptr);
                return act("QUIT");
            default:
                FAIL("the vehicle editor outlived its exit");
                return act("QUIT");
        }
    });
    editor::run_vehicle_editor();
    CHECK(step == 3);
}

TEST_CASE(
    "the vehicle editor shows no tab a world vehicle edited in place lacks", "[editor_vehicle]") {
    const auto guard = screen_guard{};
    clear_map();
    const auto cleanup_vehicles = on_out_of_scope([] { clear_vehicles(); });
    REQUIRE(editor::spawn_design(
        vproto_id("bicycle"), get_avatar().bub_pos() + tripoint_rel_ms(8, 0, 0), {}));
    const auto tabs = [](const ec::screen_view& screen) {
        return body<ec::tabs_body>(part(screen, "vehicle:tools")).tabs;
    };
    auto step = 0;
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto screen = ec::top_screen(captured());
        switch (step++) {
            case 0:
                return invoke("vehicle:source", {.row = "source:world"});
            case 1:
                return invoke("vehicle:edit", {.row = "world:0"});
            case 2:
                return invoke("vehicle:show:cargo");
            case 3:
                CHECK(std::ranges::count_if(tabs(screen), &ec::tab_entry::selected) == 1);
                CHECK(tabs(screen)[0].selected);
                return invoke("vehicle:tool", {.row = "tool:json"});
            case 4:
                CHECK(tabs(screen)[0].selected);
                return act("QUIT");
            default:
                FAIL("the vehicle editor outlived its exit");
                return act("QUIT");
        }
    });
    editor::run_vehicle_editor();
    CHECK(step == 5);
}

TEST_CASE(
    "the vehicle editor puts item spawns on a mount from a palette of items and groups by name",
    "[editor_vehicle]") {
    const auto guard = screen_guard{};
    const auto spawns = [](const ec::screen_view& screen) {
        return rows(screen, "vehicle:items") | std::views::transform(&ec::row::label)
             | std::ranges::to<std::vector>();
    };
    auto step = 0;
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto screen = ec::top_screen(captured());
        switch (step++) {
            case 0:
                return invoke("vehicle:open", {.row = "prototype:bicycle"});
            case 1:
                // The items show only while the item spawns do.
                CHECK(ec::find_component(*screen.root, "vehicle:item_kinds") == nullptr);
                return invoke("vehicle:mount", {.row = "mount:-1,0"});
            case 2:
                return invoke("vehicle:show:cargo");
            case 3:
                CHECK(slot_of(screen, "vehicle:item_kinds") == "palette");
                CHECK(part(screen, "vehicle:item_kinds").label == "Items to add");
                CHECK(body<ec::list_body>(part(screen, "vehicle:item_catalog")).total > 1000);
                return invoke("vehicle:item_filter", {.text = "flashlight"});
            case 4:
                CHECK(row(screen, "vehicle:item_catalog", "item:flashlight").look.has_value());
                return invoke("vehicle:item_pick", {.row = "item:flashlight"});
            case 5:
                CHECK(spawns(screen) == std::vector<std::string>{"flashlight (off)"});
                CHECK(body<ec::list_body>(part(screen, "vehicle:history")).rows.back().label
                      == "Add flashlight (off) at -1,0");
                return invoke("vehicle:item_kind", {.row = "kind:item_group"});
            case 6:
                return invoke("vehicle:item_filter", {.text = "tools_common"});
            case 7:
                return invoke("vehicle:item_pick", {.row = "group:tools_common"});
            case 8:
                CHECK(spawns(screen).size() == 2);
                CHECK(row(screen, "vehicle:items", "item:1").description == "Item group");
                return invoke("vehicle:undo");
            case 9:
                return invoke("vehicle:undo");
            case 10:
                return act("QUIT");
            default:
                FAIL("the vehicle editor outlived its exit");
                return act("QUIT");
        }
    });
    editor::run_vehicle_editor();
    CHECK(step == 11);
}

TEST_CASE(
    "the vehicle editor says in words why the raw JSON cannot be set there", "[editor_vehicle]") {
    const auto guard = screen_guard{};
    auto step = 0;
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto screen = ec::top_screen(captured());
        switch (step++) {
            case 0:
                return invoke("vehicle:open", {.row = "prototype:bicycle"});
            case 1:
                return invoke("vehicle:show:json");
            case 2:
                return invoke("vehicle:field_set", {.row = "pointer", .text = "name"});
            case 3:
                return invoke("vehicle:field_apply");
            case 4:
                CHECK(notice_of(screen) == "A pointer starts with /, as /name does.");
                return invoke("vehicle:field_set", {.row = "pointer", .text = "/nothing/here"});
            case 5:
                return invoke("vehicle:field_set", {.row = "value", .text = "1"});
            case 6:
                return invoke("vehicle:field_apply");
            case 7:
                CHECK(notice_of(screen) == "The prototype has nothing at /nothing to hold here.");
                CHECK(stat(screen, "saved").value == "Yes");
                return act("QUIT");
            default:
                FAIL("the vehicle editor outlived its exit");
                return act("QUIT");
        }
    });
    editor::run_vehicle_editor();
    CHECK(step == 8);
}

TEST_CASE("the vehicle editor says what it did to the world's vehicles", "[editor_vehicle]") {
    const auto guard = screen_guard{};
    clear_map();
    const auto cleanup_vehicles = on_out_of_scope([] { clear_vehicles(); });
    REQUIRE(editor::spawn_design(
        vproto_id("bicycle"), get_avatar().bub_pos() + tripoint_rel_ms(-8, 0, 0), {}));
    const auto copied = std::filesystem::path(PATH_INFO::user_dir()) / "vehicle_copy.json";
    std::filesystem::remove(copied);
    const auto cleanup = on_out_of_scope([&] { std::filesystem::remove(copied); });
    const auto status = [](const ec::screen_view& screen) {
        const auto& said = body<ec::text_body>(part(screen, "vehicle:status"));
        CHECK(said.tone == "good");
        CHECK(slot_of(screen, "vehicle:status") == "log");
        return said.markup;
    };
    auto step = 0;
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto screen = ec::top_screen(captured());
        switch (step++) {
            case 0:
                return invoke("vehicle:source", {.row = "source:world"});
            case 1:
                return invoke("vehicle:copy", {.row = "world:0"});
            case 2:
                CHECK(status(screen) == "Copied Bicycle; paste it in any world.");
                return invoke("vehicle:paste");
            case 3:
                CHECK(status(screen) == "Pasted Bicycle 8 squares south of you.");
                return invoke("vehicle:show:prototypes");
            case 4:
                CHECK(ec::find_component(*screen.root, "vehicle:status") == nullptr);
                return invoke("vehicle:open", {.row = "prototype:bicycle"});
            case 5:
                return invoke("vehicle:spawn");
            case 6:
                CHECK(status(screen)
                      == "Spawned Bicycle 8 squares east of you. Vehicles > World > Edit in place "
                         "changes it where it stands.");
                return act("QUIT");
            default:
                FAIL("the vehicle editor outlived its exit");
                return act("QUIT");
        }
    });
    editor::run_vehicle_editor();
    CHECK(step == 7);
}

TEST_CASE(
    "the vehicle editor adds the item a palette row names even when a group has its id",
    "[editor_vehicle]") {
    const auto guard = screen_guard{};
    auto step = 0;
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto screen = ec::top_screen(captured());
        switch (step++) {
            case 0:
                return invoke("vehicle:open", {.row = "prototype:bicycle"});
            case 1:
                return invoke("vehicle:mount", {.row = "mount:-1,0"});
            case 2:
                return invoke("vehicle:show:cargo");
            case 3:
                return invoke("vehicle:item_filter", {.text = "beer"});
            case 4:
                // `beer` is an item and an item group both.
                return invoke("vehicle:item_pick", {.row = "item:beer"});
            case 5:
                REQUIRE(rows(screen, "vehicle:items").size() == 1);
                CHECK(row(screen, "vehicle:items", "item:0").description.empty());
                return invoke("vehicle:item_kind", {.row = "kind:item_group"});
            case 6:
                return invoke("vehicle:item_pick", {.row = "group:beer"});
            case 7:
                REQUIRE(rows(screen, "vehicle:items").size() == 2);
                CHECK(row(screen, "vehicle:items", "item:1").description == "Item group");
                return invoke("vehicle:undo");
            case 8:
                return invoke("vehicle:undo");
            case 9:
                return act("QUIT");
            default:
                FAIL("the vehicle editor outlived its exit");
                return act("QUIT");
        }
    });
    editor::run_vehicle_editor();
    CHECK(step == 10);
}

TEST_CASE(
    "the vehicle editor's raw JSON removes a list entry by its index and names the step",
    "[editor_vehicle]") {
    const auto guard = screen_guard{};
    const auto last_step = [](const ec::screen_view& screen) {
        return body<ec::list_body>(part(screen, "vehicle:history")).rows.back().label;
    };
    auto step = 0;
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto screen = ec::top_screen(captured());
        switch (step++) {
            case 0:
                return invoke("vehicle:open", {.row = "prototype:bicycle"});
            case 1:
                return invoke("vehicle:show:json");
            case 2:
                // The front wheel's mount: a frame, a wheel mount and a wheel.
                return invoke("vehicle:field_set", {.row = "pointer", .text = "/parts/1"});
            case 3:
                return invoke("vehicle:field_set", {.row = "value", .text = ""});
            case 4:
                return invoke("vehicle:field_apply");
            case 5:
                CHECK(notice_of(screen).empty());
                CHECK(stat(screen, "saved").value == "No");
                CHECK(last_step(screen)
                      == "Remove 3 parts (bicycle wheel, extra light frame, light wheel mount "
                         "(steerable))");
                return invoke("vehicle:field_set", {.row = "pointer", .text = "/parts/9"});
            case 6:
                return invoke("vehicle:field_set", {.row = "value", .text = ""});
            case 7:
                return invoke("vehicle:field_apply");
            case 8:
                CHECK(notice_of(screen) == "The prototype has nothing at /parts/9 to remove.");
                return invoke("vehicle:undo");
            case 9:
                CHECK(stat(screen, "saved").value == "Yes");
                return act("QUIT");
            default:
                FAIL("the vehicle editor outlived its exit");
                return act("QUIT");
        }
    });
    editor::run_vehicle_editor();
    CHECK(step == 10);
}

TEST_CASE(
    "the vehicle editor keeps a refusal in view until an action changes something",
    "[editor_vehicle]") {
    const auto guard = screen_guard{};
    const auto refusal = std::string("A pointer starts with /, as /name does.");
    auto step = 0;
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto screen = ec::top_screen(captured());
        switch (step++) {
            case 0:
                return invoke("vehicle:open", {.row = "prototype:bicycle"});
            case 1:
                return invoke("vehicle:show:json");
            case 2:
                return invoke("vehicle:field_set", {.row = "pointer", .text = "name"});
            case 3:
                return invoke("vehicle:field_apply");
            case 4:
                CHECK(notice_of(screen) == refusal);
                // Looking around changes nothing.
                return invoke("vehicle:show:mount");
            case 5:
                CHECK(notice_of(screen) == refusal);
                return invoke("vehicle:mount", {.row = "mount:0,0"});
            case 6:
                CHECK(notice_of(screen) == refusal);
                return invoke("vehicle:part_filter", {.text = "light"});
            case 7:
                CHECK(notice_of(screen) == refusal);
                return invoke("vehicle:prop_set", {.row = "name", .text = ""});
            case 8:
                // A new refusal takes its place.
                CHECK(notice_of(screen) == "A prototype keeps its name and id.");
                return invoke("vehicle:prop_set", {.row = "name", .text = "Kept"});
            case 9:
                CHECK(notice_of(screen).empty());
                return invoke("vehicle:undo");
            case 10:
                return act("QUIT");
            default:
                FAIL("the vehicle editor outlived its exit");
                return act("QUIT");
        }
    });
    editor::run_vehicle_editor();
    CHECK(step == 11);
}

TEST_CASE(
    "the vehicle editor spawns and pastes at the nearest clear place when walls stand at the first",
    "[editor_vehicle]") {
    const auto guard = screen_guard{};
    clear_map();
    const auto cleanup_vehicles = on_out_of_scope([] { clear_vehicles(); });
    const auto copied = std::filesystem::path(PATH_INFO::user_dir()) / "vehicle_copy.json";
    std::filesystem::remove(copied);
    const auto cleanup = on_out_of_scope([&] {
        std::filesystem::remove(copied);
        clear_map();
    });
    const auto here = get_avatar().bub_pos();
    // Walls on the squares a bicycle would cover 8 east and 8 south of the avatar.
    for (const auto& first : {tripoint_rel_ms(8, 0, 0), tripoint_rel_ms(0, 8, 0)}) {
        for (auto dx = -2; dx <= 2; ++dx) {
            for (auto dy = -2; dy <= 2; ++dy) {
                get_map().ter_set(here + first + tripoint_rel_ms(dx, dy, 0), ter_id("t_wall"));
            }
        }
    }
    const auto status = [](const ec::screen_view& screen) {
        const auto* const found = ec::find_component(*screen.root, "vehicle:status");
        return found ? body<ec::text_body>(*found).markup : std::string{};
    };
    auto step = 0;
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto screen = ec::top_screen(captured());
        switch (step++) {
            case 0:
                return invoke("vehicle:source", {.row = "source:world"});
            case 1:
                // Nothing near to act on: the world's actions say so.
                CHECK_FALSE(affordance(screen, "vehicle:edit").enabled);
                CHECK(affordance(screen, "vehicle:edit").denial == "No vehicle is near you.");
                CHECK_FALSE(affordance(screen, "vehicle:copy").enabled);
                CHECK_FALSE(affordance(screen, "vehicle:export").enabled);
                return invoke("vehicle:open", {.row = "prototype:bicycle"});
            case 2:
                return invoke("vehicle:spawn");
            case 3:
                CHECK(notice_of(screen).empty());
                CHECK(status(screen).starts_with("Spawned Bicycle "));
                CHECK(get_map().get_vehicles().size() == 1);
                return invoke("vehicle:source", {.row = "source:world"});
            case 4:
                CHECK(affordance(screen, "vehicle:edit").enabled);
                return invoke("vehicle:copy", {.row = "world:0"});
            case 5:
                return invoke("vehicle:paste");
            case 6:
                CHECK(notice_of(screen).empty());
                CHECK(status(screen).starts_with("Pasted Bicycle "));
                CHECK(get_map().get_vehicles().size() == 2);
                return act("QUIT");
            default:
                FAIL("the vehicle editor outlived its exit");
                return act("QUIT");
        }
    });
    editor::run_vehicle_editor();
    CHECK(step == 7);
}

TEST_CASE(
    "the vehicle editor loads a world turret in place with the rounds it fires",
    "[editor_vehicle]") {
    const auto guard = screen_guard{};
    clear_map();
    const auto cleanup_vehicles = on_out_of_scope([] { clear_vehicles(); });
    REQUIRE(editor::spawn_design(
        vproto_id("armored_car"), get_avatar().bub_pos() + tripoint_rel_ms(10, 0, 0), {}));
    const auto gun = [](const ec::screen_view& screen) {
        const auto& stack = rows(screen, "vehicle:stack");
        const auto found = std::ranges::find(stack, "mounted M249", &ec::row::label);
        REQUIRE(found != stack.end());
        return *found;
    };
    const auto field = [](const ec::screen_view& screen, const std::string& id) {
        const auto& fields = body<ec::form_body>(part(screen, "vehicle:live_form")).fields;
        const auto found = std::ranges::find(fields, id, &ec::form_field::id);
        INFO(id);
        REQUIRE(found != fields.end());
        return *found;
    };
    auto step = 0;
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto screen = ec::top_screen(captured());
        switch (step++) {
            case 0:
                return invoke("vehicle:source", {.row = "source:world"});
            case 1:
                return invoke("vehicle:edit", {.row = "world:0"});
            case 2:
                return invoke("vehicle:mount", {.row = "mount:-3,1"});
            case 3:
                return invoke("vehicle:pick", {.row = gun(screen).id});
            case 4:
                return invoke("vehicle:show:part");
            case 5:
                CHECK(field(screen, "fuel").label == "Rounds");
                CHECK(field(screen, "fuel_type").label == "Ammunition");
                return invoke("vehicle:live_set", {.row = "fuel_type", .text = "556"});
            case 6:
                CHECK(notice_of(screen).empty());
                CHECK(field(screen, "fuel_type").value == "556");
                return invoke("vehicle:live_set", {.row = "fuel", .text = "25"});
            case 7:
                CHECK(notice_of(screen).empty());
                CHECK(field(screen, "fuel").value == "25");
                return invoke("vehicle:live_set", {.row = "fuel", .text = "0"});
            case 8:
                CHECK(notice_of(screen).empty());
                CHECK(field(screen, "fuel").value == "0");
                // An emptied turret loads the rounds it held again.
                return invoke("vehicle:live_set", {.row = "fuel", .text = "10"});
            case 9:
                CHECK(notice_of(screen).empty());
                CHECK(field(screen, "fuel").value == "10");
                CHECK(field(screen, "fuel_type").value == "556");
                return invoke("vehicle:show:mount");
            case 10:
                CHECK(gun(screen).columns.size() == 2);
                return act("QUIT");
            default:
                FAIL("the vehicle editor outlived its exit");
                return act("QUIT");
        }
    });
    editor::run_vehicle_editor();
    CHECK(step == 11);
}

TEST_CASE(
    "the vehicle editor says where it spawned and pasted in one sentence a language translates "
    "whole",
    "[editor_vehicle]") {
    const auto guard = screen_guard{};
    clear_map();
    const auto cleanup_vehicles = on_out_of_scope([] { clear_vehicles(); });
    const auto copied = std::filesystem::path(PATH_INFO::user_dir()) / "vehicle_copy.json";
    std::filesystem::remove(copied);
    const auto cleanup = on_out_of_scope([&] { std::filesystem::remove(copied); });
    const auto fixture = locale_fixture::korean_catalogue{
        {{locale_fixture::plural(
              "Spawned %1$s %2$d square %3$s of you.", "Spawned %1$s %2$d squares %3$s of you."),
          locale_fixture::plural("S1 %3$s %2$d %1$s", "SN %3$s %2$d %1$s")},
         {locale_fixture::plural(
              "Pasted %1$s %2$d square %3$s of you.", "Pasted %1$s %2$d squares %3$s of you."),
          locale_fixture::plural("P1 %3$s %2$d %1$s", "PN %3$s %2$d %1$s")}}};
    const auto status = [](const ec::screen_view& screen) {
        const auto* const found = ec::find_component(*screen.root, "vehicle:status");
        return found ? body<ec::text_body>(*found).markup : std::string{};
    };
    auto step = 0;
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto screen = ec::top_screen(captured());
        switch (step++) {
            case 0:
                return invoke("vehicle:open", {.row = "prototype:bicycle"});
            case 1:
                return invoke("vehicle:spawn");
            case 2:
                CHECK(status(screen).starts_with("SN "));
                CHECK(status(screen).contains(" 8 Bicycle "));
                return invoke("vehicle:source", {.row = "source:world"});
            case 3:
                return invoke("vehicle:copy", {.row = "world:0"});
            case 4:
                return invoke("vehicle:paste");
            case 5:
                CHECK(status(screen).starts_with("PN "));
                CHECK(status(screen).ends_with(" 8 Bicycle"));
                return act("QUIT");
            default:
                FAIL("the vehicle editor outlived its exit");
                return act("QUIT");
        }
    });
    {
        const auto scope = localization::locale_scope{locale_fixture::korean};
        editor::run_vehicle_editor();
    }
    CHECK(step == 6);
}

TEST_CASE(
    "the vehicle editor fills in a free file name of the user's mods for New vehicle and Save as",
    "[editor_vehicle]") {
    const auto guard = screen_guard{};
    const auto folder = std::filesystem::path(PATH_INFO::user_moddir()) / "my_mod";
    std::filesystem::remove_all(folder);
    std::filesystem::create_directories(folder);
    // A file the name would take already exists.
    std::ofstream(folder / "my_vehicle.json") << "[]";
    const auto cleanup = on_out_of_scope([&] { std::filesystem::remove_all(folder); });
    const auto initial = [](const ec::screen_view& screen, const std::string& id) {
        const auto& text = affordance(screen, id).args.text;
        REQUIRE(text.has_value());
        return text->initial;
    };
    auto step = 0;
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto screen = ec::top_screen(captured());
        switch (step++) {
            case 0:
                // New asks for a name, whose file is free.
                CHECK(initial(screen, "vehicle:new") == "My vehicle");
                return invoke("vehicle:open", {.row = "prototype:bicycle"});
            case 1:
                CHECK(initial(screen, "vehicle:save_as") == "my_mod/bicycle");
                return invoke("vehicle:new", {.text = initial(screen, "vehicle:new")});
            case 2:
                CHECK(stat(screen, "file").value.ends_with("my_mod/my_vehicle_2.json"));
                return invoke("vehicle:save");
            case 3:
                return act("QUIT");
            default:
                FAIL("the vehicle editor outlived its exit");
                return act("QUIT");
        }
    });
    editor::run_vehicle_editor();
    CHECK(step == 4);
}

TEST_CASE(
    "the vehicle editor turns away a spawn value out of its bounds, naming the field and them",
    "[editor_vehicle][engine_client_component]") {
    const auto guard = screen_guard{};
    auto step = 0;
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto state = captured();
        switch (step++) {
            case 0:
                return invoke("vehicle:open", {.row = "prototype:bicycle"});
            case 1:
                return invoke("vehicle:show:spawn");
            case 2:
                CHECK(refusal(state, "vehicle:spawn_set", {.row = "facing", .text = "400"})
                      == "Facing takes a whole number from 0 to 359.");
                CHECK(refusal(state, "vehicle:spawn_set", {.row = "fuel", .text = "-1"})
                      == "Fuel % takes a whole number from 0 to 100.");
                CHECK(refusal(state, "vehicle:spawn_set", {.row = "facing", .text = "90"}).empty());
                return act("QUIT");
            default:
                FAIL("the vehicle editor outlived its exit");
                return act("QUIT");
        }
    });
    editor::run_vehicle_editor();
    CHECK(step == 3);
}

TEST_CASE(
    "the vehicle editor's picking of a part on a mount is a selection",
    "[editor_vehicle][engine_client_component]") {
    const auto guard = screen_guard{};
    auto step = 0;
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto screen = ec::top_screen(captured());
        switch (step++) {
            case 0:
                return invoke("vehicle:open", {.row = "prototype:bicycle"});
            case 1:
                return invoke("vehicle:show:mount");
            case 2:
                CHECK(affordance(screen, "vehicle:pick").role == "select");
                return act("QUIT");
            default:
                FAIL("the vehicle editor outlived its exit");
                return act("QUIT");
        }
    });
    editor::run_vehicle_editor();
    CHECK(step == 3);
}

TEST_CASE(
    "the vehicle editor names no heading like an action or a tab",
    "[editor_vehicle][engine_client_component]") {
    const auto guard = screen_guard{};
    const auto tabs =
        std::vector<std::string>{"mount", "part", "vehicle", "cargo", "spawn", "json"};
    auto step = std::size_t{0};
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto screen = ec::top_screen(captured());
        const auto at = step++;
        INFO(at);
        CHECK(headings_named_like_actions(screen) == std::vector<std::string>{});
        if (at == 0) { return invoke("vehicle:open", {.row = "prototype:bicycle"}); }
        if (at - 1 < tabs.size()) { return invoke("vehicle:show:" + tabs[at - 1]); }
        return act("QUIT");
    });
    editor::run_vehicle_editor();
    CHECK(step == tabs.size() + 2);
}

TEST_CASE(
    "the vehicle editor explains its world actions and says what its empty lists mean",
    "[editor_vehicle][engine_client_component]") {
    const auto guard = screen_guard{};
    clear_map();
    const auto cleanup_vehicles = on_out_of_scope([] { clear_vehicles(); });
    const auto top = [](const ec::boundary_state& state) {
        return nlohmann::json::parse(ec::serialize_interaction_boundary(state))["stack"].back();
    };
    auto step = 0;
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto state = captured();
        const auto screen = ec::top_screen(state);
        switch (step++) {
            case 0:
                return invoke("vehicle:source", {.row = "source:world"});
            case 1:
                return invoke("vehicle:edit", {.row = "world:0"});
            case 2:
                return invoke("vehicle:mount", {.row = "mount:-3,1"});
            case 3:
                return invoke("vehicle:pick", {.row = named(screen, "mounted M249")});
            case 4:
                return invoke("vehicle:show:part");
            case 5: {
                const auto& ammo = form_field(screen, "vehicle:live_form", "fuel_type");
                CHECK(ammo.label == "Ammunition");
                CHECK(ammo.type == "select");
                CHECK(offers(ammo, "556"));
                CHECK_FALSE(offers(ammo, "gasoline"));
                CHECK(refused(state, "vehicle:live_set", {.row = "fuel_type", .text = "9mm"}));
                return invoke("vehicle:live_set", {.row = "fuel_type", .text = "556"});
            }
            case 6:
                CHECK(notice_of(screen).empty());
                return act("QUIT");
            default:
                FAIL("the vehicle editor outlived its exit");
                return act("QUIT");
        }
    });
    editor::run_vehicle_editor();
    CHECK(step == 7);
}

TEST_CASE(
    "the vehicle editor puts a new vehicle in a mod folder of the player's mods and makes that "
    "folder a mod when it saves",
    "[editor_vehicle][engine_client_component]") {
    const auto guard = screen_guard{};
    const auto mods = std::filesystem::path(PATH_INFO::user_moddir());
    const auto folder = mods / "vehicle_editor_mod";
    const auto forget = [&] {
        std::filesystem::remove_all(folder);
        std::filesystem::remove(mods / "flat_vehicle.json");
        std::filesystem::remove(editor::recent_documents_file());
    };
    forget();
    const auto cleanup = on_out_of_scope(forget);
    auto step = 0;
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto screen = ec::top_screen(captured());
        switch (step++) {
            case 0:
                // A file loose in the mods is no mod's, so no world would load it.
                return invoke("vehicle:new", {.text = (mods / "flat_vehicle.json").string()});
            case 1:
                CHECK(notice_of(screen).contains("folder"));
                CHECK_FALSE(std::filesystem::exists(mods / "flat_vehicle.json"));
                return invoke("vehicle:new", {.text = "vehicle_editor_mod/kart.json"});
            case 2:
                CHECK(body<ec::panel_body>(part(screen, "vehicle:document")).title == "kart");
                // Nothing is written until the player saves.
                CHECK(stat(screen, "saved").value == "No");
                CHECK_FALSE(std::filesystem::exists(folder / "kart.json"));
                return invoke("vehicle:save");
            case 3:
                CHECK(stat(screen, "saved").value == "Yes");
                return act("QUIT");
            default:
                FAIL("the vehicle editor outlived its exit");
                return act("QUIT");
        }
    });
    editor::run_vehicle_editor();
    CHECK(step == 4);
    REQUIRE(std::filesystem::exists(folder / "kart.json"));
    REQUIRE(std::filesystem::exists(folder / "modinfo.json"));
    const auto info = nlohmann::ordered_json::parse(read_file(folder / "modinfo.json"));
    REQUIRE(info.is_array());
    REQUIRE(info.size() == 1);
    CHECK(info[0]["type"] == "MOD_INFO");
    CHECK(info[0]["id"] == "vehicle_editor_mod");
    CHECK(info[0]["name"] == "vehicle_editor_mod");
    CHECK(info[0]["category"] == "content");
    CHECK(
        info[0]["dependencies"]
        == nlohmann::ordered_json::array({mod_management::get_default_core_content_pack().str()}));
    // Opening it again loads that mod, which defines the vehicle.
    const auto recent = editor::recent_documents();
    REQUIRE_FALSE(recent.empty());
    CHECK(recent.front().name == "kart");
    CHECK(std::ranges::contains(recent.front().mods, mod_id("vehicle_editor_mod")));
}

TEST_CASE(
    "the vehicle editor asks before New, Open or Export replace a design with unsaved changes",
    "[editor_vehicle][engine_client_component]") {
    const auto guard = screen_guard{};
    clear_map();
    const auto cleanup_vehicles = on_out_of_scope([] { clear_vehicles(); });
    REQUIRE(editor::spawn_design(
        vproto_id("bicycle"), get_avatar().bub_pos() + tripoint_rel_ms(8, 0, 0), {}));
    const auto folder = std::filesystem::path(PATH_INFO::user_moddir()) / "vehicle_editor_ask";
    std::filesystem::remove_all(folder);
    const auto cleanup = on_out_of_scope([&] { std::filesystem::remove_all(folder); });
    const auto title = [](const ec::screen_view& screen) {
        return body<ec::panel_body>(part(screen, "vehicle:document")).title;
    };
    auto kinds = std::vector<std::string>{};
    auto step = 0;
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto screen = ec::top_screen(captured());
        kinds.push_back(screen.kind);
        switch (step++) {
            case 0:
                return invoke("vehicle:open", {.row = "prototype:bicycle"});
            case 1:
                return invoke("vehicle:show:vehicle");
            case 2:
                return invoke("vehicle:prop_set", {.row = "name", .text = "Unsaved"});
            case 3:
                return invoke("vehicle:open", {.row = "prototype:car"});
            case 4:
                CHECK(kinds.back() == "uilist");
                return answer("Stay");
            case 5:
                CHECK(title(screen) == "Unsaved");
                return invoke("vehicle:new", {.text = "vehicle_editor_ask/kart.json"});
            case 6:
                CHECK(kinds.back() == "uilist");
                return answer("Stay");
            case 7:
                CHECK(title(screen) == "Unsaved");
                CHECK_FALSE(std::filesystem::exists(folder / "kart.json"));
                return invoke("vehicle:source", {.row = "source:world"});
            case 8:
                return invoke("vehicle:export", {.row = "world:0", .text = "vehicle_editor_ask/x"});
            case 9:
                CHECK(kinds.back() == "uilist");
                return answer("Stay");
            case 10:
                CHECK(title(screen) == "Unsaved");
                CHECK_FALSE(std::filesystem::exists(folder / "x.json"));
                return invoke("vehicle:show:prototypes");
            case 11:
                return invoke("vehicle:open", {.row = "prototype:car"});
            case 12:
                CHECK(kinds.back() == "uilist");
                return answer("Leave without saving");
            case 13:
                CHECK(title(screen) == "Car");
                return act("QUIT");
            default:
                FAIL("the vehicle editor outlived its exit");
                return act("QUIT");
        }
    });
    editor::run_vehicle_editor();
    CHECK(step == 14);
}

TEST_CASE(
    "the vehicle editor picks the nearest world vehicle and tells apart vehicles of one name",
    "[editor_vehicle][engine_client_component]") {
    const auto guard = screen_guard{};
    clear_map();
    const auto cleanup_vehicles = on_out_of_scope([] { clear_vehicles(); });
    const auto here = get_avatar().bub_pos();
    auto* const far =
        editor::spawn_design(vproto_id("bicycle"), here + tripoint_rel_ms(0, 12, 0), {});
    auto* const near =
        editor::spawn_design(vproto_id("bicycle"), here + tripoint_rel_ms(8, 0, 0), {});
    REQUIRE(far != nullptr);
    REQUIRE(near != nullptr);
    const auto column = [](const ec::row& entry, const std::string& key) {
        const auto found =
            std::ranges::find(entry.columns, key, &game_client::interaction_column::key);
        REQUIRE(found != entry.columns.end());
        return found->value;
    };
    auto step = 0;
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto screen = ec::top_screen(captured());
        switch (step++) {
            case 0:
                return invoke("vehicle:source", {.row = "source:world"});
            case 1: {
                const auto& world = rows(screen, "vehicle:world");
                REQUIRE(world.size() == 2);
                // The nearest first, picked, so that Copy, Edit in place and Export act on it.
                CHECK(world[0].selected);
                CHECK_FALSE(world[1].selected);
                CHECK(screen.focus_row == world[0].id);
                CHECK(world[0].label == world[1].label);
                // The distance has its unit; the direction tells two of a name apart.
                CHECK(column(world[0], "distance") == "8 squares");
                CHECK(column(world[0], "direction") == "east");
                CHECK(column(world[1], "distance") == "12 squares");
                CHECK(column(world[1], "direction") == "south");
                // Export suggests a free file name, as New vehicle does.
                const auto& text = affordance(screen, "vehicle:export").args.text;
                REQUIRE(text.has_value());
                CHECK_FALSE(text->initial.empty());
                return act("QUIT");
            }
            default:
                FAIL("the vehicle editor outlived its exit");
                return act("QUIT");
        }
    });
    editor::run_vehicle_editor();
    CHECK(step == 2);
}

TEST_CASE(
    "the vehicle editor starts with the select tool on a design it opens and on a world vehicle it "
    "edits in place, and with the install tool on one it starts",
    "[editor_vehicle][engine_client_component]") {
    const auto guard = screen_guard{};
    clear_map();
    const auto cleanup_vehicles = on_out_of_scope([] { clear_vehicles(); });
    REQUIRE(editor::spawn_design(
        vproto_id("bicycle"), get_avatar().bub_pos() + tripoint_rel_ms(8, 0, 0), {}));
    const auto folder = std::filesystem::path(PATH_INFO::user_moddir()) / "vehicle_editor_tool";
    std::filesystem::remove_all(folder);
    const auto cleanup = on_out_of_scope([&] { std::filesystem::remove_all(folder); });
    auto step = 0;
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto screen = ec::top_screen(captured());
        switch (step++) {
            case 0:
                return invoke("vehicle:open", {.row = "prototype:bicycle"});
            case 1:
                CHECK(picked_tool(screen) == "canvas_tool:select");
                return invoke("vehicle:choose", {.row = "vpart:headlight"});
            case 2:
                CHECK(picked_tool(screen) == "canvas_tool:install");
                return invoke("vehicle:open", {.row = "prototype:car"});
            case 3:
                CHECK(picked_tool(screen) == "canvas_tool:select");
                return invoke("vehicle:tool_install");
            case 4:
                return invoke("vehicle:new", {.text = "vehicle_editor_tool/kart.json"});
            case 5:
                // A new design starts with a frame in hand.
                CHECK(picked_tool(screen) == "canvas_tool:install");
                return invoke("vehicle:tool_install");
            case 6:
                return invoke("vehicle:source", {.row = "source:world"});
            case 7:
                return invoke("vehicle:edit", {.row = "world:0"});
            case 8:
                CHECK(picked_tool(screen) == "canvas_tool:select");
                return invoke("vehicle:done");
            case 9:
                return invoke("vehicle:show:prototypes");
            case 10:
                return invoke("vehicle:open", {.row = "prototype:bicycle"});
            case 11:
                return answer("Leave without saving");
            case 12:
                return act("QUIT");
            default:
                FAIL("the vehicle editor outlived its exit");
                return act("QUIT");
        }
    });
    editor::run_vehicle_editor();
    CHECK(step == 13);
}

TEST_CASE(
    "the vehicle editor states in one line which field broken raw JSON breaks and that Undo puts "
    "it back",
    "[editor_vehicle][engine_client_component]") {
    const auto guard = screen_guard{};
    auto step = 0;
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto screen = ec::top_screen(captured());
        switch (step++) {
            case 0:
                return invoke("vehicle:open", {.row = "prototype:bicycle"});
            case 1:
                return invoke("vehicle:show:json");
            case 2:
                return invoke("vehicle:field_set", {.row = "pointer", .text = "/parts"});
            case 3:
                return invoke("vehicle:field_set", {.row = "value", .text = "42"});
            case 4:
                return invoke("vehicle:field_apply");
            case 5: {
                const auto said = messages(screen);
                REQUIRE(said.size() == 2);
                CHECK(said[0].starts_with("/parts: "));
                CHECK_FALSE(said[0].contains('\n'));
                CHECK_FALSE(said[0].contains("Json error"));
                CHECK(said[1].contains("Undo"));
                return invoke("vehicle:undo");
            }
            case 6:
                CHECK(messages(screen).empty());
                return act("QUIT");
            default:
                FAIL("the vehicle editor outlived its exit");
                return act("QUIT");
        }
    });
    editor::run_vehicle_editor();
    CHECK(step == 7);
}

TEST_CASE(
    "the vehicle editor turns away raw JSON that does not parse and a flag the game does not know",
    "[editor_vehicle][engine_client_component]") {
    const auto guard = screen_guard{};
    const auto name = [](const ec::screen_view& screen) {
        return body<ec::panel_body>(part(screen, "vehicle:document")).title;
    };
    auto step = 0;
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto screen = ec::top_screen(captured());
        switch (step++) {
            case 0:
                return invoke("vehicle:open", {.row = "prototype:bicycle"});
            case 1:
                return invoke("vehicle:show:json");
            case 2:
                // Opening picks the top part, so the form points at it until told otherwise.
                return invoke("vehicle:field_set", {.row = "pointer", .text = "/name"});
            case 3:
                // Text that starts as JSON does after a space is still meant as JSON.
                return invoke("vehicle:field_set", {.row = "value", .text = " [1, broken"});
            case 4:
                return invoke("vehicle:field_apply");
            case 5:
                CHECK(notice_of(screen).contains("JSON"));
                CHECK(name(screen) == "Bicycle");
                CHECK(stat(screen, "saved").value == "Yes");
                // A bare word is still a string.
                return invoke("vehicle:field_set", {.row = "value", .text = "Bike"});
            case 6:
                return invoke("vehicle:field_apply");
            case 7:
                CHECK(name(screen) == "Bike");
                return invoke("vehicle:show:vehicle");
            case 8:
                return invoke("vehicle:prop_set", {.row = "flags", .text = "NOT_A_FLAG"});
            case 9:
                CHECK(notice_of(screen).contains("NOT_A_FLAG"));
                CHECK(form_field(screen, "vehicle:props", "flags").value.empty());
                return invoke("vehicle:prop_set", {.row = "flags", .text = "VEHICLE_LOCKED"});
            case 10:
                CHECK(notice_of(screen).empty());
                CHECK(form_field(screen, "vehicle:props", "flags").value == "VEHICLE_LOCKED");
                return invoke("vehicle:undo");
            case 11:
                return invoke("vehicle:undo");
            case 12:
                return act("QUIT");
            default:
                FAIL("the vehicle editor outlived its exit");
                return act("QUIT");
        }
    });
    editor::run_vehicle_editor();
    CHECK(step == 13);
}

TEST_CASE(
    "the editors' refusal for want of the Debug Editor trait says where to turn it on",
    "[editor_vehicle][editor_session]") {
    const auto guard = screen_guard{};
    get_avatar().unset_mutation(trait_id("DEBUG_EDITOR"));
    editor::allow_editor_this_session(false);
    const auto denial = editor::editor_denial();
    CHECK(denial.contains("Debug Editor"));
    CHECK(denial.contains("debug menu"));
    CHECK(denial.contains("Mutate"));
}

TEST_CASE(
    "the vehicle editor sends a canvas tool and a history step as picked on a single click",
    "[editor_vehicle][engine_client_component]") {
    const auto guard = screen_guard{};
    // The affordance `id` as the boundary's wire carries it, null when it is not there.
    const auto wire_affordance = [](const ec::boundary_state& state, const std::string& id) {
        const auto wire = nlohmann::json::parse(ec::serialize_interaction_boundary(state));
        auto found = nlohmann::json();
        const auto walk = [&](this const auto& self, const nlohmann::json& value) -> void {
            if (!found.is_null() || !value.is_structured()) { return; }
            if (value.is_object() && value.contains("affordances")
                && value["affordances"].is_array()) {
                for (const auto& entry : value["affordances"]) {
                    if (entry.is_object() && entry.value("id", "") == id) {
                        found = entry;
                        return;
                    }
                }
            }
            for (const auto& child : value) { self(child); }
        };
        walk(wire);
        return found;
    };
    auto step = 0;
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto state = captured();
        switch (step++) {
            case 0:
                // Opening a prototype stays an activation, as a double click opens a file.
                CHECK(wire_affordance(state, "vehicle:open").value("role", "") != "select");
                return invoke("vehicle:open", {.row = "prototype:bicycle"});
            case 1: {
                const auto tool = wire_affordance(state, "vehicle:canvas_tool");
                REQUIRE_FALSE(tool.is_null());
                CHECK(tool.value("role", "") == "select");
                const auto jump = wire_affordance(state, "vehicle:jump");
                REQUIRE_FALSE(jump.is_null());
                CHECK(jump.value("role", "") == "select");
                return act("QUIT");
            }
            default:
                FAIL("the vehicle editor outlived its exit");
                return act("QUIT");
        }
    });
    editor::run_vehicle_editor();
    CHECK(step == 2);
}

TEST_CASE(
    "the vehicle editor's New suggests no file an unsaved new design takes and refuses to make it "
    "twice",
    "[editor_vehicle]") {
    const auto guard = screen_guard{};
    const auto folder = std::filesystem::path(PATH_INFO::user_moddir()) / "vehicle_editor_twice";
    std::filesystem::remove_all(folder);
    const auto cleanup = on_out_of_scope([&] { std::filesystem::remove_all(folder); });
    auto step = 0;
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto screen = ec::top_screen(captured());
        switch (step++) {
            case 0:
                return invoke("vehicle:new", {.text = "vehicle_editor_twice/my_vehicle.json"});
            case 1:
                CHECK_FALSE(std::filesystem::exists(folder / "my_vehicle.json"));
                return invoke("vehicle:new", {.text = "vehicle_editor_twice/my_vehicle.json"});
            case 2:
                CHECK(screen.kind == "uilist");
                return answer("Save and leave");
            case 3:
                // Saving made the file, so a second design of that name would overwrite it.
                CHECK(std::filesystem::exists(folder / "my_vehicle.json"));
                CHECK(notice_of(screen).contains("already exists"));
                return invoke("vehicle:show:vehicle");
            case 4:
                return invoke("vehicle:prop_set", {.row = "name", .text = "Twice"});
            case 5:
                return invoke("vehicle:save");
            case 6:
                CHECK_FALSE(notice_of(screen).contains("changed on disk"));
                CHECK(stat(screen, "saved").value == "Yes");
                // A name's file goes beside the open one, past the file there.
                return invoke("vehicle:new", {.text = "My vehicle"});
            case 7:
                CHECK(
                    stat(screen, "file").value.ends_with("vehicle_editor_twice/my_vehicle_2.json"));
                return invoke("vehicle:new", {.text = "My vehicle"});
            case 8:
                return answer("Leave without saving");
            case 9:
                // The unsaved design takes my_vehicle_2.json though the file is not there yet.
                CHECK_FALSE(std::filesystem::exists(folder / "my_vehicle_2.json"));
                CHECK(
                    stat(screen, "file").value.ends_with("vehicle_editor_twice/my_vehicle_3.json"));
                return act("QUIT");
            case 10:
                return answer("Leave without saving");
            default:
                FAIL("the vehicle editor outlived its exit");
                return act("QUIT");
        }
    });
    editor::run_vehicle_editor();
    CHECK(step == 11);
}

TEST_CASE(
    "the vehicle editor makes no mod of a folder whose name is another mod's id",
    "[editor_vehicle]") {
    const auto guard = screen_guard{};
    const auto folder = std::filesystem::path(PATH_INFO::user_moddir()) / "bn";
    std::filesystem::remove_all(folder);
    const auto cleanup = on_out_of_scope([&] { std::filesystem::remove_all(folder); });
    auto step = 0;
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto screen = ec::top_screen(captured());
        switch (step++) {
            case 0:
                return invoke("vehicle:new", {.text = "bn/kart.json"});
            case 1:
                // bn is the game's own mod: a mod of that id would stand in for it.
                CHECK(notice_of(screen).contains("bn_2"));
                CHECK_FALSE(std::filesystem::exists(folder / "kart.json"));
                CHECK_FALSE(std::filesystem::exists(folder / "modinfo.json"));
                return act("QUIT");
            default:
                FAIL("the vehicle editor outlived its exit");
                return act("QUIT");
        }
    });
    editor::run_vehicle_editor();
    CHECK(step == 2);
}

TEST_CASE(
    "the vehicle editor folds its list of vehicles while a design or a world vehicle is open",
    "[editor_vehicle]") {
    const auto guard = screen_guard{};
    clear_map();
    const auto cleanup_vehicles = on_out_of_scope([] { clear_vehicles(); });
    REQUIRE(editor::spawn_design(
        vproto_id("bicycle"), get_avatar().bub_pos() + tripoint_rel_ms(8, 0, 0), {}));
    const auto folded = [](const ec::screen_view& screen) {
        return part(screen, "vehicle:sources").collapsed;
    };
    auto step = 0;
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto screen = ec::top_screen(captured());
        switch (step++) {
            case 0:
                // Nothing is open: the vehicles to open are the page.
                CHECK_FALSE(folded(screen));
                return invoke("vehicle:open", {.row = "prototype:bicycle"});
            case 1:
                CHECK(folded(screen));
                return invoke("vehicle:source", {.row = "source:world"});
            case 2:
                return invoke("vehicle:edit", {.row = "world:0"});
            case 3:
                CHECK(folded(screen));
                return act("QUIT");
            default:
                FAIL("the vehicle editor outlived its exit");
                return act("QUIT");
        }
    });
    editor::run_vehicle_editor();
    CHECK(step == 4);
}

TEST_CASE(
    "the vehicle editor says what an install or removal did and how to fix one it refuses",
    "[editor_vehicle]") {
    const auto guard = screen_guard{};
    const auto status = [](const ec::screen_view& screen) {
        const auto* const found = ec::find_component(*screen.root, "vehicle:status");
        if (found == nullptr) { return std::string{}; }
        CHECK(body<ec::text_body>(*found).tone == "good");
        return body<ec::text_body>(*found).markup;
    };
    auto step = 0;
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto screen = ec::top_screen(captured());
        switch (step++) {
            case 0:
                return invoke("vehicle:open", {.row = "prototype:bicycle"});
            case 1:
                // The names of the parts on a square, bottom up, for a client to show on hover.
                CHECK(cell(screen, "mount:1,0").label.starts_with("extra light frame, "));
                CHECK(cell(screen, "mount:2,0").label.empty());
                return invoke("vehicle:choose", {.row = "vpart:headlight"});
            case 2:
                return invoke("vehicle:mount", {.row = "mount:2,0"});
            case 3:
                CHECK(notice_of(screen).ends_with("install a frame here first."));
                CHECK(status(screen).empty());
                return invoke("vehicle:mount", {.row = "mount:1,0"});
            case 4:
                // The refusal gives way to what the install did.
                CHECK(notice_of(screen).empty());
                CHECK(status(screen) == "Installed headlight at 1,0.");
                return invoke("vehicle:tool_remove");
            case 5:
                return invoke("vehicle:mount", {.row = "mount:1,0"});
            case 6:
                CHECK(status(screen) == "Removed headlight at 1,0.");
                return invoke("vehicle:part_filter", {.text = "wheel"});
            case 7:
                return invoke("vehicle:choose", {.row = "vpart:wheel"});
            case 8:
                return invoke("vehicle:mount", {.row = "mount:0,0"});
            case 9:
                // A part another must hold up brings that part, and says so.
                CHECK(notice_of(screen).empty());
                CHECK(status(screen).starts_with("Installed wheel hub assembly"));
                CHECK(status(screen).ends_with(" and wheel at 0,0."));
                return act("QUIT");
            default:
                return answer("Leave without saving");
        }
    });
    editor::run_vehicle_editor();
    CHECK(step >= 10);
}

TEST_CASE("the vehicle editor says where it saved the design", "[editor_vehicle]") {
    const auto guard = screen_guard{};
    const auto folder = std::filesystem::path(PATH_INFO::user_moddir()) / "vehicle_editor_saved";
    std::filesystem::remove_all(folder);
    const auto cleanup = on_out_of_scope([&] { std::filesystem::remove_all(folder); });
    const auto status = [](const ec::screen_view& screen) {
        return body<ec::text_body>(part(screen, "vehicle:status")).markup;
    };
    auto step = 0;
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto screen = ec::top_screen(captured());
        switch (step++) {
            case 0:
                return invoke("vehicle:open", {.row = "prototype:bicycle"});
            case 1:
                return invoke("vehicle:save_as", {.text = "vehicle_editor_saved/bike"});
            case 2:
                CHECK(status(screen) == "Saved vehicle_editor_saved/bike.json.");
                return invoke("vehicle:show:vehicle");
            case 3:
                return invoke("vehicle:prop_set", {.row = "name", .text = "Bike"});
            case 4:
                return invoke("vehicle:save");
            case 5:
                CHECK(status(screen) == "Saved vehicle_editor_saved/bike.json.");
                return act("QUIT");
            default:
                FAIL("the vehicle editor outlived its exit");
                return act("QUIT");
        }
    });
    editor::run_vehicle_editor();
    CHECK(step == 6);
}

TEST_CASE(
    "the vehicle editor's palette shows one row per part, and a filter lists every shape by a "
    "name of its own",
    "[editor_vehicle]") {
    const auto guard = screen_guard{};
    const auto ids = [](const ec::screen_view& screen) {
        return rows(screen, "vehicle:parts") | std::views::transform(&ec::row::id)
             | std::ranges::to<std::vector>();
    };
    auto step = 0;
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto screen = ec::top_screen(captured());
        switch (step++) {
            case 0:
                return invoke("vehicle:open", {.row = "prototype:bicycle"});
            case 1: {
                const auto& list = body<ec::list_body>(part(screen, "vehicle:parts"));
                REQUIRE_FALSE(list.rows.empty());
                // The shapes of a frame are one row, the shape the game's vehicles use most.
                CHECK(std::ranges::contains(ids(screen), std::string("vpart:frame_vertical")));
                CHECK_FALSE(std::ranges::contains(ids(screen), std::string("vpart:frame_cross")));
                CHECK_FALSE(
                    std::ranges::contains(ids(screen), std::string("vpart:frame_horizontal")));
                // No two rows read the same.
                auto labels =
                    list.rows | std::views::transform(&ec::row::label)
                    | std::ranges::to<std::vector>();
                std::ranges::sort(labels);
                CHECK(std::ranges::adjacent_find(labels) == labels.end());
                return invoke("vehicle:part_filter", {.text = "frame_"});
            }
            case 2: {
                // A filter lists every part that matches, each shape by a name of its own.
                CHECK(row(screen, "vehicle:parts", "vpart:frame_horizontal").label
                      == "frame (horizontal)");
                CHECK(row(screen, "vehicle:parts", "vpart:frame_horizontal_2").label
                      == "frame (horizontal_2)");
                CHECK(has_affordance(screen, "vehicle:part_page"));
                return invoke("vehicle:mount", {.row = "mount:1,0"});
            }
            case 3:
                return invoke("vehicle:pick", {.row = rows(screen, "vehicle:stack").front().id});
            case 4:
                return invoke("vehicle:show:part");
            case 5: {
                // The picked frame's shape is a setting of its own.
                const auto& fields = body<ec::form_body>(part(screen, "vehicle:part_form")).fields;
                const auto shape = std::ranges::find(fields, "shape", &ec::form_field::id);
                REQUIRE(shape != fields.end());
                CHECK(shape->type == "select");
                CHECK(shape->value == "xlframe_vertical");
                const auto horizontal =
                    std::ranges::find(shape->options, "xlframe_horizontal", &ec::form_option::id);
                REQUIRE(horizontal != shape->options.end());
                CHECK(horizontal->label == "extra light frame (horizontal)");
                return invoke("vehicle:part_set", {.row = "shape", .text = "xlframe_horizontal"});
            }
            case 6: {
                CHECK(notice_of(screen).empty());
                const auto& fields = body<ec::form_body>(part(screen, "vehicle:part_form")).fields;
                const auto shape = std::ranges::find(fields, "shape", &ec::form_field::id);
                REQUIRE(shape != fields.end());
                CHECK(shape->value == "xlframe_horizontal");
                // A shape of another part is none of this one's.
                return invoke("vehicle:part_set", {.row = "shape", .text = "frame_vertical"});
            }
            case 7:
                CHECK_FALSE(notice_of(screen).empty());
                return act("QUIT");
            default:
                return answer("Leave without saving");
        }
    });
    editor::run_vehicle_editor();
    CHECK(step >= 8);
}

TEST_CASE(
    "the vehicle editor shows the picked part's settings under the mount's parts, the usual "
    "fuels first",
    "[editor_vehicle]") {
    const auto guard = screen_guard{};
    auto step = 0;
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto screen = ec::top_screen(captured());
        switch (step++) {
            case 0:
                return invoke("vehicle:open", {.row = "prototype:bicycle"});
            case 1:
                return invoke("vehicle:choose", {.row = "vpart:tank_small"});
            case 2:
                return invoke("vehicle:mount", {.row = "mount:-1,0"});
            case 3: {
                // The tank just installed is picked, its fuel right under the mount's parts.
                CHECK(body<ec::tabs_body>(part(screen, "vehicle:tools")).tabs.front().selected);
                CHECK(rows(screen, "vehicle:stack").back().selected);
                const auto& fields = body<ec::form_body>(part(screen, "vehicle:part_form")).fields;
                const auto fuel = std::ranges::find(fields, "fuel", &ec::form_field::id);
                REQUIRE(fuel != fields.end());
                const auto ids =
                    fuel->options | std::views::transform(&ec::form_option::id)
                    | std::ranges::to<std::vector>();
                REQUIRE(ids.size() > 20);
                CHECK(ids.front().empty());
                const auto place = [&](const std::string& id) {
                    return std::ranges::find(ids, id) - ids.begin();
                };
                // The fuels the game's vehicles fill tanks with lead, then the rest.
                CHECK(place("gasoline") < 8);
                CHECK(place("diesel") < 8);
                CHECK(place("water_clean") < static_cast<std::ptrdiff_t>(ids.size()));
                CHECK(std::ranges::all_of(fuel->options, [](const ec::form_option& option) {
                    return !option.label.empty();
                }));
                CHECK(has_affordance(screen, "vehicle:part_set"));
                return invoke("vehicle:part_set", {.row = "fuel", .text = "diesel"});
            }
            case 4: {
                const auto& fields = body<ec::form_body>(part(screen, "vehicle:part_form")).fields;
                const auto fuel = std::ranges::find(fields, "fuel", &ec::form_field::id);
                REQUIRE(fuel != fields.end());
                CHECK(fuel->value == "diesel");
                return act("QUIT");
            }
            default:
                return answer("Leave without saving");
        }
    });
    editor::run_vehicle_editor();
    CHECK(step >= 5);
}

TEST_CASE(
    "the vehicle editor starts a new vehicle by its name with a frame and the frame in hand",
    "[editor_vehicle]") {
    const auto guard = screen_guard{};
    const auto folder = std::filesystem::path(PATH_INFO::user_moddir()) / "my_mod";
    std::filesystem::remove_all(folder);
    const auto cleanup = on_out_of_scope([&] { std::filesystem::remove_all(folder); });
    const auto status = [](const ec::screen_view& screen) {
        const auto* const found = ec::find_component(*screen.root, "vehicle:status");
        return found ? body<ec::text_body>(*found).markup : std::string{};
    };
    auto step = 0;
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto screen = ec::top_screen(captured());
        switch (step++) {
            case 0: {
                // New asks for a name, not a file.
                const auto& text = affordance(screen, "vehicle:new").args.text;
                REQUIRE(text.has_value());
                CHECK(text->initial == "My vehicle");
                return invoke("vehicle:new", {.text = "Go kart"});
            }
            case 1:
                CHECK(body<ec::panel_body>(part(screen, "vehicle:document")).title == "Go kart");
                CHECK(stat(screen, "file").value.ends_with("my_mod/go_kart.json"));
                // A frame to build on, the install tool and a frame in hand, and what to do.
                CHECK(rows(screen, "vehicle:stack").size() == 1);
                CHECK(cell(screen, "mount:0,0").label == "frame");
                CHECK(picked_tool(screen) == "canvas_tool:install");
                CHECK(row(screen, "vehicle:parts", "vpart:frame_vertical").selected);
                CHECK(status(screen)
                      == "Pick a part under Parts, then click a square next to "
                         "the frame.");
                return invoke("vehicle:mount", {.row = "mount:1,0"});
            case 2:
                CHECK(status(screen) == "Installed frame at 1,0.");
                return invoke("vehicle:save");
            case 3:
                // A name the game's own vehicle has gets an id of its own.
                return invoke("vehicle:new", {.text = "Bicycle"});
            case 4:
                CHECK(stat(screen, "file").value.ends_with("my_mod/bicycle_2.json"));
                return invoke("vehicle:save");
            case 5:
                // A slash in a name is part of the name.
                return invoke("vehicle:new", {.text = "AC/DC van"});
            case 6:
                CHECK(stat(screen, "file").value.ends_with("my_mod/ac_dc_van.json"));
                return invoke("vehicle:save");
            case 7:
                // A file named outright gives an id the game reads too.
                return invoke("vehicle:new", {.text = "my_mod/Fast Car.json"});
            case 8:
                CHECK(stat(screen, "file").value.ends_with("my_mod/Fast Car.json"));
                return invoke("vehicle:save");
            case 9:
                return act("QUIT");
            default:
                FAIL("the vehicle editor outlived its exit");
                return act("QUIT");
        }
    });
    editor::run_vehicle_editor();
    CHECK(step == 10);
    const auto saved = nlohmann::ordered_json::parse(read_file(folder / "go_kart.json"));
    REQUIRE(saved.size() == 1);
    CHECK(saved[0]["id"] == "go_kart");
    CHECK(saved[0]["name"] == "Go kart");
    CHECK(editor::design_parts(saved[0]).size() == 2);
    const auto second = nlohmann::ordered_json::parse(read_file(folder / "bicycle_2.json"));
    CHECK(second[0]["id"] == "bicycle_2");
    CHECK(second[0]["name"] == "Bicycle");
    const auto van = nlohmann::ordered_json::parse(read_file(folder / "ac_dc_van.json"));
    CHECK(van[0]["id"] == "ac_dc_van");
    CHECK(van[0]["name"] == "AC/DC van");
    const auto car = nlohmann::ordered_json::parse(read_file(folder / "Fast Car.json"));
    CHECK(car[0]["id"] == "fast_car");
}

TEST_CASE(
    "the vehicle editor gives a new design an id no prototype has, from a file name or a name "
    "with no letters of its id",
    "[editor_vehicle]") {
    const auto guard = screen_guard{};
    const auto folder = std::filesystem::path(PATH_INFO::user_moddir()) / "my_mod";
    std::filesystem::remove_all(folder);
    const auto cleanup = on_out_of_scope([&] { std::filesystem::remove_all(folder); });
    const auto notice = [](const ec::screen_view& screen) {
        const auto* const found = ec::find_component(*screen.root, "vehicle:notice");
        return found ? body<ec::text_body>(*found).markup : std::string{};
    };
    auto step = 0;
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto screen = ec::top_screen(captured());
        switch (step++) {
            case 0:
                // A file named as the game's own prototype gets an id of its own all the same.
                return invoke("vehicle:new", {.text = "my_mod/Bicycle.json"});
            case 1:
                CHECK(stat(screen, "file").value.ends_with("my_mod/Bicycle.json"));
                return invoke("vehicle:save");
            case 2:
                // A name with none of an id's letters keeps its name.
                return invoke("vehicle:new", {.text = "내 차"});
            case 3:
                CHECK(stat(screen, "file").value.ends_with("my_mod/my_vehicle.json"));
                return invoke("vehicle:save");
            case 4:
                return invoke("vehicle:new", {.text = "★★"});
            case 5:
                CHECK(stat(screen, "file").value.ends_with("my_mod/my_vehicle_2.json"));
                return invoke("vehicle:save");
            case 6:
                return invoke("vehicle:new", {.text = "my_mod/차.json"});
            case 7:
                CHECK(stat(screen, "file").value.ends_with("my_mod/차.json"));
                return invoke("vehicle:save");
            case 8:
                // A file needs a name before its .json.
                return invoke("vehicle:new", {.text = "my_mod/.json"});
            case 9:
                CHECK(notice(screen) == "A new vehicle needs a name.");
                CHECK(stat(screen, "file").value.ends_with("my_mod/차.json"));
                return act("QUIT");
            default:
                FAIL("the vehicle editor outlived its exit");
                return act("QUIT");
        }
    });
    editor::run_vehicle_editor();
    CHECK(step == 10);
    const auto prototype = [&](const char* const file) {
        const auto read = nlohmann::ordered_json::parse(read_file(folder / file));
        REQUIRE(read.size() == 1);
        return read[0];
    };
    const auto bicycle = prototype("Bicycle.json");
    CHECK(bicycle["id"] == "bicycle_2");
    CHECK(bicycle["name"] == "Bicycle");
    const auto korean = prototype("my_vehicle.json");
    CHECK(korean["id"] == "my_vehicle");
    CHECK(korean["name"] == "내 차");
    const auto stars = prototype("my_vehicle_2.json");
    CHECK(stars["id"] == "my_vehicle_2");
    CHECK(stars["name"] == "★★");
    // A file of the same id elsewhere is no prototype the game has, but this folder's are.
    const auto car = prototype("차.json");
    CHECK(car["name"] == "차");
    CHECK(car["id"] != "my_vehicle");
    CHECK(car["id"] != "my_vehicle_2");
    CHECK(std::filesystem::exists(folder / ".json") == false);
}

TEST_CASE(
    "the vehicle editor's canvas removes the top part of a square on its secondary click",
    "[editor_vehicle][engine_client_component]") {
    const auto guard = screen_guard{};
    const auto status = [](const ec::screen_view& screen) {
        const auto* const found = ec::find_component(*screen.root, "vehicle:status");
        return found ? body<ec::text_body>(*found).markup : std::string{};
    };
    auto step = 0;
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto state = captured();
        const auto screen = ec::top_screen(state);
        switch (step++) {
            case 0:
                return invoke("vehicle:open", {.row = "prototype:bicycle"});
            case 1: {
                // The canvas names it for a client to bind its right click to.
                const auto wire = nlohmann::json::parse(ec::serialize_interaction_boundary(state));
                CHECK(wire.dump().contains(R"("secondary":"vehicle:clear")"));
                // A right drag erases as it goes.
                CHECK(wire.dump().contains(R"("secondary_drag":"trail")"));
                CHECK(affordance(screen, "vehicle:clear").args.row->component_id == "vehicle:grid");
                return invoke("vehicle:clear", {.row = "mount:1,0"});
            }
            case 2:
                // Whatever tool is in hand.
                CHECK(picked_tool(screen) == "canvas_tool:select");
                CHECK(cell(screen, "mount:1,0").selected);
                CHECK(rows(screen, "vehicle:stack").size() == 2);
                CHECK(status(screen).starts_with("Removed "));
                CHECK(status(screen).ends_with(" at 1,0."));
                return invoke("vehicle:clear", {.row = "mount:3,0"});
            case 3:
                CHECK(notice_of(screen) == "This mount has no part to remove.");
                return act("QUIT");
            default:
                return answer("Leave without saving");
        }
    });
    editor::run_vehicle_editor();
    CHECK(step >= 4);
}

TEST_CASE(
    "the vehicle editor's palette shows one category of parts at a time, the part in hand's first, "
    "and a filter searches them all",
    "[editor_vehicle]") {
    const auto guard = screen_guard{};
    const auto groups = [](const ec::screen_view& screen) -> const ec::tabs_body& {
        return body<ec::tabs_body>(part(screen, "vehicle:part_groups"));
    };
    /// The tab of the category labelled `label`.
    const auto group = [&](const ec::screen_view& screen, const std::string& label) {
        const auto& tabs = groups(screen).tabs;
        const auto found = std::ranges::find(tabs, label, &ec::tab_entry::label);
        INFO(label);
        REQUIRE(found != tabs.end());
        return *found;
    };
    const auto shown_group = [&](const ec::screen_view& screen) {
        const auto& tabs = groups(screen).tabs;
        const auto found = std::ranges::find(tabs, true, &ec::tab_entry::selected);
        REQUIRE(found != tabs.end());
        return found->label;
    };
    const auto ids = [](const ec::screen_view& screen) {
        return rows(screen, "vehicle:parts") | std::views::transform(&ec::row::id)
             | std::ranges::to<std::vector>();
    };
    const auto shows = [&](const ec::screen_view& screen, const std::string& id) {
        return std::ranges::contains(ids(screen), id);
    };
    auto step = 0;
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto screen = ec::top_screen(captured());
        switch (step++) {
            case 0:
                return invoke("vehicle:open", {.row = "prototype:bicycle"});
            case 1: {
                // The categories are tabs of the palette, which shows the parts of one.
                CHECK(slot_of(screen, "vehicle:part_groups") == "palette");
                CHECK(part(screen, "vehicle:part_groups").label == "Parts");
                CHECK(groups(screen).select == "vehicle:part_group");
                CHECK(
                    (groups(screen).tabs | std::views::transform(&ec::tab_entry::label)
                     | std::ranges::to<std::vector>())
                    == std::vector<std::string>{
                        "Frames", "Wheels & hubs", "Engines & motors", "Fuel & batteries",
                        "Seats & controls", "Doors & windows", "Lights", "Storage", "Weapons",
                        "Other"});
                CHECK(shown_group(screen) == "Frames");
                CHECK(std::ranges::none_of(rows(screen, "vehicle:parts"), &ec::row::header));
                CHECK(shows(screen, "vpart:frame_vertical"));
                CHECK_FALSE(shows(screen, "vpart:headlight"));
                // A page at a time, however many parts a category has.
                const auto& list = body<ec::list_body>(part(screen, "vehicle:parts"));
                CHECK(list.rows.size() <= 50);
                CHECK(list.total >= list.rows.size());
                return invoke("vehicle:part_group", {.row = group(screen, "Lights").id});
            }
            case 2:
                CHECK(shown_group(screen) == "Lights");
                CHECK(shows(screen, "vpart:headlight"));
                CHECK_FALSE(shows(screen, "vpart:frame_vertical"));
                return invoke("vehicle:part_filter", {.text = "wheel_mount_medium"});
            case 3:
                // A filter searches every category.
                CHECK(shows(screen, "vpart:wheel_mount_medium"));
                return invoke("vehicle:choose", {.row = "vpart:wheel_mount_medium"});
            case 4:
                return invoke("vehicle:part_filter", {.text = ""});
            case 5:
                // The part in hand's category shows.
                CHECK(shown_group(screen) == "Wheels & hubs");
                CHECK(row(screen, "vehicle:parts", "vpart:wheel_mount_medium").selected);
                return invoke("vehicle:part_group", {.row = group(screen, "Storage").id});
            case 6:
                return act("QUIT");
            default:
                return answer("Leave without saving");
        }
    });
    editor::run_vehicle_editor();
    CHECK(step >= 7);

    // The editor keeps the category shown.
    step = 0;
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto screen = ec::top_screen(captured());
        switch (step++) {
            case 0:
                return invoke("vehicle:open", {.row = "prototype:bicycle"});
            case 1:
                CHECK(shown_group(screen) == "Storage");
                return act("QUIT");
            default:
                return answer("Leave without saving");
        }
    });
    editor::run_vehicle_editor();
    CHECK(step >= 2);
}

TEST_CASE(
    "the vehicle editor's canvas marks the squares the part in hand cannot go on, which still "
    "take a click",
    "[editor_vehicle][engine_client_component]") {
    const auto guard = screen_guard{};
    /// Why the cell of `id` refuses the part in hand, as a client reads it at that cell's square.
    const auto refused = [&](const ec::screen_view& screen, const std::string& id) {
        const auto grid = wire_grid();
        const auto entry = wire_square(grid, cell(screen, id).at.x, cell(screen, id).at.y);
        INFO(id);
        CHECK(entry.value("enabled", false));
        return entry.contains("refused")
                 ? grid.at("refusals").at(entry["refused"].get<int>()).get<std::string>()
                 : std::string{};
    };
    const auto none_refused = [] {
        const auto grid = wire_grid();
        return !grid.contains("refusals");
    };
    auto step = 0;
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto state = captured();
        const auto screen = ec::top_screen(state);
        switch (step++) {
            case 0:
                return invoke("vehicle:open", {.row = "prototype:bicycle"});
            case 1:
                // Nothing in hand refuses no square.
                CHECK_FALSE(wire_grid().at("cells").empty());
                CHECK(none_refused());
                return invoke("vehicle:part_filter", {.text = "headlight"});
            case 2:
                return invoke("vehicle:choose", {.row = "vpart:headlight"});
            case 3:
                // East of the rear wheel nothing holds a headlight up, and the square says so.
                CHECK(refused(screen, "mount:2,0").contains("frame"));
                // Far from the frame, where a frame would not hold either, it says so.
                CHECK(refused(screen, "mount:-5,0").contains("touch the vehicle's frame"));
                // Most squares refuse it for that, so the grid's blank cell says it.
                CHECK(
                    wire_grid()
                        .at("styles")
                        .at(wire_grid().at("blank").get<int>())
                        .contains("refused"));
                // A frame takes it.
                CHECK_FALSE(none_refused());
                CHECK(
                    std::ranges::any_of(wire_grid().at("styles"), [](const nlohmann::json& entry) {
                        return !entry.contains("refused");
                    }));
                // The square still takes the click, which says why.
                return invoke("vehicle:mount", {.row = "mount:2,0"});
            case 4:
                CHECK(notice_of(screen).find("frame") != std::string::npos);
                CHECK(cell(screen, "mount:2,0").selected);
                return invoke("vehicle:canvas_tool", {.row = "canvas_tool:select"});
            case 5:
                // Only the install tool refuses squares.
                CHECK(none_refused());
                return act("QUIT");
            default:
                return answer("Leave without saving");
        }
    });
    editor::run_vehicle_editor();
    CHECK(step >= 6);
}

TEST_CASE(
    "the vehicle editor's palette shows the first page of a category it turns to by itself",
    "[editor_vehicle]") {
    const auto guard = screen_guard{};
    const auto folder = std::filesystem::path(PATH_INFO::user_moddir()) / "my_mod";
    std::filesystem::remove_all(folder);
    const auto cleanup = on_out_of_scope([&] { std::filesystem::remove_all(folder); });
    const auto list = [](const ec::screen_view& screen) -> const ec::list_body& {
        return body<ec::list_body>(part(screen, "vehicle:parts"));
    };
    /// The tab of the category with the most parts but frames, the first.
    auto biggest = std::string{};
    auto most = std::size_t{0};
    auto tabs = std::vector<std::string>{};
    auto step = 0;
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto screen = ec::top_screen(captured());
        if (step == 0) {
            ++step;
            return invoke("vehicle:open", {.row = "prototype:bicycle"});
        }
        const auto at = step++ - 1;
        if (at == 0) {
            tabs = body<ec::tabs_body>(part(screen, "vehicle:part_groups")).tabs
                 | std::views::transform(&ec::tab_entry::id) | std::ranges::to<std::vector>();
        }
        // Visits every category to find the biggest.
        if (at > 1 && at <= static_cast<int>(tabs.size()) && list(screen).total > most) {
            most = list(screen).total;
            biggest = tabs[at - 1];
        }
        if (at < static_cast<int>(tabs.size())) {
            return invoke("vehicle:part_group", {.row = tabs[at]});
        }
        if (at == static_cast<int>(tabs.size())) {
            REQUIRE(most > 50);
            return invoke("vehicle:part_group", {.row = biggest});
        }
        if (at == static_cast<int>(tabs.size()) + 1) {
            // Its last page.
            return invoke(
                "vehicle:part_page", {.count = static_cast<int>((list(screen).total - 1) / 50)});
        }
        if (at == static_cast<int>(tabs.size()) + 2) {
            CHECK(list(screen).offset > 0);
            // A new vehicle puts a frame in hand, whose category shows from its first page.
            return invoke("vehicle:new", {.text = "Page van"});
        }
        if (at == static_cast<int>(tabs.size()) + 3) {
            CHECK(list(screen).offset == 0);
            CHECK_FALSE(list(screen).rows.empty());
            return act("QUIT");
        }
        return answer("Leave without saving");
    });
    editor::run_vehicle_editor();
    CHECK(step >= static_cast<int>(tabs.size()) + 5);
}

TEST_CASE(
    "the vehicle editor tells why the part in hand does not fit only when that is news, and asks "
    "for a new tank's fuel",
    "[editor_vehicle]") {
    const auto guard = screen_guard{};
    const auto folder = std::filesystem::path(PATH_INFO::user_moddir()) / "my_mod";
    std::filesystem::remove_all(folder);
    const auto cleanup = on_out_of_scope([&] { std::filesystem::remove_all(folder); });
    const auto status = [](const ec::screen_view& screen) {
        const auto* const found = ec::find_component(*screen.root, "vehicle:status");
        return found ? body<ec::text_body>(*found).markup : std::string{};
    };
    /// What the palette says of the part in hand at the picked mount.
    const auto fit = [](const ec::screen_view& screen) {
        const auto& rows = body<ec::list_body>(part(screen, "vehicle:parts")).rows;
        const auto held = std::ranges::find(rows, true, &ec::row::selected);
        REQUIRE(held != rows.end());
        return held->description;
    };
    auto step = 0;
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto screen = ec::top_screen(captured());
        switch (step++) {
            case 0:
                return invoke("vehicle:new", {.text = "Fit van"});
            case 1:
                // A single click on a part takes it in hand.
                CHECK(affordance(screen, "vehicle:choose").role == "select");
                return invoke("vehicle:mount", {.row = "mount:1,0"});
            case 2:
                // After an install, the frame in hand fits nowhere it just went: no news.
                CHECK(status(screen) == "Installed frame at 1,0.");
                CHECK(fit(screen).empty());
                return invoke("vehicle:mount", {.row = "mount:1,0"});
            case 3:
                // A refused click says why once, as the status line's news.
                CHECK_FALSE(notice_of(screen).empty());
                CHECK(fit(screen).empty());
                return invoke("vehicle:canvas_tool", {.row = "canvas_tool:select"});
            case 4:
                // Looking around, the palette says nothing of the part in hand already there.
                CHECK(fit(screen).empty());
                return invoke("vehicle:mount", {.row = "mount:5,5"});
            case 5:
                // Nor does it hide why the part in hand would not fit elsewhere.
                CHECK(fit(screen).contains("touch the vehicle's frame"));
                return invoke("vehicle:mount", {.row = "mount:1,0"});
            case 6:
                return invoke("vehicle:part_filter", {.text = "tank_little"});
            case 7:
                return invoke("vehicle:choose", {.row = "vpart:tank_little"});
            case 8:
                return invoke("vehicle:mount", {.row = "mount:1,0"});
            case 9:
                // A tank comes empty: the status says where its fuel is set.
                CHECK(status(screen).starts_with("Installed "));
                CHECK(status(screen).ends_with(" Set its fuel under Fuel."));
                return invoke("vehicle:canvas_tool", {.row = "canvas_tool:select"});
            case 10:
                // Moving on, the palette does not call the new tank in the way of itself.
                CHECK(status(screen).empty());
                CHECK(fit(screen).empty());
                return invoke("vehicle:part_filter", {.text = "headlight"});
            case 11:
                return invoke("vehicle:choose", {.row = "vpart:headlight"});
            case 12:
                return invoke("vehicle:mount", {.row = "mount:1,0"});
            case 13:
                CHECK(status(screen) == "Installed headlight at 1,0.");
                return act("QUIT");
            default:
                return answer("Leave without saving");
        }
    });
    editor::run_vehicle_editor();
    CHECK(step >= 14);
}

namespace {
/// What the player does next, from what the screen shows.
using walk_step = std::function<auto(const ec::screen_view&)->input_event>;
/// Runs the vehicle editor through `steps`, then leaves it without saving; how many steps ran.
auto walk(const std::vector<walk_step>& steps) -> std::size_t {
    auto step = std::size_t{0};
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto screen = ec::top_screen(captured());
        if (step < steps.size()) { return steps[step++](screen); }
        ++step;
        // Unsaved changes ask first.
        const auto asked = game_client::current_interaction({.limit = 200}).choices;
        return std::ranges::contains(asked, std::string("Leave without saving"),
                                     &game_client::interaction_choice::label)
                 ? answer("Leave without saving")
                 : act("QUIT");
    });
    editor::run_vehicle_editor();
    return std::min(step, steps.size());
}
auto mount_at(const int x, const int y) -> input_event {
    return invoke("vehicle:mount", {.row = string_format("mount:%d,%d", x, y)});
}
auto said(const ec::screen_view& screen) -> std::string {
    const auto* const found = ec::find_component(*screen.root, "vehicle:status");
    return found ? body<ec::text_body>(*found).markup : std::string{};
}
auto steps_done(const ec::screen_view& screen) -> std::size_t {
    return rows(screen, "vehicle:history").size();
}
/// The label of the step the history is at.
auto step_at(const ec::screen_view& screen) -> std::string {
    const auto& steps = rows(screen, "vehicle:history");
    const auto found = std::ranges::find(steps, true, &ec::row::selected);
    REQUIRE(found != steps.end());
    return found->label;
}
auto canvas(const ec::screen_view& screen) -> const ec::grid_body& {
    return body<ec::grid_body>(part(screen, "vehicle:grid"));
}
auto has_part(const ec::screen_view& screen, const int x, const int y) -> bool {
    return !cell(screen, string_format("mount:%d,%d", x, y)).label.empty();
}
/// The cell of mount `x`,`y` on the canvas, as the client places it.
auto shown_at(const ec::screen_view& screen, const int x, const int y) -> ec::grid_point {
    return cell(screen, string_format("mount:%d,%d", x, y)).at;
}
auto front(const ec::screen_view& screen) -> std::string { return stat(screen, "front").value; }
/// A new design in my_mod, removed again after the test.
struct my_mod_folder {
    std::filesystem::path path = std::filesystem::path(PATH_INFO::user_moddir()) / "my_mod";
    my_mod_folder() { std::filesystem::remove_all(path); }
    ~my_mod_folder() { std::filesystem::remove_all(path); }
    my_mod_folder(const my_mod_folder&) = delete;
    auto operator=(const my_mod_folder&) -> my_mod_folder& = delete;
};
} // namespace

TEST_CASE(
    "the vehicle editor draws the part in hand along a line, over a rectangle and into a fill, "
    "each one undo step",
    "[editor_vehicle]") {
    const auto guard = screen_guard{};
    const auto folder = my_mod_folder{};
    auto before = std::size_t{0};
    const auto steps = std::vector<walk_step>{
        [](const auto&) { return invoke("vehicle:new", {.text = "Shape van"}); },
        [](const auto& screen) {
            // The map editor's drawing tools, with its keys.
            const auto tools =
                rows(screen, "vehicle:canvas_tools")
                | std::views::transform([](const ec::row& entry) {
                      return entry.id + " " + entry.label + " " + entry.hotkey;
                  })
                | std::ranges::to<std::vector>();
            CHECK(
                tools
                == std::vector<std::string>{
                    "canvas_tool:select Select s", "canvas_tool:install Pencil b",
                    "canvas_tool:line Line l", "canvas_tool:rect Rectangle r",
                    "canvas_tool:rect_filled Filled rectangle u", "canvas_tool:fill Fill f",
                    "canvas_tool:remove Eraser e", "canvas_tool:pick Eyedropper i",
                    "canvas_tool:paste Paste v", "canvas_tool:mirror_sides Mirror left and right ",
                    "canvas_tool:mirror_ends Mirror front and back "});
            CHECK(
                affordance(screen, "vehicle:tool_pick").keys == std::vector<std::string>{"i", "q"});
            // The pencil draws a trail, one step from press to release.
            CHECK(canvas(screen).drag == "trail");
            CHECK(canvas(screen).drag_begin == "vehicle:stroke_begin");
            CHECK(canvas(screen).drag_end == "vehicle:stroke_end");
            // A secondary drag erases as it goes.
            CHECK(canvas(screen).secondary == "vehicle:clear");
            CHECK(canvas(screen).secondary_drag == "trail");
            return invoke("vehicle:tool_rect_filled");
        },
        [&](const auto& screen) {
            // A shape spans from the square pressed to the one released.
            CHECK(canvas(screen).drag == "span");
            before = steps_done(screen);
            return mount_at(0, 0);
        },
        [](const auto& screen) {
            CHECK(cell(screen, "mount:0,0").selected);
            return mount_at(4, 2);
        },
        [&](const auto& screen) {
            // The frame already at 0,0 is no refusal.
            CHECK(said(screen) == "Installed frame on 14 squares.");
            // The step names the part a stroke drew.
            CHECK(step_at(screen) == "Install frame ×14");
            CHECK(steps_done(screen) == before + 1);
            for (auto y = 0; y <= 2; ++y) {
                for (auto x = 0; x <= 4; ++x) { CHECK(has_part(screen, x, y)); }
            }
            return invoke("vehicle:undo");
        },
        [](const auto& screen) {
            CHECK_FALSE(has_part(screen, 4, 2));
            CHECK(has_part(screen, 0, 0));
            return invoke("vehicle:redo");
        },
        [](const auto&) { return invoke("vehicle:tool_line"); },
        [](const auto&) { return mount_at(5, 1); },
        [](const auto&) { return mount_at(8, 1); },
        [&](const auto& screen) {
            CHECK(said(screen) == "Installed frame on 4 squares.");
            CHECK(steps_done(screen) == before + 2);
            return invoke("vehicle:tool_rect");
        },
        [](const auto&) { return mount_at(0, 3); },
        [](const auto&) { return mount_at(4, 5); },
        [](const auto& screen) {
            CHECK(said(screen) == "Installed frame on 12 squares.");
            CHECK_FALSE(has_part(screen, 2, 4));
            return invoke("vehicle:tool_fill");
        },
        [](const auto&) { return mount_at(2, 4); },
        [&](const auto& screen) {
            // The empty squares the outline closes in.
            CHECK(said(screen) == "Installed frame on 3 squares.");
            CHECK(has_part(screen, 1, 4));
            CHECK(has_part(screen, 3, 4));
            CHECK(steps_done(screen) == before + 4);
            return invoke("vehicle:part_filter", {.text = "seat"});
        },
        [](const auto&) { return invoke("vehicle:choose", {.row = "vpart:seat"}); },
        [](const auto& screen) {
            // Taking a part keeps the drawing tool in hand.
            CHECK(picked_tool(screen) == "canvas_tool:fill");
            return mount_at(0, 0);
        },
        [&](const auto& screen) {
            // Every square joined to it whose top part is a frame.
            CHECK(said(screen) == "Installed seat on 34 squares.");
            CHECK(steps_done(screen) == before + 5);
            return invoke("vehicle:tool_pick");
        },
        [](const auto&) { return mount_at(8, 1); },
        [](const auto& screen) {
            // The eyedropper takes the top part in hand, and the pencil to draw it.
            CHECK(picked_tool(screen) == "canvas_tool:install");
            const auto& palette = rows(screen, "vehicle:parts");
            const auto held = std::ranges::find(palette, true, &ec::row::selected);
            REQUIRE(held != palette.end());
            CHECK(held->label == "seat");
            return invoke("vehicle:tool_remove");
        },
        [&](const auto& screen) {
            CHECK(canvas(screen).drag == "trail");
            before = steps_done(screen);
            return invoke("vehicle:stroke_begin");
        },
        [](const auto&) { return mount_at(8, 1); },
        [](const auto&) { return mount_at(7, 1); },
        [](const auto&) { return invoke("vehicle:stroke_end"); },
        [&](const auto& screen) {
            // The eraser takes the top part, the seat, of each square it crosses.
            CHECK(cell(screen, "mount:8,1").label == "frame");
            CHECK(cell(screen, "mount:7,1").label == "frame");
            CHECK(steps_done(screen) == before + 1);
            return invoke("vehicle:stroke_begin");
        },
        [](const auto&) { return invoke("vehicle:clear", {.row = "mount:6,1"}); },
        [](const auto&) { return invoke("vehicle:clear", {.row = "mount:5,1"}); },
        [](const auto&) { return invoke("vehicle:stroke_end"); },
        [&](const auto& screen) {
            CHECK(cell(screen, "mount:6,1").label == "frame");
            CHECK(cell(screen, "mount:5,1").label == "frame");
            CHECK(steps_done(screen) == before + 2);
            return act("QUIT");
        },
    };
    CHECK(walk(steps) == steps.size());
}

TEST_CASE(
    "the vehicle editor mirrors what it draws across the vehicle's middle, side to side and end "
    "to end",
    "[editor_vehicle]") {
    const auto guard = screen_guard{};
    const auto folder = my_mod_folder{};
    const auto mirror = [](const ec::screen_view& screen, const char* const id) {
        const auto& entry = row(screen, "vehicle:canvas_tools", std::string("canvas_tool:") + id);
        REQUIRE(entry.columns.size() == 1);
        return entry.columns.front().value;
    };
    const auto steps = std::vector<walk_step>{
        [](const auto&) { return invoke("vehicle:new", {.text = "Mirror van"}); },
        [&](const auto& screen) {
            CHECK(mirror(screen, "mirror_sides") == "Off");
            CHECK(mirror(screen, "mirror_ends") == "Off");
            return invoke("vehicle:mirror_sides");
        },
        [&](const auto& screen) {
            CHECK(mirror(screen, "mirror_sides") == "On");
            // The canvas tool still in hand.
            CHECK(picked_tool(screen) == "canvas_tool:install");
            return mount_at(0, 1);
        },
        [](const auto& screen) {
            CHECK(said(screen) == "Installed frame on 2 squares.");
            CHECK(has_part(screen, 0, -1));
            return invoke("vehicle:canvas_tool", {.row = "canvas_tool:mirror_ends"});
        },
        [&](const auto& screen) {
            CHECK(mirror(screen, "mirror_ends") == "On");
            return mount_at(1, 1);
        },
        [](const auto& screen) {
            CHECK(said(screen) == "Installed frame on 4 squares.");
            CHECK(has_part(screen, -1, 1));
            CHECK(has_part(screen, 1, -1));
            CHECK(has_part(screen, -1, -1));
            return invoke("vehicle:tool_remove");
        },
        [](const auto&) { return mount_at(1, 1); },
        [](const auto& screen) {
            CHECK_FALSE(has_part(screen, 1, 1));
            CHECK_FALSE(has_part(screen, -1, 1));
            CHECK_FALSE(has_part(screen, 1, -1));
            CHECK_FALSE(has_part(screen, -1, -1));
            CHECK(has_part(screen, 0, 1));
            return act("QUIT");
        },
    };
    CHECK(walk(steps) == steps.size());
}

TEST_CASE(
    "the vehicle editor turns its canvas a quarter at a time and still draws on the mounts "
    "clicked",
    "[editor_vehicle]") {
    const auto guard = screen_guard{};
    const auto folder = my_mod_folder{};
    auto turned = std::vector<ec::grid_cell>{};
    const auto places = [](const ec::screen_view& screen) {
        auto cells = canvas(screen).cells;
        std::ranges::sort(cells, {}, &ec::grid_cell::id);
        return cells | std::views::transform([](const ec::grid_cell& entry) {
                   return entry.id + string_format(" %d,%d", entry.at.x, entry.at.y);
               })
             | std::ranges::to<std::vector>();
    };
    auto kept = std::vector<std::string>{};
    auto size = ec::grid_point{};
    const auto steps = std::vector<walk_step>{
        [](const auto&) { return invoke("vehicle:new", {.text = "Turn van"}); },
        [&](const auto& screen) {
            CHECK(front(screen) == "Right");
            CHECK(affordance(screen, "vehicle:view_cw").keys == std::vector<std::string>{"z"});
            CHECK(affordance(screen, "vehicle:view_ccw").keys == std::vector<std::string>{"Z"});
            // The spawn facing turns on its own keys, clear of the Rectangle's r.
            CHECK(affordance(screen, "vehicle:turn_cw").keys == std::vector<std::string>{"t"});
            CHECK(affordance(screen, "vehicle:turn_ccw").keys == std::vector<std::string>{"T"});
            size = {canvas(screen).cols, canvas(screen).rows};
            return invoke("vehicle:view_cw");
        },
        [&](const auto& screen) {
            CHECK(front(screen) == "Down");
            CHECK(canvas(screen).cols == size.y);
            CHECK(canvas(screen).rows == size.x);
            // The front, east of the middle, shows below it.
            const auto middle = shown_at(screen, 0, 0);
            CHECK(shown_at(screen, 1, 0).x == middle.x);
            CHECK(shown_at(screen, 1, 0).y == middle.y + 1);
            CHECK(shown_at(screen, 0, 1).x == middle.x - 1);
            return invoke("vehicle:tool_rect_filled");
        },
        [](const auto&) { return mount_at(1, 0); },
        [](const auto&) { return mount_at(2, 1); },
        [&](const auto& screen) {
            CHECK(said(screen) == "Installed frame on 4 squares.");
            CHECK(has_part(screen, 2, 1));
            CHECK_FALSE(has_part(screen, 2, 2));
            CHECK_FALSE(has_part(screen, -1, 1));
            kept = places(screen);
            return invoke("vehicle:view_cw");
        },
        [](const auto& screen) {
            CHECK(front(screen) == "Left");
            return invoke("vehicle:view_cw");
        },
        [](const auto& screen) {
            CHECK(front(screen) == "Up");
            return invoke("vehicle:view_cw");
        },
        [](const auto& screen) {
            CHECK(front(screen) == "Right");
            return invoke("vehicle:view_cw");
        },
        [&](const auto& screen) {
            // Round the four quarters, everything is where it was.
            CHECK(places(screen) == kept);
            return invoke("vehicle:view_ccw");
        },
        [](const auto& screen) {
            CHECK(front(screen) == "Right");
            return act("QUIT");
        },
    };
    CHECK(walk(steps) == steps.size());
}

TEST_CASE(
    "the vehicle editor builds a 20 by 20 vehicle with wheels, seats, an engine and a tank in a "
    "few strokes",
    "[editor_vehicle]") {
    const auto guard = screen_guard{};
    const auto folder = my_mod_folder{};
    auto before = std::size_t{0};
    auto strokes = std::size_t{0};
    /// A stroke from the mount `from` to `to`, both on the canvas.
    const auto span = [&](const point& from, const point& to) {
        return std::vector<walk_step>{
            [=, &strokes](const auto& screen) {
                cell(screen, string_format("mount:%d,%d", to.x, to.y));
                ++strokes;
                return mount_at(from.x, from.y);
            },
            [=](const auto&) { return mount_at(to.x, to.y); }};
    };
    const auto take = [](const std::string& id) {
        return std::vector<walk_step>{
            [=](const auto&) { return invoke("vehicle:part_filter", {.text = id}); },
            [=](const auto&) { return invoke("vehicle:choose", {.row = "vpart:" + id}); }};
    };
    auto steps = std::vector<walk_step>{
        [](const auto&) { return invoke("vehicle:new", {.text = "Big van"}); },
        [&](const auto& screen) {
            before = steps_done(screen);
            return invoke("vehicle:tool_rect_filled");
        }};
    const auto add = [&](std::vector<walk_step> more) {
        std::ranges::move(more, std::back_inserter(steps));
    };
    // A new design has room for the whole frame, drawn in one stroke.
    add(span({-9, -9}, {10, 10}));
    add({[](const auto& screen) {
        CHECK(said(screen) == "Installed frame on 399 squares.");
        // Still twelve free mounts around it on every side.
        CHECK(shown_at(screen, -9, -9).x >= 12);
        CHECK(shown_at(screen, -9, -9).y >= 12);
        CHECK(canvas(screen).cols - 1 - shown_at(screen, 10, 10).x >= 12);
        CHECK(canvas(screen).rows - 1 - shown_at(screen, 10, 10).y >= 12);
        return invoke("vehicle:part_filter", {.text = ""});
    }});
    // A wheel mount and its wheel on each corner, drawn once on one of them.
    add(take("wheel_mount_medium"));
    add({[](const auto&) { return invoke("vehicle:tool_install"); },
         [](const auto&) { return invoke("vehicle:mirror_sides"); },
         [](const auto&) { return invoke("vehicle:mirror_ends"); },
         [&](const auto&) {
             ++strokes;
             return mount_at(-9, -9);
         }});
    add(take("wheel"));
    add({[&](const auto& screen) {
             CHECK(said(screen).empty());
             ++strokes;
             return mount_at(-9, -9);
         },
         [](const auto& screen) {
             CHECK(said(screen) == "Installed wheel on 4 squares.");
             return invoke("vehicle:mirror_sides");
         },
         [](const auto&) { return invoke("vehicle:mirror_ends"); }});
    add(take("seat"));
    add({[](const auto&) { return invoke("vehicle:tool_line"); }});
    add(span({-5, 0}, {5, 0}));
    add(take("engine_1cyl"));
    add({[](const auto&) { return invoke("vehicle:tool_install"); },
         [&](const auto&) {
             ++strokes;
             return mount_at(8, 0);
         }});
    add(take("tank_little"));
    add({[&](const auto&) {
             ++strokes;
             return mount_at(7, 0);
         },
         [&](const auto& screen) {
             CHECK(notice_of(screen).empty());
             // One undo step a stroke.
             CHECK(steps_done(screen) == before + strokes);
             return invoke("vehicle:save");
         },
         [](const auto&) { return act("QUIT"); }});
    CHECK(walk(steps) == steps.size());
    CHECK(strokes == 6);
    const auto saved = nlohmann::ordered_json::parse(read_file(folder.path / "big_van.json"));
    REQUIRE(saved.size() == 1);
    const auto parts = editor::design_parts(saved[0]);
    const auto count = [&](const std::string& id) {
        return std::ranges::count(parts, id, &editor::design_part::part);
    };
    CHECK(parts.size() == 400 + 4 + 4 + 11 + 1 + 1);
    CHECK(count("wheel_mount_medium") == 4);
    CHECK(count("wheel") == 4);
    CHECK(count("seat") == 11);
    CHECK(count("engine_1cyl") == 1);
    CHECK(count("tank_little") == 1);
    const auto mounts =
        parts | std::views::transform(&editor::design_part::mount) | std::ranges::to<std::set>();
    CHECK(mounts.size() == 400);
    CHECK(mounts.contains(point(-9, -9)));
    CHECK(mounts.contains(point(10, 10)));
}

namespace {
/// A new design drawn as a 60 by 60 frame in filled rectangles, the canvas growing between them,
/// each leaving the size of the boundary in `bytes`.
auto frame_60(std::size_t& bytes) -> std::vector<walk_step> {
    auto steps = std::vector<walk_step>{
        [](const auto&) { return invoke("vehicle:new", {.text = "Land train"}); },
        [](const auto&) { return invoke("vehicle:tool_rect_filled"); }};
    for (const auto& [from, to] :
         {std::pair{point(-15, -15), point(15, 15)}, std::pair{point(-27, -27), point(27, 27)},
          std::pair{point(-29, -29), point(30, 30)}}) {
        steps.push_back([=](const auto&) { return mount_at(from.x, from.y); });
        steps.push_back([=](const auto&) { return mount_at(to.x, to.y); });
        steps.push_back([&bytes](const auto& screen) {
            CHECK(said(screen).starts_with("Installed frame on "));
            bytes = ec::serialize_interaction_boundary(captured()).size();
            CHECK(bytes < ec::maximum_inline_bytes);
            return invoke("vehicle:tool_rect_filled");
        });
    }
    return steps;
}
} // namespace

TEST_CASE(
    "the vehicle editor sends the canvas of a 60 by 60 frame design within the inline limit",
    "[editor_vehicle][engine_client_component]") {
    const auto guard = screen_guard{};
    const auto folder = my_mod_folder{};
    auto bytes = std::size_t{0};
    auto steps = frame_60(bytes);
    steps.push_back([](const auto& screen) {
        CHECK(has_part(screen, -29, -29));
        CHECK(has_part(screen, 30, 30));
        CHECK(canvas(screen).cols == 84);
        CHECK(canvas(screen).rows == 84);
        // Each row of frames is one run, with the empty squares beside it where a frame can go.
        CHECK(wire_grid().at("cells").size() <= 3 * 62);
        return act("QUIT");
    });
    CHECK(walk(steps) == steps.size());
    CAPTURE(bytes);
    CHECK(bytes < 64 * 1024);
}

TEST_CASE(
    "the vehicle editor sends the canvas of a 60 by 60 design of different stacks side by side "
    "within the inline limit",
    "[editor_vehicle][engine_client_component]") {
    const auto guard = screen_guard{};
    const auto folder = my_mod_folder{};
    // A frame on each mount, and a seat, a headlight or both on top by turns, so that no two
    // squares side by side show the same.
    const auto stacks = std::array{
        nlohmann::json::array({"frame_cross"}), nlohmann::json::array({"frame_cross", "seat"}),
        nlohmann::json::array({"frame_cross", "headlight"}),
        nlohmann::json::array({"frame_cross", "seat", "headlight"})};
    auto parts = nlohmann::json::array();
    for (auto y = -29; y <= 30; ++y) {
        for (auto x = -29; x <= 30; ++x) {
            parts.push_back({{"x", x}, {"y", y}, {"parts", stacks[(x + y + 58) % 4]}});
        }
    }
    auto bytes = std::size_t{0};
    const auto steps = std::vector<walk_step>{
        [](const auto&) { return invoke("vehicle:new", {.text = "Parade float"}); },
        [](const auto&) { return invoke("vehicle:show:json"); },
        [](const auto&) {
            return invoke("vehicle:field_set", {.row = "pointer", .text = "/parts"});
        },
        [&](const auto&) {
            return invoke("vehicle:field_set", {.row = "value", .text = parts.dump()});
        },
        [](const auto&) { return invoke("vehicle:field_apply"); },
        // The field shows a short value again.
        [](const auto&) {
            return invoke("vehicle:field_set", {.row = "pointer", .text = "/name"});
        },
        [&](const auto& screen) {
            // The game builds it.
            CHECK(ec::find_component(*screen.root, "vehicle:problems") == nullptr);
            CHECK(canvas(screen).cols == 84);
            CHECK(cell(screen, "mount:0,0").label != cell(screen, "mount:1,0").label);
            CHECK(cell(screen, "mount:1,0").label != cell(screen, "mount:2,0").label);
            CHECK(cell(screen, "mount:2,0").label != cell(screen, "mount:3,0").label);
            bytes = ec::serialize_interaction_boundary(captured()).size();
            return act("QUIT");
        }};
    CHECK(walk(steps) == steps.size());
    CAPTURE(bytes);
    CHECK(bytes < 160 * 1024);
}

namespace {
/// Steps that set the design's parts to a frame on each of `mounts`, then put the field back on
/// its name.
auto frames_on(const std::vector<point>& mounts) -> std::vector<walk_step> {
    auto parts = nlohmann::json::array();
    for (const auto& at : mounts) {
        parts.push_back({{"x", at.x}, {"y", at.y}, {"parts", {"frame_cross"}}});
    }
    return {
        [](const auto&) { return invoke("vehicle:show:json"); },
        [](const auto&) {
            return invoke("vehicle:field_set", {.row = "pointer", .text = "/parts"});
        },
        [=](const auto&) {
            return invoke("vehicle:field_set", {.row = "value", .text = parts.dump()});
        },
        [](const auto&) { return invoke("vehicle:field_apply"); },
        [](const auto&) {
            return invoke("vehicle:field_set", {.row = "pointer", .text = "/name"});
        }};
}
/// The mounts from `low` to `high`, north to south and west to east, that `keep` keeps.
auto mounts_in(const point& low, const point& high, const std::function<bool(const point&)>& keep)
    -> std::vector<point> {
    auto mounts = std::vector<point>{};
    for (auto y = low.y; y <= high.y; ++y) {
        for (auto x = low.x; x <= high.x; ++x) {
            if (keep({x, y})) { mounts.emplace_back(x, y); }
        }
    }
    return mounts;
}
using steady = std::chrono::steady_clock;
} // namespace

TEST_CASE(
    "the vehicle editor draws a 60 square line of seats over a 60 by 60 frame at once",
    "[editor_vehicle]") {
    const auto guard = screen_guard{};
    const auto folder = my_mod_folder{};
    auto began = steady::time_point{};
    auto took = steady::duration{};
    auto steps = std::vector<walk_step>{[](const auto&) {
        return invoke("vehicle:new", {.text = "Bus"});
    }};
    std::ranges::move(frames_on(mounts_in({-29, -29}, {30, 30}, [](const point&) { return true; })),
                      std::back_inserter(steps));
    std::ranges::move(
        std::vector<walk_step>{
            [](const auto&) { return invoke("vehicle:part_filter", {.text = "seat"}); },
            [](const auto&) { return invoke("vehicle:choose", {.row = "vpart:seat"}); },
            [](const auto&) { return invoke("vehicle:tool_line"); },
            [&](const auto&) {
                began = steady::now();
                return mount_at(-29, 0);
            },
            [](const auto&) { return mount_at(30, 0); },
            [&](const auto& screen) {
                took = steady::now() - began;
                CHECK(said(screen) == "Installed seat on 60 squares.");
                return act("QUIT");
            }},
        std::back_inserter(steps));
    CHECK(walk(steps) == steps.size());
    const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(took).count();
    WARN("R10 line " << ms << " ms");
    CHECK(ms < 1000);
}

TEST_CASE("the vehicle editor fills a 60 by 60 rectangle of frames at once", "[editor_vehicle]") {
    const auto guard = screen_guard{};
    const auto folder = my_mod_folder{};
    auto began = steady::time_point{};
    auto took = steady::duration{};
    auto steps = std::vector<walk_step>{[](const auto&) {
        return invoke("vehicle:new", {.text = "Barge"});
    }};
    // A frame along the front and down the left, so the canvas has room for the rest.
    std::ranges::move(
        frames_on(mounts_in(
            {-29, -29}, {30, 30}, [](const point& at) { return at.x == -29 || at.y == -29; })),
        std::back_inserter(steps));
    std::ranges::move(
        std::vector<walk_step>{
            [](const auto&) { return invoke("vehicle:tool_rect_filled"); },
            [&](const auto&) {
                began = steady::now();
                return mount_at(-28, -28);
            },
            [](const auto&) { return mount_at(30, 30); },
            [&](const auto& screen) {
                took = steady::now() - began;
                CHECK(said(screen) == "Installed frame on 3481 squares.");
                return act("QUIT");
            }},
        std::back_inserter(steps));
    CHECK(walk(steps) == steps.size());
    const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(took).count();
    WARN("R10 rect " << ms << " ms");
    CHECK(ms < 2000);
}

TEST_CASE(
    "the vehicle editor erases a square of a 60 by 60 frame and deletes the rest at once",
    "[editor_vehicle]") {
    const auto guard = screen_guard{};
    const auto folder = my_mod_folder{};
    auto began = steady::time_point{};
    auto erased = steady::duration{};
    auto deleted = steady::duration{};
    auto steps = std::vector<walk_step>{[](const auto&) {
        return invoke("vehicle:new", {.text = "Raft"});
    }};
    std::ranges::move(frames_on(mounts_in({-29, -29}, {30, 30}, [](const point&) { return true; })),
                      std::back_inserter(steps));
    std::ranges::move(
        std::vector<walk_step>{
            [](const auto&) { return invoke("vehicle:tool_remove"); },
            [&](const auto&) {
                began = steady::now();
                return mount_at(-29, -29);
            },
            [&](const auto& screen) {
                erased = steady::now() - began;
                CHECK(said(screen) == "Removed frame at -29,-29.");
                return invoke("vehicle:tool_select");
            },
            [](const auto&) { return invoke("vehicle:stroke_begin"); },
            [](const auto&) { return mount_at(-29, -29); },
            [](const auto&) { return mount_at(30, 30); },
            [](const auto&) { return invoke("vehicle:stroke_end"); },
            [&](const auto&) {
                began = steady::now();
                return invoke("vehicle:delete_mounts");
            },
            [&](const auto& screen) {
                deleted = steady::now() - began;
                CHECK(said(screen)
                      == "Deleted 3598 parts. 1 part refused: A vehicle keeps its last part.");
                return act("QUIT");
            }},
        std::back_inserter(steps));
    CHECK(walk(steps) == steps.size());
    const auto ms = [](const steady::duration& took) {
        return std::chrono::duration_cast<std::chrono::milliseconds>(took).count();
    };
    WARN("R10 erase " << ms(erased) << " ms, delete " << ms(deleted) << " ms");
    // Mostly drawing the canvas again, as for the line of seats.
    CHECK(ms(erased) < 1000);
    CHECK(ms(deleted) < 600);
}

TEST_CASE(
    "the vehicle editor shows the canvas of a 60 by 60 frame with a part held at once",
    "[editor_vehicle][engine_client_component]") {
    const auto guard = screen_guard{};
    const auto folder = my_mod_folder{};
    auto shown = steady::duration{};
    auto sent = steady::duration{};
    auto steps = std::vector<walk_step>{[](const auto&) {
        return invoke("vehicle:new", {.text = "Bus"});
    }};
    std::ranges::move(frames_on(mounts_in({-29, -29}, {30, 30}, [](const point&) { return true; })),
                      std::back_inserter(steps));
    std::ranges::move(
        std::vector<walk_step>{
            [](const auto&) { return invoke("vehicle:part_filter", {.text = "seat"}); },
            [](const auto&) { return invoke("vehicle:choose", {.row = "vpart:seat"}); },
            [](const auto&) { return invoke("vehicle:tool_install"); },
            [&](const auto& screen) {
                // Every square of the canvas says whether the seat would go on it.
                CHECK(canvas(screen).cols * canvas(screen).rows >= 84 * 84);
                const auto began = steady::now();
                const auto state = captured();
                shown = steady::now() - began;
                const auto sending = steady::now();
                CHECK(ec::serialize_interaction_boundary(state).size() < ec::maximum_inline_bytes);
                sent = steady::now() - sending;
                return act("QUIT");
            }},
        std::back_inserter(steps));
    CHECK(walk(steps) == steps.size());
    const auto ms = [](const steady::duration& took) {
        return std::chrono::duration_cast<std::chrono::milliseconds>(took).count();
    };
    WARN("R10 canvas " << ms(shown) << " ms, boundary " << ms(sent) << " ms");
    CHECK(ms(shown) < 200);
    CHECK(ms(sent) < 200);
}

TEST_CASE(
    "the vehicle editor installs on the mount a client picks by an unlisted square, in a turned "
    "view too",
    "[editor_vehicle][engine_client_component]") {
    const auto guard = screen_guard{};
    const auto folder = my_mod_folder{};
    /// How a client picks the square `east` and `south` of where mount 0,0 shows on the canvas
    /// `grid`, and whether the grid lists a cell there.
    struct picked {
        std::string row;
        bool listed = false;
    };
    const auto beside =
        [](const ec::screen_view& screen, const nlohmann::json& grid, const int east,
           const int south) -> picked {
        const auto origin = shown_at(screen, 0, 0);
        const auto x = origin.x + east;
        const auto y = origin.y + south;
        return {.row = string_format("square:%d,%d", x, y), .listed = wire_listed(grid, x, y)};
    };
    // A drag reads the squares off the canvas as it was when the drag began.
    auto trail = std::vector<picked>{};
    auto line = std::vector<picked>{};
    const auto steps = std::vector<walk_step>{
        [](const auto&) { return invoke("vehicle:new", {.text = "Turn cart"}); },
        // The front, east, turns south.
        [](const auto&) { return invoke("vehicle:view_cw"); },
        [](const auto&) { return invoke("vehicle:tool_install"); },
        [&](const auto& screen) {
            const auto grid = wire_grid();
            // South of mount 0,0 on the canvas are the mounts in front of it.
            trail = {beside(screen, grid, 0, 1), beside(screen, grid, 0, 2),
                     beside(screen, grid, 0, 3)};
            // Next to the frame a frame can go, so that square is listed; the ones beyond are not.
            CHECK(trail[0].listed);
            CHECK_FALSE(trail[1].listed);
            CHECK_FALSE(trail[2].listed);
            return invoke("vehicle:stroke_begin");
        },
        [&](const auto&) { return pick("vehicle:mount", trail[0].row); },
        [&](const auto&) { return pick("vehicle:mount", trail[1].row); },
        [&](const auto&) { return pick("vehicle:mount", trail[2].row); },
        [](const auto&) { return invoke("vehicle:stroke_end"); },
        [&](const auto& screen) {
            CHECK(has_part(screen, 1, 0));
            CHECK(has_part(screen, 2, 0));
            CHECK(has_part(screen, 3, 0));
            CHECK(cell(screen, "mount:3,0").selected);
            // West of mount 0,0 is the mount on its right, too far from the frame for one.
            const auto far = beside(screen, wire_grid(), -2, 0);
            CHECK_FALSE(far.listed);
            return pick("vehicle:mount", far.row);
        },
        [](const auto& screen) {
            CHECK(cell(screen, "mount:0,2").selected);
            CHECK_FALSE(has_part(screen, 0, 2));
            CHECK(notice_of(screen).contains("touch the vehicle's frame"));
            return invoke("vehicle:tool_line");
        },
        [&](const auto& screen) {
            const auto grid = wire_grid();
            // East of mount 0,0 on the canvas are the mounts on its left.
            line = {beside(screen, grid, 1, 0), beside(screen, grid, 3, 0)};
            CHECK_FALSE(line[1].listed);
            return invoke("vehicle:stroke_begin");
        },
        [&](const auto&) { return pick("vehicle:mount", line[0].row); },
        [&](const auto&) { return pick("vehicle:mount", line[1].row); },
        [](const auto&) { return invoke("vehicle:stroke_end"); },
        [&](const auto& screen) {
            INFO(notice_of(screen));
            CHECK(said(screen) == "Installed frame on 3 squares.");
            CHECK(has_part(screen, 0, -1));
            CHECK(has_part(screen, 0, -3));
            // A secondary click on a square the grid does not list erases on its mount.
            const auto empty = beside(screen, wire_grid(), 1, 5);
            CHECK_FALSE(empty.listed);
            return pick("vehicle:clear", empty.row);
        },
        [](const auto& screen) {
            CHECK(notice_of(screen) == "This mount has no part to remove.");
            CHECK(cell(screen, "mount:5,-1").selected);
            return act("QUIT");
        }};
    CHECK(walk(steps) == steps.size());
}

TEST_CASE(
    "the vehicle editor's canvas grows to keep free mounts only once a stroke ends",
    "[editor_vehicle]") {
    const auto guard = screen_guard{};
    const auto folder = my_mod_folder{};
    auto origin = ec::grid_point{};
    auto cols = 0;
    const auto steps = std::vector<walk_step>{
        [](const auto&) { return invoke("vehicle:new", {.text = "Long cart"}); },
        [](const auto&) { return invoke("vehicle:tool_line"); },
        [&](const auto& screen) {
            origin = shown_at(screen, 0, 0);
            cols = canvas(screen).cols;
            return invoke("vehicle:stroke_begin");
        },
        [](const auto&) { return mount_at(0, 0); },
        // Twelve free mounts west of mount -9,0 lie beyond the canvas.
        [](const auto&) { return mount_at(-9, 0); },
        [&](const auto& screen) {
            CHECK(has_part(screen, -9, 0));
            // The squares a drag names by their place stay where they were.
            CHECK(shown_at(screen, 0, 0) == origin);
            CHECK(canvas(screen).cols == cols);
            return invoke("vehicle:stroke_end");
        },
        [&](const auto& screen) {
            CHECK(shown_at(screen, -9, 0).x == 12);
            CHECK(canvas(screen).cols > cols);
            return act("QUIT");
        }};
    CHECK(walk(steps) == steps.size());
}

TEST_CASE(
    "the vehicle editor selects a rectangle of mounts and copies, pastes, cuts and deletes it, "
    "each one undo step",
    "[editor_vehicle]") {
    const auto guard = screen_guard{};
    const auto folder = my_mod_folder{};
    auto before = std::size_t{0};
    /// Selects the mounts from `from` to `to` with a drag of the Select tool.
    const auto select = [](const point& from, const point& to) {
        return std::vector<walk_step>{
            [](const auto&) { return invoke("vehicle:tool_select"); },
            [](const auto&) { return invoke("vehicle:stroke_begin"); },
            [=](const auto&) { return mount_at(from.x, from.y); },
            [=](const auto&) { return mount_at(to.x, to.y); },
            [](const auto&) { return invoke("vehicle:stroke_end"); }};
    };
    const auto selection = [](const ec::screen_view& screen) {
        const auto& regions = canvas(screen).regions;
        return std::ranges::find(regions, std::string("selection"), &ec::grid_region::id);
    };
    const auto stamp = [](const ec::screen_view& screen) {
        return canvas(screen).stamp | std::views::transform([](const ec::grid_point& offset) {
                   return string_format("%d,%d", offset.x, offset.y);
               })
             | std::ranges::to<std::vector>();
    };
    auto steps = std::vector<walk_step>{
        [](const auto&) { return invoke("vehicle:new", {.text = "Clip van"}); },
        [](const auto&) { return invoke("vehicle:tool_rect_filled"); },
        [](const auto&) { return mount_at(0, 0); },
        [](const auto&) { return mount_at(2, 1); },
        [](const auto&) { return invoke("vehicle:part_filter", {.text = "seat"}); },
        [](const auto&) { return invoke("vehicle:choose", {.row = "vpart:seat"}); },
        [](const auto&) { return invoke("vehicle:tool_line"); },
        [](const auto&) { return mount_at(0, 0); },
        [](const auto&) { return mount_at(2, 0); },
        [&](const auto& screen) {
            CHECK(said(screen) == "Installed seat on 3 squares.");
            // Nothing to paste yet.
            CHECK_FALSE(affordance(screen, "vehicle:tool_paste").enabled);
            CHECK(affordance(screen, "vehicle:copy_mounts").keys
                  == std::vector<std::string>{"CTRL+C"});
            CHECK(affordance(screen, "vehicle:cut_mounts").keys
                  == std::vector<std::string>{"CTRL+X"});
            CHECK(affordance(screen, "vehicle:tool_paste").keys
                  == std::vector<std::string>{"v", "CTRL+V"});
            // The world's Paste vehicle leaves Ctrl+V to the canvas.
            CHECK(affordance(screen, "vehicle:paste").keys.empty());
            return invoke("vehicle:tool_select");
        },
        [](const auto& screen) {
            // The Select tool spans from the square pressed to the one released.
            CHECK(canvas(screen).drag == "span");
            return invoke("vehicle:stroke_begin");
        },
    };
    const auto add = [&](std::vector<walk_step> more) {
        std::ranges::move(more, std::back_inserter(steps));
    };
    add({[](const auto&) { return mount_at(0, 0); }, [](const auto&) { return mount_at(2, 1); },
         [](const auto&) { return invoke("vehicle:stroke_end"); },
         [&](const auto& screen) {
             const auto found = selection(screen);
             REQUIRE(found != canvas(screen).regions.end());
             CHECK(found->from == shown_at(screen, 0, 0));
             CHECK(found->to == shown_at(screen, 2, 1));
             CHECK(found->label == "Selection");
             // With squares marked, Delete deletes them instead of the picked part.
             CHECK(affordance(screen, "vehicle:delete_mounts").keys
                   == std::vector<std::string>{"DELETE"});
             CHECK(affordance(screen, "vehicle:remove").keys.empty());
             return invoke("vehicle:copy_mounts");
         },
         [&](const auto& screen) {
             CHECK(said(screen) == "Copied 9 parts.");
             return invoke("vehicle:tool_paste");
         },
         [&](const auto& screen) {
             CHECK(picked_tool(screen) == "canvas_tool:paste");
             // Where the paste lands follows the pointer.
             CHECK(stamp(screen)
                   == std::vector<std::string>{"0,0", "1,0", "2,0", "0,1", "1,1", "2,1"});
             before = steps_done(screen);
             return mount_at(10, 10);
         },
         [&](const auto& screen) {
             // Off the vehicle no frame holds on: nothing changes, and it says why.
             CHECK_FALSE(notice_of(screen).empty());
             CHECK_FALSE(has_part(screen, 10, 10));
             CHECK(steps_done(screen) == before);
             return mount_at(3, 0);
         },
         [&](const auto& screen) {
             // Frames first, spreading from the vehicle, then the seats on them.
             CHECK(said(screen) == "Pasted 9 parts.");
             CHECK(cell(screen, "mount:5,0").label == "frame, seat");
             CHECK(cell(screen, "mount:5,1").label == "frame");
             CHECK(steps_done(screen) == before + 1);
             return invoke("vehicle:view_cw");
         },
         [](const auto& screen) {
             // The footprint turns with the canvas.
             CHECK(canvas(screen).stamp.back() == ec::grid_point{-1, 2});
             return mount_at(0, 2);
         },
         [&](const auto& screen) {
             CHECK(said(screen) == "Pasted 9 parts.");
             CHECK(cell(screen, "mount:2,2").label == "frame, seat");
             CHECK(cell(screen, "mount:2,3").label == "frame");
             CHECK(steps_done(screen) == before + 2);
             return invoke("vehicle:view_ccw");
         }});
    add(select({3, 0}, {5, 1}));
    add({[](const auto&) { return invoke("vehicle:cut_mounts"); },
         [&](const auto& screen) {
             CHECK(said(screen) == "Cut 9 parts.");
             CHECK_FALSE(has_part(screen, 3, 0));
             CHECK_FALSE(has_part(screen, 5, 1));
             CHECK(steps_done(screen) == before + 3);
             return invoke("vehicle:undo");
         },
         [&](const auto& screen) {
             CHECK(cell(screen, "mount:3,0").label == "frame, seat");
             // The step undone stays in the history, to redo.
             CHECK(steps_done(screen) == before + 3);
             return invoke("vehicle:redo");
         }});
    add(select({0, 2}, {2, 3}));
    add({[](const auto&) { return invoke("vehicle:delete_mounts"); },
         [&](const auto& screen) {
             CHECK(said(screen) == "Deleted 9 parts.");
             CHECK_FALSE(has_part(screen, 0, 2));
             CHECK_FALSE(has_part(screen, 2, 3));
             CHECK(steps_done(screen) == before + 4);
             return invoke("vehicle:stroke_begin");
         },
         // A click without a drag marks nothing.
         [](const auto&) { return mount_at(1, 1); },
         [](const auto&) { return invoke("vehicle:stroke_end"); },
         [&](const auto& screen) {
             CHECK(selection(screen) == canvas(screen).regions.end());
             CHECK(affordance(screen, "vehicle:remove").keys == std::vector<std::string>{"DELETE"});
             return invoke("vehicle:tool_select");
         }});
    add(select({1, 0}, {1, 1}));
    add({[](const auto&) { return invoke("vehicle:delete_mounts"); },
         [&](const auto& screen) {
             // The last frame between the two halves stays, as the game keeps the vehicle whole.
             CHECK(said(screen)
                   == "Deleted 2 parts. 1 part refused: Removing this part would split the "
                      "vehicle.");
             CHECK(has_part(screen, 1, 0) != has_part(screen, 1, 1));
             CHECK(steps_done(screen) == before + 5);
             return invoke("vehicle:undo");
         },
         [](const auto&) { return invoke("vehicle:cut_mounts"); },
         [&](const auto& screen) {
             CHECK(said(screen)
                   == "Cut 2 parts. 1 part refused: Removing this part would split the vehicle.");
             return invoke("vehicle:tool_paste");
         },
         [](const auto&) { return mount_at(3, 0); },
         [&](const auto& screen) {
             // The clip holds what the cut took: the seat, which needs a frame there, and the
             // frame below it, never the frame that stayed.
             CHECK(said(screen).starts_with("Pasted 1 part. 1 part refused: "));
             CHECK(cell(screen, "mount:3,1").label == "frame");
             CHECK_FALSE(has_part(screen, 3, 0));
             return act("QUIT");
         }});
    CHECK(walk(steps) == steps.size());
}

TEST_CASE(
    "the vehicle editor copies, pastes and deletes mounts of a world vehicle in place",
    "[editor_vehicle]") {
    const auto guard = screen_guard{};
    clear_map();
    const auto cleanup_vehicles = on_out_of_scope([] { clear_vehicles(); });
    const auto at = get_avatar().bub_pos() + tripoint_rel_ms(8, 0, 0);
    auto* const veh = editor::spawn_design(vproto_id("bicycle"), at, {});
    REQUIRE(veh != nullptr);
    const auto parts = veh->part_count();
    auto before = std::size_t{0};
    const auto steps = std::vector<walk_step>{
        [](const auto&) { return invoke("vehicle:source", {.row = "source:world"}); },
        [](const auto&) { return invoke("vehicle:edit", {.row = "world:0"}); },
        [](const auto&) { return invoke("vehicle:tool_select"); },
        [](const auto&) { return invoke("vehicle:stroke_begin"); },
        [](const auto&) { return mount_at(-1, 0); },
        [](const auto&) { return mount_at(1, 0); },
        [](const auto&) { return invoke("vehicle:stroke_end"); },
        [](const auto&) { return invoke("vehicle:copy_mounts"); },
        [](const auto&) { return invoke("vehicle:tool_paste"); },
        [&](const auto& screen) {
            before = steps_done(screen);
            return mount_at(-1, 1);
        },
        [&](const auto& screen) {
            CHECK(said(screen).starts_with("Pasted "));
            CHECK(has_part(screen, 0, 1));
            CHECK(steps_done(screen) == before + 1);
            return invoke("vehicle:tool_select");
        },
        [](const auto&) { return invoke("vehicle:stroke_begin"); },
        [](const auto&) { return mount_at(-1, 1); },
        [](const auto&) { return mount_at(1, 1); },
        [](const auto&) { return invoke("vehicle:stroke_end"); },
        [](const auto&) { return invoke("vehicle:delete_mounts"); },
        [&](const auto& screen) {
            CHECK(said(screen).starts_with("Deleted "));
            CHECK_FALSE(has_part(screen, -1, 1));
            CHECK_FALSE(has_part(screen, 0, 1));
            CHECK_FALSE(has_part(screen, 1, 1));
            CHECK(has_part(screen, 0, 0));
            CHECK(steps_done(screen) == before + 2);
            return invoke("vehicle:undo");
        },
        [](const auto&) { return invoke("vehicle:undo"); },
        [](const auto&) { return invoke("vehicle:done"); },
        [](const auto&) { return act("QUIT"); },
    };
    CHECK(walk(steps) == steps.size());
    const auto placed = get_map().veh_at(at);
    REQUIRE(placed);
    CHECK(placed->vehicle().part_count() == parts);
}

TEST_CASE(
    "the vehicle editor draws a line on a world vehicle in place as one undo step",
    "[editor_vehicle]") {
    const auto guard = screen_guard{};
    clear_map();
    const auto cleanup_vehicles = on_out_of_scope([] { clear_vehicles(); });
    const auto at = get_avatar().bub_pos() + tripoint_rel_ms(8, 0, 0);
    auto* const veh = editor::spawn_design(vproto_id("bicycle"), at, {});
    REQUIRE(veh != nullptr);
    const auto parts = veh->part_count();
    auto opened = std::string{};
    const auto steps = std::vector<walk_step>{
        [](const auto&) { return invoke("vehicle:source", {.row = "source:world"}); },
        [](const auto&) { return invoke("vehicle:edit", {.row = "world:0"}); },
        [](const auto&) { return invoke("vehicle:part_filter", {.text = "xlframe_vertical"}); },
        [](const auto&) { return invoke("vehicle:choose", {.row = "vpart:xlframe_vertical"}); },
        [](const auto&) { return invoke("vehicle:tool_line"); },
        [&](const auto& screen) {
            opened = step_at(screen);
            return mount_at(0, 1);
        },
        [](const auto&) { return mount_at(0, 3); },
        [](const auto& screen) {
            CHECK(said(screen) == "Installed extra light frame on 3 squares.");
            CHECK(step_at(screen) == "Install extra light frame ×3");
            return mount_at(0, 1);
        },
        [](const auto&) { return mount_at(0, 4); },
        [](const auto& screen) {
            // The step names what the stroke installed, not the squares that had it already.
            CHECK(said(screen) == "Installed extra light frame on 1 square.");
            CHECK(step_at(screen) == "Install extra light frame ×1");
            return invoke("vehicle:undo");
        },
        [](const auto& screen) {
            CHECK_FALSE(has_part(screen, 0, 4));
            CHECK(has_part(screen, 0, 3));
            return invoke("vehicle:undo");
        },
        [&](const auto& screen) {
            // The whole line went with one undo.
            CHECK_FALSE(has_part(screen, 0, 1));
            CHECK(step_at(screen) == opened);
            return invoke("vehicle:done");
        },
        [](const auto&) { return act("QUIT"); },
    };
    CHECK(walk(steps) == steps.size());
    const auto placed = get_map().veh_at(at);
    REQUIRE(placed);
    CHECK(placed->vehicle().part_count() == parts);
}

TEST_CASE(
    "the vehicle editor names a pencil or eraser drag on a world vehicle in place by what it did",
    "[editor_vehicle]") {
    const auto guard = screen_guard{};
    clear_map();
    const auto cleanup_vehicles = on_out_of_scope([] { clear_vehicles(); });
    const auto at = get_avatar().bub_pos() + tripoint_rel_ms(8, 0, 0);
    REQUIRE(editor::spawn_design(vproto_id("bicycle"), at, {}) != nullptr);
    const auto steps = std::vector<walk_step>{
        [](const auto&) { return invoke("vehicle:source", {.row = "source:world"}); },
        [](const auto&) { return invoke("vehicle:edit", {.row = "world:0"}); },
        [](const auto&) { return invoke("vehicle:part_filter", {.text = "xlframe_vertical"}); },
        [](const auto&) { return invoke("vehicle:choose", {.row = "vpart:xlframe_vertical"}); },
        [](const auto&) { return invoke("vehicle:tool_install"); },
        [](const auto&) { return invoke("vehicle:stroke_begin"); },
        [](const auto&) { return mount_at(0, 1); },
        [](const auto&) { return mount_at(0, 2); },
        [](const auto&) { return mount_at(0, 3); },
        [](const auto&) { return invoke("vehicle:stroke_end"); },
        [](const auto& screen) {
            CHECK(step_at(screen) == "Install extra light frame ×3");
            return invoke("vehicle:stroke_begin");
        },
        [](const auto&) { return invoke("vehicle:clear", {.row = "mount:0,3"}); },
        [](const auto&) { return invoke("vehicle:clear", {.row = "mount:0,2"}); },
        [](const auto&) { return invoke("vehicle:stroke_end"); },
        [](const auto& screen) {
            CHECK(step_at(screen) == "Remove extra light frame ×2");
            // A click is a step of its own, named by the mount.
            return mount_at(0, 2);
        },
        [](const auto& screen) {
            CHECK(step_at(screen) == "Install extra light frame at 0,2");
            return invoke("vehicle:done");
        },
        [](const auto&) { return act("QUIT"); },
    };
    CHECK(walk(steps) == steps.size());
}

TEST_CASE(
    "the vehicle editor keeps what it copied when a cut removes nothing", "[editor_vehicle]") {
    const auto guard = screen_guard{};
    const auto folder = my_mod_folder{};
    const auto steps = std::vector<walk_step>{
        [](const auto&) { return invoke("vehicle:new", {.text = "Solo"}); },
        [](const auto&) { return invoke("vehicle:part_filter", {.text = "seat"}); },
        [](const auto&) { return invoke("vehicle:choose", {.row = "vpart:seat"}); },
        [](const auto&) { return mount_at(0, 0); },
        [](const auto&) { return invoke("vehicle:copy_mounts"); },
        [](const auto& screen) {
            CHECK(said(screen) == "Copied 2 parts.");
            // The seat goes, the frame stays: the last part of a vehicle.
            return invoke("vehicle:remove");
        },
        [](const auto&) { return invoke("vehicle:cut_mounts"); },
        [](const auto& screen) {
            CHECK(said(screen).empty());
            CHECK(notice_of(screen) == "A vehicle keeps its last part.");
            return invoke("vehicle:tool_paste");
        },
        [](const auto&) { return mount_at(0, 1); },
        [](const auto& screen) {
            // What was copied before, the frame and the seat.
            CHECK(said(screen) == "Pasted 2 parts.");
            CHECK(cell(screen, "mount:0,1").label.contains("seat"));
            return act("QUIT");
        },
    };
    CHECK(walk(steps) == steps.size());
}

TEST_CASE(
    "the vehicle editor keeps its canvas over the same parts when deleting moves a world "
    "vehicle's mounts",
    "[editor_vehicle]") {
    const auto guard = screen_guard{};
    clear_map();
    const auto cleanup_vehicles = on_out_of_scope([] { clear_vehicles(); });
    const auto at = get_avatar().bub_pos() + tripoint_rel_ms(8, 0, 0);
    REQUIRE(editor::spawn_design(vproto_id("bicycle"), at, {}) != nullptr);
    auto rear = ec::grid_point{};
    auto marked = ec::grid_region{};
    const auto selection = [](const ec::screen_view& screen) -> ec::grid_region {
        const auto& regions = canvas(screen).regions;
        const auto found =
            std::ranges::find(regions, std::string("selection"), &ec::grid_region::id);
        REQUIRE(found != regions.end());
        return *found;
    };
    const auto steps = std::vector<walk_step>{
        [](const auto&) { return invoke("vehicle:source", {.row = "source:world"}); },
        [](const auto&) { return invoke("vehicle:edit", {.row = "world:0"}); },
        [](const auto&) { return invoke("vehicle:tool_select"); },
        [](const auto&) { return invoke("vehicle:stroke_begin"); },
        [](const auto&) { return mount_at(0, 0); },
        [](const auto&) { return mount_at(1, 0); },
        [](const auto&) { return invoke("vehicle:stroke_end"); },
        [&](const auto& screen) {
            rear = shown_at(screen, -1, 0);
            marked = selection(screen);
            return invoke("vehicle:delete_mounts");
        },
        [&](const auto& screen) {
            // Taking 0,0 away moved every mount; the rear wheel and its basket still show where
            // they did, and the selection over the squares it marked.
            const auto& cells = canvas(screen).cells;
            const auto shown = std::ranges::find(cells, rear, &ec::grid_cell::at);
            REQUIRE(shown != cells.end());
            CHECK(shown->label.contains("basket"));
            CHECK(selection(screen).from == marked.from);
            CHECK(selection(screen).to == marked.to);
            CHECK(
                std::ranges::
                    count_if(cells, [](const ec::grid_cell& entry) { return !entry.label.empty(); })
                == 1);
            return invoke("vehicle:done");
        },
        [](const auto&) { return act("QUIT"); },
    };
    CHECK(walk(steps) == steps.size());
}

TEST_CASE(
    "the vehicle editor makes a new step for a change after an undo in the middle of a stroke on "
    "a world vehicle",
    "[editor_vehicle]") {
    const auto guard = screen_guard{};
    clear_map();
    const auto cleanup_vehicles = on_out_of_scope([] { clear_vehicles(); });
    const auto at = get_avatar().bub_pos() + tripoint_rel_ms(8, 0, 0);
    REQUIRE(editor::spawn_design(vproto_id("bicycle"), at, {}) != nullptr);
    const auto steps = std::vector<walk_step>{
        [](const auto&) { return invoke("vehicle:source", {.row = "source:world"}); },
        [](const auto&) { return invoke("vehicle:edit", {.row = "world:0"}); },
        [](const auto&) { return invoke("vehicle:part_filter", {.text = "xlframe_vertical"}); },
        [](const auto&) { return invoke("vehicle:choose", {.row = "vpart:xlframe_vertical"}); },
        [](const auto&) { return invoke("vehicle:tool_install"); },
        [](const auto&) { return invoke("vehicle:stroke_begin"); },
        [](const auto&) { return mount_at(0, 1); },
        [](const auto&) { return invoke("vehicle:undo"); },
        [](const auto& screen) {
            CHECK_FALSE(has_part(screen, 0, 1));
            return mount_at(0, 1);
        },
        [](const auto&) { return mount_at(0, 2); },
        [](const auto&) { return invoke("vehicle:stroke_end"); },
        [](const auto& screen) {
            CHECK(has_part(screen, 0, 2));
            return invoke("vehicle:undo");
        },
        [](const auto& screen) {
            // The undo ended the stroke, as it ends a design's, so each change after it is a step
            // of its own that undo takes back.
            CHECK_FALSE(has_part(screen, 0, 2));
            CHECK(has_part(screen, 0, 1));
            return invoke("vehicle:undo");
        },
        [](const auto& screen) {
            CHECK_FALSE(has_part(screen, 0, 1));
            return invoke("vehicle:done");
        },
        [](const auto&) { return act("QUIT"); },
    };
    CHECK(walk(steps) == steps.size());
}

TEST_CASE(
    "the vehicle editor's eraser keeps the mirror's axis through a stroke and tells what it "
    "removed",
    "[editor_vehicle]") {
    const auto guard = screen_guard{};
    const auto folder = my_mod_folder{};
    const auto steps = std::vector<walk_step>{
        [](const auto&) { return invoke("vehicle:new", {.text = "Axis van"}); },
        [](const auto&) { return invoke("vehicle:tool_line"); },
        [](const auto&) { return mount_at(0, 0); },
        [](const auto&) { return mount_at(4, 0); },
        [](const auto&) { return invoke("vehicle:tool_install"); },
        [](const auto&) { return mount_at(0, 1); },
        [](const auto&) { return invoke("vehicle:part_filter", {.text = "seat"}); },
        [](const auto&) { return invoke("vehicle:choose", {.row = "vpart:seat"}); },
        [](const auto&) { return mount_at(0, 0); },
        [](const auto&) { return invoke("vehicle:mirror_ends"); },
        [](const auto&) { return invoke("vehicle:tool_remove"); },
        // An empty square whose image across the middle has a part.
        [](const auto&) { return mount_at(4, 1); },
        [](const auto& screen) {
            CHECK_FALSE(has_part(screen, 0, 1));
            CHECK(said(screen) == "Removed frame at 0,1.");
            CHECK(notice_of(screen).empty());
            return invoke("vehicle:stroke_begin");
        },
        [](const auto&) { return mount_at(4, 0); },
        [](const auto& screen) {
            // The frame clicked and the seat across the middle.
            CHECK(said(screen) == "Removed 2 parts.");
            CHECK(cell(screen, "mount:0,0").label == "frame");
            return mount_at(3, 0);
        },
        [](const auto&) { return invoke("vehicle:stroke_end"); },
        [](const auto& screen) {
            // The axis stays where the stroke began, so 3,0 mirrors to 1,0, never to 0,0.
            CHECK_FALSE(has_part(screen, 4, 0));
            CHECK_FALSE(has_part(screen, 3, 0));
            CHECK(has_part(screen, 0, 0));
            // A step of more than one part names them.
            CHECK(step_at(screen) == "Remove 3 parts (frame, seat)");
            return act("QUIT");
        },
    };
    CHECK(walk(steps) == steps.size());
}

TEST_CASE(
    "the vehicle editor picks the part a palette symbol puts on each mount it is drawn on",
    "[editor_vehicle]") {
    const auto guard = screen_guard{};
    const auto folder = my_mod_folder{};
    const auto frame = vpart_id("frame_vertical")->name();
    const auto seat = vpart_id("seat")->name();
    /// Where the picked part is among the parts on the picked mount; none for none.
    const auto picked_at = [](const ec::screen_view& screen) -> std::optional<std::ptrdiff_t> {
        const auto& stack = rows(screen, "vehicle:stack");
        const auto found = std::ranges::find(stack, true, &ec::row::selected);
        return found == stack.end() ? std::nullopt : std::optional(found - stack.begin());
    };
    const auto set = [](const char* const pointer, const char* const value) {
        return std::vector<walk_step>{
            [=](const auto&) {
                return invoke("vehicle:field_set", {.row = "pointer", .text = pointer});
            },
            [=](const auto&) {
                return invoke("vehicle:field_set", {.row = "value", .text = value});
            },
            [](const auto&) { return invoke("vehicle:field_apply"); }};
    };
    auto steps = std::vector<walk_step>{
        [](const auto&) { return invoke("vehicle:new", {.text = "Palette van"}); },
        [](const auto&) { return invoke("vehicle:show:json"); }};
    const auto add = [&](std::vector<walk_step> more) {
        std::ranges::move(more, std::back_inserter(steps));
    };
    add(set("/palette", R"({"x":["frame_vertical","seat"]})"));
    add(set("/blueprint", R"(["xx"])"));
    add(set("/parts", "[]"));
    add({[](const auto& screen) {
             // One symbol draws a frame and a seat on two mounts.
             CHECK(messages(screen).empty());
             CHECK(cell(screen, "mount:1,0").looks.size() == 2);
             return invoke("vehicle:tool_select");
         },
         [](const auto&) { return invoke("vehicle:show:mount"); },
         [](const auto&) { return mount_at(1, 0); },
         [&](const auto& screen) {
             // The second mount the symbol is drawn on has its top part picked, as the first.
             CHECK(picked_at(screen) == 1);
             CHECK(affordance(screen, "vehicle:remove").label == "Remove " + seat);
             return invoke("vehicle:pick", {.row = rows(screen, "vehicle:stack").front().id});
         },
         [&](const auto& screen) {
             CHECK(picked_at(screen) == 0);
             CHECK(affordance(screen, "vehicle:remove").label == "Remove " + frame);
             CHECK(affordance(screen, "vehicle:later").enabled);
             CHECK(affordance(screen, "vehicle:show:part").enabled);
             return invoke("vehicle:remove");
         },
         [](const auto& screen) {
             // Remove says why the part stays, rather than that the mount has none.
             CHECK(notice_of(screen)
                   == "The blueprint's palette places this part; edit the palette.");
             CHECK(rows(screen, "vehicle:stack").size() == 2);
             return mount_at(0, 0);
         },
         [&](const auto& screen) {
             CHECK(picked_at(screen) == 1);
             return act("QUIT");
         }});
    CHECK(walk(steps) == steps.size());
}

TEST_CASE(
    "the vehicle editor's eraser takes one part off each square a stroke crosses, however often "
    "it crosses it",
    "[editor_vehicle]") {
    const auto guard = screen_guard{};
    const auto folder = my_mod_folder{};
    auto before = std::size_t{0};
    const auto parts_on = [](const ec::screen_view& screen, const int x, const int y) {
        return cell(screen, string_format("mount:%d,%d", x, y)).looks.size();
    };
    const auto steps = std::vector<walk_step>{
        [](const auto&) { return invoke("vehicle:new", {.text = "Rub van"}); },
        [](const auto&) { return invoke("vehicle:tool_line"); },
        [](const auto&) { return mount_at(0, 0); },
        [](const auto&) { return mount_at(3, 0); },
        [](const auto&) { return invoke("vehicle:part_filter", {.text = "seat"}); },
        [](const auto&) { return invoke("vehicle:choose", {.row = "vpart:seat"}); },
        [](const auto&) { return invoke("vehicle:tool_install"); },
        [](const auto&) { return invoke("vehicle:stroke_begin"); },
        [](const auto&) { return mount_at(3, 0); },
        [](const auto&) { return mount_at(2, 0); },
        [](const auto&) { return mount_at(3, 0); },
        [](const auto&) { return invoke("vehicle:stroke_end"); },
        [&](const auto& screen) {
            // The pencil back over a square puts no second seat on it.
            CHECK(parts_on(screen, 3, 0) == 2);
            CHECK(parts_on(screen, 2, 0) == 2);
            before = steps_done(screen);
            return invoke("vehicle:tool_remove");
        },
        [](const auto&) { return invoke("vehicle:stroke_begin"); },
        [](const auto&) { return mount_at(3, 0); },
        [](const auto&) { return mount_at(2, 0); },
        // Back over the squares erased, as a shaky hand goes, and a secondary drag too.
        [](const auto&) { return mount_at(3, 0); },
        [](const auto&) { return invoke("vehicle:clear", {.row = "mount:2,0"}); },
        [](const auto&) { return invoke("vehicle:stroke_end"); },
        [&](const auto& screen) {
            // The seats went; the frames under them stay.
            CHECK(parts_on(screen, 3, 0) == 1);
            CHECK(parts_on(screen, 2, 0) == 1);
            CHECK(steps_done(screen) == before + 1);
            CHECK(notice_of(screen).empty());
            return invoke("vehicle:stroke_begin");
        },
        [](const auto&) { return mount_at(3, 0); },
        [](const auto&) { return invoke("vehicle:stroke_end"); },
        [](const auto& screen) {
            // A new stroke takes the part under.
            CHECK_FALSE(has_part(screen, 3, 0));
            CHECK(has_part(screen, 2, 0));
            return act("QUIT");
        },
    };
    CHECK(walk(steps) == steps.size());
}

TEST_CASE(
    "the vehicle editor's eraser takes one part off each square of a world vehicle a stroke "
    "crosses, as its mounts move",
    "[editor_vehicle]") {
    const auto guard = screen_guard{};
    clear_map();
    const auto cleanup_vehicles = on_out_of_scope([] { clear_vehicles(); });
    const auto at = get_avatar().bub_pos() + tripoint_rel_ms(8, 0, 0);
    REQUIRE(editor::spawn_design(vproto_id("bicycle"), at, {}) != nullptr);
    const auto parts_on = [](const ec::screen_view& screen, const int x, const int y) {
        return cell(screen, string_format("mount:%d,%d", x, y)).looks.size();
    };
    /// The cell the canvas shows at `where`.
    const auto shown_cell =
        [](const ec::screen_view& screen, const ec::grid_point& where) -> const ec::grid_cell& {
        const auto& cells = canvas(screen).cells;
        const auto found = std::ranges::find(cells, where, &ec::grid_cell::at);
        REQUIRE(found != cells.end());
        return *found;
    };
    auto rear = ec::grid_point{};
    const auto steps = std::vector<walk_step>{
        [](const auto&) { return invoke("vehicle:source", {.row = "source:world"}); },
        [](const auto&) { return invoke("vehicle:edit", {.row = "world:0"}); },
        [](const auto&) { return invoke("vehicle:tool_remove"); },
        [](const auto&) { return invoke("vehicle:stroke_begin"); },
        [](const auto&) { return mount_at(0, 0); },
        [](const auto&) { return mount_at(-1, 0); },
        [](const auto&) { return mount_at(0, 0); },
        [](const auto&) { return invoke("vehicle:stroke_end"); },
        [&](const auto& screen) {
            // The pedals and the basket on top went, nothing under them.
            CHECK(parts_on(screen, 0, 0) == 3);
            CHECK(parts_on(screen, -1, 0) == 3);
            return mount_at(1, 0);
        },
        // Each click is a stroke of its own: the front wheel, its mount and its frame go, then
        // all of the middle but its frame.
        [](const auto&) { return mount_at(1, 0); },
        [](const auto&) { return mount_at(1, 0); },
        [](const auto&) { return mount_at(0, 0); },
        [](const auto&) { return mount_at(0, 0); },
        [&](const auto& screen) {
            CHECK_FALSE(has_part(screen, 1, 0));
            CHECK(parts_on(screen, 0, 0) == 1);
            rear = shown_at(screen, -1, 0);
            return invoke("vehicle:stroke_begin");
        },
        [](const auto&) { return mount_at(0, 0); },
        [&](const auto& screen) {
            // Taking the last part of 0,0 away moved every mount: the rear is 0,0 now, a square
            // the stroke has not erased on.
            CHECK(shown_cell(screen, rear).id == "mount:0,0");
            return invoke("vehicle:clear", {.row = shown_cell(screen, rear).id});
        },
        [](const auto&) { return invoke("vehicle:stroke_end"); },
        [&](const auto& screen) {
            CHECK(shown_cell(screen, rear).looks.size() == 2);
            return invoke("vehicle:done");
        },
        [](const auto&) { return act("QUIT"); },
    };
    CHECK(walk(steps) == steps.size());
}

TEST_CASE(
    "the vehicle editor's mirrored eraser takes the part off the mirror square of a world vehicle "
    "where the mounts moved to",
    "[editor_vehicle]") {
    const auto guard = screen_guard{};
    clear_map();
    const auto cleanup_vehicles = on_out_of_scope([] { clear_vehicles(); });
    const auto at = get_avatar().bub_pos() + tripoint_rel_ms(8, 0, 0);
    REQUIRE(editor::spawn_design(vproto_id("bicycle"), at, {}) != nullptr);
    /// The cell the canvas shows at `where`.
    const auto shown_cell =
        [](const ec::screen_view& screen, const ec::grid_point& where) -> const ec::grid_cell& {
        const auto& cells = canvas(screen).cells;
        const auto found = std::ranges::find(cells, where, &ec::grid_cell::at);
        REQUIRE(found != cells.end());
        return *found;
    };
    auto rear = ec::grid_point{};
    auto rear_parts = std::size_t{0};
    const auto steps = std::vector<walk_step>{
        [](const auto&) { return invoke("vehicle:source", {.row = "source:world"}); },
        [](const auto&) { return invoke("vehicle:edit", {.row = "world:0"}); },
        [](const auto&) { return invoke("vehicle:tool_remove"); },
        // The front wheel goes, then all of the middle but its frame.
        [](const auto&) { return mount_at(1, 0); },
        [](const auto&) { return mount_at(1, 0); },
        [](const auto&) { return mount_at(1, 0); },
        [](const auto&) { return mount_at(0, 0); },
        [](const auto&) { return mount_at(0, 0); },
        [](const auto&) { return mount_at(0, 0); },
        [&](const auto& screen) {
            CHECK_FALSE(has_part(screen, 1, 0));
            CHECK(cell(screen, "mount:0,0").looks.size() == 1);
            rear = shown_at(screen, -1, 0);
            rear_parts = shown_cell(screen, rear).looks.size();
            REQUIRE(rear_parts > 1);
            return invoke("vehicle:mirror_ends");
        },
        [](const auto&) { return mount_at(0, 0); },
        [&](const auto& screen) {
            // Taking the frame of 0,0 away moved every mount: the rear is 0,0 now, where its
            // image across the middle is, and loses its top part as well.
            CHECK(shown_cell(screen, rear).id == "mount:0,0");
            CHECK(shown_cell(screen, rear).looks.size() == rear_parts - 1);
            return invoke("vehicle:done");
        },
        [](const auto&) { return act("QUIT"); },
    };
    CHECK(walk(steps) == steps.size());
}

namespace {
/// The check `key` of the Ready to drive list.
auto drive_row(const ec::screen_view& screen, const std::string& key) -> const ec::row& {
    return row(screen, "vehicle:check", "check:" + key);
}
auto drive_ok(const ec::screen_view& screen, const std::string& key) -> bool {
    const auto& columns = drive_row(screen, key).columns;
    REQUIRE(columns.size() == 1);
    return columns.front().value == "✓";
}
auto performance_has(const ec::screen_view& screen, const std::string& key) -> bool {
    return std::ranges::contains(
        body<ec::panel_body>(part(screen, "vehicle:performance")).stats, key, &ec::panel_stat::key);
}
} // namespace

TEST_CASE(
    "the vehicle editor's wheel on a bare frame brings its hub, steerable at the front, as one step",
    "[editor_vehicle]") {
    const auto guard = screen_guard{};
    const auto mods = my_mod_folder{};
    auto done = std::size_t{0};
    const auto steps = std::vector<walk_step>{
        [](const auto&) { return invoke("vehicle:new", {.text = "Kart"}); },
        [](const auto&) { return invoke("vehicle:tool_rect_filled"); },
        [](const auto&) { return mount_at(-2, -1); },
        [](const auto&) { return mount_at(2, 1); },
        [](const auto&) { return invoke("vehicle:part_filter", {.text = "wheel"}); },
        [](const auto&) { return invoke("vehicle:choose", {.row = "vpart:wheel"}); },
        [](const auto&) { return invoke("vehicle:tool_install"); },
        [](const auto&) { return mount_at(2, -1); },
        [&](const auto& screen) {
            // The wheel shows its size.
            CHECK(cell(screen, "mount:2,-1").label.contains(", wheel hub assembly (steerable), "));
            CHECK(cell(screen, "mount:2,-1").label.ends_with(" wheel"));
            CHECK(step_at(screen) == "Install wheel hub assembly (steerable) and wheel at 2,-1");
            CHECK(said(screen) == "Installed wheel hub assembly (steerable) and wheel at 2,-1.");
            CHECK(notice_of(screen).empty());
            done = steps_done(screen);
            return mount_at(-2, 1);
        },
        [&](const auto& screen) {
            // Behind the middle, the hub does not steer.
            CHECK(cell(screen, "mount:-2,1").label.contains(", wheel hub assembly, "));
            CHECK(steps_done(screen) == done + 1);
            return invoke("vehicle:undo");
        },
        [](const auto& screen) {
            // One undo takes the wheel and its hub.
            CHECK(cell(screen, "mount:-2,1").label.find("wheel") == std::string::npos);
            return act("QUIT");
        },
    };
    CHECK(walk(steps) == steps.size());
}

TEST_CASE(
    "the vehicle editor's part search puts the part named so first, then the most used",
    "[editor_vehicle]") {
    const auto guard = screen_guard{};
    const auto mods = my_mod_folder{};
    const auto first = [](const ec::screen_view& screen) {
        const auto& found = rows(screen, "vehicle:parts");
        REQUIRE_FALSE(found.empty());
        return found.front().label;
    };
    const auto steps = std::vector<walk_step>{
        [](const auto&) { return invoke("vehicle:new", {.text = "Kart"}); },
        [](const auto&) { return invoke("vehicle:part_filter", {.text = "wheel"}); },
        [&](const auto& screen) {
            CHECK(first(screen) == "wheel");
            return invoke("vehicle:part_filter", {.text = "tank"});
        },
        [&](const auto& screen) {
            CHECK(first(screen) == "vehicle tank (60L)");
            return invoke("vehicle:part_filter", {.text = "door"});
        },
        [&](const auto& screen) {
            CHECK(first(screen) == "door");
            return act("QUIT");
        },
    };
    CHECK(walk(steps) == steps.size());
}

TEST_CASE(
    "the vehicle editor names the fuel it set in the history and the status", "[editor_vehicle]") {
    const auto guard = screen_guard{};
    const auto mods = my_mod_folder{};
    const auto steps = std::vector<walk_step>{
        [](const auto&) { return invoke("vehicle:new", {.text = "Tanker"}); },
        [](const auto&) { return invoke("vehicle:part_filter", {.text = "tank"}); },
        [](const auto&) { return invoke("vehicle:choose", {.row = "vpart:tank"}); },
        [](const auto&) { return mount_at(0, 0); },
        [](const auto&) { return invoke("vehicle:part_set", {.row = "fuel", .text = "diesel"}); },
        [](const auto& screen) {
            CHECK(step_at(screen) == "Set fuel of vehicle tank (60L) to diesel");
            CHECK(said(screen) == "Set fuel of vehicle tank (60L) to diesel.");
            return invoke("vehicle:part_set", {.row = "fuel", .text = ""});
        },
        [](const auto& screen) {
            CHECK(step_at(screen) == "Empty vehicle tank (60L)");
            return act("QUIT");
        },
    };
    CHECK(walk(steps) == steps.size());
}

TEST_CASE(
    "the vehicle editor checks a design is ready to drive and fixes each check with a click",
    "[editor_vehicle][editor_vehicle_check]") {
    const auto guard = screen_guard{};
    const auto mods = my_mod_folder{};
    auto done = std::size_t{0};
    const auto fix = [&](const ec::screen_view& screen, const std::string& key) {
        CHECK_FALSE(drive_ok(screen, key));
        const auto id = "vehicle:fix:" + key;
        CHECK(affordance(screen, id).enabled);
        CHECK(std::ranges::contains(part(screen, "vehicle:check").affordances, id));
        done = steps_done(screen);
        return invoke(id);
    };
    const auto fixed = [&](const ec::screen_view& screen, const std::string& key) {
        CHECK(drive_ok(screen, key));
        CHECK_FALSE(has_affordance(screen, "vehicle:fix:" + key));
        // A fix is one step.
        CHECK(steps_done(screen) == done + 1);
        CHECK(notice_of(screen).empty());
    };
    const auto steps = std::vector<walk_step>{
        [](const auto&) { return invoke("vehicle:new", {.text = "Go kart"}); },
        [](const auto& screen) {
            CHECK(slot_of(screen, "vehicle:check") == "tools");
            CHECK_FALSE(drive_ok(screen, "wheels"));
            CHECK(drive_row(screen, "wheels").label == "It has no wheels.");
            // No speed until it can drive.
            CHECK_FALSE(performance_has(screen, "speed"));
            return invoke("vehicle:tool_rect_filled");
        },
        [](const auto&) { return mount_at(-2, -1); },
        [](const auto&) { return mount_at(2, 1); },
        [&](const auto& screen) { return fix(screen, "wheels"); },
        [&](const auto& screen) {
            fixed(screen, "wheels");
            CHECK(step_at(screen) == "Add wheels at the corners");
            CHECK(drive_ok(screen, "steering"));
            return fix(screen, "engine");
        },
        [&](const auto& screen) {
            // Choosing an engine is the player's: the palette shows them, the pencil in hand.
            const auto& groups = body<ec::tabs_body>(part(screen, "vehicle:part_groups")).tabs;
            const auto shown = std::ranges::find(groups, true, &ec::tab_entry::selected);
            REQUIRE(shown != groups.end());
            CHECK(shown->label == "Engines & motors");
            CHECK(picked_tool(screen) == "canvas_tool:install");
            return invoke("vehicle:choose", {.row = "vpart:diesel_engine_i6"});
        },
        [](const auto&) { return mount_at(1, 0); },
        [&](const auto& screen) {
            CHECK(drive_ok(screen, "engine"));
            return fix(screen, "fuel");
        },
        [&](const auto& screen) {
            fixed(screen, "fuel");
            return fix(screen, "battery");
        },
        [&](const auto& screen) {
            fixed(screen, "battery");
            return fix(screen, "controls");
        },
        [&](const auto& screen) {
            fixed(screen, "controls");
            for (const auto& entry : rows(screen, "vehicle:check")) {
                INFO(entry.label);
                CHECK(entry.columns.at(0).value == "✓");
            }
            CHECK(performance_has(screen, "speed"));
            return act("QUIT");
        },
    };
    CHECK(walk(steps) == steps.size());
}

#endif // CATA_MCP
