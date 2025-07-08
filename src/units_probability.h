#pragma once

#include "units_def.h"

namespace units
{

class probability_in_one_in_million_tag
{
};

using probability = quantity<int, probability_in_one_in_million_tag>;

const probability probability_min = units::probability(
                                        0,
                                        units::probability::unit_type{} );

const probability probability_max = units::probability(
                                        1000000,
                                        units::probability::unit_type{} );

template<typename value_type>
constexpr quantity<value_type, probability_in_one_in_million_tag>
from_one_in_million(
    const value_type v )
{
    return quantity<value_type, probability_in_one_in_million_tag>( v,
            probability_in_one_in_million_tag{} );
}

template<typename value_type>
constexpr quantity<value_type, probability_in_one_in_million_tag>
from_percent(
    const value_type v )
{
    return quantity<value_type, probability_in_one_in_million_tag>( 10000 * v,
            probability_in_one_in_million_tag{} );
}

template<typename value_type>
constexpr value_type to_one_in_million( const
                                        quantity<value_type, probability_in_one_in_million_tag> &v )
{
    return v / from_one_in_million<value_type>( 1 );
}

} // namespace units

constexpr units::probability operator""_pm( const unsigned long long v )
{
    return units::from_one_in_million<int>( v );
}

constexpr units::probability operator""_pct( const unsigned long long v )
{
    return units::from_one_in_million<int>( v * 10000 );
}

constexpr units::probability operator""_pct( const long double v )
{
    return units::from_one_in_million<int>( v * 10000 );
}



