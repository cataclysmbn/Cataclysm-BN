#include "dimension_bounds.h"

#include "map.h"

// Number of map squares per submap
static constexpr int SEEX_SIZE = SEEX;
static constexpr int SEEY_SIZE = SEEY;

bool dimension_bounds::contains( const tripoint_abs_sm &p ) const
{
    return p.x() >= min_bound.x() && p.x() <= max_bound.x() &&
           p.y() >= min_bound.y() && p.y() <= max_bound.y() &&
           p.z() >= min_bound.z() && p.z() <= max_bound.z();
}

bool dimension_bounds::contains_ms( const tripoint_abs_ms &p ) const
{
    // Convert map square coordinates to submap coordinates
    // Each submap is SEEX * SEEY map squares
    tripoint_abs_sm sm_pos( p.x() / SEEX_SIZE, p.y() / SEEY_SIZE, p.z() );
    return contains( sm_pos );
}

bool dimension_bounds::contains_omt( const tripoint_abs_omt &p ) const
{
    // Convert OMT to submap: each OMT spans 2 submaps in x and y
    tripoint_abs_sm sm_pos( p.x() * 2, p.y() * 2, p.z() );
    return contains( sm_pos );
}

bool dimension_bounds::contains_local( const tripoint &p, const tripoint_abs_sm &map_origin ) const
{
    // Convert local map position to absolute submap coordinates
    // Local position is in map squares, origin is in submaps
    int abs_x = map_origin.x() * SEEX_SIZE + p.x;
    int abs_y = map_origin.y() * SEEY_SIZE + p.y;
    tripoint_abs_ms abs_ms( abs_x, abs_y, p.z );
    return contains_ms( abs_ms );
}
