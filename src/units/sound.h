#pragma once

#include "units_def.h"

#include <limits>

namespace units {

class sound_in_decibel_tag {};

using sound = quantity<int, sound_in_decibel_tag>;

constexpr auto sound_min =
    units::sound(std::numeric_limits<units::sound::value_type>::min(), units::sound::unit_type{});
constexpr auto sound_max =
    units::sound(std::numeric_limits<units::sound::value_type>::max(), units::sound::unit_type{});

template <typename value_type>
constexpr auto from_decibel(const value_type v) -> quantity<value_type, sound_in_decibel_tag> {
    return quantity<value_type, sound_in_decibel_tag>(v, sound_in_decibel_tag{});
}

template <typename value_type>
constexpr auto to_decibel(const quantity<value_type, sound_in_decibel_tag>& v) -> value_type {
    return v.value();
}

} // namespace units

constexpr auto operator""_dB(const unsigned long long v) -> units::sound {
    return units::from_decibel(v);
}

constexpr auto operator""_dB(const long double v)
    -> units::quantity<double, units::sound_in_decibel_tag> {
    return units::from_decibel(v);
}
