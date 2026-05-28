#pragma once

#include "units_temperature.h"

#include <cstdlib>
#include <limits>

namespace units {

class temperature_delta_in_millidegree_celsius_tag {};

using temperature_delta = quantity<int, temperature_delta_in_millidegree_celsius_tag>;

const auto temperature_delta_min = units::temperature_delta(
    std::numeric_limits<units::temperature_delta::value_type>::min(),
    units::temperature_delta::unit_type{});

const auto temperature_delta_max = units::temperature_delta(
    std::numeric_limits<units::temperature_delta::value_type>::max(),
    units::temperature_delta::unit_type{});

template <typename value_type>
constexpr auto from_millidegree_celsius_delta(const value_type v)
    -> quantity<value_type, temperature_delta_in_millidegree_celsius_tag> {
    return quantity<value_type, temperature_delta_in_millidegree_celsius_tag>(
        v, temperature_delta_in_millidegree_celsius_tag{});
}

template <typename value_type>
constexpr auto from_celsius_delta(const value_type v)
    -> quantity<value_type, temperature_delta_in_millidegree_celsius_tag> {
    return from_millidegree_celsius_delta<value_type>(v * 1000);
}

template <typename value_type>
constexpr auto from_fahrenheit_delta(const value_type v)
    -> quantity<value_type, temperature_delta_in_millidegree_celsius_tag> {
    return from_celsius_delta<value_type>(v * 5.0 / 9.0);
}

template <typename value_type>
constexpr auto to_millidegree_celsius_delta(
    const quantity<value_type, temperature_delta_in_millidegree_celsius_tag>& v) -> value_type {
    return v / from_millidegree_celsius_delta<value_type>(1);
}

template <typename value_type>
constexpr auto to_celsius_delta(
    const quantity<value_type, temperature_delta_in_millidegree_celsius_tag>& v) -> value_type {
    return to_millidegree_celsius_delta(v) / 1000.0;
}

template <typename value_type>
constexpr auto to_fahrenheit_delta(
    const quantity<value_type, temperature_delta_in_millidegree_celsius_tag>& v) -> value_type {
    return to_celsius_delta(v) * 9.0 / 5.0;
}

template <typename value_type>
constexpr auto from_legacy_bodypart_temp(const value_type temp) -> units::temperature {
    return units::from_celsius(37.0 + (temp - 5000.0) * 0.002);
}

template <typename value_type>
constexpr auto to_legacy_bodypart_temp(
    const quantity<value_type, temperature_in_millidegree_celsius_tag>& v) -> int {
    return static_cast<int>(5000 + (units::to_celsius(v) - 37.0) * 500.0);
}

template <typename value_type>
constexpr auto from_legacy_bodypart_temp_delta(const value_type temp) -> units::temperature_delta {
    return units::from_celsius_delta(temp * 0.002);
}

template <typename value_type>
constexpr auto to_legacy_bodypart_temp_delta(
    const quantity<value_type, temperature_delta_in_millidegree_celsius_tag>& v) -> int {
    return static_cast<int>(units::to_celsius_delta(v) * 500.0);
}

constexpr inline auto abs(units::temperature_delta x) -> units::temperature_delta {
    return units::from_millidegree_celsius_delta(std::abs(units::to_millidegree_celsius_delta(x)));
}

} // namespace units

constexpr auto operator""_c_delta(const unsigned long long v) -> units::temperature_delta {
    return units::from_celsius_delta<int>(v);
}

constexpr auto operator""_c_delta(const long double v) -> units::temperature_delta {
    return units::from_celsius_delta<double>(static_cast<double>(v));
}

constexpr auto operator""_f_delta(const unsigned long long v) -> units::temperature_delta {
    return units::from_fahrenheit_delta<int>(v);
}

constexpr auto operator-(const units::temperature& lhs, const units::temperature& rhs)
    -> units::temperature_delta {
    return units::from_millidegree_celsius_delta(
        units::to_millidegree_celsius(lhs) - units::to_millidegree_celsius(rhs));
}

constexpr auto operator+(const units::temperature& lhs, const units::temperature_delta& rhs)
    -> units::temperature {
    return units::from_millidegree_celsius(
        units::to_millidegree_celsius(lhs) + units::to_millidegree_celsius_delta(rhs));
}

constexpr auto operator+(const units::temperature_delta& lhs, const units::temperature& rhs)
    -> units::temperature {
    return rhs + lhs;
}

constexpr auto operator-(const units::temperature& lhs, const units::temperature_delta& rhs)
    -> units::temperature {
    return units::from_millidegree_celsius(
        units::to_millidegree_celsius(lhs) - units::to_millidegree_celsius_delta(rhs));
}
