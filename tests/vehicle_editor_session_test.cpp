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
/// What the game said building the open prototype, as the document panel lists it.
auto messages(const ec::screen_view& screen) -> std::vector<ec::panel_stat> {
    return body<ec::panel_body>(part(screen, "vehicle:document")).stats
         | std::views::filter([](const ec::panel_stat& entry) {
               return entry.key.starts_with("message:");
           })
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
                CHECK_FALSE(has_affordance(screen, "vehicle:install"));
                CHECK(affordance(screen, "QUIT").role == "cancel");
                return invoke("vehicle:filter", {.text = "bicycle"});
            case 1:
                CHECK(row(screen, "vehicle:prototypes", "prototype:bicycle").label == "Bicycle");
                CHECK(body<ec::list_body>(part(screen, "vehicle:prototypes")).filter == "bicycle");
                CHECK(std::ranges::contains(
                    part(screen, "vehicle:prototypes").affordances, std::string("vehicle:filter")));
                return invoke("vehicle:open", {.row = "prototype:bicycle"});
            case 2: {
                CHECK(body<ec::panel_body>(part(screen, "vehicle:document")).title == "Bicycle");
                // The file as a modder finds it in the game's directory.
                CHECK(body<ec::panel_body>(part(screen, "vehicle:document"))
                          .subtitle.starts_with("data/json/vehicles/"));
                CHECK(stat(screen, "saved").value == "Yes");
                CHECK(std::stoi(stat(screen, "mass").value) > 0);
                // Its three mounts in a row, and a ring of free mounts around them.
                const auto& grid = body<ec::grid_body>(part(screen, "vehicle:grid"));
                CHECK(grid.cols == 5);
                CHECK(grid.rows == 3);
                CHECK(grid.role == "canvas");
                CHECK(cell(screen, "mount:-1,0").at == ec::grid_point{1, 1});
                REQUIRE(cell(screen, "mount:1,0").looks.size() == 1);
                CHECK(
                    grid.looks.at(cell(screen, "mount:1,0").looks.front()).kind == "vehicle_part");
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
                CHECK(refused(state, "vehicle:mount", {.row = "mount:3,0"}));
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
                // A second frame is not mountable here, as the game says.
                CHECK_FALSE(row(screen, "vehicle:parts", "vpart:xlframe_vertical").enabled);
                CHECK_FALSE(row(screen, "vehicle:parts", "vpart:xlframe_vertical").denial.empty());
                return invoke("vehicle:part_filter", {.text = "headlight"});
            case 5:
                CHECK(row(screen, "vehicle:parts", "vpart:headlight").enabled);
                return invoke("vehicle:install", {.row = "vpart:headlight"});
            case 6:
                CHECK(rows(screen, "vehicle:stack").size() == 4);
                CHECK(rows(screen, "vehicle:stack").back().label == "headlight");
                CHECK(stat(screen, "saved").value == "No");
                CHECK(affordance(screen, "vehicle:undo").enabled);
                return invoke("vehicle:undo");
            case 7:
                CHECK(rows(screen, "vehicle:stack").size() == 3);
                CHECK(stat(screen, "saved").value == "Yes");
                return invoke("vehicle:redo");
            case 8:
                CHECK(rows(screen, "vehicle:stack").size() == 4);
                // The raw form of the JSON is a tab of its own, out of the way, and so is what
                // sets it.
                CHECK_FALSE(has_affordance(screen, "vehicle:field_set"));
                return invoke("vehicle:tool", {.row = "tool:json"});
            case 9:
                return invoke("vehicle:field_set", {.row = "pointer", .text = "/name"});
            case 10:
                CHECK(body<ec::form_body>(part(screen, "vehicle:field")).fields[1].value
                      == "Bicycle");
                return invoke("vehicle:field_set", {.row = "value", .text = "Lit bicycle"});
            case 11:
                return invoke("vehicle:field_apply");
            case 12:
                CHECK(
                    body<ec::panel_body>(part(screen, "vehicle:document")).title == "Lit bicycle");
                return invoke("vehicle:tool", {.row = "tool:mount"});
            case 13:
                return invoke("vehicle:remove", {.row = rows(screen, "vehicle:stack").front().id});
            case 14:
                // The frame holds the wheel: the game's reason, and nothing removed.
                CHECK_FALSE(body<ec::text_body>(part(screen, "vehicle:notice")).markup.empty());
                CHECK(rows(screen, "vehicle:stack").size() == 4);
                return invoke("vehicle:save_as", {.text = "/tmp/bicycle.json"});
            case 15:
                CHECK(body<ec::text_body>(part(screen, "vehicle:notice")).markup.find("your mods")
                      != std::string::npos);
                return invoke("vehicle:save_as", {.text = saved_as.string()});
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
                return invoke("vehicle:mount", {.row = "mount:1,0"});
            case 6:
                return invoke("vehicle:part_filter", {.text = "headlight"});
            case 7:
                return invoke("vehicle:install", {.row = "vpart:headlight"});
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
                return invoke("vehicle:install", {.row = "vpart:headlight"});
            case 4:
                CHECK(labels(screen).size() == plain.size() + 1);
                CHECK(affordance(screen, "vehicle:undo").enabled);
                return invoke("vehicle:pick", {.row = light(screen).id});
            case 5:
                CHECK(light(screen).selected);
                return invoke("vehicle:tool", {.row = "tool:part"});
            case 6:
                return invoke("vehicle:live_set", {.row = "name", .text = "Mine"});
            case 7:
                CHECK(title(screen) == "Mine");
                return invoke("vehicle:live_set", {.row = "health", .text = "50"});
            case 8:
                return invoke("vehicle:install", {.row = "vpart:xlframe_vertical"});
            case 9:
                CHECK(body<ec::text_body>(part(screen, "vehicle:notice"))
                          .markup.find("cannot be mounted")
                      != std::string::npos);
                // A refused install leaves the mount's parts in view.
                CHECK(light(screen).columns[0].value == "50");
                return invoke("vehicle:undo");
            case 10:
                CHECK(light(screen).columns[0].value == "100");
                return invoke("vehicle:undo");
            case 11:
                CHECK(title(screen) == "Bicycle");
                return invoke("vehicle:undo");
            case 12:
                CHECK(labels(screen) == plain);
                CHECK(rows(screen, "vehicle:world").size() == 1);
                CHECK(affordance(screen, "vehicle:redo").enabled);
                return invoke("vehicle:redo");
            case 13:
                CHECK(labels(screen).size() == plain.size() + 1);
                return invoke("vehicle:done");
            case 14:
                CHECK(ec::find_component(*screen.root, "vehicle:live") == nullptr);
                return act("QUIT");
            default:
                FAIL("the vehicle editor outlived its exit");
                return act("QUIT");
        }
    });
    editor::run_vehicle_editor();
    CHECK(step == 15);
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
                return invoke("vehicle:earlier", {.row = "part:2"});
            case 3:
                CHECK(labels(screen)
                      == std::vector<std::string>{before[0], before[2], before[1], before[3]});
                return invoke("vehicle:later", {.row = "part:3"});
            case 4:
                CHECK(body<ec::text_body>(part(screen, "vehicle:notice")).markup
                      == "The part is already at that end of its mount.");
                return invoke("vehicle:undo");
            case 5:
                CHECK(labels(screen) == before);
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
        const auto found =
            std::ranges::find(stack, std::string("/parts/18/parts/2"), &ec::row::description);
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
        const auto found =
            std::ranges::find(stack, std::string("/parts/0/parts/3"), &ec::row::description);
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

#endif
