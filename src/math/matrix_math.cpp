#include "math/matrix_math.h"

namespace matrices {

auto rotation_z_axis(units::angle angle) -> matrix_3d {
    return matrix_3d({cos(angle), -sin(angle), 0, sin(angle), cos(angle), 0, 0, 0, 1});
}

} // namespace matrices
