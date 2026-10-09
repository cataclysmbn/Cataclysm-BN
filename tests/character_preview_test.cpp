#if defined(CATA_NATIVE_PREVIEW_TEST)

#    include "avatar.h"
#    include "bionics.h"
#    include "calendar.h"
#    include "cata_utility.h"
#    include "catch/catch.hpp"
#    include "flag.h"
#    include "game.h"
#    include "item.h"
#    include "json.h"
#    include "magic/magic.h"
#    include "messages.h"
#    include "player_helpers.h"
#    include "profession.h"
#    include "rng.h"
#    include "rng_observation.h"
#    include "state_helpers.h"
#    include "type_id.h"

#    include <algorithm>
#    include <functional>
#    include <ranges>
#    include <sstream>
#    include <stdexcept>
#    include <string>
#    include <vector>

#    if defined(TILES)
#        include "cached_options.h"
#        include "cata_tiles.h"
#        include "character_preview.h"
#        include "client/tiles/character_preview_observation.h"
#        include "cursesdef.h"
#        include "faction.h"
#        include "map/map.h"
#        include "map/submap.h"
#        include "options_helpers.h"
#        include "output.h"
#        include "scenario.h"
#        include "sdltiles.h"
#        include "tile_helpers.h"
#        include "worldfactory.h"
#    endif

namespace {

auto saved_messages() -> std::string {
    auto stream = std::ostringstream{};
    auto json = JsonOut{stream};
    json.start_object();
    Messages::serialize(json);
    json.end_object();
    return stream.str();
}

struct preview_test_state {
    restore_on_out_of_scope<cata_default_random_engine> restore_rng{rng_get_engine()};
    restore_on_out_of_scope<time_point> restore_turn{calendar::turn};
    restore_on_out_of_scope<profession_id> restore_profession{get_avatar().prof};
    std::string original_messages = saved_messages();

    preview_test_state() {
        REQUIRE_FALSE(rng_deterministic_seed_active());
        clear_all_state();
        clear_character(get_avatar(), false);
        get_avatar().prof = profession_id("test_preview_profession");
        calendar::turn = calendar::turn_zero + 12_hours;
        Messages::add_msg(m_warning, "Retain this preexisting preview message.");
        Messages::add_msg(m_good, "Retain this repeated preview message.");
        Messages::add_msg(m_good, "Retain this repeated preview message.");
    }

    ~preview_test_state() {
        // Test teardown only, never between construction/display/clear assertions.
        rng_clear_deterministic_seed();
        clear_all_state();
        auto stream = std::istringstream{original_messages};
        auto json = JsonIn{stream};
        Messages::deserialize(json.get_object());
    }
};

#    if defined(TILES)

/// The existing tile fixture supplies real overlays/renderer; native initialization
/// supplies the fonts/framebuffer needed by the public preview's border drawing.
struct native_preview_display {
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
    restore_on_out_of_scope<bool> restore_use_tiles{use_tiles};
    restore_on_out_of_scope<bool> restore_use_overmap_tiles{use_tiles_overmap};

    explicit native_preview_display(
        const std::function<auto()->void>& initialize = catacurses::init_interface) {
        REQUIRE_FALSE(tilecontext);
        try {
            initialize();
            TERMX = 120;
            TERMY = 40;
        } catch (...) {
            // A throwing constructor has no destructor: release partial native setup.
            catacurses::endwin();
            throw;
        }
    }
    ~native_preview_display() { catacurses::endwin(); }
};

struct preview_observer_guard {
    game_client::tiles::character_preview_observer previous;
    explicit preview_observer_guard(game_client::tiles::character_preview_observer observer)
        : previous(game_client::tiles::set_character_preview_observer(std::move(observer))) {}
    ~preview_observer_guard() {
        game_client::tiles::set_character_preview_observer(std::move(previous));
    }
};

auto saved_actor() -> std::string {
    auto stream = std::ostringstream{};
    auto json = JsonOut{stream};
    get_avatar().serialize(json);
    return stream.str();
}

auto saved_game() -> std::string {
    auto stream = std::ostringstream{};
    g->serialize(stream);
    return stream.str();
}

/// All loaded submaps on the preview actor's plane, including the color-query origin.
/// This is not an assertion about unobserved threads or unloaded overmaps.
auto saved_world() -> std::string {
    auto stream = std::ostringstream{};
    auto json = JsonOut{stream};
    json.start_object();
    json.member("factions", *g->faction_manager_ptr);
    json.member("options");
    json.start_object();
    for (const auto& [id, option] : world_generator->active_world->info->WORLD_OPTIONS) {
        json.member(id, option.getValue());
    }
    json.end_object();
    json.member("submaps");
    json.start_array();
    for (const auto x : std::views::iota(0, get_map().getmapsize())) {
        for (const auto y : std::views::iota(0, get_map().getmapsize())) {
            json.start_object();
            get_map().get_submap_at(tripoint_bub_ms{x * SEEX, y * SEEY, 0})->store(json);
            json.end_object();
        }
    }
    json.end_array();
    json.end_object();
    return stream.str();
}

struct preview_authority {
    std::string actor = saved_actor();
    std::string game = saved_game();
    std::string world = saved_world();
    std::string messages = saved_messages();
    std::optional<rng_observation> context = observe_deterministic_rng();
    cata_default_random_engine selected_engine = rng_get_engine();
    cata_default_random_engine main_engine;
    const scenario* scenario_ptr = get_scenario();
    time_point turn = calendar::turn;
    int actions = g->get_user_action_counter();

    explicit preview_authority(const cata_default_random_engine& main): main_engine(main) {}

    auto check(const cata_default_random_engine& main) const -> void {
        const auto after = observe_deterministic_rng();
        REQUIRE(after.has_value() == context.has_value());
        CHECK(rng_get_engine() == selected_engine);
        CHECK(main == main_engine);
        if (context) {
            CAPTURE(context->engine_state, after->engine_state);
            CHECK(after->source == context->source);
            CHECK(after->root_seed == context->root_seed);
            CHECK(after->next_root_call == context->next_root_call);
            CHECK(after->task == context->task);
        }
        CHECK(saved_messages() == messages);
        CHECK(saved_actor() == actor);
        // Avoid dumping the complete native save/submap payload for message-only failures.
        const auto game_unchanged = saved_game() == game;
        const auto world_unchanged = saved_world() == world;
        CHECK(game_unchanged);
        CHECK(world_unchanged);
        CHECK(get_scenario() == scenario_ptr);
        CHECK(calendar::turn == turn);
        CHECK(g->get_user_action_counter() == actions);
    }
};

auto expected_overlays(const bool with_clothing) -> std::vector<std::string> {
    auto ids = std::vector<std::string>{
        "effect_test_preview_effect",
        "mutation_test_preview_trait",
        "mutation_active_test_preview_active_trait",
        "mutation_test_preview_profession_cbm",
        "mutation_test_preview_included_cbm",
        "mutation_test_preview_installed_cbm"};
    if (with_clothing) {
        ids.emplace_back("worn_test_preview_coat");
        ids.emplace_back("wielded_test_preview_wielded");
    }
    return ids;
}

#    endif // TILES

} // namespace

TEST_CASE(
    "native profession generation and installation retain gameplay effects",
    "[character_preview][native_generation]") {
    const auto fixture = preview_test_state{};
    rng_set_deterministic_seed(92824);
    const auto before = observe_deterministic_rng();
    const auto messages = saved_messages();
    auto items = get_avatar().prof->items(true, {});
    REQUIRE(items.size() == 2);
    CHECK(observe_deterministic_rng() != before);
    auto ids = items | std::views::transform([](const auto& it) { return it->typeId().str(); })
             | std::ranges::to<std::vector>();
    std::ranges::sort(ids);
    CHECK(ids == std::vector<std::string>{"test_preview_coat", "test_preview_wielded"});

    get_avatar().add_bionic(bionic_id("test_preview_profession_cbm"));
    CHECK(get_avatar().get_max_power_level() == 7_kJ);
    CHECK(get_avatar().has_bionic(bionic_id("test_preview_included_cbm")));
    CHECK(saved_messages() != messages);
    const auto moves = get_avatar().moves;
    const auto installed_messages = saved_messages();
    const auto coat = std::ranges::find_if(items, [](const auto& it) {
        return it->typeId() == itype_id("test_preview_coat");
    });
    REQUIRE(coat != items.end());
    REQUIRE_FALSE(get_avatar().wear_item(std::move(*coat), true));
    CHECK(get_avatar().worn_with_id(itype_id("test_preview_coat")));
    CHECK(get_avatar().moves < moves);
    CHECK(saved_messages() != installed_messages);
    CHECK(std::ranges::any_of(Messages::recent_messages(Messages::size()), [](const auto& message) {
        return message.second == "Retain this preexisting preview message.";
    }));
}

TEST_CASE(
    "native initial activation retains power fuel and spell rules",
    "[character_preview][native_generation][active_spells]") {
    const auto fixture = preview_test_state{};
    auto& actor = get_avatar();
    REQUIRE(actor.get_power_level() == 0_J);
    const auto messages = saved_messages();
    actor.add_bionic(bionic_id("test_preview_active_cbm"));
    CHECK(actor.has_active_bionic(bionic_id("test_preview_active_cbm")));
    CHECK(actor.has_trait(trait_id("test_preview_spell_class")));
    REQUIRE(actor.magic->knows_spell(spell_id("test_preview_spell")));
    CHECK(actor.magic->get_spell(spell_id("test_preview_spell")).get_level() == 1);
    actor.add_bionic(bionic_id("test_preview_spell_upgrade_cbm"));
    CHECK(actor.magic->get_spell(spell_id("test_preview_spell")).get_level() == 3);
    actor.add_bionic(bionic_id("test_preview_power_blocked_cbm"));
    actor.add_bionic(bionic_id("test_preview_fuel_blocked_cbm"));
    CHECK_FALSE(actor.has_active_bionic(bionic_id("test_preview_power_blocked_cbm")));
    CHECK_FALSE(actor.has_active_bionic(bionic_id("test_preview_fuel_blocked_cbm")));
    CHECK(saved_messages() != messages);

    // The same real objects become eligible when actual native inputs are supplied.
    actor.set_max_power_level(2_kJ);
    actor.set_power_level(2_kJ);
    const auto powered = std::ranges::
        find(*actor.my_bionics, bionic_id("test_preview_power_blocked_cbm"), &bionic::id);
    REQUIRE(powered != actor.my_bionics->end());
    CHECK(actor.activate_bionic(*powered));
    CHECK(actor.get_power_level() == 1_kJ);
    actor.set_value("battery", "1");
    const auto fueled = std::ranges::
        find(*actor.my_bionics, bionic_id("test_preview_fuel_blocked_cbm"), &bionic::id);
    REQUIRE(fueled != actor.my_bionics->end());
    CHECK(actor.activate_bionic(*fueled));
    CHECK(actor.has_active_bionic(bionic_id("test_preview_fuel_blocked_cbm")));
}

#    if defined(TILES)

TEST_CASE(
    "native character preview lifetime preserves gameplay authority and overlays",
    "[character_preview][tiles][presentation_purity]") {
    const auto fixture = preview_test_state{};
    const auto display = native_preview_display{};
    const auto tiles = tile_context_fixture{true};
    REQUIRE(tiles.valid());
    auto& actor = get_avatar();
    actor.set_mutation(trait_id("test_preview_trait"));
    actor.set_mutation(trait_id("test_preview_active_trait"));
    actor.add_effect(efftype_id("test_preview_effect"), 1_hours);
    actor.add_bionic(bionic_id("test_preview_installed_cbm"));

    const auto first_clothed = GENERATE(true, false);
    const auto nested_context = GENERATE(false, true);
    CAPTURE(first_clothed, nested_context);
    rng_set_deterministic_seed(92825);
    // Nonzero root/child cursors detect accidental allocation, not just engine draws.
    REQUIRE(rng_next_deterministic_call_seed(0x74657374));
    auto& main_engine = rng_get_engine();
    const auto root_before = observe_deterministic_rng();

    const auto exercise = [&]() {
        const auto baseline = preview_authority{main_engine};
        REQUIRE(baseline.context);
        // Prove the snapshot helpers themselves are observational before rendering.
        baseline.check(main_engine);
        auto observed = std::vector<std::vector<std::string>>{};
        const auto observer = preview_observer_guard{
            [&](const auto& temporary, const auto entries) {
                CHECK(&temporary != &actor);
                CHECK(temporary.get_max_power_level() == 18_kJ);
                CHECK(temporary.has_bionic(bionic_id("test_preview_profession_cbm")));
                CHECK(temporary.has_bionic(bionic_id("test_preview_included_cbm")));
                CHECK(temporary.has_bionic(bionic_id("test_preview_installed_cbm")));
                const auto clothed = std::ranges::
                    contains(entries, "worn_test_preview_coat", &Character::overlay_entry::id);
                CHECK(temporary.worn.size() == (clothed ? 1u : 0u));
                if (clothed) {
                    const auto* coat = temporary.item_worn_with_id(itype_id("test_preview_coat"));
                    REQUIRE(coat);
                    CHECK(coat->has_flag(flag_id("FIT")));
                }
                observed.push_back(
                    entries | std::views::transform(&Character::overlay_entry::id)
                    | std::ranges::to<std::vector>());
            }};
        for (const auto lifetime : std::views::iota(0, 2)) {
            CAPTURE(lifetime);
            {
                auto preview = game_client::make_character_preview();
                REQUIRE(preview);
                preview->init(&actor);
                baseline.check(main_engine);
                const auto orientation = character_preview_window::Orientation{};
                preview->prepare(
                    {.nlines = 12, .ncols = 12, .orientation = &orientation, .hide_below_ncols = 0});
                if (!first_clothed) { preview->toggle_clothes(); }
                baseline.check(main_engine);
                for (const auto with_clothing : {first_clothed, !first_clothed}) {
                    CAPTURE(with_clothing);
                    for (const auto render : std::views::iota(0, 2)) {
                        CAPTURE(render);
                        const auto calls_before = observed.size();
                        preview->display();
                        REQUIRE(observed.size() == calls_before + 1);
                        CHECK(observed.back() == expected_overlays(with_clothing));
                        baseline.check(main_engine);
                    }
                    preview->toggle_clothes();
                }
                preview->clear();
                baseline.check(main_engine);
            }
            baseline.check(main_engine);
        }
    };

    if (nested_context) {
        {
            const auto outer = rng_deterministic_task_scope{731};
            REQUIRE(rng_next_deterministic_call_seed(0x74657374));
            exercise();
        }
        // Separately observe the suspended main/root context after the outer task ends.
        CHECK(observe_deterministic_rng() == root_before);
    } else {
        exercise();
    }
}

TEST_CASE(
    "native preview display cleans up a failed initialization",
    "[character_preview][tiles][graphics_setup]") {
    REQUIRE_FALSE(tilecontext);
    REQUIRE_FALSE(get_sdl_renderer());
    REQUIRE_THROWS_AS(
        native_preview_display{[]() {
            catacurses::init_interface();
            REQUIRE(tilecontext);
            throw std::runtime_error("Injected failure after native graphics allocation");
        }},
        std::runtime_error);
    CHECK_FALSE(tilecontext);
    CHECK_FALSE(overmap_tilecontext);
    CHECK_FALSE(get_sdl_renderer());
    // Native initialization must still work after failure cleanup.
    {
        const auto display = native_preview_display{};
        CHECK(tilecontext);
        CHECK(get_sdl_renderer());
    }
    CHECK_FALSE(tilecontext);
    CHECK_FALSE(get_sdl_renderer());
}

TEST_CASE(
    "native preview retains active eligibility and spell installation across RNG modes",
    "[character_preview][tiles][presentation_purity][active_spells]") {
    const auto fixture = preview_test_state{};
    const auto display = native_preview_display{};
    const auto tiles = tile_context_fixture{true};
    REQUIRE(tiles.valid());
    auto& actor = get_avatar();
    actor.prof = profession_id("test_preview_active_profession");
    actor.add_bionic(bionic_id("test_preview_spell_upgrade_cbm"));
    REQUIRE_FALSE(rng_deterministic_seed_active());
    const auto deterministic = GENERATE(false, true);
    CAPTURE(deterministic);
    if (deterministic) {
        rng_set_deterministic_seed(92826);
        REQUIRE(rng_next_deterministic_call_seed(0x74657374));
    }
    auto& main = rng_get_engine();
    const auto baseline = preview_authority{main};
    baseline.check(main);
    auto observations = 0;
    auto inscriptions = std::vector<std::string>{};
    const auto observer = preview_observer_guard{[&](const auto& temporary, const auto entries) {
        ++observations;
        CHECK(temporary.get_power_level() == 0_J);
        CHECK(temporary.has_active_bionic(bionic_id("test_preview_active_cbm")));
        CHECK_FALSE(temporary.has_active_bionic(bionic_id("test_preview_power_blocked_cbm")));
        CHECK_FALSE(temporary.has_active_bionic(bionic_id("test_preview_fuel_blocked_cbm")));
        CHECK(temporary.has_trait(trait_id("test_preview_spell_class")));
        REQUIRE(temporary.magic->knows_spell(spell_id("test_preview_spell")));
        CHECK(temporary.magic->get_spell(spell_id("test_preview_spell")).get_level() == 3);
        CHECK(std::ranges::contains(
            entries, "mutation_active_test_preview_active_cbm", &Character::overlay_entry::id));
        CHECK(std::ranges::contains(
            entries, "mutation_test_preview_power_blocked_cbm", &Character::overlay_entry::id));
        CHECK(std::ranges::contains(
            entries, "mutation_test_preview_fuel_blocked_cbm", &Character::overlay_entry::id));
        const auto* coat = temporary.item_worn_with_id(itype_id("test_preview_coat"));
        REQUIRE(coat);
        CHECK(coat->has_flag(flag_id("FIT")));
        REQUIRE_FALSE(coat->snip_id.is_null());
        inscriptions.push_back(coat->snip_id.str());
    }};
    for (const auto lifetime : std::views::iota(0, 2)) {
        CAPTURE(lifetime);
        {
            auto preview = game_client::make_character_preview();
            REQUIRE(preview);
            preview->init(&actor);
            baseline.check(main);
            const auto orientation = character_preview_window::Orientation{};
            preview->prepare(
                {.nlines = 12, .ncols = 12, .orientation = &orientation, .hide_below_ncols = 0});
            baseline.check(main);
            preview->display();
            baseline.check(main);
            preview->zoom_out();
            preview->display();
            baseline.check(main);
            preview->clear();
            baseline.check(main);
        }
        baseline.check(main);
    }
    CHECK(observations == 4);
    REQUIRE(inscriptions.size() == 4);
    CHECK(std::ranges::all_of(inscriptions, [&](const auto& note) {
        return note == inscriptions.front();
    }));
}

#    endif // TILES

#endif // CATA_NATIVE_PREVIEW_TEST
