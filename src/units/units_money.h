#pragma once

#include "units_def.h"

#include <algorithm>

namespace units {

class money_in_cent_tag {};

using money = quantity<int, money_in_cent_tag>;

const money money_min =
    units::money(std::numeric_limits<units::money::value_type>::min(), units::money::unit_type{});

const money money_max =
    units::money(std::numeric_limits<units::money::value_type>::max(), units::money::unit_type{});

template <typename value_type>
constexpr auto from_cent(const value_type v) -> quantity<value_type, money_in_cent_tag> {
    return quantity<value_type, money_in_cent_tag>(v, money_in_cent_tag{});
}

template <typename value_type>
constexpr auto from_usd(const value_type v) -> quantity<value_type, money_in_cent_tag> {
    return from_cent<value_type>(v * 100);
}

template <typename value_type>
constexpr auto from_kusd(const value_type v) -> quantity<value_type, money_in_cent_tag> {
    return from_usd<value_type>(v * 1000);
}

template <typename value_type>
constexpr auto to_cent(const quantity<value_type, money_in_cent_tag>& v) -> value_type {
    return v / from_cent<value_type>(1);
}

template <typename value_type>
constexpr auto to_usd(const quantity<value_type, money_in_cent_tag>& v) -> value_type {
    return to_cent(v) / 100.0;
}

template <typename value_type>
constexpr auto to_kusd(const quantity<value_type, money_in_cent_tag>& v) -> value_type {
    return to_usd(v) / 1000.0;
}

} // namespace units

constexpr auto operator""_cent(const unsigned long long v) -> units::money {
    return units::from_cent(v);
}

constexpr auto operator""_cent(const long double v)
    -> units::quantity<double, units::money_in_cent_tag> {
    return units::from_cent(v);
}

constexpr auto operator""_USD(const unsigned long long v) -> units::money {
    return units::from_usd(v);
}

constexpr auto operator""_USD(const long double v)
    -> units::quantity<double, units::money_in_cent_tag> {
    return units::from_usd(v);
}

constexpr auto operator""_kUSD(const unsigned long long v) -> units::money {
    return units::from_kusd(v);
}

constexpr auto operator""_kUSD(const long double v)
    -> units::quantity<double, units::money_in_cent_tag> {
    return units::from_kusd(v);
}
