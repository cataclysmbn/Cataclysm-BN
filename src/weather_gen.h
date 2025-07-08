#pragma once

#include <string>

#include "calendar.h"
#include "coordinates.h"
#include "units_temperature.h"
#include "weather_type.h"

struct tripoint;
class JsonObject;

struct w_point {
    units::temperature temperature = 0_f;
    double humidity = 0;
    double pressure = 0;
    double windpower = 0;
    std::string wind_desc;
    int winddirection = 0;
    bool acidic = false;
};

struct season_modifier {
    units::temperature average_temperature = 0_c;
    int humidity_mod = 0;
};

class weather_generator
{
    public:
        // Average humidity
        double base_humidity = 0;
        // Average atmospheric pressure
        double base_pressure = 0;
        double base_acid = 0;
        //Average yearly windspeed
        double base_wind = 0;
        //How much the wind peaks above average
        int base_wind_distrib_peaks = 0;
        std::array<season_modifier, 4> season_stats;
        //How much the wind folows seasonal variation ( lower means more change )
        int base_wind_season_variation = 0;

        // Half the difference between coldest hour and warmest hour
        units::temperature temperature_daily_amplitude = 5_c;
        // Half the difference between coldest point in 3D coords and warmest one
        units::temperature temperature_noise_amplitude = 8_c;

        // TODO: Remove this horrible static variable!
        static int current_winddir;
        std::vector<weather_type_id> weather_types;
        weather_generator();

        const weather_type_id &get_bad_weather() const;
        const weather_type_id &get_default_weather() const;

        int forecast_priority( const weather_type_id &w ) const;

        /**
         * TODO: Remove the regular tripoint overload, replace with *_abs_ms one.
         */
        w_point get_weather( const tripoint &, const time_point &, unsigned seed ) const;
        w_point get_weather( const tripoint_abs_ms &location, const time_point &t,
                             const calendar_config &calendar_config, unsigned seed ) const;
        const weather_type_id &get_weather_conditions( const tripoint &, const time_point &,
                unsigned seed ) const;
        const weather_type_id &get_weather_conditions( const w_point & ) const;
        int get_wind_direction( season_type ) const;
        int convert_winddir( int ) const;
        void test_weather( unsigned ) const;

        units::temperature get_weather_temperature( const tripoint_abs_ms &, const time_point &,
                const calendar_config &calendar_config, unsigned ) const;
        units::temperature get_water_temperature( const tripoint_abs_ms &, const time_point &,
                const calendar_config &calendar_config, unsigned ) const;

        static weather_generator load( const JsonObject &jo );
};


