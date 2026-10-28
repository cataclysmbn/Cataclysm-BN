#if defined(CATA_MCP)

#    include "avatar.h"
#    include "cata_utility.h"
#    include "catch/catch.hpp"
#    include "client_input.h"
#    include "client_interaction.h"
#    include "client_memory.h"
#    include "client_memory_scope.h"
#    include "cursesdef.h"
#    include "cursesport.h"
#    include "editor/editor_session.h"
#    include "editor/recent_documents.h"
#    include "editor/vehicle_design.h"
#    include "editor/vehicle_session.h"
#    include "engine_client_component.h"
#    include "engine_client_contract.h"
#    include "json_formatter.h"
#    include "map/map.h"
#    include "map_helpers.h"
#    include "output.h"
#    include "path_info.h"
#    include "point.h"
#    include "state_helpers.h"
#    include "vehicle/veh_type.h"
#    include "vehicle/vehicle.h"
#    include "vehicle/vpart_position.h"

#    include <algorithm>
#    include <filesystem>
#    include <fstream>
#    include <nlohmann/json.hpp>
#    include <ranges>
#    include <set>
#    include <sstream>
#    include <string>
#    include <variant>

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
                // Its three mounts in a row, and two rings of free mounts around them.
                const auto& grid = body<ec::grid_body>(part(screen, "vehicle:grid"));
                CHECK(grid.cols == 7);
                CHECK(grid.rows == 5);
                CHECK(grid.role == "canvas");
                CHECK(cell(screen, "mount:-1,0").at == ec::grid_point{2, 2});
                REQUIRE(cell(screen, "mount:1,0").looks.size() == 3);
                CHECK(grid.looks.at(cell(screen, "mount:1,0").looks.back()).kind == "vehicle_part");
                CHECK(cell(screen, "mount:2,0").looks.empty());
                CHECK(cell(screen, "mount:0,0").selected);
                CHECK(messages(screen).empty());
                CHECK_FALSE(affordance(screen, "vehicle:undo").enabled);
                CHECK(
                    affordance(screen, "vehicle:undo").keys == std::vector<std::string>{"CTRL+Z"});
                CHECK(
                    affordance(screen, "vehicle:redo").keys == std::vector<std::string>{"CTRL+Y"});
                CHECK(
                    affordance(screen, "vehicle:save").keys == std::vector<std::string>{"CTRL+S"});
                CHECK(refused(state, "vehicle:mount", {.row = "mount:4,0"}));
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
                // A second one has the first in its way.
                return invoke("vehicle:spawn");
            case 10:
                CHECK(body<ec::text_body>(part(screen, "vehicle:notice")).markup
                      == "Something is in the way of the vehicle there.");
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
                CHECK(world[0].columns[0].value == "8");
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
                CHECK(body<ec::text_body>(part(screen, "vehicle:notice")).markup
                      == "Something is in the way of the vehicle there.");
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

#endif

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
                CHECK(std::ranges::all_of(rows, [](const ec::row& row) {
                    return row.look && "vpart:" + row.look->id.value_or("") == row.id;
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
                CHECK(slot_of(screen, "vehicle:parts") == "palette");
                CHECK(part(screen, "vehicle:parts").label == "Parts");
                CHECK_FALSE(std::ranges::contains(tabs(screen), std::string("tool:install")));
                return invoke("vehicle:choose", {.row = "vpart:headlight"});
            case 2:
                return invoke("vehicle:mount", {.row = "mount:1,0"});
            case 3:
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
    CHECK(step >= 5);
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
                CHECK((tools | std::views::transform(&ec::row::id) | std::ranges::to<std::vector>())
                      == std::vector<std::string>{
                          "canvas_tool:select", "canvas_tool:install", "canvas_tool:remove"});
                CHECK(tools.front().selected);
                CHECK((tools | std::views::transform(&ec::row::hotkey)
                       | std::ranges::to<std::vector>())
                      == std::vector<std::string>{"s", "i", "e"});
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
                      == std::vector<std::string>{"i"});
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
                CHECK(row(screen, "vehicle:canvas_tools", "canvas_tool:install").label
                      == "Install headlight");
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
                CHECK(stat(screen, "saved").value == "Yes");
                CHECK(std::filesystem::exists(made));
                return invoke("vehicle:part_filter", {.text = "headlight"});
            case 3:
                return invoke("vehicle:choose", {.row = "vpart:headlight"});
            case 4:
                return invoke("vehicle:mount", {.row = "mount:0,0"});
            case 5:
                // The first part of a vehicle is its frame.
                CHECK(notice_of(screen).find("frame") != std::string::npos);
                return invoke("vehicle:part_filter", {.text = "xlframe_vertical"});
            case 6:
                return invoke("vehicle:choose", {.row = "vpart:xlframe_vertical"});
            case 7:
                return invoke("vehicle:mount", {.row = "mount:0,0"});
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
                // A name alone is a file of that name in the player's mods.
                return invoke("vehicle:new", {.text = "vehicle_editor_new/hand_cart"});
            case 11:
                CHECK(body<ec::panel_body>(part(screen, "vehicle:document")).title == "hand_cart");
                CHECK(std::filesystem::exists(made.parent_path() / "hand_cart.json"));
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
                CHECK(affordance(screen, "vehicle:turn_cw").keys == std::vector<std::string>{"r"});
                CHECK(affordance(screen, "vehicle:turn_ccw").keys == std::vector<std::string>{"R"});
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
                CHECK(part(screen, "vehicle:item_kinds").label == "Items");
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
                CHECK(status(screen) == "Spawned Bicycle 8 squares east of you.");
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
                CHECK(last_step(screen) == "Remove 3 parts");
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
