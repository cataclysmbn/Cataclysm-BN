#include "catch/catch.hpp"

#include <sstream>
#include <string>

#include "json.h"
#include "json_assertion_helpers.h"

namespace
{

auto sample_json_output() -> std::string
{
    auto output = std::ostringstream {};
    auto jsout = JsonOut( output, true );
    jsout.start_object();
    jsout.member( "outer" );
    jsout.start_object();
    jsout.member( "b", 2 );
    jsout.member( "a" );
    jsout.start_array();
    jsout.start_object();
    jsout.member( "y", true );
    jsout.member( "x", 1 );
    jsout.end_object();
    jsout.write_null();
    jsout.end_array();
    jsout.end_object();
    jsout.member( "name", "snapshot" );
    jsout.end_object();
    return output.str();
}

} // namespace

TEST_CASE( "json_snapshot_helper_matches_deep_json_ignoring_object_order", "[json][snapshot]" )
{
    json_snapshot::check_json_snapshot(
        sample_json_output(),
        "tests/data/json_snapshots/json_snapshot_test/sample.json",
        { .ignore_deep_object_order = true, .pretty_print = true } );
}
