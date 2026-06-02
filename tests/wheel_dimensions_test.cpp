#include "catch/catch.hpp"

#include <sstream>
#include <string>

#include "json.h"
#include "options_helpers.h"
#include "wheel_dimensions.h"

namespace
{

auto read_dimension( const std::string &json, const std::string &member ) -> int
{
    std::istringstream buffer( json );
    JsonIn jsin( buffer );
    const auto jo = jsin.get_object();
    const auto dimension = wheel_dimensions::read_from_json( jo, member );
    REQUIRE( dimension.has_value() );
    return *dimension;
}

} // namespace

TEST_CASE( "wheel dimensions accept metric strings and legacy inches", "[wheel][units]" )
{
    CHECK( read_dimension( R"({"diameter":17})", "diameter" ) == 425 );
    CHECK( read_dimension( R"({"diameter":"425 mm"})", "diameter" ) == 425 );
    CHECK( read_dimension( R"({"width":"225mm"})", "width" ) == 225 );
}

TEST_CASE( "wheel dimensions display in selected units", "[wheel][units]" )
{
    {
        const override_option metric_display( "DISTANCE_UNITS", "metric" );
        CHECK( wheel_dimensions::format_for_display( 425 ) == "42.5cm" );
        CHECK( wheel_dimensions::format_for_display( 500 ) == "50cm" );
    }

    {
        const override_option imperial_display( "DISTANCE_UNITS", "imperial" );
        CHECK( wheel_dimensions::format_for_display( 425 ) == "17\"" );
        CHECK( wheel_dimensions::format_for_display( 500 ) == "20\"" );
    }
}
