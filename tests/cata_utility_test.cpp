#include "assertion_helpers.h"
#include "cata_utility.h"
#include "catch/catch.hpp"
#include "string_utils.h"
#include "units.h"
#include "units_utility.h"

TEST_CASE("divide_round_up", "[utility]") {
    CHECK(divide_round_up(0, 5) == 0);
    CHECK(divide_round_up(1, 5) == 1);
    CHECK(divide_round_up(4, 5) == 1);
    CHECK(divide_round_up(5, 5) == 1);
    CHECK(divide_round_up(6, 5) == 2);
}

TEST_CASE("divide_round_up_units", "[utility]") {
    CHECK(divide_round_up(0_ml, 5_ml) == 0);
    CHECK(divide_round_up(1_ml, 5_ml) == 1);
    CHECK(divide_round_up(4_ml, 5_ml) == 1);
    CHECK(divide_round_up(5_ml, 5_ml) == 1);
    CHECK(divide_round_up(6_ml, 5_ml) == 2);
}

struct repl_test_data {
    size_t serial;
    std::string input;
    std::string what;
    std::string with;
    std::string expected;
};

TEST_CASE("replace_all", "[utility]") {
    static const std::vector<repl_test_data> data = {{
        {0, "aaaaaaa", "aa", "aaab", "aaabaaabaaaba"},
        {1, "aaaaaaa", "bb", "aa", "aaaaaaa"},
        {2, "", "", "", ""},
        {3, "a", "a", "", ""},
        {4, "", "a", "a", ""},
        {5, "", "", "a", ""},
        {6, "a", "", "a", "a"},
    }};

    for (const repl_test_data& it : data) {
        CAPTURE(it.serial);
        std::string res = replace_all(it.input, it.what, it.with);
        CHECK(res == it.expected);
    }
}

TEST_CASE("replace_first", "[utility]") {
    static const std::vector<repl_test_data> data = {{
        {0, "aaaaaaa", "aa", "aaab", "aaabaaaaa"},
        {1, "aaaaaaa", "bb", "aa", "aaaaaaa"},
        {2, "", "", "", ""},
        {3, "a", "a", "", ""},
        {4, "", "a", "a", ""},
        {5, "", "", "a", ""},
        {6, "a", "", "a", "a"},
    }};

    for (const repl_test_data& it : data) {
        CAPTURE(it.serial);
        std::string text = it.input;
        replace_first(text, it.what, it.with);
        CHECK(text == it.expected);
    }
}

TEST_CASE("trim_whitespaces", "[utility]") {
    CHECK(trim_whitespaces("abc") == "abc");
    CHECK(trim_whitespaces("  abc") == "abc");
    CHECK(trim_whitespaces("abc  ") == "abc");
    CHECK(trim_whitespaces("  abc  ") == "abc");
    // NOLINTNEXTLINE(cata-text-style)
    CHECK(trim_whitespaces("  \t  \t  abc\t\t  \t\t\t  ") == "abc");
    // NOLINTNEXTLINE(cata-text-style)
    CHECK(trim_whitespaces("abc  \t   def") == "abc  \t   def");
    // NOLINTNEXTLINE(cata-text-style)
    CHECK(trim_whitespaces("  abc  \t   def \t") == "abc  \t   def");
    // NOLINTNEXTLINE(readability-container-size-empty)
    CHECK(trim_whitespaces("   ") == "");
    // NOLINTNEXTLINE(readability-container-size-empty)
    CHECK(trim_whitespaces(" ") == "");
    // NOLINTNEXTLINE(readability-container-size-empty, cata-text-style)
    CHECK(trim_whitespaces("\t") == "");
    // NOLINTNEXTLINE(readability-container-size-empty)
    CHECK(trim_whitespaces("") == "");
}

TEST_CASE("equal_ignoring_elements", "[utility]") {
    SECTION("empty sets") {
        CHECK(equal_ignoring_elements<std::set<int>>({1, 2, 3}, {1, 2, 3}, {}));
        CHECK(equal_ignoring_elements<std::set<int>>({}, {}, {}));
        CHECK(equal_ignoring_elements<std::set<int>>({1, 2, 3}, {1, 2, 3}, {1, 2, 3}));
        CHECK(equal_ignoring_elements<std::set<int>>({1}, {1}, {1}));
    }

    SECTION("single element ignored") {
        int el = GENERATE(-1, 0, 1, 2, 3, 4);
        CAPTURE(el);
        CHECK(equal_ignoring_elements<std::set<int>>({1, 2, 3}, {1, 2, 3}, {el}));
    }

    SECTION("not equal, single element ignored") {
        int el = GENERATE(-1, 0, 4);
        CAPTURE(el);
        CHECK_FALSE(equal_ignoring_elements<std::set<int>>({1, 2, 3}, {1, 2, 4}, {el}));
        CHECK_FALSE(equal_ignoring_elements<std::set<int>>({1, 2, 3}, {2, 3}, {el}));
        CHECK_FALSE(equal_ignoring_elements<std::set<int>>({1, 2, 3}, {2, 3}, {el}));
    }

    SECTION("two elements ignored") {
        int el1 = GENERATE(1, 2, 3);
        int el2 = GENERATE(1, 2, 3);
        CAPTURE(el1, el2);

        CHECK(equal_ignoring_elements<std::set<int>>({1, 2, 3}, {1, 2, 3}, {el1, el2}));
        CHECK_FALSE(equal_ignoring_elements<std::set<int>>({1, 2, 3}, {0, 1, 2, 3}, {el1, el2}));
        CHECK_FALSE(equal_ignoring_elements<std::set<int>>({0, 1, 2, 3}, {1, 2, 3}, {el1, el2}));
        CHECK_FALSE(equal_ignoring_elements<std::set<int>>({1, 2, 3, 4}, {1, 2, 3}, {el1, el2}));
        CHECK_FALSE(equal_ignoring_elements<std::set<int>>({1, 2, 3}, {1, 2, 3, 4}, {el1, el2}));
    }

    SECTION("random check") {
        std::set<int> set1{-2, 0, 2, 4, 6};
        std::set<int> set2{0, 1, 2, 4, 5};

        int el1 = GENERATE(-2, 0, 1, 2, 3);
        int el2 = GENERATE(1, 2, 4, 5);
        int el3 = GENERATE(2, 4, 5, 6, 7);
        std::set<int> ignored_els{el1, el2, el3};

        CAPTURE(set1, set2, ignored_els);

        // generate set symmetric difference into v
        std::vector<int> v(set1.size() + set2.size());
        auto it = std::
            set_symmetric_difference(set1.begin(), set1.end(), set2.begin(), set2.end(), v.begin());
        v.resize(it - v.begin());

        // equal_ignoring_elements is the same as checking if symmetric difference of tho sets
        // contains only "ignored" elements
        bool equal = std::all_of(v.begin(), v.end(), [&](int i) { return ignored_els.count(i); });

        CHECK(equal_ignoring_elements(set1, set2, ignored_els) == equal);
    }
}
