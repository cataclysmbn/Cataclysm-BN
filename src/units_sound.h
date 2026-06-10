#pragma once

#include <limits>

#include "units_def.h"

namespace units
{

class sound_in_decibel_tag
{
};

using sound = quantity<int, sound_in_decibel_tag>;

constexpr sound sound_min = units::sound( std::numeric_limits<units::sound::value_type>::min(),
                            units::sound::unit_type{} );

constexpr sound sound_max = units::sound( std::numeric_limits<units::sound::value_type>::max(),
                            units::sound::unit_type{} );

template<typename value_type>
constexpr quantity<value_type, sound_in_decibel_tag> from_decibel( const value_type v )
{
    return quantity<value_type, sound_in_decibel_tag>( v, sound_in_decibel_tag{} );
}

template<typename value_type>
constexpr value_type to_decibel( const quantity<value_type, sound_in_decibel_tag> &v )
{
    return v.value();
}

} // namespace units

constexpr units::sound operator""_dB( const unsigned long long v )
{
    return units::from_decibel( v );
}

constexpr units::quantity<double, units::sound_in_decibel_tag> operator""_dB( const long double v )
{
    return units::from_decibel( v );
}
