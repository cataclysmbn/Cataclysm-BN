#if defined(CATA_MCP)

#    include "catch/catch.hpp"
#    include "language.h"
#    include "locale_fixture.h"
#    include "locale_scope.h"
#    include "translations.h"

#    include <string>

using namespace locale_fixture;

TEST_CASE(
    "a locale scope words the game in its language and then restores the engine's",
    "[engine_client_locale][locale_scope]") {
    const auto fixture = korean_catalogue{};
    const auto engine = get_language().id;
    CHECK(_("Strength") == "Strength");
    {
        const auto scope = localization::locale_scope{korean};
        CHECK(get_language().id == korean);
        CHECK(_("Strength") == "Strength-ko");
        {
            const auto nested = localization::locale_scope{korean};
            CHECK(_("Strength") == "Strength-ko");
        }
        CHECK(_("Strength") == "Strength-ko");
    }
    CHECK(get_language().id == engine);
    CHECK(_("Strength") == "Strength");
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

#endif
