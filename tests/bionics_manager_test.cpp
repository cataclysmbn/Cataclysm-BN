#if defined(CATA_MCP)

#    include "avatar.h"
#    include "bionics.h"
#    include "bionics_ui.h"
#    include "bionics_ui_observation.h"
#    include "cata_utility.h"
#    include "catalua_icallback_actor.h"
#    include "catalua_impl.h"
#    include "catch/catch.hpp"
#    include "client_command.h"
#    include "client_input.h"
#    include "client_interaction_prepared.h"
#    include "client_memory.h"
#    include "client_memory_scope.h"
#    include "cursesdef.h"
#    include "init.h"
#    include "input.h"
#    include "json.h"
#    include "messages.h"
#    include "options.h"
#    include "options_helpers.h"
#    include "output.h"
#    include "player_helpers.h"
#    include "rng.h"
#    include "state_helpers.h"
#    include "uistate.h"

#    include <array>
#    include <ranges>
#    include <sstream>
#    include <stdexcept>

namespace {
namespace client = game_client;

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

auto id(const std::string& native_id) -> std::string {
    return client::opaque_interaction_id("bionic", {native_id});
}

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

auto choose(const client::interaction_snapshot& snapshot, const std::string& target)
    -> input_event {
    return resolve(
        {.input_id = snapshot.input_id,
         .operation = client::interaction_operation::choose,
         .target_id = target});
}

auto cancel(const client::interaction_snapshot& snapshot) -> input_event {
    return resolve(
        {.input_id = snapshot.input_id, .operation = client::interaction_operation::cancel});
}

/// Actual native input callback, never a synthetic snapshot or activation-rule mock.
auto observe() -> client::interaction_snapshot {
    const auto before = actor_state();
    const auto rng = rng_get_engine();
    const auto messages = Messages::recent_messages(100);
    const auto all = client::current_interaction({.limit = 200});
    REQUIRE(all.structured);
    client::reset_interaction_work();
    for (const auto offset : std::views::iota(std::size_t{0}, all.choices.size() + 1)) {
        const auto page = client::current_interaction({.offset = offset, .limit = 1});
        CHECK(page.schema_id == all.schema_id);
        CHECK(page.message == all.message);
        CHECK(page.choice_total == all.choice_total);
        if (offset < all.choices.size()) {
            REQUIRE(page.choices.size() == 1);
            CHECK(page.choices.front().description == all.choices[offset].description);
            CHECK(page.choices.front().id == all.choices[offset].id);
        }
    }
    if (all.context == "BIONICS" || all.field) {
        CHECK(client::interaction_work().schema_hashes == 0);
        CHECK(client::interaction_work().hashed_choices == 0);
        CHECK(client::interaction_work().materialized_choices <= all.choices.size() + 1);
    }
    CHECK(actor_state() == before);
    CHECK(rng_get_engine() == rng);
    CHECK(Messages::recent_messages(100) == messages);
    return all;
}
} // namespace

TEST_CASE(
    "cold bionics descriptions are bounded by requested rows rather than installed CBMs",
    "[bionics_manager][bionics_cold_work][client][interaction][mcp]") {
    const auto guard = manager_guard{};
    auto& you = get_avatar();
    // Real installed native CBMs, not fabricated snapshots or description-provider mocks.
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
    auto mode = std::string{};
    SECTION("native only does not produce semantic details") { mode = "native"; }
    SECTION("cold one row and repeated reads") { mode = "page"; }
    SECTION("zero limit does not produce details") { mode = "zero"; }
    SECTION("past end does not produce details") { mode = "past"; }
    SECTION("full query is a positive semantic production control") { mode = "full"; }
    SECTION("native examination is a positive native production control") { mode = "examine"; }
    auto polls = 0;
    bionics_ui::reset_description_work();
    client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto poll = polls++;
        CHECK(actor_state() == before);
        CHECK(rng_get_engine() == rng);
        CHECK(message_state() == messages);
        if (mode == "native" || mode == "examine") {
            CHECK(bionics_ui::description_work().semantic == 0);
            if (mode == "examine" && poll == 0) {
                CHECK(bionics_ui::description_work().native == 0);
                return input_event{'!', input_event_t::keyboard};
            }
            if (mode == "examine") { CHECK(bionics_ui::description_work().native > 0); }
            return input_event{KEY_ESCAPE, input_event_t::keyboard};
        }
        // Reset before opening the manager, NEVER after eager preparation has hidden its work.
        const auto request = client::interaction_page{
            .offset = mode == "past" ? installed.size() : 0,
            .limit = mode == "zero" ? 0
                   : mode == "full" ? installed.size()
                                    : 1};
        const auto first = client::current_interaction(request);
        REQUIRE(first.context == "BIONICS");
        REQUIRE(first.choice_total == installed.size());
        const auto expected =
            mode == "full" ? installed.size()
            : mode == "page"
                ? std::size_t{1}
                : std::size_t{0};
        CHECK(first.choices.size() == expected);
        CHECK(bionics_ui::description_work().semantic == expected);
        for (const auto& choice : first.choices) { CHECK_FALSE(choice.description.empty()); }
        const auto work = bionics_ui::description_work().semantic;
        const auto repeat = client::current_interaction(request);
        CHECK(repeat.schema_id == first.schema_id);
        CHECK(repeat.input_id == first.input_id);
        CHECK(bionics_ui::description_work().semantic == work);
        if (!first.choices.empty()) {
            CHECK(repeat.choices.front().description == first.choices.front().description);
            CHECK(first.choices.front().description.find(
                      bionic_id(installed.front())->description.translated())
                  != std::string::npos);
        }
        CHECK(actor_state() == before);
        CHECK(rng_get_engine() == rng);
        CHECK(message_state() == messages);
        return input_event{KEY_ESCAPE, input_event_t::keyboard};
    });
    show_bionics_ui(you);
    CHECK(polls == (mode == "examine" ? 2 : 1));
    CHECK(actor_state() == before);
    CHECK(rng_get_engine() == rng);
    CHECK(message_state() == messages);
}

TEST_CASE(
    "bionics manager reads are pure and semantic activation uses real power and effects",
    "[bionics_manager][client][interaction][mcp]") {
    const auto guard = manager_guard{};
    auto& you = get_avatar();
    you.add_bionic(bionic_id("bio_flashlight"));
    you.add_bionic(bionic_id("bio_carbon"));
    const auto power = you.get_power_level();
    const auto cost = bionic_id("bio_flashlight")->power_activate;
    const auto light = you.active_light();
    auto step = 0;
    auto previous_input = std::uint64_t{0};
    client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto snapshot = observe();
        REQUIRE(snapshot.context == "BIONICS");
        REQUIRE(snapshot.panes.size() == 2);
        if (step++ == 0) {
            REQUIRE(snapshot.choices.size() == 1);
            CHECK(snapshot.choices.front().description.find(
                      bionic_id("bio_flashlight")->description.translated())
                  != std::string::npos);
            CHECK(snapshot.choices.front().description.find("eyes") != std::string::npos);
            CHECK_FALSE(client::resolve_interaction_command(
                {.input_id = snapshot.input_id,
                 .operation = client::interaction_operation::choose,
                 .target_id = id("bio_carbon")}));
            previous_input = snapshot.input_id;
            return choose(snapshot, id("bio_flashlight"));
        }
        CHECK(you.has_active_bionic(bionic_id("bio_flashlight")));
        CHECK(you.get_power_level() == power - cost);
        CHECK(you.active_light() > light);
        CHECK_FALSE(client::resolve_interaction_command(
            {.input_id = previous_input,
             .operation = client::interaction_operation::choose,
             .target_id = id("bio_flashlight")}));
        return cancel(snapshot);
    });
    show_bionics_ui(you);
    CHECK(step == 2);
    CHECK(you.has_active_bionic(bionic_id("bio_flashlight")));
}

TEST_CASE(
    "bionics manager preserves native power fuel and incapacitation feedback",
    "[bionics_manager][client][interaction][mcp]") {
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
        const auto snapshot = observe();
        REQUIRE(snapshot.context == "BIONICS");
        if (step++ == 0) {
            REQUIRE(snapshot.choices.size() == 1);
            CHECK(snapshot.choices.front().enabled == !disabled);
            CHECK(snapshot.choices.front().selectable);
            CHECK(snapshot.choices.front().denial.empty() == !disabled);
            Messages::clear_messages();
            return choose(snapshot, id(target));
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
        return cancel(snapshot);
    });
    show_bionics_ui(you);
    CHECK(step == 2);
}

TEST_CASE(
    "bionics manager passive examination retains denial and hidden-tab rules",
    "[bionics_manager][client][interaction][mcp]") {
    const auto guard = manager_guard{};
    auto& you = get_avatar();
    you.add_bionic(bionic_id("bio_flashlight"));
    you.add_bionic(bionic_id("bio_carbon"));
    auto step = 0;
    client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto snapshot = observe();
        if (snapshot.context == "POPUP_WAIT") {
            REQUIRE(step == 2);
            CHECK(snapshot.message.find("can not activate") != std::string::npos);
            ++step;
            return choose(snapshot, "acknowledge");
        }
        REQUIRE(snapshot.context == "BIONICS");
        switch (step++) {
            case 0:
                return action("NEXT_TAB");
            case 1:
                REQUIRE(snapshot.choices.size() == 2);
                CHECK_FALSE(snapshot.choices.front().enabled);
                CHECK(snapshot.choices.front().selectable);
                return choose(snapshot, id("bio_carbon"));
            case 3:
                return action("TOGGLE_EXAMINE");
            case 4:
                CHECK(snapshot.choices.front().enabled);
                CHECK(snapshot.choices.front().denial.empty());
                return choose(snapshot, id("bio_carbon"));
            default:
                return cancel(snapshot);
        }
    });
    show_bionics_ui(you);
    CHECK(step == 6);
    CHECK_FALSE(you.has_active_bionic(bionic_id("bio_flashlight")));
}

TEST_CASE(
    "bionics manager fuel sprite sort and nested threshold handlers remain native",
    "[bionics_manager][client][interaction][mcp]") {
    const auto guard = manager_guard{};
    auto& you = get_avatar();
    you.add_bionic(bionic_id("bio_batteries"));
    you.set_value("battery", "10");
    auto cancel_nested = false;
    SECTION("select nested threshold and sort") {}
    SECTION("cancel nested threshold and sort") { cancel_nested = true; }
    auto phase = 0;
    client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto snapshot = observe();
        auto& bio = you.get_bionic_state(bionic_id("bio_batteries"));
        if (snapshot.context == "UILIST") {
            if (phase == 3) {
                phase = 4;
                REQUIRE(snapshot.choices.size() == 5);
                return cancel_nested ? cancel(snapshot) : choose(snapshot, snapshot.choices[2].id);
            }
            REQUIRE(phase == 5);
            phase = 6;
            return cancel_nested ? cancel(snapshot) : choose(snapshot, snapshot.choices[1].id);
        }
        REQUIRE(snapshot.context == "BIONICS");
        switch (phase++) {
            case 0:
                CHECK(snapshot.message.find("Available Fuel") != std::string::npos);
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
                return cancel(snapshot);
            default:
                FAIL("unexpected native boundary");
                return cancel(snapshot);
        }
    });
    show_bionics_ui(you);
    CHECK(phase == 7);
}

TEST_CASE(
    "bionics manager reassignment supports drafts clear swap and cancellation",
    "[bionics_manager][client][interaction][mcp]") {
    const auto guard = manager_guard{};
    auto& you = get_avatar();
    you.add_bionic(bionic_id("bio_flashlight"));
    you.add_bionic(bionic_id("bio_batteries"));
    you.get_bionic_state(bionic_id("bio_flashlight")).invlet = 'a';
    you.get_bionic_state(bionic_id("bio_batteries")).invlet = 'b';
    auto phase = 0;
    auto clear = false;
    auto invalid = false;
    SECTION("swap") {}
    SECTION("clear") { clear = true; }
    SECTION("invalid retains native feedback") { invalid = true; }
    SECTION("cancel") { phase = 10; }
    const auto value = invalid ? "?" : clear ? " " : "b";
    client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto snapshot = observe();
        if (snapshot.context == "POPUP_WAIT" && !snapshot.field) {
            REQUIRE(invalid);
            REQUIRE(phase == 4);
            CHECK(snapshot.message.find("Invalid bionic letter") != std::string::npos);
            ++phase;
            return choose(snapshot, "acknowledge");
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
                return choose(snapshot, id("bio_flashlight"));
            default:
                return cancel(snapshot);
        }
    });
    show_bionics_ui(you);
    if (phase == 14 || invalid) {
        CHECK(you.get_bionic_state(bionic_id("bio_flashlight")).invlet == 'a');
        CHECK(you.get_bionic_state(bionic_id("bio_batteries")).invlet == 'b');
    } else {
        CHECK(you.get_bionic_state(bionic_id("bio_flashlight")).invlet == (clear ? ' ' : 'b'));
        CHECK(you.get_bionic_state(bionic_id("bio_batteries")).invlet == (clear ? 'b' : 'a'));
    }
}

TEST_CASE(
    "bionics manager callbacks run only on explicit native activation and deactivation",
    "[bionics_manager][client][interaction][mcp]") {
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
        const auto snapshot = observe();
        REQUIRE(snapshot.context == "BIONICS");
        switch (phase++) {
            case 0:
                CHECK(activations == 0);
                CHECK(deactivations == 0);
                return choose(snapshot, id(fixture_id.str()));
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
                return cancel(snapshot);
        }
    });
    show_bionics_ui(you);
    CHECK(phase == 3);
}

TEST_CASE(
    "bionics manager registrations unwind after native input exceptions",
    "[bionics_manager][client][interaction][mcp]") {
    const auto guard = manager_guard{};
    get_avatar().add_bionic(bionic_id("bio_flashlight"));
    client::memory::set_input_provider([&](const int /*timeout*/) -> input_event {
        const auto snapshot = observe();
        REQUIRE(snapshot.context == "BIONICS");
        throw std::runtime_error("manager-input-test");
    });
    CHECK_THROWS_AS(show_bionics_ui(get_avatar()), std::runtime_error);
    CHECK(client::active_input_context().context == nullptr);
    CHECK_FALSE(client::current_interaction().structured);
}

TEST_CASE(
    "bionic cold capture owns long off-page definition text without cloning it",
    "[bionics_manager][bionics_lazy_dependencies][mcp]") {
    const auto guard = manager_guard{};
    const auto fixture = bionic_id{"test_bionics_manager_callback"};
    auto& definition = const_cast<bionic_data&>(fixture.obj());
    const auto restore_definition = restore_on_out_of_scope{definition};
    auto& you = get_avatar();
    you.add_bionic(bionic_id("bio_flashlight"));
    you.add_bionic(fixture);
    const auto long_text = std::string(2 * 1024 * 1024, 'x');
    definition.description = no_translation(long_text);
    const auto* original_storage = definition.description.debug_get_raw().data();
    const auto frozen = definition.description.snapshot();
    localization::reset_snapshot_work();
    bionics_ui::reset_description_work();
    auto calls = 0;
    client::memory::set_input_provider([&](const int /*timeout*/) {
        CHECK(localization::snapshot_work().input_copied_bytes == 0);
        CHECK(localization::snapshot_work().input_hashed_bytes == 0);
        CHECK(frozen.raw_view().data() == original_storage);
        CHECK(bionics_ui::description_work().semantic == 0);
        const auto metadata = client::current_interaction({.limit = 0});
        CHECK(bionics_ui::description_work().semantic == 0);
        const auto page = client::current_interaction({.limit = 1});
        CHECK(bionics_ui::description_work().semantic == 1);
        CHECK(page.choices.front().description.find(long_text) == std::string::npos);
        // Producer replacement after capture must not change the owned old boundary or its schema.
        definition.description = no_translation("new short description");
        const auto old = client::current_interaction({.offset = 1, .limit = 1});
        CHECK(old.schema_id == metadata.schema_id);
        CHECK(old.choices.front().description.find(long_text) != std::string::npos);
        CHECK(bionics_ui::description_work().semantic == 2);
        ++calls;
        return input_event{KEY_ESCAPE, input_event_t::keyboard};
    });
    show_bionics_ui(you);
    CHECK(calls == 1);
    CHECK(frozen.raw_view() == long_text);
}

TEST_CASE(
    "bionic descriptions invalidate all frozen text power state and slot dependencies",
    "[bionics_manager][bionics_lazy_dependencies][mcp]") {
    const auto guard = manager_guard{};
    const auto fixture = bionic_id{"test_bionics_manager_callback"};
    auto& definition = const_cast<bionic_data&>(fixture.obj());
    const auto restore_definition = restore_on_out_of_scope{definition};
    auto& heading = const_cast<body_part_type&>(convert_bp(bp_eyes).obj()).name_as_heading;
    const auto restore_heading = restore_on_out_of_scope{heading};
    definition.occupied_bodyparts = {{bodypart_str_id("eyes"), 1}};
    definition.fuel_opts = {itype_id("battery")};
    definition.flags.insert(flag_id("MULTIINSTALL"));
    auto& you = get_avatar();
    you.add_bionic(fixture);
    you.add_bionic(fixture);
    auto& bio = you.get_bionic_state(fixture);
    const auto capture = [&]() {
        auto result = client::interaction_snapshot{};
        client::memory::set_input_provider([&](const int /*timeout*/) {
            result = client::current_interaction({.limit = 1});
            REQUIRE(result.context == "BIONICS");
            REQUIRE(result.choices.size() == 1);
            return input_event{KEY_ESCAPE, input_event_t::keyboard};
        });
        show_bionics_ui(you);
        return result;
    };
    const auto previous = capture();
    const auto old_text = previous.choices.front().description;
    REQUIRE(old_text.find("You have 2 instances") != std::string::npos);
    auto changes_schema = true;
    auto changes_text = true;
    auto add_instance = false;
    auto expected_fragment = std::string{};
    client::memory::set_input_provider([&](const int /*timeout*/) {
        // Capture a cold, otherwise-identical boundary. Do not toggle examination/help mode.
        const auto metadata = client::current_interaction({.limit = 0});
        REQUIRE(metadata.schema_id == previous.schema_id);
        SECTION("no mutation") {
            changes_schema = false;
            changes_text = false;
        }
        SECTION("name") {
            definition.name = no_translation("new name");
            expected_fragment = "new name";
        }
        SECTION("raw text") {
            definition.description = no_translation("new description");
            expected_fragment = "new description";
        }
        SECTION("translation context") {
            definition.description.add_context("new context");
            changes_text = false;
        }
        SECTION("plural payload") {
            definition.description.make_plural();
            changes_text = false;
        }
        SECTION("activation cost") {
            definition.power_activate += 1_J;
            expected_fragment = "9 J act";
        }
        SECTION("deactivation cost") {
            definition.power_deactivate += 1_J;
            expected_fragment = "3 J deact";
        }
        SECTION("trigger cost") {
            definition.power_trigger += 1_J;
            expected_fragment = "1 J trigger";
        }
        SECTION("calorie cost") {
            definition.kcal_trigger += 1;
            expected_fragment = "1 kcal trigger";
        }
        SECTION("periodic power") {
            definition.charge_time = 2;
            definition.power_over_time = 1_J;
            expected_fragment = "1 J/2 turns";
        }
        SECTION("powered state") {
            bio.powered = true;
            expected_fragment = "Power usage: 8 J act, 2 J deact, ON";
        }
        SECTION("incapacitation") {
            bio.incapacitated_time = 1_hours;
            expected_fragment = "(incapacitated)";
        }
        SECTION("sprite visibility") {
            bio.show_sprite = false;
            expected_fragment = "(hidden)";
        }
        SECTION("safe fuel state") { bio.toggle_safe_fuel_mod(); }
        SECTION("auto-start threshold") {
            bio.set_auto_start_thresh(0.5f);
            expected_fragment = "auto start < 50 %";
        }
        SECTION("multi-install count") {
            add_instance = true;
            expected_fragment = "You have 3 instances";
        }
        SECTION("slot count") {
            definition.occupied_bodyparts.begin()->second = 2;
            expected_fragment = "(2 slots);";
        }
        SECTION("heading") {
            heading = no_translation("new heading");
            expected_fragment = "new heading (1 slots);";
        }
        SECTION("slot option") { get_options().get_option("CBM_SLOTS_ENABLED").setValue("false"); }
        // First render is after producer mutation: the old boundary must still own every input.
        const auto retained = client::current_interaction({.limit = 1});
        CHECK(retained.schema_id == metadata.schema_id);
        REQUIRE(retained.choices.size() == 1);
        CHECK(retained.choices.front().description == old_text);
        return input_event{KEY_ESCAPE, input_event_t::keyboard};
    });
    show_bionics_ui(you);
    // Structural collection mutation must not invalidate pointers held by a running native UI.
    if (add_instance) {
        you.add_bionic(fixture);
        REQUIRE(you.count_bionic_of_type(fixture) == 3);
    }
    const auto current = capture();
    CHECK((current.schema_id != previous.schema_id) == changes_schema);
    CHECK((current.choices.front().description != old_text) == changes_text);
    CHECK(previous.choices.front().description == old_text);
    if (!expected_fragment.empty()) {
        CHECK(current.choices.front().description.find(expected_fragment) != std::string::npos);
    }
    if (!changes_schema) { CHECK(current.message == previous.message); }
}

#endif
