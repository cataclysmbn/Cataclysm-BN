#if defined(CATA_MCP)

#    include "avatar.h"
#    include "calendar.h"
#    include "cata_utility.h"
#    include "catch/catch.hpp"
#    include "client_command.h"
#    include "client_input.h"
#    include "client_interaction.h"
#    include "client_memory.h"
#    include "client_memory_scope.h"
#    include "cursesdef.h"
#    include "faction.h"
#    include "game.h"
#    include "input.h"
#    include "item.h"
#    include "iteminfo_request.h"
#    include "json.h"
#    include "map/map.h"
#    include "map/submap.h"
#    include "map_helpers.h"
#    include "npc.h"
#    include "npctrade.h"
#    include "output.h"
#    include "player_helpers.h"
#    include "recipe.h"
#    include "recipe_dictionary.h"
#    include "rng.h"
#    include "skill.h"
#    include "state_helpers.h"
#    include "trade_win.h"
#    include "ui.h"
#    include "uistate.h"

#    include <algorithm>
#    include <ranges>
#    include <sstream>
#    include <string>
#    include <vector>

namespace {

constexpr auto trade_pos = tripoint_bub_ms{60, 60, 0};

struct availability_entry {
    available_status* field;
    available_status value;
};

auto availability_fields() -> std::vector<availability_entry> {
    auto result = std::vector<availability_entry>{};
    const auto append = [&](const requirement_data& requirements) {
        const auto groups = [&](const auto& values) {
            for (const auto& value : values | std::views::join) {
                result.push_back({.field = &value.available, .value = value.available});
            }
        };
        groups(requirements.get_components());
        groups(requirements.get_tools());
        groups(requirements.get_qualities());
    };
    for (const auto& entry : recipe_dict) {
        append(entry.second.simple_requirements());
        std::ranges::for_each(entry.second.deduped_requirements().alternatives(), append);
    }
    return result;
}

struct trade_observation_fixture {
    game_client::memory::scoped_state memory;
    restore_on_out_of_scope<bool> mode{test_mode};
    restore_on_out_of_scope<int> width{TERMX};
    restore_on_out_of_scope<int> height{TERMY};
    restore_on_out_of_scope<int> full_width{FULL_SCREEN_WIDTH};
    restore_on_out_of_scope<int> full_height{FULL_SCREEN_HEIGHT};
    restore_on_out_of_scope<time_point> turn{calendar::turn};
    restore_on_out_of_scope<cata_default_random_engine> rng{rng_get_engine()};
    restore_on_out_of_scope<uistatedata> ui{uistate};
    avatar original_avatar = std::move(get_avatar());
    std::vector<availability_entry> availability = availability_fields();

    trade_observation_fixture() {
        test_mode = true;
        get_avatar() = avatar{};
        clear_all_state();
        clear_character(get_avatar(), false);
        get_avatar().setpos(map_local_to_abs(get_map(), trade_pos));
        calendar::turn = calendar::turn_zero + 12_hours;
        TERMX = 100;
        TERMY = 40;
        FULL_SCREEN_WIDTH = 80;
        FULL_SCREEN_HEIGHT = 24;
        game_client::memory::resize(TERMX, TERMY);
        catacurses::stdscr = catacurses::newwin(TERMY, TERMX, point_zero);
        catacurses::newscr = catacurses::newwin(TERMY, TERMX, point_zero);
        test_mode = false;
    }

    ~trade_observation_fixture() {
        game_client::memory::set_input_provider({});
        test_mode = true;
        clear_all_state();
        get_avatar() = std::move(original_avatar);
        for (const auto& entry : availability) { *entry.field = entry.value; }
    }
};

auto saved(const auto& value) -> std::string {
    auto stream = std::ostringstream{};
    auto json = JsonOut{stream};
    value.serialize(json);
    return stream.str();
}

auto nearby_authority() -> std::string {
    auto stream = std::ostringstream{};
    auto json = JsonOut{stream};
    json.start_object();
    json.member("factions", *g->faction_manager_ptr);
    json.member("runtime_factions");
    json.start_array();
    for (const auto& [id, value] : g->faction_manager_ptr->all()) {
        json.start_object();
        json.member("id", id);
        json.member("validated", value.validated);
        json.member("description", value.desc);
        json.member("currency", value.currency);
        json.member("lone_wolf_faction", value.lone_wolf_faction);
        json.member("monster_faction", value.mon_faction);
        json.member("members");
        json.start_array();
        for (const auto& [member_id, member] : value.members) {
            json.start_object();
            json.member("id", member_id);
            json.member("name", member.first);
            json.member("known", member.second);
            json.end_object();
        }
        json.end_array();
        json.member("epilogues");
        json.start_array();
        for (const auto& [minimum, maximum, snippet] : value.epilogue_data) {
            json.start_object();
            json.member("minimum", minimum);
            json.member("maximum", maximum);
            json.member("snippet", snippet);
            json.end_object();
        }
        json.end_array();
        json.end_object();
    }
    json.end_array();
    json.member("submaps");
    json.start_array();
    for (const auto x : {-1, 0, 1}) {
        for (const auto y : {-1, 0, 1}) {
            json.start_object();
            get_map().get_submap_at(trade_pos + tripoint{x * SEEX, y * SEEY, 0})->store(json);
            json.end_object();
        }
    }
    json.end_array();
    json.end_object();
    return stream.str();
}

auto action(const std::string& name) -> input_event {
    auto command = game_client::input_command{};
    command.action = name;
    const auto resolved =
        game_client::resolve_input_command(command, game_client::memory::screen_size());
    REQUIRE(resolved.has_value());
    return *resolved;
}

} // namespace

TEST_CASE(
    "trade semantic descriptions are passive before native inspection",
    "[client][interaction][npc_trade][trade_observation][observation][rng][mcp]") {
    const auto fixture = trade_observation_fixture{};
    auto& manager = *g->faction_manager_ptr;
    const auto restore_factions = restore_on_out_of_scope<faction_manager>{std::move(manager)};
    manager.create_if_needed();
    const auto cleanup_npcs = on_out_of_scope([]() {
        game_client::memory::set_input_provider({});
        test_mode = true;
        clear_all_state();
    });
    auto& trader = spawn_npc(trade_pos + tripoint_south, "test_talker");
    clear_character(trader, false);
    trader.set_fac(faction_id("tacoma_commune"));
    trader.mission = NPC_MISSION_SHOPKEEP;
    auto& you = get_avatar();
    auto type = itype_id("test_info_jersey");
    auto expected_text = std::string{"An unseen observation jersey inscription."};
    auto cold_resources = false;

    SECTION("unseen snippet remains unread") {}
    SECTION("book does not insert a missing skill") {
        type = itype_id("test_info_book");
        expected_text = "unread chapters";
    }
    SECTION("transform preview does not consume RNG") {
        type = itype_id("test_info_transform");
        expected_text = "TEST observation transform target";
    }
    SECTION("fresh recipes and nearby stale ownership remain unchanged") {
        cold_resources = true;
        expected_text = "TEST observation water assembly";
    }
    SECTION("recipe exemplar retains full nutrient information") {
        type = itype_id("test_info_food");
        expected_text = "Calories";
        cold_resources = true;
    }

    auto candidate = item::spawn(type, calendar::turn);
    candidate->set_owner(trader);
    if (type == itype_id("test_info_food")) {
        candidate->set_var("recipe_exemplar", "test_info_food");
    }
    auto& traded_item = trader.i_add(std::move(candidate));
    auto state = npc_trading::trade_state{};
    npc_trading::setup_trade_state(state, 0, trader);
    REQUIRE(state.theirs.size() == 1);
    REQUIRE(state.theirs.front().locs.front() == &traded_item);

    // Native setup owns eligibility/pricing effects. Install cold observation defects only after
    // it, before the real widget's first semantic query; no description or crafting query primes
    // them.
    if (cold_resources) {
        you.set_skill_level(skill_id("mechanics"), 2);
        auto component = item::spawn("test_info_jersey", calendar::turn);
        component->set_owner(faction_id("test_info_missing_owner"));
        component->set_old_owner(faction_id("test_info_missing_previous_owner"));
        get_map().add_item(trade_pos + tripoint_north, std::move(component));
        you.i_add(item::spawn("test_info_food_component", calendar::turn, 5));
        get_map().ter_set(trade_pos + tripoint_east, ter_id("t_sewage"));
        get_map().ter_set(trade_pos + tripoint_west, ter_id("t_water_sh"));
    }
    you.invalidate_crafting_inventory();
    REQUIRE_FALSE(you.has_seen_snippet(snippet_id("test_info_unseen")));
    if (type == itype_id("test_info_book")) {
        REQUIRE_FALSE(you.get_all_skills().contains(skill_id("mechanics")));
    }
    auto reads = 0;
    auto description = std::string{};
    auto passive_rng = rng_get_engine();
    auto passive_avatar = std::string{};
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        ++reads;
        if (reads == 1) {
            const auto avatar_before = saved(you);
            const auto npc_before = saved(trader);
            const auto item_before = saved(traded_item);
            const auto nearby_before = nearby_authority();
            const auto rng_before = rng_get_engine();
            const auto availability_before = availability_fields();
            const auto check_authority = [&] {
                CHECK(saved(you) == avatar_before);
                CHECK(saved(trader) == npc_before);
                CHECK(saved(traded_item) == item_before);
                CHECK(nearby_authority() == nearby_before);
                CHECK(rng_get_engine() == rng_before);
                CHECK(std::ranges::all_of(availability_before, [](const auto& entry) {
                    return *entry.field == entry.value;
                }));
            };
            const auto first = game_client::current_interaction({.limit = 200});
            REQUIRE(first.context == "NPC_TRADE");
            check_authority();
            const auto row = std::ranges::find_if(first.choices, [&](const auto& choice) {
                return std::ranges::any_of(choice.columns, [&](const auto& column) {
                    return column.label == "Type" && column.value == type.str();
                });
            });
            REQUIRE(row != first.choices.end());
            description = row->description;
            CHECK(description.find(expected_text) != std::string::npos);
            CHECK(description
                  == remove_color_tags(
                      traded_item.info_string({.mode = iteminfo_mode::observation})));
            check_authority();
            for (const auto pass : std::views::iota(0, 3)) {
                INFO("repeated semantic observation " << pass);
                CHECK(game_client::serialize_interaction(
                          game_client::current_interaction({.limit = 200}))
                      == game_client::serialize_interaction(first));
                const auto page = game_client::current_interaction({.offset = 0, .limit = 1});
                REQUIRE(page.choices.size() == 1);
                CHECK(page.choices.front().description == description);
                check_authority();
            }
            passive_rng = rng_get_engine();
            passive_avatar = saved(you);
            return action("EXAMINE");
        }
        if (reads == 2) {
            // This is show_item_data's native popup, not a direct test-only inspection call.
            if (type == itype_id("test_info_jersey")) {
                CHECK(you.has_seen_snippet(snippet_id("test_info_unseen")));
                CHECK(saved(you) != passive_avatar);
            }
            if (type == itype_id("test_info_book")) {
                CHECK(you.get_all_skills().contains(skill_id("mechanics")));
            }
            if (type == itype_id("test_info_transform") || cold_resources) {
                CHECK(rng_get_engine() != passive_rng);
            }
            return action("QUIT");
        }
        REQUIRE(reads == 3);
        return action("QUIT");
    });
    CHECK_FALSE(trading_window(state).perform_trade(trader, "Trade"));
    CHECK(reads == 3);
    CHECK(remove_color_tags(traded_item.info_string()) == description);
    CHECK_FALSE(state.theirs.front().selected);
    CHECK(trader.amount_of(type) == 1);
}

TEST_CASE(
    "trade choice identities do not repair stale ownership of stack members",
    "[client][interaction][npc_trade][trade_observation][observation][mcp]") {
    const auto fixture = trade_observation_fixture{};
    auto& manager = *g->faction_manager_ptr;
    const auto restore_factions = restore_on_out_of_scope<faction_manager>{std::move(manager)};
    manager.create_if_needed();
    const auto cleanup_npcs = on_out_of_scope([]() {
        game_client::memory::set_input_provider({});
        test_mode = true;
        clear_all_state();
    });
    auto& trader = spawn_npc(trade_pos + tripoint_south, "test_talker");
    clear_character(trader, false);
    trader.set_fac(faction_id("tacoma_commune"));
    trader.mission = NPC_MISSION_SHOPKEEP;
    auto first = item::spawn("test_info_jersey", calendar::turn);
    auto second = item::spawn("test_info_jersey", calendar::turn);
    first->set_owner(trader);
    second->set_owner(trader);
    auto& stale_member = trader.i_add(std::move(second));
    trader.i_add(std::move(first));
    auto state = npc_trading::trade_state{};
    npc_trading::setup_trade_state(state, 0, trader);
    REQUIRE(state.theirs.size() == 1);
    REQUIRE(state.theirs.front().locs.size() == 2);
    // Native setup validates only the stack's first member; install the defect after it.
    stale_member.set_old_owner(faction_id("test_info_missing_previous_owner"));
    const auto stale_before = saved(stale_member);
    REQUIRE(stale_before.find("test_info_missing_previous_owner") != std::string::npos);
    auto reads = 0;
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        ++reads;
        const auto authority_before = nearby_authority();
        const auto rng_before = rng_get_engine();
        const auto first_snapshot = game_client::current_interaction({.limit = 200});
        REQUIRE(first_snapshot.context == "NPC_TRADE");
        CHECK(saved(stale_member) == stale_before);
        CHECK(nearby_authority() == authority_before);
        CHECK(rng_get_engine() == rng_before);
        CHECK(game_client::serialize_interaction(game_client::current_interaction({.limit = 200}))
              == game_client::serialize_interaction(first_snapshot));
        return action("QUIT");
    });
    CHECK_FALSE(trading_window(state).perform_trade(trader, "Trade"));
    CHECK(reads == 1);
}

#endif
