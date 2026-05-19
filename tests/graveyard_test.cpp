#include "catch/catch.hpp"

#include <string>
#include <vector>

#include "avatar.h"
#include "catacharset.h"
#include "filesystem.h"
#include "fstream_utils.h"
#include "game.h"
#include "path_info.h"
#include "world.h"

static void write_dummy_file( const std::string &path )
{
    const auto writer = []( std::ostream & out ) {
        out << "test";
    };
    REQUIRE( write_to_file( path, writer, nullptr ) );
}

// Asserts dest is a direct child of graveyard_dir before recursively deleting it,
// preventing accidental deletion of the graveyard root or unrelated paths.
static void remove_graveyard_subdir( const std::string &graveyard_dir,
                                     const std::string &dest_dir )
{
    REQUIRE( !dest_dir.empty() );
    REQUIRE( dest_dir != graveyard_dir );
    REQUIRE( dest_dir.rfind( graveyard_dir, 0 ) == 0 );
    remove_tree( dest_dir );
}

TEST_CASE( "graveyarddir_returns_user_dir_graveyard", "[graveyard]" )
{
    const std::string expected = PATH_INFO::user_dir() + "graveyard/";
    CHECK( PATH_INFO::graveyarddir() == expected );
}

TEST_CASE( "move_save_to_graveyard_moves_character_files", "[graveyard]" )
{
    const std::string save_dir = g->get_active_world()->info->folder_path() + "/";
    const std::string prefix   = base64_encode( g->u.get_save_id() ) + ".";
    const std::string graveyard_dir = PATH_INFO::graveyarddir();
    const std::string dirname  = "test_char_" + get_pid_string();
    const std::string dest_dir = graveyard_dir + dirname + "/";

    // Create dummy save files with the character's prefix
    const std::string file1 = save_dir + prefix + "sav";
    const std::string file2 = save_dir + prefix + "map";
    write_dummy_file( file1 );
    write_dummy_file( file2 );

    // Ensure graveyard destination does not pre-exist
    remove_graveyard_subdir( graveyard_dir, dest_dir );
    REQUIRE( !dir_exist( dest_dir ) );

    g->move_save_to_graveyard( dirname );

    // Files should be in the graveyard subdirectory
    CHECK( file_exist( dest_dir + prefix + "sav" ) );
    CHECK( file_exist( dest_dir + prefix + "map" ) );

    // Source files should no longer exist in save dir
    CHECK( !file_exist( file1 ) );
    CHECK( !file_exist( file2 ) );

    // Cleanup
    remove_graveyard_subdir( graveyard_dir, dest_dir );
}

TEST_CASE( "move_save_to_graveyard_ignores_unrelated_files", "[graveyard]" )
{
    const std::string save_dir  = g->get_active_world()->info->folder_path() + "/";
    const std::string prefix    = base64_encode( g->u.get_save_id() ) + ".";
    const std::string graveyard_dir = PATH_INFO::graveyarddir();
    const std::string dirname   = "test_unrelated_" + get_pid_string();
    const std::string dest_dir  = graveyard_dir + dirname + "/";

    // A character file must be present so save_files is non-empty
    const std::string char_file = save_dir + prefix + "unrelated_test_char";
    write_dummy_file( char_file );

    // An unrelated file with a different prefix
    const std::string unrelated = save_dir + "unrelated_file_graveyard_test.sav";
    write_dummy_file( unrelated );

    remove_graveyard_subdir( graveyard_dir, dest_dir );
    REQUIRE( !dir_exist( dest_dir ) );

    g->move_save_to_graveyard( dirname );

    // Unrelated file must still exist in save dir
    CHECK( file_exist( unrelated ) );

    remove_file( unrelated );
    remove_graveyard_subdir( graveyard_dir, dest_dir );
}

TEST_CASE( "move_save_to_graveyard_creates_directories", "[graveyard]" )
{
    const std::string save_dir  = g->get_active_world()->info->folder_path() + "/";
    const std::string prefix    = base64_encode( g->u.get_save_id() ) + ".";
    const std::string graveyard_dir = PATH_INFO::graveyarddir();
    const std::string dirname   = "test_mkdir_" + get_pid_string();
    const std::string dest_dir  = graveyard_dir + dirname + "/";

    // Ensure the graveyard subtree does not exist beforehand
    remove_graveyard_subdir( graveyard_dir, dest_dir );
    REQUIRE( !dir_exist( dest_dir ) );

    // At least one matching file so the function has something to move
    const std::string file1 = save_dir + prefix + "graveyard_mkdir_test";
    write_dummy_file( file1 );

    g->move_save_to_graveyard( dirname );

    CHECK( dir_exist( graveyard_dir ) );
    CHECK( dir_exist( dest_dir ) );

    remove_file( file1 );
    remove_graveyard_subdir( graveyard_dir, dest_dir );
}
