#include "catch/catch.hpp"

#include "filesystem.h"
#include "path_info.h"
#include "preload_config.h"

#include <fstream>
#include <iterator>
#include <string>

namespace
{

class preload_config_file_guard
{
    public:
        ~preload_config_file_guard() {
            remove_file( PATH_INFO::preload() );
            preload_config::set_compute_accel( preload_config::compute_accel::auto_select );
            preload_config::set_texture_streaming( preload_config::tristate::auto_select );
        }
};

auto read_preload_config() -> std::string
{
    REQUIRE( assure_dir_exist( PATH_INFO::config_dir() ) );
    auto file = std::ifstream( PATH_INFO::preload(), std::ios::binary );
    REQUIRE( file.good() );
    return { std::istreambuf_iterator<char>( file ), std::istreambuf_iterator<char>() };
}

auto write_preload_config( const std::string &contents ) -> void
{
    REQUIRE( assure_dir_exist( PATH_INFO::config_dir() ) );
    auto file = std::ofstream( PATH_INFO::preload(), std::ios::binary );
    REQUIRE( file.good() );
    file << contents;
}

} // namespace

TEST_CASE( "preload config saves texture streaming as a string", "[preload_config]" )
{
    auto restore_preload_config = preload_config_file_guard();

    preload_config::set_texture_streaming( preload_config::tristate::enable );
    REQUIRE( assure_dir_exist( PATH_INFO::config_dir() ) );

    preload_config::save();

    const auto contents = read_preload_config();
    CHECK( contents.find( R"("texture_streaming": "enable")" ) != std::string::npos );
}

TEST_CASE( "preload config loads legacy numeric texture streaming", "[preload_config]" )
{
    auto restore_preload_config = preload_config_file_guard();
    write_preload_config( R"({ "compute_acceleration": "auto", "texture_streaming": 0 })" );

    preload_config::set_texture_streaming( preload_config::tristate::enable );

    preload_config::load();

    CHECK( preload_config::get_texture_streaming() == preload_config::tristate::auto_select );
}

TEST_CASE( "preload config accepts explicit compute acceleration backends", "[preload_config]" )
{
    CHECK( preload_config::compute_accel_from_string( "auto" ) ==
           preload_config::compute_accel::auto_select );
    CHECK( preload_config::compute_accel_from_string( "cpu" ) ==
           preload_config::compute_accel::cpu );
    CHECK( preload_config::compute_accel_from_string( "gpu" ) ==
           preload_config::compute_accel::gpu );
    CHECK( preload_config::compute_accel_from_string( "gpu_software" ) ==
           preload_config::compute_accel::gpu_software );

    CHECK( preload_config::compute_accel_from_string( "off" ) ==
           preload_config::compute_accel::cpu );
    CHECK( preload_config::compute_accel_from_string( "force" ) ==
           preload_config::compute_accel::gpu );
    CHECK( preload_config::compute_accel_from_string( "software" ) ==
           preload_config::compute_accel::gpu_software );

    CHECK( preload_config::compute_accel_to_string( preload_config::compute_accel::auto_select ) ==
           "auto" );
    CHECK( preload_config::compute_accel_to_string( preload_config::compute_accel::cpu ) ==
           "cpu" );
    CHECK( preload_config::compute_accel_to_string( preload_config::compute_accel::gpu ) ==
           "gpu" );
    CHECK( preload_config::compute_accel_to_string( preload_config::compute_accel::gpu_software ) ==
           "gpu_software" );
}
