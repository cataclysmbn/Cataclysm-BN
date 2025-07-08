#pragma once

#include <functional>

struct point;
struct rl_vec2d;

void draw_line( const std::function<void( point )> &set, point p1, point p2 );

void draw_square( const std::function<void( point )> &set, point p1, point p2 );

void draw_rough_circle( const std::function<void( point )> &set, point p, int rad );

void draw_circle( const std::function<void( point )> &set, const rl_vec2d &p, double rad );

void draw_circle( const std::function<void( point )> &set, point p, int rad );


