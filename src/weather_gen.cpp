#include "weather_gen.h"

#include <algorithm>
#include <cmath>
#include <numeric>
#include <ostream>
#include <random>
#include <string>

#include "assign.h"
#include "cached_options.h"
#include "cata_utility.h"
#include "fstream_utils.h"
#include "game_constants.h"
#include "generic_readers.h"
#include "json.h"
#include "math_defines.h"
#include "point.h"
#include "rng.h"
#include "simplexnoise.h"
#include "weather.h"

namespace
{
constexpr double tau = M_PI * 2;
// Out of 24 hours
constexpr double coldest_hour = 5;
} //namespace

weather_generator::weather_generator() = default;
// TODO: Remove this disgusting static variable!
int weather_generator::current_winddir = 1000;

struct weather_gen_common {
    double x;
    double y;
    double z;
    // Awkward name, but better than `cyf`
    double cosine_of_gregorian_year_fraction;
    double year_fraction;
    unsigned modSEED;
    season_type season;
};

static weather_gen_common get_common_data( const point_abs_ms &location, const time_point &t,
        const calendar_config &calendar_config, unsigned seed )
{
    weather_gen_common result;
    // Integer x position / widening factor of the Perlin function.
    result.x = location.x() / 2000.0;
    // Integer y position / widening factor of the Perlin function.
    result.y = location.y() / 2000.0;
    // Integer turn / widening factor of the Perlin function.
    result.z = to_days<double>( t - calendar::turn_zero );
    // Limit the random seed during noise calculation, a large value flattens the noise generator to zero
    // Windows has a rand limit of 32768, other operating systems can have higher limits
    result.modSEED = seed % SIMPLEX_NOISE_RANDOM_SEED_LIMIT;
    // Eternal season = midpoint of initial season all the time
    const double year_fraction = calendar_config.eternal_season() ?
                                 ( 0.25 * static_cast<double>( calendar_config.initial_season() ) + 0.125 ) :
                                 ( time_past_new_year( t ) / calendar::year_length() );

    result.year_fraction = year_fraction;
    // We add one-eighth to line it up so that +1 is at
    // midwinter and -1 at midsummer. (Cataclysm years
    // start when spring starts. Gregorian years start when
    // winter starts.)
    result.cosine_of_gregorian_year_fraction = std::cos( tau * ( year_fraction + .125 ) ); // [-1, 1]
    result.season = season_of_year( t );

    return result;
}

static units::temperature season_temp( const weather_generator &wg, double year_fraction )
{
    // Interpolate seasons temperature
    // Scale year_fraction [0, 1) to [0.0, 4.0). So [0.0, 1.0) - spring, [1.0, 2.0) - summer, [2.0, 3.0) - autumn, [3.0, 4.0) - winter.
    const double quadrum = year_fraction * 4;
    const double season_midpoint_quadrum = quadrum - 0.5;
    constexpr auto num_seasons = static_cast<size_t>( season_type::NUM_SEASONS );
    const size_t current_season = ( static_cast<size_t>( std::floor( season_midpoint_quadrum ) +
                                    num_seasons ) ) % num_seasons;
    const size_t next_season = ( current_season + 1 ) % num_seasons;
    // 0 - current season just started, 1 - season just ended (shouldn't actually happen)
    const double t = season_midpoint_quadrum - std::floor( season_midpoint_quadrum );
    return units::multiply_any_unit( wg.season_stats[current_season].average_temperature, 1.0 - t )
           + units::multiply_any_unit( wg.season_stats[next_season].average_temperature, t );
}

static units::temperature weather_temperature_from_common_data( const weather_generator &wg,
        const weather_gen_common &common, const time_point &t )
{
    const double x( common.x );
    const double y( common.y );
    const double z( common.z );

    const unsigned modSEED = common.modSEED;
    const double dayFraction = time_past_midnight( t ) / 1_days;
    // -1 at coldest_hour, +1 twelve hours later
    const double dayv = std::cos( tau * ( dayFraction + .5 - coldest_hour / 24 ) );

    units::temperature season_factor = season_temp( wg, common.year_fraction );
    const double temperature_celsius =
        units::to_celsius<double>( season_factor ) +
        dayv * units::to_celsius<double>( wg.temperature_daily_amplitude ) +
        raw_noise_4d( x, y, z, modSEED ) * units::to_celsius<double>( wg.temperature_noise_amplitude );

    return units::from_celsius( temperature_celsius );
}

units::temperature weather_generator::get_weather_temperature( const tripoint_abs_ms &location,
        const time_point &t, const calendar_config &calendar_config, unsigned seed ) const
{
    return weather_temperature_from_common_data( *this, get_common_data( location.xy(), t,
            calendar_config, seed ), t );
}

w_point weather_generator::get_weather( const tripoint &location, const time_point &t,
                                        unsigned seed ) const
{
    return get_weather( tripoint_abs_ms( location ), t, calendar::config, seed );
}

w_point weather_generator::get_weather( const tripoint_abs_ms &location, const time_point &t,
                                        const calendar_config &calendar_config, unsigned seed ) const
{
    const weather_gen_common common = get_common_data( location.xy(), t, calendar_config, seed );

    const double x( common.x );
    const double y( common.y );
    const double z( common.z );

    const unsigned modSEED = common.modSEED;
    // +1 in midwinter, -1 in midsummer
    const double cgyf = common.cosine_of_gregorian_year_fraction;
    const season_type season = common.season;

    // Noise factors
    const units::temperature T( weather_temperature_from_common_data( *this, common, t ) );
    double A( raw_noise_4d( x, y, z, modSEED ) * 8.0 );
    double W( raw_noise_4d( x / 2.5, y / 2.5, z / 200, modSEED ) * 10.0 );

    // Humidity variation
    double mod_h = season_stats[static_cast<size_t>( season )].humidity_mod;
    // Relative humidity, a percentage.
    double H = std::min( 100., std::max( 0.,
                                         base_humidity + mod_h + 100 * (
                                                 .15 * -cgyf +
                                                 raw_noise_4d( x, y, z, modSEED + 101 ) *
                                                 .2 * ( cgyf + 2 ) ) ) );

    // Pressure
    double P =
        base_pressure +
        raw_noise_4d( x, y, z, modSEED + 211 ) *
        10 * ( cgyf + 2 );

    // Wind power
    W = std::max( 0, static_cast<int>( base_wind * rng( 1, 2 ) / std::pow( ( P + W ) / 1014.78, rng( 9,
                                       base_wind_distrib_peaks ) ) +
                                       -cgyf / base_wind_season_variation * rng( 1, 2 ) ) );
    // Initial static variable
    if( current_winddir == 1000 ) {
        current_winddir = get_wind_direction( season );
        current_winddir = convert_winddir( current_winddir );
    } else {
        // When wind strength is low, wind direction is more variable
        bool changedir = one_in( W * 2160 );
        if( changedir ) {
            current_winddir = get_wind_direction( season );
            current_winddir = convert_winddir( current_winddir );
        }
    }
    std::string wind_desc = get_wind_desc( W );
    // Acid rains
    const double acid_content = base_acid * A;
    bool acid = acid_content >= 1.0;
    return w_point{ T, H, P, W, wind_desc, current_winddir, acid };
}

const weather_type_id &weather_generator::get_default_weather() const
{
    return weather_types[0];
}

const weather_type_id &weather_generator::get_bad_weather() const
{
    const weather_type_id *bad_weather = &get_default_weather();
    for( const weather_type_id &wt : weather_types ) {
        if( wt->precip == precip_class::heavy ) {
            bad_weather = &wt;
        }
    }
    return *bad_weather;
}

int weather_generator::forecast_priority( const weather_type_id &w ) const
{
    auto it = std::ranges::find( weather_types, w );
    if( it == weather_types.end() ) {
        return -1;
    }
    return std::distance( weather_types.begin(), it );
}

const weather_type_id &weather_generator::get_weather_conditions( const tripoint &location,
        const time_point &t, unsigned seed ) const
{
    w_point w( get_weather( location, t, seed ) );
    return get_weather_conditions( w );
}

const weather_type_id &weather_generator::get_weather_conditions( const w_point &w ) const
{
    w_point wp2 = w;
    const weather_type_id *current_conditions = &weather_type_id::NULL_ID();
    for( const weather_type_id &type : weather_types ) {
        const weather_requirements &wrequires = type->requirements;
        weather_requirements rq2 = wrequires;
        bool test_pressure =
            wrequires.pressure_max > w.pressure &&
            wrequires.pressure_min < w.pressure;
        bool test_humidity =
            wrequires.humidity_max > w.humidity &&
            wrequires.humidity_min < w.humidity;
        if( ( wrequires.humidity_and_pressure && !( test_pressure && test_humidity ) ) ||
            ( !wrequires.humidity_and_pressure && !( test_pressure || test_humidity ) ) ) {
            continue;
        }
        bool test_temperature =
            wrequires.temperature_max > units::to_fahrenheit( w.temperature ) &&
            wrequires.temperature_min < units::to_fahrenheit( w.temperature );
        bool test_windspeed =
            wrequires.windpower_max > w.windpower &&
            wrequires.windpower_min < w.windpower;
        bool test_acidic = !wrequires.acidic || w.acidic;
        if( !( test_temperature && test_windspeed && test_acidic ) ) {
            continue;
        }

        if( !wrequires.required_weathers.empty() ) {
            if( std::ranges::find( wrequires.required_weathers,
                                   *current_conditions ) == wrequires.required_weathers.end() ) {
                continue;
            }
        }

        if( wrequires.time != weather_time_requirement_type::both ) {
            bool day = is_day( calendar::turn );
            if( ( wrequires.time == weather_time_requirement_type::day && !day ) ||
                ( wrequires.time == weather_time_requirement_type::night && day ) ) {
                continue;
            }
        }
        current_conditions = &type;
    }
    return current_conditions->obj().id;
}

int weather_generator::get_wind_direction( const season_type season ) const
{
    cata_default_random_engine &wind_dir_gen = rng_get_engine();
    // Assign chance to angle direction
    if( season == SPRING ) {
        std::discrete_distribution<int> distribution {3, 3, 5, 8, 11, 10, 5, 2, 5, 6, 6, 5, 8, 10, 8, 6};
        return distribution( wind_dir_gen );
    } else if( season == SUMMER ) {
        std::discrete_distribution<int> distribution {3, 4, 4, 8, 8, 9, 8, 3, 7, 8, 10, 7, 7, 7, 5, 3};
        return distribution( wind_dir_gen );
    } else if( season == AUTUMN ) {
        std::discrete_distribution<int> distribution {4, 6, 6, 7, 6, 5, 4, 3, 5, 6, 8, 8, 10, 10, 8, 5};
        return distribution( wind_dir_gen );
    } else if( season == WINTER ) {
        std::discrete_distribution<int> distribution {5, 3, 2, 3, 2, 2, 2, 2, 4, 6, 10, 8, 12, 19, 13, 9};
        return distribution( wind_dir_gen );
    } else {
        return 0;
    }
}

int weather_generator::convert_winddir( const int inputdir ) const
{
    // Convert from discrete distribution output to angle
    float finputdir = inputdir * 22.5;
    return static_cast<int>( finputdir );
}

units::temperature weather_generator::get_water_temperature(
    const tripoint_abs_ms &location,
    const time_point &time,
    const calendar_config &calendar_config,
    unsigned seed ) const
{
    // Instead of using a realistic model, we'll just smooth out air temperature
    // Smooth out both in time and intensity
    // And add caps - it must stay liquid water
    constexpr std::array<std::pair<time_duration, double>, 7> measurement_weights = {{
            { 7_days, 0.1 },
            { 7_days + 12_hours, 0.1 },
            { 3_days, 0.2 },
            { 3_days + 12_hours, 0.2 },
            { 1_days, 0.2 },
            { 0_days + 12_hours, 0.2 },
            { 0_days, 0.1 }
        }
    };
    const units::temperature weighted_avg = std::accumulate( measurement_weights.begin(),
                                            measurement_weights.end(),
                                            0_c,
                                            [this, location, time, seed, calendar_config]( units::temperature acc,
    const std::pair<time_duration, double> &pr ) {
        units::temperature weather_temperature =
            get_weather_temperature( location, time - pr.first, calendar_config, seed );
        return acc + multiply_any_unit( weather_temperature, pr.second );
    } );
    // Rescale the range:
    // For avg air temp<-10C, water is 0C
    // For avg air temp> 30C, water is 30C
    // logarithmic_range smoothing for the in-between
    constexpr int lower_limit = units::to_millidegree_celsius( -10_c );
    constexpr int upper_limit = units::to_millidegree_celsius( 30_c );
    const int weighted_average_celsius = units::to_millidegree_celsius( weighted_avg );
    const double t = logarithmic_range( lower_limit,
                                        upper_limit,
                                        weighted_average_celsius );
    return multiply_any_unit( 0_c, 1 - t ) + multiply_any_unit( 30_c, t );
}

void weather_generator::test_weather( unsigned seed = 1000 ) const
{
    // Outputs a Cata year's worth of weather data to a CSV file.
    // Usage:
    // weather_generator WEATHERGEN; // Instantiate the class.
    // WEATHERGEN.test_weather(); // Runs this test.
    write_to_file( "weather.output", [&]( std::ostream & testfile ) {
        testfile <<
                 "|;year;season;day;hour;minute;temperature(F);humidity(%);pressure(mB);weatherdesc;windspeed(mph);winddirection"
                 << '\n';

        const time_point begin = calendar::turn;
        const time_point end = begin + 2 * calendar::year_length();
        for( time_point i = begin; i < end; i += 20_minutes ) {
            w_point w = get_weather( tripoint_zero, i, seed );
            const weather_type_id &conditions = get_weather_conditions( w );

            int year = to_turns<int>( i - calendar::turn_zero ) / to_turns<int>
                       ( calendar::year_length() ) + 1;
            const int hour = hour_of_day<int>( i );
            const int minute = minute_of_hour<int>( i );
            int day;
            if( calendar::eternal_season() ) {
                day = to_days<int>( time_past_new_year( i ) );
            } else {
                day = day_of_season<int>( i );
            }
            testfile << "|;" << year << ";" << season_of_year( i ) << ";" << day << ";" << hour << ";" << minute
                     << ";" << w.temperature << ";" << w.humidity << ";" << w.pressure << ";" << conditions->name << ";"
                     << w.windpower << ";" << w.winddirection << '\n';
        }

    }, "weather test file" );
}

inline bool maybe_temperature_reader( const JsonObject &jo, const std::string &member_name,
                                      units::temperature &member, bool was_loaded )
{
    try {
        return temperature_reader()( jo, member_name, member, was_loaded );
    } catch( const JsonError & ) {
        int legacy_value;
        if( !jo.read( member_name, legacy_value ) ) {
            return false;
        }
        member = units::from_celsius( legacy_value );
    }
    return true;
}

weather_generator weather_generator::load( const JsonObject &jo )
{
    static const std::array<std::pair<std::string, int>, NUM_SEASONS> legacy_temp_id_values = {{
            {"spring_temp_manual_mod", 0},
            {"summer_temp_manual_mod", 10},
            {"autumn_temp_manual_mod", 0},
            {"winter_temp_manual_mod", -15},
        }
    };
    static const std::array<std::string, NUM_SEASONS> season_temp_ids = {
        "spring_temp", "summer_temp", "autumn_temp", "winter_temp"
    };
    static const std::array<std::string, NUM_SEASONS> season_humidity_ids = {
        "spring_humidity_manual_mod",
        "summer_humidity_manual_mod",
        "autumn_humidity_manual_mod",
        "winter_humidity_manual_mod"
    };

    weather_generator ret;

    float base_temp = jo.get_float( "base_temperature", 0.0 );
    // Handling legacy temperature settings
    // Don't handle legacy settings in strict mode, let it error
    if( !json_report_strict ) {
        for( size_t i = 0; i < season_temp_ids.size(); i++ ) {
            ret.season_stats[i].average_temperature = units::from_celsius(
                        base_temp +
                        jo.get_int( legacy_temp_id_values[i].first, 0 ) +
                        legacy_temp_id_values[i].second
                    );
        }
    }
    // Reading temperature settings
    for( size_t i = 0; i < season_temp_ids.size(); i++ ) {
        maybe_temperature_reader( jo, season_temp_ids[i],
                                  ret.season_stats[i].average_temperature, false );
        assign( jo, season_humidity_ids[i], ret.season_stats[i].humidity_mod );
    }

    // Reading other weather settings.
    ret.base_humidity = jo.get_float( "base_humidity", 50.0 );
    ret.base_pressure = jo.get_float( "base_pressure", 0.0 );
    ret.base_acid = jo.get_float( "base_acid", 0.0 );
    ret.base_wind = jo.get_float( "base_wind", 0.0 );
    ret.base_wind_distrib_peaks = jo.get_int( "base_wind_distrib_peaks", 0 );
    ret.base_wind_season_variation = jo.get_int( "base_wind_season_variation", 0 );

    assign( jo, "temperature_daily_amplitude", ret.temperature_daily_amplitude );
    assign( jo, "temperature_noise_amplitude", ret.temperature_noise_amplitude );

    jo.get_member( "weather_types" ); // Throw if does not exist
    jo.read( "weather_types", ret.weather_types );
    if( ret.weather_types.empty() ) {
        jo.throw_error( "expected at least 1 weather type", "weather_types" );
    }
    return ret;
}
