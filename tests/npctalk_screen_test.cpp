#if defined(CATA_MCP)

#    include "catch/catch.hpp"
#    include "client_memory.h"
#    include "dialogue_helpers.h"
#    include "engine_client_component.h"
#    include "locale_fixture.h"
#    include "locale_scope.h"
#    include "mission.h"

#    include <algorithm>
#    include <nlohmann/json.hpp>
#    include <ranges>
#    include <set>
#    include <string>
#    include <vector>

namespace {
using namespace dialogue_test;
using json = nlohmann::json;

const auto identity_topic = std::string{"TALK_TEST_NPCTALK_IDENTITY"};

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

/// The ids of the responses a topic offers, in the order the screen lists them.
auto response_ids(dialogue_fixture& fixture, const std::string& topic) -> std::vector<std::string> {
    auto ids = std::vector<std::string>{};
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        ids = ids_of(rows(top(), responses_id));
        return key('L');
    });
    fixture.opt(topic);
    return ids;
}
auto has_id(const std::vector<std::string>& ids, const std::string& id) -> bool {
    return std::ranges::find(ids, id) != ids.end();
}
auto without(std::vector<std::string> ids, const std::string& id) -> std::vector<std::string> {
    std::erase(ids, id);
    return ids;
}

} // namespace

TEST_CASE(
    "the NPC talk screen is the same at any viewport size and has the same ids in another language",
    "[npctalk_screen][engine_client_component]") {
    auto fixture = dialogue_fixture{};
    const auto fixture_ko = locale_fixture::korean_catalogue{{
        {"Look at", "Look at-ko"},
        {"Size up stats", "Size up stats-ko"},
        {"Yell", "Yell-ko"},
        {"Check opinion", "Check opinion-ko"},
        {"Your response:", "Your response:-ko"},
        {"Dialogue:", "Dialogue:-ko"},
        {"Select", "Select-ko"},
        {"Goodbye.", "Goodbye.-ko"},
    }};
    auto reads = 0;
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
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
        // The words really do differ, so equal ids are not equal text.
        CHECK(narrow.dump() != korean.dump());
        return key('a');
    });
    fixture.opt();
    CHECK(reads == 1);
}

TEST_CASE(
    "the NPC talk screen publishes no window text", "[npctalk_screen][engine_client_component]") {
    auto fixture = dialogue_fixture{};
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto screen = top();
        for (const auto* const list : {responses_id, special_id}) {
            for (const auto& row : rows(screen, list)) {
                CHECK(row.label.find("<color") == std::string::npos);
                CHECK(row.id.find(row.label) == std::string::npos);
            }
        }
        return key('a');
    });
    fixture.opt();
}

TEST_CASE(
    "a JSON response is named by its topic and its place in the topic's data",
    "[npctalk_identity][npctalk_screen]") {
    auto fixture = dialogue_fixture{};
    const auto ids = response_ids(fixture, start_topic);
    REQUIRE(ids.size() == 9);
    for (const auto index : std::views::iota(0, 9)) {
        CHECK(ids[static_cast<std::size_t>(index)] == start_topic + ":" + std::to_string(index));
    }
}

TEST_CASE(
    "a response's name does not change when a conditional response before it appears or disappears",
    "[npctalk_identity][npctalk_screen]") {
    auto fixture = dialogue_fixture{};
    const auto hidden = response_ids(fixture, identity_topic);
    CHECK_FALSE(has_id(hidden, identity_topic + ":0"));
    get_avatar().set_value("npctalk_var_test_npctalk_identity_flagged", "yes");
    const auto shown = response_ids(fixture, identity_topic);
    CHECK(has_id(shown, identity_topic + ":0"));
    CHECK(shown.size() == hidden.size() + 1);
    CHECK(without(shown, identity_topic + ":0") == hidden);
    CHECK(has_id(hidden, identity_topic + ":1"));
    CHECK(has_id(hidden, identity_topic + ":3"));
}

TEST_CASE(
    "a response with an explicit id in the data is named by it",
    "[npctalk_identity][npctalk_screen]") {
    auto fixture = dialogue_fixture{};
    const auto ids = response_ids(fixture, identity_topic);
    CHECK(has_id(ids, "leave"));
    CHECK_FALSE(has_id(ids, identity_topic + ":2"));
}

TEST_CASE(
    "the responses of a topic that several data files extend are numbered across them",
    "[npctalk_identity][npctalk_screen]") {
    auto fixture = dialogue_fixture{};
    const auto topic = std::string{"TALK_TEST_NPCTALK_EXTENDED"};
    CHECK(response_ids(fixture, topic) == std::vector<std::string>{topic + ":0", topic + ":1"});
}

TEST_CASE(
    "a repeated response is named by what it is about, not by its place in the shown list",
    "[npctalk_identity][npctalk_screen]") {
    auto fixture = dialogue_fixture{};
    auto& you = get_avatar();
    you.wear_item(item::spawn("tshirt"), false);
    you.wear_item(item::spawn("jeans"), false);
    const auto before = response_ids(fixture, identity_topic);
    CHECK(has_id(before, identity_topic + ":repeat:0:tshirt"));
    CHECK(has_id(before, identity_topic + ":repeat:0:jeans"));
    CHECK(has_id(before, identity_topic + ":repeat:1:tshirt"));
    CHECK(has_id(before, identity_topic + ":repeat:1:jeans"));
    // A second shirt is a second response about the same thing, named apart without a place in the
    // list.
    you.wear_item(item::spawn("tshirt"), false);
    const auto after = response_ids(fixture, identity_topic);
    CHECK(after.size() == before.size() + 1);
    CHECK(has_id(after, identity_topic + ":repeat:1:tshirt:2"));
    CHECK(std::set(after.begin(), after.end()).size() == after.size());
    for (const auto& id : before) { CHECK(has_id(after, id)); }
}

TEST_CASE(
    "a response made by the game is named by a fixed name of the place that makes it",
    "[npctalk_identity][npctalk_screen]") {
    auto fixture = dialogue_fixture{};
    CHECK(response_ids(fixture, "TALK_HOW_MUCH_FURTHER")
          == std::vector<
              std::string>{"TALK_HOW_MUCH_FURTHER:thanks", "TALK_HOW_MUCH_FURTHER:keep_moving"});
    get_avatar().toggle_trait(trait_id("DEBUG_MIND_CONTROL"));
    const auto debug = response_ids(fixture, "TALK_HOW_MUCH_FURTHER");
    CHECK(has_id(debug, "debug:obey"));
    CHECK(has_id(debug, "debug:bye"));
}

TEST_CASE(
    "a mission response is named by the mission it is about",
    "[npctalk_identity][npctalk_screen]") {
    auto fixture = dialogue_fixture{};
    auto& talker = *fixture.talker;
    auto missions = std::vector<mission*>{};
    for (const mission_type& type : mission_type::get_all()) {
        if (type.goal == MGOAL_ASSASSINATE && missions.size() < 2) {
            missions.push_back(mission::reserve_new(type.id, talker.getID()));
        }
    }
    REQUIRE(missions.size() == 2);
    talker.chatbin.missions = missions;
    const auto ids = response_ids(fixture, "TALK_MISSION_LIST");
    for (const auto* const mission : missions) {
        CHECK(has_id(ids, "TALK_MISSION_LIST:mission:" + std::to_string(mission->get_id())));
    }
    talker.chatbin.missions = {missions.front()};
    CHECK(has_id(response_ids(fixture, "TALK_MISSION_LIST"), "TALK_MISSION_LIST:offer"));
}

#endif
