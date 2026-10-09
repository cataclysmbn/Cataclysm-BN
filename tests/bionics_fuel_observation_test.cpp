#if defined(CATA_MCP) && defined(CATA_BIONICS_FUEL_NATIVE_TEST)

#    include "active_tile_data.h"
#    include "active_tile_data_def.h"
#    include "avatar.h"
#    include "bionics.h"
#    include "bionics_ui.h"
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
#    include "debug.h"
#    include "distribution_grid.h"
#    include "game.h"
#    include "input.h"
#    include "item.h"
#    include "itype.h"
#    include "json.h"
#    include "map/map.h"
#    include "map_helpers.h"
#    include "messages.h"
#    include "options_helpers.h"
#    include "output.h"
#    include "player_helpers.h"
#    include "rng.h"
#    include "state_helpers.h"
#    include "uistate.h"
#    include "vehicle/vehicle.h"

#    include <algorithm>
#    include <memory>
#    include <optional>
#    include <ranges>
#    include <sstream>
#    include <stdexcept>

namespace {
namespace client = game_client;

/// This executable links real messages.cpp, not ordinary fake_messages.cpp.
auto remote_messages() -> std::string {
    auto stream = std::ostringstream{};
    auto json = JsonOut{stream};
    json.start_object();
    Messages::serialize(json);
    json.end_object();
    return stream.str();
}

auto serialized(const auto& value) -> std::string {
    auto stream = std::ostringstream{};
    auto json = JsonOut{stream};
    value.serialize(json);
    return stream.str();
}

struct remote_guard {
    client::memory::scoped_state memory;
    restore_on_out_of_scope<bool> mode{test_mode};
    restore_on_out_of_scope<int> width{TERMX};
    restore_on_out_of_scope<int> height{TERMY};
    restore_on_out_of_scope<int> full_width{FULL_SCREEN_WIDTH};
    restore_on_out_of_scope<int> full_height{FULL_SCREEN_HEIGHT};
    restore_on_out_of_scope<bionic_ui_sort_mode> sort{uistate.bionic_sort_mode};
    restore_on_out_of_scope<time_point> time{calendar::turn};
    restore_on_out_of_scope<cata_default_random_engine> rng{rng_get_engine()};
    override_option slots{"CBM_SLOTS_ENABLED", "true"};
    std::string messages = remote_messages();

    explicit remote_guard(const bool fail_construction = false) {
        test_mode = true;
        clear_all_state();
        clear_character(get_avatar(), false);
        get_avatar().remove_value("rem_battery");
        get_avatar().remove_value("sunlight");
        get_avatar().add_bionic(bionic_id("bio_power_storage"));
        get_avatar().add_bionic(bionic_id("bio_cable"));
        get_avatar().set_power_level(50_kJ);
        get_avatar().set_moves(1000);
        calendar::turn = calendar::turn_zero + 12_hours;
        TERMX = 100;
        TERMY = 40;
        FULL_SCREEN_WIDTH = 80;
        FULL_SCREEN_HEIGHT = 24;
        client::memory::resize(TERMX, TERMY);
        catacurses::stdscr = catacurses::newwin(TERMY, TERMX, point_zero);
        catacurses::newscr = catacurses::newwin(TERMY, TERMX, point_zero);
        if (fail_construction) { throw std::runtime_error("fixture constructor failure"); }
        test_mode = false;
    }

    ~remote_guard() {
        client::memory::set_input_provider({});
        test_mode = true;
        clear_all_state();
        clear_character(get_avatar(), false);
        get_avatar().remove_value("rem_battery");
        get_avatar().remove_value("sunlight");
        auto stream = std::istringstream{messages};
        auto json = JsonIn{stream};
        Messages::deserialize(json.get_object());
    }
};

struct connection_options {
    cable_state remote_state = state_UPS;
    tripoint_abs_ms point = tripoint_abs_ms_min;
    bool character_first = true;
};

auto connect(item& cable, const connection_options& options) -> void {
    auto data = cable_connection_data{cable};
    data.con1 = {.state = state_self};
    data.con2 = {.state = options.remote_state, .point = options.point};
    if (!options.character_first) { std::swap(data.con1, data.con2); }
    cable_connection_data::unset_vars(&cable);
    data.set_vars(&cable);
    cable.activate();
}

auto add_cable() -> item& { // *NOPAD*
    auto& result = get_avatar().i_add(item::spawn("jumper_cable", calendar::turn));
    // A partially unreeled real cable makes native reset charge accounting observable.
    result.charges = result.type->maximum_charges() - 1;
    return result;
}

auto add_ups(const int charges) -> void {
    auto ups = item::spawn("UPS_off", calendar::turn, charges);
    ups->set_var("cable", "plugged_in");
    get_avatar().i_add(std::move(ups));
}

auto activate_remote() -> void {
    auto& you = get_avatar();
    REQUIRE(you.activate_bionic(you.get_bionic_state(bionic_id("bio_cable"))));
    REQUIRE(you.has_active_bionic(bionic_id("bio_cable")));
}

auto resolve_remote(client::input_command command) -> input_event {
    const auto result = client::resolve_input_command(command, client::memory::screen_size());
    REQUIRE(result.has_value());
    return *result;
}

auto cancel_remote() -> input_event {
    auto command = client::input_command{};
    command.action = "QUIT";
    return resolve_remote(std::move(command));
}

/// Core-only evidence: native manager/actions and passive fallback, not an unaccepted adapter.
auto check_native_reads() -> void {
    const auto before = serialized(get_avatar());
    const auto rng = rng_get_engine();
    const auto messages = remote_messages();
    const auto all = client::current_interaction({.limit = 200});
    REQUIRE(all.context == "BIONICS");
    const auto first = client::serialize_interaction(all);
    for (const auto offset : std::views::iota(std::size_t{0}, all.choices.size() + 1)) {
        const auto page = client::current_interaction({.offset = offset, .limit = 1});
        CHECK(page.schema_id == all.schema_id);
        CHECK(page.message == all.message);
    }
    CHECK(client::serialize_interaction(client::current_interaction({.limit = 200})) == first);
    CHECK(serialized(get_avatar()) == before);
    CHECK(rng_get_engine() == rng);
    CHECK(remote_messages() == messages);
}

/// Capturing an expected debug diagnostic does not silence gameplay messages or skip reset logic.
/// The raw native debug prompt is still acknowledged using its documented SPACE control.
auto acknowledge_debug() -> input_event {
    REQUIRE(client::active_input_context().context == nullptr);
    return input_event{' ', input_event_t::keyboard};
}
auto window_copy(const catacurses::window& window) -> std::optional<cata_cursesport::WINDOW> {
    const auto* const native = window.get();
    return native ? std::optional{*native} : std::nullopt;
}

auto check_window(
    const catacurses::window& window, const std::optional<cata_cursesport::WINDOW>& before)
    -> void {
    const auto after = window_copy(window);
    REQUIRE(after.has_value() == before.has_value());
    if (!before) { return; }
    CHECK(after->pos == before->pos);
    CHECK(after->width == before->width);
    CHECK(after->height == before->height);
    CHECK(after->cursor == before->cursor);
    CHECK(after->FG == before->FG);
    CHECK(after->BG == before->BG);
    CHECK(after->inuse == before->inuse);
    CHECK(after->draw == before->draw);
    CHECK(std::ranges::equal(after->line, before->line, [](const auto& a, const auto& b) {
        return a.touched == b.touched && a.chars == b.chars;
    }));
}

} // namespace

TEST_CASE(
    "native bionic fixture restores complete memory state",
    "[bionics_remote_controls][bionics_fixture]") {
    const auto initialized = GENERATE(false, true);
    const auto exit = GENERATE(0, 1, 2); // Normal, body exception, constructor exception.
    CAPTURE(initialized, exit);
    auto outer = std::unique_ptr<remote_guard>{};
    if (initialized) {
        outer = std::make_unique<remote_guard>();
        catacurses::mvwprintw(catacurses::stdscr, point(2, 1), "sentinel");
        client::memory::draw_window(catacurses::stdscr);
        catacurses::wmove(catacurses::newscr, point(3, 2));
    }
    auto presented = 0;
    auto timeout = -2;
    const auto cleanup = on_out_of_scope([&]() {
        client::memory::set_input_provider({});
        client::memory::set_present_callback({});
        client::memory::set_cursor(0);
        client::memory::set_timeout(-1);
    });
    client::memory::set_cursor(1);
    client::memory::set_timeout(17);
    client::memory::set_present_callback([&](const auto& /*screen*/) { ++presented; });
    client::memory::set_input_provider([&](const int value) {
        timeout = value;
        return input_event('x', input_event_t::keyboard);
    });
    const auto before = client::memory::snapshot();
    const auto screen = catacurses::stdscr;
    const auto new_screen = catacurses::newscr;
    const auto screen_before = window_copy(screen);
    const auto new_screen_before = window_copy(new_screen);
    CAPTURE(before.width, before.height);
    auto threw = false;
    try {
        const auto guard = remote_guard(exit == 2);
        catacurses::mvwprintw(catacurses::stdscr, point_zero, "changed");
        client::memory::draw_window(catacurses::stdscr);
        client::memory::set_cursor(0);
        client::memory::set_timeout(98);
        client::memory::set_input_provider({});
        client::memory::set_present_callback({});
        if (exit == 1) { throw std::runtime_error("fixture body failure"); }
    } catch (const std::runtime_error&) { threw = true; }
    CHECK(threw == (exit != 0));
    const auto after = client::memory::snapshot();
    CHECK(after.width == before.width);
    CHECK(after.height == before.height);
    CHECK(after.cursor == before.cursor);
    CHECK(after.cursor_visible == before.cursor_visible);
    CHECK(after.text == before.text);
    CHECK(std::ranges::equal(after.cells, before.cells, [](const auto& a, const auto& b) {
        return a.text == b.text && a.foreground == b.foreground && a.background == b.background;
    }));
    CHECK(catacurses::stdscr == screen);
    CHECK(catacurses::newscr == new_screen);
    check_window(catacurses::stdscr, screen_before);
    check_window(catacurses::newscr, new_screen_before);
    CHECK(client::memory::read_input().get_first_input() == 'x');
    CHECK(timeout == 17);
    CHECK(presented == 1);
}

TEST_CASE(
    "cold bionics manager observation preserves malformed remote cables",
    "[bionics_remote_purity][client][interaction][mcp]") {
    const auto guard = remote_guard{};
    const auto endpoint = GENERATE(state_grid, state_vehicle);
    const auto character_first = GENERATE(true, false);
    CAPTURE(endpoint, character_first);
    auto& you = get_avatar();
    auto& cable = add_cable();
    add_ups(40);
    connect(cable, {});
    activate_remote();
    // Model a real cable with missing saved endpoint coordinates, after explicit native activation.
    connect(cable, {.remote_state = endpoint, .character_first = character_first});
    const auto data = cable_connection_data::make_data(cable);
    REQUIRE(data);
    REQUIRE(data->complete());
    REQUIRE(data->character_connected());
    auto remote = *data;
    REQUIRE_FALSE(remote.get_nonchar_connection()->point_valid());
    const auto before = serialized(you);
    const auto cable_before = serialized(cable);
    const auto rng = rng_get_engine();
    const auto messages = remote_messages();
    const auto moves = you.moves;
    auto reads = 0;
    client::memory::set_input_provider([&](const int /*timeout*/) {
        if (!client::active_input_context().context) { return acknowledge_debug(); }
        // These checks deliberately cover the first actual redraw, BEFORE a passive getter runs.
        CHECK(serialized(you) == before);
        CHECK(serialized(cable) == cable_before);
        CHECK(cable.is_active());
        CHECK(you.moves == moves);
        CHECK(rng_get_engine() == rng);
        CHECK(remote_messages() == messages);
        check_native_reads();
        if (reads++ == 0) {
            auto command = client::input_command{};
            command.action = "NEXT_TAB";
            return resolve_remote(std::move(command));
        }
        return cancel_remote();
    });
    const auto diagnostic = capture_debugmsg_during([&]() { show_bionics_ui(you); });
    CHECK(reads == 2);
    CHECK(diagnostic.empty());
    CHECK(serialized(you) == before);
    CHECK(rng_get_engine() == rng);
    CHECK(remote_messages() == messages);
}

TEST_CASE(
    "explicit remote cable gameplay still cleans up malformed endpoints with feedback",
    "[bionics_remote_controls][mcp]") {
    const auto guard = remote_guard{};
    const auto endpoint = GENERATE(state_grid, state_vehicle);
    const auto character_first = GENERATE(true, false);
    CAPTURE(endpoint, character_first);
    auto& you = get_avatar();
    auto& cable = add_cable();
    connect(cable, {.remote_state = endpoint, .character_first = character_first});
    Messages::clear_messages();
    const auto moves = you.moves;
    const auto maximum = cable.type->maximum_charges();
    client::memory::set_input_provider([](const int /*timeout*/) { return acknowledge_debug(); });
    const auto diagnostic = capture_debugmsg_during([&]() {
        CHECK(you.find_remote_fuel(false).is_empty());
    });
    CHECK(diagnostic.find("Cable_data was not properly initialized") != std::string::npos);
    CHECK_FALSE(cable.is_active());
    CHECK(cable.charges == maximum);
    CHECK(cable_connection_data::make_data(cable)->empty());
    CHECK(you.moves == moves - 10 * maximum);
    const auto feedback = Messages::recent_messages(10);
    REQUIRE(feedback.size() == 2);
    CHECK(feedback[0].second.find("cable has come loose") != std::string::npos);
    CHECK(feedback[1].second.find("reel in the cable") != std::string::npos);
}

TEST_CASE(
    "remote fuel look only gates leave real sources and stored fuel unchanged",
    "[bionics_remote_controls][mcp]") {
    const auto guard = remote_guard{};
    auto& you = get_avatar();
    auto& here = get_map();
    auto& cable = add_cable();
    auto endpoint = state_UPS;
    auto point = tripoint_abs_ms_min;
    auto expected = itype_id{"battery"};
    auto explicit_value = std::string{"40"};
    auto* vehicle_source = static_cast<vehicle*>(nullptr);
    auto* grid_source = static_cast<battery_tile*>(nullptr);
    SECTION("UPS with actual charges") { add_ups(40); }
    SECTION("empty UPS branch") { explicit_value = "0"; }
    SECTION("solar daylight branch") {
        endpoint = state_solar_pack;
        expected = itype_id{"sunlight"};
        explicit_value = "1";
        REQUIRE(here.is_outside(you.bub_pos()));
    }
    SECTION("real grid connector and battery") {
        endpoint = state_grid;
        const auto connector_pos = tripoint_bub_ms{63, 60, 0};
        const auto battery_pos = tripoint_bub_ms{64, 60, 0};
        here.furn_set(connector_pos, furn_str_id("f_cable_connector"));
        here.furn_set(battery_pos, furn_str_id("f_battery"));
        point = map_local_to_abs(here, connector_pos);
        REQUIRE(active_tiles::furn_at<vehicle_connector_tile>(point));
        grid_source = active_tiles::furn_at<battery_tile>(map_local_to_abs(here, battery_pos));
        REQUIRE(grid_source);
        REQUIRE(grid_source->mod_resource(40) == 0);
        auto& grid = get_distribution_grid_tracker().grid_at(point);
        REQUIRE_FALSE(grid.empty());
        REQUIRE(grid.get_resource() == 40);
    }
    SECTION("real vehicle battery") {
        endpoint = state_vehicle;
        const auto position = tripoint_bub_ms{63, 60, 0};
        vehicle_source = here.add_vehicle(vproto_id("car"), position, 0_degrees, 0, 0, false);
        REQUIRE(vehicle_source);
        REQUIRE(vehicle_source->charge_battery(40, false) == 0);
        REQUIRE(vehicle_source->fuel_left(itype_id("battery"), true) == 40);
        point = map_local_to_abs(here, position);
        REQUIRE(here.veh_at(point));
    }
    connect(cable, {.remote_state = endpoint, .point = point});
    activate_remote();
    const auto key = endpoint == state_solar_pack ? "sunlight" : "rem_battery";
    you.set_value(key, "17");
    const auto before = serialized(you);
    const auto messages = remote_messages();
    const auto rng = rng_get_engine();
    const auto vehicle_before = vehicle_source ? serialized(*vehicle_source) : std::string{};
    const auto grid_before = grid_source ? serialized(*grid_source) : std::string{};
    std::ranges::for_each(std::views::iota(0, 3), [&](const auto /*read*/) {
        CHECK(you.find_remote_fuel(true) == expected);
        CHECK(serialized(you) == before);
        CHECK(remote_messages() == messages);
        CHECK(rng_get_engine() == rng);
    });
    auto reads = 0;
    client::memory::set_input_provider([&](const int /*timeout*/) {
        check_native_reads();
        CHECK(serialized(you) == before);
        CHECK(remote_messages() == messages);
        CHECK(rng_get_engine() == rng);
        CHECK(client::memory::snapshot().text.find("Available Fuel") != std::string::npos);
        ++reads;
        return cancel_remote();
    });
    show_bionics_ui(you);
    CHECK(reads == 1);
    if (vehicle_source) { CHECK(serialized(*vehicle_source) == vehicle_before); }
    if (grid_source) { CHECK(serialized(*grid_source) == grid_before); }
    // An explicit native gameplay lookup must still refresh fuel accounting.
    CHECK(you.find_remote_fuel(false) == expected);
    CHECK(you.get_value(key) == explicit_value);
    if (vehicle_source) { CHECK(serialized(*vehicle_source) == vehicle_before); }
    if (grid_source) { CHECK(serialized(*grid_source) == grid_before); }
}

TEST_CASE(
    "incomplete inactive and noncharacter cable connections remain ignored",
    "[bionics_remote_controls][mcp]") {
    const auto guard = remote_guard{};
    auto& you = get_avatar();
    auto& cable = add_cable();
    connect(cable, {.remote_state = state_grid});
    SECTION("inactive malformed cable") { cable.deactivate(); }
    SECTION("incomplete character connection") { cable_connection_data::unset_con2(&cable); }
    SECTION("complete connection without character") { cable.set_var(p1_name, state_UPS); }
    const auto before = serialized(you);
    const auto messages = remote_messages();
    const auto rng = rng_get_engine();
    CHECK(you.find_remote_fuel(true).is_empty());
    CHECK(you.find_remote_fuel(false).is_empty());
    CHECK(serialized(you) == before);
    CHECK(remote_messages() == messages);
    CHECK(rng_get_engine() == rng);
}

#endif
