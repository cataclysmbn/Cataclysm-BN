#pragma once

#include <cstdlib>

/// Vertical reach of the 3D field of vision in z-levels, cached from the `FOV_3D_Z_RANGE` option.
/// Levels farther than this from the vision origin are not computed for visibility or lightmap
/// regeneration.
extern int fov_3d_z_range;

/// Whether @p z is within @ref fov_3d_z_range levels of @p origin_z.
inline auto fov_z_in_range( const int z, const int origin_z ) -> bool { return std::abs( z - origin_z ) <= fov_3d_z_range; }
