#include "map_selector.h"

#include <algorithm>
#include <functional>
#include <memory>
#include <optional>
#include <vector>

#include "game.h"
#include "game_constants.h"
#include "map.h"
#include "map_iterator.h"
#include "rng.h"

map_selector::map_selector( const tripoint_bub_ms &pos, int radius, bool accessible )
{
    for( const tripoint_bub_ms &e : closest_points_first( pos, radius ) ) {
        if( !accessible ||
            get_map().clear_path( pos, e, radius, 1, 100 ) ) {
            data.emplace_back( e );
        }
    }
}

tripoint_range<tripoint_bub_ms> points_in_range( const map &m )
{
    const int z = m.get_abs_sub().z();
    const bool hasz = m.has_zlevels();
    return tripoint_range<tripoint_bub_ms>(
               tripoint_bub_ms( 0, 0, hasz ? -OVERMAP_DEPTH : z ),
               tripoint_bub_ms( SEEX * m.getmapsize() - 1, SEEY * m.getmapsize() - 1,
                                hasz ? OVERMAP_HEIGHT : z ) );
}

std::optional<tripoint_bub_ms> random_point( const map &m,
        const std::function<bool( const tripoint_bub_ms & )> &predicate )
{
    return random_point( points_in_range( m ), predicate );
}

std::optional<tripoint_bub_ms> random_point( const tripoint_range<tripoint_bub_ms> &range,
        const std::function<bool( const tripoint_bub_ms & )> &predicate )
{
    // Optimist approach: just assume there are plenty of suitable places and a randomly
    // chosen point will have a good chance to hit one of them.
    // If there are only few suitable places, we have to find them all, otherwise this loop may never finish.
    for( int tries = 0; tries < 10; ++tries ) {
        const tripoint_bub_ms p( rng( range.min().x(), range.max().x() ), rng( range.min().y(),
                                 range.max().y() ),
                                 rng( range.min().z(), range.max().z() ) );
        if( predicate( p ) ) {
            return p;
        }
    }
    std::vector<tripoint_bub_ms> suitable;
    for( const auto &p : range ) {
        if( predicate( p ) ) {
            suitable.push_back( p );
        }
    }
    if( suitable.empty() ) {
        return {};
    }
    return random_entry( suitable );
}

map_cursor::map_cursor( const tripoint_abs_ms &pos )
{
    pos_ = g ? get_map().abs_to_bub( pos ) : pos.reinterpret_as<tripoint_bub_ms>();
}

map_cursor::map_cursor( const tripoint_bub_ms &pos )
{
    pos_ = pos;
}

map_cursor::operator tripoint_abs_ms() const
{
    return g ? get_map().bub_to_abs( pos_ ) : pos_.reinterpret_as<tripoint_abs_ms>();
}

map_cursor::operator tripoint_bub_ms() const
{
    return pos_;
}
