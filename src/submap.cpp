#include "submap.h"

#include <algorithm>
#include <array>
#include <iterator>
#include <memory>
#include <ranges>
#include <span>
#include <utility>

#include "debug.h"
#include "int_id.h"
#include "lightmap.h"
#include "map.h"
#include "mapdata.h"
#include "tileray.h"
#include "trap.h"
#include "vehicle.h"
#include "vehicle_part.h"
#include "weather.h"


const data_vars::data_set submap::EMPTY_VARS{};

template<int sx, int sy>
void maptile_soa<sx, sy>::swap_soa_tile( const point_sm_ms &p1, const point_sm_ms &p2 )
{
    std::swap( ter[p1.x()][p1.y()], ter[p2.x()][p2.y()] );
    std::swap( frn[p1.x()][p1.y()], frn[p2.x()][p2.y()] );
    std::swap( lum[p1.x()][p1.y()], lum[p2.x()][p2.y()] );
    std::swap( itm[p1.x()][p1.y()], itm[p2.x()][p2.y()] );
    std::swap( fld[p1.x()][p1.y()], fld[p2.x()][p2.y()] );
    std::swap( trp[p1.x()][p1.y()], trp[p2.x()][p2.y()] );
    std::swap( rad[p1.x()][p1.y()], rad[p2.x()][p2.y()] );
}

void submap::swap( submap &first, submap &second )
{
    std::swap( first.pos, second.pos );
    std::swap( first.ter, second.ter );
    std::swap( first.frn, second.frn );
    std::swap( first.lum, second.lum );
    std::swap( first.fld, second.fld );
    std::swap( first.trp, second.trp );
    std::swap( first.rad, second.rad );
    std::swap( first.is_uniform, second.is_uniform );
    std::swap( first.active_items, second.active_items );
    std::swap( first.field_count, second.field_count );
    std::swap( first.trap_cache, second.trap_cache );
    std::swap( first.field_cache, second.field_cache );
    std::swap( first.emitter_cache, second.emitter_cache );
    std::swap( first.last_touched, second.last_touched );
    std::swap( first.spawns, second.spawns );
    std::swap( first.vehicles, second.vehicles );
    std::swap( first.partial_constructions, second.partial_constructions );
    std::swap( first.active_furniture, second.active_furniture );
    std::swap( first.transformer_last_run, second.transformer_last_run );
    std::swap( first.is_uniform, second.is_uniform );
    std::swap( first.computers, second.computers );
    std::swap( first.legacy_computer, second.legacy_computer );
    std::swap( first.temperature, second.temperature );
    std::swap( first.cosmetics, second.cosmetics );
    std::swap( first.frn_vars, second.frn_vars );
    std::swap( first.ter_vars, second.ter_vars );

    for( const auto &p : submap_tiles() ) {
        std::swap( first.itm[p.x()][p.y()], second.itm[p.x()][p.y()] );
    }
}

template<int sx, int sy>
maptile_soa<sx, sy>::maptile_soa( const tripoint_abs_sm &position )
{
    for( const auto &p : submap_tiles() ) {
        itm[p.x()][p.y()].init_location( new tile_item_location( project_combine( position, p ) ) );
    }
}

submap::submap( const tripoint_abs_sm &position ) : maptile_soa<SEEX, SEEY>( position )
{
    pos = position;
    std::fill_n( &ter[0][0], elements, t_null );
    std::fill_n( &frn[0][0], elements, f_null );
    std::fill_n( &lum[0][0], elements, 0 );
    std::fill_n( &trp[0][0], elements, tr_null );
    std::fill_n( &rad[0][0], elements, 0 );

    is_uniform = false;
}

submap::~submap() = default;

void submap::update_lum_rem( const point_sm_ms &p, const item &i )
{
    is_uniform = false;
    if( !i.is_emissive() ) {
        return;
    } else if( lum[p.x()][p.y()] && lum[p.x()][p.y()] < 255 ) {
        lum[p.x()][p.y()]--;
        return;
    }

    // Have to scan through all items to be sure removing i will actually lower
    // the count below 255.
    int count = 0;
    for( const auto &it : itm[p.x()][p.y()] ) {
        if( it->is_emissive() ) {
            count++;
        }
    }

    if( count <= 256 ) {
        lum[p.x()][p.y()] = static_cast<uint8_t>( count - 1 );
    }
}

void submap::insert_cosmetic( const point_sm_ms &p, const std::string &type,
                              const std::string &str )
{
    cosmetic_t ins;

    ins.pos = p;
    ins.type = type;
    ins.str = str;

    cosmetics.push_back( ins );
}

static const std::string COSMETICS_GRAFFITI( "GRAFFITI" );
static const std::string COSMETICS_SIGNAGE( "SIGNAGE" );
// Handle GCC warning: 'warning: returning reference to temporary'
static const std::string STRING_EMPTY;

struct cosmetic_find_result {
    bool result;
    int ndx;
};
static cosmetic_find_result make_result( bool b, int ndx )
{
    cosmetic_find_result result;
    result.result = b;
    result.ndx = ndx;
    return result;
}
static cosmetic_find_result find_cosmetic(
    const std::vector<submap::cosmetic_t> &cosmetics, const point_sm_ms &p, const std::string &type )
{
    for( size_t i = 0; i < cosmetics.size(); ++i ) {
        if( cosmetics[i].pos == p && cosmetics[i].type == type ) {
            return make_result( true, i );
        }
    }
    return make_result( false, -1 );
}

bool submap::has_graffiti( const point_sm_ms &p ) const
{
    return find_cosmetic( cosmetics, p, COSMETICS_GRAFFITI ).result;
}

const std::string &submap::get_graffiti( const point_sm_ms &p ) const
{
    const auto fresult = find_cosmetic( cosmetics, p, COSMETICS_GRAFFITI );
    if( fresult.result ) {
        return cosmetics[ fresult.ndx ].str;
    }
    return STRING_EMPTY;
}

void submap::set_graffiti( const point_sm_ms &p, const std::string &new_graffiti )
{
    is_uniform = false;
    // Find signage at p if available
    const auto fresult = find_cosmetic( cosmetics, p, COSMETICS_GRAFFITI );
    if( fresult.result ) {
        cosmetics[ fresult.ndx ].str = new_graffiti;
    } else {
        insert_cosmetic( p, COSMETICS_GRAFFITI, new_graffiti );
    }
}

void submap::delete_graffiti( const point_sm_ms &p )
{
    is_uniform = false;
    const auto fresult = find_cosmetic( cosmetics, p, COSMETICS_GRAFFITI );
    if( fresult.result ) {
        cosmetics[ fresult.ndx ] = cosmetics.back();
        cosmetics.pop_back();
    }
}
bool submap::has_signage( const point_sm_ms &p ) const
{
    if( frn[p.x()][p.y()].obj().has_flag( "SIGN" ) ) {
        return find_cosmetic( cosmetics, p, COSMETICS_SIGNAGE ).result;
    }

    return false;
}
std::string submap::get_signage( const point_sm_ms &p ) const
{
    if( frn[p.x()][p.y()].obj().has_flag( "SIGN" ) ) {
        const auto fresult = find_cosmetic( cosmetics, p, COSMETICS_SIGNAGE );
        if( fresult.result ) {
            return cosmetics[ fresult.ndx ].str;
        }
    }

    return STRING_EMPTY;
}
void submap::set_signage( const point_sm_ms &p, const std::string &s )
{
    is_uniform = false;
    // Find signage at p if available
    const auto fresult = find_cosmetic( cosmetics, p, COSMETICS_SIGNAGE );
    if( fresult.result ) {
        cosmetics[ fresult.ndx ].str = s;
    } else {
        insert_cosmetic( p, COSMETICS_SIGNAGE, s );
    }
}
void submap::delete_signage( const point_sm_ms &p )
{
    is_uniform = false;
    const auto fresult = find_cosmetic( cosmetics, p, COSMETICS_SIGNAGE );
    if( fresult.result ) {
        cosmetics[ fresult.ndx ] = cosmetics.back();
        cosmetics.pop_back();
    }
}

void submap::update_legacy_computer()
{
    if( legacy_computer ) {
        for( const auto &p : submap_tiles() ) {
            if( ter[p.x()][p.y()] == t_console ) {
                computers.emplace( p, *legacy_computer );
            }
        }
        legacy_computer.reset();
    }
}

bool submap::has_computer( const point_sm_ms &p ) const
{
    return computers.contains( p ) || ( legacy_computer && ter[p.x()][p.y()] == t_console );
}

const computer *submap::get_computer( const point_sm_ms &p ) const
{
    // the returned object will not get modified (should not, at least), so we
    // don't yet need to update to std::map
    const auto it = computers.find( p );
    if( it != computers.end() ) {
        return &it->second;
    }
    if( legacy_computer && ter[p.x()][p.y()] == t_console ) {
        return legacy_computer.get();
    }
    return nullptr;
}

computer *submap::get_computer( const point_sm_ms &p )
{
    // need to update to std::map first so modifications to the returned object
    // only affects the exact const point_sm_ms &p
    update_legacy_computer();
    const auto it = computers.find( p );
    if( it != computers.end() ) {
        return &it->second;
    }
    return nullptr;
}

void submap::set_computer( const point_sm_ms &p, const computer &c )
{
    update_legacy_computer();
    const auto it = computers.find( p );
    if( it != computers.end() ) {
        it->second = c;
    } else {
        computers.emplace( p, c );
    }
}

void submap::delete_computer( const point_sm_ms &p )
{
    update_legacy_computer();
    computers.erase( p );
}

bool submap::contains_vehicle( vehicle *veh )
{
    const auto match = std::ranges::find_if(
                           vehicles,
    [veh]( const std::unique_ptr<vehicle> &v ) {
        return v.get() == veh;
    } );
    return match != vehicles.end();
}

void submap::rotate( int turns )
{
    turns = turns % 4;

    if( turns == 0 ) {
        return;
    }

    const auto rotate_point = [turns]( const point_sm_ms & p ) {
        return p.rotate( turns, { SEEX, SEEY } );
    };

    if( turns == 2 ) {
        // Swap horizontal stripes.
        for( int j = 0, je = SEEY / 2; j < je; ++j ) {
            for( int i = j, ie = SEEX - j; i < ie; ++i ) {
                swap_soa_tile( { i, j }, rotate_point( { i, j } ) );
            }
        }
        // Swap vertical stripes so that they don't overlap with
        // the already swapped horizontals.
        for( int i = 0, ie = SEEX / 2; i < ie; ++i ) {
            for( int j = i + 1, je = SEEY - i - 1; j < je; ++j ) {
                swap_soa_tile( { i, j }, rotate_point( { i, j } ) );
            }
        }
    } else {
        for( int i = 0; i < SEEX / 2; i++ ) {
            for( int j = 0; j < SEEY / 2; j++ ) {

                /* We first number each of the four points as so:
                 * Clockwise            Anti-clockwise
                 *   12                     14
                 *   43                     23
                 * Then do a series of swaps:
                 *            Start
                 *   AB                     AB
                 *   CD                     CD
                 *           Swap 1 <-> 2
                 *   BA                     CB
                 *   CD                     AD
                 *           Swap 1 <-> 3
                 *   DA                     DB
                 *   CB                     AC
                 *           Swap 1 <-> 4
                 *   CA                     BD
                 *   DB                     AC
                 *   As you can see, this causes the desired rotation.
                 */

                const point_sm_ms &p1 = point_sm_ms( i, j );
                const point_sm_ms &p2 = rotate_point( p1 );
                const point_sm_ms &p3 = rotate_point( p2 );
                const point_sm_ms &p4 = rotate_point( p3 );

                swap_soa_tile( p1, p2 );
                swap_soa_tile( p1, p3 );
                swap_soa_tile( p1, p4 );
            }
        }
    }

    for( auto &elem : cosmetics ) {
        elem.pos = rotate_point( elem.pos );
    }

    for( auto &elem : spawns ) {
        elem.pos = rotate_point( elem.pos );
    }

    for( auto &elem : vehicles ) {
        const point_sm_ms new_pos = rotate_point( elem->sm_ms_pos );

        elem->sm_ms_pos = new_pos;
        elem->set_facing( elem->turn_dir + turns * 90_degrees );
    }

    std::map<point_sm_ms, computer> rot_comp;
    for( auto &elem : computers ) {
        rot_comp.emplace( rotate_point( elem.first ), elem.second );
    }
    computers = rot_comp;

    std::map<point_sm_ms, cata::poly_serialized<active_tile_data>> rot_active_furn;
    for( auto &elem : active_furniture ) {
        rot_active_furn.emplace( point_sm_ms( rotate_point( elem.first ) ), elem.second );
    }
    active_furniture = rot_active_furn;

    std::unordered_map<point_sm_ms, data_vars::data_set> rot_frn_vars;
    for( auto &elem : frn_vars ) {
        rot_frn_vars.emplace( rotate_point( elem.first ), elem.second );
    }
    frn_vars = rot_frn_vars;

    std::unordered_map<point_sm_ms, data_vars::data_set> rot_ter_vars;
    for( auto &elem : ter_vars ) {
        rot_ter_vars.emplace( rotate_point( elem.first ), elem.second );
    }
    ter_vars = rot_ter_vars;
    std::map<point_sm_ms, time_point> rot_transformer_last_run;
    for( auto &elem : transformer_last_run ) {
        rot_transformer_last_run.emplace( point_sm_ms( rotate_point( elem.first ) ), elem.second );
    }
    transformer_last_run = rot_transformer_last_run;

    // Tile data was moved in-place by swap_soa_tile, bypassing set_trap/set_furn.
    // Rebuild position caches from scratch now that all arrays are in their final state.
    trap_cache.clear();
    field_cache.clear();
    emitter_cache = std::nullopt;
    std::ranges::for_each(
        std::views::iota( 0, SEEX * SEEY )
        | std::views::transform( []( int i ) -> point_sm_ms { return { i % SEEX, i / SEEX }; } ),
    [this]( const point_sm_ms & p ) {
        if( trp[p.x()][p.y()] != tr_null ) {
            trap_cache.push_back( p );
        }
        if( fld[p.x()][p.y()].displayed_field_type() ) {
            field_cache.push_back( p );
        }
    } );
}


auto submap::rebuild_outside_cache( const level_cache *above,
                                    const tripoint_bub_sm &grid_pos ) -> void
{
    if( !outside_dirty ) {
        return;
    }
    // Base case: OVERMAP_HEIGHT — everything is open sky.
    if( above == nullptr ) {
        std::ranges::fill( std::span( &outside_cache[0][0], SEEX * SEEY ), true );
        std::ranges::fill( std::span( &sheltered_cache[0][0], SEEX * SEEY ), false );
        outside_dirty = false;
        return;
    }
    const auto abs_p = project_to<coords::ms>( grid_pos ).xy();
    for( const auto &p : submap_tiles() ) {
        // A tile is outside if any tile in the 3×3 at z+1 satisfies:
        // (outside at z+1) AND (no floor at z+1 blocking the path).
        // Out-of-bounds neighbours (edge of loaded map) are treated as inside.
        const auto ap = abs_p + p.raw(); // avoid projection cost of project_combine
        bool result = false;
        for( int dx = -1; dx <= 1 && !result; ++dx ) {
            for( int dy = -1; dy <= 1 && !result; ++dy ) {
                const auto nb = ap + point{ dx, dy };
                if( !above->inbounds( nb ) ) {
                    continue; // out of bounds = inside
                }
                const int idx = above->idx( nb.x(), nb.y() );
                if( above->outside_cache[idx] && !above->floor_cache[idx] ) {
                    result = true;
                }
            }
        }
        outside_cache[p.x()][p.y()] = result;
        // A tile is sheltered if any tile in the 3×3 at z+1 has a floor,
        // or is itself sheltered (coverage propagates downward with a 1-tile overhang).
        // Out-of-bounds neighbours are treated as sheltered (edge of loaded map).
        result = false;
        for( int dx = -1; dx <= 1 && !result; ++dx ) {
            for( int dy = -1; dy <= 1 && !result; ++dy ) {
                const auto nb = ap + point{ dx, dy };
                if( !above->inbounds( nb ) ) {
                    continue;
                }
                const int idx = above->idx( nb.x(), nb.y() );
                if( above->floor_cache[idx] || above->sheltered_cache[idx] ) {
                    result = true;
                }
            }
        }
        sheltered_cache[p.x()][p.y()] = result;
    }
    outside_dirty = false;
}

auto submap::rebuild_floor_cache( const map &m, const tripoint_bub_sm &grid_pos ) -> void
{
    if( !floor_dirty ) {
        return;
    }
    // Default: has floor (non-zero).
    std::ranges::fill( std::span( &floor_cache[0][0], SEEX * SEEY ), '\x01' );

    const bool lowest_z = grid_pos.z() <= -OVERMAP_DEPTH;
    const submap *below = lowest_z ? nullptr
                          : m.get_submap_at_grid( grid_pos - tripoint_rel_sm( 0, 0, 1 ) );

    for( const auto &sp : submap_tiles() ) {
        const auto &ter_obj = get_ter( sp ).obj();
        if( ter_obj.has_flag( TFLAG_NO_FLOOR ) || ter_obj.has_flag( TFLAG_Z_TRANSPARENT ) ) {
            if( !below || !below->get_furn( sp ).obj().has_flag( TFLAG_SUN_ROOF_ABOVE ) ) {
                floor_cache[sp.x()][sp.y()] = '\0';
            }
        }
    }
    floor_dirty = false;
}

auto submap::rebuild_pf_cache( const map &m, const tripoint_bub_sm &grid_pos ) -> void
{
    if( !pf_dirty ) {
        return;
    }
    for( const auto &sp : submap_tiles() ) {
        const tripoint_bub_ms p = project_combine( grid_pos, sp );
        auto cur_value = PF_NORMAL;

        const auto &terrain   = get_ter( sp ).obj();
        const auto &furniture = get_furn( sp ).obj();
        int vpart = -1;
        const vehicle *veh = m.veh_at_internal( p, vpart );
        const int cost = m.move_cost_internal( furniture, terrain, veh, vpart );

        if( cost > 2 ) {
            cur_value |= PF_SLOW;
        } else if( cost <= 0 ) {
            cur_value |= PF_WALL;
            if( terrain.has_flag( TFLAG_CLIMBABLE ) ) {
                cur_value |= PF_CLIMBABLE;
            }
        }

        if( veh != nullptr ) {
            cur_value |= PF_VEHICLE;
        }

        for( const auto &fld : get_field( sp ) ) {
            const auto &cur_fld = fld.second;
            if( cur_fld.get_field_type().obj().get_dangerous(
                    cur_fld.get_field_intensity() - 1 ) ) {
                cur_value |= PF_FIELD;
            }
        }

        if( !get_trap( sp ).obj().is_benign() || !terrain.trap.obj().is_benign() ) {
            cur_value |= PF_TRAP;
        }

        if( terrain.has_flag( TFLAG_GOES_DOWN ) || terrain.has_flag( TFLAG_GOES_UP ) ||
            terrain.has_flag( TFLAG_RAMP )      || terrain.has_flag( TFLAG_RAMP_UP ) ||
            terrain.has_flag( TFLAG_RAMP_DOWN ) ) {
            cur_value |= PF_UPDOWN;
        }

        if( terrain.has_flag( TFLAG_SHARP ) ) {
            cur_value |= PF_SHARP;
        }

        pf_special_cache[sp.x()][sp.y()] = cur_value;
    }
    pf_dirty = false;
}

auto submap::rebuild_transparency_cache( const map &m, const tripoint_bub_sm &grid_pos ) -> void
{
    if( !transparency_dirty ) {
        return;
    }
    // outside_cache must be current before applying the weather sight penalty.
    if( outside_dirty ) {
        const level_cache *above = ( grid_pos.z() < OVERMAP_HEIGHT )
                                   ? &m.get_cache_ref( grid_pos.z() + 1 )
                                   : nullptr;
        rebuild_outside_cache( above, grid_pos );
    }

    const float sight_penalty = get_weather().weather_id->sight_penalty;

    for( const auto &sp : submap_tiles() ) {
        if( ( get_ter( sp ).obj().transparent || !get_furn( sp ).obj().transparent ) ) {
            auto value = LIGHT_TRANSPARENCY_OPEN_AIR;
            if( outside_cache[sp.x()][sp.y()] ) {
                value *= sight_penalty;
            }

            for( const auto &fld : get_field( sp ) ) {
                if( !fld.first.is_valid() ) {
                    debugmsg( "rebuild_transparency_cache: invalid field type id %d at "
                              "grid(%d,%d,%d) tile(%d,%d) field_count=%d is_uniform=%d",
                              fld.first.to_i(), grid_pos.x(), grid_pos.y(), grid_pos.z(),
                              sp.x(), sp.y(), field_count, static_cast<int>( is_uniform ) );
                    break;
                }
                const auto &cur = fld.second;
                if( !cur.is_transparent() ) {
                    value *= cur.translucency();
                }
            }
            transparency_cache[sp.x()][sp.y()] = value;
        } else {
            transparency_cache[sp.x()][sp.y()] = LIGHT_TRANSPARENCY_SOLID;
        }
    }
    transparency_dirty = false;
}
