#include "catch/catch.hpp"

#include <list>
#include <sstream>
#include <utility>

#include "bodypart.h"
#include "json.h"
#include "cached_options.h"
#include "cata_utility.h"
#include "string_formatter.h"
#include "type_id.h"

template<typename T>
static void test_serialization( const T &val, const std::string &s )
{
    CAPTURE( val );
    {
        INFO( "test_serialization" );
        std::ostringstream os;
        JsonOut jsout( os );
        jsout.write( val );
        CHECK( os.str() == s );
    }
    {
        INFO( "test_deserialization" );
        std::istringstream is( s );
        JsonIn jsin( is );
        T read_val;
        CHECK( jsin.read( read_val ) );
        CHECK( val == read_val );
    }
}

TEST_CASE( "serialize_vector", "[json]" )
{
    std::vector<std::string> c = { "foo", "bar" };
    test_serialization( c, R"(["foo","bar"])" );
}

TEST_CASE( "serialize_map", "[json]" )
{
    std::map<std::string, std::string> s_map = { { "foo", "foo_val" }, { "bar", "bar_val" } };
    test_serialization( s_map, R"({"bar":"bar_val","foo":"foo_val"})" );
    std::map<mtype_id, std::string> string_id_map = { { mtype_id( "foo" ), "foo_val" } };
    test_serialization( string_id_map, R"({"foo":"foo_val"})" );
    std::map<body_part, std::string> enum_map = { { bp_foot_l, "foo_val" } };
    test_serialization( enum_map, R"({"FOOT_L":"foo_val"})" );
}

TEST_CASE( "serialize_pair", "[json]" )
{
    std::pair<std::string, int> p = { "foo", 42 };
    test_serialization( p, R"(["foo",42])" );
}

TEST_CASE( "serialize_sequences", "[json]" )
{
    std::vector<std::string> v = { "foo", "bar" };
    test_serialization( v, R"(["foo","bar"])" );
    std::array<std::string, 2> a = {{ "foo", "bar" }};
    test_serialization( a, R"(["foo","bar"])" );
    std::list<std::string> l = { "foo", "bar" };
    test_serialization( l, R"(["foo","bar"])" );
}

TEST_CASE( "serialize_set", "[json]" )
{
    std::set<std::string> s_set = { "foo", "bar" };
    test_serialization( s_set, R"(["bar","foo"])" );
    std::set<mtype_id> string_id_set = { mtype_id( "foo" ) };
    test_serialization( string_id_set, R"(["foo"])" );
    std::set<body_part> enum_set = { bp_foot_l };
    test_serialization( enum_set, string_format( R"([%d])", static_cast<int>( bp_foot_l ) ) );
}

template<typename Matcher>
static void test_translation_text_style_check( Matcher &&matcher, const std::string &json )
{
    std::istringstream iss( json );
    JsonIn jsin( iss );
    translation trans;
    const std::string dmsg = capture_debugmsg_during( [&]() {
        jsin.read( trans );
    } );
    CHECK_THAT( dmsg, matcher );
}

TEST_CASE( "translation_text_style_check", "[json][translation][.]" )
{
    // this test case is mainly for checking the caret position.
    // the text style check itself is tested in the lit test of clang-tidy.
    restore_on_out_of_scope<error_log_format_t> restore_error_log_format( error_log_format );
    error_log_format = error_log_format_t::human_readable;

    // string, ascii
    test_translation_text_style_check(
        Catch::Equals(
            R"((json-error))" "\n"
            R"(Json error: <unknown source file>:1:5: insufficient spaces at this location.  2 required, but only 1 found.)"
            "\n"
            R"(    Suggested fix: insert " ")" "\n"
            R"(    At the following position (marked with caret))" "\n"
            R"()" "\n"
            R"("foo.)" "\n"
            R"(    ^)" "\n"
            R"(      bar.")" "\n" ),
        R"("foo. bar.")" ); // NOLINT(cata-text-style)
    // string, unicode
    test_translation_text_style_check(
        Catch::Equals(
            R"((json-error))" "\n"
            R"(Json error: <unknown source file>:1:8: insufficient spaces at this location.  2 required, but only 1 found.)"
            "\n"
            R"(    Suggested fix: insert " ")" "\n"
            R"(    At the following position (marked with caret))" "\n"
            R"()" "\n"
            R"("…foo.)" "\n"
            R"(       ^)" "\n"
            R"(         bar.")" "\n" ),
        R"("…foo. bar.")" ); // NOLINT(cata-text-style)
    // string, escape sequence
    test_translation_text_style_check(
        Catch::Equals(
            R"((json-error))" "\n"
            R"(Json error: <unknown source file>:1:11: insufficient spaces at this location.  2 required, but only 1 found.)"
            "\n"
            R"(    Suggested fix: insert " ")" "\n"
            R"(    At the following position (marked with caret))" "\n"
            R"()" "\n"
            R"("\u2026foo.)" "\n"
            R"(          ^)" "\n"
            R"(            bar.")" "\n" ),
        R"("\u2026foo. bar.")" ); // NOLINT(cata-text-style)
    // object, ascii
    test_translation_text_style_check(
        Catch::Equals(
            R"((json-error))" "\n"
            R"(Json error: <unknown source file>:1:13: insufficient spaces at this location.  2 required, but only 1 found.)"
            "\n"
            R"(    Suggested fix: insert " ")" "\n"
            R"(    At the following position (marked with caret))" "\n"
            R"()" "\n"
            R"({"str": "foo.)" "\n"
            R"(            ^)" "\n"
            R"(              bar."})" "\n" ),
        R"({"str": "foo. bar."})" ); // NOLINT(cata-text-style)
    // object, unicode
    test_translation_text_style_check(
        Catch::Equals(
            R"((json-error))" "\n"
            R"(Json error: <unknown source file>:1:16: insufficient spaces at this location.  2 required, but only 1 found.)"
            "\n"
            R"(    Suggested fix: insert " ")" "\n"
            R"(    At the following position (marked with caret))" "\n"
            R"()" "\n"
            R"({"str": "…foo.)" "\n"
            R"(               ^)" "\n"
            R"(                 bar."})" "\n" ),
        R"({"str": "…foo. bar."})" ); // NOLINT(cata-text-style)
    // object, escape sequence
    test_translation_text_style_check(
        Catch::Equals(
            R"((json-error))" "\n"
            R"(Json error: <unknown source file>:1:19: insufficient spaces at this location.  2 required, but only 1 found.)"
            "\n"
            R"(    Suggested fix: insert " ")" "\n"
            R"(    At the following position (marked with caret))" "\n"
            R"()" "\n"
            R"({"str": "\u2026foo.)" "\n"
            R"(                  ^)" "\n"
            R"(                    bar."})" "\n" ),
        R"({"str": "\u2026foo. bar."})" ); // NOLINT(cata-text-style)
}

TEST_CASE( "translation_text_style_check_error_recovery", "[json][translation][.]" )
{
    restore_on_out_of_scope<error_log_format_t> restore_error_log_format( error_log_format );
    error_log_format = error_log_format_t::human_readable;

    SECTION( "string" ) {
        const std::string json =
            R"([)" "\n"
            R"(  "foo. bar.",)" "\n" // NOLINT(cata-text-style)
            R"(  "foobar")" "\n"
            R"(])" "\n";
        std::istringstream iss( json );
        JsonIn jsin( iss );
        jsin.start_array();
        translation trans;
        const std::string dmsg = capture_debugmsg_during( [&]() {
            jsin.read( trans );
        } );
        // check that the correct debug message is shown
        CHECK_THAT(
            dmsg,
            Catch::Equals(
                R"((json-error))" "\n"
                R"(Json error: <unknown source file>:2:7: insufficient spaces at this location.  2 required, but only 1 found.)"
                "\n"
                R"(    Suggested fix: insert " ")" "\n"
                R"(    At the following position (marked with caret))" "\n"
                R"()" "\n"
                R"([)" "\n"
                R"(  "foo.)" "\n"
                R"(      ^)" "\n"
                R"(        bar.",)" "\n"
                R"(  "foobar")" "\n"
                R"(])" "\n" ) );
        // check that the stream is correctly restored to after the first string
        CHECK( jsin.get_string() == "foobar" );
        CHECK( jsin.end_array() );
    }

    SECTION( "object" ) {
        const std::string json =
            R"([)" "\n"
            R"(  { "str": "foo. bar." },)" "\n" // NOLINT(cata-text-style)
            R"(  "foobar")" "\n"
            R"(])" "\n";
        std::istringstream iss( json );
        JsonIn jsin( iss );
        jsin.start_array();
        translation trans;
        const std::string dmsg = capture_debugmsg_during( [&]() {
            jsin.read( trans );
        } );
        // check that the correct debug message is shown
        CHECK_THAT(
            dmsg,
            Catch::Equals(
                R"((json-error))" "\n"
                R"(Json error: <unknown source file>:2:16: insufficient spaces at this location.  2 required, but only 1 found.)"
                "\n"
                R"(    Suggested fix: insert " ")" "\n"
                R"(    At the following position (marked with caret))" "\n"
                R"()" "\n"
                R"([)" "\n"
                R"(  { "str": "foo.)" "\n"
                R"(               ^)" "\n"
                R"(                 bar." },)" "\n"
                R"(  "foobar")" "\n"
                R"(])" "\n" ) );
        // check that the stream is correctly restored to after the first string
        CHECK( jsin.get_string() == "foobar" );
        CHECK( jsin.end_array() );
    }
}

static void test_get_string( const std::string &str, const std::string &json )
{
    CAPTURE( json );
    std::istringstream iss( json );
    JsonIn jsin( iss );
    CHECK( jsin.get_string() == str );
}

template<typename Matcher>
static void test_get_string_throws_matches( Matcher &&matcher, const std::string &json )
{
    CAPTURE( json );
    std::istringstream iss( json );
    JsonIn jsin( iss );
    CHECK_THROWS_MATCHES( jsin.get_string(), JsonError, matcher );
}

template<typename Matcher>
static void test_string_error_throws_matches( Matcher &&matcher, const std::string &json,
        const int offset )
{
    CAPTURE( json );
    CAPTURE( offset );
    std::istringstream iss( json );
    JsonIn jsin( iss );
    CHECK_THROWS_MATCHES( jsin.string_error( "<message>", offset ), JsonError, matcher );
}

TEST_CASE( "jsonin_get_string", "[json]" )
{
    restore_on_out_of_scope<error_log_format_t> restore_error_log_format( error_log_format );
    error_log_format = error_log_format_t::human_readable;

    // read plain text
    test_get_string( "foo", R"("foo")" );
    // ignore starting spaces
    test_get_string( "bar", R"(  "bar")" );
    // read unicode characters
    test_get_string( "……", R"("……")" );
    test_get_string( "……", "\"\u2026\u2026\"" );
    test_get_string( "\xe2\x80\xa6", R"("…")" );
    test_get_string( "\u00A0", R"("\u00A0")" );
    test_get_string( "\u00A0", R"("\u00a0")" );
    // read escaped unicode
    test_get_string( "…", R"("\u2026")" );
    // read utf8 sequence
    test_get_string( "…", "\"\xe2\x80\xa6\"" );
    // read newline
    test_get_string( "a\nb\nc", R"("a\nb\nc")" );
    // read slash
    test_get_string( "foo\\bar", R"("foo\\bar")" );
    // read escaped characters
    // NOLINTNEXTLINE(cata-text-style)
    test_get_string( "\"\\/\b\f\n\r\t\u2581", R"("\"\\\/\b\f\n\r\t\u2581")" );

    // empty json
    test_get_string_throws_matches(
        Catch::Message(
            "Json error: <unknown source file>:EOF: couldn't find end of string, reached EOF." ),
        std::string() );
    // no starting quote
    test_get_string_throws_matches(
        Catch::Message(
            R"(Json error: <unknown source file>:1:1: expected string but got 'a')" "\n"
            R"()" "\n"
            R"(a)" "\n"
            R"(^)" "\n"
            R"( bc)" "\n" ),
        R"(abc)" );
    // no ending quote
    test_get_string_throws_matches(
        Catch::Message(
            "Json error: <unknown source file>:EOF: couldn't find end of string, reached EOF." ),
        R"(")" );
    test_get_string_throws_matches(
        Catch::Message(
            "Json error: <unknown source file>:EOF: couldn't find end of string, reached EOF." ),
        R"("foo)" );
    // incomplete escape sequence and no ending quote
    test_get_string_throws_matches(
        Catch::Message(
            "Json error: <unknown source file>:EOF: couldn't find end of string, reached EOF." ),
        R"("\)" );
    test_get_string_throws_matches(
        Catch::Message(
            "Json error: <unknown source file>:EOF: couldn't find end of string, reached EOF." ),
        R"("\u12)" );
    // incorrect escape sequence
    test_get_string_throws_matches(
        Catch::Message(
            R"(Json error: <unknown source file>:1:3: invalid escape sequence)" "\n"
            R"()" "\n"
            R"("\.)" "\n"
            R"(  ^)" "\n"
            R"(   ")" "\n" ),
        R"("\.")" );
    test_get_string_throws_matches(
        Catch::Message(
            R"(Json error: <unknown source file>:1:7: expected hex digit)" "\n"
            R"()" "\n"
            R"("\uDEFG)" "\n"
            R"(      ^)" "\n"
            R"(       ")" "\n" ),
        R"("\uDEFG")" );
    // not a valid utf8 sequence
    test_get_string_throws_matches(
        Catch::Message(
            R"(Json error: <unknown source file>:1:2: invalid utf8 sequence)" "\n"
            R"()" "\n"
            "\"\x80\n"
            R"( ^)" "\n"
            R"(  ")" "\n" ),
        "\"\x80\"" );
    test_get_string_throws_matches(
        Catch::Message(
            R"(Json error: <unknown source file>:1:4: invalid utf8 sequence)" "\n"
            R"()" "\n"
            "\"\xFC\x80\"\n"
            R"(   ^)" "\n" ),
        "\"\xFC\x80\"" );
    test_get_string_throws_matches(
        Catch::Message(
            R"(Json error: <unknown source file>:1:7: invalid unicode codepoint)" "\n"
            R"()" "\n"
            "\"\xFD\x80\x80\x80\x80\x80\n"
            R"(      ^)" "\n"
            R"(       ")" "\n" ),
        "\"\xFD\x80\x80\x80\x80\x80\"" );
    test_get_string_throws_matches(
        Catch::Message(
            R"(Json error: <unknown source file>:1:7: invalid utf8 sequence)" "\n"
            R"()" "\n"
            "\"\xFC\x80\x80\x80\x80\xC0\n"
            R"(      ^)" "\n"
            R"(       ")" "\n" ),
        "\"\xFC\x80\x80\x80\x80\xC0\"" );
    // end of line
    test_get_string_throws_matches(
        Catch::Message(
            R"(Json error: <unknown source file>:1:3: reached end of line without closing string)" "\n"
            R"()" "\n"
            R"("a)" "\n"
            R"(  ^)" "\n"
            R"(")" "\n" ),
        "\"a\n\"" );
    test_get_string_throws_matches(
        Catch::Message(
            R"(Json error: <unknown source file>:1:3: reached end of line without closing string)" "\n"
            R"()" "\n"
            R"("b)" "\n"
            R"(  ^)" "\n"
            R"(")" "\n" ),
        "\"b\r\"" ); // NOLINT(cata-text-style)

    // test throwing error after the given number of unicode characters
    // ascii
    test_string_error_throws_matches(
        Catch::Message(
            R"(Json error: <unknown source file>:1:1: <message>)" "\n"
            R"()" "\n"
            R"(")" "\n"
            R"(^)" "\n"
            R"( foobar")" "\n" ),
        R"("foobar")", 0 );
    test_string_error_throws_matches(
        Catch::Message(
            R"(Json error: <unknown source file>:1:4: <message>)" "\n"
            R"()" "\n"
            R"("foo)" "\n"
            R"(   ^)" "\n"
            R"(    bar")" "\n" ),
        R"("foobar")", 3 );
    // unicode
    test_string_error_throws_matches(
        Catch::Message(
            R"(Json error: <unknown source file>:1:4: <message>)" "\n"
            R"()" "\n"
            R"("foo)" "\n"
            R"(   ^)" "\n"
            R"(    …bar1")" "\n" ),
        R"("foo…bar1")", 3 );
    test_string_error_throws_matches(
        Catch::Message(
            R"(Json error: <unknown source file>:1:7: <message>)" "\n"
            R"()" "\n"
            R"("foo…)" "\n"
            R"(      ^)" "\n"
            R"(       bar2")" "\n" ),
        R"("foo…bar2")", 4 );
    test_string_error_throws_matches(
        Catch::Message(
            R"(Json error: <unknown source file>:1:8: <message>)" "\n"
            R"()" "\n"
            R"("foo…b)" "\n"
            R"(       ^)" "\n"
            R"(        ar3")" "\n" ),
        R"("foo…bar3")", 5 );
    // escape sequence
    test_string_error_throws_matches(
        Catch::Message(
            R"(Json error: <unknown source file>:1:11: <message>)" "\n"
            R"()" "\n"
            R"("foo\u2026b)" "\n"
            R"(          ^)" "\n"
            R"(           ar")" "\n" ),
        R"("foo\u2026bar")", 5 );
    test_string_error_throws_matches(
        Catch::Message(
            R"(Json error: <unknown source file>:1:7: <message>)" "\n"
            R"()" "\n"
            R"("foo\nb)" "\n"
            R"(      ^)" "\n"
            R"(       ar")" "\n" ),
        R"("foo\nbar")", 5 );
}

TEST_CASE( "serialize_optional", "[json]" )
{
    SECTION( "simple_empty_optional" ) {
        std::optional<int> o;
        test_serialization( o, "null" );
    }
    SECTION( "optional_of_int" ) {
        std::optional<int> o( 7 );
        test_serialization( o, "7" );
    }
    SECTION( "vector_of_empty_optional" ) {
        std::vector<std::optional<int>> v( 3 );
        test_serialization( v, "[null,null,null]" );
    }
    SECTION( "vector_of_optional_of_int" ) {
        std::vector<std::optional<int>> v{ { 1 }, { 2 }, { 3 } };
        test_serialization( v, "[1,2,3]" );
    }
}

// Float comparison woes
template<typename T>
requires std::is_floating_point_v<T>
static bool approximatelyEqual( T a, T b, T eps, const unsigned ulps )
{
    using limits = std::numeric_limits<T>;

    if( std::isnan( a ) || std::isnan( b ) ) {
        return false;
    }

    if( std::signbit( a ) != std::signbit( b ) ) {
        return false;
    }

    if( std::abs( a - b ) <= ulps * limits::denorm_min() ) {
        return true;
    }

    int min_exp, max_exp;
    T min_frac = std::frexp( a, &min_exp );
    T max_frac = std::frexp( b, &max_exp );
    if( min_exp > max_exp ) {
        std::swap( min_frac, max_frac );
        std::swap( min_exp, max_exp );
    }

    const T min_frac2 = std::ldexp( min_frac, min_exp - max_exp );
    const T delta = std::abs( max_frac - min_frac2 );

    return delta <= ulps * eps / 2;
}

template<typename T>
requires std::is_integral_v<T>
constexpr static bool approximatelyEqual( T a, T b, T eps, const unsigned )
{
    if( b > a ) {
        std::swap( a, b );
    }
    return static_cast<T>( a - b ) <= eps;
}

template<typename T>
static void test_number_roundtrip( T val, T eps, int ulps )
{
    T read_val;
    CAPTURE( val );
    {
        INFO( "test_round_trip" );
        std::ostringstream os;
        JsonOut jsout( os );
        jsout.write( val );
        const auto repr = os.str();
        CAPTURE( repr );

        std::istringstream is( repr );
        JsonIn jsin( is );
        CHECK( jsin.read( read_val ) );
        CAPTURE( read_val );
        double delta = read_val - val;
        CAPTURE( delta );
        bool flag = approximatelyEqual( val, read_val, eps, ulps );
        if( !flag ) {
            CAPTURE( val );
            std::istringstream is2( repr );
            JsonIn jsin2( is2 );
            jsin2.read( read_val );
        }
        CHECK( flag );
    }
}

TEST_CASE( "serialize_integers", "[json]" )
{
    SECTION( "bad_values" ) {
        std::vector<std::string> to_check = {"", " ", "1e", "2E", "00", "a" };
        for( const auto &str : to_check ) {
            std::stringstream is { str };
            uint64_t read_val;
            JsonIn jsin( is );
            CHECK_THROWS( jsin.read( read_val, true ) );
            CAPTURE( read_val );
            CAPTURE( str );
        }
    }

    SECTION( "space_padded" ) {
        std::vector<std::string> to_check = {"   123   ", "  456", "789   "  };
        for( const auto &str : to_check ) {
            std::stringstream is { str };
            uint64_t read_val;
            JsonIn jsin( is );
            CHECK( jsin.read( read_val, true ) );
            CAPTURE( read_val );
            CAPTURE( str );
        }
    }

    SECTION( "single_digit" ) {
        for( uint32_t i = -9; i <= 9; i++ ) {
            std::stringstream os;
            JsonOut jsout( os );
            jsout.write( i );
            auto str = os.str();
            CAPTURE( i );
            CAPTURE( str );

            std::stringstream is { str };
            uint64_t read_val;
            JsonIn jsin( is );
            CHECK( jsin.read( read_val ) );
            CAPTURE( read_val );

            CHECK( read_val == i );
        }
    }

    SECTION( "exponent" ) {
        for( uint32_t i = -9; i <= 9; i++ ) {
            std::string str = string_format( "1e%d", i );
            std::stringstream is { str };
            uint64_t read_val;
            JsonIn jsin( is );
            CHECK( jsin.read( read_val, true ) );
            uint64_t expectedval = std::pow( 10, i );
            CHECK( read_val == expectedval );
            CAPTURE( read_val );
            CAPTURE( str );
        }
    }

    SECTION( "random_unsigned" ) {
        std::random_device rd;
        std::mt19937 gen( rd() );
        std::uniform_int_distribution<uint64_t> distrib( std::numeric_limits<uint64_t>::min(),
                std::numeric_limits<uint64_t>::max() );

        for( int i = 0; i < 100000; i++ ) {
            test_number_roundtrip<uint64_t>( distrib( gen ), 0, 0 );
        }
    }

    SECTION( "random_signed" ) {
        std::random_device rd;
        std::mt19937 gen( rd() );
        std::uniform_int_distribution<int64_t> distrib( std::numeric_limits<int64_t>::min(),
                std::numeric_limits<int64_t>::max() );

        for( int i = 0; i < 100000; i++ ) {
            test_number_roundtrip<int64_t>( distrib( gen ), 0, 0 );
        }
    }
}

TEST_CASE( "truncate_doubles", "[json]" )
{
    std::vector<std::pair<std::string, ptrdiff_t>> test {
        std::make_pair( "123456789.", 123456789 ),
        std::make_pair( "12345678.9", 12345678 ),
        std::make_pair( "1234567.89", 1234567 ),
        std::make_pair( "123456.789", 123456 ),
        std::make_pair( "12345.6789", 12345 ),
        std::make_pair( "1234.56789", 1234 ),
        std::make_pair( "123.456789", 123 ),
        std::make_pair( "12.3456789", 12 ),
        std::make_pair( "1.23456789", 1 ),
        std::make_pair( "0.123456789", 0 ),

        std::make_pair( "123456789e-0", 123456789 ),
        std::make_pair( "123456789e-1", 12345678 ),
        std::make_pair( "123456789e-2", 1234567 ),
        std::make_pair( "123456789e-3", 123456 ),
        std::make_pair( "123456789e-4", 12345 ),
        std::make_pair( "123456789e-5", 1234 ),
        std::make_pair( "123456789e-6", 123 ),
        std::make_pair( "123456789e-7", 12 ),
        std::make_pair( "123456789e-8", 1 ),
        std::make_pair( "123456789e-9", 0 ),

        std::make_pair( "0.123456789e9", 123456789 ),
        std::make_pair( "0.123456789e8", 12345678 ),
        std::make_pair( "0.123456789e7", 1234567 ),
        std::make_pair( "0.123456789e6", 123456 ),
        std::make_pair( "0.123456789e5", 12345 ),
        std::make_pair( "0.123456789e4", 1234 ),
        std::make_pair( "0.123456789e3", 123 ),
        std::make_pair( "0.123456789e2", 12 ),
        std::make_pair( "0.123456789e1", 1 ),
        std::make_pair( "0.123456789e0", 0 ),
    };

    for( auto& [k, v] : test ) {
        std::stringstream is { k };
        ptrdiff_t read_val;
        JsonIn jsin( is );
        CHECK( jsin.read( read_val ) );
        CAPTURE( read_val );

        CHECK( read_val == v );
    }
}

TEST_CASE( "serialize_doubles", "[json]" )
{
    // 64-bit IEEE 754 float -> text -> float roundtrip is only guaranteed for 15 decimal digits
    constexpr double eps = 1E-15;

    SECTION( "validate_checker" ) {
        for( int i = 0; i < std::numeric_limits<double>::max_exponent10; i++ ) {
            double a = std::pow( 10, i );
            double b = std::pow( 10, i - 1 );
            bool flag = approximatelyEqual( a, b, eps, 4 );
            double delta = fabs( b - a );
            CAPTURE( delta );
            CHECK_FALSE( flag );
        }
    }

    SECTION( "bad_values" ) {
        std::vector<std::string> to_check = {"", " ", "1e", "2E", "00", "a" };
        for( const auto &str : to_check ) {
            std::stringstream is { str };
            double read_val;
            JsonIn jsin( is );
            CHECK_THROWS( jsin.read( read_val, true ) );
            CAPTURE( read_val );
            CAPTURE( str );
        }
    }

    SECTION( "space_padded" ) {
        std::vector<std::string> to_check = {"   123   ", "  456", "789   "  };
        for( const auto &str : to_check ) {
            std::stringstream is { str };
            double read_val;
            JsonIn jsin( is );
            CHECK( jsin.read( read_val, true ) );
            CAPTURE( read_val );
            CAPTURE( str );
        }
    }

    SECTION( "single_digit" ) {
        for( int i = 0; i <= 9; i++ ) {
            std::stringstream os;
            JsonOut jsout( os );
            jsout.write( i );
            auto str = os.str();
            CAPTURE( i );
            CAPTURE( str );

            std::stringstream is { str };
            double read_val;
            JsonIn jsin( is );
            CHECK( jsin.read( read_val ) );
            CAPTURE( read_val );

            CHECK( read_val == i );
        }
    }

    SECTION( "test_exponents" ) {
        for( int i = -std::numeric_limits<double>::max_exponent10;
             i <= std::numeric_limits<double>::max_exponent10; i++ ) {
            test_number_roundtrip( std::pow( 10, i ), eps, 4 );
        }
    }

    SECTION( "random_doubles_large" ) {
        std::random_device rd;
        std::mt19937 gen( rd() );
        std::uniform_real_distribution<> distrib( std::numeric_limits<double>::min(),
                std::numeric_limits<double>::max() );

        for( int i = 0; i < 100000; i++ ) {
            test_number_roundtrip( distrib( gen ), eps, 4 );
        }
    }
    SECTION( "random_doubles_small" ) {
        std::random_device rd;
        std::mt19937 gen( rd() );
        std::uniform_int_distribution<uint64_t> distrib( 0, 999999999999999UL );

        for( int i = 0; i < 100000; i++ ) {
            // Generate a synthetic double with 15 decimal digits
            // so it can survive the round trip from text and back
            std::string str = string_format( "0.%015d", distrib( gen ) );
            const double dbl = std::atof( str.c_str() );

            test_number_roundtrip( dbl, eps, 4 );
        }
    }
}