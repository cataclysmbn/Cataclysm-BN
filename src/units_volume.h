#pragma once

#include <algorithm>

#include "units_def.h"

namespace units
{

class volume_in_milliliter_tag
{
};

using volume = quantity<int, volume_in_milliliter_tag>;

const volume volume_min = units::volume( std::numeric_limits<units::volume::value_type>::min(),
                          units::volume::unit_type{} );

const volume volume_max = units::volume( std::numeric_limits<units::volume::value_type>::max(),
                          units::volume::unit_type{} );

template<typename value_type>
constexpr quantity<value_type, volume_in_milliliter_tag> from_milliliter(
    const value_type v )
{
    return quantity<value_type, volume_in_milliliter_tag>( v, volume_in_milliliter_tag{} );
}

template<typename value_type>
constexpr quantity<value_type, volume_in_milliliter_tag> from_liter( const value_type v )
{
    return from_milliliter<value_type>( v * 1000 );
}

template<typename value_type>
constexpr value_type to_milliliter( const quantity<value_type, volume_in_milliliter_tag> &v )
{
    return v / from_milliliter<value_type>( 1 );
}

constexpr double to_liter( const volume &v )
{
    return v.value() / 1000.0;
}

// Legacy conversions factor for old volume values.
// Don't use in new code! Use one of the from_* functions instead.
static constexpr volume legacy_volume_factor = from_milliliter( 250 );

} // namespace units

// Implicitly converted to volume, which has int as value_type!
constexpr units::volume operator""_ml( const unsigned long long v )
{
    return units::from_milliliter( v );
}

constexpr units::quantity<double, units::volume_in_milliliter_tag> operator""_ml(
    const long double v )
{
    return units::from_milliliter( v );
}

// Implicitly converted to volume, which has int as value_type!
constexpr units::volume operator""_liter( const unsigned long long v )
{
    return units::from_milliliter( v * 1000 );
}

constexpr units::quantity<double, units::volume_in_milliliter_tag> operator""_liter(
    const long double v )
{
    return units::from_milliliter( v * 1000 );
}


