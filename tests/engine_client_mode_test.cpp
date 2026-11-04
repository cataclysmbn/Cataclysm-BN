#if defined(CATA_MCP)

#    include "avatar.h"
#    include "cached_options.h"
#    include "calendar.h"
#    include "cata_utility.h"
#    include "catch/catch.hpp"
#    include "engine_client_mode.h"
#    include "engine_client_world.h"
#    include "game.h"
#    include "game_session.h"
#    include "message_feed.h"
#    include "messages.h"
#    include "options.h"
#    include "options_helpers.h"
#    include "player_helpers.h"
#    include "state_helpers.h"
#    include "type_id.h"

#    include <algorithm>
#    include <ranges>

namespace {
namespace ec = engine_client;

auto mode_named(const std::string& id) -> ec::mode_value {
    const auto modes = ec::engine_modes();
    const auto found = std::ranges::find(modes, id, &ec::mode_value::id);
    REQUIRE(found != modes.end());
    return *found;
}

auto value_of(const std::string& id) -> std::string { return mode_named(id).value; }

auto value_ids(const ec::mode_value& mode) -> std::vector<std::string> {
    return mode.values | std::views::transform(&ec::mode_choice::id)
         | std::ranges::to<std::vector>();
}

auto logged(const std::string& text) -> bool {
    return std::ranges::contains(Messages::feed_since({}), text, &Messages::feed_entry::text);
}

/// Puts the modes the tests change back as they were.
struct mode_guard {
    safe_mode_type safe = g->safe_mode;
    bool travel = g->auto_travel_mode;
    bool overlay = g->show_zone_overlay;
    ~mode_guard() {
        g->safe_mode = safe;
        g->auto_travel_mode = travel;
        g->show_zone_overlay = overlay;
        get_avatar().set_value("THIEF_MODE", "THIEF_ASK");
        get_avatar().set_value("THIEF_MODE_KEEP", "NO");
        get_avatar().set_movement_mode(CMM_WALK);
    }
};
} // namespace

TEST_CASE(
    "every mode is published with its value now and the values it takes",
    "[engine_client_mode][engine_client_world]") {
    clear_all_state();
    const auto modes = ec::engine_modes();
    const auto ids =
        modes | std::views::transform(&ec::mode_value::id) | std::ranges::to<std::vector>();
    CHECK(
        ids
        == std::vector<std::string>{
            "safe_mode", "auto_safe_mode", "auto_travel_mode", "manual_combat_mode", "thief_mode",
            "auto_features", "auto_pulp_butcher", "auto_mining", "auto_foraging", "auto_pickup",
            "zone_overlay", "map_memory", "hour_timer", "debug_mode", "move_mode"});
    for (const auto& mode : modes) {
        INFO(mode.id);
        CHECK_FALSE(mode.label.empty());
        CHECK(std::ranges::contains(value_ids(mode), mode.value));
        CHECK(std::ranges::all_of(mode.values, [](const auto& v) {
            return !v.id.empty() && !v.label.empty();
        }));
    }
    CHECK(value_ids(mode_named("safe_mode")) == std::vector<std::string>{"on", "off"});
    CHECK(value_ids(mode_named("auto_pulp_butcher"))
          == std::vector<std::string>{"off", "pulp", "pulp_adjacent", "butcher"});
    CHECK(value_ids(mode_named("move_mode"))
          == std::vector<std::string>{"walk", "run", "crouch", "prone"});
    CHECK(
        value_ids(mode_named("thief_mode")) == std::vector<std::string>{"ask", "honest", "steal"});
    const auto running = game_session::running();
    const auto restore = on_out_of_scope([&] { game_session::set_running(running); });
    game_session::set_running(true);
    const auto state = ec::world::capture_worded();
    REQUIRE(state.avatar);
    CHECK(state.avatar->modes == ec::engine_modes());
}

TEST_CASE(
    "setting safe mode reaches the target however often it is sent",
    "[engine_client_mode][engine_client_world]") {
    clear_all_state();
    const auto guard = mode_guard{};
    g->set_safe_mode(SAFE_MODE_ON);
    for (const auto* target : {"off", "off", "on", "on", "off"}) {
        INFO(target);
        REQUIRE(ec::run_mode_operation({.mode = "safe_mode", .value = target}));
        CHECK(value_of("safe_mode") == target);
    }
}

TEST_CASE(
    "setting a mode to the value it holds does nothing",
    "[engine_client_mode][engine_client_world]") {
    clear_all_state();
    const auto guard = mode_guard{};
    g->set_safe_mode(SAFE_MODE_OFF);
    const auto before = Messages::feed_since({}).size();
    REQUIRE(ec::run_mode_operation({.mode = "safe_mode", .value = "off"}));
    REQUIRE(ec::run_mode_operation({.mode = "move_mode", .value = "walk"}));
    CHECK(Messages::feed_since({}).size() == before);
    CHECK(get_avatar().get_movement_mode() == CMM_WALK);
}

TEST_CASE(
    "setting safe mode applies what the native toggle does on landing there",
    "[engine_client_mode][engine_client_world]") {
    clear_all_state();
    const auto guard = mode_guard{};
    g->set_safe_mode(SAFE_MODE_ON);
    g->turnssincelastmon = 7;
    get_avatar().add_effect(efftype_id("laserlocked"), 1_turns);
    REQUIRE(ec::run_mode_operation({.mode = "safe_mode", .value = "off"}));
    CHECK(g->turnssincelastmon == 0);
    CHECK_FALSE(get_avatar().has_effect(efftype_id("laserlocked")));
    CHECK(logged("Safe mode OFF!"));
    CHECK(g->safe_mode == SAFE_MODE_OFF);
    g->mostseen = 3;
    REQUIRE(ec::run_mode_operation({.mode = "safe_mode", .value = "on"}));
    CHECK(g->mostseen == 0);
    CHECK(logged("Safe mode ON!"));
}

TEST_CASE(
    "an unknown mode or value is rejected and changes nothing",
    "[engine_client_mode][engine_client_world]") {
    clear_all_state();
    const auto guard = mode_guard{};
    const auto before = ec::engine_modes();
    for (const auto& operation :
         {ec::mode_operation{.mode = "no_such_mode", .value = "on"},
          ec::mode_operation{.mode = "safe_mode", .value = "maybe"},
          ec::mode_operation{.mode = "move_mode", .value = "on"},
          ec::mode_operation{.mode = "SAFE_MODE", .value = "on"}}) {
        INFO(operation.mode << "=" << operation.value);
        const auto checked = ec::mode_operation_check(operation);
        REQUIRE_FALSE(checked);
        CHECK_FALSE(checked.error().empty());
        CHECK_FALSE(ec::run_mode_operation(operation));
    }
    CHECK(ec::engine_modes() == before);
}

TEST_CASE(
    "a multi-valued option is set to the named value directly",
    "[engine_client_mode][engine_client_world]") {
    clear_all_state();
    const auto restore = override_option("AUTO_PULP_BUTCHER", "off");
    REQUIRE(value_of("auto_pulp_butcher") == "off");
    REQUIRE(ec::run_mode_operation({.mode = "auto_pulp_butcher", .value = "butcher"}));
    CHECK(get_option<std::string>("AUTO_PULP_BUTCHER") == "butcher");
    CHECK(value_of("auto_pulp_butcher") == "butcher");
    REQUIRE(ec::run_mode_operation({.mode = "auto_pulp_butcher", .value = "butcher"}));
    CHECK(get_option<std::string>("AUTO_PULP_BUTCHER") == "butcher");
    REQUIRE(ec::run_mode_operation({.mode = "auto_pulp_butcher", .value = "pulp"}));
    CHECK(get_option<std::string>("AUTO_PULP_BUTCHER") == "pulp");
}

TEST_CASE("a bool option is set to on or off", "[engine_client_mode][engine_client_world]") {
    clear_all_state();
    const auto restore = override_option("AUTO_PICKUP", "false");
    CHECK(value_of("auto_pickup") == "off");
    REQUIRE(ec::run_mode_operation({.mode = "auto_pickup", .value = "on"}));
    REQUIRE(ec::run_mode_operation({.mode = "auto_pickup", .value = "on"}));
    CHECK(get_option<bool>("AUTO_PICKUP"));
    CHECK(value_of("auto_pickup") == "on");
    REQUIRE(ec::run_mode_operation({.mode = "auto_pickup", .value = "off"}));
    CHECK_FALSE(get_option<bool>("AUTO_PICKUP"));
}

TEST_CASE(
    "the movement mode and the thief mode skip the values between",
    "[engine_client_mode][engine_client_world]") {
    clear_all_state();
    const auto guard = mode_guard{};
    REQUIRE(ec::run_mode_operation({.mode = "move_mode", .value = "prone"}));
    CHECK(get_avatar().get_movement_mode() == CMM_PRONE);
    CHECK(value_of("move_mode") == "prone");
    REQUIRE(ec::run_mode_operation({.mode = "move_mode", .value = "run"}));
    CHECK(get_avatar().get_movement_mode() == CMM_RUN);
    REQUIRE(ec::run_mode_operation({.mode = "thief_mode", .value = "steal"}));
    CHECK(get_avatar().get_value("THIEF_MODE") == "THIEF_STEAL");
    CHECK(get_avatar().get_value("THIEF_MODE_KEEP") == "YES");
    REQUIRE(ec::run_mode_operation({.mode = "thief_mode", .value = "ask"}));
    CHECK(get_avatar().get_value("THIEF_MODE_KEEP") == "NO");
}

#endif // CATA_MCP
