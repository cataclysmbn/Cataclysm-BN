#pragma once

#include "coordinates.h"

#include <cstdlib>

/// Return whether two points share a four-way, same-z adjacency relationship.
template <typename Point>
auto is_cardinally_adjacent_2d(
    const Point& lhs, const Point& rhs, const bool include_origin = false) -> bool {
    if (lhs == rhs) { return include_origin; }
    return lhs.z() == rhs.z() && std::abs(lhs.x() - rhs.x()) + std::abs(lhs.y() - rhs.y()) == 1;
}

/// Return whether two points share a six-way, face-only 3D adjacency relationship.
template <typename Point>
auto is_cardinally_adjacent_3d(
    const Point& lhs, const Point& rhs, const bool include_origin = false) -> bool {
    if (lhs == rhs) { return include_origin; }
    return std::abs(lhs.x() - rhs.x()) + std::abs(lhs.y() - rhs.y()) + std::abs(lhs.z() - rhs.z())
        == 1;
}

/// Return whether two points are diagonally or cardinally adjacent on one z-level.
template <typename Point>
auto is_adjacent_8_way_2d(const Point& lhs, const Point& rhs, const bool include_origin = false)
    -> bool {
    if (lhs == rhs) { return include_origin; }
    return lhs.z() == rhs.z() && std::abs(lhs.x() - rhs.x()) <= 1
        && std::abs(lhs.y() - rhs.y()) <= 1;
}

/// Return whether two points differ on at most two axes by one tile each.
template <typename Point>
auto is_face_or_edge_adjacent_3d(
    const Point& lhs, const Point& rhs, const bool include_origin = false) -> bool {
    if (lhs == rhs) { return include_origin; }
    const auto dx = std::abs(lhs.x() - rhs.x());
    const auto dy = std::abs(lhs.y() - rhs.y());
    const auto dz = std::abs(lhs.z() - rhs.z());
    return dx <= 1 && dy <= 1 && dz <= 1 && (dx == 0 || dy == 0 || dz == 0);
}

/// Return whether two points are within the inclusive 3x3x3 cube around each other.
template <typename Point>
auto is_within_cube_3d(const Point& lhs, const Point& rhs, const bool include_origin = false)
    -> bool {
    if (lhs == rhs) { return include_origin; }
    return std::abs(lhs.x() - rhs.x()) <= 1 && std::abs(lhs.y() - rhs.y()) <= 1
        && std::abs(lhs.z() - rhs.z()) <= 1;
}

/// Return whether two points are within Manhattan distance two on one z-level.
template <typename Point>
auto is_within_manhattan_distance_2_2d(
    const Point& lhs, const Point& rhs, const bool include_origin = false) -> bool {
    if (lhs == rhs) { return include_origin; }
    return lhs.z() == rhs.z() && std::abs(lhs.x() - rhs.x()) + std::abs(lhs.y() - rhs.y()) <= 2;
}

/// Return whether two points are within 3D Manhattan distance two.
template <typename Point>
auto is_within_manhattan_distance_2_3d(
    const Point& lhs, const Point& rhs, const bool include_origin = false) -> bool {
    if (lhs == rhs) { return include_origin; }
    return std::abs(lhs.x() - rhs.x()) + std::abs(lhs.y() - rhs.y()) + std::abs(lhs.z() - rhs.z())
        <= 2;
}
