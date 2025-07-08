#pragma once

#include <cstdint>
#include <algorithm>

#include "units_def.h"

// yeah this is very cursed position for constant
// but if it's on game_constants.h it causes circular dependency
// TODO: move this to constants_mass or something
// 9.81m/(s^2) or newtons per kilogram
constexpr double GRAVITY_OF_EARTH = 9.81;

namespace units
{

class mass_in_milligram_tag
{
};

using mass = quantity<std::int64_t, mass_in_milligram_tag>;

constexpr mass mass_min = units::mass( std::numeric_limits<units::mass::value_type>::min(),
                                       units::mass::unit_type{} );

constexpr mass mass_max = units::mass( std::numeric_limits<units::mass::value_type>::max(),
                                       units::mass::unit_type{} );



template<typename value_type>
constexpr quantity<value_type, mass_in_milligram_tag> from_milligram(
    const value_type v )
{
    return quantity<value_type, mass_in_milligram_tag>( v, mass_in_milligram_tag{} );
}

template<typename value_type>
constexpr quantity<value_type, mass_in_milligram_tag> from_gram(
    const value_type v )
{
    return from_milligram( v * 1000 );
}

template<typename value_type>
constexpr quantity<value_type, mass_in_milligram_tag> from_kilogram(
    const value_type v )
{
    return from_gram( v * 1000 );
}

template<typename value_type>
constexpr quantity<value_type, mass_in_milligram_tag> from_newton(
    const value_type v )
{
    return from_kilogram( v / GRAVITY_OF_EARTH );
}

template<typename value_type>
constexpr value_type to_milligram( const quantity<value_type, mass_in_milligram_tag> &v )
{
    return v.value();
}

template<typename value_type>
constexpr value_type to_gram( const quantity<value_type, mass_in_milligram_tag> &v )
{
    return v.value() / 1000.0;
}

template<typename value_type>
constexpr value_type to_kilogram( const quantity<value_type, mass_in_milligram_tag> &v )
{
    return v.value() / 1000000.0;
}

template<typename value_type>
constexpr value_type to_newton( const quantity<value_type, mass_in_milligram_tag> &v )
{
    return to_kilogram( v ) * GRAVITY_OF_EARTH;
}

} // namespace units

// Implicitly converted to mass, which has int as value_type!
constexpr units::mass operator""_milligram( const unsigned long long v )
{
    return units::from_milligram( v );
}

constexpr units::mass operator""_gram( const unsigned long long v )
{
    return units::from_gram( v );
}

constexpr units::mass operator""_kilogram( const unsigned long long v )
{
    return units::from_kilogram( v );
}

constexpr units::mass operator""_newton( const unsigned long long v )
{
    return units::from_newton( v );
}

constexpr units::quantity<double, units::mass_in_milligram_tag> operator""_milligram(
    const long double v )
{
    return units::from_milligram( v );
}

constexpr units::quantity<double, units::mass_in_milligram_tag> operator""_gram(
    const long double v )
{
    return units::from_gram( v );
}

constexpr units::quantity<double, units::mass_in_milligram_tag> operator""_kilogram(
    const long double v )
{
    return units::from_kilogram( v );
}

constexpr units::quantity<double, units::mass_in_milligram_tag> operator""_newton(
    const long double v )
{
    return units::from_newton( v );
}




