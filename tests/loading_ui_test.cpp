#include "catch/catch.hpp"

#include "loading_ui.h"
#include "options.h"

TEST_CASE( "loading_screen_images_option_exists", "[loading_ui]" )
{
    REQUIRE( get_options().has_option( "LOADING_SCREEN_IMAGES" ) );
    CHECK( get_options().get_option( "LOADING_SCREEN_IMAGES" ).getValue() == "true" );
}

TEST_CASE( "loading_screen_image_scales_to_full_screen_size", "[loading_ui]" )
{
    const auto scaled_size = get_scaled_loading_image_size( loading_image_scaling_options{
        .image_size = point( 2560, 1440 ),
        .screen_size = point( 1600, 900 )
    } );

    REQUIRE( scaled_size.has_value() );
    CHECK( *scaled_size == point( 1600, 900 ) );
}

TEST_CASE( "loading_screen_image_fits_full_height_and_scales_width", "[loading_ui]" )
{
    const auto scaled_size = get_scaled_loading_image_size( loading_image_scaling_options{
        .image_size = point( 2560, 1440 ),
        .screen_size = point( 1600, 700 )
    } );

    REQUIRE( scaled_size.has_value() );
    CHECK( *scaled_size == point( 1244, 700 ) );
}

TEST_CASE( "loading_screen_image_fits_full_height_for_tall_image", "[loading_ui]" )
{
    const auto scaled_size = get_scaled_loading_image_size( loading_image_scaling_options{
        .image_size = point( 1000, 2000 ),
        .screen_size = point( 1600, 900 )
    } );

    REQUIRE( scaled_size.has_value() );
    CHECK( *scaled_size == point( 450, 900 ) );
}

TEST_CASE( "loading_screen_image_scaling_rejects_invalid_sizes", "[loading_ui]" )
{
    CHECK( !get_scaled_loading_image_size( loading_image_scaling_options{
        .image_size = point_zero,
        .screen_size = point( 1280, 720 )
    } ).has_value() );
    CHECK( !get_scaled_loading_image_size( loading_image_scaling_options{
        .image_size = point( 1920, 1080 ),
        .screen_size = point_zero
    } ).has_value() );
}
