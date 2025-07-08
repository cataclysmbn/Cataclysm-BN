#pragma once

#include <algorithm>

#include "units_def.h"

namespace units
{

class money_in_cent_tag
{
};

using money = quantity<int, money_in_cent_tag>;

const money money_min = units::money( std::numeric_limits<units::money::value_type>::min(),
                                      units::money::unit_type{} );

const money money_max = units::money( std::numeric_limits<units::money::value_type>::max(),
                                      units::money::unit_type{} );

template<typename value_type>
constexpr quantity<value_type, money_in_cent_tag> from_cent(
    const value_type v )
{
    return quantity<value_type, money_in_cent_tag>( v, money_in_cent_tag{} );
}

template<typename value_type>
constexpr quantity<value_type, money_in_cent_tag> from_usd( const value_type v )
{
    return from_cent<value_type>( v * 100 );
}

template<typename value_type>
constexpr quantity<value_type, money_in_cent_tag> from_kusd( const value_type v )
{
    return from_usd<value_type>( v * 1000 );
}

template<typename value_type>
constexpr value_type to_cent( const quantity<value_type, money_in_cent_tag> &v )
{
    return v / from_cent<value_type>( 1 );
}

template<typename value_type>
constexpr value_type to_usd( const quantity<value_type, money_in_cent_tag> &v )
{
    return to_cent( v ) / 100.0;
}

template<typename value_type>
constexpr value_type to_kusd( const quantity<value_type, money_in_cent_tag> &v )
{
    return to_usd( v ) / 1000.0;
}

} // namespace units

constexpr units::money operator""_cent( const unsigned long long v )
{
    return units::from_cent( v );
}

constexpr units::quantity<double, units::money_in_cent_tag> operator""_cent(
    const long double v )
{
    return units::from_cent( v );
}

constexpr units::money operator""_USD( const unsigned long long v )
{
    return units::from_usd( v );
}

constexpr units::quantity<double, units::money_in_cent_tag> operator""_USD(
    const long double v )
{
    return units::from_usd( v );
}

constexpr units::money operator""_kUSD( const unsigned long long v )
{
    return units::from_kusd( v );
}

constexpr units::quantity<double, units::money_in_cent_tag> operator""_kUSD(
    const long double v )
{
    return units::from_kusd( v );
}


