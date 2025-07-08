#pragma once

#include "game_constants.h"
#include "point.h"

/**
 * This file defines legacy coordinate conversion functions.  We should be
 * migrating to the new functions defined in coordinates.h.
 *
 * For documentation on coordinate systems in general see
 * doc/POINTS_COORDINATES.md.
 *
 * TODO: type for mmr coords, also add them to doc/POINTS_COORDINATES.md
 * memory map region (mmr): Memory map region is a unit of tile memory saved to a directory.
 * Each region contains MM_REG_SIZExMM_REG_SIZE memorized submaps, and is used only for
 * saving/loading memorized submaps, see map_memory.cpp.
 * Translation from sm to mmr:
 * sm.x /= MM_REG_SIZE
 * sm.y /= MM_REG_SIZE
 * (with special handling for negative values).
 *
 * This file provides static translation functions, named like this:
@code
    static point <from>_to_<to>_copy(int x, int y);
    static point <from>_to_<to>_copy(const point& p);
    static tripoint <from>_to_<to>_copy(const tripoint& p);
    static void <from>_to_<to>(int &x, int &y);
    static void <from>_to_<to>(point& p);
    static void <from>_to_<to>(tripoint& p);
    static point <from>_to_<to>_remain(int &x, int &y);
    static point <from>_to_<to>_remain(point& p);
@endcode
 * Functions ending with _copy return the translated coordinates,
 * other functions change the parameters itself and don't return anything.
 * Functions ending with _remain return the translated coordinates and
 * store the remainder in the parameters.
 */

// overmap terrain to overmap
auto omt_to_om_copy( point p ) -> point;

auto omt_to_om_copy( const tripoint &p ) -> tripoint;

void omt_to_om( int &x, int &y );

inline void omt_to_om( point &p )
{
    omt_to_om( p.x, p.y );
}

inline void omt_to_om( tripoint &p )
{
    omt_to_om( p.x, p.y );
}

auto omt_to_om_remain( int &x, int &y ) -> point;

inline auto omt_to_om_remain( point &p ) -> point
{
    return omt_to_om_remain( p.x, p.y );
}

// overmap to overmap terrain
auto om_to_omt_copy( point p ) -> point;

// submap to overmap terrain
auto sm_to_omt_copy( point p ) -> point;

auto sm_to_omt_copy( const tripoint &p ) -> tripoint;

void sm_to_omt( int &x, int &y );

inline void sm_to_omt( point &p )
{
    sm_to_omt( p.x, p.y );
}

inline void sm_to_omt( tripoint &p )
{
    sm_to_omt( p.x, p.y );
}

auto sm_to_omt_remain( int &x, int &y ) -> point;

inline auto sm_to_omt_remain( point &p ) -> point
{
    return sm_to_omt_remain( p.x, p.y );
}

// submap to overmap, basically: x / (OMAPX * 2)
auto sm_to_om_copy( point p ) -> point;

auto sm_to_om_copy( const tripoint &p ) -> tripoint;

void sm_to_om( int &x, int &y );

inline void sm_to_om( point &p )
{
    sm_to_om( p.x, p.y );
}

inline void sm_to_om( tripoint &p )
{
    sm_to_om( p.x, p.y );
}

auto sm_to_om_remain( int &x, int &y ) -> point;

inline auto sm_to_om_remain( point &p ) -> point
{
    return sm_to_om_remain( p.x, p.y );
}

// overmap terrain to submap, basically: x *= 2
inline auto omt_to_sm_copy( int a ) -> int
{
    return 2 * a;
}

auto omt_to_sm_copy( point p ) -> point;

auto omt_to_sm_copy( const tripoint &p ) -> tripoint;

void omt_to_sm( int &x, int &y );

inline void omt_to_sm( point &p )
{
    omt_to_sm( p.x, p.y );
}

inline void omt_to_sm( tripoint &p )
{
    omt_to_sm( p.x, p.y );
}

// overmap terrain to map square
auto omt_to_ms_copy( point p ) -> point;

// overmap to submap, basically: x *= 2 * OMAPX
auto om_to_sm_copy( point p ) -> point;

auto om_to_sm_copy( const tripoint &p ) -> tripoint;

void om_to_sm( int &x, int &y );

inline void om_to_sm( point &p )
{
    om_to_sm( p.x, p.y );
}

inline void om_to_sm( tripoint &p )
{
    om_to_sm( p.x, p.y );
}

// map squares to submap, basically: x /= SEEX
auto ms_to_sm_copy( point p ) -> point;

auto ms_to_sm_copy( const tripoint &p ) -> tripoint;

void ms_to_sm( int &x, int &y );

inline void ms_to_sm( point &p )
{
    ms_to_sm( p.x, p.y );
}

inline void ms_to_sm( tripoint &p )
{
    ms_to_sm( p.x, p.y );
}

auto ms_to_sm_remain( int &x, int &y ) -> point;

inline auto ms_to_sm_remain( point &p ) -> point
{
    return ms_to_sm_remain( p.x, p.y );
}

inline auto ms_to_sm_remain( tripoint &p ) -> tripoint
{
    return tripoint( ms_to_sm_remain( p.x, p.y ), p.z );
}

// submap back to map squares, basically: x *= SEEX
// Note: this gives you the map square coordinates of the top-left corner
// of the given submap.
inline auto sm_to_ms_copy( point p ) -> point
{
    return point( p.x * SEEX, p.y * SEEY );
}

inline auto sm_to_ms_copy( const tripoint &p ) -> tripoint
{
    return tripoint( p.x * SEEX, p.y * SEEY, p.z );
}

void sm_to_ms( int &x, int &y );

inline void sm_to_ms( point &p )
{
    sm_to_ms( p.x, p.y );
}

inline void sm_to_ms( tripoint &p )
{
    sm_to_ms( p.x, p.y );
}

// map squares to overmap terrain, basically: x /= SEEX * 2

auto ms_to_omt_copy( point p ) -> point;

auto ms_to_omt_copy( const tripoint &p ) -> tripoint;

void ms_to_omt( int &x, int &y );

inline void ms_to_omt( point &p )
{
    ms_to_omt( p.x, p.y );
}

inline void ms_to_omt( tripoint &p )
{
    ms_to_omt( p.x, p.y );
}

auto ms_to_omt_remain( int &x, int &y ) -> point;

inline auto ms_to_omt_remain( point &p ) -> point
{
    return ms_to_omt_remain( p.x, p.y );
}

// overmap terrain to map segment.
auto omt_to_seg_copy( const tripoint &p ) -> tripoint;

// Submap to memory map region.
auto sm_to_mmr_remain( int &x, int &y ) -> point;

// Memory map region to submap.
// Note: this produces sm coords of top-left corner of the region.
auto mmr_to_sm_copy( const tripoint &p ) -> tripoint;


