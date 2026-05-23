#include "catch/catch.hpp"

#include "cata_utility.h"

#include <filesystem>
#include <string>

#include "debug.h"
#include "filesystem.h"
#include "game.h"
#include "state_helpers.h"
#include "world.h"
#include "worldfactory.h"

namespace fs = std::filesystem;

TEST_CASE( "failed save load blocks saving over the broken save", "[save][load]" )
{
    clear_all_state();
    g->clear_failed_load_save_block();
    const auto cleanup = on_out_of_scope( []() {
        clear_all_state();
        g->clear_failed_load_save_block();
    } );

    const auto fixture_path = fs::path( "tests/data/save/broken_load_save/#QnJva2VuU2F2ZQ==.sav" );
    const auto world_path = fs::path( g->get_active_world()->info->folder_path() );
    const auto save_path = world_path / "#QnJva2VuU2F2ZQ==.sav";
    CHECK( fs::copy_file( fixture_path, save_path, fs::copy_options::overwrite_existing ) );

    const auto before_load = read_entire_file( save_path );
    auto loaded = true;
    const auto debug_message = capture_debugmsg_during( [&]() { loaded = g->load( save_t::from_save_id( "BrokenSave" ) ); } );

    CHECK_FALSE( loaded );
    CHECK( debug_message.find( "Bad save json" ) != std::string::npos );
    CHECK_FALSE( g->save( false ) );
    CHECK( read_entire_file( save_path ) == before_load );
}
