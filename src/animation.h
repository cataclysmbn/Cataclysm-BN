#pragma once

#include <list>
#include <map>
#include <vector>

#include "color.h"
#include "point.h"

enum explosion_neighbors {
    N_NO_NEIGHBORS = 0,
    N_NORTH = 1,

    N_SOUTH = 2,
    N_NS = 3,

    N_WEST = 4,
    N_NW = 5,
    N_SW = 6,
    N_NSW = 7,

    N_EAST = 8,
    N_NE = 9,
    N_SE = 10,
    N_NSE = 11,
    N_WE = 12,
    N_NWE = 13,
    N_SWE = 14,
    N_NSWE = 15
};

struct explosion_tile {
    explosion_neighbors neighborhood;
    nc_color color;
};

struct point_with_value {
    tripoint pt;
    double val;
};

using one_bucket = std::vector<point_with_value>;
using bucketed_points = std::list<one_bucket>;

namespace explosion_handler
{
void draw_explosion( const tripoint &p, int radius, const nc_color &col,
                     const std::string &exp_name );
void draw_custom_explosion( const tripoint &p, const std::map<tripoint, nc_color> &area,
                            const std::string &exp_name );
} // namespace explosion_handler

// TODO: Better file
bucketed_points bucket_by_distance( const tripoint &origin,
                                    const std::map<tripoint, double> &to_bucket );
bucketed_points optimal_bucketing( const bucketed_points &buckets, size_t max_buckets );

bool minimap_requires_animation();
bool terrain_requires_animation();

struct draw_sprite_line_options {
    const tripoint &p;
    const std::vector<tripoint> &points;
    /// Sprite for 0 degree angle (↑)
    std::string sprite;
    bool rotate = false;
};
/// Draw a line of sprites along the given points
void draw_line_of( const draw_sprite_line_options &options );
