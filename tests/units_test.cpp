#include "catch/catch.hpp"

#include <array>
#include <sstream>
#include <string>
#include <vector>

#include "calendar.h"
#include "json.h"
#include "options_helpers.h"
#include "units.h"
#include "units_serde.h"
#include "units_utility.h"
#include "weather.h"

TEST_CASE( "units_have_correct_ratios", "[units]" )
{
    CHECK( 1_liter == 1000_ml );
    CHECK( 1.0_liter == 1000.0_ml );
    CHECK( 1_gram == 1000_milligram );
    CHECK( 1.0_gram == 1000.0_milligram );
    CHECK( 1_kilogram == 1000_gram );
    CHECK( 1.0_kilogram == 1000.0_gram );
    CHECK( 1_kJ == 1000_J );
    CHECK( 1.0_kJ == 1000.0_J );
    CHECK( 1_USD == 100_cent );
    CHECK( 1.0_USD == 100.0_cent );
    CHECK( 1_kUSD == 1000_USD );
    CHECK( 1.0_kUSD == 1000.0_USD );
    CHECK( 1_days == 24_hours );
    CHECK( 1_hours == 60_minutes );
    CHECK( 1_minutes == 60_seconds );

    CHECK( 1_J == units::from_joule( 1 ) );
    CHECK( 1_kJ == units::from_kilojoule( 1 ) );

    CHECK( 1_seconds == time_duration::from_seconds( 1 ) );
    CHECK( 1_minutes == time_duration::from_minutes( 1 ) );
    CHECK( 1_hours == time_duration::from_hours( 1 ) );
    CHECK( 1_days == time_duration::from_days( 1 ) );

    CHECK( M_PI * 1_radians == 1_pi_radians );
    CHECK( 2_pi_radians == 360_degrees );
    CHECK( 60_arcmin == 1_degrees );

    CHECK( 1_c == units::from_celsius( 1 ) );
}

TEST_CASE( "body_temperature_units_preserve_legacy_body_temperature_scale", "[units][bodytemp]" )
{
    const auto thresholds = std::array{
        std::pair{ BODYTEMP_FREEZING, 500 },
        std::pair{ BODYTEMP_VERY_COLD, 2000 },
        std::pair{ BODYTEMP_COLD, 3500 },
        std::pair{ BODYTEMP_NORM, 5000 },
        std::pair{ BODYTEMP_HOT, 6500 },
        std::pair{ BODYTEMP_VERY_HOT, 8000 },
        std::pair{ BODYTEMP_SCORCHING, 9500 },
    };

    for( const auto &[temperature, legacy] : thresholds ) {
        CAPTURE( legacy );
        CHECK( units::to_legacy_bodypart_temp( temperature ) == legacy );
        CHECK( units::from_legacy_bodypart_temp( legacy ) == temperature );
    }

    const auto hot_delta = BODYTEMP_HOT - BODYTEMP_NORM;
    CHECK( units::to_legacy_bodypart_temp_delta( hot_delta ) == 1500 );
    CHECK( units::from_legacy_bodypart_temp_delta( 1500 ) == hot_delta );
    CHECK( BODYTEMP_NORM + units::from_legacy_bodypart_temp_delta( 1500 ) == BODYTEMP_HOT );
    CHECK( units::from_legacy_bodypart_temp( 5250 ) == 37.5_c );
    CHECK( units::to_legacy_bodypart_temp( 37.5_c ) == 5250 );
}

static units::energy parse_energy_quantity( const std::string &json )
{
    std::istringstream buffer( json );
    JsonIn jsin( buffer );
    return read_from_json_string<units::energy>( jsin, units::energy_units );
}

TEST_CASE( "energy parsing from JSON", "[units]" )
{
    CHECK_THROWS( parse_energy_quantity( "\"\"" ) ); // empty string
    CHECK_THROWS( parse_energy_quantity( "27" ) ); // not a string at all
    CHECK_THROWS( parse_energy_quantity( "\"    \"" ) ); // only spaces
    CHECK_THROWS( parse_energy_quantity( "\"27\"" ) ); // no energy unit

    CHECK( parse_energy_quantity( "\"1 J\"" ) == 1_J );
    CHECK( parse_energy_quantity( "\"1 kJ\"" ) == 1_kJ );
    CHECK( parse_energy_quantity( "\"+1 J\"" ) == 1_J );
    CHECK( parse_energy_quantity( "\"+1 kJ\"" ) == 1_kJ );

    CHECK( parse_energy_quantity( "\"1 J 1 kJ\"" ) == 1_J + 1_kJ );
    CHECK( parse_energy_quantity( "\"1 kJ -4 J\"" ) == 1_kJ - 4_J );
}

static time_duration parse_time_duration( const std::string &json )
{
    std::istringstream buffer( json );
    JsonIn jsin( buffer );
    return read_from_json_string<time_duration>( jsin, time_duration::units );
}

TEST_CASE( "time_duration parsing from JSON", "[units]" )
{
    CHECK_THROWS( parse_time_duration( "\"\"" ) ); // empty string
    CHECK_THROWS( parse_time_duration( "27" ) ); // not a string at all
    CHECK_THROWS( parse_time_duration( "\"    \"" ) ); // only spaces
    CHECK_THROWS( parse_time_duration( "\"27\"" ) ); // no time unit

    CHECK( parse_time_duration( "\"1 turns\"" ) == 1_turns );
    CHECK( parse_time_duration( "\"1 minutes\"" ) == 1_minutes );
    CHECK( parse_time_duration( "\"+1 hours\"" ) == 1_hours );
    CHECK( parse_time_duration( "\"+1 days\"" ) == 1_days );

    CHECK( parse_time_duration( "\"1 turns 1 minutes 1 hours 1 days\"" ) == 1_turns + 1_minutes +
           1_hours + 1_days );
    CHECK( parse_time_duration( "\"1 turns -4 minutes 1 hours -4 days\"" ) == 1_turns - 4_minutes +
           1_hours - 4_days );
}

static units::angle parse_angle( const std::string &json )
{
    std::istringstream buffer( json );
    JsonIn jsin( buffer );
    return read_from_json_string<units::angle>( jsin, units::angle_units );
}

TEST_CASE( "angle parsing from JSON", "[units]" )
{
    CHECK_THROWS( parse_angle( "\"\"" ) ); // empty string
    CHECK_THROWS( parse_angle( "27" ) ); // not a string at all
    CHECK_THROWS( parse_angle( "\"    \"" ) ); // only spaces
    CHECK_THROWS( parse_angle( "\"27\"" ) ); // no time unit

    CHECK( parse_angle( "\"1 rad\"" ) == 1_radians );
    CHECK( parse_angle( "\"1 °\"" ) == 1_degrees );
    CHECK( parse_angle( "\"+1 arcmin\"" ) == 1_arcmin );
}

TEST_CASE( "angles_to_trig_functions" )
{
    CHECK( sin( 0_radians ) == 0 );
    CHECK( sin( 0.5_pi_radians ) == Approx( 1 ) );
    CHECK( sin( 270_degrees ) == Approx( -1 ) );
    CHECK( cos( 1_pi_radians ) == Approx( -1 ) );
    CHECK( cos( 360_degrees ) == Approx( 1 ) );
    CHECK( units::atan2( 0, -1 ) == 1_pi_radians );
    CHECK( units::atan2( 0, 1 ) == 0_radians );
    CHECK( units::atan2( 1, 0 ) == 90_degrees );
    CHECK( units::atan2( -1, 0 ) == -90_degrees );
}

TEST_CASE( "rounding" )
{
    CHECK( round_to_multiple_of( 0_degrees, 15_degrees ) == 0_degrees );
    CHECK( round_to_multiple_of( 1_degrees, 15_degrees ) == 0_degrees );
    CHECK( round_to_multiple_of( 7_degrees, 15_degrees ) == 0_degrees );
    CHECK( round_to_multiple_of( 8_degrees, 15_degrees ) == 15_degrees );
    CHECK( round_to_multiple_of( 15_degrees, 15_degrees ) == 15_degrees );
    CHECK( round_to_multiple_of( 360_degrees, 15_degrees ) == 360_degrees );
    CHECK( round_to_multiple_of( -1_degrees, 15_degrees ) == 0_degrees );
    CHECK( round_to_multiple_of( -7_degrees, 15_degrees ) == 0_degrees );
    CHECK( round_to_multiple_of( -8_degrees, 15_degrees ) == -15_degrees );
    CHECK( round_to_multiple_of( -15_degrees, 15_degrees ) == -15_degrees );
    CHECK( round_to_multiple_of( -360_degrees, 15_degrees ) == -360_degrees );
}
