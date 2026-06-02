#include "catch/catch.hpp"

#include "cached_options.h"
#include "catacharset.h"
#include "pinyin.h"
#include "string_utils.h"

TEST_CASE( "pinyin_match_handles_basic_search", "[pinyin]" )
{
    const auto text = std::u32string( U"\u7269\u54c1" );
    CHECK( pinyin::pinyin_match( text, U"wupin" ) );
    CHECK_FALSE( pinyin::pinyin_match( text, U"zidan" ) );
}

TEST_CASE( "lcmatch_uses_pinyin_search_when_enabled", "[pinyin]" )
{
    const auto old_use_pinyin_search = use_pinyin_search;
    const auto item_text = utf32_to_utf8( U"\u7269\u54c1" );
    const auto library_text = utf32_to_utf8( U"\u56fe\u4e66\u9986" );

    use_pinyin_search = true;
    CHECK( lcmatch( item_text, "wupin" ) );
    CHECK( lcmatch( library_text, "tushuguan" ) );

    use_pinyin_search = false;
    CHECK_FALSE( lcmatch( item_text, "wupin" ) );

    use_pinyin_search = old_use_pinyin_search;
}
