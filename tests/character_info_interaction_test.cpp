#if defined(CATA_MCP)

#    include "avatar.h"
#    include "bionics.h"
#    include "cata_utility.h"
#    include "catalua.h"
#    include "catalua_impl.h"
#    include "catch/catch.hpp"
#    include "character_display.h"
#    include "client_command.h"
#    include "client_input.h"
#    include "client_interaction_prepared.h"
#    include "client_memory.h"
#    include "client_memory_scope.h"
#    include "cursesdef.h"
#    include "cursesport.h"
#    include "effect.h"
#    include "game.h"
#    include "init.h"
#    include "input.h"
#    include "json.h"
#    include "kill_tracker.h"
#    include "map/map.h"
#    include "map/submap.h"
#    include "mutation.h"
#    include "newcharacter.h"
#    include "npc.h"
#    include "options_helpers.h"
#    include "output.h"
#    include "path_info.h"
#    include "replay/replay.h"
#    include "rng.h"
#    include "skill.h"
#    include "state_helpers.h"

#    include <array>
#    include <atomic>
#    include <filesystem>
#    include <functional>
#    include <map>
#    include <ranges>
#    include <sstream>
#    include <stdexcept>
#    include <vector>

namespace {
namespace client = game_client;

auto actor_state() -> std::string {
    auto stream = std::ostringstream{};
    auto json = JsonOut{stream};
    get_avatar().serialize(json);
    return stream.str();
}

// Compare complete saved state across native load round trips. JSON object order is not
// semantic (mutation storage is unordered); array order and exact scalar tokens remain strict.
auto saved_value_fingerprint(JsonIn& json) -> std::string {
    auto stream = std::ostringstream{};
    auto output = JsonOut{stream};
    if (json.test_object()) {
        const auto object = json.get_object();
        auto fields = std::map<std::string, std::string>{};
        for (const auto& member : object) {
            fields.emplace(member.name(), saved_value_fingerprint(*object.get_raw(member.name())));
        }
        output.write(fields);
        return "object:" + stream.str();
    }
    if (json.test_array()) {
        auto values = std::vector<std::string>{};
        json.start_array();
        // Stream consumption advances the parser, so this is not a collection iterator loop.
        while (!json.end_array()) { values.push_back(saved_value_fingerprint(json)); }
        output.write(values);
        return "array:" + stream.str();
    }
    json.eat_whitespace();
    const auto start = json.tell();
    json.skip_value();
    auto scalar = json.substr(start, json.tell() - start);
    scalar.erase(scalar.find_last_not_of(" \n\r\t,") + 1);
    return "scalar:" + scalar;
}

auto saved_state_fingerprint(const std::string& state) -> std::string {
    auto stream = std::istringstream{state};
    auto json = JsonIn{stream};
    return saved_value_fingerprint(json);
}

auto kills_state() -> std::string {
    auto stream = std::ostringstream{};
    auto json = JsonOut{stream};
    g->get_kill_tracker().serialize(json);
    return stream.str();
}

auto world_state() -> std::string {
    auto stream = std::ostringstream{};
    auto json = JsonOut{stream};
    json.start_array();
    for (const auto& view : get_map().active_submap_views()) {
        json.start_object();
        json.member("position", view.abs_pos());
        view.get_submap().store(json);
        json.end_object();
    }
    json.end_array();
    return stream.str();
}

struct character_info_guard {
    client::memory::scoped_state memory;
    bool old_test_mode = test_mode;
    int old_termx = TERMX;
    int old_termy = TERMY;
    int old_width = FULL_SCREEN_WIDTH;
    int old_height = FULL_SCREEN_HEIGHT;
    std::string old_kills = kills_state();
    cata_default_random_engine old_rng = rng_get_engine();
    std::optional<unsigned int> old_seed = rng_deterministic_seed_value();
    avatar old_actor = std::move(get_avatar());

    character_info_guard() {
        // The general reset retains training, recipes and chargen metadata. Start with the
        // actual native constructor, and preserve the incoming actor for exact cleanup.
        get_avatar() = avatar{};
        clear_all_state();
        // Match native loaded characters: general test reset removes cosmetic defaults.
        newcharacter::add_default_mutation_type_traits(get_avatar());
        test_mode = false;
        TERMX = 100;
        TERMY = 60;
        FULL_SCREEN_WIDTH = 80;
        FULL_SCREEN_HEIGHT = 24;
        client::memory::resize(TERMX, TERMY);
        catacurses::stdscr = catacurses::newwin(TERMY, TERMX, point_zero);
        catacurses::newscr = catacurses::newwin(TERMY, TERMX, point_zero);
        g->get_kill_tracker().clear();
    }

    ~character_info_guard() {
        client::memory::set_input_provider({});
        // clear_character feeds debug nutrition and expects the ordinary test helper mode.
        // Keep native widget mode only while exercising the view, not during fixture cleanup.
        test_mode = true;
        clear_all_state();
        get_avatar() = std::move(old_actor);
        auto stream = std::istringstream{old_kills};
        auto json = JsonIn{stream};
        g->get_kill_tracker().deserialize(json);
        TERMX = old_termx;
        TERMY = old_termy;
        FULL_SCREEN_WIDTH = old_width;
        FULL_SCREEN_HEIGHT = old_height;
        test_mode = old_test_mode;
        if (old_seed) {
            rng_set_deterministic_seed(*old_seed);
        } else {
            rng_clear_deterministic_seed();
        }
        rng_get_engine() = old_rng;
    }
};

auto resolve(client::interaction_command command) -> input_event {
    auto input = client::input_command{};
    input.interaction = std::move(command);
    const auto result = client::resolve_input_command(input, client::memory::screen_size());
    REQUIRE(result.has_value());
    return *result;
}

auto action(const std::string& name) -> input_event {
    auto input = client::input_command{};
    input.action = name;
    const auto result = client::resolve_input_command(input, client::memory::screen_size());
    REQUIRE(result.has_value());
    return *result;
}

auto choice(const client::interaction_snapshot& snapshot, const std::string& label)
    -> client::interaction_choice {
    const auto found =
        std::ranges::find(snapshot.choices, label, &client::interaction_choice::label);
    REQUIRE(found != snapshot.choices.end());
    return *found;
}

auto choose(const client::interaction_snapshot& snapshot, const std::string& label) -> input_event {
    return resolve(
        {.input_id = snapshot.input_id,
         .operation = client::interaction_operation::choose,
         .target_id = choice(snapshot, label).id});
}

auto cancel(const client::interaction_snapshot& snapshot) -> input_event {
    return resolve(
        {.input_id = snapshot.input_id, .operation = client::interaction_operation::cancel});
}

/// Materialize every page, not merely a truncated prefix when simultaneous panes exceed 200.
auto complete_snapshot() -> client::interaction_snapshot {
    auto snapshot = client::current_interaction({.limit = 200});
    while (snapshot.choices.size() < snapshot.choice_total) {
        const auto page = client::current_interaction(
            {.offset = snapshot.choices.size(), .limit = 200});
        REQUIRE_FALSE(page.choices.empty());
        CHECK(page.input_id == snapshot.input_id);
        CHECK(page.schema_id == snapshot.schema_id);
        snapshot.choices.insert(snapshot.choices.end(), page.choices.begin(), page.choices.end());
    }
    return snapshot;
}

/// Includes the very first passive read, not merely repeated reads of an already observed model.
auto observe() -> client::interaction_snapshot {
    const auto before = actor_state();
    const auto kills = kills_state();
    const auto world = world_state();
    const auto rng = rng_get_engine();
    client::reset_interaction_work();
    const auto snapshot = complete_snapshot();
    const auto repeat = complete_snapshot();
    CHECK(client::serialize_interaction(snapshot) == client::serialize_interaction(repeat));
    auto rows = snapshot.choices.size() + repeat.choices.size();
    for (const auto offset : std::views::iota(std::size_t{0}, snapshot.choice_total)) {
        const auto page = client::current_interaction({.offset = offset, .limit = 1});
        REQUIRE(page.choices.size() == 1);
        CHECK(page.choices[0].id == snapshot.choices[offset].id);
        CHECK(page.choices[0].description == snapshot.choices[offset].description);
        rows += page.choices.size();
    }
    CHECK(client::interaction_work().schema_hashes == 0);
    CHECK(client::interaction_work().hashed_choices == 0);
    CHECK(client::interaction_work().materialized_choices == rows);
    CHECK(actor_state() == before);
    CHECK(kills_state() == kills);
    CHECK(world_state() == world);
    CHECK(rng_get_engine() == rng);
    return snapshot;
}

} // namespace

TEST_CASE(
    "character info saved-state comparison preserves every scalar and ordered array",
    "[character_info_interaction]") {
    const auto baseline = saved_state_fingerprint(
        R"({"a":[1,null,true," x,"],"b":{"n":18446744073709551615,"f":0.123456789123456789}})");
    CHECK(
        baseline
        == saved_state_fingerprint(
            R"({"b":{"f":0.123456789123456789,"n":18446744073709551615},"a":[1,null,true," x,"]})"));
    CHECK(
        baseline
        != saved_state_fingerprint(
            R"({"a":[null,1,true," x,"],"b":{"n":18446744073709551615,"f":0.123456789123456789}})"));
    CHECK(
        baseline
        != saved_state_fingerprint(
            R"({"a":[1,null,false," x,"],"b":{"n":18446744073709551615,"f":0.123456789123456789}})"));
    CHECK(
        baseline
        != saved_state_fingerprint(
            R"({"a":[1,null,true," x,"],"b":{"n":18446744073709551614,"f":0.123456789123456789}})"));
    CHECK(
        baseline
        != saved_state_fingerprint(
            R"({"a":[1,null,true," x,"],"b":{"n":18446744073709551615,"f":0.123456789123456788}})"));
    CHECK(
        baseline
        != saved_state_fingerprint(
            R"({"a":[1,null,true,"x,"],"b":{"n":18446744073709551615,"f":0.123456789123456789}})"));
    CHECK(
        baseline
        != saved_state_fingerprint(R"({"a":[1,null,true," x,"],"b":{"n":18446744073709551615}})"));
}

TEST_CASE(
    "character info native tabs disclose full details without passive actor or RNG work",
    "[character_info_interaction]") {
    const auto guard = character_info_guard{};
    auto& you = get_avatar();
    you.set_mutation(trait_id("test_character_info_visible_trait"));
    you.set_mutation(trait_id("test_character_info_hidden_trait"));
    you.add_effect(efftype_id("test_character_info_visible_effect"), 10_turns);
    you.add_effect(efftype_id("test_character_info_hidden_effect"), 10_turns);
    you.add_bionic(bionic_id("test_character_info_cbm"));
    you.add_bionic(bionic_id("test_character_info_cbm"));
    const auto training = you.get_skill_level_object(skill_id("melee")).isTraining();
    const auto before = actor_state();
    const auto rng = rng_get_engine();
    auto reads = 0;
    client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto snapshot = observe();
        REQUIRE(snapshot.context == "PLAYER_INFO");
        REQUIRE(snapshot.structured);
        CHECK(snapshot.panes.size() == 8);
        switch (reads++) {
            case 0:
                CHECK(choice(snapshot, "Strength:").description.find("Base HP:")
                      != std::string::npos);
                CHECK(choice(snapshot, "Dexterity:").description.find("Throwing penalty")
                      != std::string::npos);
                CHECK(choice(snapshot, "Intelligence:").description.find("Crafting bonus")
                      != std::string::npos);
                CHECK(choice(snapshot, "Perception:").description.find("Trap detection")
                      != std::string::npos);
                return choose(snapshot, "ENCUMBRANCE AND WARMTH");
            case 1: {
                const auto row = std::ranges::find_if(snapshot.choices, [](const auto& entry) {
                    return entry.pane_id == "encumbrance";
                });
                REQUIRE(row != snapshot.choices.end());
                CHECK_FALSE(row->columns.empty());
                CHECK_FALSE(row->description.empty());
                return choose(snapshot, "TRAITS");
            }
            case 2:
                CHECK(choice(snapshot, "TEST character info visible trait")
                          .description.find("last sentence")
                      != std::string::npos);
                CHECK(std::ranges::none_of(snapshot.choices, [](const auto& entry) {
                    return entry.label == "TEST character info hidden trait";
                }));
                CHECK_FALSE(client::resolve_interaction_command(
                    {.input_id = snapshot.input_id,
                     .operation = client::interaction_operation::choose,
                     .target_id = client::opaque_interaction_id(
                         "character-trait", {"test_character_info_hidden_trait"})}));
                return choose(snapshot, "BIONICS");
            case 3:
                CHECK(
                    choice(snapshot, "TEST character info CBM (2)").description.find("2 instances")
                    != std::string::npos);
                CHECK(
                    snapshot.panes[4].area_description.find("Bionic Power:") != std::string::npos);
                return choose(snapshot, "EFFECTS");
            case 4:
                CHECK(
                    choice(snapshot, "TEST character info visible effect").description
                    == you.get_effect(efftype_id("test_character_info_visible_effect")).disp_desc());
                CHECK(std::ranges::none_of(snapshot.choices, [](const auto& entry) {
                    return entry.description.find("Undisclosed") != std::string::npos;
                }));
                return choose(snapshot, "SKILLS");
            case 5:
                return choose(snapshot, "Melee");
            case 6:
                CHECK(choice(snapshot, "Melee").highlighted);
                CHECK(you.get_skill_level_object(skill_id("melee")).isTraining() == training);
                CHECK(actor_state() == before);
                CHECK(rng_get_engine() == rng);
                return cancel(snapshot);
            default:
                FAIL("unexpected native read");
                return input_event{};
        }
    });
    character_display::disp_info(you);
    CHECK(reads == 7);
    CHECK(actor_state() == before);
    CHECK(rng_get_engine() == rng);
}

TEST_CASE(
    "character info exposes simultaneous native panes independently of focus",
    "[character_info_interaction][character_info_cross_pane_red]") {
    const auto guard = character_info_guard{};
    auto& you = get_avatar();
    you.set_mutation(trait_id("test_character_info_visible_trait"));
    you.add_effect(efftype_id("test_character_info_visible_effect"), 10_turns);
    you.add_bionic(bionic_id("test_character_info_cbm"));
    const auto before = actor_state();
    const auto rng = rng_get_engine();
    auto reads = 0;
    client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto snapshot = complete_snapshot();
        REQUIRE(snapshot.context == "PLAYER_INFO");
        REQUIRE(snapshot.structured);
        for (const auto* pane :
             {"stats", "encumbrance", "skills", "traits", "bionics", "effects"}) {
            CAPTURE(reads, pane);
            CHECK(std::ranges::any_of(snapshot.choices, [&](const auto& row) {
                return row.pane_id == pane && row.selectable;
            }));
        }
        const auto power =
            std::ranges::find(snapshot.panes, "bionics", &client::interaction_pane::id);
        REQUIRE(power != snapshot.panes.end());
        CHECK(power->area_description.find("Bionic Power:") != std::string::npos);
        CHECK(actor_state() == before);
        CHECK(rng_get_engine() == rng);
        if (reads++ == 0) { return choose(snapshot, "SKILLS"); }
        REQUIRE(reads == 2);
        return cancel(snapshot);
    });
    character_display::disp_info(you);
    CHECK(reads == 2);
    CHECK(actor_state() == before);
    CHECK(rng_get_engine() == rng);
}

TEST_CASE(
    "character info cold preparation and unchanged reads have distinct work bounds",
    "[character_info_interaction]") {
    const auto guard = character_info_guard{};
    client::reset_interaction_work();
    auto step = 0;
    auto first_rows = std::size_t{};
    auto first_input = std::uint64_t{};
    client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto cold = client::interaction_work();
        const auto snapshot = complete_snapshot();
        const auto observed = client::interaction_work();
        CHECK(observed.schema_hashes == cold.schema_hashes);
        CHECK(observed.hashed_choices == cold.hashed_choices);
        CHECK(observed.materialized_choices - cold.materialized_choices == snapshot.choice_total);
        if (step == 0) {
            CHECK(cold.schema_hashes == 1);
            CHECK(cold.hashed_choices == snapshot.choice_total);
            CHECK(cold.materialized_choices == 0);
            first_rows = snapshot.choice_total;
            first_input = snapshot.input_id;
        } else if (step == 1) {
            CHECK(snapshot.input_id > first_input);
            CHECK(cold.schema_hashes == 1);
            CHECK(cold.hashed_choices == first_rows);
            ++step;
            return choose(snapshot, "SKILLS");
        } else {
            CHECK(cold.schema_hashes == 2);
            CHECK(cold.hashed_choices == first_rows + snapshot.choice_total);
            ++step;
            return cancel(snapshot);
        }
        ++step;
        return input_event{'~', input_event_t::keyboard};
    });
    character_display::disp_info(get_avatar());
    CHECK(step == 3);
}

TEST_CASE(
    "character info complete paging retains off-viewport rows and exact focus",
    "[character_info_interaction]") {
    const auto guard = character_info_guard{};
    auto& you = get_avatar();
    for (const auto i : std::views::iota(0, 205)) {
        you.set_mutation(trait_id("test_character_info_page_" + std::to_string(i)));
    }
    auto step = 0;
    client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto snapshot = observe();
        REQUIRE(snapshot.choice_total > 200);
        CHECK(snapshot.choices.size() == snapshot.choice_total);
        const auto traits =
            std::ranges::count(snapshot.choices, "traits", &client::interaction_choice::pane_id);
        CHECK(traits >= 205);
        CHECK(
            std::ranges::count_if(
                snapshot.choices,
                [](const auto& row) {
                    return row.pane_id == "traits"
                        && row.label.starts_with("TEST character info page ");
                })
            == 205);
        const auto pane =
            std::ranges::find(snapshot.panes, "traits", &client::interaction_pane::id);
        REQUIRE(pane != snapshot.panes.end());
        CHECK(pane->area_description.find("of " + std::to_string(traits)) != std::string::npos);
        if (step++ == 0) { return choose(snapshot, "TEST character info page 204"); }
        CHECK(choice(snapshot, "TEST character info page 204").highlighted);
        CHECK(std::ranges::none_of(snapshot.choices, [](const auto& row) {
            return row.pane_id != "traits" && row.highlighted && row.pane_id.has_value();
        }));
        CHECK(pane->area_description.find(
                  "-" + std::to_string(traits) + " of " + std::to_string(traits))
              != std::string::npos);
        return cancel(snapshot);
    });
    character_display::disp_info(you);
    CHECK(step == 2);
}

TEST_CASE(
    "character info fixture restores borrowed window geometry without resizing it",
    "[character_info_interaction]") {
    const auto original_stdscr = catacurses::stdscr;
    const auto original_newscr = catacurses::newscr;
    const auto cleanup = on_out_of_scope{[&]() {
        catacurses::stdscr = original_stdscr;
        catacurses::newscr = original_newscr;
    }};
    const auto borrowed = catacurses::newwin(2, 3, point_zero);
    catacurses::stdscr = borrowed;
    catacurses::newscr = borrowed;
    {
        const auto guard = character_info_guard{};
        CHECK(getmaxx(borrowed) == 3);
        CHECK(getmaxy(borrowed) == 2);
    }
    CHECK(getmaxx(catacurses::stdscr) == 3);
    CHECK(getmaxy(catacurses::newscr) == 2);
}

TEST_CASE(
    "character info focus and native training execution remain distinct",
    "[character_info_interaction]") {
    const auto guard = character_info_guard{};
    auto& you = get_avatar();
    const auto initial = you.get_skill_level_object(skill_id("melee")).isTraining();
    auto step = 0;
    auto old_input = std::uint64_t{};
    auto old_schema = std::string{};
    client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto snapshot = observe();
        switch (step++) {
            case 0:
                old_input = snapshot.input_id;
                old_schema = snapshot.schema_id;
                return action("NEXT_TAB");
            case 1:
                return action("NEXT_TAB");
            case 2: {
                const auto header = std::ranges::find_if(snapshot.choices, [](const auto& entry) {
                    return !entry.selectable;
                });
                REQUIRE(header != snapshot.choices.end());
                CHECK_FALSE(client::resolve_interaction_command(
                    {.input_id = snapshot.input_id,
                     .operation = client::interaction_operation::choose,
                     .target_id = header->id}));
                CHECK_FALSE(client::resolve_interaction_command(
                    {.input_id = snapshot.input_id,
                     .operation = client::interaction_operation::choose,
                     .target_id = "foreign"}));
                CHECK_FALSE(client::resolve_interaction_command(
                    {.input_id = old_input,
                     .operation = client::interaction_operation::choose,
                     .target_id = header->id}));
                CHECK(snapshot.schema_id != old_schema);
                REQUIRE(client::active_input_context().context != nullptr);
                CHECK_FALSE(client::validate_interaction_event(
                    *client::active_input_context().context,
                    {.operation = client::interaction_operation::choose,
                     .schema_id = old_schema,
                     .target_id = choice(snapshot, "STATS").id}));
                return choose(snapshot, "Melee");
            }
            case 3:
                CHECK(you.get_skill_level_object(skill_id("melee")).isTraining() == initial);
                return choose(snapshot, "Toggle skill training");
            case 4:
                CHECK(you.get_skill_level_object(skill_id("melee")).isTraining() != initial);
                return action("CONFIRM");
            case 5:
                CHECK(you.get_skill_level_object(skill_id("melee")).isTraining() == initial);
                return cancel(snapshot);
            default:
                FAIL("unexpected native read");
                return input_event{};
        }
    });
    character_display::disp_info(you);
    CHECK(step == 6);
}

TEST_CASE(
    "character info stat controls retain native denial and nested confirmation",
    "[character_info_interaction]") {
    const auto guard = character_info_guard{};
    const auto enabled = override_option{"STATS_THROUGH_KILLS", "true"};
    const auto coefficient = override_option{"AVATAR_LEVEL_XP_COEFF", "1"};
    auto& you = get_avatar();
    auto grant = false;
    auto raise = false;
    SECTION("denied XP remains selectable and reaches native warning") {}
    SECTION("upgrade cancel") { grant = true; }
    SECTION("upgrade confirm") {
        grant = true;
        raise = true;
    }
    if (grant) {
        g->get_kill_tracker().add_monster(mtype_id("mon_zombie"));
        REQUIRE(you.free_upgrade_points() > 0);
    }
    const auto initial = you.get_str_base();
    const auto points = you.free_upgrade_points();
    auto step = 0;
    auto nested = false;
    client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto snapshot = client::current_interaction();
        if (snapshot.context != "PLAYER_INFO") {
            nested = true;
            CHECK_FALSE(std::ranges::any_of(snapshot.choices, [](const auto& entry) {
                return entry.pane_id == "controls";
            }));
            if (grant) { return action(raise ? "YES" : "NO"); }
            CHECK(snapshot.message.find("experience") != std::string::npos);
            return choose(snapshot, snapshot.choices.front().label);
        }
        if (step++ == 0) {
            const auto control = choice(snapshot, "Upgrade stat");
            CHECK(control.enabled == grant);
            CHECK(control.selectable);
            return choose(snapshot, "Upgrade stat");
        }
        CHECK(you.get_str_base() == initial + (raise ? 1 : 0));
        CHECK(you.free_upgrade_points() == points - (raise ? 1 : 0));
        return cancel(snapshot);
    });
    character_display::disp_info(you);
    CHECK(nested);
    CHECK(step == 2);
}

TEST_CASE(
    "character info preserves CQB disclosure and native training toggle",
    "[character_info_interaction]") {
    const auto guard = character_info_guard{};
    auto& you = get_avatar();
    you.add_bionic(bionic_id("bio_cqb"));
    you.get_bionic_state(bionic_id("bio_cqb")).powered = true;
    const auto initial = you.get_skill_level_object(skill_id("melee")).isTraining();
    auto step = 0;
    client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto snapshot = observe();
        switch (step++) {
            case 0:
                return choose(snapshot, "SKILLS");
            case 1:
                return choose(snapshot, "Melee");
            case 2:
            case 3: {
                const auto row = choice(snapshot, "Melee");
                CHECK(std::ranges::none_of(row.columns, [](const auto& column) {
                    return column.label == "Training";
                }));
                CHECK(row.columns[0].value == std::to_string(BIO_CQB_LEVEL));
                CHECK(row.columns[1].value == "0%");
                CHECK(row.columns[2].value == string_from_color(h_yellow));
                if (step == 3) { return choose(snapshot, "Toggle skill training"); }
                CHECK(you.get_skill_level_object(skill_id("melee")).isTraining() != initial);
                return cancel(snapshot);
            }
            default:
                FAIL("unexpected native read");
                return input_event{};
        }
    });
    character_display::disp_info(you);
    CHECK(step == 4);
}

TEST_CASE(
    "character info does not offer stat execution when native restrictions exclude it",
    "[character_info_interaction]") {
    const auto guard = character_info_guard{};
    const auto disabled = override_option{"STATS_THROUGH_KILLS", "false"};
    auto& you = get_avatar();
    const auto before = you.get_str_base();
    auto step = 0;
    client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto snapshot = observe();
        CHECK(std::ranges::none_of(snapshot.choices, [](const auto& entry) {
            return entry.label == "Upgrade stat";
        }));
        if (step++ == 0) { return action("CONFIRM"); }
        CHECK(you.get_str_base() == before);
        return cancel(snapshot);
    });
    character_display::disp_info(you);
    CHECK(step == 2);
}

TEST_CASE(
    "character info avatar-only upgrades stay unavailable for a real NPC",
    "[character_info_interaction]") {
    const auto guard = character_info_guard{};
    const auto enabled = override_option{"STATS_THROUGH_KILLS", "true"};
    auto non_avatar = npc{};
    const auto initial = non_avatar.get_str_base();
    auto step = 0;
    client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto snapshot = client::current_interaction();
        REQUIRE(snapshot.context == "PLAYER_INFO");
        CHECK(std::ranges::none_of(snapshot.choices, [](const auto& entry) {
            return entry.label == "Upgrade stat";
        }));
        if (step++ == 0) { return action("CONFIRM"); }
        CHECK(non_avatar.get_str_base() == initial);
        return cancel(snapshot);
    });
    character_display::disp_info(non_avatar);
    CHECK(step == 2);
}

TEST_CASE(
    "character info name fields preserve draft submit cancel and nested restoration",
    "[character_info_interaction]") {
    const auto guard = character_info_guard{};
    auto& you = get_avatar();
    you.name = "Before";
    auto step = 0;
    client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto snapshot = client::current_interaction();
        switch (step++) {
            case 0:
                return choose(snapshot, "Change name");
            case 1:
                REQUIRE(snapshot.field);
                CHECK(snapshot.field->max_length == 50);
                CHECK(snapshot.choices.empty());
                CHECK_FALSE(client::resolve_interaction_command({
                    .input_id = snapshot.input_id,
                    .operation = client::interaction_operation::fill,
                    .target_id = snapshot.field->id,
                    .value = std::string(51, 'x'),
                    .submit = true,
                }));
                CHECK_FALSE(client::resolve_interaction_command({
                    .input_id = snapshot.input_id,
                    .operation = client::interaction_operation::fill,
                    .target_id = snapshot.field->id,
                    .value = "Missing submit",
                }));
                CHECK_FALSE(client::resolve_interaction_command(
                    {.input_id = snapshot.input_id,
                     .operation = client::interaction_operation::choose,
                     .target_id = "foreign"}));
                return resolve(
                    {.input_id = snapshot.input_id,
                     .operation = client::interaction_operation::fill,
                     .target_id = snapshot.field->id,
                     .value = "After",
                     .submit = false});
            case 2:
                REQUIRE(snapshot.field);
                CHECK(snapshot.field->value == "After");
                CHECK(you.name == "Before");
                return resolve(
                    {.input_id = snapshot.input_id,
                     .operation = client::interaction_operation::fill,
                     .target_id = snapshot.field->id,
                     .value = "After",
                     .submit = true});
            case 3:
                CHECK(snapshot.context == "PLAYER_INFO");
                CHECK(snapshot.message.find("After") != std::string::npos);
                CHECK(you.name == "After");
                return choose(snapshot, "Change profession name");
            case 4:
                REQUIRE(snapshot.field);
                CHECK(snapshot.field->max_length == 25);
                CHECK_FALSE(client::resolve_interaction_command({
                    .input_id = snapshot.input_id,
                    .operation = client::interaction_operation::fill,
                    .target_id = snapshot.field->id,
                    .value = std::string(26, 'x'),
                    .submit = true,
                }));
                return resolve(
                    {.input_id = snapshot.input_id,
                     .operation = client::interaction_operation::fill,
                     .target_id = snapshot.field->id,
                     .value = "Survivor",
                     .submit = true});
            case 5:
                CHECK(you.custom_profession == "Survivor");
                CHECK(snapshot.message.find("Survivor") != std::string::npos);
                return action("CHANGE_NAME");
            case 6:
                REQUIRE(snapshot.field);
                return cancel(snapshot);
            case 7:
                CHECK(snapshot.context == "PLAYER_INFO");
                // Native caller assigns the canceled popup text (empty), not the old name.
                CHECK(you.name.empty());
                return cancel(snapshot);
            default:
                FAIL("unexpected native read");
                return input_event{};
        }
    });
    character_display::disp_info(you);
    CHECK(step == 8);
}

TEST_CASE(
    "character info native real-object skill hooks are prepared not passively reinvoked",
    "[character_info_interaction]") {
    const auto guard = character_info_guard{};
    auto& you = get_avatar();
    auto* state = DynamicDataLoader::get_instance().lua.get();
    REQUIRE(state != nullptr);
    auto& lua = state->lua;
    auto hooks = lua["game"]["hooks"].get<sol::table>();
    const auto original_info = hooks["on_character_display_skill_info"].get<sol::table>();
    const auto original_action = hooks["on_character_display_skill_action"].get<sol::table>();
    const auto cleanup = on_out_of_scope{[&]() {
        hooks["on_character_display_skill_info"] = original_info;
        hooks["on_character_display_skill_action"] = original_action;
    }};
    auto info_calls = 0;
    auto action_calls = 0;
    auto info_hook = lua.create_table();
    info_hook[1] = [&](sol::table params) {
        CHECK(params["character"].get<Character*>() == &you);
        ++info_calls;
        params["results"]["text"] = "Complete native hook detail, including the final sentence.";
    };
    auto action_hook = lua.create_table();
    action_hook[1] = [&](sol::table params) {
        CHECK(params["character"].get<Character*>() == &you);
        CHECK(params["skill"].get<skill_id>() == skill_id("melee"));
        ++action_calls;
        params["results"]["handled"] = true;
    };
    hooks["on_character_display_skill_info"] = info_hook;
    hooks["on_character_display_skill_action"] = action_hook;
    const auto training = you.get_skill_level_object(skill_id("melee")).isTraining();
    auto step = 0;
    client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto calls = info_calls;
        const auto snapshot = observe();
        CHECK(info_calls == calls);
        switch (step++) {
            case 0:
                return choose(snapshot, "SKILLS");
            case 1:
                CHECK(info_calls == 1);
                return choose(snapshot, "Melee");
            case 2:
                CHECK(choice(snapshot, "Melee").description.find("final sentence")
                      != std::string::npos);
                CHECK(action_calls == 0);
                return choose(snapshot, "Toggle skill training");
            case 3:
                CHECK(action_calls == 1);
                CHECK(you.get_skill_level_object(skill_id("melee")).isTraining() == training);
                return cancel(snapshot);
            default:
                FAIL("unexpected native read");
                return input_event{};
        }
    });
    character_display::disp_info(you);
    CHECK(step == 4);
}

TEST_CASE(
    "character info retains actual pane paint before a focused Lua mutation",
    "[character_info_interaction]") {
    const auto guard = character_info_guard{};
    auto& you = get_avatar();
    auto* state = DynamicDataLoader::get_instance().lua.get();
    REQUIRE(state != nullptr);
    auto hooks = state->lua["game"]["hooks"].get<sol::table>();
    const auto original = hooks["on_character_display_skill_info"].get<sol::table>();
    const auto cleanup = on_out_of_scope{[&]() {
        hooks["on_character_display_skill_info"] = original;
    }};
    auto calls = 0;
    auto hook = state->lua.create_table();
    hook[1] = [&](sol::table params) {
        CHECK(params["character"].get<Character*>() == &you);
        ++calls;
        you.str_max = 11;
        params["results"]["text"] = "Native focused callback changed strength after pane paint.";
    };
    hooks["on_character_display_skill_info"] = hook;
    auto step = 0;
    client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto snapshot = observe();
        const auto& strength = choice(snapshot, "Strength:");
        const auto base =
            std::ranges::find(strength.columns, "Base", &client::interaction_column::label);
        REQUIRE(base != strength.columns.end());
        if (step++ == 0) {
            CHECK(calls == 0);
            CHECK(base->value == "8");
            return choose(snapshot, "SKILLS");
        }
        if (step == 2) {
            CHECK(calls == 1);
            CHECK(you.get_str_base() == 11);
            CHECK(base->value == "8");
            return choose(snapshot, "STATS");
        }
        CHECK(calls == 1);
        CHECK(base->value == "11");
        return cancel(snapshot);
    });
    character_display::disp_info(you);
    CHECK(step == 3);
}

TEST_CASE(
    "character info semantic native workflow replays with identical actor and RNG",
    "[character_info_interaction][replay]") {
    const auto guard = character_info_guard{};
    static auto counter = std::atomic_uint64_t{};
    const auto directory =
        std::filesystem::path(PATH_INFO::user_dir())
        / ("character-info-replay-" + std::to_string(counter.fetch_add(1)));
    REQUIRE(std::filesystem::create_directory(directory));
    const auto cleanup = on_out_of_scope{[&]() {
        replay::stop();
        std::filesystem::remove_all(directory);
    }};
    const auto path = directory / "input.jsonl";
    const auto setup = []() {
        const auto restore_mode = restore_on_out_of_scope{test_mode};
        test_mode = true;
        get_avatar() = avatar{};
        clear_all_state();
        newcharacter::add_default_mutation_type_traits(get_avatar());
        get_avatar().name = "Replay actor";
    };
    setup();
    const auto initial_actor = actor_state();
    auto step = 0;
    replay::configure_recording(path.string(), {.rng_seed = 37});
    replay::start();
    client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto snapshot = client::current_interaction();
        switch (step++) {
            case 0:
                return choose(snapshot, "SKILLS");
            case 1:
                return choose(snapshot, "Melee");
            case 2:
                return choose(snapshot, "Toggle skill training");
            case 3:
                return choose(snapshot, "Change name");
            case 4:
                REQUIRE(snapshot.field);
                return resolve(
                    {.input_id = snapshot.input_id,
                     .operation = client::interaction_operation::fill,
                     .target_id = snapshot.field->id,
                     .value = "Recorded actor",
                     .submit = true});
            case 5:
                return cancel(snapshot);
            default:
                FAIL("unexpected native read");
                return input_event{};
        }
    });
    character_display::disp_info(get_avatar());
    const auto recorded = saved_state_fingerprint(actor_state());
    const auto rng = rng_get_engine();
    replay::finish();
    setup();
    auto initial_stream = std::istringstream{initial_actor};
    auto initial_json = JsonIn{initial_stream};
    get_avatar().deserialize(initial_json);
    REQUIRE(saved_state_fingerprint(actor_state()) == saved_state_fingerprint(initial_actor));
    client::memory::set_input_provider([](const int /*timeout*/) -> input_event {
        throw std::runtime_error("character info replay must not read live input");
    });
    replay::configure_playback(path.string(), {.rng_seed = 37});
    replay::start();
    character_display::disp_info(get_avatar());
    CHECK(saved_state_fingerprint(actor_state()) == recorded);
    CHECK(rng_get_engine() == rng);
    CHECK_THROWS_AS(replay::next_input_event(), replay::completed);
    replay::finish();
    CHECK(step == 6);
}

TEST_CASE(
    "character info nested input exceptions release enclosing prepared authority",
    "[character_info_interaction]") {
    const auto guard = character_info_guard{};
    auto step = 0;
    client::memory::set_input_provider([&](const int /*timeout*/) -> input_event {
        const auto snapshot = client::current_interaction();
        if (step++ == 0) { return choose(snapshot, "Change name"); }
        REQUIRE(snapshot.field);
        throw std::runtime_error("character info nested input failure");
    });
    REQUIRE_THROWS_AS(character_display::disp_info(get_avatar()), std::runtime_error);
    CHECK_FALSE(client::current_interaction().structured);
    CHECK(client::active_input_context().context == nullptr);
    CHECK(step == 2);
}

namespace {

struct character_fixture_window_witness {
    catacurses::window identity;
    std::optional<cata_cursesport::WINDOW> contents;
};

auto capture_character_window(const catacurses::window& window)
    -> character_fixture_window_witness {
    const auto* native = window.get();
    return {.identity = window, .contents = native ? std::optional{*native} : std::nullopt};
}

auto check_character_window(
    const catacurses::window& actual, const character_fixture_window_witness& expected) -> void {
    CHECK(actual == expected.identity);
    const auto* native = actual.get();
    REQUIRE(static_cast<bool>(native) == expected.contents.has_value());
    if (!native) { return; }
    const auto& before = *expected.contents;
    CHECK(native->pos == before.pos);
    CHECK(native->width == before.width);
    CHECK(native->height == before.height);
    CHECK(native->cursor == before.cursor);
    CHECK(native->FG == before.FG);
    CHECK(native->BG == before.BG);
    CHECK(native->inuse == before.inuse);
    CHECK(native->draw == before.draw);
    CHECK(std::ranges::equal(native->line, before.line, [](const auto& a, const auto& b) {
        return a.touched == b.touched && a.chars == b.chars;
    }));
}

enum class character_fixture_exit {
    normal,
    nested_input_exception,
    view_constructor_exception,
};

/// Exercise member unwinding when the real character view fails inside an owning constructor.
/// No throw seam is added to character_info_guard itself.
struct constructing_character_view {
    character_info_guard fixture;
    explicit constructing_character_view(const std::function<auto()->void>& show) { show(); }
};

} // namespace

TEST_CASE(
    "character info fixture restores complete native compositor state",
    "[character_info_fixture_restoration]") {
    const auto initialized = GENERATE(false, true);
    const auto exit =
        GENERATE(character_fixture_exit::normal, character_fixture_exit::nested_input_exception,
                 character_fixture_exit::view_constructor_exception);
    CAPTURE(initialized, static_cast<int>(exit));
    // Preserve arbitrary incoming compositor/windows/callbacks in mixed test selections.
    const auto outer_memory = client::memory::scoped_state{};
    REQUIRE(client::memory::screen_size() == point_zero);
    REQUIRE_FALSE(catacurses::stdscr);
    REQUIRE_FALSE(catacurses::newscr);
    if (initialized) {
        // Color configuration is initialized by test_main, not owned by the memory scope.
        client::memory::resize(19, 9);
        // Borrowed virtual windows intentionally differ from the compositor and each other.
        catacurses::stdscr = catacurses::newwin(3, 11, point(2, 1));
        catacurses::newscr = catacurses::newwin(4, 15, point(1, 4));
        catacurses::wattron(catacurses::stdscr, hilite(c_light_red));
        catacurses::mvwprintw(catacurses::stdscr, point(1, 1), "actor α");
        catacurses::wattron(catacurses::newscr, invert_color(c_green));
        catacurses::mvwprintw(catacurses::newscr, point(3, 2), "pane β");
        client::memory::draw_window(catacurses::stdscr);
        client::memory::draw_window(catacurses::newscr);
        catacurses::wmove(catacurses::stdscr, point(8, 2));
        catacurses::wmove(catacurses::newscr, point(9, 3));
    }
    client::memory::set_cursor(1);
    client::memory::set_timeout(43);
    auto incoming_inputs = 0;
    auto incoming_presents = 0;
    auto timeout_seen = std::optional<int>{};
    auto presented = std::optional<client::screen_snapshot>{};
    client::memory::set_input_provider([&](const int timeout) {
        ++incoming_inputs;
        timeout_seen = timeout;
        return input_event{'R', input_event_t::keyboard};
    });
    client::memory::set_present_callback([&](const client::screen_snapshot& screen) {
        ++incoming_presents;
        presented = screen;
    });
    const auto before = client::memory::snapshot();
    const auto screen = capture_character_window(catacurses::stdscr);
    const auto new_screen = capture_character_window(catacurses::newscr);
    if (initialized) {
        REQUIRE(before.cells.size() == 19 * 9);
        REQUIRE(before.text.find("actor α") != std::string::npos);
        REQUIRE(before.text.find("pane β") != std::string::npos);
        REQUIRE(before.cursor == point(9, 3));
        REQUIRE(std::ranges::any_of(before.cells, [](const auto& cell) {
            return cell.background != 0 && cell.text != " ";
        }));
    } else {
        REQUIRE(before.cells.empty());
        REQUIRE(before.text.empty());
    }
    const auto check_screen = [&](const client::screen_snapshot& actual) {
        CHECK(actual.width == before.width);
        CHECK(actual.height == before.height);
        CHECK(actual.cursor == before.cursor);
        CHECK(actual.cursor_visible == before.cursor_visible);
        CHECK(actual.text == before.text);
        CHECK(std::ranges::equal(actual.cells, before.cells, [](const auto& a, const auto& b) {
            return a.text == b.text && a.foreground == b.foreground && a.background == b.background;
        }));
    };
    auto reads = 0;
    const auto show = [&]() {
        client::memory::set_input_provider([&](const int /*timeout*/) -> input_event {
            const auto snapshot = complete_snapshot();
            REQUIRE((snapshot.context == "PLAYER_INFO" || snapshot.field.has_value()));
            switch (reads++) {
                case 0:
                    return choose(snapshot, "ENCUMBRANCE AND WARMTH");
                case 1: {
                    const auto body = std::ranges::
                        find(snapshot.panes, "encumbrance", &client::interaction_pane::id);
                    REQUIRE(body != snapshot.panes.end());
                    REQUIRE(body->role == "focused");
                    if (exit == character_fixture_exit::normal) { return cancel(snapshot); }
                    return choose(snapshot, "Change name");
                }
                default:
                    REQUIRE(snapshot.field);
                    client::memory::set_cursor(0);
                    client::memory::set_timeout(98);
                    throw std::runtime_error("character fixture native nested input failure");
            }
        });
        character_display::disp_info(get_avatar());
        client::memory::set_cursor(0);
        client::memory::set_timeout(98);
    };
    auto threw = false;
    try {
        if (exit == character_fixture_exit::view_constructor_exception) {
            const auto owner = constructing_character_view{show};
        } else {
            const auto fixture = character_info_guard{};
            show();
        }
    } catch (const std::runtime_error& error) {
        CHECK(std::string{error.what()} == "character fixture native nested input failure");
        threw = true;
    }
    CHECK(threw == (exit != character_fixture_exit::normal));
    CHECK(reads == (exit == character_fixture_exit::normal ? 2 : 3));
    CHECK_FALSE(client::current_interaction().structured);
    CHECK(client::active_input_context().context == nullptr);
    // The incoming presentation callback must neither observe fixture frames nor disappear.
    CHECK(incoming_inputs == 0);
    CHECK(incoming_presents == 0);
    check_screen(client::memory::snapshot());
    check_character_window(catacurses::stdscr, screen);
    check_character_window(catacurses::newscr, new_screen);
    const auto restored = client::memory::read_input();
    CHECK(restored.type == input_event_t::keyboard);
    CHECK(restored.get_first_input() == 'R');
    CHECK(incoming_inputs == 1);
    CHECK(timeout_seen.has_value());
    if (timeout_seen) { CHECK(*timeout_seen == 43); }
    CHECK(incoming_presents == 1);
    REQUIRE(presented.has_value());
    check_screen(*presented);
    // Probe the timeout independently even when the original input callback was lost.
    auto restored_timeout = std::optional<int>{};
    client::memory::set_input_provider([&](const int timeout) {
        restored_timeout = timeout;
        return input_event{'T', input_event_t::keyboard};
    });
    CHECK(client::memory::read_input().get_first_input() == 'T');
    REQUIRE(restored_timeout.has_value());
    CHECK(*restored_timeout == 43);
}

#endif
