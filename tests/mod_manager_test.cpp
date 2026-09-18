#include "catch/catch.hpp"

#include <algorithm>
#include <sstream>
#include <string>
#include <vector>

#include <semver.hpp>

#include "filesystem.h"
#include "fstream_utils.h"
#include "json.h"
#include "mod_manager.h"

namespace
{

struct mod_version_entry {
    std::string path;
    std::string id;
    std::string version;
};

auto load_single_modfile( const std::string &json ) -> std::optional<MOD_INFORMATION>
{
    auto stream = std::istringstream( json );
    auto jsin = JsonIn( stream, "mod_manager_test" );
    auto jo = jsin.get_object();
    auto result = mod_management::load_modfile( jo, "data/mods/test_mod" );
    jo.finish();
    return result;
}

auto collect_mod_versions_from_file( const std::string &path ) -> std::vector<mod_version_entry>
{
    auto versions = std::vector<mod_version_entry> {};

    read_from_file_json( path, [&]( JsonIn & jsin ) {
        const auto append_mod_version = [&]( const JsonObject & jo ) {
            jo.allow_omitted_members();

            if( !jo.has_string( "type" ) || jo.get_string( "type" ) != "MOD_INFO" ||
                !jo.has_string( "version" ) ) {
                return;
            }

            const auto id = jo.has_string( "id" ) ? jo.get_string( "id" ) : jo.get_string( "ident" );

            versions.push_back( mod_version_entry{
                .path = path,
                .id = id,
                .version = jo.get_string( "version" ),
            } );
        };

        if( jsin.test_object() ) {
            auto jo = jsin.get_object();
            append_mod_version( jo );
            return;
        }

        auto entries = jsin.get_array();
        while( entries.has_more() ) {
            auto jo = entries.next_object();
            append_mod_version( jo );
        }
    }, true );

    return versions;
}

} // namespace

TEST_CASE( "load_modfile accepts project-style dependency maps", "[mod_manager]" )
{
    const auto mod = load_single_modfile( R"JSON(
        {
          "type": "MOD_INFO",
          "id": "dependency_map_test",
          "name": "Dependency Map Test",
          "description": "Tests dependency maps.",
          "version": "1.2.3",
          "dependencies": {
            "bn": "0.7.0",
            "aftershock": "1.0.0"
          }
        }
    )JSON" );

    REQUIRE( mod );

    auto dependency_ids = std::vector<std::string> {};
    for( const auto &dependency : mod->dependencies ) {
        dependency_ids.emplace_back( dependency.str() );
    }
    std::ranges::sort( dependency_ids );

    CHECK( dependency_ids == std::vector<std::string> { "aftershock", "bn" } );
}

TEST_CASE( "bundled mods use strict semantic versions", "[mod_manager]" )
{
    auto bundled_versions = std::vector<mod_version_entry> {};
    for( const auto &path : get_files_from_path( "modinfo.json", "data/mods", true ) ) {
        auto versions = collect_mod_versions_from_file( path );
        bundled_versions.insert( bundled_versions.end(), versions.begin(), versions.end() );
    }

    for( const auto &entry : bundled_versions ) {
        CAPTURE( entry.path );
        CAPTURE( entry.id );
        CAPTURE( entry.version );
        CHECK( semver::valid( entry.version ) );
    }
}
