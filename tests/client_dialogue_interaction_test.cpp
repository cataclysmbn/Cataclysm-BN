#if defined(CATA_MCP)

#    include "avatar.h"
#    include "calendar.h"
#    include "cata_utility.h"
#    include "catch/catch.hpp"
#    include "character_id.h"
#    include "client_command.h"
#    include "client_input.h"
#    include "client_interaction.h"
#    include "client_memory.h"
#    include "client_memory_scope.h"
#    include "cursesdef.h"
#    include "dialogue.h"
#    include "faction.h"
#    include "game.h"
#    include "input.h"
#    include "item.h"
#    include "json.h"
#    include "map/map.h"
#    include "npc.h"
#    include "options_helpers.h"
#    include "output.h"
#    include "path_info.h"
#    include "pimpl.h"
#    include "player_helpers.h"
#    include "replay/replay.h"
#    include "rng.h"
#    include "skill.h"
#    include "state_helpers.h"
#    include "uistate.h"

#    include <algorithm>
#    include <array>
#    include <atomic>
#    include <filesystem>
#    include <map>
#    include <ranges>
#    include <set>
#    include <sstream>
#    include <stdexcept>
#    include <string>
#    include <unordered_map>
#    include <vector>

namespace {

const auto start_topic = std::string{"TALK_TEST_CLIENT_DIALOGUE_START"};
const auto first_topic = std::string{"TALK_TEST_CLIENT_DIALOGUE_FIRST"};
const auto second_topic = std::string{"TALK_TEST_CLIENT_DIALOGUE_SECOND"};
const auto branch_var = std::string{"npctalk_var_test_client_dialogue_branches"};
const auto trial_var = std::string{"npctalk_var_test_client_dialogue_trials"};
const auto removed_var = std::string{"npctalk_var_test_client_dialogue_removed"};
const auto speaker_var = std::string{"npctalk_var_test_client_dialogue_speaker"};

/// Own the real world fixture and the memory widget lifecycle, including exceptional exits.
struct dialogue_fixture {
    const game_client::memory::scoped_state memory;
    bool old_test_mode = test_mode;
    int old_termx = TERMX;
    int old_termy = TERMY;
    int old_width = FULL_SCREEN_WIDTH;
    int old_height = FULL_SCREEN_HEIGHT;
    cata_default_random_engine old_rng = rng_get_engine();
    faction_manager old_factions = *g->faction_manager_ptr;
    uistatedata old_uistate = uistate;
    time_point old_time = calendar::turn;
    std::unordered_map<std::string, std::string> old_values = get_avatar().get_values_map();
    SkillLevel old_speech = get_avatar().get_skill_level_object(skill_id("speech"));
    int old_focus = get_avatar().focus_pool;
    override_option capital_yn{"FORCE_CAPITAL_YN", "false"};
    npc* talker = nullptr;
    dialogue d;
    dialogue_window window;

    dialogue_fixture() {
        test_mode = true;
        clear_all_state();
        clear_character(get_avatar(), false);
        auto& you = get_avatar();
        const auto values = you.get_values_map();
        for (const auto& entry : values) { you.remove_value(entry.first); }
        you.setpos(map_local_to_abs(get_map(), tripoint_bub_ms{60, 60, 0}));
        you.cash = 100;
        // Use ordinary hands, not debug storage/traits, for the native consumption effect.
        auto currency = item::spawn("test_platinum_bit", calendar::turn, 7);
        REQUIRE(you.can_wield(*currency).success());
        REQUIRE_FALSE(you.wield(std::move(currency)));
        you.get_skill_level_object(skill_id("speech")) = SkillLevel{};
        you.focus_pool = 100;
        you.set_moves(1000);
        g->faction_manager_ptr->create_if_needed();
        talker = &spawn_npc(you.bub_pos() + tripoint_east, "test_talker");
        talker->name = "Adapter talker";
        talker->op_of_u = npc_opinion{};
        talker->set_attitude(NPCATT_NULL);
        talker->set_moves(100);
        talker->chatbin.first_topic = start_topic;
        d.alpha = &you;
        d.beta = talker;
        d.add_topic(start_topic);
        TERMX = 80;
        TERMY = 24;
        FULL_SCREEN_WIDTH = 80;
        FULL_SCREEN_HEIGHT = 24;
        game_client::memory::resize(TERMX, TERMY);
        catacurses::stdscr = catacurses::newwin(TERMY, TERMX, point_zero);
        catacurses::newscr = catacurses::newwin(TERMY, TERMX, point_zero);
        test_mode = false;
    }

    ~dialogue_fixture() {
        test_mode = true;
        clear_all_state();
        *g->faction_manager_ptr = old_factions;
        uistate = old_uistate;
        calendar::turn = old_time;
        auto& you = get_avatar();
        const auto values = you.get_values_map();
        for (const auto& entry : values) { you.remove_value(entry.first); }
        for (const auto& entry : old_values) { you.set_value(entry.first, entry.second); }
        you.get_skill_level_object(skill_id("speech")) = old_speech;
        you.focus_pool = old_focus;
        rng_get_engine() = old_rng;
        TERMX = old_termx;
        TERMY = old_termy;
        FULL_SCREEN_WIDTH = old_width;
        FULL_SCREEN_HEIGHT = old_height;
        test_mode = old_test_mode;
    }

    /// Restore only fixture inputs actually changed by these responses; do not load/save
    /// through unordered containers or normalize serialized actor state for equality.
    auto reset_effect_state() -> void {
        auto& you = get_avatar();
        you.cash = 100;
        you.primary_weapon().charges = 7;
        you.get_skill_level_object(skill_id("speech")) = SkillLevel{};
        you.focus_pool = 100;
        you.set_moves(1000);
        talker->set_moves(100);
        talker->op_of_u = npc_opinion{};
        talker->remove_value(branch_var);
        talker->remove_value(trial_var);
        talker->remove_value(speaker_var);
    }

    auto opt(const std::string& topic = start_topic) -> talk_topic {
        return d.opt(window, talker->name, talk_topic(topic));
    }
};

auto choose(const game_client::interaction_snapshot& snapshot, const std::string& id)
    -> input_event {
    auto command = game_client::input_command{};
    command.interaction = game_client::interaction_command{
        .input_id = snapshot.input_id,
        .operation = game_client::interaction_operation::choose,
        .target_id = id,
    };
    const auto event =
        game_client::resolve_input_command(command, game_client::memory::screen_size());
    REQUIRE(event);
    return *event;
}

auto choose_label(const game_client::interaction_snapshot& snapshot, const std::string& label)
    -> input_event {
    const auto choice = std::ranges::find_if(snapshot.choices, [&](const auto& entry) {
        return entry.label.find(label) != std::string::npos;
    });
    REQUIRE(choice != snapshot.choices.end());
    return choose(snapshot, choice->id);
}

auto choose_popup(const game_client::interaction_snapshot& snapshot, const std::string& action)
    -> input_event {
    const auto choice =
        std::ranges::find(snapshot.choices, action, &game_client::interaction_choice::description);
    REQUIRE(choice != snapshot.choices.end());
    return choose(snapshot, choice->id);
}

template <typename Actor> auto serialize_actor(const Actor& actor) -> std::string {
    auto stream = std::ostringstream{};
    auto json = JsonOut(stream);
    actor.serialize(json);
    return stream.str();
}

} // namespace

TEST_CASE(
    "dialogue observation uses evaluated live narrative and distinct native responses",
    "[client][interaction][client_dialogue][mcp]") {
    auto fixture = dialogue_fixture{};
    auto reads = 0;
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        REQUIRE(++reads == 1);
        const auto before = rng_get_engine();
        const auto snapshot = game_client::current_interaction();
        CHECK(snapshot.context == "DIALOGUE_CHOOSE_RESPONSE");
        CHECK(game_client::active_input_context().context != nullptr);
        CHECK(game_client::active_input_context().category == snapshot.context);
        CHECK(snapshot.structured);
        CHECK_FALSE(snapshot.actions_only);
        CHECK_FALSE(snapshot.allow_cancel);
        CHECK(snapshot.kind == game_client::interaction_kind::choices);
        CHECK(snapshot.title == "Adapter talker");
        CHECK(snapshot.message.find("Adapter narrative, already evaluated:") != std::string::npos);
        CHECK(snapshot.message.find("<swear>") == std::string::npos);
        CHECK(snapshot.message.find(fixture.talker->name) != std::string::npos);
        REQUIRE(fixture.d.responses.size() == 9);
        REQUIRE(snapshot.choices.size() == fixture.d.responses.size() + 4);
        CHECK(snapshot.choices[0].label == snapshot.choices[1].label);
        CHECK(snapshot.choices[0].id != snapshot.choices[1].id);
        CHECK(snapshot.choices[2].label == snapshot.choices[3].label);
        CHECK(snapshot.choices[2].id != snapshot.choices[3].id);
        CHECK(snapshot.choices[4].label.find("100%") != std::string::npos);
        CHECK(snapshot.choices[4].label.find("cost 17 cents") != std::string::npos);
        auto ids = std::set<std::string>{};
        for (const auto& choice : snapshot.choices) { CHECK(ids.insert(choice.id).second); }
        const auto serialized = game_client::serialize_interaction(snapshot);
        for (const auto query : std::views::iota(0, 8)) {
            INFO("passive query " << query);
            CHECK(game_client::serialize_interaction(game_client::current_interaction())
                  == serialized);
            const auto page = game_client::current_interaction({.offset = 1, .limit = 2});
            REQUIRE(page.choices.size() == 2);
            CHECK(page.choices.front().id == snapshot.choices[1].id);
            CHECK(page.schema_id == snapshot.schema_id);
            CHECK(page.message == snapshot.message);
        }
        CHECK(rng_get_engine() == before);
        CHECK(fixture.talker->get_value(branch_var).empty());
        CHECK(fixture.talker->get_value(trial_var).empty());
        CHECK(get_avatar().cash == 100);
        CHECK(get_avatar().charges_of(itype_id("test_platinum_bit")) == 7);
        CHECK(fixture.talker->get_value(speaker_var) == "1");
        return choose(snapshot, snapshot.choices[1].id);
    });
    CHECK(fixture.opt().id == second_topic);
    CHECK(fixture.talker->get_value(branch_var) == "10");
    CHECK(reads == 1);
    CHECK(game_client::active_input_context().context == nullptr);
    CHECK_FALSE(game_client::current_interaction().structured);
}

TEST_CASE(
    "dialogue identical labels and identical topics still select the exact original effect",
    "[client][interaction][client_dialogue][mcp]") {
    auto fixture = dialogue_fixture{};
    const auto index = GENERATE(0, 1, 2, 3);
    const auto increments = std::array{"1", "10", "100", "1000"};
    auto reads = 0;
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        REQUIRE(++reads == 1);
        const auto snapshot = game_client::current_interaction();
        return choose(snapshot, snapshot.choices[index].id);
    });
    CHECK(fixture.opt().id == (index == 1 ? second_topic : first_topic));
    CHECK(fixture.talker->get_value(branch_var) == increments[index]);
}

TEST_CASE(
    "dialogue semantic choice runs the native trial and effects once including failure",
    "[client][interaction][client_dialogue][mcp]") {
    auto fixture = dialogue_fixture{};
    const auto success = GENERATE(true, false);
    rng_set_engine_seed(74261);
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        return input_event(success ? 'e' : 'f', input_event_t::keyboard);
    });
    CHECK(fixture.opt().id == (success ? second_topic : first_topic));
    const auto expected_rng = rng_get_engine();
    const auto expected_npc = serialize_actor(*fixture.talker);
    const auto expected_avatar = serialize_actor(get_avatar());
    fixture.reset_effect_state();
    rng_set_engine_seed(74261);
    auto reads = 0;
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        REQUIRE(++reads == 1);
        return choose_label(
            game_client::current_interaction(),
            success ? "Successful native trial" : "Denied native trial");
    });
    CHECK(fixture.opt().id == (success ? second_topic : first_topic));
    CHECK(fixture.talker->get_value(trial_var) == (success ? "1" : "-1"));
    CHECK(fixture.talker->op_of_u.value == (success ? 3 : -3));
    CHECK(get_avatar().cash == (success ? 83 : 100));
    CHECK(get_avatar().charges_of(itype_id("test_platinum_bit")) == (success ? 5 : 7));
    CHECK(rng_get_engine() == expected_rng);
    CHECK(serialize_actor(*fixture.talker) == expected_npc);
    CHECK(serialize_actor(get_avatar()) == expected_avatar);
    CHECK(reads == 1);
}

TEST_CASE(
    "dialogue rejects stale removed foreign and replay schema choices without effects",
    "[client][interaction][client_dialogue][mcp]") {
    auto fixture = dialogue_fixture{};
    auto previous = game_client::interaction_snapshot{};
    auto removed_id = std::string{};
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        previous = game_client::current_interaction();
        const auto conditional = std::ranges::find_if(previous.choices, [](const auto& entry) {
            return entry.label.find("Only displayed before") != std::string::npos;
        });
        REQUIRE(conditional != previous.choices.end());
        removed_id = conditional->id;
        return choose_label(previous, "Goodbye.");
    });
    CHECK(fixture.opt().id == "TALK_DONE");
    get_avatar().set_value(removed_var, "yes");
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto snapshot = game_client::current_interaction();
        CHECK(get_avatar().get_value(removed_var) == "yes");
        CHECK(snapshot.choices.size() + 1 == previous.choices.size());
        CHECK(std::ranges::none_of(snapshot.choices, [](const auto& entry) {
            return entry.label.find("Only displayed before") != std::string::npos;
        }));
        CHECK(snapshot.schema_id != previous.schema_id);
        const auto old_boundary = game_client::resolve_interaction_command({
            .input_id = previous.input_id,
            .operation = game_client::interaction_operation::choose,
            .target_id = previous.choices[0].id,
        });
        CHECK_FALSE(old_boundary);
        const auto removed = game_client::resolve_interaction_command({
            .input_id = snapshot.input_id,
            .operation = game_client::interaction_operation::choose,
            .target_id = removed_id,
        });
        CHECK_FALSE(removed);
        const auto cancel = game_client::resolve_interaction_command({
            .input_id = snapshot.input_id,
            .operation = game_client::interaction_operation::cancel,
        });
        CHECK_FALSE(cancel);
        const auto schema = game_client::validate_interaction_event(
            *game_client::active_input_context().context,
            {.operation = game_client::interaction_operation::choose,
             .schema_id = previous.schema_id,
             .target_id = previous.choices[0].id});
        CHECK_FALSE(schema);
        const auto foreign = game_client::resolve_interaction_command({
            .input_id = snapshot.input_id,
            .operation = game_client::interaction_operation::choose,
            .target_id = "not-a-live-dialogue-response",
        });
        CHECK_FALSE(foreign);
        CHECK(fixture.talker->get_value(branch_var).empty());
        return choose_label(snapshot, "Goodbye.");
    });
    CHECK(fixture.opt().id == "TALK_DONE");
    CHECK(fixture.talker->get_value(branch_var).empty());

    SECTION("foreign topic id") {
        game_client::memory::set_input_provider([&](const int /*timeout*/) {
            const auto snapshot = game_client::current_interaction();
            CHECK_FALSE(game_client::resolve_interaction_command({
                .input_id = snapshot.input_id,
                .operation = game_client::interaction_operation::choose,
                .target_id = previous.choices[0].id,
            }));
            return choose_label(snapshot, "Goodbye.");
        });
        CHECK(fixture.opt(first_topic).id == "TALK_DONE");
    }
    SECTION("foreign NPC id with the same topic and name") {
        auto& other = spawn_npc(get_avatar().bub_pos() + tripoint_north, "test_talker");
        other.name = fixture.talker->name;
        other.op_of_u = npc_opinion{};
        other.set_attitude(NPCATT_NULL);
        fixture.d.beta = &other;
        game_client::memory::set_input_provider([&](const int /*timeout*/) {
            const auto snapshot = game_client::current_interaction();
            CHECK_FALSE(game_client::resolve_interaction_command({
                .input_id = snapshot.input_id,
                .operation = game_client::interaction_operation::choose,
                .target_id = previous.choices[0].id,
            }));
            return choose_label(snapshot, "Goodbye.");
        });
        CHECK(fixture.opt().id == "TALK_DONE");
        CHECK(other.get_value(branch_var).empty());
        CHECK(fixture.talker->get_value(branch_var).empty());
    }
    SECTION("replayed event validation also fails closed in handle_input") {
        game_client::memory::set_input_provider([&](const int /*timeout*/) {
            auto event = input_event{};
            event.type = input_event_t::interaction;
            event.interaction = game_client::interaction_event{
                .operation = game_client::interaction_operation::choose,
                .schema_id = previous.schema_id,
                .target_id = previous.choices[0].id,
            };
            return event;
        });
        CHECK_THROWS_WITH(fixture.opt(), Catch::Matchers::Contains("Semantic interaction rejected"));
        CHECK(fixture.talker->get_value(branch_var).empty());
        CHECK(game_client::active_input_context().context == nullptr);
    }
}

TEST_CASE(
    "dialogue nested native confirmation restores the response context after refusal",
    "[client][interaction][client_dialogue][mcp]") {
    auto fixture = dialogue_fixture{};
    auto reads = 0;
    auto parent_schema = std::string{};
    auto parent_choice = std::string{};
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto snapshot = game_client::current_interaction();
        const auto step = reads++;
        REQUIRE(step < 4);
        if (step == 0 || step == 2) {
            CHECK(snapshot.context == "DIALOGUE_CHOOSE_RESPONSE");
            CHECK(fixture.talker->op_of_u.anger == 0);
            CHECK(fixture.talker->get_attitude() == NPCATT_NULL);
            const auto choice = std::ranges::find_if(snapshot.choices, [](const auto& entry) {
                return entry.label.find("Provoke the NPC") != std::string::npos;
            });
            REQUIRE(choice != snapshot.choices.end());
            if (step == 0) {
                parent_schema = snapshot.schema_id;
                parent_choice = choice->id;
            } else {
                CHECK(snapshot.schema_id == parent_schema);
                CHECK(choice->id == parent_choice);
            }
            return choose(snapshot, choice->id);
        }
        CHECK(snapshot.context == "YESNO");
        CHECK(snapshot.message.find("You may be attacked!") != std::string::npos);
        CHECK(snapshot.schema_id != parent_schema);
        CHECK_FALSE(game_client::resolve_interaction_command({
            .input_id = snapshot.input_id,
            .operation = game_client::interaction_operation::choose,
            .target_id = parent_choice,
        }));
        CHECK(fixture.talker->op_of_u.anger == 0);
        return choose_popup(snapshot, step == 1 ? "NO" : "YES");
    });
    CHECK(fixture.opt().id == "TALK_DONE");
    CHECK(reads == 4);
    CHECK(fixture.talker->op_of_u.anger == 1000);
    CHECK(fixture.talker->get_attitude() == NPCATT_KILL);
    CHECK(game_client::active_input_context().context == nullptr);
}

TEST_CASE(
    "dialogue native missing-item denial remains a nested popup, not a bypass",
    "[client][interaction][client_dialogue][mcp]") {
    auto fixture = dialogue_fixture{};
    get_avatar().remove_primary_weapon();
    auto reads = 0;
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto snapshot = game_client::current_interaction();
        REQUIRE(reads < 2);
        if (reads++ == 0) {
            CHECK(snapshot.context == "DIALOGUE_CHOOSE_RESPONSE");
            return choose_label(snapshot, "Successful native trial");
        }
        CHECK(snapshot.context == "POPUP_WAIT");
        CHECK(snapshot.message.find("doesn't have") != std::string::npos);
        CHECK(snapshot.message.find("TEST platinum bit") != std::string::npos);
        return choose_popup(snapshot, "QUIT");
    });
    CHECK(fixture.opt().id == second_topic);
    CHECK(reads == 2);
    CHECK(get_avatar().charges_of(itype_id("test_platinum_bit")) == 0);
    CHECK(fixture.talker->get_value(trial_var) == "1");
    CHECK(get_avatar().cash == 83);
    CHECK(game_client::active_input_context().context == nullptr);
}

TEST_CASE(
    "dialogue preserves raw keyboard letters navigation paging escape and special actions",
    "[client][interaction][client_dialogue][mcp]") {
    auto fixture = dialogue_fixture{};
    SECTION("letters select the original branch") {
        game_client::memory::set_input_provider([](const int /*timeout*/) {
            return input_event('b', input_event_t::keyboard);
        });
        CHECK(fixture.opt().id == second_topic);
        CHECK(fixture.talker->get_value(branch_var) == "10");
    }
    SECTION("escape is not cancel and arrows still wrap") {
        const auto keys = std::array{KEY_ESCAPE, KEY_UP, KEY_DOWN, KEY_DOWN, KEY_ENTER};
        auto reads = std::size_t{0};
        game_client::memory::set_input_provider([&](const int /*timeout*/) {
            REQUIRE(reads < keys.size());
            const auto snapshot = game_client::current_interaction();
            if (reads == 1) { CHECK(snapshot.choices.front().highlighted); }
            if (reads == 2) { CHECK(snapshot.choices[fixture.d.responses.size() - 1].highlighted); }
            if (reads == 4) { CHECK(snapshot.choices[1].highlighted); }
            return input_event(keys[reads++], input_event_t::keyboard);
        });
        CHECK(fixture.opt().id == second_topic);
        CHECK(reads == keys.size());
    }
    SECTION("page keys move to an actual entry and back without changing schema") {
        const auto keys = std::array{KEY_NPAGE, KEY_PPAGE, KEY_ENTER};
        auto reads = std::size_t{0};
        auto schema = std::string{};
        game_client::memory::set_input_provider([&](const int /*timeout*/) {
            REQUIRE(reads < keys.size());
            const auto snapshot = game_client::current_interaction();
            if (reads == 0) {
                schema = snapshot.schema_id;
            } else {
                CHECK(snapshot.schema_id == schema);
            }
            if (reads == 1) { CHECK_FALSE(snapshot.choices.front().highlighted); }
            if (reads == 2) { CHECK(snapshot.choices.front().highlighted); }
            return input_event(keys[reads++], input_event_t::keyboard);
        });
        CHECK(fixture.opt().id == first_topic);
        CHECK(reads == keys.size());
    }
    SECTION("only native special actions when conditional responses are empty") {
        auto reads = 0;
        game_client::memory::set_input_provider([&](const int /*timeout*/) {
            REQUIRE(++reads == 1);
            const auto snapshot = game_client::current_interaction();
            CHECK(fixture.d.responses.empty());
            REQUIRE(snapshot.choices.size() == 4);
            CHECK(std::ranges::none_of(
                snapshot.choices, &game_client::interaction_choice::highlighted));
            CHECK_FALSE(snapshot.allow_cancel);
            return choose_label(snapshot, "Look at");
        });
        CHECK(fixture.opt("TALK_TEST_CLIENT_DIALOGUE_EMPTY").id == "TALK_LOOK_AT");
        CHECK(fixture.talker->get_value(branch_var).empty());
    }
    SECTION("all existing special keys remain available") {
        const auto key = GENERATE('L', 'S', 'Y', 'O');
        const auto topics = std::map<char, std::string>{
            {'L', "TALK_LOOK_AT"},
            {'S', "TALK_SIZE_UP"},
            {'Y', "TALK_SHOUT"},
            {'O', "TALK_OPINION"},
        };
        game_client::memory::set_input_provider([&](const int /*timeout*/) {
            return input_event(key, input_event_t::keyboard);
        });
        CHECK(fixture.opt().id == topics.at(key));
        CHECK(fixture.talker->get_value(branch_var).empty());
    }
    SECTION("special actions also have semantic choices") {
        const auto label = GENERATE("Look at", "Size up stats", "Yell", "Check opinion");
        const auto topics = std::map<std::string, std::string>{
            {"Look at", "TALK_LOOK_AT"},
            {"Size up stats", "TALK_SIZE_UP"},
            {"Yell", "TALK_SHOUT"},
            {"Check opinion", "TALK_OPINION"},
        };
        game_client::memory::set_input_provider([&](const int /*timeout*/) {
            return choose_label(game_client::current_interaction(), label);
        });
        CHECK(fixture.opt().id == topics.at(label));
        CHECK(fixture.talker->get_value(branch_var).empty());
    }
}

TEST_CASE(
    "semantic dialogue replay repeats the real NPC conversation loop and topic stack",
    "[client][interaction][client_dialogue][replay][mcp]") {
    auto fixture = dialogue_fixture{};
    static auto counter = std::atomic_uint64_t{0};
    const auto directory =
        std::filesystem::path(PATH_INFO::user_dir())
        / ("client-dialogue-replay-" + std::to_string(counter.fetch_add(1)));
    REQUIRE(std::filesystem::create_directory(directory));
    const auto cleanup = on_out_of_scope([&] {
        replay::stop();
        std::filesystem::remove_all(directory);
    });
    const auto path = directory / "input.jsonl";
    auto& you = get_avatar();
    const auto original_npc = serialize_actor(*fixture.talker);
    const auto original_avatar = serialize_actor(you);
    const auto original_known = fixture.talker->get_known_to_u();
    auto* talker_faction = fixture.talker->get_faction();
    REQUIRE(talker_faction != nullptr);
    const auto original_faction = *talker_faction;
    rng_set_engine_seed(74261);
    replay::configure_recording(path.string(), {.rng_seed = 74261});
    replay::start();
    auto reads = 0;
    auto second_branch_id = std::string{};
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto snapshot = game_client::current_interaction();
        REQUIRE(reads < 5);
        CHECK(snapshot.context == "DIALOGUE_CHOOSE_RESPONSE");
        switch (reads++) {
            case 0:
                CHECK(snapshot.message.find("Adapter narrative") != std::string::npos);
                second_branch_id = snapshot.choices[1].id;
                return choose(snapshot, second_branch_id);
            case 1:
                CHECK(snapshot.message.find("Second native branch") != std::string::npos);
                CHECK(fixture.talker->get_value(branch_var) == "10");
                return choose_label(snapshot, "Back to the previous topic");
            case 2:
                CHECK(snapshot.message.find("Adapter narrative") != std::string::npos);
                CHECK(snapshot.choices[1].id == second_branch_id);
                return choose_label(snapshot, "Successful native trial");
            case 3:
                CHECK(snapshot.message.find("Second native branch") != std::string::npos);
                CHECK(you.cash == 83);
                return choose_label(snapshot, "Goodbye.");
            default:
                FAIL("unexpected native dialogue boundary");
                return input_event{};
        }
    });
    fixture.talker->talk_to_u(false, false);
    CHECK(reads == 4);
    CHECK(fixture.talker->get_value(branch_var) == "10");
    CHECK(fixture.talker->get_value(trial_var) == "1");
    CHECK(you.cash == 83);
    CHECK(you.charges_of(itype_id("test_platinum_bit")) == 5);
    CHECK(fixture.talker->get_value(speaker_var) == "2");
    replay::finish();
    const auto recorded_npc = serialize_actor(*fixture.talker);
    const auto recorded_avatar = serialize_actor(you);
    const auto recorded_faction = serialize_actor(*talker_faction);
    const auto recorded_rng = rng_get_engine();
    fixture.reset_effect_state();
    fixture.talker->set_known_to_u(original_known);
    // Keep the live faction object's address stable while resetting native discovery/membership.
    *talker_faction = original_faction;
    REQUIRE(serialize_actor(*fixture.talker) == original_npc);
    REQUIRE(serialize_actor(you) == original_avatar);
    game_client::memory::set_input_provider([](const int /*timeout*/) -> input_event {
        throw std::runtime_error("dialogue playback must not read live input");
    });
    rng_set_engine_seed(74261);
    replay::configure_playback(path.string(), {.rng_seed = 74261});
    replay::start();
    fixture.talker->talk_to_u(false, false);
    CHECK(replay::playback_exhausted());
    replay::finish();
    CHECK(serialize_actor(*fixture.talker) == recorded_npc);
    CHECK(serialize_actor(you) == recorded_avatar);
    CHECK(serialize_actor(*talker_faction) == recorded_faction);
    CHECK(rng_get_engine() == recorded_rng);
    CHECK(game_client::active_input_context().context == nullptr);
}

#endif
