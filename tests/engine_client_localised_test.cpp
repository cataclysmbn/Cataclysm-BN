#if defined(CATA_MCP)

#    include "activity_actor_definitions.h"
#    include "activity_handlers.h"
#    include "avatar.h"
#    include "avatar_functions.h"
#    include "catch/catch.hpp"
#    include "engine_client_event.h"
#    include "engine_client_session.h"
#    include "explosion.h"
#    include "explosion_queue.h"
#    include "game.h"
#    include "iexamine.h"
#    include "item.h"
#    include "iuse.h"
#    include "locale_fixture.h"
#    include "locale_scope.h"
#    include "map_helpers.h"
#    include "map_perception.h"
#    include "message_feed.h"
#    include "messages.h"
#    include "monattack.h"
#    include "mondeath.h"
#    include "monexamine.h"
#    include "monster.h"
#    include "npc.h"
#    include "npctalk.h"
#    include "player_activity.h"
#    include "player_helpers.h"
#    include "state_helpers.h"
#    include "translations.h"
#    include "trap.h"
#    include "vehicle/vehicle.h"

#    include <nlohmann/json.hpp>
#    include <string>
#    include <vector>

namespace {
namespace ec = engine_client;
using json = nlohmann::json;
using namespace locale_fixture;

/// The newest line of the native log, as a reader of the feed sees it.
auto newest_line() -> Messages::feed_entry {
    const auto lines = Messages::feed_since({});
    REQUIRE_FALSE(lines.empty());
    return lines.back();
}

/// The newest line of the log as `bn.subscribe` carries it.
auto published_line() -> json {
    auto session = ec::session{};
    REQUIRE(session.publish_boundary());
    const auto current = session.current();
    REQUIRE(current);
    const auto wire = ec::serialize_snapshot(*current);
    REQUIRE(wire);
    return json::parse(wire->header)["messages"].back();
}

/// A zombie the avatar sees.
auto seen_zombie() -> monster& {
    clear_all_state();
    build_test_map(ter_id("t_floor"));
    get_avatar().setpos(tripoint_bub_ms{60, 60, 0});
    auto& zombie = spawn_test_monster("mon_zombie", tripoint_bub_ms{62, 60, 0});
    get_map().build_map_cache(0);
    map_perception::acquire();
    REQUIRE(get_avatar().sees(zombie));
    return zombie;
}
} // namespace

TEST_CASE(
    "a line made from a translatable format keeps its native text and carries the format and values",
    "[engine_client_localised][messages]") {
    Messages::clear_messages();

    SECTION("the text is the rendering the native log shows") {
        add_msg(m_good, to_translation("The %s dies!"), std::string{"zombie"});
        const auto line = newest_line();
        CHECK(line.text == "The zombie dies!");
        CHECK(line.type == m_good);
        CHECK(
            line.localised
            == localised_text{
                .key = "The %s dies!",
                .ctx = std::nullopt,
                .plural = std::nullopt,
                .args = {std::string{"zombie"}}});
    }
    SECTION("numbers stay numbers and strings stay strings") {
        add_msg(to_translation("%s hits %d for %.1f"), "Ada", 7, 2.5);
        const auto line = newest_line();
        CHECK(line.text == "Ada hits 7 for 2.5");
        REQUIRE(line.localised);
        CHECK(line.localised->args
              == std::vector<localised_text::arg>{std::string{"Ada"}, std::int64_t{7}, 2.5});
    }
    SECTION("the context of the format goes along") {
        add_msg(to_translation("noun", "Fire"));
        REQUIRE(newest_line().localised);
        CHECK(newest_line().localised->ctx == "noun");
        CHECK(newest_line().localised->key == "Fire");
    }
    SECTION(
        "a translatable value is a nested LocalisedString and a character keeps the line a string") {
        add_msg(to_translation("Count %s"), to_translation("three"));
        // A character is not a number either.
        add_msg(to_translation("Letter %c"), 'x');
        const auto lines = Messages::feed_since({});
        REQUIRE(lines.size() == 2);
        CHECK(lines.front().text == "Count three");
        CHECK(lines.front().localised
              == localised_text{
                  .key = "Count %s", .args = {localised::nested(localised_text{.key = "three"})}});
        CHECK(lines.back().text == "Letter x");
        CHECK_FALSE(lines.back().localised);
    }
    SECTION(
        "a name reads in the engine language in its form for the count and carries both msgids") {
        add_msg(to_translation("Two %s"), localised::name_of(pl_translation("rat", "rats"), 2));
        const auto line = newest_line();
        CHECK(line.text == "Two rats");
        CHECK(
            line.localised
            == localised_text{
                .key = "Two %s",
                .args = {localised::nested(
                    localised_text{.key = "rat", .plural = 2.0, .plural_key = "rats"})}});
    }
    SECTION("a string that needs no translation and an already rendered line have no key") {
        add_msg(no_translation("Plain"));
        Messages::add_msg("Already rendered 1");
        const auto lines = Messages::feed_since({});
        REQUIRE(lines.size() == 2);
        for (const auto& line : lines) { CHECK_FALSE(line.localised); }
        CHECK(lines.front().text == "Plain");
    }
    SECTION("a plural format reads in its singular form, as the native log shows it") {
        add_msg(pl_translation("%d rat", "%d rats"), 2);
        CHECK(newest_line().text == "2 rat");
        CHECK(
            newest_line().localised == localised_text{.key = "%d rat", .args = {std::int64_t{2}}});
    }
    SECTION("a debug line follows the debug mode, as every other overload does") {
        add_msg(m_debug, to_translation("Hidden %d"), 1);
        CHECK(Messages::feed_since({}).empty());
    }
    SECTION("a repeat keeps the format and counts") {
        add_msg(to_translation("Again %d"), 4);
        add_msg(to_translation("Again %d"), 4);
        const auto lines = Messages::feed_since({});
        REQUIRE(lines.size() == 1);
        CHECK(lines.front().count == 2);
        CHECK(lines.front().localised);
    }
}

TEST_CASE(
    "a converted line is a LocalisedString on the wire and a plain one stays a string",
    "[engine_client_localised][engine_client_session]") {
    Messages::clear_messages();

    SECTION("text and label hold the key and the values") {
        add_msg(m_bad, to_translation("The %s hits %d times."),
                std::string{"<color_red>zombie</color>"}, 3);
        const auto line = published_line();
        const auto expected = json::parse(R"({"key":"The %s hits %d times.","args":["zombie",3]})");
        CHECK(line["text"] == expected);
        CHECK(line["label"] == expected);
        CHECK(line["kind"] == "bad");
        CHECK_FALSE(line.contains("lang"));
    }
    SECTION("a repeat labels the text with the native suffix message") {
        add_msg(to_translation("Again."));
        add_msg(to_translation("Again."));
        const auto line = published_line();
        CHECK(line["count"] == 2);
        CHECK(line["text"] == json::parse(R"({"key":"Again."})"));
        CHECK(line["label"] == json::parse(R"({"key":"%s x %d","args":[{"key":"Again."},2]})"));
    }
    SECTION("a line with no format is the string it was") {
        Messages::add_msg("Plain 5.");
        const auto line = published_line();
        CHECK(line["text"] == "Plain 5.");
        CHECK(line["label"] == "Plain 5.");
    }
    SECTION("a name is a nested LocalisedString with its count and its plural msgid") {
        add_msg(to_translation("Two %s"), localised::name_of(pl_translation("rat", "rats"), 2));
        CHECK(published_line()["text"]
              == json::parse(
                  R"({"key":"Two %s","args":[{"key":"rat","plural":2.0,"plural_key":"rats"}]})"));
    }
    SECTION("the plural count and the context are members of the LocalisedString") {
        Messages::add_msg(
            m_neutral, "2 rats",
            localised_text{
                .key = "%d rat", .ctx = "animal", .plural = 2.0, .args = {std::int64_t{2}}});
        CHECK(published_line()["text"]
              == json::parse(R"({"key":"%d rat","ctx":"animal","plural":2.0,"args":[2]})"));
    }
}

TEST_CASE(
    "the monster death messages read as before in the engine language and carry their format",
    "[engine_client_localised][messages]") {
    auto& zombie = seen_zombie();
    Messages::clear_messages();

    SECTION("in English") {
        mdeath::disappear(zombie);
        mdeath::melt(zombie);
        mdeath::disintegrate(zombie);
        const auto lines = Messages::feed_since({});
        REQUIRE(lines.size() == 3);
        CHECK(lines[0].text == "The zombie disappears.");
        CHECK(lines[1].text == "The zombie melts away.");
        CHECK(lines[2].text == "The zombie disintegrates!");
        CHECK(
            lines[0].localised
            == localised_text{
                .key = "The %s disappears.",
                .args = {localised::nested(
                    localised_text{.key = "zombie", .plural = 1.0, .plural_key = "zombies"})}});
        CHECK(lines[1].localised->key == "The %s melts away.");
        CHECK(lines[2].localised->key == "The %s disintegrates!");
        for (const auto& line : lines) { CHECK(line.type == m_good); }
    }
    SECTION("a monster with a name of its own joins its kind and that name") {
        zombie.unique_name = "Bob";
        mdeath::disappear(zombie);
        const auto line = newest_line();
        CHECK(line.text == "The zombie: Bob disappears.");
        REQUIRE(line.localised);
        const auto kind = localised_text{.key = "zombie", .plural = 1.0, .plural_key = "zombies"};
        CHECK(
            line.localised->args
            == std::vector<localised_text::arg>{localised::nested(localised_text{
                .key = "",
                .args = {localised::nested(kind), std::string{": "}, std::string{"Bob"}}})});
    }
    SECTION("in a translated engine language the text is translated and the key stays the msgid") {
        const auto fixture = korean_catalogue{{{"The %s disappears.", "%s-ko disappears."}}};
        {
            const auto scope = localization::locale_scope{korean};
            mdeath::disappear(zombie);
        }
        const auto line = newest_line();
        CHECK(line.text == "zombie-ko disappears.");
        REQUIRE(line.localised);
        CHECK(line.localised->key == "The %s disappears.");
    }
}

TEST_CASE(
    "the avatar's own lines carry their format and a plural format its count, and an NPC's do not",
    "[engine_client_localised][messages]") {
    clear_all_state();
    build_test_map(ter_id("t_floor"));
    auto& you = get_avatar();
    you.setpos(tripoint_bub_ms{60, 60, 0});
    Messages::clear_messages();

    SECTION(
        "a plural format reads in its form for the count and carries both msgids and the count") {
        add_msg(
            m_info,
            localised::counted{
                .format = pl_translation("You drop %d item.", "You drop %d items."), .count = 3},
            3);
        const auto line = newest_line();
        CHECK(line.text == "You drop 3 items.");
        CHECK(line.type == m_info);
        CHECK(
            line.localised
            == localised_text{
                .key = "You drop %d item.",
                .plural = 3.0,
                .plural_key = "You drop %d items.",
                .args = {std::int64_t{3}}});
    }
    SECTION("the avatar's line and its line of a pair") {
        you.add_msg_if_player(m_good, to_translation("You see %s."), std::string{"a rat"});
        CHECK(newest_line().text == "You see a rat.");
        CHECK(newest_line().type == m_good);
        CHECK(newest_line().localised
              == localised_text{.key = "You see %s.", .args = {std::string{"a rat"}}});
        you.add_msg_if_player(
            localised::counted{.format = pl_translation("%d rat", "%d rats"), .count = 2}, 2);
        CHECK(newest_line().text == "2 rats");
        REQUIRE(newest_line().localised);
        CHECK(newest_line().localised->plural == 2.0);
        you.add_msg_player_or_npc(to_translation("You fall."), to_translation("<npcname> falls."));
        CHECK(newest_line().text == "You fall.");
        CHECK(newest_line().localised == localised_text{.key = "You fall."});
    }
    SECTION("an NPC's line and its line of a pair do not reach the log as the avatar's") {
        auto& other = spawn_npc(tripoint_bub_ms{61, 60, 0}, "test_talker");
        other.add_msg_if_player(to_translation("You see %s."), std::string{"a rat"});
        // The NPC's own line of the pair goes to the global log, which unit tests keep quiet.
        other
            .add_msg_player_or_npc(to_translation("You fall."), to_translation("<npcname> falls."));
        CHECK(Messages::feed_since({}).empty());
    }
}

TEST_CASE(
    "a native plural line carries its format, its plural msgid and its count",
    "[engine_client_localised][messages]") {
    clear_all_state();
    build_test_map(ter_id("t_floor"));
    auto& you = get_avatar();
    you.setpos(tripoint_bub_ms{60, 60, 0});
    Messages::clear_messages();
    drop_on_map(you, item_drop_reason::too_large,
                item::spawn("test_platinum_bit", calendar::turn, 2), you.bub_pos());
    const auto line = newest_line();
    REQUIRE(line.localised);
    CHECK(line.localised->key == "There's no room in your inventory for the %s, so you drop it.");
    CHECK(line.localised->plural == 2.0);
    CHECK(line.localised->plural_key
          == "There's no room in your inventory for the %s, so you drop them.");
    CHECK(line.localised->args.size() == 1);
}

TEST_CASE(
    "a monster's attack line carries its format and the monster's name as a LocalisedString",
    "[engine_client_localised][messages]") {
    auto& zombie = seen_zombie();
    Messages::clear_messages();
    zombie.set_dest(get_avatar().bub_pos());
    REQUIRE(mattack::shriek_alert(&zombie));
    const auto line = newest_line();
    CHECK(line.text == "The zombie begins shrieking!");
    CHECK(
        line.localised
        == localised_text{
            .key = "The %s begins shrieking!",
            .args = {localised::nested(
                localised_text{.key = "zombie", .plural = 1.0, .plural_key = "zombies"})}});
}

TEST_CASE(
    "a trap's line carries its format as a LocalisedString",
    "[engine_client_localised][messages]") {
    seen_zombie();
    Messages::clear_messages();
    auto& you = get_avatar();
    REQUIRE(trapfunc::bubble(you.bub_pos(), &you, nullptr));
    CHECK(Messages::feed_since({}).front().localised
          == localised_text{.key = "You step on some bubble wrap!"});
}

TEST_CASE(
    "the lines of using and examining things carry their formats as LocalisedStrings",
    "[engine_client_localised][messages]") {
    seen_zombie();
    Messages::clear_messages();
    auto& you = get_avatar();
    auto shot = item::spawn("vaccine_shot");
    iuse::vaccine(&you, &*shot, false, you.bub_pos());
    CHECK(newest_line().localised == localised_text{.key = "You feel tough."});
    iexamine::none(you, you.bub_pos());
    const auto line = newest_line();
    CHECK(line.text == "That is a wooden floor.");
    CHECK(line.localised == localised_text{.key = "That is a %s.", .args = {"wooden floor"}});
}

TEST_CASE(
    "a character's line carries its format as a LocalisedString",
    "[engine_client_localised][messages]") {
    seen_zombie();
    auto& you = get_avatar();
    REQUIRE(!you.wear_item(item::spawn("tshirt"), false));
    Messages::clear_messages();
    REQUIRE(you.takeoff(*you.worn.front()));
    const auto line = newest_line();
    REQUIRE(line.localised);
    CHECK(line.localised->key == "You take off your %s.");
    CHECK(line.localised->args.size() == 1);
}

TEST_CASE(
    "an action's line carries its format as a LocalisedString",
    "[engine_client_localised][messages]") {
    seen_zombie();
    Messages::clear_messages();
    avatar_funcs::try_to_sleep(get_avatar());
    CHECK(newest_line().localised == localised_text{.key = "You start trying to fall asleep."});
}

TEST_CASE(
    "an activity's line carries its format as a LocalisedString",
    "[engine_client_localised][messages]") {
    seen_zombie();
    Messages::clear_messages();
    auto act = player_activity{};
    autodrive_activity_actor{}.canceled(act, get_avatar());
    CHECK(newest_line().localised == localised_text{.key = "Auto-drive canceled."});
}

TEST_CASE(
    "a vehicle's line carries its format as a LocalisedString",
    "[engine_client_localised][messages]") {
    seen_zombie();
    REQUIRE(get_map()
                .add_vehicle(vproto_id("bicycle"), get_avatar().bub_pos(), 0_degrees, 0, 0, false));
    Messages::clear_messages();
    handbrake();
    CHECK(newest_line().localised == localised_text{.key = "You pull a handbrake."});
}

TEST_CASE(
    "a pet's line carries its format as a LocalisedString", "[engine_client_localised][messages]") {
    auto& zombie = seen_zombie();
    Messages::clear_messages();
    monexamine::tie_pet(zombie);
    const auto line = newest_line();
    CHECK(line.text == "You tie your zombie.");
    CHECK(line.localised == localised_text{.key = "You tie your %s.", .args = {"zombie"}});
}

TEST_CASE(
    "a combat line carries its format as a LocalisedString",
    "[engine_client_localised][messages]") {
    auto& zombie = seen_zombie();
    Messages::clear_messages();
    explosion_handler::
        shockwave(zombie.bub_pos() + tripoint_east, {.radius = 1}, "explosion", nullptr);
    explosion_handler::get_explosion_queue().execute();
    const auto lines = Messages::feed_since({});
    const auto line = std::ranges::
        find(lines, std::string{"zombie is caught in the shockwave!"}, &Messages::feed_entry::text);
    REQUIRE(line != lines.end());
    CHECK(
        line->localised
        == localised_text{
            .key = "%s is caught in the shockwave!",
            .args = {localised::nested(
                localised_text{.key = "zombie", .plural = 1.0, .plural_key = "zombies"})}});
}

TEST_CASE(
    "an NPC's line carries its format as a LocalisedString",
    "[engine_client_localised][messages]") {
    seen_zombie();
    auto& other = spawn_npc(tripoint_bub_ms{61, 60, 0}, "test_talker");
    Messages::clear_messages();
    talk_function::stranger_neutral(other);
    CHECK(newest_line().localised
          == localised_text{.key = "%s feels less threatened by you.", .args = {other.name}});
}

TEST_CASE(
    "a mutation's line carries its format as a LocalisedString",
    "[engine_client_localised][messages]") {
    seen_zombie();
    auto& you = get_avatar();
    const auto web = trait_id("WEB_WEAVER");
    you.set_mutation(web);
    Messages::clear_messages();
    you.activate_mutation(web);
    CHECK(newest_line().localised
          == localised_text{.key = "You start spinning web with your spinnerets!"});
}

TEST_CASE("a format with no values reads as written", "[engine_client_localised][messages]") {
    Messages::clear_messages();
    add_msg(to_translation("You reeled in %s."));
    CHECK(newest_line().text == "You reeled in %s.");
}

TEST_CASE(
    "a line worded by the game data carries its format as a LocalisedString",
    "[engine_client_localised][messages]") {
    seen_zombie();
    auto& you = get_avatar();
    auto& grenade = you.i_add(item::spawn("gasbomb"));
    Messages::clear_messages();
    you.invoke_item(&grenade);
    const auto line = newest_line();
    REQUIRE(line.localised);
    CHECK(line.localised->key == "You pull the pin on the %s.");
}

#endif // CATA_MCP
