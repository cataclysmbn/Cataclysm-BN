#if defined(CATA_MCP)

#    include "cata_utility.h"
#    include "catch/catch.hpp"
#    include "client_memory.h"
#    include "dialogue_helpers.h"
#    include "engine_client_component.h"
#    include "engine_client_contract.h"
#    include "json.h"
#    include "path_info.h"
#    include "replay/replay.h"

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
#    include <vector>

namespace {
using namespace dialogue_test;

auto start_id(const int index) -> std::string { return start_topic + ":" + std::to_string(index); }

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
        const auto state = captured();
        const auto screen = ec::top_screen(state);
        CHECK(screen.context == "DIALOGUE_CHOOSE_RESPONSE");
        CHECK(screen.role == "dialog");
        CHECK(screen.title == "Adapter talker");
        CHECK(history(screen).find("Adapter narrative, already evaluated:") != std::string::npos);
        CHECK(history(screen).find("<swear>") == std::string::npos);
        CHECK(history(screen).find(fixture.talker->name) != std::string::npos);
        const auto& list = rows(screen, responses_id);
        REQUIRE(fixture.d.responses.size() == 9);
        REQUIRE(list.size() == fixture.d.responses.size());
        CHECK(list[0].label == list[1].label);
        CHECK(list[0].id != list[1].id);
        CHECK(list[2].label == list[3].label);
        CHECK(list[2].id != list[3].id);
        CHECK(list[4].label.find("100%") != std::string::npos);
        CHECK(list[4].label.find("cost 17 cents") != std::string::npos);
        CHECK(list[0].hotkey == "a");
        CHECK(list[8].hotkey == "i");
        auto ids = std::set<std::string>{};
        for (const auto& row : list) { CHECK(ids.insert(row.id).second); }
        for (const auto& row : rows(screen, special_id)) { CHECK(ids.insert(row.id).second); }
        CHECK(screen.focus_component == responses_id);
        CHECK(screen.focus_row == list.front().id);
        const auto published = ec::serialize_interaction_boundary(state);
        for (const auto query : std::views::iota(0, 8)) {
            INFO("passive query " << query);
            CHECK(ec::serialize_interaction_boundary(captured()) == published);
        }
        CHECK(rng_get_engine() == before);
        CHECK(fixture.talker->get_value(branch_var).empty());
        CHECK(fixture.talker->get_value(trial_var).empty());
        CHECK(get_avatar().cash == 100);
        CHECK(get_avatar().charges_of(itype_id("test_platinum_bit")) == 7);
        CHECK(fixture.talker->get_value(speaker_var) == "1");
        return respond(list[1].id);
    });
    CHECK(fixture.opt().id == second_topic);
    CHECK(fixture.talker->get_value(branch_var) == "10");
    CHECK(reads == 1);
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
        return respond(start_id(index));
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
        return key(success ? 'e' : 'f');
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
        return respond(
            row_with(top(), success ? "Successful native trial" : "Denied native trial"));
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
    "dialogue rejects removed foreign and special rows of the wrong affordance without effects",
    "[client][interaction][client_dialogue][mcp]") {
    auto fixture = dialogue_fixture{};
    auto removed_id = std::string{};
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        removed_id = row_with(top(), "Only displayed before");
        return respond(row_with(top(), "Goodbye."));
    });
    CHECK(fixture.opt().id == "TALK_DONE");
    get_avatar().set_value(removed_var, "yes");
    auto reads = 0;
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto state = captured();
        const auto screen = ec::top_screen(state);
        const auto refused = [&](const std::string& affordance, const std::string& row) {
            const auto result =
                ec::resolve_invoke(state, {.affordance_id = affordance, .args = {.row = row}});
            return !result.has_value() && result.error() == ec::error::invalid_arguments;
        };
        if (reads++ == 0) {
            CHECK(get_avatar().get_value(removed_var) == "yes");
            CHECK(std::ranges::none_of(rows(screen, responses_id), [](const auto& row) {
                return row.label.find("Only displayed before") != std::string::npos;
            }));
            CHECK(refused(respond_id, removed_id));
            CHECK(refused(respond_id, first_topic + ":0"));
            CHECK(refused(respond_id, "special:look_at"));
            CHECK(refused(special_affordance_id, start_id(0)));
            CHECK(refused(respond_id, "not-a-live-dialogue-response"));
            CHECK(std::ranges::none_of(screen.affordances, [](const auto& entry) {
                return entry.role == "cancel";
            }));
            // A row the screen does not hold, delivered anyway, changes nothing.
            return respond("not-a-live-dialogue-response");
        }
        CHECK(fixture.talker->get_value(branch_var).empty());
        return respond(row_with(screen, "Goodbye."));
    });
    CHECK(fixture.opt().id == "TALK_DONE");
    CHECK(reads == 2);
    CHECK(fixture.talker->get_value(branch_var).empty());
}

TEST_CASE(
    "dialogue nested native confirmation restores the response context after refusal",
    "[client][interaction][client_dialogue][mcp]") {
    auto fixture = dialogue_fixture{};
    auto reads = 0;
    auto screen_id = std::string{};
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto state = captured();
        const auto stack = ec::interaction_stack(state);
        const auto step = reads++;
        REQUIRE(step < 4);
        if (step == 0 || step == 2) {
            const auto screen = ec::top_screen(state);
            CHECK(screen.context == "DIALOGUE_CHOOSE_RESPONSE");
            CHECK(fixture.talker->op_of_u.anger == 0);
            CHECK(fixture.talker->get_attitude() == NPCATT_NULL);
            if (step == 0) {
                screen_id = screen.screen_id;
            } else {
                CHECK(screen.screen_id == screen_id);
            }
            return respond(row_with(screen, "Provoke the NPC"));
        }
        CHECK(stack.size() >= 3);
        CHECK(stack[stack.size() - 2].screen_id == screen_id);
        CHECK(stack.back().context == "YESNO");
        CHECK(fixture.talker->op_of_u.anger == 0);
        return act(step == 1 ? "NO" : "YES");
    });
    CHECK(fixture.opt().id == "TALK_DONE");
    CHECK(reads == 4);
    CHECK(fixture.talker->op_of_u.anger == 1000);
    CHECK(fixture.talker->get_attitude() == NPCATT_KILL);
}

TEST_CASE(
    "dialogue native missing-item denial remains a nested popup, not a bypass",
    "[client][interaction][client_dialogue][mcp]") {
    auto fixture = dialogue_fixture{};
    get_avatar().remove_primary_weapon();
    auto reads = 0;
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        REQUIRE(reads < 2);
        if (reads++ == 0) { return respond(row_with(top(), "Successful native trial")); }
        const auto screen = ec::top_screen(captured());
        CHECK(screen.context == "POPUP_WAIT");
        return act("QUIT");
    });
    CHECK(fixture.opt().id == second_topic);
    CHECK(reads == 2);
    CHECK(get_avatar().charges_of(itype_id("test_platinum_bit")) == 0);
    CHECK(fixture.talker->get_value(trial_var) == "1");
    CHECK(get_avatar().cash == 83);
}

TEST_CASE(
    "dialogue preserves raw keyboard letters navigation paging escape and special actions",
    "[client][interaction][client_dialogue][mcp]") {
    auto fixture = dialogue_fixture{};
    SECTION("letters select the original branch") {
        game_client::memory::set_input_provider([](const int /*timeout*/) { return key('b'); });
        CHECK(fixture.opt().id == second_topic);
        CHECK(fixture.talker->get_value(branch_var) == "10");
    }
    SECTION("escape is not cancel and arrows still wrap, the cursor being the focus") {
        const auto keys = std::array{KEY_ESCAPE, KEY_UP, KEY_DOWN, KEY_DOWN, KEY_ENTER};
        auto reads = std::size_t{0};
        game_client::memory::set_input_provider([&](const int /*timeout*/) {
            REQUIRE(reads < keys.size());
            const auto screen = top();
            if (reads == 1) { CHECK(screen.focus_row == start_id(0)); }
            if (reads == 2) { CHECK(screen.focus_row == start_id(8)); }
            if (reads == 4) { CHECK(screen.focus_row == start_id(1)); }
            return key(keys[reads++]);
        });
        CHECK(fixture.opt().id == second_topic);
        CHECK(reads == keys.size());
    }
    SECTION("page keys move to an actual entry and back without changing the ids") {
        const auto keys = std::array{KEY_NPAGE, KEY_PPAGE, KEY_ENTER};
        auto reads = std::size_t{0};
        auto ids = std::vector<std::string>{};
        game_client::memory::set_input_provider([&](const int /*timeout*/) {
            REQUIRE(reads < keys.size());
            const auto screen = top();
            if (reads == 0) {
                ids = ids_of(rows(screen, responses_id));
            } else {
                CHECK(ids_of(rows(screen, responses_id)) == ids);
            }
            if (reads == 1) { CHECK(screen.focus_row != start_id(0)); }
            if (reads == 2) { CHECK(screen.focus_row == start_id(0)); }
            return key(keys[reads++]);
        });
        CHECK(fixture.opt().id == first_topic);
        CHECK(reads == keys.size());
    }
    SECTION("only native special actions when conditional responses are empty") {
        auto reads = 0;
        game_client::memory::set_input_provider([&](const int /*timeout*/) {
            REQUIRE(++reads == 1);
            const auto screen = top();
            CHECK(fixture.d.responses.empty());
            CHECK(rows(screen, responses_id).empty());
            CHECK(screen.focus_row.empty());
            REQUIRE(rows(screen, special_id).size() == 4);
            return invoke(special_affordance_id, {.row = "special:look_at"});
        });
        CHECK(fixture.opt("TALK_TEST_CLIENT_DIALOGUE_EMPTY").id == "TALK_LOOK_AT");
        CHECK(fixture.talker->get_value(branch_var).empty());
    }
    SECTION("all existing special keys remain available") {
        const auto code = GENERATE('L', 'S', 'Y', 'O');
        const auto topics = std::map<char, std::string>{
            {'L', "TALK_LOOK_AT"},
            {'S', "TALK_SIZE_UP"},
            {'Y', "TALK_SHOUT"},
            {'O', "TALK_OPINION"},
        };
        game_client::memory::set_input_provider([&](const int /*timeout*/) { return key(code); });
        CHECK(fixture.opt().id == topics.at(code));
        CHECK(fixture.talker->get_value(branch_var).empty());
    }
    SECTION("special actions are rows with fixed names and the keys of the native ones") {
        const auto row = GENERATE("look_at", "size_up", "yell", "opinion");
        const auto topics = std::map<std::string, std::string>{
            {"look_at", "TALK_LOOK_AT"},
            {"size_up", "TALK_SIZE_UP"},
            {"yell", "TALK_SHOUT"},
            {"opinion", "TALK_OPINION"},
        };
        const auto keys = std::map<
            std::string,
            std::string>{{"look_at", "L"}, {"size_up", "S"}, {"yell", "Y"}, {"opinion", "O"}};
        game_client::memory::set_input_provider([&](const int /*timeout*/) {
            const auto screen = top();
            const auto& list = rows(screen, special_id);
            CHECK(ids_of(list)
                  == std::vector<std::string>{
                      "special:look_at", "special:size_up", "special:yell", "special:opinion"});
            CHECK(std::ranges::find(list, "special:" + std::string{row}, &ec::row::id)->hotkey
                  == keys.at(row));
            return invoke(special_affordance_id, {.row = "special:" + std::string{row}});
        });
        CHECK(fixture.opt().id == topics.at(row));
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
        const auto screen = top();
        REQUIRE(reads < 5);
        switch (reads++) {
            case 0:
                CHECK(history(screen).find("Adapter narrative") != std::string::npos);
                second_branch_id = rows(screen, responses_id)[1].id;
                return respond(second_branch_id);
            case 1:
                CHECK(history(screen).find("Second native branch") != std::string::npos);
                CHECK(fixture.talker->get_value(branch_var) == "10");
                return respond(row_with(screen, "Back to the previous topic"));
            case 2:
                CHECK(history(screen).find("Adapter narrative") != std::string::npos);
                CHECK(rows(screen, responses_id)[1].id == second_branch_id);
                return respond(row_with(screen, "Successful native trial"));
            case 3:
                CHECK(history(screen).find("Second native branch") != std::string::npos);
                CHECK(you.cash == 83);
                return respond(row_with(screen, "Goodbye."));
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
    CHECK_THROWS_AS(replay::next_input_event(), replay::completed);
    replay::finish();
    CHECK(serialize_actor(*fixture.talker) == recorded_npc);
    CHECK(serialize_actor(you) == recorded_avatar);
    CHECK(serialize_actor(*talker_faction) == recorded_faction);
    CHECK(rng_get_engine() == recorded_rng);
}

#endif
