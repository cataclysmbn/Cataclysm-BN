#pragma once

#include <algorithm>

#include "units_def.h"

namespace units
{

class energy_in_joule_tag
{
};

using energy = quantity<int, energy_in_joule_tag>;

const energy energy_min = units::energy( std::numeric_limits<units::energy::value_type>::min(),
                          units::energy::unit_type{} );

const energy energy_max = units::energy( std::numeric_limits<units::energy::value_type>::max(),
                          units::energy::unit_type{} );

template<typename value_type>
constexpr quantity<value_type, energy_in_joule_tag> from_joule(
    const value_type v )
{
    return quantity<value_type, energy_in_joule_tag>( v, energy_in_joule_tag{} );
}

template<typename value_type>
constexpr quantity<value_type, energy_in_joule_tag> from_kilojoule( const value_type v )
{
    const value_type max_energy_joules = std::numeric_limits<value_type>::max() / 1000;
    value_type energy = v * 1000 > max_energy_joules ? max_energy_joules : v * 1000;
    return from_joule<value_type>( energy );
}

template<typename value_type>
constexpr value_type to_joule( const quantity<value_type, energy_in_joule_tag> &v )
{
    return v / from_joule<value_type>( 1 );
}

template<typename value_type>
constexpr value_type to_kilojoule( const quantity<value_type, energy_in_joule_tag> &v )
{
    return to_joule( v ) / 1000.0;
}

} // namespace units

constexpr units::energy operator""_J( const unsigned long long v )
{
    return units::from_joule( v );
}

constexpr units::quantity<double, units::energy_in_joule_tag> operator""_J(
    const long double v )
{
    return units::from_joule( v );
}

constexpr units::energy operator""_kJ( const unsigned long long v )
{
    return units::from_kilojoule( v );
}

constexpr units::quantity<double, units::energy_in_joule_tag> operator""_kJ(
    const long double v )
{
    return units::from_kilojoule( v );
}


