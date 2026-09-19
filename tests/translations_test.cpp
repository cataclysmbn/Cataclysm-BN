#include "catch/catch.hpp"
#include "language.h"
#include "options.h"
#include "options_helpers.h"
#include "string_formatter.h"
#include "translations.h"

// wrapping in another macro to prevent collection of the test string for translation
#ifndef TRANSLATE_MACRO
#    define TRANSLATE_MACRO(msg) _(msg)
#endif

// wrapping in another macro to prevent collection of the test string for translation
#ifndef TRANSLATE_TRANSLATION
#    define TRANSLATE_TRANSLATION(msg) to_translation(msg).translated()
#endif

TEST_CASE("translations_sanity_test", "[translations][i18n]") {
    const std::string test_string = "__untranslated_test_string__";

    CHECK(TRANSLATE_MACRO(test_string) == test_string);
    CHECK(TRANSLATE_TRANSLATION(test_string) == test_string);
}

// assuming [en] language is used for this test
// test should succeed both with and without the .mo file
TEST_CASE("translations_macro_string_stability", "[translations][i18n]") {
    std::vector<std::string> test_strings;

    SECTION("untranslated strings") {
        test_strings = {"__untranslated_string1__", "__untranslated_string2__"};
    }

    SECTION("translated strings") { test_strings = {"thread", "yarn"}; }

    std::string test_arg;
    // note: this TRANSLATE_MACRO called in different iterations of the `for` loop will have
    //  the same static cache
    for (const std::string& test_string : test_strings) {
        test_arg.assign(test_string);
        // translation result should be assignable to the `const std::string &` local variable
        const std::string& res1 = TRANSLATE_MACRO(test_arg);
        CHECK(res1 == test_string);
    }
}

// assuming [en] language is used for this test
// test should succeed both with and without the .mo file
TEST_CASE("translations_macro_char_address", "[translations][i18n]") {
    SECTION("address should be same when translation is absent") {
        const char* test_string = "__untranslated_string1__";
        CHECK(TRANSLATE_MACRO(test_string) == test_string);
    }

    SECTION("current address of the arg should be returned when translation is absent") {
        // wrapped in lambda to ensure that different calls hit the same internal cache
        const auto translate = [](const char* msg) { return TRANSLATE_MACRO(msg); };

        // same content, different address (std::string is used to ensure that)
        const std::string test_string1 = "__untranslated_string1__";
        const std::string test_string2 = "__untranslated_string1__";
        // address should be different!
        REQUIRE(test_string1.c_str() != test_string2.c_str());
        // note: address comparison is intended
        CHECK(translate(test_string1.c_str()) == test_string1.c_str());
        CHECK(translate(test_string2.c_str()) == test_string2.c_str());
        CHECK(translate(test_string1.c_str()) == test_string1.c_str());
    }
}

TEST_CASE("translations_add_context", "[translations][i18n]") {
    SECTION("if context is absent, set new context as translation's context") {
        translation orig = translation::to_translation("msg");
        translation exp = translation::to_translation("ctxt", "msg");
        translation combined = orig;
        REQUIRE(combined == orig);
        REQUIRE(combined != exp);
        combined.add_context("ctxt");
        REQUIRE(combined != orig);
        REQUIRE(combined == exp);
    }
    SECTION("if context is present, append new context to existing context") {
        translation orig = translation::to_translation("yay", "msg");
        translation exp = translation::to_translation("yay|ctxt", "msg");
        translation combined = orig;
        REQUIRE(combined == orig);
        REQUIRE(combined != exp);
        combined.add_context("ctxt");
        REQUIRE(combined != orig);
        REQUIRE(combined == exp);
    }
}

// assuming [en] language is used for this test
// requires .mo file for "en" language
TEST_CASE("translations_macro_char_address_translated", "[.][translations][i18n]") {
    if (!try_set_utf8_locale()) {
        // On platforms where we can't set the locale, ignore this test
        WARN("Skipped (unable to set locale)");
        return;
    }
    if (!translations_exists_for_lang("en_US")) {
        WARN("Skipped (translation files not found)");
        return;
    }

    set_language();
    // translated string
    const char* test_string = "thread";

    // should return a stable address of translation that is different from the argument
    const char* translated = TRANSLATE_MACRO(test_string);
    CHECK(translated != test_string);
}

struct trans_test_case {
    std::string id;
    std::string str;
    bool must_have_files = false;
};

// requires .mo files for languages listed below
TEST_CASE("translations_actually_translate", "[translations][i18n]") {
    const std::vector<trans_test_case> test_cases = {{
        {"en_US", "<R|r>andom Character", false},
        {"fr_FR", "Personnage <A|a>léatoire", true},
        {"ru_RU", "<R|r> Случайный персонаж", true},
    }};

    const char* test_msgid = "<R|r>andom Character";
    const char* test_msgctx = "Main Menu|New Game";

    if (!try_set_utf8_locale()) {
        // On platforms where we can't set the locale, ignore this test
        WARN("Skipped (unable to set locale)");
        return;
    }

    const auto has_lang = [&](const std::string& id) -> bool {
        for (const language_info& li : list_available_languages()) {
            if (li.id == id) { return true; }
        }
        return false;
    };

    for (const auto& test : test_cases) {
        const std::string& lang = test.id;
        CAPTURE(lang);
        REQUIRE(has_lang(lang));
        if (test.must_have_files && !translations_exists_for_lang(lang)) {
            WARN(string_format("Skipped (translation files not found for lang '%s')", lang));
            return;
        }
    }

    const static std::string USE_LANG("USE_LANG");

    for (const auto& test : test_cases) {
        CAPTURE(test.id);

        get_options().get_option(USE_LANG).setValue(test.id);
        get_options().save();
        CHECK(get_option<std::string>(USE_LANG) == test.id);

        set_language();

        // Should return translated string (or original/same string for English)
        const char* translated = pgettext(test_msgctx, test_msgid);
        CHECK(test.str == translated);
    }

    // Restore language
    get_options().get_option(USE_LANG).setValue("en_US");
    get_options().save();
    set_language();
}
