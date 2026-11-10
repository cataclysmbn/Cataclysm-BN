#include "point.h"

#include "cata_utility.h"

#include <algorithm>
#include <sstream>
#include <utility>


auto point::rotate(int turns, point dim) const -> point {
    assert(turns >= 0);
    assert(turns <= 4);

    switch (turns) {
        case 1:
            return {dim.y - y - 1, x};
        case 2:
            return {dim.x - x - 1, dim.y - y - 1};
        case 3:
            return {y, dim.x - x - 1};
    }

    return *this;
}

auto point::to_string() const -> std::string {
    std::ostringstream os;
    os << *this;
    return os.str();
}

auto tripoint::to_string() const -> std::string {
    std::ostringstream os;
    os << *this;
    return os.str();
}

auto operator<<(std::ostream& os, point pos) -> std::ostream& {
    return os << "(" << pos.x << "," << pos.y << ")";
}

auto operator<<(std::ostream& os, const tripoint& pos) -> std::ostream& {
    return os << "(" << pos.x << "," << pos.y << "," << pos.z << ")";
}

namespace detail {
spiral_generator_impl::spiral_generator_impl(point center, int min_dist, int max_dist)
    : center(center),
      min_dist(min_dist),
      max_dist(max_dist),
      min_edge(min_dist * 2 + 1),
      max_edge(max_dist * 2 + 1),
      n((max_edge * max_edge) - (min_edge - 2) * (min_edge - 2)),
      is_center_included(min_dist == 0) {
    i = is_center_included ? -1 : 0;
    x = std::max(min_dist, 1);
    y = 1 - x;
    dx = 1;
    dy = 0;

    p = is_center_included ? center : center + point{x, y};
}

auto spiral_generator_impl::exhausted(point center, int min_dist, int max_dist)
    -> spiral_generator_impl {
    auto q = spiral_generator_impl(center, min_dist, max_dist);
    q.i = q.n;
    return q;
}

spiral_generator_impl::operator bool() const noexcept { return i < n; }

auto spiral_generator_impl::next() -> bool {
    if (i >= n) { return false; }

    if (i++ < 0) {
        p = center + point{x, y};
        return true;
    }

    if (x == y || (x < 0 && x == -y) || (x > 0 && x == 1 - y)) {
        std::swap(dx, dy);
        dx = -dx;
    }

    x += dx;
    y += dy;

    p = center + point{x, y};

    return true;
}

auto spiral_generator_impl::current() const -> const point& { return p; }

auto spiral_generator_impl::operator==(const spiral_generator_impl& other) const -> bool {
    return i == other.i && center == other.center && min_dist == other.min_dist
        && max_dist == other.max_dist;
}
} // namespace detail
