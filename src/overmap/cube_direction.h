#pragma once

#include "enum_traits.h"

#include <functional>

struct tripoint;

namespace om_direction {
enum class type : int;
} // namespace om_direction

// We have other direction enums, but for this purpose we need to have one for
// the six rectilinear directions.  These correspond to the faces of a cube, so
// I've called it cube_direction
enum class cube_direction : int { north, east, south, west, above, below, last };

template <> struct enum_traits<cube_direction> {
    static constexpr cube_direction last = cube_direction::last;
};

namespace std {
template <> struct hash<cube_direction> {
    auto operator()(const cube_direction& d) const -> std::size_t {
        return static_cast<std::size_t>(d);
    }
};
} // namespace std

auto operator+(cube_direction, om_direction::type) -> cube_direction;
auto operator+(cube_direction, int i) -> cube_direction;
auto operator-(cube_direction, om_direction::type) -> cube_direction;
auto operator-(cube_direction, int i) -> cube_direction;

auto displace(cube_direction d) -> tripoint;
