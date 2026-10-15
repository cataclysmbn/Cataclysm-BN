#if defined(CATA_MCP)

#    include "catch/catch.hpp"
#    include "engine_client_event.h"
#    include "engine_client_session.h"
#    include "message_feed.h"
#    include "messages.h"
#    include "translations.h"

#    include <nlohmann/json.hpp>
#    include <string>
#    include <vector>

namespace {
namespace ec = engine_client;
using json = nlohmann::json;

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
    SECTION("a value a client cannot render keeps the line a plain string") {
        add_msg(to_translation("Count %s"), to_translation("three"));
        // A character is not a number either.
        add_msg(to_translation("Letter %c"), 'x');
        const auto lines = Messages::feed_since({});
        REQUIRE(lines.size() == 2);
        CHECK(lines.front().text == "Count three");
        CHECK_FALSE(lines.front().localised);
        CHECK(lines.back().text == "Letter x");
        CHECK_FALSE(lines.back().localised);
    }
    SECTION("a string that needs no translation, a plural form and a plain format have no key") {
        add_msg(no_translation("Plain"));
        add_msg(pl_translation("%d rat", "%d rats"), 2);
        add_msg("Format %d", 1);
        add_msg(_("Translated %d"), 1);
        const auto lines = Messages::feed_since({});
        REQUIRE(lines.size() == 4);
        for (const auto& line : lines) { CHECK_FALSE(line.localised); }
        CHECK(lines.front().text == "Plain");
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
        add_msg("Plain %d.", 5);
        const auto line = published_line();
        CHECK(line["text"] == "Plain 5.");
        CHECK(line["label"] == "Plain 5.");
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

#endif // CATA_MCP
