#include "catch/catch.hpp"

#include "cata_libintl.h"
#include "filesystem.h"
#include "fstream_utils.h"
#include "rng.h"
#include "string_formatter.h"

#include <algorithm>
#include <array>
#include <functional>
#include <iostream>
#include <iterator>
#include <vector>
#include <sstream>

using namespace cata_libintl;

struct test_case_data {
    int serial;
    std::string input;
    std::string expected;
};

static const std::vector<test_case_data> tests_plural_form_rules = {{
        {
            1, // a valid expression
            "n%2",
            "(n%2)",
        },
        {
            2, // same as previous, but with brackets and spaces
            " ( n % 2 ) ",
            "(n%2)",
        },
        {
            3, // same as previous, but with multiple brackets
            " (( ( ( n % 2 )   )) )",
            "(n%2)",
        },
        {
            4, // ternary op
            "n?0:1",
            "(n?0:1)",
        },
        {
            5, // two ternary ops
            "n?1?2:3:4",
            "(n?(1?2:3):4)",
        },
        {
            6, // same op
            "1 && 2 && 3 && 4",
            "(1&&(2&&(3&&4)))",
        },
        {
            7, // binary op priority
            "n%10==1 && n%100!=11",
            "(((n%10)==1)&&((n%100)!=11))",
        },
        {
            8, // ternary op priority
            "n==1?n%2:n%3",
            "((n==1)?(n%2):(n%3))",
        },
        {
            9, // maximum integer
            // NOLINTNEXTLINE(cata-text-style): plf expression, not text
            "n == 4294967295 ? 1 : 0",
            "((n==4294967295)?1:0)",
        },
        {
            10, // Japanese, Korean
            "0",
            "0",
        },
        {
            11, // Germanic languages (including English)
            "n!=1",
            "(n!=1)",
        },
        {
            12, // French, Brazilian
            "n>1",
            "(n>1)",
        },
        {
            13, // Latvian
            // NOLINTNEXTLINE(cata-text-style): plf expression, not text
            "n%10==1 && n%100!=11 ? 0 : n != 0 ? 1 : 2",
            "((((n%10)==1)&&((n%100)!=11))?0:((n!=0)?1:2))",
        },
        {
            14, // Polish
            // NOLINTNEXTLINE(cata-text-style): plf expression, not text
            "n==1 ? 0 : n%10>=2 && n%10<=4 && (n%100<10 || n%100>=20) ? 1 : 2",
            "((n==1)?0:((((n%10)>=2)&&(((n%10)<=4)&&(((n%100)<10)||((n%100)>=20))))?1:2))",
        },
        {
            15, // Russian, Lithuanian, Ukrainian, Belarusian, Serbian, Croatian
            // NOLINTNEXTLINE(cata-text-style): plf expression, not text
            "n%10==1 && n%100!=11 ? 0 : n%10>=2 && n%10<=4 && (n%100<10 || n%100>=20) ? 1 : 2",
            "((((n%10)==1)&&((n%100)!=11))?0:((((n%10)>=2)&&(((n%10)<=4)&&(((n%100)<10)||((n%100)>=20))))?1:2))",
        },
        {
            16, // Slovenian
            // NOLINTNEXTLINE(cata-text-style): plf expression, not text
            "n%100==1 ? 0 : n%100==2 ? 1 : n%100==3 || n%100==4 ? 2 : 3",
            "(((n%100)==1)?0:(((n%100)==2)?1:((((n%100)==3)||((n%100)==4))?2:3)))",
        },
    }
};

static const std::vector<test_case_data> tests_plural_form_rules_fail = {{
        {
            0, // missing right-hand expression
            "n%",
            "expected expression at pos 2",
        },
        {
            1, // missing left-hand expression
            "%2",
            "expected expression at pos 0",
        },
        {
            2, // missing op
            "n2",
            "unexpected token at pos 1",
        },
        {
            3, // missing closing bracket
            " ( n % 2 ",
            "expected closing bracket at pos 9",
        },
        {
            4, // stray closing bracket
            "  n % 2     )  ",
            "unexpected token at pos 12",
        },
        {
            5, // empty expression
            "  ",
            "expected expression at pos 2",
        },
        {
            6, // missing op
            " ( n % 2 ) 2 % n",
            "unexpected token at pos 11",
        },
        {
            7, // missing right-hand expression
            " ( n % 2 ) % % 4",
            "expected expression at pos 13",
        },
        {
            8, // missing left-hand expression
            "%% 3",
            "expected expression at pos 0",
        },
        {
            9, // unknown op
            "n % -3",
            "unexpected character '-' at pos 4",
        },
        {
            10, // unknown op
            "n * 3",
            "unexpected character '*' at pos 2",
        },
        {
            11, // extra closing bracket
            "(((((n % 3))))))",
            "unexpected token at pos 15",
        },
        {
            12, // missing op
            "n % 2 3",
            "unexpected token at pos 6",
        },
        {
            13, // integer overflow
            // NOLINTNEXTLINE(cata-text-style): plf expression, not text
            "n == 4294967296 ? 1 : 0",
            "invalid number '4294967296' at pos 5",
        },
        {
            14, // missing ternary delimiter
            // NOLINTNEXTLINE(cata-text-style): plf expression, not text
            "n ? 2 3",
            "expected ternary delimiter at pos 6",
        },
    }
};

// MO Plural forms expression parsing
TEST_CASE( "mo_plurals_parsing", "[libintl][i18n]" )
{
    for( const auto &it : tests_plural_form_rules ) {
        CAPTURE( it.serial );
        PlfNodePtr ptr = parse_plural_rules( it.input );
        REQUIRE( ptr );
        CHECK( ptr->debug_dump() == it.expected );
    }
    for( const auto &it : tests_plural_form_rules_fail ) {
        CAPTURE( it.serial );
        try {
            PlfNodePtr ptr = parse_plural_rules( it.input );
            CAPTURE( ptr->debug_dump() );
            FAIL_CHECK();
        } catch( const std::runtime_error &err ) {
            CHECK( err.what() == it.expected );
        }
    }
}

static std::vector<test_case_data> plf_calc_test_cases {{
        {
            0, // Japanese
            "0",
            "0000000000"
            "0000000000"
            "0000000000"
            "0000000000"
            "0000000000"
            "0000000000"
            "0000000000"
            "0000000000"
            "0000000000"
            "0000000000"
            "0000000000"
            "0000000000"
        },
        {
            1, // English
            "n!=1",
            "1011111111"
            "1111111111"
            "1111111111"
            "1111111111"
            "1111111111"
            "1111111111"
            "1111111111"
            "1111111111"
            "1111111111"
            "1111111111"
            "1111111111"
            "1111111111"
            "1111111111"
        },
        {
            2, // French
            "n>1",
            "0011111111"
            "1111111111"
            "1111111111"
            "1111111111"
            "1111111111"
            "1111111111"
            "1111111111"
            "1111111111"
            "1111111111"
            "1111111111"
            "1111111111"
            "1111111111"
            "1111111111"
        },
        {
            3, // Slovenian
            // NOLINTNEXTLINE(cata-text-style): plf expression, not text
            "(n%100==1 ? 0 : n%100==2 ? 1 : n%100==3 || n%100==4 ? 2 : 3)",
            "3012233333"
            "3333333333"
            "3333333333"
            "3333333333"
            "3333333333"
            "3333333333"
            "3333333333"
            "3333333333"
            "3333333333"
            "3333333333"
            "3012233333"
            "3333333333"
            "3333333333"
        },
        {
            4, // Russian
            // NOLINTNEXTLINE(cata-text-style): plf expression, not text
            "n%10==1 && n%100!=11 ? 0 : n%10>=2 && n%10<=4 && (n%100<10 || n%100>=20) ? 1 : 2",
            "2011122222"
            "2222222222"
            "2011122222"
            "2011122222"
            "2011122222"
            "2011122222"
            "2011122222"
            "2011122222"
            "2011122222"
            "2011122222"
            "2222222222"
            "2011122222"
            "2011122222"
        },
    }
};

static void do_plf_calc_test( const test_case_data &test )
{
    constexpr size_t PLF_PERIOD = 100;

    PlfNodePtr expr = parse_plural_rules( test.input );
    assert( test.expected.size() >= PLF_PERIOD );
    std::vector<size_t> expected;
    for( char c : test.expected ) {
        expected.push_back( c - '0' );
    }

    SECTION( "Produces expected values for small numbers" ) {
        for( size_t i = 0; i < expected.size(); i++ ) {
            size_t x = i;
            size_t exp = expected[i];

            CAPTURE( x );
            size_t res = expr->eval( x );
            CHECK( exp == res );
        }
    };

    SECTION( "Produces expected values for big numbers" ) {
        constexpr size_t CHECK_MAX = 1'234'567;

        for( size_t i = expected.size(); i < CHECK_MAX; i++ ) {
            size_t x = i;
            size_t exp = expected[i % PLF_PERIOD];

            CAPTURE( x );
            size_t res = expr->eval( x );
            if( exp != res ) {
                REQUIRE( exp == res );
            }
        }
    };

    SECTION( "Produces expected values for any numbers" ) {
        constexpr size_t CHECK_TOTAL = 1'000'000;

        for( size_t i = 0; i < CHECK_TOTAL; i++ ) {
            size_t x;
            if( i == 0 ) {
                x = std::numeric_limits<size_t>::max();
            } else {
                static std::uniform_int_distribution<size_t> rng_uint_dist;
                x = rng_uint_dist( rng_get_engine() );
            }
            size_t exp = expected[x % PLF_PERIOD];

            CAPTURE( x );
            size_t res = expr->eval( x );
            if( exp != res ) {
                REQUIRE( exp == res );
            }
        }
    };
}

// MO Plural forms calculation
TEST_CASE( "mo_plurals_calculation", "[libintl][i18n][.]" )
{
    for( const auto &test : plf_calc_test_cases ) {
        CAPTURE( test.serial );
        do_plf_calc_test( test );
    }
}

// For some languages Transifex defines additional plural form for fractions.
// Neither GNU gettext nor Cata's implementation support fractional numbers, so
// the extra plural form goes unused. Relevant issue:
// https://github.com/cataclysmbn/Cataclysm-BN/issues/432
//
// This test reaffirms the assumption that both Transifex's and GNU's plf expressions
// produce same values for integer numbers.
TEST_CASE( "gnu_transifex_rules_equal", "[libintl][i18n][.]" )
{
    constexpr size_t CHECK_TOTAL = 1'000'000;

    struct rules {
        int serial;
        std::string gnu;
        std::string tfx;
    };

    static std::vector<rules> rules_to_compare = {{
            {
                0, // Polish
                // NOLINTNEXTLINE(cata-text-style): plf expression, not text
                "(n==1 ? 0 : n%10>=2 && n%10<=4 && (n%100<10 || n%100>=20) ? 1 : 2)",
                // NOLINTNEXTLINE(cata-text-style): plf expression, not text
                "(n==1 ? 0 : (n%10>=2 && n%10<=4) && (n%100<12 || n%100>14) ? 1 : n!=1"
                // NOLINTNEXTLINE(cata-text-style): plf expression, not text
                "&& (n%10>=0 && n%10<=1) || (n%10>=5 && n%10<=9) || (n%100>=12 && n%100<=14) ? 2 : 3)"
            },
            {
                1, // Russian
                // NOLINTNEXTLINE(cata-text-style): plf expression, not text
                "(n%10==1 && n%100!=11 ? 0 : n%10>=2 && n%10<=4 && (n%100<10 || n%100>=20) ? 1 : 2)",
                // NOLINTNEXTLINE(cata-text-style): plf expression, not text
                "(n%10==1 && n%100!=11 ? 0 : n%10>=2 && n%10<=4 && (n%100<12 || n%100>14) ? 1 :"
                " n%10==0 || (n%10>=5 && n%10<=9) || (n%100>=11 && n%100<=14)? 2 : 3)"
            },
            {
                2, // Ukrainian
                // NOLINTNEXTLINE(cata-text-style): plf expression, not text
                "(n%10==1 && n%100!=11 ? 0 : n%10>=2 && n%10<=4 && (n%100<10 || n%100>=20) ? 1 : 2)",
                "(n % 1 == 0 && n % 10 == 1 && n % 100 != "
                // NOLINTNEXTLINE(cata-text-style): plf expression, not text
                "11 ? 0 : n % 1 == 0 && n % 10 >= 2 && n % 10 <= 4 && (n % 100 < 12 || n % "
                // NOLINTNEXTLINE(cata-text-style): plf expression, not text
                "100 > 14) ? 1 : n % 1 == 0 && (n % 10 ==0 || (n % 10 >=5 && n % 10 <=9) || "
                // NOLINTNEXTLINE(cata-text-style): plf expression, not text
                "(n % 100 >=11 && n % 100 <=14 )) ? 2: 3)"
            }
        }
    };

    for( const rules &it : rules_to_compare ) {
        CAPTURE( it.serial );
        PlfNodePtr expr_gnu = parse_plural_rules( it.gnu );
        PlfNodePtr expr_tfx = parse_plural_rules( it.tfx );

        for( size_t i = 0; i < CHECK_TOTAL; i++ ) {
            static std::uniform_int_distribution<size_t> rng_uint_dist;
            size_t x = rng_uint_dist( rng_get_engine() );

            CAPTURE( x );

            size_t res_gnu = expr_gnu->eval( x );
            size_t res_tfx = expr_tfx->eval( x );
            if( res_gnu != res_tfx ) {
                REQUIRE( res_gnu == res_tfx );
            }
        }
    }
}

static void tst( int serial, const char *s, const char *expected )
{
    CAPTURE( serial );
    REQUIRE( s );
    CHECK( std::string( s ) == expected );
}

static void test_get_strings( const trans_library &lib )
{
    // _()
    tst( 1, lib.get( "Cataclysm" ), "Катаклизм" );

    // pgettext()
    tst( 11, lib.get_ctx( "noun", "Test" ), "Тест" );
    tst( 12, lib.get_ctx( "verb", "Test" ), "Тестировать" );

    // vgettext()
    tst( 21, lib.get_pl( "%d item", "%d items", 1 ), "%d предмет" );
    tst( 22, lib.get_pl( "%d item", "%d items", 2 ), "%d предмета" );
    tst( 23, lib.get_pl( "%d item", "%d items", 5 ), "%d предметов" );

    // vpgettext()
    tst( 31, lib.get_ctx_pl( "source of water", "%d spring", "%d springs", 1 ), "%d родник" );
    tst( 32, lib.get_ctx_pl( "source of water", "%d spring", "%d springs", 2 ), "%d родника" );
    tst( 33, lib.get_ctx_pl( "source of water", "%d spring", "%d springs", 5 ), "%d родников" );
    tst( 34, lib.get_ctx_pl( "metal coil", "%d spring", "%d springs", 1 ), "%d пружина" );
    tst( 35, lib.get_ctx_pl( "metal coil", "%d spring", "%d springs", 2 ), "%d пружины" );
    tst( 36, lib.get_ctx_pl( "metal coil", "%d spring", "%d springs", 5 ), "%d пружин" );

    // Plural form does not affect string lookup
    tst( 41, lib.get( "%d item" ), "%d предмет" );
    tst( 42, lib.get_pl( "%d item", "%d itemses", 5 ), "%d предметов" );
    tst( 43, lib.get_ctx( "source of water", "%d spring" ), "%d родник" );
    tst( 44, lib.get_ctx( "metal coil", "%d spring" ), "%d пружина" );
    tst( 45, lib.get_ctx_pl( "metal coil", "%d spring", "%d of 'em!", 5 ), "%d пружин" );
    tst( 46, lib.get_ctx_pl( "source of water", "%d spring", "%d of 'em!", 5 ), "%d родников" );

    // Metadata entry should not be revealed
    tst( 51, lib.get( "" ), "" );

    // If translation is missing, original string is returned. Plural forms follow Germanic rules.
    tst( 61, lib.get( "Catsplosion" ), "Catsplosion" );
    tst( 62, lib.get_ctx( "missing", "Test" ), "Test" );
    tst( 63, lib.get_pl( "%d tool", "%d tools", 1 ), "%d tool" );
    tst( 64, lib.get_pl( "%d tool", "%d tools", 2 ), "%d tools" );
    tst( 65, lib.get_pl( "%d tool", "%d tools", 5 ), "%d tools" );
    tst( 66, lib.get_ctx_pl( "time of year", "%d spring", "%d springs", 1 ), "%d spring" );
    tst( 67, lib.get_ctx_pl( "time of year", "%d spring", "%d springs", 2 ), "%d springs" );
    tst( 68, lib.get_ctx_pl( "time of year", "%d spring", "%d springs", 5 ), "%d springs" );
}

static const std::string mo_dir = "tests/data/cata_libintl/";

// Load single MO and get strings
TEST_CASE( "single_mo_strings", "[libintl][i18n]" )
{
    SECTION( "Little endian file" ) {
        std::vector<trans_catalogue> list;
        list.push_back( trans_catalogue::load_from_file( mo_dir + "single_ru_little_endian.mo" ) );
        trans_library lib = trans_library::create( std::move( list ) );

        test_get_strings( lib );
    }
    SECTION( "Big endian file" ) {
        std::vector<trans_catalogue> list;
        list.push_back( trans_catalogue::load_from_file( mo_dir + "single_ru_big_endian.mo" ) );
        trans_library lib = trans_library::create( std::move( list ) );

        test_get_strings( lib );
    }
}

// Load multiple MO and get strings
TEST_CASE( "multiple_mo_strings", "[libintl][i18n]" )
{
    std::vector<trans_catalogue> list;
    list.push_back( trans_catalogue::load_from_file( mo_dir + "multi_1_ru.mo" ) );
    list.push_back( trans_catalogue::load_from_file( mo_dir + "multi_2_ru.mo" ) );
    list.push_back( trans_catalogue::load_from_file( mo_dir + "multi_3_ru.mo" ) );
    trans_library lib = trans_library::create( std::move( list ) );

    test_get_strings( lib );
}

// Load multiple MO for different languages and get plural strings
TEST_CASE( "multiple_mo_different_languages", "[libintl][i18n]" )
{
    std::vector<trans_catalogue> list;
    list.push_back( trans_catalogue::load_from_file( mo_dir + "multilang_ru.mo" ) );
    list.push_back( trans_catalogue::load_from_file( mo_dir + "multilang_fr.mo" ) );
    trans_library lib = trans_library::create( std::move( list ) );

    // Ru
    tst( 11, lib.get_pl( "%d item", "%d items", 0 ), "%d предметов" );
    tst( 12, lib.get_pl( "%d item", "%d items", 1 ), "%d предмет" );
    tst( 13, lib.get_pl( "%d item", "%d items", 2 ), "%d предмета" );

    // Fr
    tst( 21, lib.get_pl( "%d monster", "%d monsters", 0 ), "%d monstre" );
    tst( 22, lib.get_pl( "%d monster", "%d monsters", 1 ), "%d monstre" );
    tst( 23, lib.get_pl( "%d monster", "%d monsters", 2 ), "%d monstres" );

    // En (original strings)
    tst( 31, lib.get_pl( "%d actor", "%d actors", 0 ), "%d actors" );
    tst( 32, lib.get_pl( "%d actor", "%d actors", 1 ), "%d actor" );
    tst( 33, lib.get_pl( "%d actor", "%d actors", 2 ), "%d actors" );
}

static const std::vector<test_case_data> tests_mo_loading_failures = {{
        {
            0, // file not found
            "non-existent.mo",
            "failed to open file",
        },
        {
            1, // not a MO file (magic number mismatch)
            "single.pot",
            "not a MO file",
        },
        {
            2, // not a MO file (too small to have magic number)
            "not_actually_mo.mo",
            "not a MO file",
        },
        {
            3, // wrong charset (only UTF-8 is supported)
            "wrong_charset_ru.mo",
            "unrecognized value in Content-Type header (wrong charset?). Expected \"Content-Type: text/plain; charset=UTF-8\"",
        },
        {
            4, // one of the strings extends beyond end of file
            "single_ru_string_ignores_eof.mo",
            "string_descr at offs 0x84: extends beyond EOF (len:0x16 offs:0x35f fsize:0x375)",
        },
        {
            5, // one of the strings is missing null terminator
            "single_ru_missing_nullterm.mo",
            "string_descr at offs 0x84: missing null terminator",
        },
    }
};

// MO loading failure
TEST_CASE( "mo_loading_failure", "[libintl][i18n]" )
{
    for( const auto &it : tests_mo_loading_failures ) {
        CAPTURE( it.serial );
        try {
            trans_catalogue::load_from_file( mo_dir + it.input );
            FAIL_CHECK();
        } catch( const std::runtime_error &err ) {
            CHECK( err.what() == it.expected );
        }
    }
}

// Load all MO files for the base game to check for loading failures
TEST_CASE( "load_all_base_game_mos", "[libintl][i18n]" )
{
    std::vector<std::string> mo_files = get_files_from_path( ".mo", "lang/mo", true, true );

    if( mo_files.empty() ) {
        WARN( "Skipping (no MO files found)" );
        return;
    }

    for( const std::string &file : mo_files ) {
        try {
            std::vector<trans_catalogue> list;
            list.push_back( trans_catalogue::load_from_file( file ) );
            trans_library lib = trans_library::create( std::move( list ) );
        } catch( const std::runtime_error &err ) {
            CAPTURE( err.what() );
            FAIL_CHECK();
        }
    }
}

static std::string get_bench_file()
{
    // Using Russian here because it's the largest one
    std::string path = "lang/mo/ru_RU/LC_MESSAGES/cataclysm-bn.mo";
    if( !file_exist( path ) ) {
        WARN( "Skipping (file not found)" );
        return "";
    }
    std::stringstream buffer;
    buffer << cata_ifstream().mode( cata_ios_mode::binary ).open( path )->rdbuf();
    return buffer.str();
}

// Measure how long it takes to find all strings in a MO file
TEST_CASE( "bench_get_translated_string", "[libintl][i18n][benchmark][.]" )
{
    std::string data = get_bench_file();
    if( data.empty() ) {
        return;
    }

    std::vector<trans_catalogue> list;
    list.push_back( trans_catalogue::load_from_memory( data ) );

    size_t num = list.back().get_num_strings();
    std::vector<std::string> originals;
    for( size_t i = 0; i < num; i++ ) {
        originals.push_back( list.back().get_nth_orig_string( i ) );
    }
    trans_library lib = trans_library::create( std::move( list ) );

    std::shuffle( originals.begin(), originals.end(), rng_get_engine() );

    cata_printf( "N strings: %d\n", originals.size() );
    BENCHMARK( "get_all_strings" ) {
        for( const std::string &s : originals ) {
            volatile const char *res = lib.get( s.c_str() );
            ( void )res;
        }
    };
}

// Measure how long it takes to parse single MO file
TEST_CASE( "bench_parse_mo", "[libintl][i18n][benchmark][.]" )
{
    std::string data = get_bench_file();
    if( data.empty() ) {
        return;
    }

    int n_strings = trans_catalogue::load_from_memory( data ).get_num_strings();

    cata_printf( "File size: %d bytes\n", data.size() );
    cata_printf( "N strings: %d\n", n_strings );
    BENCHMARK( "parse MO" ) {
        volatile trans_catalogue res = trans_catalogue::load_from_memory( data );
        ( void )res;
    };
}

// Measure how long it takes to parse single MO file + assemble library
TEST_CASE( "bench_asssemble_trans_lib", "[libintl][i18n][benchmark][.]" )
{
    std::string data = get_bench_file();
    if( data.empty() ) {
        return;
    }

    int n_strings = trans_catalogue::load_from_memory( data ).get_num_strings();

    cata_printf( "File size: %d bytes\n", data.size() );
    cata_printf( "N strings: %d\n", n_strings );
    BENCHMARK( "parse MO + assemble lib" ) {
        // We must parse MO from scratch on each iteration because
        // trans_library takes ownership of all trans_catalogues,
        // and trans_catalogue cannot be copied
        std::vector<trans_catalogue> list;
        list.push_back( trans_catalogue::load_from_memory( data ) );
        volatile trans_library res = trans_library::create( std::move( list ) );
        ( void ) res;
    };
}
