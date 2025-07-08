#pragma once

#include "units.h"

#include <type_traits>

/**
 * Type of object that a measurement is taken on.  Used, for example, to display wind speed in m/s
 * while displaying vehicle speed in km/h.
 */
enum units_type {
    VU_VEHICLE,
    VU_WIND
};

/** Divide @p num by @p den, rounding up
 *
 * @p num must be non-negative, @p den must be positive, and @c num+den must not overflow.
 */
template<typename T, typename U>
T divide_round_up( units::quantity<T, U> num, units::quantity<T, U> den )
{
    T n = num.value();
    T d = den.value();
    return ( n + d - 1 ) / d;
}

/** Given an angle, add or subtract multiples of 360_degrees until it's in the
 * range [0, 360_degrees).
 *
 * With a second argument, can use a different maximum.
 */
units::angle normalize( units::angle, units::angle mod = 360_degrees );

template<typename T, typename U>
units::quantity<T, U> round_to_multiple_of( units::quantity<T, U> val, units::quantity<T, U> of )
requires std::is_floating_point_v<T> {
    int multiple = std::lround( val / of );
    return multiple * of;
}

/**
 * Create a units label for a velocity value.
 *
 * Gives the name of the velocity unit in the user selected unit system, either
 * "km/h", "ms/s" or "mph".  Used to add abbreviated unit labels to the output of
 * @ref convert_velocity.
 *
 * @param vel_units type of velocity desired (i.e. wind or vehicle)
 *
 * @return name of unit.
 */
const char *velocity_units( units_type vel_units );

/**
 * Create a units label for a weight value.
 *
 * Gives the name of the weight unit in the user selected unit system, either
 * "kgs" or "lbs".  Used to add unit labels to the output of @ref convert_weight.
 *
 * @return name of unit
 */
const char *weight_units();

/**
 * Create an abbreviated units label for a volume value.
 *
 * Returns the abbreviated name for the volume unit for the user selected unit system,
 * i.e. "c", "L", or "qt". Used to add unit labels to the output of @ref convert_volume.
 *
 * @return name of unit.
 */
const char *volume_units_abbr();

/**
 * Create a units label for a volume value.
 *
 * Returns the abbreviated name for the volume unit for the user selected unit system,
 * ie "cup", "liter", or "quart". Used to add unit labels to the output of @ref convert_volume.
 *
 * @return name of unit.
 */
const char *volume_units_long();

/**
 * Convert internal velocity units to units defined by user.
 *
 * @param velocity A velocity value in internal units.
 * @param vel_units General type of item this velocity is for (e.g. vehicles or wind)
 *
 * @returns Velocity in the user selected measurement system and in appropriate
 *          units for the object being measured.
 */
double convert_velocity( int velocity, units_type vel_units );

/**
 * Convert weight in grams to units defined by user (kg or lbs)
 *
 * @param weight to be converted.
 *
 * @returns Weight converted to user selected unit
 */
double convert_weight( const units::mass &weight );

/**
 * Convert volume from ml to units defined by user.
 */
double convert_volume( int volume );

/**
 * Convert volume from ml to units defined by user,
 * optionally returning the units preferred scale.
 */
double convert_volume( int volume, int *out_scale );


