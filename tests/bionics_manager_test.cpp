#if defined(CATA_MCP)

#    include "avatar.h"
#    include "bionics.h"
#    include "bionics_ui.h"
#    include "cata_utility.h"
#    include "catalua_icallback_actor.h"
#    include "catalua_impl.h"
#    include "catch/catch.hpp"
#    include "client_command.h"
#    include "client_input.h"
#    include "client_memory.h"
#    include "client_memory_scope.h"
#    include "cursesdef.h"
#    include "engine_client_component.h"
#    include "engine_client_contract.h"
#    include "init.h"
#    include "input.h"
#    include "json.h"
#    include "locale_fixture.h"
#    include "locale_scope.h"
#    include "messages.h"
#    include "options.h"
#    include "options_helpers.h"
#    include "output.h"
#    include "player_helpers.h"
#    include "rng.h"
#    include "state_helpers.h"
#    include "uistate.h"

#    include <algorithm>
#    include <array>
#    include <nlohmann/json.hpp>
#    include <ranges>
#    include <sstream>
#    include <stdexcept>

namespace {
namespace client = game_client;
namespace ec = engine_client;
using json = nlohmann::json;

auto message_state() -> std::string {
    auto stream = std::ostringstream{};
    auto json = JsonOut{stream};
    json.start_object();
    Messages::serialize(json);
    json.end_object();
    return stream.str();
}

struct manager_guard {
    client::memory::scoped_state memory;
    restore_on_out_of_scope<bool> mode{test_mode};
    restore_on_out_of_scope<int> width{TERMX};
    restore_on_out_of_scope<int> height{TERMY};
    restore_on_out_of_scope<int> full_width{FULL_SCREEN_WIDTH};
    restore_on_out_of_scope<int> full_height{FULL_SCREEN_HEIGHT};
    bionic_ui_sort_mode old_sort = uistate.bionic_sort_mode;
    std::string old_messages = message_state();
    override_option slots{"CBM_SLOTS_ENABLED", "true"};

    manager_guard() {
        test_mode = true;
        clear_all_state();
        clear_character(get_avatar(), false);
        get_avatar().remove_value("battery");
        get_avatar().remove_value("test_bionics_manager_callback");
        get_avatar().add_bionic(bionic_id("bio_power_storage"));
        get_avatar().set_power_level(50_kJ);
        get_avatar().set_moves(1000);
        uistate.bionic_sort_mode = bionic_ui_sort_mode::NONE;
        TERMX = 100;
        TERMY = 40;
        FULL_SCREEN_WIDTH = 80;
        FULL_SCREEN_HEIGHT = 24;
        client::memory::resize(TERMX, TERMY);
        catacurses::stdscr = catacurses::newwin(TERMY, TERMX, point_zero);
        catacurses::newscr = catacurses::newwin(TERMY, TERMX, point_zero);
        test_mode = false;
    }

    ~manager_guard() {
        client::memory::set_input_provider({});
        test_mode = true;
        clear_all_state();
        clear_character(get_avatar(), false);
        get_avatar().remove_value("battery");
        get_avatar().remove_value("test_bionics_manager_callback");
        uistate.bionic_sort_mode = old_sort;
        auto stream = std::istringstream{old_messages};
        auto json = JsonIn{stream};
        Messages::deserialize(json.get_object());
    }
};

auto actor_state() -> std::string {
    auto stream = std::ostringstream{};
    auto json = JsonOut{stream};
    get_avatar().serialize(json);
    return stream.str();
}

auto id(const std::string& native_id) -> std::string { return "bionic:" + native_id; }

auto resolve(client::interaction_command command) -> input_event {
    auto request = client::input_command{};
    request.interaction = std::move(command);
    const auto result = client::resolve_input_command(request, client::memory::screen_size());
    REQUIRE(result.has_value());
    return *result;
}

auto action(const std::string& name) -> input_event {
    auto request = client::input_command{};
    request.action = name;
    const auto result = client::resolve_input_command(request, client::memory::screen_size());
    REQUIRE(result.has_value());
    return *result;
}

/// The invoke a client sends for an affordance the screen's loop runs.
auto invoke(const std::string& affordance, client::interaction_arguments args = {}) -> input_event {
    auto event = input_event{};
    event.type = input_event_t::interaction;
    event.interaction = client::interaction_event{
        .operation = client::interaction_operation::invoke,
        .target_id = affordance,
        .arguments = std::move(args)};
    return event;
}

/// Cancels a nested native popup, which only the 0.1 snapshot describes.
auto cancel(const client::interaction_snapshot& snapshot) -> input_event {
    return resolve(
        {.input_id = snapshot.input_id, .operation = client::interaction_operation::cancel});
}

auto captured() -> ec::boundary_state {
    const auto state = ec::capture_boundary(
        {.epoch = "epoch:bionics",
         .ready =
             {.phase = "waiting_for_input",
              .game_ready = true,
              .accepts_interaction_commands = true,
              .accepts_registered_actions = true}});
    REQUIRE(state.has_value());
    return *state;
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
auto rows(const ec::screen_view& screen) -> const std::vector<ec::row>& {
    return body<ec::list_body>(part(screen, "bionics:list")).rows;
}
auto row_of(const ec::screen_view& screen, const std::string& row_id) -> const ec::row& {
    const auto& all = rows(screen);
    const auto found = std::ranges::find(all, row_id, &ec::row::id);
    INFO(row_id);
    REQUIRE(found != all.end());
    return *found;
}
auto text_of(const ec::screen_view& screen, const std::string& component) -> const std::string& {
    return body<ec::text_body>(part(screen, component)).markup;
}
auto column_of(const ec::row& row, const std::string& key) -> const std::string& {
    const auto found = std::ranges::find(row.columns, key, &client::interaction_column::key);
    REQUIRE(found != row.columns.end());
    return found->value;
}
auto denial_of(const ec::screen_view& screen, const std::string& action_id) -> std::string {
    const auto found = std::ranges::find(screen.denials, action_id, &ec::denial::id);
    return found == screen.denials.end() ? std::string{} : found->reason;
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

/// The screen a nested native popup leaves under it is still the bionics screen.
auto is_bionics() -> bool { return client::current_interaction().context == "BIONICS"; }
} // namespace


TEST_CASE(
    "the bionics screen publishes a row per bionic and describes only the one examined",
    "[bionics_manager][engine_client_component][mcp]") {
    const auto guard = manager_guard{};
    auto& you = get_avatar();
    const auto installed =
        std::array{"bio_flashlight", "bio_hydraulics", "bio_tools", "bio_batteries", "bio_cable"};
    for (const auto* native_id : installed) {
        const auto bio = bionic_id{native_id};
        REQUIRE(bio->activated);
        you.add_bionic(bio);
    }
    const auto before = actor_state();
    const auto rng = rng_get_engine();
    const auto messages = message_state();
    auto polls = 0;
    client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto state = captured();
        const auto screen = ec::top_screen(state);
        CHECK(screen.context == "BIONICS");
        CHECK(actor_state() == before);
        CHECK(rng_get_engine() == rng);
        CHECK(message_state() == messages);
        switch (polls++) {
            case 0: {
                REQUIRE(rows(screen).size() == installed.size());
                CHECK_FALSE(has_part(screen, "bionics:description"));
                CHECK(screen.focus_component == "bionics:list");
                CHECK(screen.focus_row == rows(screen).front().id);
                const auto& tabs = body<ec::tabs_body>(part(screen, "bionics:tabs"));
                CHECK(tabs.select == "bionics:tab");
                REQUIRE(tabs.tabs.size() == 2);
                CHECK(tabs.tabs.front().id == "active");
                CHECK(tabs.tabs.front().selected);
                for (const auto& row : rows(screen)) {
                    CHECK(row.id.starts_with("bionic:"));
                    CHECK(row.description.empty());
                    CHECK_FALSE(row.columns.empty());
                }
                const auto& power = body<ec::panel_body>(part(screen, "bionics:status")).stats;
                REQUIRE(power.size() == 1);
                CHECK(power.front().key == "power");
                CHECK(power.front().max
                      == std::to_string(units::to_kilojoule(you.get_max_power_level())));
                return action("TOGGLE_EXAMINE");
            }
            case 1:
                CHECK(text_of(screen, "bionics:description")
                          .find(bionic_id(installed[0])->description.translated())
                      != std::string::npos);
                return action("DOWN");
            case 2:
                CHECK(screen.focus_row == rows(screen)[1].id);
                CHECK(text_of(screen, "bionics:description")
                          .find(bionic_id(installed[1])->description.translated())
                      != std::string::npos);
                return action("QUIT");
            default:
                FAIL("the bionics screen outlived its cancel");
                return action("QUIT");
        }
    });
    show_bionics_ui(you);
    CHECK(polls == 3);
    CHECK(actor_state() == before);
    CHECK(rng_get_engine() == rng);
    CHECK(message_state() == messages);
}

TEST_CASE(
    "the bionics screen reads are pure and its pick activates with real power and effects",
    "[bionics_manager][engine_client_component][mcp]") {
    const auto guard = manager_guard{};
    auto& you = get_avatar();
    you.add_bionic(bionic_id("bio_flashlight"));
    you.add_bionic(bionic_id("bio_carbon"));
    const auto power = you.get_power_level();
    const auto cost = bionic_id("bio_flashlight")->power_activate;
    const auto light = you.active_light();
    auto step = 0;
    client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto before = actor_state();
        const auto state = captured();
        const auto screen = ec::top_screen(state);
        CHECK(actor_state() == before);
        REQUIRE(screen.context == "BIONICS");
        if (step++ == 0) {
            REQUIRE(rows(screen).size() == 1);
            CHECK(rows(screen).front().id == id("bio_flashlight"));
            // A bionic of the other tab is not a row of this one.
            CHECK(
                ec::resolve_invoke(
                    state, {.affordance_id = "bionics:pick", .args = {.row = id("bio_carbon")}})
                    .error()
                == ec::error::invalid_arguments);
            return invoke("bionics:pick", {.row = id("bio_flashlight")});
        }
        CHECK(you.has_active_bionic(bionic_id("bio_flashlight")));
        CHECK(you.get_power_level() == power - cost);
        CHECK(you.active_light() > light);
        return action("QUIT");
    });
    show_bionics_ui(you);
    CHECK(step == 2);
    CHECK(you.has_active_bionic(bionic_id("bio_flashlight")));
}

TEST_CASE(
    "the bionics screen keeps the native power fuel and incapacitation denials",
    "[bionics_manager][engine_client_component][mcp]") {
    const auto guard = manager_guard{};
    auto& you = get_avatar();
    auto target = std::string{"bio_flashlight"};
    auto disabled = true;
    SECTION("power") { you.set_power_level(0_J); }
    SECTION("fuel") {
        target = "bio_batteries";
        disabled = false;
    }
    SECTION("incapacitated") {}
    you.add_bionic(bionic_id(target));
    if (target == "bio_flashlight" && you.get_power_level() > 0_J) {
        you.get_bionic_state(bionic_id(target)).incapacitated_time = 1_hours;
    }
    const auto power = you.get_power_level();
    auto step = 0;
    client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto screen = ec::top_screen(captured());
        REQUIRE(screen.context == "BIONICS");
        if (step++ == 0) {
            REQUIRE(rows(screen).size() == 1);
            CHECK(rows(screen).front().enabled == !disabled);
            CHECK(rows(screen).front().denial.empty() == !disabled);
            CHECK(denial_of(screen, "CONFIRM") == rows(screen).front().denial);
            Messages::clear_messages();
            return invoke("bionics:pick", {.row = id(target)});
        }
        CHECK_FALSE(you.has_active_bionic(bionic_id(target)));
        CHECK(you.get_power_level() == power);
#    if defined(CATA_BIONICS_NATIVE_TEST)
        // Ordinary tests deliberately link fake_messages.cpp; the private native harness does not.
        const auto feedback = Messages::recent_messages(10);
        REQUIRE_FALSE(feedback.empty());
        const auto expected =
            target == "bio_batteries" ? "enough fuel to start"
            : you.get_power_level() == 0_J
                ? "power to activate"
                : "shorting out";
        CHECK(feedback.back().second.find(expected) != std::string::npos);
#    endif
        return action("QUIT");
    });
    show_bionics_ui(you);
    CHECK(step == 2);
}

TEST_CASE(
    "the bionics screen passive tab keeps its denial and the examining rules",
    "[bionics_manager][engine_client_component][mcp]") {
    const auto guard = manager_guard{};
    auto& you = get_avatar();
    you.add_bionic(bionic_id("bio_flashlight"));
    you.add_bionic(bionic_id("bio_carbon"));
    auto step = 0;
    client::memory::set_input_provider([&](const int /*timeout*/) {
        if (!is_bionics()) {
            REQUIRE(step == 2);
            CHECK(client::current_interaction().message.find("can not activate")
                  != std::string::npos);
            ++step;
            return action("QUIT");
        }
        const auto state = captured();
        const auto screen = ec::top_screen(state);
        switch (step++) {
            case 0:
                CHECK(
                    ec::resolve_invoke(
                        state, {.affordance_id = "bionics:tab", .args = {.row = "nowhere"}})
                        .error()
                    == ec::error::invalid_arguments);
                return invoke("bionics:tab", {.row = "passive"});
            case 1:
                CHECK(body<ec::tabs_body>(part(screen, "bionics:tabs")).tabs[1].selected);
                REQUIRE(rows(screen).size() == 2);
                CHECK_FALSE(row_of(screen, id("bio_carbon")).enabled);
                CHECK_FALSE(row_of(screen, id("bio_carbon")).denial.empty());
                return invoke("bionics:pick", {.row = id("bio_carbon")});
            case 3:
                return action("TOGGLE_EXAMINE");
            case 4:
                CHECK(row_of(screen, id("bio_carbon")).enabled);
                CHECK(row_of(screen, id("bio_carbon")).denial.empty());
                CHECK(denial_of(screen, "CONFIRM").empty());
                return invoke("bionics:pick", {.row = id("bio_carbon")});
            default:
                return action("QUIT");
        }
    });
    show_bionics_ui(you);
    CHECK(step == 6);
    CHECK_FALSE(you.has_active_bionic(bionic_id("bio_flashlight")));
}

TEST_CASE(
    "the bionics screen fuel sprite sort and nested threshold handlers remain native",
    "[bionics_manager][engine_client_component][mcp]") {
    const auto guard = manager_guard{};
    auto& you = get_avatar();
    you.add_bionic(bionic_id("bio_batteries"));
    you.set_value("battery", "10");
    auto cancel_nested = false;
    SECTION("select nested threshold and sort") {}
    SECTION("cancel nested threshold and sort") { cancel_nested = true; }
    auto phase = 0;
    client::memory::set_input_provider([&](const int /*timeout*/) {
        auto& bio = you.get_bionic_state(bionic_id("bio_batteries"));
        if (!is_bionics()) {
            const auto snapshot = client::current_interaction();
            REQUIRE(snapshot.context == "UILIST");
            const auto pick = [&](const std::size_t index) {
                return cancel_nested
                         ? cancel(snapshot)
                         : resolve({.input_id = snapshot.input_id,
                                    .operation = client::interaction_operation::choose,
                                    .target_id = snapshot.choices[index].id});
            };
            if (phase == 3) {
                phase = 4;
                REQUIRE(snapshot.choices.size() == 5);
                return pick(2);
            }
            REQUIRE(phase == 5);
            phase = 6;
            return pick(1);
        }
        const auto screen = ec::top_screen(captured());
        switch (phase++) {
            case 0:
                CHECK(text_of(screen, "bionics:fuel").find("Available Fuel") != std::string::npos);
                return action("TOGGLE_SAFE_FUEL");
            case 1:
                CHECK(bio.has_flag("SAFE_FUEL_OFF"));
                return action("TOGGLE_SPRITE");
            case 2:
                CHECK_FALSE(bio.show_sprite);
                return action("TOGGLE_AUTO_START");
            case 4:
                CHECK(bio.get_auto_start_thresh() == Approx(cancel_nested ? -1.0 : 0.5));
                return action("SORT");
            case 6:
                CHECK(uistate.bionic_sort_mode
                      == (cancel_nested ? bionic_ui_sort_mode::NONE : bionic_ui_sort_mode::NAME));
                return action("QUIT");
            default:
                FAIL("unexpected native boundary");
                return action("QUIT");
        }
    });
    show_bionics_ui(you);
    CHECK(phase == 7);
}

TEST_CASE(
    "the bionics screen publishes power, state and color per row and switches its tabs by the native action",
    "[bionics_manager][engine_client_component][mcp]") {
    const auto guard = manager_guard{};
    auto& you = get_avatar();
    you.add_bionic(bionic_id("bio_flashlight"));
    auto step = 0;
    client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto screen = ec::top_screen(captured());
        switch (step++) {
            case 0: {
                const auto& flashlight = row_of(screen, id("bio_flashlight"));
                CHECK(column_of(flashlight, "state") == "Off");
                CHECK(column_of(flashlight, "power").find("act") != std::string::npos);
                CHECK(flashlight.color == string_from_color(c_light_red));
                CHECK(flashlight.hotkey.size() == 1);
                return action("NEXT_TAB");
            }
            case 1: {
                CHECK(body<ec::tabs_body>(part(screen, "bionics:tabs")).tabs[1].selected);
                const auto& storage = row_of(screen, id("bio_power_storage"));
                CHECK(column_of(storage, "state") == "Passive");
                CHECK(storage.color == string_from_color(c_cyan));
                return action("NEXT_TAB");
            }
            default:
                CHECK(body<ec::tabs_body>(part(screen, "bionics:tabs")).tabs[0].selected);
                return action("QUIT");
        }
    });
    show_bionics_ui(you);
    CHECK(step == 3);
}

TEST_CASE(
    "the bionics screen refuses fuel toggles on a bionic that burns no fuel",
    "[bionics_manager][engine_client_component][mcp]") {
    const auto guard = manager_guard{};
    auto& you = get_avatar();
    you.add_bionic(bionic_id("bio_flashlight"));
    auto step = 0;
    client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto screen = ec::top_screen(captured());
        REQUIRE(screen.context == "BIONICS");
        CHECK_FALSE(has_part(screen, "bionics:fuel"));
        CHECK_FALSE(denial_of(screen, "TOGGLE_SAFE_FUEL").empty());
        CHECK_FALSE(denial_of(screen, "TOGGLE_AUTO_START").empty());
        CHECK(denial_of(screen, "CONFIRM").empty());
        ++step;
        return action("QUIT");
    });
    show_bionics_ui(you);
    CHECK(step == 1);
}

TEST_CASE(
    "the bionics screen says when a tab has no bionics",
    "[bionics_manager][engine_client_component][mcp]") {
    const auto guard = manager_guard{};
    client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto screen = ec::top_screen(captured());
        CHECK(rows(screen).empty());
        CHECK(screen.focus_row.empty());
        CHECK_FALSE(text_of(screen, "bionics:empty").empty());
        return action("QUIT");
    });
    show_bionics_ui(get_avatar());
}

TEST_CASE(
    "the bionics screen examining describes power usage instances and slots",
    "[bionics_manager][engine_client_component][mcp]") {
    const auto guard = manager_guard{};
    const auto fixture = bionic_id{"test_bionics_manager_callback"};
    auto& definition = const_cast<bionic_data&>(fixture.obj());
    const auto restore_definition = restore_on_out_of_scope{definition};
    definition.occupied_bodyparts = {{bodypart_str_id("eyes"), 2}};
    definition.flags.insert(flag_id("MULTIINSTALL"));
    auto& you = get_avatar();
    you.add_bionic(fixture);
    you.add_bionic(fixture);
    auto step = 0;
    client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto screen = ec::top_screen(captured());
        if (step++ == 0) { return action("TOGGLE_EXAMINE"); }
        const auto& text = text_of(screen, "bionics:description");
        CHECK(text.find("Power usage:") != std::string::npos);
        CHECK(text.find("You have 2 instances") != std::string::npos);
        CHECK(text.find("(2 slots);") != std::string::npos);
        const auto& slots = body<ec::panel_body>(part(screen, "bionics:slots")).stats;
        const auto eyes = std::ranges::find(slots, "slot:eyes", &ec::panel_stat::key);
        REQUIRE(eyes != slots.end());
        CHECK(eyes->value == "4");
        CHECK_FALSE(eyes->max.empty());
        return action("QUIT");
    });
    show_bionics_ui(you);
    CHECK(step == 2);
}

TEST_CASE(
    "the bionics screen reassigns a letter through the native popup",
    "[bionics_manager][engine_client_component][mcp]") {
    const auto guard = manager_guard{};
    auto& you = get_avatar();
    you.add_bionic(bionic_id("bio_flashlight"));
    you.add_bionic(bionic_id("bio_batteries"));
    you.get_bionic_state(bionic_id("bio_flashlight")).invlet = 'a';
    you.get_bionic_state(bionic_id("bio_batteries")).invlet = 'b';
    auto phase = 0;
    auto clear = false;
    auto invalid = false;
    auto slash = false;
    SECTION("swap") {}
    SECTION("clear") { clear = true; }
    SECTION("invalid retains native feedback") { invalid = true; }
    SECTION("cancel") { phase = 10; }
    SECTION("slash is a native shortcut") { slash = true; }
    const auto value = invalid ? "?" : clear ? " " : slash ? "/" : "b";
    client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto snapshot = client::current_interaction();
        if (snapshot.context == "POPUP_WAIT" && !snapshot.field) {
            REQUIRE(invalid);
            REQUIRE(phase == 4);
            CHECK(snapshot.message.find("Invalid bionic letter") != std::string::npos);
            ++phase;
            return action("QUIT");
        }
        if (snapshot.field) {
            REQUIRE(snapshot.context == "POPUP_WAIT");
            if (phase == 12) {
                phase = 13;
                return cancel(snapshot);
            }
            if (phase++ == 2) {
                return resolve(
                    {.input_id = snapshot.input_id,
                     .operation = client::interaction_operation::fill,
                     .target_id = snapshot.field->id,
                     .value = value,
                     .submit = false});
            }
            REQUIRE(phase == 4);
            CHECK(snapshot.field->value == value);
            CHECK(you.get_bionic_state(bionic_id("bio_flashlight")).invlet == 'a');
            return resolve(
                {.input_id = snapshot.input_id,
                 .operation = client::interaction_operation::fill,
                 .target_id = snapshot.field->id,
                 .value = value,
                 .submit = true});
        }
        REQUIRE(snapshot.context == "BIONICS");
        switch (phase++) {
            case 0:
            case 10:
                return action("REASSIGN");
            case 1:
            case 11:
                return invoke("bionics:pick", {.row = id("bio_flashlight")});
            default:
                return action("QUIT");
        }
    });
    show_bionics_ui(you);
    if (phase == 14 || invalid) {
        CHECK(you.get_bionic_state(bionic_id("bio_flashlight")).invlet == 'a');
        CHECK(you.get_bionic_state(bionic_id("bio_batteries")).invlet == 'b');
    } else {
        CHECK(
            you.get_bionic_state(bionic_id("bio_flashlight")).invlet
            == (clear   ? ' '
                : slash ? '/'
                        : 'b'));
        CHECK(
            you.get_bionic_state(bionic_id("bio_batteries")).invlet
            == (clear   ? 'b'
                : slash ? 'b'
                        : 'a'));
    }
}

TEST_CASE(
    "a bionic installed several times is one row named by its kind",
    "[bionics_manager][engine_client_component][mcp]") {
    const auto guard = manager_guard{};
    auto& you = get_avatar();
    const auto storage = bionic_id("bio_power_storage");
    you.add_bionic(storage);
    you.add_bionic(bionic_id("bio_power_storage_mkII"));
    REQUIRE(you.count_bionic_of_type(storage) == 2);
    auto step = 0;
    client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto screen = ec::top_screen(captured());
        REQUIRE(screen.context == "BIONICS");
        const auto passive = invoke("bionics:tab", {.row = "passive"});
        if (step++ == 0) { return passive; }
        auto ids = std::vector<std::string>{};
        for (const auto& row : rows(screen)) { ids.push_back(row.id); }
        CHECK(std::ranges::count(ids, id("bio_power_storage")) == 1);
        CHECK(std::ranges::count(ids, id("bio_power_storage_mkII")) == 1);
        std::ranges::sort(ids);
        CHECK(std::ranges::adjacent_find(ids) == ids.end());
        return action("QUIT");
    });
    show_bionics_ui(you);
}

TEST_CASE(
    "the bionics screen callbacks run only on explicit native activation and deactivation",
    "[bionics_manager][engine_client_component][mcp]") {
    const auto guard = manager_guard{};
    auto& you = get_avatar();
    const auto fixture_id = bionic_id{"test_bionics_manager_callback"};
    you.add_bionic(fixture_id);
    you.get_bionic_state(fixture_id).invlet = 'z';
    auto& lua = DynamicDataLoader::get_instance().lua->lua;
    auto table = lua.create_table();
    auto activations = 0;
    auto deactivations = 0;
    table["activate"] = [&](sol::table params) {
        CHECK(params["user"].get<Character*>() == &you);
        CHECK(params["bionic"].get<bionic*>() == &you.get_bionic_state(fixture_id));
        you.set_value("test_bionics_manager_callback", "active");
        ++activations;
    };
    table["deactivate"] = [&](sol::table params) {
        CHECK(params["user"].get<Character*>() == &you);
        you.set_value("test_bionics_manager_callback", "inactive");
        ++deactivations;
    };
    auto callback = lua_bionic_callback_actor{
        fixture_id.str(),
        table.get<sol::protected_function>("activate"),
        table.get<sol::protected_function>("deactivate"),
        {},
        {}};
    const auto restore = restore_on_out_of_scope{fixture_id->lua_callbacks};
    fixture_id->lua_callbacks = &callback;
    const auto power = you.get_power_level();
    auto phase = 0;
    client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto screen = ec::top_screen(captured());
        REQUIRE(screen.context == "BIONICS");
        switch (phase++) {
            case 0:
                CHECK(activations == 0);
                CHECK(deactivations == 0);
                return invoke("bionics:pick", {.row = id(fixture_id.str())});
            case 1:
                CHECK(activations == 1);
                CHECK(deactivations == 0);
                CHECK(you.get_value("test_bionics_manager_callback") == "active");
                // A native hotkey reaches the same handler as semantic choose.
                return input_event{'z', input_event_t::keyboard};
            default:
                CHECK(activations == 1);
                CHECK(deactivations == 1);
                CHECK(you.get_value("test_bionics_manager_callback") == "inactive");
                CHECK(you.get_power_level()
                      == power - fixture_id->power_activate - fixture_id->power_deactivate);
                return action("QUIT");
        }
    });
    show_bionics_ui(you);
    CHECK(phase == 3);
}


TEST_CASE(
    "bionics manager registrations unwind after native input exceptions",
    "[bionics_manager][engine_client_component][mcp]") {
    const auto guard = manager_guard{};
    get_avatar().add_bionic(bionic_id("bio_flashlight"));
    client::memory::set_input_provider([&](const int /*timeout*/) -> input_event {
        REQUIRE(ec::top_screen(captured()).context == "BIONICS");
        throw std::runtime_error("manager-input-test");
    });
    CHECK_THROWS_AS(show_bionics_ui(get_avatar()), std::runtime_error);
    CHECK(client::active_input_context().context == nullptr);
    CHECK_FALSE(client::current_interaction().structured);
}

TEST_CASE(
    "the bionics screen is the same at any viewport size and has the same ids in another language",
    "[bionics_manager][engine_client_component][mcp]") {
    const auto guard = manager_guard{};
    const auto fixture_ko = locale_fixture::korean_catalogue{};
    auto& you = get_avatar();
    you.add_bionic(bionic_id("bio_flashlight"));
    you.add_bionic(bionic_id("bio_batteries"));
    auto reads = 0;
    client::memory::set_input_provider([&](const int /*timeout*/) {
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
        return action("QUIT");
    });
    show_bionics_ui(you);
    CHECK(reads == 1);
}

TEST_CASE(
    "the bionics screen publishes no window text and no native line",
    "[bionics_manager][engine_client_component][mcp]") {
    const auto guard = manager_guard{};
    get_avatar().add_bionic(bionic_id("bio_batteries"));
    client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto screen = ec::top_screen(captured());
        for (const auto& row : rows(screen)) {
            CHECK(row.label.find("<color") == std::string::npos);
            CHECK(row.label.find(row.hotkey + " ") != 0);
        }
        for (const auto& stat : body<ec::panel_body>(part(screen, "bionics:status")).stats) {
            CHECK(stat.value.find("<color") == std::string::npos);
            CHECK(stat.label.find(':') == std::string::npos);
        }
        return action("QUIT");
    });
    show_bionics_ui(get_avatar());
}

#endif
