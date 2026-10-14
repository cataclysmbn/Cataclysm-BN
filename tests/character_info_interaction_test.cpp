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
#    include "client_memory.h"
#    include "client_memory_scope.h"
#    include "cursesdef.h"
#    include "cursesport.h"
#    include "effect.h"
#    include "engine_client_component.h"
#    include "engine_client_contract.h"
#    include "game.h"
#    include "init.h"
#    include "input.h"
#    include "json.h"
#    include "kill_tracker.h"
#    include "locale_fixture.h"
#    include "locale_scope.h"
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
#    include <nlohmann/json.hpp>
#    include <ranges>
#    include <sstream>
#    include <stdexcept>
#    include <vector>

namespace {
namespace client = game_client;
namespace ec = engine_client;
using json = nlohmann::json;

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

/// Cancels a nested native popup, which only the 0.1 snapshot describes.
auto cancel(const client::interaction_snapshot& snapshot) -> input_event {
    return resolve(
        {.input_id = snapshot.input_id, .operation = client::interaction_operation::cancel});
}

/// The top screen as a client reads it.
auto captured() -> ec::boundary_state {
    const auto state = ec::capture_boundary(
        {.epoch = "epoch:character",
         .ready =
             {.phase = "waiting_for_input",
              .game_ready = true,
              .accepts_interaction_commands = true,
              .accepts_registered_actions = true}});
    REQUIRE(state.has_value());
    return *state;
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
template <typename Body> auto body(const ec::component& value) -> const Body& {
    REQUIRE(std::holds_alternative<Body>(value.body));
    return std::get<Body>(value.body);
}
auto rows(const ec::screen_view& screen, const std::string& tab) -> const std::vector<ec::row>& {
    return body<ec::list_body>(part(screen, "character:" + tab + ":rows")).rows;
}
auto row_of(const ec::screen_view& screen, const std::string& tab, const std::string& id)
    -> const ec::row& {
    const auto& all = rows(screen, tab);
    const auto found = std::ranges::find(all, id, &ec::row::id);
    INFO(id);
    REQUIRE(found != all.end());
    return *found;
}
auto stats_of(const ec::screen_view& screen, const std::string& panel)
    -> const std::vector<ec::panel_stat>& {
    return body<ec::panel_body>(part(screen, panel)).stats;
}
auto affordance(const ec::screen_view& screen, const std::string& id) -> const ec::affordance& {
    const auto found = std::ranges::find(screen.affordances, id, &ec::affordance::id);
    INFO(id);
    REQUIRE(found != screen.affordances.end());
    return *found;
}
auto skill_level(const std::string& skill) -> bool {
    return get_avatar().get_skill_level_object(skill_id(skill)).isTraining();
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
    "the character sheet publishes its tabs and the rows of every tab by identity",
    "[character_info_interaction][engine_client_component]") {
    const auto guard = character_info_guard{};
    auto& you = get_avatar();
    you.set_mutation(trait_id("test_character_info_visible_trait"));
    you.set_mutation(trait_id("test_character_info_hidden_trait"));
    you.add_effect(efftype_id("test_character_info_visible_effect"), 10_turns);
    you.add_effect(efftype_id("test_character_info_hidden_effect"), 10_turns);
    you.add_bionic(bionic_id("test_character_info_cbm"));
    you.add_bionic(bionic_id("test_character_info_cbm"));
    const auto training = skill_level("melee");
    const auto before = actor_state();
    const auto rng = rng_get_engine();
    auto reads = 0;
    client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto state = captured();
        const auto screen = ec::top_screen(state);
        REQUIRE(screen.root);
        CHECK(screen.context == "PLAYER_INFO");
        CHECK(actor_state() == before);
        CHECK(rng_get_engine() == rng);
        switch (reads++) {
            case 0: {
                const auto& tabs = body<ec::tabs_body>(part(screen, "character:tabs"));
                REQUIRE(tabs.tabs.size() == 6);
                CHECK(tabs.tabs.front().id == "stats");
                CHECK(tabs.tabs.front().selected);
                CHECK(tabs.select == "character:tab");
                CHECK(body<ec::split_body>(part(screen, "character:panes")).active
                      == "character:stats");
                CHECK(screen.focus_component == "character:stats:rows");
                CHECK(screen.focus_row == "stat:str");
                CHECK(row_of(screen, "stats", "stat:str").description.find("Base HP:")
                      != std::string::npos);
                CHECK(row_of(screen, "stats", "stat:dex").description.find("Throwing penalty")
                      != std::string::npos);
                CHECK(row_of(screen, "stats", "stat:int").description.find("Crafting bonus")
                      != std::string::npos);
                CHECK(row_of(screen, "stats", "stat:per").description.find("Trap detection")
                      != std::string::npos);
                CHECK(affordance(screen, "QUIT").role == "cancel");
                // A tab is picked by its identity, and an unknown one is refused.
                CHECK(
                    ec::resolve_invoke(
                        state, {.affordance_id = "character:tab", .args = {.row = "nowhere"}})
                        .error()
                    == ec::error::invalid_arguments);
                return invoke("character:tab", {.row = "encumbrance"});
            }
            case 1: {
                CHECK(body<ec::split_body>(part(screen, "character:panes")).active
                      == "character:encumbrance");
                const auto& all = rows(screen, "encumbrance");
                REQUIRE_FALSE(all.empty());
                CHECK(all.front().id.starts_with("body:"));
                CHECK_FALSE(all.front().columns.empty());
                CHECK_FALSE(all.front().description.empty());
                return invoke("character:tab", {.row = "traits"});
            }
            case 2:
                CHECK(row_of(screen, "traits", "trait:test_character_info_visible_trait")
                          .description.find("last sentence")
                      != std::string::npos);
                CHECK(std::ranges::none_of(rows(screen, "traits"), [](const auto& row) {
                    return row.id == "trait:test_character_info_hidden_trait";
                }));
                CHECK(
                    ec::resolve_invoke(
                        state, {.affordance_id = "character:focus:traits",
                                .args = {.row = "trait:test_character_info_hidden_trait"}})
                        .error()
                    == ec::error::invalid_arguments);
                return invoke("character:tab", {.row = "bionics"});
            case 3: {
                const auto& cbm = row_of(screen, "bionics", "bionic:test_character_info_cbm");
                CHECK(cbm.label == "TEST character info CBM (2)");
                CHECK(cbm.description.find("2 instances") != std::string::npos);
                const auto& power = stats_of(screen, "character:bionics");
                REQUIRE(power.size() == 1);
                CHECK(power.front().key == "power");
                CHECK(power.front().value
                      == std::to_string(units::to_kilojoule(you.get_power_level())));
                CHECK(power.front().max
                      == std::to_string(units::to_kilojoule(you.get_max_power_level())));
                return invoke("character:tab", {.row = "effects"});
            }
            case 4: {
                const auto& visible = rows(screen, "effects").front();
                CHECK(visible.label == "TEST character info visible effect");
                CHECK(
                    visible.description
                    == you.get_effect(efftype_id("test_character_info_visible_effect")).disp_desc());
                CHECK(std::ranges::none_of(rows(screen, "effects"), [](const auto& row) {
                    return row.description.find("Undisclosed") != std::string::npos;
                }));
                return invoke("character:tab", {.row = "skills"});
            }
            case 5: {
                const auto& all = rows(screen, "skills");
                CHECK(all.front().header);
                CHECK_FALSE(all.front().selectable);
                CHECK(screen.focus_row == all[1].id);
                return invoke("character:focus:skills", {.row = "skill:melee"});
            }
            case 6:
                CHECK(screen.focus_row == "skill:melee");
                CHECK(skill_level("melee") == training);
                return action("QUIT");
            default:
                FAIL("the character sheet outlived its cancel");
                return action("QUIT");
        }
    });
    character_display::disp_info(you);
    CHECK(reads == 7);
    CHECK(actor_state() == before);
    CHECK(rng_get_engine() == rng);
}

TEST_CASE(
    "the character sheet publishes native colors and the speed breakdown as figures",
    "[character_info_interaction][character_info_colors][engine_client_component]") {
    const auto guard = character_info_guard{};
    auto& you = get_avatar();
    you.set_mutation(trait_id("test_character_info_visible_trait"));
    auto reads = 0;
    client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto screen = ec::top_screen(captured());
        const auto column = [](const ec::row& row, const std::string& key) -> const auto& {
            const auto found =
                std::ranges::find(row.columns, key, &client::interaction_column::key);
            REQUIRE(found != row.columns.end());
            return *found;
        };
        if (reads++ == 0) {
            const auto& strength = row_of(screen, "stats", "stat:str");
            CHECK(column(strength, "current").color == string_from_color(c_white));
            const auto& speed = stats_of(screen, "character:speed");
            const auto stat = [&](const std::string& key) -> const auto& {
                const auto found = std::ranges::find(speed, key, &ec::panel_stat::key);
                REQUIRE(found != speed.end());
                return *found;
            };
            CHECK(stat("current_speed").value == std::to_string(you.get_speed()));
            CHECK(stat("current_speed").tone == (you.get_speed() >= 100 ? "good" : "bad"));
            CHECK(stat("base_move_cost").value == std::to_string(you.run_cost(100)));
            return invoke("character:tab", {.row = "traits"});
        }
        const auto& trait = row_of(screen, "traits", "trait:test_character_info_visible_trait");
        CHECK(trait.color
              == string_from_color(
                  trait_id("test_character_info_visible_trait")->get_display_color()));
        return action("QUIT");
    });
    character_display::disp_info(you);
    CHECK(reads == 2);
}

TEST_CASE(
    "the character sheet publishes every pane at once, whatever the focus",
    "[character_info_interaction][engine_client_component]") {
    const auto guard = character_info_guard{};
    auto& you = get_avatar();
    you.set_mutation(trait_id("test_character_info_visible_trait"));
    you.add_effect(efftype_id("test_character_info_visible_effect"), 10_turns);
    you.add_bionic(bionic_id("test_character_info_cbm"));
    client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto screen = ec::top_screen(captured());
        for (const auto* pane :
             {"stats", "encumbrance", "skills", "traits", "bionics", "effects"}) {
            CAPTURE(pane);
            CHECK_FALSE(rows(screen, pane).empty());
        }
        CHECK_FALSE(stats_of(screen, "character:speed").empty());
        return action("QUIT");
    });
    character_display::disp_info(you);
}

TEST_CASE(
    "the character sheet pages the rows past the first page and keeps the focus on its row",
    "[character_info_interaction][engine_client_component]") {
    const auto guard = character_info_guard{};
    auto& you = get_avatar();
    for (const auto i : std::views::iota(0, 205)) {
        you.set_mutation(trait_id("test_character_info_page_" + std::to_string(i)));
    }
    auto step = 0;
    client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto state = captured();
        const auto screen = ec::top_screen(state);
        const auto total = rows(screen, "traits").size();
        REQUIRE(total >= 205);
        const auto page = ec::read_rows(
            {.epoch = "epoch:character",
             .boundary_id = state.id,
             .component_id = "character:traits:rows",
             .offset = 200,
             .limit = 100},
            state);
        REQUIRE(page);
        CHECK(page->total == total);
        CHECK(page->rows.size() == total - 200);
        switch (step++) {
            case 0:
                return invoke("character:tab", {.row = "traits"});
            case 1:
                return invoke(
                    "character:focus:traits", {.row = "trait:test_character_info_page_204"});
            default:
                CHECK(screen.focus_row == "trait:test_character_info_page_204");
                return action("QUIT");
        }
    });
    character_display::disp_info(you);
    CHECK(step == 3);
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
    "the character sheet moves its focus by row and runs native training on confirm",
    "[character_info_interaction][engine_client_component]") {
    const auto guard = character_info_guard{};
    auto& you = get_avatar();
    const auto initial = skill_level("melee");
    auto step = 0;
    client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto state = captured();
        const auto screen = ec::top_screen(state);
        switch (step++) {
            case 0:
                CHECK_FALSE(
                    ec::resolve_invoke(
                        state,
                        {.affordance_id = "character:focus:skills", .args = {.row = "foreign"}})
                        .has_value());
                return invoke("character:tab", {.row = "skills"});
            case 1:
                return invoke("character:focus:skills", {.row = "skill:melee"});
            case 2:
                // Picking a row moves the focus only; training stays as it was.
                CHECK(screen.focus_row == "skill:melee");
                CHECK(skill_level("melee") == initial);
                return action("CONFIRM");
            case 3:
                CHECK(skill_level("melee") != initial);
                return action("CONFIRM");
            case 4:
                CHECK(skill_level("melee") == initial);
                return action("QUIT");
            default:
                FAIL("unexpected native read");
                return action("QUIT");
        }
    });
    character_display::disp_info(you);
    CHECK(step == 5);
}

TEST_CASE(
    "the character sheet keeps the native denial and the nested confirmation of a stat upgrade",
    "[character_info_interaction][engine_client_component]") {
    const auto guard = character_info_guard{};
    const auto enabled = override_option{"STATS_THROUGH_KILLS", "true"};
    const auto coefficient = override_option{"AVATAR_LEVEL_XP_COEFF", "1"};
    auto& you = get_avatar();
    auto grant = false;
    auto raise = false;
    SECTION("denied XP is a disabled confirm that says why and still reaches the native warning") {}
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
            if (grant) { return action(raise ? "YES" : "NO"); }
            CHECK(snapshot.message.find("experience") != std::string::npos);
            return cancel(snapshot);
        }
        const auto screen = ec::top_screen(captured());
        if (step++ == 0) {
            const auto found = std::ranges::find(screen.denials, "CONFIRM", &ec::denial::id);
            CHECK((found == screen.denials.end()) == grant);
            if (found != screen.denials.end()) {
                CHECK(found->reason.find("experience") != std::string::npos);
            }
            return action("CONFIRM");
        }
        CHECK(you.get_str_base() == initial + (raise ? 1 : 0));
        CHECK(you.free_upgrade_points() == points - (raise ? 1 : 0));
        return action("QUIT");
    });
    character_display::disp_info(you);
    CHECK(nested);
    CHECK(step == 2);
}

TEST_CASE(
    "the character sheet preserves the CQB disclosure and the native training toggle",
    "[character_info_interaction][engine_client_component]") {
    const auto guard = character_info_guard{};
    auto& you = get_avatar();
    you.add_bionic(bionic_id("bio_cqb"));
    you.get_bionic_state(bionic_id("bio_cqb")).powered = true;
    const auto initial = skill_level("melee");
    auto step = 0;
    client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto screen = ec::top_screen(captured());
        switch (step++) {
            case 0:
                return invoke("character:tab", {.row = "skills"});
            case 1:
                return invoke("character:focus:skills", {.row = "skill:melee"});
            case 2:
            case 3: {
                const auto& row = row_of(screen, "skills", "skill:melee");
                CHECK(std::ranges::none_of(row.columns, [](const auto& column) {
                    return column.label == "Training";
                }));
                CHECK(row.columns[0].value == std::to_string(BIO_CQB_LEVEL));
                CHECK(row.columns[1].value == "0%");
                CHECK(row.columns[0].color == string_from_color(c_yellow));
                if (step == 3) { return action("CONFIRM"); }
                CHECK(skill_level("melee") != initial);
                return action("QUIT");
            }
            default:
                FAIL("unexpected native read");
                return action("QUIT");
        }
    });
    character_display::disp_info(you);
    CHECK(step == 4);
}

TEST_CASE(
    "the character sheet offers no stat denial when native restrictions exclude the upgrade",
    "[character_info_interaction][engine_client_component]") {
    const auto guard = character_info_guard{};
    const auto disabled = override_option{"STATS_THROUGH_KILLS", "false"};
    auto& you = get_avatar();
    const auto before = you.get_str_base();
    auto step = 0;
    client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto screen = ec::top_screen(captured());
        CHECK(screen.denials.empty());
        if (step++ == 0) { return action("CONFIRM"); }
        CHECK(you.get_str_base() == before);
        return action("QUIT");
    });
    character_display::disp_info(you);
    CHECK(step == 2);
}

TEST_CASE(
    "the character sheet of a real NPC offers no stat upgrade",
    "[character_info_interaction][engine_client_component]") {
    const auto guard = character_info_guard{};
    const auto enabled = override_option{"STATS_THROUGH_KILLS", "true"};
    auto non_avatar = npc{};
    const auto initial = non_avatar.get_str_base();
    auto step = 0;
    client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto screen = ec::top_screen(captured());
        REQUIRE(screen.context == "PLAYER_INFO");
        CHECK(screen.denials.empty());
        if (step++ == 0) { return action("CONFIRM"); }
        CHECK(non_avatar.get_str_base() == initial);
        return action("QUIT");
    });
    character_display::disp_info(non_avatar);
    CHECK(step == 2);
}

TEST_CASE(
    "the character sheet names are changed by the native fields and keep their drafts",
    "[character_info_interaction]") {
    const auto guard = character_info_guard{};
    auto& you = get_avatar();
    you.name = "Before";
    auto step = 0;
    client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto snapshot = client::current_interaction();
        switch (step++) {
            case 0:
                return action("CHANGE_NAME");
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
                CHECK(ec::top_screen(captured()).title.find("After") != std::string::npos);
                CHECK(you.name == "After");
                return action("CHANGE_PROFESSION_NAME");
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
                CHECK(ec::top_screen(captured()).title.find("Survivor") != std::string::npos);
                return action("CHANGE_NAME");
            case 6:
                REQUIRE(snapshot.field);
                return cancel(snapshot);
            case 7:
                CHECK(snapshot.context == "PLAYER_INFO");
                // Native caller assigns the canceled popup text (empty), not the old name.
                CHECK(you.name.empty());
                return action("QUIT");
            default:
                FAIL("unexpected native read");
                return input_event{};
        }
    });
    character_display::disp_info(you);
    CHECK(step == 8);
}

TEST_CASE(
    "the character sheet native skill hooks run in the loop and not on a read",
    "[character_info_interaction][engine_client_component]") {
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
    const auto training = skill_level("melee");
    auto step = 0;
    client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto calls = info_calls;
        const auto screen = ec::top_screen(captured());
        // Publishing the screen again does not run the hook again.
        static_cast<void>(captured());
        CHECK(info_calls == calls);
        switch (step++) {
            case 0:
                return invoke("character:tab", {.row = "skills"});
            case 1:
                return invoke("character:focus:skills", {.row = "skill:melee"});
            case 2:
                CHECK(info_calls >= 1);
                CHECK(row_of(screen, "skills", "skill:melee").description.find("final sentence")
                      != std::string::npos);
                CHECK(action_calls == 0);
                return action("CONFIRM");
            case 3:
                CHECK(action_calls == 1);
                CHECK(skill_level("melee") == training);
                return action("QUIT");
            default:
                FAIL("unexpected native read");
                return action("QUIT");
        }
    });
    character_display::disp_info(you);
    CHECK(step == 4);
}

TEST_CASE(
    "the character sheet shows the figures as of the last native paint after a focused Lua mutation",
    "[character_info_interaction][engine_client_component]") {
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
        params["results"]["text"] = "Native focused callback changed strength.";
    };
    hooks["on_character_display_skill_info"] = hook;
    auto step = 0;
    client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto screen = ec::top_screen(captured());
        const auto base = [&]() -> const std::string& {
            const auto& strength = row_of(screen, "stats", "stat:str");
            const auto found =
                std::ranges::find(strength.columns, "base", &client::interaction_column::key);
            REQUIRE(found != strength.columns.end());
            return found->value;
        };
        switch (step++) {
            case 0:
                CHECK(calls == 0);
                CHECK(base() == "8");
                return invoke("character:tab", {.row = "skills"});
            case 1:
                CHECK(calls >= 1);
                CHECK(you.get_str_base() == 11);
                return invoke("character:tab", {.row = "stats"});
            default:
                CHECK(base() == "11");
                return action("QUIT");
        }
    });
    character_display::disp_info(you);
    CHECK(step == 3);
}

TEST_CASE(
    "the character sheet is the same at any viewport size and has the same ids in another language",
    "[character_info_interaction][engine_client_component]") {
    const auto guard = character_info_guard{};
    const auto fixture_ko = locale_fixture::korean_catalogue{};
    auto& you = get_avatar();
    you.set_mutation(trait_id("test_character_info_visible_trait"));
    you.add_bionic(bionic_id("test_character_info_cbm"));
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
    character_display::disp_info(you);
    CHECK(reads == 1);
}

TEST_CASE(
    "the character sheet publishes no window text and no native line",
    "[character_info_interaction][engine_client_component]") {
    const auto guard = character_info_guard{};
    client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto screen = ec::top_screen(captured());
        for (const auto& stat : stats_of(screen, "character:speed")) {
            CHECK(stat.value.find("<color") == std::string::npos);
            CHECK(stat.label.find("<color") == std::string::npos);
            CHECK_FALSE(stat.key.empty());
        }
        for (const auto* tab : {"stats", "encumbrance", "skills", "traits", "bionics", "effects"}) {
            for (const auto& row : rows(screen, tab)) {
                CHECK(row.label.find("<color") == std::string::npos);
                CHECK_FALSE(row.id.empty());
            }
        }
        CHECK(screen.title.find("  ") == std::string::npos);
        CHECK_FALSE(screen.title.empty());
        return action("QUIT");
    });
    character_display::disp_info(get_avatar());
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
                return invoke("character:tab", {.row = "skills"});
            case 1:
                return invoke("character:focus:skills", {.row = "skill:melee"});
            case 2:
                return action("CONFIRM");
            case 3:
                return action("CHANGE_NAME");
            case 4:
                REQUIRE(snapshot.field);
                return resolve(
                    {.input_id = snapshot.input_id,
                     .operation = client::interaction_operation::fill,
                     .target_id = snapshot.field->id,
                     .value = "Recorded actor",
                     .submit = true});
            case 5:
                return action("QUIT");
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
        if (step++ == 0) { return action("CHANGE_NAME"); }
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
            const auto snapshot = client::current_interaction();
            REQUIRE((snapshot.context == "PLAYER_INFO" || snapshot.field.has_value()));
            switch (reads++) {
                case 0:
                    return invoke("character:tab", {.row = "encumbrance"});
                case 1: {
                    const auto screen = ec::top_screen(captured());
                    CHECK(body<ec::split_body>(part(screen, "character:panes")).active
                          == "character:encumbrance");
                    if (exit == character_fixture_exit::normal) { return action("QUIT"); }
                    return action("CHANGE_NAME");
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
