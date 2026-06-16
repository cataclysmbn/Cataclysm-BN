#include "legacy_pathfinding.h"

#include <algorithm>
#include <cstdlib>
#include <optional>
#include <queue>
#include <ranges>
#include <set>
#include <array>
#include <memory>
#include <utility>
#include <vector>

#include "cata_cartesian_product.h"
#include "cata_utility.h"
#include "coordinates.h"
#include "debug.h"
#include "game_constants.h"
#include "map.h"
#include "mapdata.h"
#include "options.h"
#include "submap.h"
#include "trap.h"
#include "veh_type.h"
#include "vehicle.h"
#include "vehicle_part.h"
#include "vpart_position.h"
#include "line.h"
#include "mapbuffer.h"
#include "mapbuffer_registry.h"
#include "type_id.h"
#include "point.h"

enum astar_state {
    ASL_NONE,
    ASL_OPEN,
    ASL_CLOSED
};

// Turns two coordinates into an index into the 1D backing array.
// stride_y is the total tile height of the loaded map.
static int flat_index( const tripoint_abs_ms &p, const point_abs_ms &origin, int stride_y )
{
    const auto offset = p.xy() - origin;
    return ( offset.x() * stride_y ) + offset.y();
}

// Flattened 2D array representing a single z-level worth of pathfinding data.
// All four fields are heap-allocated vectors sized from runtime g_mapsize_x / g_mapsize_y
// so that they scale correctly with any REALITY_BUBBLE_SIZE setting.
struct path_data_layer {
    const int stride_y; // = g_mapsize_y at construction time
    const point_abs_ms origin;

    // State is accessed way more often than all other values here.
    std::vector<astar_state> state;
    std::vector<int> score;
    std::vector<int> gscore;
    std::vector<tripoint_abs_ms> parent;

    explicit path_data_layer( const point_abs_ms &_origin ) :
        stride_y( g_mapsize_y ),
        origin( _origin ),
        state( static_cast<std::size_t>( g_mapsize_x * g_mapsize_y ), ASL_NONE ),
        score( static_cast<std::size_t>( g_mapsize_x * g_mapsize_y ), 0 ),
        gscore( static_cast<std::size_t>( g_mapsize_x * g_mapsize_y ), 0 ),
        parent( static_cast<std::size_t>( g_mapsize_x * g_mapsize_y ) )
    {}

    // All elements are initialised to ASL_NONE in the constructor; init() is kept
    // for compatibility but is a no-op when the vector is already zeroed.
    void init( point_abs_ms min, point_abs_ms max ) {
        std::ranges::for_each(
            cata::views::cartesian_product(
                std::views::iota( min.x(), max.x() + 1 ),
                std::views::iota( min.y(), max.y() + 1 ) ),
        [this]( auto xy ) {
            const auto &[x, y] = xy;
            state[flat_index( tripoint_abs_ms{ x, y, 0 }, origin, stride_y )] = ASL_NONE;
        } );
    }
};

struct pathfinder {
    point_abs_ms min;
    point_abs_ms max;
    point_abs_ms origin;
    pathfinder( point_abs_ms _min, point_abs_ms _max, point_abs_ms _origin ) :
        min( _min ), max( _max ), origin( _origin ) {
    }

    std::priority_queue<std::pair<int, tripoint_abs_ms>, std::vector< std::pair<int, tripoint_abs_ms>>, pair_greater_cmp_first>
            open;
    std::array< std::unique_ptr<path_data_layer>, OVERMAP_LAYERS > path_data;

    path_data_layer &get_layer( const int z ) {
        std::unique_ptr< path_data_layer > &ptr = path_data[z + OVERMAP_DEPTH];
        if( ptr != nullptr ) {
            return *ptr;
        }

        ptr = std::make_unique<path_data_layer>( origin );
        ptr->init( min, max );
        return *ptr;
    }

    bool empty() const {
        return open.empty();
    }

    tripoint_abs_ms get_next() {
        const auto pt = open.top();
        open.pop();
        return pt.second;
    }

    void add_point( const int gscore, const int score, const tripoint_abs_ms &from,
                    const tripoint_abs_ms &to ) {
        auto &layer = get_layer( to.z() );
        const int index = flat_index( to, origin, layer.stride_y );
        if( ( layer.state[index] == ASL_OPEN && gscore >= layer.gscore[index] ) ||
            layer.state[index] == ASL_CLOSED ) {
            return;
        }

        layer.state [index] = ASL_OPEN;
        layer.gscore[index] = gscore;
        layer.parent[index] = from;
        layer.score [index] = score;
        open.emplace( score, to );
    }

    void close_point( const tripoint_abs_ms &p ) {
        auto &layer = get_layer( p.z() );
        const int index = flat_index( p, origin, layer.stride_y );
        layer.state[index] = ASL_CLOSED;
    }

    void unclose_point( const tripoint_abs_ms &p ) {
        auto &layer = get_layer( p.z() );
        const int index = flat_index( p, origin, layer.stride_y );
        layer.state[index] = ASL_NONE;
    }
};

// Modifies `t` to be a tile with `flag` in the overmap tile that `t` was originally on
// return false if it could not find a suitable point
template<ter_bitflags flag>
bool vertical_move_destination( const map &m, tripoint_abs_ms &t )
{
    if( !m.has_zlevels() ) {
        return false;
    }

    const auto omt = project_to<coords::omt>( t );
    auto &buffer = MAPBUFFER_REGISTRY.get( m.get_bound_dimension() );
    const auto omt_view = buffer.get_abs_omt_view( omt );
    if( !omt_view ) {
        return false;
    }

    const auto submap_offsets = std::array {
        point_omt_sm::zero(),
        point_omt_sm( 1, 0 ),
        point_omt_sm( 0, 1 ),
        point_omt_sm( 1, 1 ),
    };
    for( const auto &local_sm : submap_offsets ) {
        const auto submap_view = omt_view->get_submap_view( local_sm );
        if( !submap_view ) {
            continue;
        }

        for( const auto tile_pos : submap_view->tiles() ) {
            const auto tile = submap_view->tile( tile_pos );
            if( tile.get_ter_t().has_flag( flag ) ) {
                t = tile.abs_pos();
                return true;
            }
        }
    }

    return false;
}

template<class Set1, class Set2>
bool is_disjoint( const Set1 &set1, const Set2 &set2 )
{
    if( set1.empty() || set2.empty() ) {
        return true;
    }

    typename Set1::const_iterator it1 = set1.begin();
    typename Set1::const_iterator it1_end = set1.end();

    typename Set2::const_iterator it2 = set2.begin();
    typename Set2::const_iterator it2_end = set2.end();

    if( *set2.rbegin() < *it1 || *set1.rbegin() < *it2 ) {
        return true;
    }

    while( it1 != it1_end && it2 != it2_end ) {
        if( *it1 == *it2 ) {
            return false;
        }
        if( *it1 < *it2 ) {
            it1++;
        } else {
            it2++;
        }
    }

    return true;
}

struct legacy_pathfinding_tile {
    ter_id terrain;
    furn_id furniture;
    trap_id trap;
    const vehicle *vehicle_ptr_ = nullptr;
    int vehicle_part = -1;
    int move_cost = 0;

    auto vehicle_ptr() const -> const vehicle * { // *NOPAD*
        return vehicle_ptr_;
    }

    auto part_index() const -> int {
        return vehicle_part;
    }
};

// Astyle can snort my carpet dust
auto get_legacy_pathfinding_tile( const map &here,
                                  const tripoint_abs_ms &p ) -> std::optional<legacy_pathfinding_tile> // *NOPAD*
{
    const auto bubble_pos = abs_to_map_local( here, p );
    if( !here.inbounds( bubble_pos ) ) {
        return std::nullopt;
    }

    const auto tile = here.maptile_at_internal( bubble_pos );
    auto vehicle_part = -1;
    const auto *vehicle = here.veh_at_internal( bubble_pos, vehicle_part );
    const auto &terrain = tile.get_ter_t();
    const auto &furniture = tile.get_furn_t();

    return legacy_pathfinding_tile {
        .terrain = tile.get_ter(),
        .furniture = tile.get_furn(),
        .trap = tile.get_trap(),
        .vehicle_ptr_ = vehicle,
        .vehicle_part = vehicle_part,
        .move_cost = here.move_cost_internal( furniture, terrain, vehicle, vehicle_part ),
    };
}

auto get_pf_special_from_tile( const legacy_pathfinding_tile &tile ) -> pf_special
{
    auto cur_value = PF_NORMAL;
    const auto &terrain = tile.terrain.obj();

    if( tile.move_cost > 2 ) {
        cur_value |= PF_SLOW;
    } else if( tile.move_cost <= 0 ) {
        cur_value |= PF_WALL;
        if( terrain.has_flag( TFLAG_CLIMBABLE ) ) {
            cur_value |= PF_CLIMBABLE;
        }
    }

    if( tile.vehicle_ptr() != nullptr ) {
        cur_value |= PF_VEHICLE;
    }

    if( !tile.trap.obj().is_benign() || !terrain.trap.obj().is_benign() ) {
        cur_value |= PF_TRAP;
    }

    if( terrain.has_flag( TFLAG_GOES_DOWN ) || terrain.has_flag( TFLAG_GOES_UP ) ||
        terrain.has_flag( TFLAG_RAMP ) || terrain.has_flag( TFLAG_RAMP_UP ) ||
        terrain.has_flag( TFLAG_RAMP_DOWN ) ) {
        cur_value |= PF_UPDOWN;
    }

    if( terrain.has_flag( TFLAG_SHARP ) ) {
        cur_value |= PF_SHARP;
    }

    return cur_value;
}

auto get_pf_special_abs( const map &here, const tripoint_abs_ms &p ) -> pf_special
{
    const auto tile = get_legacy_pathfinding_tile( here, p );
    return tile ? get_pf_special_from_tile( *tile ) : PF_WALL;
}

std::vector<tripoint_abs_ms> map::route( const tripoint_abs_ms &f, const tripoint_abs_ms &t,
        const pathfinding_settings &settings,
        const std::set<tripoint_abs_ms> &pre_closed ) const
{
    /* TODO: If the origin or destination is out of bound, figure out the closest
     * in-bounds point and go to that, then to the real origin/destination.
     */
    std::vector<tripoint_abs_ms> ret;
    auto &buffer = MAPBUFFER_REGISTRY.get( get_bound_dimension() );

    if( f == t || !inbounds( abs_to_map_local( *this, f ) ) ) {
        return ret;
    }

    if( !inbounds( abs_to_map_local( *this, t ) ) ) {
        return ret;
    }
    // First, check for a simple straight line on flat ground
    // Except when the line contains a pre-closed tile - we need to do regular pathing then
    static const auto non_normal = PF_SLOW | PF_WALL | PF_VEHICLE | PF_TRAP | PF_SHARP;
    if( f.z() == t.z() ) {
        const auto line_path = line_to( f, t );
        // Check all points for any special case (including just hard terrain)
        if( !( get_pf_special_abs( *this, f ) & non_normal ) &&
        std::ranges::all_of( line_path, [this]( const tripoint_abs_ms & p ) {
        return !( get_pf_special_abs( *this, p ) & non_normal );
        } ) ) {
            const std::set<tripoint_abs_ms> sorted_line( line_path.begin(), line_path.end() );

            if( is_disjoint( sorted_line, pre_closed ) ) {
                return line_path;
            }
        }
    }

    // Apply the global distance cap from settings.  min() naturally handles both the normal case
    // (monster has an explicit max_dist already smaller than the cap) and the unlimited case
    // (max_dist == INT_MAX from mods that remove the per-monster limit).
    const int dist_cap = get_option<int>( "PATHFINDING_MAX_DIST" );
    // If expected path length is greater than max distance, allow only line path, like above
    if( rl_dist( f, t ) > std::min( settings.max_dist, dist_cap ) ) {
        return ret;
    }

    int max_length = std::min( settings.max_length, dist_cap * 5 );
    int bash = settings.bash_strength;
    int climb_cost = settings.climb_cost;
    bool doors = settings.allow_open_doors;
    bool trapavoid = settings.avoid_traps;
    bool roughavoid = settings.avoid_rough_terrain;
    bool sharpavoid = settings.avoid_sharp;

    // Search-area inflation: at least 16 tiles, scaled with bubble radius so pathfinders
    // can route around obstacles near the bubble boundary at large bubble sizes.
    const int pad = std::max( 16, 4 * g_half_mapsize );
    auto min = tripoint_abs_ms( std::min( f.x(), t.x() ) - pad, std::min( f.y(), t.y() ) - pad,
                                std::min( f.z(), t.z() ) );
    auto max = tripoint_abs_ms( std::max( f.x(), t.x() ) + pad, std::max( f.y(), t.y() ) + pad,
                                std::max( f.z(), t.z() ) );
    const auto origin = project_to<coords::ms>( get_abs_sub() ).xy();
    const auto bounds_max = origin + point_rel_ms( g_mapsize_x - 1, g_mapsize_y - 1 );
    min.x() = std::max( min.x(), origin.x() );
    min.y() = std::max( min.y(), origin.y() );
    max.x() = std::min( max.x(), bounds_max.x() );
    max.y() = std::min( max.y(), bounds_max.y() );
    min.z() = std::max( min.z(), -OVERMAP_DEPTH );
    max.z() = std::min( max.z(), OVERMAP_HEIGHT );

    pathfinder pf( min.xy(), max.xy(), origin );
    // Make NPCs not want to path through player
    // But don't make player pathing stop working
    for( const auto &p : pre_closed ) {
        if( p.x() >= min.x() && p.x() <= max.x() && p.y() >= min.y() && p.y() <= max.y() ) {
            pf.close_point( p );
        }
    }

    // Start and end must not be closed
    pf.unclose_point( f );
    pf.unclose_point( t );
    pf.add_point( 0, 0, f, f );

    bool done = false;

    do {
        auto cur = pf.get_next();

        auto &layer = pf.get_layer( cur.z() );
        const int parent_index = flat_index( cur, pf.origin, layer.stride_y );
        auto &cur_state = layer.state[parent_index];
        if( cur_state == ASL_CLOSED ) {
            continue;
        }

        if( layer.gscore[parent_index] > max_length ) {
            // Shortest path would be too long, return empty vector
            return std::vector<tripoint_abs_ms>();
        }

        if( cur == t ) {
            done = true;
            break;
        }

        cur_state = ASL_CLOSED;

        const auto cur_tile = get_legacy_pathfinding_tile( *this, cur );
        if( !cur_tile ) {
            continue;
        }
        const auto cur_special = get_pf_special_from_tile( *cur_tile );
        const auto *cur_veh = cur_tile->vehicle_ptr();

        // 7 3 5
        // 1 . 2
        // 6 4 8
        constexpr std::array<int, 8> x_offset{{ -1,  1,  0,  0,  1, -1, -1, 1 }};
        constexpr std::array<int, 8> y_offset{{  0,  0, -1,  1, -1,  1, -1, 1 }};
        for( size_t i = 0; i < 8; i++ ) {
            const tripoint_abs_ms p( cur.x() + x_offset[i], cur.y() + y_offset[i], cur.z() );
            const int index = flat_index( p, pf.origin, layer.stride_y );

            // TODO: Remove this and instead have sentinels at the edges
            if( p.x() < min.x() || p.x() > max.x() || p.y() < min.y() || p.y() > max.y() ) {
                continue;
            }

            if( layer.state[index] == ASL_CLOSED ) {
                continue;
            }

            const auto p_tile = get_legacy_pathfinding_tile( *this, p );
            if( !p_tile ) {
                layer.state[index] = ASL_CLOSED;
                continue;
            }

            int part = p_tile->part_index();
            const vehicle *veh = p_tile->vehicle_ptr();
            if( cur_veh &&
                !cur_veh->allowed_move( cur_veh->abs_to_mount( cur ),
                                        cur_veh->abs_to_mount( p ) ) ) {
                //Trying to squeeze through a vehicle hole, skip this movement but don't close the tile as other paths may lead to it
                continue;
            }

            if( veh && veh != cur_veh &&
                !veh->allowed_move( veh->abs_to_mount( cur ),
                                    veh->abs_to_mount( p ) ) ) {
                //Same as above but moving into rather than out of a vehicle
                continue;
            }

            // Penalize for diagonals or the path will look "unnatural"
            int newg = layer.gscore[parent_index] + ( ( cur.x() != p.x() && cur.y() != p.y() ) ? 1 : 0 );

            const auto p_special = get_pf_special_from_tile( *p_tile );
            // TODO: De-uglify, de-huge-n
            if( !( p_special & non_normal ) ) {
                // Boring flat dirt - the most common case above the ground
                newg += 2;
            } else {
                if( roughavoid ) {
                    layer.state[index] = ASL_CLOSED; // Close all rough terrain tiles
                    continue;
                }

                const auto &terrain = p_tile->terrain.obj();
                const auto &furniture = p_tile->furniture.obj();

                const int cost = p_tile->move_cost;
                // Don't calculate bash rating unless we intend to actually use it
                const int rating = ( bash == 0 || cost != 0 ) ? -1 :
                                   bash_rating_internal( bash, furniture, terrain, false, veh, part );

                if( cost == 0 && rating <= 0 && ( !doors || !terrain.open || !furniture.open ) && veh == nullptr &&
                    climb_cost <= 0 ) {
                    layer.state[index] = ASL_CLOSED; // Close it so that next time we won't try to calculate costs
                    continue;
                }

                newg += cost;
                if( cost == 0 ) {
                    if( climb_cost > 0 && p_special & PF_CLIMBABLE ) {
                        // Climbing fences
                        newg += climb_cost;
                    } else if( doors && ( terrain.open || furniture.open ) &&
                               ( !terrain.has_flag( "OPENCLOSE_INSIDE" ) || !furniture.has_flag( "OPENCLOSE_INSIDE" ) ||
                                 !is_outside( abs_to_map_local( *this, cur ) ) ) ) {
                        // Only try to open INSIDE doors from the inside
                        // To open and then move onto the tile
                        newg += 4;
                    } else if( veh != nullptr ) {
                        const auto vpobst = vpart_position( const_cast<vehicle &>( *veh ), part ).obstacle_at_part();
                        part = vpobst ? vpobst->part_index() : -1;
                        if( doors && veh->part_flag( part, VPFLAG_OPENABLE ) &&
                            ( !veh->part_flag( part, "OPENCLOSE_INSIDE" ) ||
                              cur_veh == veh ) ) {
                            // Handle car doors, but don't try to path through curtains
                            newg += 10; // One turn to open, 4 to move there
                        } else if( part >= 0 && bash > 0 ) {
                            // Car obstacle that isn't a door
                            // TODO: Account for armor
                            int hp = veh->cpart( part ).hp();
                            if( hp / 20 > bash ) {
                                // Threshold damage thing means we just can't bash this down
                                layer.state[index] = ASL_CLOSED;
                                continue;
                            } else if( hp / 10 > bash ) {
                                // Threshold damage thing means we will fail to deal damage pretty often
                                hp *= 2;
                            }

                            newg += 2 * hp / bash + 8 + 4;
                        } else if( part >= 0 ) {
                            if( !doors || !veh->part_flag( part, VPFLAG_OPENABLE ) ) {
                                // Won't be openable, don't try from other sides
                                layer.state[index] = ASL_CLOSED;
                            }

                            continue;
                        }
                    } else if( rating > 1 ) {
                        // Expected number of turns to bash it down, 1 turn to move there
                        // and 5 turns of penalty not to trash everything just because we can
                        newg += ( 20 / rating ) + 2 + 10;
                    } else if( rating == 1 ) {
                        // Desperate measures, avoid whenever possible
                        newg += 500;
                    } else {
                        // Unbashable and unopenable from here
                        if( !doors || !terrain.open || !furniture.open ) {
                            // Or anywhere else for that matter
                            layer.state[index] = ASL_CLOSED;
                        }

                        continue;
                    }
                }

                if( trapavoid && p_special & PF_TRAP ) {
                    const auto &ter_trp = terrain.trap.obj();
                    const auto &trp = ter_trp.is_benign() ? p_tile->trap.obj() : ter_trp;
                    if( !trp.is_benign() ) {
                        // For now make them detect all traps
                        if( has_zlevels() && terrain.has_flag( TFLAG_NO_FLOOR ) ) {
                            // Special case - ledge in z-levels
                            // Warning: really expensive, needs a cache
                            const auto below = p + tripoint_rel_ms::below();
                            if( buffer.valid_move( p, below, { .flying = true, .zlevels = has_zlevels() } ) ) {
                                const auto below_tile = buffer.get_abs_tile( below );
                                if( below_tile && !below_tile->get_ter_t().has_flag( TFLAG_NO_FLOOR ) ) {
                                    // Otherwise this would have been a huge fall
                                    auto &layer = pf.get_layer( p.z() - 1 );
                                    // From cur, not p, because we won't be walking on air
                                    pf.add_point( layer.gscore[parent_index] + 10,
                                                  layer.score[parent_index] + 10 + 2 * rl_dist( below, t ),
                                                  cur, below );
                                }

                                // Close p, because we won't be walking on it
                                layer.state[index] = ASL_CLOSED;
                                continue;
                            }
                        } else if( trapavoid ) {
                            // Otherwise it's walkable
                            newg += 500;
                        }
                    }
                }

                if( sharpavoid && p_special & PF_SHARP ) {
                    layer.state[index] = ASL_CLOSED; // Avoid sharp things
                }

            }

            // If not visited, add as open
            // If visited, add it only if we can do so with better score
            if( layer.state[index] == ASL_NONE || newg < layer.gscore[index] ) {
                pf.add_point( newg, newg + 2 * rl_dist( p, t ), cur, p );
            }
        }

        if( !has_zlevels() || !( cur_special & PF_UPDOWN ) || !settings.allow_climb_stairs ) {
            // The part below is only for z-level pathing
            continue;
        }

        if( !cur_tile ) {
            continue;
        }
        const auto &parent_terrain = cur_tile->terrain.obj();
        if( settings.allow_climb_stairs && cur.z() > min.z() &&
            parent_terrain.has_flag( TFLAG_GOES_DOWN ) ) {
            tripoint_abs_ms dest( cur.xy(), cur.z() - 1 );
            if( vertical_move_destination<TFLAG_GOES_UP>( *this, dest ) ) {
                auto &layer = pf.get_layer( dest.z() );
                pf.add_point( layer.gscore[parent_index] + 2,
                              layer.score[parent_index] + 2 * rl_dist( dest, t ),
                              cur, dest );
            }
        }
        if( settings.allow_climb_stairs && cur.z() < max.z() && parent_terrain.has_flag( TFLAG_GOES_UP ) ) {
            tripoint_abs_ms dest( cur.xy(), cur.z() + 1 );
            if( vertical_move_destination<TFLAG_GOES_DOWN>( *this, dest ) ) {
                auto &layer = pf.get_layer( dest.z() );
                pf.add_point( layer.gscore[parent_index] + 2,
                              layer.score[parent_index] + 2 * rl_dist( dest, t ),
                              cur, dest );
            }
        }
        if( cur.z() < max.z() && parent_terrain.has_flag( TFLAG_RAMP ) &&
            buffer.valid_move( cur, cur + tripoint_rel_ms::above(),
        { .flying = true, .zlevels = has_zlevels() } ) ) {
            auto &layer = pf.get_layer( cur.z() + 1 );
            for( size_t it = 0; it < 8; it++ ) {
                const tripoint_abs_ms above( cur.x() + x_offset[it], cur.y() + y_offset[it], cur.z() + 1 );
                pf.add_point( layer.gscore[parent_index] + 4,
                              layer.score[parent_index] + 4 + 2 * rl_dist( above, t ),
                              cur, above );
            }
        }
        if( cur.z() < max.z() && parent_terrain.has_flag( TFLAG_RAMP_UP ) &&
            buffer.valid_move( cur, cur + tripoint_rel_ms::above(),
        { .flying = true, .via_ramp = true, .zlevels = has_zlevels() } ) ) {
            auto &layer = pf.get_layer( cur.z() + 1 );
            for( size_t it = 0; it < 8; it++ ) {
                const tripoint_abs_ms above( cur.x() + x_offset[it], cur.y() + y_offset[it], cur.z() + 1 );
                pf.add_point( layer.gscore[parent_index] + 4,
                              layer.score[parent_index] + 4 + 2 * rl_dist( above, t ),
                              cur, above );
            }
        }
        if( cur.z() > min.z() && parent_terrain.has_flag( TFLAG_RAMP_DOWN ) &&
            buffer.valid_move( cur, cur + tripoint_rel_ms::below(),
        { .flying = true, .via_ramp = true, .zlevels = has_zlevels() } ) ) {
            auto &layer = pf.get_layer( cur.z() - 1 );
            for( size_t it = 0; it < 8; it++ ) {
                const tripoint_abs_ms below( cur.x() + x_offset[it], cur.y() + y_offset[it], cur.z() - 1 );
                pf.add_point( layer.gscore[parent_index] + 4,
                              layer.score[parent_index] + 4 + 2 * rl_dist( below, t ),
                              cur, below );
            }
        }

    } while( !done && !pf.empty() );

    if( done ) {
        ret.reserve( rl_dist( f, t ) * 2 );
        auto cur = t;
        // Just to limit max distance, in case something weird happens
        for( int fdist = max_length; fdist != 0; fdist-- ) {
            const auto &layer = pf.get_layer( cur.z() );
            const int cur_index = flat_index( cur, pf.origin, layer.stride_y );
            const tripoint_abs_ms &par = layer.parent[cur_index];
            if( cur == f ) {
                break;
            }

            ret.push_back( cur );
            // Jumps are acceptable on 1 z-level changes
            // This is because stairs teleport the player too
            if( rl_dist( cur, par ) > 1 && std::abs( cur.z() - par.z() ) != 1 ) {
                debugmsg( "Jump in our route!  %d:%d:%d->%d:%d:%d",
                          cur.x(), cur.y(), cur.z(), par.x(), par.y(), par.z() );
                return ret;
            }

            cur = par;
        }

        std::ranges::reverse( ret );
    }

    return ret;
}

std::vector<tripoint_bub_ms> map::route( const tripoint_bub_ms &f, const tripoint_bub_ms &t,
        const pathfinding_settings &settings,
        const std::set<tripoint_bub_ms> &pre_closed ) const
{
    auto clipped_f = f;
    auto clipped_t = t;
    clip_to_bounds( clipped_f );
    clip_to_bounds( clipped_t );

    std::set<tripoint_abs_ms> pre_closed_abs;
    for( const tripoint_bub_ms &p : pre_closed ) {
        pre_closed_abs.insert( map_local_to_abs( *this, p ) );
    }

    const auto abs_route = route( map_local_to_abs( *this, clipped_f ),
                                  map_local_to_abs( *this, clipped_t ),
                                  settings, pre_closed_abs );
    std::vector<tripoint_bub_ms> result;
    result.reserve( abs_route.size() );
    for( const tripoint_abs_ms &p : abs_route ) {
        result.push_back( abs_to_map_local( *this, p ) );
    }
    return result;
}
