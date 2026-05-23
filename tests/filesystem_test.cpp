#include "catch/catch.hpp"

#include <algorithm>
#include <sstream>

#include "filesystem.h"
#include "fstream_utils.h"
#include "string_formatter.h"
#include "path_info.h"
#include "path_utils.h"
#include "game.h"
#include "world.h"
#include "cata_utility.h"

static std::string str_to_hex( const std::string &s )
{
    std::stringstream ss;
    for( char c : s ) {
        ss << std::hex << std::uppercase <<
           static_cast<int>( static_cast<unsigned char>( c ) ) << " ";
    }
    return ss.str();
}

static void filesystem_test_group( int serial, const std::string &s1, const std::string &s2,
                                   const std::string &s3 )
{
    CAPTURE( serial );
    CAPTURE( s1 );
    CAPTURE( str_to_hex( s1 ) );
    CAPTURE( s2 );
    CAPTURE( str_to_hex( s2 ) );
    CAPTURE( s3 );
    CAPTURE( str_to_hex( s3 ) );

    // Make sure there's no interference from e.g. uncleaned old runs
    const auto base = g->get_active_world()->info->folder_path() /
                      ( "fs_test_" + get_pid_string() + "_" + std::to_string( serial ) );
    CAPTURE( base );
    REQUIRE( !dir_exist( base ) );
    REQUIRE( assure_dir_exist( base ) );
    REQUIRE( can_write_to_dir( base ) );

    const auto dir1 = base / s1;
    const auto file1_1 = dir1 / ( s2 + ".json" );
    const auto file1_2 = dir1 / ( s3 + ".json" );
    const auto dir2 = dir1 / s3;
    const auto file2_1 = dir2 / ( s2 + ".json" );

    CAPTURE( dir1 );
    CAPTURE( file1_1 );
    CAPTURE( file1_2 );
    CAPTURE( dir2 );
    CAPTURE( file2_1 );

    std::string writebuf = s3;
    std::string writebuf2 = s2;
    std::string readbuf;
    const auto reader = [&readbuf]( std::istream & s ) {
        s >> readbuf;
    };
    const auto writer = [&writebuf]( std::ostream & s ) {
        s << writebuf;
    };
    const auto writer2 = [&writebuf2]( std::ostream & s ) {
        s << writebuf2;
    };

    // Creating/removing directory; not confusing it with a file
    REQUIRE( !dir_exist( dir1 ) );
    REQUIRE( !file_exist( dir1 ) );
    REQUIRE( assure_dir_exist( dir1 ) );
    REQUIRE( dir_exist( dir1 ) );
    REQUIRE( !file_exist( dir1 ) );
    REQUIRE( remove_directory( dir1 ) );
    REQUIRE( !remove_directory( dir1 ) );
    REQUIRE( !dir_exist( dir1 ) );

    // Creating/reading file, renaming it, removing, not confusing with a directory
    REQUIRE( assure_dir_exist( dir1 ) );
    REQUIRE( !file_exist( file1_1 ) );
    REQUIRE( write_to_file( file1_1, writer, nullptr ) );
    REQUIRE( file_exist( file1_1 ) );
    REQUIRE( read_from_file( file1_1, reader ) );
    CHECK( readbuf == writebuf );
    REQUIRE( rename_file( file1_1, file1_2 ) );
    REQUIRE( !rename_file( file1_1, file1_2 ) );
    REQUIRE( file_exist( file1_2 ) );
    REQUIRE( !rename_file( file1_2, dir1 ) );
    REQUIRE( file_exist( file1_2 ) );
    REQUIRE( !dir_exist( file1_2 ) );
    REQUIRE( remove_file( file1_2 ) );
    REQUIRE( !remove_file( file1_2 ) );

    // Copying file
    REQUIRE( write_to_file( file1_1, writer, nullptr ) );
    REQUIRE( ::copy_file( file1_1, file1_2 ) );
    REQUIRE( file_exist( file1_2 ) );
    REQUIRE( ::copy_file( file1_1, file1_2 ) );
    REQUIRE( remove_file( file1_2 ) );
    REQUIRE( write_to_file( file1_2, writer2, nullptr ) );
    REQUIRE( ::copy_file( file1_2, file1_1 ) );
    REQUIRE( remove_file( file1_2 ) );
    REQUIRE( read_from_file( file1_1, reader ) );
    CHECK( readbuf == writebuf2 );
    REQUIRE( assure_dir_exist( dir2 ) );
    REQUIRE( !::copy_file( file1_2, file1_1 ) );
    REQUIRE( !::copy_file( file1_1, dir2 ) );
    REQUIRE( !::copy_file( dir2, file1_2 ) );
    REQUIRE( remove_file( file1_1 ) );
    REQUIRE( remove_directory( dir2 ) );
    REQUIRE( !::copy_file( file1_1, file1_2 ) );

    // Checking if can write to dir
    REQUIRE( can_write_to_dir( dir1 ) );
    REQUIRE( remove_directory( dir1 ) );
    REQUIRE( !can_write_to_dir( dir1 ) );

    // Listing files from dir; paths should stay valid utf-8 strings
    REQUIRE( assure_dir_exist( dir1 ) );
    REQUIRE( write_to_file( file1_1, writer, nullptr ) );
    REQUIRE( write_to_file( file1_2, writer, nullptr ) );
    REQUIRE( assure_dir_exist( dir2 ) );
    REQUIRE( write_to_file( file2_1, writer, nullptr ) );
    {
        auto got = get_files_from_path(
                       ".json",
                       base,
                       true,
                       true
                   );
        std::vector<fs::path> exp = {
            file1_1,
            file1_2,
            file2_1,
        };
        const auto comparator = []( const fs::path & a, const fs::path & b ) {
            // We don't care about lexicographic order here, just the data order
            // NOLINTNEXTLINE(cata-use-localized-sorting)
            return a > b;
        };
        std::sort( exp.begin(), exp.end(), comparator );
        std::sort( got.begin(), got.end(), comparator );
        CHECK( got == exp );
    }

    // Can't delete directory with files
    REQUIRE( !remove_directory( dir1 ) );
    REQUIRE( dir_exist( dir1 ) );

    // TODO: fix why this fails
    // // Unless we use remove_tree
    // REQUIRE( remove_tree( dir1 ) );
    // REQUIRE( !dir_exist( dir1 ) );

    // Removing non-existent tree is not an error
    REQUIRE( remove_tree( dir1 ) );
    REQUIRE( remove_tree( dir2 ) );

    // TODO: fix why this fails
    // Clean up
    // REQUIRE( remove_directory( base ) );
}

// HACK: Need to rework std::u8string
static std::string conv_str( const char8_t *c )
{
    return reinterpret_cast<const char *>( c );
}

static auto path_from_u8( const char8_t *c ) -> fs::path
{
    return fs::path( std::u8string( c ) );
}

TEST_CASE( "filesystem_scan_unicode_paths_without_narrow_conversion",
           "[filesystem][windows][9013]" )
{
    const auto base = g->get_active_world()->info->folder_path() /
                      ( "fs_issue_9013_" + get_pid_string() );
    const auto unicode_dir = base / path_from_u8( u8"💥-issue-9013-파일" );
    const auto marker = unicode_dir / "tile_config.json";
    const auto writer = []( std::ostream & s ) { s << "{}"; };

    REQUIRE( !dir_exist( base ) );
    REQUIRE( assure_dir_exist( base ) );
    REQUIRE( assure_dir_exist( unicode_dir ) );
    REQUIRE( write_to_file( marker, writer, nullptr ) );

    auto files = std::vector<fs::path> {};
    CHECK_NOTHROW( files = get_files_from_path( ".json", base, true, true ) );
    CHECK( std::ranges::find( files, marker ) != files.end() );

    auto dirs = std::vector<fs::path> {};
    CHECK_NOTHROW( dirs = get_directories_with( "tile_config.json", base, true ) );
    CHECK( std::ranges::find( dirs, unicode_dir ) != dirs.end() );
    CHECK( cata_files::path_to_generic_utf8( marker ).find( conv_str( u8"💥-issue-9013-파일" ) ) !=
           std::string::npos );

    REQUIRE( remove_tree( base ) );
}

TEST_CASE( "filesystem_ascii", "[filesystem]" )
{
    // English
    filesystem_test_group( 1, conv_str( u8R"(DwarfFort)" ), conv_str( u8R"(warf)" ),
                           conv_str( u8R"(fFor)" ) );
}

TEST_CASE( "filesystem_utf8", "[filesystem]" )
{
    // Russian
    filesystem_test_group( 2, conv_str( u8R"(МамаМылаРаму)" ), conv_str( u8R"(амаМ)" ),
                           conv_str( u8R"(ылаР)" ) );
    // Chinese
    filesystem_test_group( 3, conv_str( u8R"(你好)" ), conv_str( u8R"(你)" ),
                           conv_str( u8R"(好)" ) );
}

// Older Macs with HFS+ filesystem apply NFD unicode normalization to file names.
// This is done to avoid having composite chars be treated differently from decomposed
// (e.g. "è" vs "e\u0300"), but it also means that paths written with composed chars
// will have them decomposed when read back from the filesystem.
// Hence, the 'mayfail' flag.
TEST_CASE( "filesystem_utf8_comp", "[filesystem][!mayfail]" )
{
    // Hindi (should be unaffected)
    filesystem_test_group( 4, conv_str( u8R"(नमस्ते)" ), conv_str( u8R"(स्ते)" ),
                           conv_str( u8R"(मस्)" ) );
    // French (should not decompose into char + combining char)
    filesystem_test_group( 5, conv_str( u8R"(crème brûlée)" ), conv_str( u8R"(rèm)" ),
                           conv_str( u8R"(rûlé)" ) );
    // Spice it up a bit
    filesystem_test_group( 6, conv_str( u8R"(Dw你ы)" ), conv_str( u8R"(лаस्तेlé)" ),
                           conv_str( u8R"(मябस्or好)" ) );
}

TEST_CASE( "filesystem_utf8_decomp", "[filesystem]" )
{
    // French (should stay decomposed)
    filesystem_test_group( 7, "cre\u0300me bru\u0302le\u0301e", "re\u0300m", "ru\u0302le\u0301" );
}
