#if defined(CATA_NATIVE_PREVIEW_TEST)

#    include "avatar.h"
#    include "calendar.h"
#    include "cata_utility.h"
#    include "catalua_coord.h"
#    include "catalua_icallback_actor.h"
#    include "catalua_impl.h"
#    include "catch/catch.hpp"
#    include "game.h"
#    include "init.h"
#    include "item.h"
#    include "itype.h"
#    include "json.h"
#    include "map/map.h"
#    include "messages.h"
#    include "player_helpers.h"
#    include "profession.h"
#    include "rng.h"
#    include "state_helpers.h"

#    include <algorithm>
#    include <memory>
#    include <optional>
#    include <ranges>
#    include <sstream>
#    include <string>
#    include <utility>
#    include <vector>

#    if defined(CATA_MCP)
#        include "client_command.h"
#        include "client_interaction.h"
#        include "client_memory.h"
#        include "client_memory_scope.h"
#        include "cursesdef.h"
#        include "output.h"
#    endif

#    if defined(TILES)
#        include "cached_options.h"
#        include "character_preview.h"
#        include "client/tiles/character_preview_observation.h"
#        include "options_helpers.h"
#        include "output.h"
#        include "sdltiles.h"
#        include "tile_helpers.h"
#    endif

namespace {

const auto coat_id = itype_id("test_preview_callback_coat");
constexpr auto callback_cell = tripoint_bub_ms{60, 60, 0};

const auto expected_trace = std::vector<std::string>{
    "wear=1;upvalue=48;alias=6;moves=987;terrain=t_dirt;temporary=true",
    "wear=2;upvalue=55;alias=9;moves=974;terrain=t_dirt;temporary=true"};

auto saved_messages() -> std::string {
    auto stream = std::ostringstream{};
    auto json = JsonOut{stream};
    json.start_object();
    Messages::serialize(json);
    json.end_object();
    return stream.str();
}

struct callback_observation {
    int calls;
    int try_calls;
    int upvalue;
    int alias_value;
    bool aliased;
    int moves;
    std::string terrain;
    std::vector<std::string> trace;
    auto operator==(const callback_observation&) const -> bool = default;
};

/// Install the genuine lua_iwearable_actor on this test-only type, exactly as
/// Item_factory::resolve_lua_callbacks does, but with reversible fixture ownership.
/// No Character/item methods, gapi functions or popup functions are replaced.
struct callback_fixture {
    sol::state& lua = DynamicDataLoader::get_instance().lua->lua;
    sol::table hooks = lua["game"]["hooks"];
    sol::object previous_hook = hooks["on_character_try_wear"];
    sol::table runtime = lua["game"]["mod_runtime"]["test_data"];
    sol::object previous_alias = runtime["preview_callback_alias"];
    restore_on_out_of_scope<profession_id> restore_profession{get_avatar().prof};
    restore_on_out_of_scope<time_point> restore_turn{calendar::turn};
    restore_on_out_of_scope<cata_default_random_engine> restore_rng{rng_get_engine()};
    std::string previous_messages = saved_messages();
    const lua_iwearable_actor* previous_callback = coat_id->iwearable_callbacks;
    std::unique_ptr<lua_iwearable_actor> callback;
    sol::table fixture;
    // Also runs if a REQUIRE in fixture construction unwinds before completion.
    on_out_of_scope cleanup{[this]() {
        const_cast<itype&>(coat_id.obj()).iwearable_callbacks = previous_callback;
        hooks["on_character_try_wear"] = previous_hook;
        runtime["preview_callback_alias"] = previous_alias;
        clear_all_state();
        auto stream = std::istringstream{previous_messages};
        auto json = JsonIn{stream};
        Messages::deserialize(json.get_object());
    }};

    explicit callback_fixture(const bool interactive = false) {
        clear_all_state();
        clear_character(get_avatar(), false);
        get_avatar().prof = profession_id("test_preview_callback_profession");
        get_avatar().set_moves(1000);
        get_map().ter_set(callback_cell, ter_id("t_floor"));
        hooks["on_character_try_wear"] = lua.create_table();
        const auto loaded = lua.safe_script_file(
            "data/mods/TEST_DATA/preview_callback_capability.lua", sol::script_pass_on_error);
        REQUIRE(loaded.valid());
        auto begin = loaded.get<sol::protected_function>();
        const auto result = begin(callback_cell, interactive);
        REQUIRE(result.valid());
        fixture = result.get<sol::table>();
        callback = std::make_unique<lua_iwearable_actor>(
            coat_id.str(), fixture.get<sol::protected_function>("on_wear"),
            sol::protected_function{sol::lua_nil}, sol::protected_function{sol::lua_nil},
            sol::protected_function{sol::lua_nil});
        const_cast<itype&>(coat_id.obj()).iwearable_callbacks = callback.get();
    }

    auto read() const -> callback_observation {
        auto getter = fixture.get<sol::protected_function>("read");
        const auto result = getter();
        REQUIRE(result.valid());
        const auto observed = result.get<sol::table>();
        return {
            .calls = observed.get<int>("calls"),
            .try_calls = observed.get<int>("try_calls"),
            .upvalue = observed.get<int>("upvalue"),
            .alias_value = observed.get<int>("alias_value"),
            .aliased = observed.get<bool>("aliased"),
            .moves = observed.get<int>("moves"),
            .terrain = observed.get<std::string>("terrain"),
            .trace = observed.get<std::vector<std::string>>("trace"),
        };
    }
};

auto check_completed_coat(const item& coat) -> void {
    CHECK(coat.get_var("item_note") == expected_trace.back());
    CHECK(coat.get_var("preview_callback_trace")
          == expected_trace.front() + "|" + expected_trace.back());
    CHECK(coat.get_var("tint_color_fg") == "#ff0000ff");
}

auto check_native_effects(const callback_fixture& fixture) -> void {
    const auto after = fixture.read();
    CHECK(after.calls == 2);
    CHECK(after.upvalue == 55);
    CHECK(after.alias_value == 9);
    CHECK(after.aliased);
    CHECK(after.moves == 974);
    CHECK(after.terrain == "t_dirt");
    CHECK(after.trace == expected_trace);
    CHECK(get_avatar().moves == 974);
    CHECK(get_map().ter(callback_cell) == ter_id("t_dirt"));
    const auto messages = Messages::recent_messages(Messages::size());
    auto callback_messages =
        messages | std::views::transform([](const auto& message) { return message.second; })
        | std::views::filter([](const auto& text) { return text.starts_with("Preview callback "); })
        | std::ranges::to<std::vector>();
    CHECK(
        callback_messages
        == std::vector<std::string>{
            "Preview callback " + expected_trace.front(),
            "Preview callback " + expected_trace.back()});
}

#    if defined(CATA_MCP)
struct popup_input_guard {
    game_client::memory::scoped_state memory_scope;
    restore_on_out_of_scope<bool> restore_test_mode{test_mode};
    restore_on_out_of_scope<int> restore_termx{TERMX};
    restore_on_out_of_scope<int> restore_termy{TERMY};
    restore_on_out_of_scope<int> restore_width{FULL_SCREEN_WIDTH};
    restore_on_out_of_scope<int> restore_height{FULL_SCREEN_HEIGHT};

    popup_input_guard() {
        test_mode = false;
        TERMX = 100;
        TERMY = 40;
        FULL_SCREEN_WIDTH = 80;
        FULL_SCREEN_HEIGHT = 24;
        game_client::memory::resize(TERMX, TERMY);
        catacurses::stdscr = catacurses::newwin(TERMY, TERMX, point_zero);
        catacurses::newscr = catacurses::newwin(TERMY, TERMX, point_zero);
    }
};
#    endif

#    if defined(TILES)
struct callback_display_guard {
    const override_option tiles{"TILES", "ASCIITiles"};
    const override_option overmap_tiles{"OVERMAP_TILES", "ASCIITiles"};
    const override_option fullscreen{"FULLSCREEN", "no"};
    const override_option renderer{"RENDERER", "software"};
    const override_option scaling{"SCALING_FACTOR", "1"};
    const override_option terminal_x{"TERMINAL_X", "120"};
    const override_option terminal_y{"TERMINAL_Y", "40"};
    const override_option sound{"SOUND_ENABLED", "false"};
    restore_on_out_of_scope<int> restore_termx{TERMX};
    restore_on_out_of_scope<int> restore_termy{TERMY};
    restore_on_out_of_scope<catacurses::window> restore_stdscr{catacurses::stdscr};
    restore_on_out_of_scope<catacurses::window> restore_newscr{catacurses::newscr};
    restore_on_out_of_scope<bool> restore_tiles{use_tiles};
    restore_on_out_of_scope<bool> restore_overmap_tiles{use_tiles_overmap};

    callback_display_guard() {
        REQUIRE_FALSE(tilecontext);
        catacurses::init_interface();
        TERMX = 120;
        TERMY = 40;
    }
    ~callback_display_guard() { catacurses::endwin(); }
};
#    endif

} // namespace

TEST_CASE(
    "real temporary wear retains captured Lua capabilities and completed item content",
    "[preview_callback][native_control]") {
    const auto fixture = callback_fixture{};
    const auto before = fixture.read();
    REQUIRE(before.calls == 0);
    REQUIRE(before.upvalue == 41);
    REQUIRE(before.alias_value == 3);
    REQUIRE(before.aliased);
    REQUIRE(before.moves == 1000);
    REQUIRE(before.terrain == "t_floor");
    const auto messages_before = saved_messages();
    auto temporary = avatar{};
    REQUIRE_FALSE(temporary.wear_item(item::spawn(coat_id), false));
    const auto* coat = temporary.item_worn_with_id(coat_id);
    REQUIRE(coat);
    check_completed_coat(*coat);
    check_native_effects(fixture);
    CHECK(fixture.read().try_calls == 1);
    CHECK(saved_messages() != messages_before);
    CHECK_FALSE(get_avatar().worn_with_id(coat_id));
}

#    if defined(CATA_MCP)
TEST_CASE(
    "real Lua try wear eligibility depends on native semantic popup answer",
    "[preview_callback][decision_dependency][mcp]") {
    auto first_start = std::optional<callback_observation>{};
    auto first_schema = std::string{};
    auto first_choices = std::vector<std::string>{};
    for (const auto& answer : {std::string{"YES"}, std::string{"NO"}}) {
        CAPTURE(answer);
        const auto fixture = callback_fixture{true};
        const auto start = fixture.read();
        if (first_start) {
            CHECK(start == *first_start);
        } else {
            first_start = start;
        }
        const auto messages_before = saved_messages();
        const auto guard = popup_input_guard{};
        auto reads = 0;
        game_client::memory::set_input_provider([&](const int /*timeout*/) {
            REQUIRE(++reads == 1);
            const auto snapshot = game_client::current_interaction();
            CHECK(snapshot.context == "YESNO");
            CHECK(snapshot.message == "Allow this fixture coat?");
            REQUIRE(snapshot.choices.size() == 2);
            const auto choices =
                snapshot.choices
                | std::views::transform(&game_client::interaction_choice::description)
                | std::ranges::to<std::vector>();
            CHECK(choices == std::vector<std::string>{"YES", "NO"});
            if (first_schema.empty()) {
                first_schema = snapshot.schema_id;
                first_choices = choices;
            } else {
                CHECK(snapshot.schema_id == first_schema);
                CHECK(choices == first_choices);
            }
            const auto choice = std::ranges::
                find(snapshot.choices, answer, &game_client::interaction_choice::description);
            REQUIRE(choice != snapshot.choices.end());
            REQUIRE(choice->enabled);
            REQUIRE(choice->selectable);
            auto command = game_client::input_command{};
            command.interaction = game_client::interaction_command{
                .input_id = snapshot.input_id,
                .operation = game_client::interaction_operation::choose,
                .target_id = choice->id,
                .value = {},
                .submit = std::nullopt,
                .count = std::nullopt,
                .position = std::nullopt,
            };
            const auto resolved =
                game_client::resolve_input_command(command, game_client::memory::screen_size());
            REQUIRE(resolved.has_value());
            return *resolved;
        });
        const auto coat = item::spawn(coat_id);
        const auto eligibility = get_avatar().can_wear(*coat);
        CHECK(eligibility.success() == (answer == "YES"));
        if (answer == "NO") { CHECK(eligibility.str() == "Fixture coat refused."); }
        CHECK(reads == 1);
        auto expected = start;
        expected.try_calls = 1;
        CHECK(fixture.read() == expected);
        CHECK(saved_messages() == messages_before);
    }
}
#    endif

#    if defined(TILES)
TEST_CASE(
    "public character preview preserves captured Lua callback authority",
    "[preview_callback][callback_purity][tiles]") {
    const auto fixture = callback_fixture{};
    const auto display = callback_display_guard{};
    const auto tiles = tile_context_fixture{true};
    REQUIRE(tiles.valid());
    // Capture all witness authority BEFORE the first preview eligibility query.
    const auto before = fixture.read();
    const auto messages_before = saved_messages();
    auto observed = 0;
    const auto previous_observer = game_client::tiles::set_character_preview_observer(
        [&](const auto& temporary, const auto /*entries*/) {
            ++observed;
            CHECK(&temporary != &get_avatar());
            const auto* coat = temporary.item_worn_with_id(coat_id);
            REQUIRE(coat);
            check_completed_coat(*coat);
        });
    const auto restore_observer = on_out_of_scope([&]() {
        game_client::tiles::set_character_preview_observer(previous_observer);
    });
    auto preview = game_client::make_character_preview();
    REQUIRE(preview);
    preview->init(&get_avatar());
    const auto orientation = character_preview_window::Orientation{};
    preview->prepare(
        {.nlines = 12, .ncols = 12, .orientation = &orientation, .hide_below_ncols = 0});
    CHECK(fixture.read() == before);
    CHECK(saved_messages() == messages_before);
    preview->display();
    REQUIRE(observed == 1);
    const auto after = fixture.read();
    CAPTURE(after.calls, after.try_calls, after.upvalue, after.alias_value, after.moves,
            after.terrain);
    CHECK(after.calls == before.calls);
    CHECK(after.try_calls == before.try_calls);
    CHECK(after.upvalue == before.upvalue);
    CHECK(after.alias_value == before.alias_value);
    CHECK(after.aliased == before.aliased);
    CHECK(after.moves == before.moves);
    CHECK(after.terrain == before.terrain);
    CHECK(after.trace == before.trace);
    CHECK(get_avatar().moves == before.moves);
    CHECK(get_map().ter(callback_cell) == ter_id(before.terrain));
    CHECK(saved_messages() == messages_before);
    preview->clear();
}
#    endif

#endif // CATA_NATIVE_PREVIEW_TEST
