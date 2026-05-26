#include "options.h"
#include "title_screen.h"

#include "catch/catch.hpp"

TEST_CASE( "title_screen_option_exists", "[title_screen]" )
{
    REQUIRE( get_options().has_option( title_screen::option_id ) );

    auto items = get_options().get_option( title_screen::option_id ).getItems();
    auto has_default = false;
    auto has_random = false;
    for( const auto &item : items ) {
        has_default = has_default || item.first == title_screen::default_option_id;
        has_random = has_random || item.first == title_screen::random_option_id;
    }

    CHECK( has_default );
    CHECK( has_random );
}
