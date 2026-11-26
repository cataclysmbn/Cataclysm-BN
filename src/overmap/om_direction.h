#pragma once

#include "coordinates.h"
#include "cube_direction.h"

#include <array>
#include <climits>
#include <cstdint>
#include <string>

/** Direction on the overmap. */
namespace om_direction {
/** Basic enum for directions. */
enum class type : int { invalid = -1, none, north = none, east, south, west, last };

/** For the purposes of iteration. */
const std::array<type, 4> all = {{type::north, type::east, type::south, type::west}};
const size_t size = all.size();

const std::array<std::string, 4> all_suffixes = {{"_north", "_east", "_south", "_west"}};
const std::string invalid_dir_suffix;
const std::array<int, 4> all_cw_rotations = {{0, 1, 2, 3}};
const std::array<int, 4> all_ccw_rotations = {{0, 3, 2, 1}};
const int invalid_dir_rotations = 0;

/** Returns directional suffix associated with the value, e.g. _north or _west. */
constexpr auto get_suffix(type dir) -> const std::string& {
    if (dir == type::invalid) {
        return invalid_dir_suffix;
    } else {
        return all_suffixes[static_cast<size_t>(dir)];
    }
}

/** Returns number of clockwise rotations needed to reach this direction from 'north'. */
constexpr auto get_num_cw_rotations(type dir) -> int {
    if (dir == type::invalid) {
        return invalid_dir_rotations;
    } else {
        return all_cw_rotations[static_cast<size_t>(dir)];
    }
}

/** Returns number of counterclockwise rotations needed to reach this direction from 'north'. */
constexpr auto get_num_ccw_rotations(type dir) -> int {
    if (dir == type::invalid) {
        return invalid_dir_rotations;
    } else {
        return all_ccw_rotations[static_cast<size_t>(dir)];
    }
}

/** Number of bits needed to store directions. */
const size_t bits = static_cast<size_t>(-1) >> (CHAR_BIT * sizeof(size_t) - size);

/** Get Human readable name of a direction */
auto name(type dir) -> std::string;

/** Various rotations. */
auto rotate(point p, type dir) -> point;
auto rotate(const tripoint& p, type dir) -> tripoint;
template <typename Point, coords::scale Scale>
auto rotate(const coords::coord_point<Point, coords::origin::relative, Scale>& p, type dir)
    -> coords::coord_point<Point, coords::origin::relative, Scale> {
    return coords::coord_point<Point, coords::origin::relative, Scale>{rotate(p.raw(), dir)};
}
auto rotate_symbol(uint32_t sym, type dir) -> uint32_t;

/** Returns point(0, 0) displaced in specified direction by a specified distance
 * @param dir Direction of displacement
 * @param dist Distance of displacement
 */
auto displace(type dir, int dist = 1) -> point;

/** Returns a sum of two numbers
 *  @param dir1 first number
 *  @param dir2 second number */
auto add(type dir1, type dir2) -> type;

/** Turn by 90 degrees to the left, to the right, or randomly (either left or right). */
auto turn_left(type dir) -> type;
auto turn_right(type dir) -> type;
auto turn_random(type dir) -> type;

/** Returns an opposite direction. */
auto opposite(type dir) -> type;

/** Returns a random direction. */
auto random() -> type;

/** Whether these directions are parallel. */
auto are_parallel(type dir1, type dir2) -> bool;

auto from_cube(cube_direction, const std::string& error_msg) -> type;

} // namespace om_direction

template <> struct enum_traits<om_direction::type> {
    static constexpr om_direction::type last = om_direction::type::last;
};
