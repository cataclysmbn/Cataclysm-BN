#include "math/matrix_math.h"

namespace matrices {

matrix_3d rotation_z_axis(units::angle angle) {
    return matrix_3d({cos(angle), -sin(angle), 0, sin(angle), cos(angle), 0, 0, 0, 1});
}

} // namespace matrices
