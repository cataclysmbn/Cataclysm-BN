#if defined(CATA_MCP)

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
                return invoke("vehicle:open", {.row = "prototype:bicycle"});
            case 2: {
                CHECK(body<ec::panel_body>(part(screen, "vehicle:document")).title == "Bicycle");
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
                CHECK(rows(screen, "vehicle:diagnostics").empty());
                CHECK_FALSE(affordance(screen, "vehicle:undo").enabled);
                CHECK(refused(state, "vehicle:mount", {.row = "mount:3,0"}));
                return invoke("vehicle:mount", {.row = "mount:1,0"});
            }
            case 3:
                CHECK(cell(screen, "mount:1,0").selected);
                CHECK(rows(screen, "vehicle:stack").size() == 3);
                return invoke("vehicle:part_filter", {.text = "frame_vertical"});
            case 4:
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
                return invoke("vehicle:field_set", {.row = "pointer", .text = "/name"});
            case 9:
                CHECK(body<ec::form_body>(part(screen, "vehicle:field")).fields[1].value
                      == "Bicycle");
                return invoke("vehicle:field_set", {.row = "value", .text = "Lit bicycle"});
            case 10:
                return invoke("vehicle:field_apply");
            case 11:
                CHECK(
                    body<ec::panel_body>(part(screen, "vehicle:document")).title == "Lit bicycle");
                return invoke("vehicle:remove", {.row = rows(screen, "vehicle:stack").front().id});
            case 12:
                // The frame holds the wheel: the game's reason, and nothing removed.
                CHECK_FALSE(body<ec::text_body>(part(screen, "vehicle:notice")).markup.empty());
                CHECK(rows(screen, "vehicle:stack").size() == 4);
                return invoke("vehicle:save_as", {.text = "/tmp/bicycle.json"});
            case 13:
                CHECK(body<ec::text_body>(part(screen, "vehicle:notice")).markup.find("your mods")
                      != std::string::npos);
                return invoke("vehicle:save_as", {.text = saved_as.string()});
            case 14:
                CHECK(stat(screen, "saved").value == "Yes");
                return act("QUIT");
            default:
                FAIL("the vehicle editor outlived its exit");
                return act("QUIT");
        }
    });

    editor::run_vehicle_editor();
    CHECK(step == 15);
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

#endif
