#pragma once

#include <type_traits>

struct point;
struct tripoint;
struct rl_vec2d;
struct rl_vec3d;

template <typename Point, typename = void> struct point_traits {
    static auto x(Point& p) -> int& { return p.x(); }
    static auto x(const Point& p) -> int { return p.x(); }
    static auto y(Point& p) -> int& { return p.y(); }
    static auto y(const Point& p) -> int { return p.y(); }
    static auto z(Point& p) -> int& { return p.z(); }
    static auto z(const Point& p) -> int { return p.z(); }
};

template <typename Point>
struct point_traits<
    Point, std::enable_if_t<std::is_same_v<Point, point> || std::is_same_v<Point, tripoint>>> {
    static auto x(Point& p) -> int& { return p.x; }
    static auto x(const Point& p) -> const int& { return p.x; }
    static auto y(Point& p) -> int& { return p.y; }
    static auto y(const Point& p) -> const int& { return p.y; }
    static auto z(Point& p) -> int& { return p.z; }
    static auto z(const Point& p) -> const int& { return p.z; }
};

template <typename Point>
struct point_traits<
    Point, std::enable_if_t<std::is_same_v<Point, rl_vec2d> || std::is_same_v<Point, rl_vec3d>>> {
    static auto x(Point& p) -> float& { return p.x; }
    static auto x(const Point& p) -> const float& { return p.x; }
    static auto y(Point& p) -> float& { return p.y; }
    static auto y(const Point& p) -> const float& { return p.y; }
    static auto z(Point& p) -> float& { return p.z; }
    static auto z(const Point& p) -> const float& { return p.z; }
    // TODO: Template
    constexpr static auto at(Point& p, std::size_t i) -> float& {
        switch (i) {
            case 0:
                return x(p);
            case 1:
                return y(p);
            case 2:
                return z(p);
        }
        // Template would make this impossible
        return x(p);
    }
    constexpr static auto at(const Point& p, std::size_t i) -> const float& {
        switch (i) {
            case 0:
                return x(p);
            case 1:
                return y(p);
            case 2:
                return z(p);
        }
        return x(p);
    }
};
