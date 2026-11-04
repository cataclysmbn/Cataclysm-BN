#if defined(CATA_MCP)

#    include "../src/vehicle/veh_interact.h"
#    include "avatar.h"
#    include "calendar.h"
#    include "catch/catch.hpp"
#    include "client_command.h"
#    include "client_input.h"
#    include "client_interaction.h"
#    include "client_memory.h"
#    include "client_memory_scope.h"
#    include "client_snapshot.h"
#    include "coordinates.h"
#    include "cursesdef.h"
#    include "cursesport.h"
#    include "engine_client_component.h"
#    include "engine_client_contract.h"
#    include "game.h"
#    include "game_constants.h"
#    include "input.h"
#    include "locale_fixture.h"
#    include "locale_scope.h"
#    include "map/map.h"
#    include "map_helpers.h"
#    include "output.h"
#    include "player_activity.h"
#    include "player_helpers.h"
#    include "state_helpers.h"
#    include "units_utility.h"
#    include "vehicle/vehicle.h"
#    include "vehicle/vehicle_part.h"

#    include <algorithm>
#    include <nlohmann/json.hpp>
#    include <ranges>
#    include <string>
#    include <variant>
#    include <vector>

namespace {
namespace ec = engine_client;
using json = nlohmann::json;

struct screen_guard {
    const game_client::memory::scoped_state memory;
    bool old_test_mode = test_mode;
    int old_termx = TERMX;
    int old_termy = TERMY;
    int old_full_screen_width = FULL_SCREEN_WIDTH;
    int old_full_screen_height = FULL_SCREEN_HEIGHT;

    screen_guard() {
        test_mode = false;
        TERMX = 100;
        TERMY = 40;
        FULL_SCREEN_WIDTH = 80;
        FULL_SCREEN_HEIGHT = 24;
        game_client::memory::resize(TERMX, TERMY);
        catacurses::stdscr = catacurses::newwin(TERMY, TERMX, point_zero);
        catacurses::newscr = catacurses::newwin(TERMY, TERMX, point_zero);
    }

    ~screen_guard() {
        TERMX = old_termx;
        TERMY = old_termy;
        FULL_SCREEN_WIDTH = old_full_screen_width;
        FULL_SCREEN_HEIGHT = old_full_screen_height;
        test_mode = old_test_mode;
    }
};

auto captured() -> ec::boundary_state {
    const auto state = ec::capture_boundary(
        {.epoch = "epoch:vehicle",
         .ready =
             {.phase = "waiting_for_input",
              .game_ready = true,
              .accepts_interaction_commands = true,
              .accepts_registered_actions = true}});
    REQUIRE(state.has_value());
    return *state;
}

/// The key a client sends for a registered action.
auto act(const std::string& action) -> input_event {
    auto input = game_client::input_command{};
    input.action = action;
    const auto result =
        game_client::resolve_input_command(input, game_client::memory::screen_size());
    REQUIRE(result.has_value());
    return *result;
}

/// The invoke a client sends for an affordance the screen's loop runs.
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
auto has_part(const ec::screen_view& screen, const std::string& id) -> bool {
    return screen.root && ec::find_component(*screen.root, id) != nullptr;
}
template <typename Body> auto body(const ec::component& value) -> const Body& {
    REQUIRE(std::holds_alternative<Body>(value.body));
    return std::get<Body>(value.body);
}
auto rows(const ec::screen_view& screen, const std::string& id) -> const std::vector<ec::row>& {
    return body<ec::list_body>(part(screen, id)).rows;
}
auto affordance(const ec::screen_view& screen, const std::string& id) -> const ec::affordance& {
    const auto found = std::ranges::find(screen.affordances, id, &ec::affordance::id);
    INFO(id);
    REQUIRE(found != screen.affordances.end());
    return *found;
}
auto stat_of(const ec::screen_view& screen, const std::string& key) -> const ec::panel_stat* {
    const auto& stats = body<ec::panel_body>(part(screen, "vehicle:overview")).stats;
    const auto found = std::ranges::find(stats, key, &ec::panel_stat::key);
    return found == stats.end() ? nullptr : &*found;
}
auto cursor_cell(const ec::screen_view& screen) -> const ec::grid_cell& {
    const auto& grid = body<ec::grid_body>(part(screen, "vehicle:grid"));
    REQUIRE(grid.cursor);
    const auto found = std::ranges::find(grid.cells, *grid.cursor, &ec::grid_cell::at);
    REQUIRE(found != grid.cells.end());
    return *found;
}

/// The top screen as a client reads it, with the ids of this screen instance left out.
auto published() -> json {
    auto top = json::parse(ec::serialize_interaction_boundary(captured()))["stack"].back();
    top.erase("screen_id");
    return top;
}
auto without_words(json value) -> json {
    if (value.is_string()) { return ""; }
    if (!value.is_structured()) { return value; }
    for (auto& member : value) { member = without_words(std::move(member)); }
    return value;
}
void collect_ids(const json& value, std::vector<std::string>& into) {
    if (value.is_object()) {
        for (const auto& [name, member] : value.items()) {
            if ((name == "id" || name == "affordance") && member.is_string()) {
                into.push_back(member.get<std::string>());
            }
            collect_ids(member, into);
        }
    } else if (value.is_array()) {
        for (const auto& member : value) { collect_ids(member, into); }
    }
}


/// The composed terminal row `y` as text, and where each character starts among the row's cells.
struct composed_row {
    std::string text;
    std::vector<std::size_t> starts;
};
auto composed(const game_client::screen_snapshot& shot, const int y) -> composed_row {
    auto row = composed_row{};
    for (auto x = 0; x < shot.width; ++x) {
        const auto& cell = shot.cells[static_cast<std::size_t>(y * shot.width + x)];
        row.starts.push_back(row.text.size());
        row.text += cell.text;
    }
    return row;
}
/// Where `needle` shows on the composed screen: its row, and the cell it starts at; none when
/// absent.
struct shown_at {
    int y = -1;
    int x = -1;
};
auto find_shown(const game_client::screen_snapshot& shot, const std::string& needle) -> shown_at {
    for (auto y = 0; y < shot.height; ++y) {
        const auto row = composed(shot, y);
        const auto found = row.text.find(needle);
        if (found == std::string::npos) { continue; }
        const auto cell = std::ranges::find(row.starts, found);
        return {.y = y, .x = static_cast<int>(cell - row.starts.begin())};
    }
    return {};
}
auto foreground_at(const game_client::screen_snapshot& shot, const shown_at at, const int dx = 0)
    -> int {
    return shot.cells[static_cast<std::size_t>(at.y * shot.width + at.x + dx)].foreground;
}

struct bicycle_fixture {
    // The state is cleared before the guard turns off test mode: clearing eats, which asks.
    const bool cleared = []() {
        clear_all_state();
        return true;
    }();
    screen_guard guard;
    vehicle* veh = nullptr;

    bicycle_fixture() {
        set_time(calendar::turn_zero + 12_hours);
        const auto origin = tripoint_bub_ms(60, 60, 0);
        get_avatar().setpos(origin + point_south);
        veh = get_map().add_vehicle(vproto_id("bicycle"), origin, 0_degrees, 0, 0);
        REQUIRE(veh != nullptr);
    }
};

} // namespace

TEST_CASE(
    "the vehicle screen is driven by its grid, lists, tabs and the actions of its context",
    "[vehicle_screen][engine_client_component]") {
    auto fixture = bicycle_fixture{};
    auto step = 0;
    auto cursor = std::string{};
    auto all_parts = std::size_t{0};

    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto state = captured();
        const auto screen = ec::top_screen(state);
        REQUIRE(screen.root);
        CHECK(screen.context == "VEH_INTERACT");
        switch (step++) {
            case 0: {
                // The main screen: the vehicle's figures, the grid around the cursor, the parts on
                // it.
                CHECK(screen.title == fixture.veh->name);
                // The vehicle screen covers the map natively, so it is a screen, not a dialog.
                CHECK(screen.role == "screen");
                // The window already says the vehicle's name: its overview pane does not repeat it.
                CHECK(body<ec::panel_body>(part(screen, "vehicle:overview")).title.empty());
                REQUIRE(stat_of(screen, "owner") != nullptr);
                CHECK(stat_of(screen, "owner")->tone == "good");
                REQUIRE(stat_of(screen, "mass") != nullptr);
                CHECK(stat_of(screen, "mass")->unit == weight_units());
                CHECK(stat_of(screen, "cargo_volume")->max.empty() == false);
                CHECK_FALSE(rows(screen, "vehicle:parts").empty());
                CHECK_FALSE(has_part(screen, "vehicle:choices"));
                CHECK(cursor_cell(screen).selected);
                CHECK_FALSE(cursor_cell(screen).glyph.empty());
                cursor = cursor_cell(screen).id;
                // Every action the context binds is an affordance; one that cannot run says why.
                CHECK(affordance(screen, "INSTALL").enabled);
                CHECK(affordance(screen, "INSTALL").denial.empty());
                CHECK_FALSE(affordance(screen, "REPAIR").enabled);
                CHECK(affordance(screen, "REPAIR").denial
                      == "There are no damaged parts on this vehicle.");
                CHECK(affordance(screen, "QUIT").role == "cancel");
                CHECK_FALSE(ec::resolve_invoke(state, {.affordance_id = "REPAIR"}).has_value());
                return act("INSTALL");
            }
            case 1: {
                // The install list under its tabs, with the preview of the highlighted part beside
                // it.
                // The window asks for a part now, so the overview says whose vehicle it is.
                CHECK(screen.title != fixture.veh->name);
                CHECK(body<ec::panel_body>(part(screen, "vehicle:overview")).title
                      == fixture.veh->name);
                const auto& tabs = body<ec::tabs_body>(part(screen, "vehicle:tabs"));
                REQUIRE(tabs.tabs.size() == 8);
                CHECK(tabs.tabs.front().id == "all");
                CHECK(tabs.tabs.front().selected);
                CHECK(tabs.select == "vehicle:tab");
                const auto& choices = rows(screen, "vehicle:choices");
                REQUIRE(choices.size() > 2);
                CHECK(choices.front().id.starts_with("vpart:"));
                CHECK(screen.focus_row == choices.front().id);
                CHECK(screen.focus_component == "vehicle:choices");
                all_parts = choices.size();
                const auto& detail = part(screen, "vehicle:detail");
                const auto& stats = body<ec::panel_body>(detail).stats;
                CHECK(body<ec::panel_body>(detail).title
                      == body<ec::list_body>(part(screen, "vehicle:choices")).rows.front().label);
                REQUIRE_FALSE(stats.empty());
                CHECK(stats.front().key == "time");
                const auto& needs = rows(screen, "vehicle:requirements");
                CHECK(needs.front().header);
                CHECK(std::ranges::any_of(needs, [](const auto& need) {
                    return need.id.starts_with("requirement:") && !need.header;
                }));
                // Without the materials the part cannot be installed, and the confirm says why.
                CHECK_FALSE(affordance(screen, "CONFIRM").enabled);
                CHECK_FALSE(affordance(screen, "CONFIRM").denial.empty());
                // The tab is picked by its identity, and an unknown one is refused.
                CHECK(
                    ec::resolve_invoke(
                        state, {.affordance_id = "vehicle:tab", .args = {.row = "nowhere"}})
                        .error()
                    == ec::error::invalid_arguments);
                return invoke("vehicle:tab", {.row = "cargo"});
            }
            case 2: {
                const auto& tabs = body<ec::tabs_body>(part(screen, "vehicle:tabs"));
                CHECK(tabs.tabs[1].selected);
                CHECK_FALSE(tabs.tabs.front().selected);
                CHECK(rows(screen, "vehicle:choices").size() < all_parts);
                return invoke("vehicle:tab", {.row = "all"});
            }
            case 3: {
                // Picking a row other than the highlighted one moves the highlight only.
                const auto& choices = rows(screen, "vehicle:choices");
                REQUIRE(choices.size() == all_parts);
                CHECK(screen.focus_row == choices.front().id);
                CHECK(
                    ec::resolve_invoke(
                        state, {.affordance_id = "vehicle:pick", .args = {.row = "vpart:nothing"}})
                        .error()
                    == ec::error::invalid_arguments);
                return invoke("vehicle:pick", {.row = choices[1].id});
            }
            case 4:
                CHECK(screen.focus_row == rows(screen, "vehicle:choices")[1].id);
                CHECK(has_part(screen, "vehicle:tabs"));
                return act("QUIT");
            case 5:
                CHECK_FALSE(has_part(screen, "vehicle:choices"));
                CHECK_FALSE(has_part(screen, "vehicle:detail"));
                return act("REMOVE");
            case 6: {
                // Removing needs a tool the character lacks, which the requirement rows say.
                CHECK_FALSE(has_part(screen, "vehicle:tabs"));
                CHECK(
                    std::ranges::any_of(rows(screen, "vehicle:requirements"), [](const auto& need) {
                        return need.color == "c_light_red";
                    }));
                CHECK_FALSE(affordance(screen, "CONFIRM").enabled);
                CHECK_FALSE(affordance(screen, "REMOVE").enabled);
                return act("QUIT");
            }
            case 7:
                CHECK_FALSE(has_part(screen, "vehicle:choices"));
                return act("RIGHT");
            case 8:
                CHECK(cursor_cell(screen).id != cursor);
                return act("QUIT");
            default:
                FAIL("the vehicle screen outlived its cancel");
                return act("QUIT");
        }
    });

    static_cast<void>(veh_interact::run(*fixture.veh, tripoint_mnt_veh::zero()));
    CHECK(step == 9);
}

TEST_CASE(
    "the vehicle screen is the same at any viewport size and has the same ids in another language",
    "[vehicle_screen][engine_client_component]") {
    auto fixture = bicycle_fixture{};
    const auto fixture_ko = locale_fixture::korean_catalogue{};
    auto reads = 0;
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        ++reads;
        const auto saved_x = TERMX;
        const auto saved_y = TERMY;
        TERMX = 80;
        TERMY = 24;
        const auto narrow = published();
        TERMX = 200;
        TERMY = 60;
        const auto wide = published();
        TERMX = saved_x;
        TERMY = saved_y;
        CHECK(narrow.dump() == wide.dump());

        const auto korean = [] {
            const auto scope = localization::locale_scope(locale_fixture::korean);
            return published();
        }();
        auto english_ids = std::vector<std::string>{};
        auto korean_ids = std::vector<std::string>{};
        collect_ids(narrow, english_ids);
        collect_ids(korean, korean_ids);
        CHECK(english_ids == korean_ids);
        CHECK(without_words(narrow) == without_words(korean));
        return act("QUIT");
    });
    static_cast<void>(veh_interact::run(*fixture.veh, tripoint_mnt_veh::zero()));
    CHECK(reads == 1);
}

TEST_CASE(
    "the vehicle grid names what each square shows by look, each look once",
    "[vehicle_screen][engine_client_component]") {
    auto fixture = bicycle_fixture{};
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto screen = ec::top_screen(captured());
        const auto& grid = body<ec::grid_body>(part(screen, "vehicle:grid"));
        for (const auto& cell : grid.cells) {
            REQUIRE_FALSE(cell.looks.empty());
            for (const auto index : cell.looks) {
                REQUIRE(index < static_cast<int>(grid.looks.size()));
                CHECK(grid.looks[index].kind == "vehicle_part");
            }
        }
        for (const auto& look : grid.looks) { CHECK(std::ranges::count(grid.looks, look) == 1); }
        return act("QUIT");
    });
    static_cast<void>(veh_interact::run(*fixture.veh, tripoint_mnt_veh::zero()));
}

TEST_CASE(
    "the vehicle screen publishes no window text and no native stat line",
    "[vehicle_screen][engine_client_component]") {
    auto fixture = bicycle_fixture{};
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto screen = ec::top_screen(captured());
        const auto& stats = body<ec::panel_body>(part(screen, "vehicle:overview")).stats;
        // Figures are values with their unit, not lines with color tags and padding.
        for (const auto& stat : stats) {
            CHECK(stat.value.find("<color") == std::string::npos);
            CHECK(stat.label.find(':') == std::string::npos);
            CHECK_FALSE(stat.key.empty());
        }
        // A name is plain; its tags ride in the row's markup.
        for (const auto& row : rows(screen, "vehicle:parts")) {
            CHECK(row.label.find("<color") == std::string::npos);
        }
        for (const auto& cell : body<ec::grid_body>(part(screen, "vehicle:grid")).cells) {
            CHECK(cell.label.find("<color") == std::string::npos);
        }
        const auto mass = std::ranges::find(stats, "mass", &ec::panel_stat::key);
        REQUIRE(mass != stats.end());
        CHECK(mass->value.find(' ') == std::string::npos);
        return act("QUIT");
    });
    static_cast<void>(veh_interact::run(*fixture.veh, tripoint_mnt_veh::zero()));
}

TEST_CASE(
    "a vehicle part row names the part without the health bar its column shows",
    "[vehicle_screen][engine_client_component]") {
    auto fixture = bicycle_fixture{};
    const auto names =
        std::views::iota(0, fixture.veh->part_count())
        | std::views::transform([&](const int index) {
              return remove_color_tags(fixture.veh->part(index).name(false));
          })
        | std::ranges::to<std::vector>();
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto screen = ec::top_screen(captured());
        const auto& all = rows(screen, "vehicle:parts");
        REQUIRE_FALSE(all.empty());
        for (const auto& row : all) {
            CAPTURE(row.label);
            CHECK(std::ranges::any_of(names, [&](const std::string& name) {
                return row.label.starts_with(name);
            }));
        }
        return act("QUIT");
    });
    static_cast<void>(veh_interact::run(*fixture.veh, tripoint_mnt_veh::zero()));
}

TEST_CASE(
    "the native stats window keeps its blank slots, its padding and its colors",
    "[vehicle_screen]") {
    auto fixture = bicycle_fixture{};
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto shot = game_client::memory::snapshot();
        const auto at = [&](const std::string& line) {
            const auto found = find_shown(shot, line);
            INFO(line);
            REQUIRE(found.y >= 0);
            return found;
        };
        // Eight slots to a column. A bicycle is no boat, so two slots stay empty after the speeds
        // and the damage lines leave two empty ones at the top of the second column.
        const auto speed = at("Safe/Top Speed:   0/  0 km/h");
        CHECK(at("Acceleration:   0 km/h/s").y == speed.y + 1);
        const auto mass = at("Mass:    21 kg");
        CHECK(mass.y == speed.y + 4);
        CHECK(at("Cargo Volume: 0.00 / 20.00 L").y == speed.y + 5);
        CHECK(at("Status: ").y == speed.y + 6);
        CHECK(at("Wheels: enough").y == speed.y + 7);
        const auto air = at("Air drag:        0.61");
        CHECK(air.y == speed.y + 2);
        CHECK(at("Rolling drag:    0.02").y == mass.y);
        CHECK(at("Static drag:        0").y == mass.y + 1);
        CHECK(at("Offroad:         100%").y == mass.y + 2);
        CHECK(air.x == mass.x + 34);
        // Labels are plain, the safe speed is green, the top speed red, the other figures blue, a
        // wheel setup that suffices light green and the status in the color of its health.
        const auto light_gray = 7;
        const auto green = 2;
        const auto light_green = 10;
        const auto light_red = 9;
        const auto light_blue = 12;
        CHECK(foreground_at(shot, speed) == light_gray);
        CHECK(foreground_at(shot, speed, 18) == light_green);
        CHECK(foreground_at(shot, speed, 22) == light_red);
        CHECK(foreground_at(shot, mass) == light_gray);
        CHECK(foreground_at(shot, mass, 10) == light_blue);
        CHECK(foreground_at(shot, air, 20) == light_blue);
        CHECK(foreground_at(shot, at("Wheels: enough"), 8) == light_green);
        CHECK(foreground_at(shot, at("Status: "), 8) == green);
        return act("QUIT");
    });
    static_cast<void>(veh_interact::run(*fixture.veh, tripoint_mnt_veh::zero()));
}

#endif // CATA_MCP
