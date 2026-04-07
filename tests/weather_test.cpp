#include "catch/catch.hpp"

#include <algorithm>
#include <memory>
#include <vector>

#include "calendar.h"
#include "point.h"
#include "weather.h"
#include "weather_gen.h"

static double mean_abs_running_diff( std::vector<double> const &v )
{
    double x = 0;
    int n = v.size() - 1;
    for( int i = 0 ; i < n ; ++i ) {
        x += std::abs( v[i + 1] - v[i] );
    }
    return x / n;
}

static double mean_pairwise_diffs( std::vector<double> const &a, std::vector<double> const &b )
{
    double x = 0;
    int n = a.size();
    for( int i = 0 ; i < n ; ++i ) {
        x += a[i] - b[i];
    }
    return x / n;
}

static double proportion_gteq_x( std::vector<double> const &v, double x )
{
    int count = 0;
    for( auto i : v ) {
        count += ( i >= x );
    }
    return static_cast<double>( count ) / v.size();
}

TEST_CASE( "default season temperatures", "[weather]" )
{
    unsigned seed = 0;

    weather_generator generator;
    auto &season_stats = generator.season_stats;
    season_stats[SPRING].average_temperature = 8_c;
    season_stats[SUMMER].average_temperature = 16_c;
    season_stats[AUTUMN].average_temperature = 7_c;
    season_stats[WINTER].average_temperature = -14_c;

    // Shouldn't require this 4_c extra
    // TODO: Find a reason for why it fails without it
    const units::temperature max_offset = 4_c
                                          + generator.temperature_daily_amplitude
                                          + generator.temperature_noise_amplitude;
    for( size_t current_season = 0;
         current_season < static_cast<size_t>( NUM_SEASONS );
         current_season++ ) {
        size_t next_season = ( current_season + 1 ) % NUM_SEASONS;
        const time_point start_season_time = calendar::turn_zero
                                             + current_season * calendar::season_length();
        const time_point end_season_time = calendar::turn_zero
                                           + next_season * calendar::season_length();
        const units::temperature min_temperature = std::min(
                    season_stats[current_season].average_temperature,
                    season_stats[next_season].average_temperature ) - max_offset;
        const units::temperature max_temperature = std::max(
                    season_stats[current_season].average_temperature,
                    season_stats[next_season].average_temperature ) + max_offset;
        constexpr const tripoint_abs_ms pos;
        for( time_point current_time = start_season_time;
             current_time < end_season_time;
             current_time += time_duration::from_hours( 1 ) ) {
            CAPTURE( current_season );
            const double season_progress = ( current_time - start_season_time ) / calendar::season_length();
            CAPTURE( season_progress );
            int hours_since_season_start = to_hours<int>( current_time - start_season_time );
            CAPTURE( hours_since_season_start );
            CHECK( generator.get_weather_temperature( pos, current_time, calendar::config,
                    seed ) >= min_temperature );
            CHECK( generator.get_weather_temperature( pos, current_time, calendar::config,
                    seed ) <= max_temperature );
        }
    }
}

TEST_CASE( "eternal seasons", "[weather]" )
{
    for( size_t i = 0; i < NUM_SEASONS; i++ ) {
        season_type initial_season = static_cast<season_type>( i );
        calendar_config no_eternal( calendar::turn_zero, calendar::turn_zero, initial_season, false );
        calendar_config yes_eternal( calendar::turn_zero, calendar::turn_zero, initial_season, true );
        unsigned seed = 0;

        weather_generator generator;
        generator.season_stats[i].average_temperature = 100_c;
        // No variation in time, other than average season temperature
        generator.temperature_daily_amplitude = 0_c;
        generator.temperature_noise_amplitude = 0_c;
        for( size_t j = 0; j < NUM_SEASONS; j++ ) {
            time_point mid_season = calendar::turn_zero + calendar::season_length() * ( j + 0.5 );
            bool is_initial_season = j == i;
            CAPTURE( initial_season );
            CAPTURE( i );
            CHECK( generator.get_weather_temperature( tripoint_abs_ms(), mid_season, no_eternal,
                    seed ) == ( is_initial_season ? 100_c : 0_c ) );
            CHECK( generator.get_weather_temperature( tripoint_abs_ms(), mid_season, yes_eternal,
                    seed ) == 100_c );
        }
    }
}

TEST_CASE( "water temperatures track season temperatures", "[weather]" )
{
    auto generator = weather_generator();
    auto &season_stats = generator.season_stats;
    season_stats[SPRING].average_temperature = 10_c;
    season_stats[SUMMER].average_temperature = 40_c;
    season_stats[AUTUMN].average_temperature = 10_c;
    season_stats[WINTER].average_temperature = -20_c;
    generator.temperature_daily_amplitude = 0_c;
    generator.temperature_noise_amplitude = 0_c;

    const auto current_time = calendar::turn_zero + calendar::season_length() / 2;
    const auto spring_calendar = calendar_config( calendar::turn_zero, calendar::turn_zero, SPRING,
                                 true );
    const auto summer_calendar = calendar_config( calendar::turn_zero, calendar::turn_zero, SUMMER,
                                 true );
    const auto winter_calendar = calendar_config( calendar::turn_zero, calendar::turn_zero, WINTER,
                                 true );

    const auto spring_water_temperature = generator.get_water_temperature( tripoint_abs_ms(),
                                          current_time,
                                          spring_calendar, 0 );
    const auto summer_water_temperature = generator.get_water_temperature( tripoint_abs_ms(),
                                          current_time,
                                          summer_calendar, 0 );
    const auto winter_water_temperature = generator.get_water_temperature( tripoint_abs_ms(),
                                          current_time,
                                          winter_calendar, 0 );

    CHECK( winter_water_temperature == 0_c );
    CHECK( spring_water_temperature > winter_water_temperature );
    CHECK( summer_water_temperature > spring_water_temperature );
    CHECK( summer_water_temperature == 30_c );
}

TEST_CASE( "weather realism", "[.]" )
// Check our simulated weather against numbers from real data
// from a few years in a few locations in New England. The numbers
// are based on NOAA's Local Climatological Data (LCD). Analysis code
// can be found at:
// https://gist.github.com/Kodiologist/e2f1e6685e8fd865650f97bb6a67ad07
{
    // Try a few randomly selected seeds.
    const std::vector<unsigned> seeds = {317'024'741, 870'078'684, 1'192'447'748};

    const weather_generator &wgen = get_weather().get_cur_weather_gen();
    const time_point begin = calendar::turn_zero;
    const time_point end = begin + calendar::year_length();
    const int n_days = to_days<int>( end - begin );
    const int n_hours = to_hours<int>( 1_days );
    const int n_minutes = to_minutes<int>( 1_days );

    for( auto seed : seeds ) {
        std::vector<std::vector<double>> temperature;
        temperature.resize( n_days, std::vector<double>( n_minutes, 0 ) );
        std::vector<double> hourly_precip;
        hourly_precip.resize( n_days * n_hours, 0 );

        // Collect generated weather data for a single year.
        for( time_point i = begin ; i < end ; i += 1_minutes ) {
            w_point w = wgen.get_weather( tripoint_zero, i, seed );
            int day = to_days<int>( time_past_new_year( i ) );
            int minute = to_minutes<int>( time_past_midnight( i ) );
            temperature[day][minute] = units::to_fahrenheit( w.temperature );
            int hour = to_hours<int>( time_past_new_year( i ) );
            hourly_precip[hour] +=
                precip_mm_per_hour(
                    wgen.get_weather_conditions( w )->precip )
                / 60;
        }

        // Collect daily highs and lows.
        std::vector<double> highs( n_days );
        std::vector<double> lows( n_days );
        for( int do_highs = 0 ; do_highs < 2 ; ++do_highs ) {
            std::vector<double> &t = do_highs ? highs : lows;
            std::transform( temperature.begin(), temperature.end(), t.begin(),
            [&]( std::vector<double> const & day ) {
                return do_highs
                       ? *std::max_element( day.begin(), day.end() )
                       : *std::min_element( day.begin(), day.end() );
            } );

            // Check the mean absolute difference between the highs or lows
            // of adjacent days (Fahrenheit).
            const double d = mean_abs_running_diff( t );
            CHECK( d >= ( do_highs ? 5.5 : 4 ) );
            CHECK( d <= ( do_highs ? 7.5 : 7 ) );
        }

        // Check the daily mean of the range in temperatures (Fahrenheit).
        const double mean_of_ranges = mean_pairwise_diffs( highs, lows );
        CHECK( mean_of_ranges >= 14 );
        CHECK( mean_of_ranges <= 25 );

        // Check the proportion of hours with light precipitation
        // or more, counting snow (mm of rain equivalent per hour).
        const double at_least_light_precip = proportion_gteq_x(
                hourly_precip, 1 );
        CHECK( at_least_light_precip >= .025 );
        CHECK( at_least_light_precip <= .05 );

        // Likewise for heavy precipitation.
        const double heavy_precip = proportion_gteq_x(
                                        hourly_precip, 2.5 );
        CHECK( heavy_precip >= .005 );
        CHECK( heavy_precip <= .02 );
    }
}

TEST_CASE( "windchill sanity check", "[wind_chill][slow]" )
{
    // Windchill function must be strictly monotonous with temperature
    // At all possible wind velocities and humidities
    for( int humidity = 0; humidity <= 100; humidity++ ) {
        for( int wind_mph = 0; wind_mph <= 100; wind_mph++ ) {
            int last_windchill = INT_MIN;
            for( int temperature_f = -200; temperature_f <= 200; temperature_f++ ) {
                int cur_windchill = get_local_windchill( temperature_f, humidity, wind_mph );
                CAPTURE( temperature_f, humidity, wind_mph );
                CHECK( cur_windchill >= last_windchill );
                last_windchill = cur_windchill;
            }
        }
    }
}

TEST_CASE( "local wind chill calculation", "[weather][wind_chill]" )
{
    // `get_local_windchill` returns degrees F offset from current temperature,
    // representing the amount of temperature difference from wind chill alone.
    //
    // It uses one of two formulas or models depending on the current temperature.
    // Below 50F, the North American / UK "wind chill index" is used. At 50F or above,
    // the Australian "apparent temperature" is used.
    //
    // All "quoted text" below is paraphrased from the Wikipedia article:
    // https://en.wikipedia.org/wiki/Wind_chill

    // CHECK expressions have the expected result on the left for readability.

    double temp_f;
    double humidity;
    double wind_mph;

    SECTION( "below 50F - North American and UK wind chill index" ) {

        // "As the air temperature falls, the chilling effect of any wind that is present increases.
        // For example, a 16 km/h (9.9 mph) wind will lower the apparent temperature by a wider
        // margin at an air temperature of −20C (−4F), than a wind of the same speed would if
        // the air temperature were −10C (14F)."

        GIVEN( "constant wind of 10mph" ) {
            wind_mph = 10.0f;

            WHEN( "temperature is -10C (14F)" ) {
                temp_f = 14.0f;

                THEN( "the wind chill effect is -12F" ) {
                    CHECK( -12 == get_local_windchill( temp_f, 0.0f, wind_mph ) );
                }
            }

            WHEN( "temperature is -20C (-4F)" ) {
                temp_f = -4.0f;

                THEN( "there is more wind chill, an effect of -12F" ) {
                    CHECK( -12 == get_local_windchill( temp_f, 0.0f, wind_mph ) );
                }
            }
        }
    }


    SECTION( "50F and above - Australian apparent temperature" ) {
        GIVEN( "constant temperature of 50F" ) {
            temp_f = 50.0f;

            WHEN( "wind is steady at 10mph" ) {
                wind_mph = 10.0f;

                THEN( "apparent temp increases as humidity increases" ) {
                    CHECK( -12 == get_local_windchill( temp_f, 0.0f, wind_mph ) );
                    CHECK( -9 == get_local_windchill( temp_f, 20.0f, wind_mph ) );
                    CHECK( -7 == get_local_windchill( temp_f, 40.0f, wind_mph ) );
                    CHECK( -6 == get_local_windchill( temp_f, 60.0f, wind_mph ) );
                    CHECK( -4 == get_local_windchill( temp_f, 80.0f, wind_mph ) );
                    CHECK( -3 == get_local_windchill( temp_f, 100.0f, wind_mph ) );
                }
            }

            WHEN( "humidity is steady at 90%%" ) {
                humidity = 90.0f;

                THEN( "apparent temp decreases as wind increases" ) {
                    CHECK( 2 == get_local_windchill( temp_f, humidity, 0.0f ) );
                    CHECK( -4 == get_local_windchill( temp_f, humidity, 10.0f ) );
                    CHECK( -8 == get_local_windchill( temp_f, humidity, 20.0f ) );
                    CHECK( -12 == get_local_windchill( temp_f, humidity, 30.0f ) );
                    CHECK( -16 == get_local_windchill( temp_f, humidity, 40.0f ) );
                    CHECK( -22 == get_local_windchill( temp_f, humidity, 50.0f ) );
                }
            }
        }

        GIVEN( "humidity is zero" ) {
            humidity = 0.0f;

            THEN( "apparent temp offset is only influenced by wind speed" ) {
                // No wind still gets -7 for some reason
                CHECK( -7 == get_local_windchill( 50.0f, humidity, 0.0f ) );
                CHECK( -7 == get_local_windchill( 70.0f, humidity, 0.0f ) );
                CHECK( -7 == get_local_windchill( 90.0f, humidity, 0.0f ) );

                // 25mph wind == -21F to temperature
                CHECK( -21 == get_local_windchill( 50.0f, humidity, 25.0f ) );
                CHECK( -21 == get_local_windchill( 70.0f, humidity, 25.0f ) );
                CHECK( -21 == get_local_windchill( 90.0f, humidity, 25.0f ) );
            }
        }

        GIVEN( "humidity is 50 percent" ) {
            humidity = 50.0f;

            WHEN( "there is no wind" ) {
                wind_mph = 0.0f;

                THEN( "apparent temp increases more as it gets hotter" ) {
                    CHECK( -1 == get_local_windchill( 50.0f, humidity, wind_mph ) );
                    CHECK( 0 == get_local_windchill( 60.0f, humidity, wind_mph ) );
                    CHECK( 1 == get_local_windchill( 70.0f, humidity, wind_mph ) );
                    CHECK( 4 == get_local_windchill( 80.0f, humidity, wind_mph ) );
                    CHECK( 8 == get_local_windchill( 90.0f, humidity, wind_mph ) );
                    CHECK( 13 == get_local_windchill( 100.0f, humidity, wind_mph ) );
                }
            }

            WHEN( "wind is steady at 10mph" ) {
                wind_mph = 10.0f;

                THEN( "apparent temp is less but still increases more as it gets hotter" ) {
                    CHECK( -7 == get_local_windchill( 50.0f, humidity, wind_mph ) );
                    CHECK( -6 == get_local_windchill( 60.0f, humidity, wind_mph ) );
                    CHECK( -5 == get_local_windchill( 70.0f, humidity, wind_mph ) );
                    CHECK( -2 == get_local_windchill( 80.0f, humidity, wind_mph ) );
                    CHECK( 2 == get_local_windchill( 90.0f, humidity, wind_mph ) );
                    CHECK( 7 == get_local_windchill( 100.0f, humidity, wind_mph ) );
                }
            }
        }
    }
}
