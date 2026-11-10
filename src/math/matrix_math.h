#pragma once

#include "math/point_traits.h"
#include "units_angle.h"

#include <array>
#include <cmath>

template <size_t w, size_t h, typename T = float> struct matrix {
private:
    static constexpr size_t n = w * h;
    std::array<T, n> data;

public:
    constexpr matrix(const std::array<T, n>& m): data(m) {}

    template <typename Vec, typename Traits = point_traits<Vec>>
    friend constexpr auto operator*(const matrix& m, const Vec& v) -> Vec {
        // TODO: std::get equivalent for point_traits?
        static_assert(Vec::dimension == 3, "Currently only vectors of dimension 3 are supported");
        static_assert(Vec::dimension == w, "Vector dimension must match matrix width");
        Vec result(0.0, 0.0, 0.0);
        for (size_t i = 0; i < h; i++) {
            for (size_t j = 0; j < w; j++) {
                Traits::at(result, i) += m.at(j, i) * Traits::at(v, j);
            }
        }
        return result;
    }

    // NOLINTNEXTLINE(cata-xy): We don't want point dependence in this .h
    constexpr auto at(size_t x, size_t y) const -> const T& { return data.at(y * w + x); }
};

using matrix_3d = matrix<3u, 3u, double>;

namespace matrices {

auto rotation_z_axis(units::angle angle) -> matrix_3d;

} // namespace matrices
