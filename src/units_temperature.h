#pragma once

#include <algorithm>

#include "units_def.h"

namespace units
{

constexpr double fahrenheit_to_celsius( double fahrenheit )
{
    return ( ( fahrenheit - 32.0 ) * 5.0 / 9.0 );
}

/**
 * Convert a temperature from degrees Fahrenheit to Kelvin.
 *
 * @return Temperature in degrees K.
 */
constexpr double fahrenheit_to_kelvin( double fahrenheit )
{
    return fahrenheit_to_celsius( fahrenheit ) + 273.15;
}

/**
 * Convert a temperature from Kelvin to degrees Fahrenheit.
 *
 * @return Temperature in degrees C.
 */
constexpr double kelvin_to_fahrenheit( double kelvin )
{
    return 1.8 * ( kelvin - 273.15 ) + 32;
}

/**
 * Convert a temperature from Celsius to degrees Fahrenheit.
 *
 * @return Temperature in degrees F.
 */
constexpr double celsius_to_fahrenheit( double celsius )
{
    return celsius * 9 / 5 + 32;
}

/**
 * Convert a temperature from Celsius to kelvins.
 *
 * @return Temperature in degrees F.
 */
constexpr double celsius_to_kelvin( double celsius )
{
    return celsius + 273.15;
}

class temperature_in_millidegree_celsius_tag
{
};

using temperature = quantity<int, temperature_in_millidegree_celsius_tag>;

// Temperature contains different units and multiplication can get weird
// Say, 10 * 1_f != 10_f
template<>
struct quantity_details<temperature_in_millidegree_celsius_tag> {
    using common_zero_point = std::false_type;
};

const temperature temperature_min = units::temperature(
                                        std::numeric_limits<units::temperature::value_type>::min(),
                                        units::temperature::unit_type{} );

const temperature temperature_max = units::temperature(
                                        std::numeric_limits<units::temperature::value_type>::max(),
                                        units::temperature::unit_type{} );

template<typename value_type>
constexpr quantity<value_type, temperature_in_millidegree_celsius_tag>
from_millidegree_celsius(
    const value_type v )
{
    return quantity<value_type, temperature_in_millidegree_celsius_tag>( v,
            temperature_in_millidegree_celsius_tag{} );
}

template<typename value_type>
constexpr quantity<value_type, temperature_in_millidegree_celsius_tag> from_celsius(
    const value_type v )
{
    const value_type max_temperature_celsius = std::numeric_limits<value_type>::max() / 1000;
    const value_type temperature = v > max_temperature_celsius ? max_temperature_celsius : v;
    return from_millidegree_celsius<value_type>( temperature * 1000 );
}

template<typename value_type>
constexpr quantity<value_type, temperature_in_millidegree_celsius_tag> from_fahrenheit(
    const value_type v )
{
    // Explicit casts to silence warnings about lossy conversions
    constexpr value_type max_temperature_fahrenheit = static_cast<value_type>( celsius_to_fahrenheit(
                // NOLINTNEXTLINE(bugprone-integer-division)
                static_cast<double>( std::numeric_limits<value_type>::max() / 1000 ) ) );
    const value_type temperature = v > max_temperature_fahrenheit ? max_temperature_fahrenheit : v;
    return from_millidegree_celsius<value_type>(
               static_cast<value_type>( fahrenheit_to_celsius( temperature ) * 1000 ) );
}

template<typename value_type>
constexpr value_type to_millidegree_celsius( const
        quantity<value_type, temperature_in_millidegree_celsius_tag> &v )
{
    return v / from_millidegree_celsius<value_type>( 1 );
}

template<typename value_type>
constexpr value_type to_celsius( const
                                 quantity<value_type, temperature_in_millidegree_celsius_tag> &v )
{
    return to_millidegree_celsius( v ) / 1000.0;
}

template<typename value_type>
constexpr value_type to_fahrenheit( const
                                    quantity<value_type, temperature_in_millidegree_celsius_tag> &v )
{
    return celsius_to_fahrenheit( to_millidegree_celsius( v ) / 1000.0 );
}

template<typename value_type>
constexpr value_type to_kelvins( const
                                 quantity<value_type, temperature_in_millidegree_celsius_tag> &v )
{
    return celsius_to_kelvin( to_millidegree_celsius( v ) / 1000.0 );
}

} // namespace units

constexpr units::temperature operator""_mc( const unsigned long long v )
{
    // Cast to int because fahrenheit conversion needs it
    // Rest gets it for consistency
    return units::from_millidegree_celsius<int>( v );
}

constexpr units::temperature operator""_c( const unsigned long long v )
{
    return units::from_celsius<int>( v );
}

constexpr units::temperature operator""_c( const long double v )
{
    return units::from_celsius<double>( static_cast<double>( v ) );
}

constexpr units::temperature operator""_f( const unsigned long long v )
{
    return units::from_fahrenheit<int>( v );
}


