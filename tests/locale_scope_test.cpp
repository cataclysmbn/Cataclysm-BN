#if defined(CATA_MCP)

#    include "avatar.h"
#    include "calendar.h"
#    include "catch/catch.hpp"
#    include "client_interaction.h"
#    include "engine_client_world.h"
#    include "item.h"
#    include "language.h"
#    include "locale_fixture.h"
#    include "locale_scope.h"
#    include "map/map.h"
#    include "map_helpers.h"
#    include "state_helpers.h"
#    include "translations.h"

#    include <ranges>
#    include <string>

namespace ec = engine_client;
using namespace locale_fixture;

TEST_CASE(
    "a locale scope words the game in its language and then restores the engine's",
    "[engine_client_locale][locale_scope]") {
    const auto fixture = korean_catalogue{};
    const auto engine = get_language().id;
    CHECK(std::string{_("Strength")} == "Strength");
    {
        const auto scope = localization::locale_scope{korean};
        CHECK(get_language().id == korean);
        CHECK(std::string{_("Strength")} == "Strength-ko");
        {
            const auto nested = localization::locale_scope{korean};
            CHECK(std::string{_("Strength")} == "Strength-ko");
        }
        CHECK(std::string{_("Strength")} == "Strength-ko");
    }
    CHECK(get_language().id == engine);
    CHECK(std::string{_("Strength")} == "Strength");
}

TEST_CASE(
    "a scope for the language in use, or for none, changes nothing",
    "[engine_client_locale][locale_scope]") {
    const auto version = detail::get_current_language_version();
    for (const auto& id : {get_language().id, std::string{}}) {
        const auto scope = localization::locale_scope{id};
        CHECK(detail::get_current_language_version() == version);
    }
    CHECK(detail::get_current_language_version() == version);
}

TEST_CASE(
    "a language is available when it is in use, defined with a catalogue, or the source",
    "[engine_client_locale][locale_scope]") {
    const auto fixture = korean_catalogue{};
    CHECK(localization::locale_available(korean));
    CHECK(localization::locale_available("en_US"));
    CHECK(localization::locale_available("en"));
    CHECK(localization::locale_available(get_language().id));
    // Defined, but nothing is compiled for it under the base path.
    CHECK_FALSE(localization::locale_available("de_DE"));
    CHECK_FALSE(localization::locale_available("xx_XX"));
    CHECK_FALSE(localization::locale_available(""));
}

TEST_CASE(
    "a loaded language is kept until the catalogues are reloaded",
    "[engine_client_locale][locale_scope]") {
    const auto fixture = korean_catalogue{};
    const auto first = localization::load_locale(korean);
    CHECK(&localization::load_locale(korean).library() == &first.library());
    l10n_data::reload_catalogues();
    CHECK(&localization::load_locale(korean).library() != &first.library());
    CHECK(localization::load_locale(korean).fingerprint() == first.fingerprint());
}

TEST_CASE(
    "an interaction has the same schema and ids in every language",
    "[engine_client_locale][interaction]") {
    const auto fixture = korean_catalogue{};
    const auto boundary = yes_no_boundary{};
    const auto english = game_client::current_interaction();
    auto other = game_client::interaction_snapshot{};
    {
        const auto scope = localization::locale_scope{korean};
        other = game_client::current_interaction();
    }
    REQUIRE(english.choices.size() == 2);
    REQUIRE(other.choices.size() == 2);
    // The words differ, and the oracle is the catalogue itself.
    CHECK(english.title == "Confirm");
    CHECK(other.title == in_korean("Confirm"));
    CHECK(other.choices.front().label == in_korean("Yes"));
    // What a recorded input names is the same, so a replay made in one language plays in the other.
    CHECK(other.schema_id == english.schema_id);
    CHECK(other.choices.front().id == english.choices.front().id);
    CHECK(other.choices.back().id == english.choices.back().id);
}

TEST_CASE("item rows have the same ids in every language", "[engine_client_locale][world]") {
    clear_all_state();
    const auto fixture = korean_catalogue{};
    get_avatar().i_add(item::spawn("test_1kg_cube", calendar::turn));
    const auto english = ec::world::item_stacks(get_map(), get_avatar());
    auto other = ec::world::avatar_item_stacks{};
    {
        const auto scope = localization::locale_scope{korean};
        other = ec::world::item_stacks(get_map(), get_avatar());
    }
    REQUIRE(english.inventory.size() == other.inventory.size());
    REQUIRE_FALSE(english.inventory.empty());
    // The rows are worded in the scope's language, and name the same stacks.
    CHECK(english.inventory.front().row.name != other.inventory.front().row.name);
    for (const auto index : std::views::iota(std::size_t{0}, english.inventory.size())) {
        CHECK(english.inventory[index].row.id == other.inventory[index].row.id);
    }
}

#endif
