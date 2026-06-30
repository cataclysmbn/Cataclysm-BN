#include "catch/catch.hpp"
#include "char_validity_check.h"

TEST_CASE("char_validity_check") {
    CHECK(is_char_allowed(' ') == true);
    CHECK(is_char_allowed('!') == true);
    CHECK(is_char_allowed('\0') == false);
    CHECK(is_char_allowed('\b') == false);
    CHECK(is_char_allowed('\t') == false);
    CHECK(is_char_allowed('\n') == false);
    CHECK(is_char_allowed('\r') == false);
    CHECK(is_char_allowed('\xa0') == false);
    CHECK(is_char_allowed('/') == false);
#if defined(__linux__)
    CHECK(is_char_allowed(':') == true);
    CHECK(is_char_allowed('<') == true);
    CHECK(is_char_allowed('>') == true);
    CHECK(is_char_allowed('"') == true);
    CHECK(is_char_allowed('\\') == true);
    CHECK(is_char_allowed('?') == true);
    CHECK(is_char_allowed('|') == true);
    CHECK(is_char_allowed('*') == true);
#elif defined(MACOSX)
    CHECK(is_char_allowed(':') == false);
    CHECK(is_char_allowed('<') == true);
    CHECK(is_char_allowed('>') == true);
    CHECK(is_char_allowed('"') == true);
    CHECK(is_char_allowed('\\') == true);
    CHECK(is_char_allowed('?') == true);
    CHECK(is_char_allowed('|') == true);
    CHECK(is_char_allowed('*') == true);
#else
    CHECK(is_char_allowed(':') == false);
    CHECK(is_char_allowed('<') == false);
    CHECK(is_char_allowed('>') == false);
    CHECK(is_char_allowed('"') == false);
    CHECK(is_char_allowed('\\') == false);
    CHECK(is_char_allowed('?') == false);
    CHECK(is_char_allowed('|') == false);
    CHECK(is_char_allowed('*') == false);
#endif
}
