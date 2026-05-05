#include "point_rotate.h"
#include "overmapbuffer.h"
#include "omdata.h"
#include "point.h"

auto rotate( point p, point dim, int turns ) -> point
{
    switch( turns ) {
        case 1:
            return { dim.y - p.y - 1, p.x };
        case 2:
            // NOLINTNEXTLINE(cata-use-point-arithmetic)
            return { dim.x - p.x - 1, dim.y - p.y - 1 };
        case 3:
            return { p.y, dim.x - p.x - 1 };
        default:
            return p;
    }
}

auto rotate( const tripoint &p, point dim, int turns ) -> tripoint
{
    return { rotate( p.xy(), dim, turns ), p.z };
}

tripoint_bub_ms rotate_point_sm( const tripoint_bub_ms &p, const tripoint_bub_ms &orig, int turns )
{
    const tripoint_rel_ms p_sm = p - orig.xy();
    const auto rd = p_sm.xy().rotate( turns, point( SEEX * 2, SEEY * 2 ) );

    return tripoint_bub_ms( point_bub_ms( rd + orig.xy() ), p.z() );
}

/** @return The difference in rotation between two overmap terrain points. */
auto get_rot_turns( const tripoint_abs_omt &here, const tripoint_abs_omt &there,
                    overmapbuffer &omb ) -> int
{
    const auto this_dir = omb.ter( there )->get_dir();
    const auto that_dir = omb.ter( here )->get_dir();

    int const diff = static_cast<int>( this_dir ) - static_cast<int>( that_dir );
    return diff >= 0 ? diff : 4 + diff;
}
