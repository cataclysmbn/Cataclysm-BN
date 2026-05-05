#include "catch/catch.hpp"

#include "options.h"

TEST_CASE( "loading_screen_images_option_exists", "[loading_ui]" )
{
    REQUIRE( get_options().has_option( "LOADING_SCREEN_IMAGES" ) );
    CHECK( get_options().get_option( "LOADING_SCREEN_IMAGES" ).getValue() == "true" );
}
