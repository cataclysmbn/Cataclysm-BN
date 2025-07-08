#pragma once

#include "point.h"
#include "coordinates.h"

/**
 * Rotate point clockwise @param turns times, 90 degrees per turn,
 * around the center of a rectangle with the dimensions specified
 * by @param dim
 * set dim to (0, 0) to rotate around the origin.
 */
auto rotate( point p, point dim, int turns ) -> point;

/**
 * Rotates just the x,y component of the tripoint. See point::rotate()
 */
auto rotate( const tripoint &p, point dim, int turns ) -> tripoint;

/** works like rotate but for submaps. */
auto rotate_point_sm( const tripoint &p, const tripoint &orig, int turns ) -> tripoint;

auto get_rot_turns( const tripoint_abs_omt &here, const tripoint_abs_omt &there ) -> int;


