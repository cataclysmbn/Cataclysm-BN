#pragma once
#ifndef CATA_TESTS_SIMULATED_ISLAND_HELPERS_H
#define CATA_TESTS_SIMULATED_ISLAND_HELPERS_H

#include <unordered_set>

#include "coordinates.h"
#include "game.h"
#include "map.h"
#include "mapbuffer.h"
#include "options.h"

/**
 * Ensure the mapbuffer has simulated islands for the given absolute position.
 * This creates simulated islands spanning the reality bubble around @p center,
 * mirroring what submap_load_manager::update() would normally do during gameplay.
 */
inline void ensure_simulated_islands_for( const tripoint_abs_ms &center )
{
    map &m = get_map();
    mapbuffer &mb = m.get_mapbuffer();

    const int half_mapsize = g_half_mapsize;
    const tripoint_abs_sm center_sm = project_to<coords::sm>( center );
    const point_rel_sm half( half_mapsize, half_mapsize );

    std::unordered_set<point_abs_sm> columns;
    for( int dx = -half.x(); dx <= half.x(); ++dx ) {
        for( int dy = -half.y(); dy <= half.y(); ++dy ) {
            columns.insert( center_sm.xy() + point_rel_sm( dx, dy ) );
        }
    }

    mb.set_simulated_submaps( columns );
}

#endif // CATA_TESTS_SIMULATED_ISLAND_HELPERS_H
