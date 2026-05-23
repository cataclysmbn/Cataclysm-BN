#include "map_memory.h"

#include "cuboid_rectangle.h"
#include "debug.h"
#include "filesystem.h"
#include "fstream_utils.h"
#include "game.h"
#include "game_constants.h"
#include "line.h"
#include "translations.h"
#include "map.h"
#include "world.h"
const memorized_terrain_tile mm_submap::default_tile { "", 0, 0 };
const int mm_submap::default_symbol = 0;

// MM_SIZE = g_mapsize * 2 (runtime; used inline where needed)

#define dbg(x) DebugLog((x),DC::MapMem)

mm_submap::mm_submap() = default;

mm_region::mm_region() : submaps {{ nullptr }} {}

bool mm_region::is_empty() const
{
    for( const auto &itt : submaps ) {
        for( const shared_ptr_fast<mm_submap> &it : itt ) {
            if( !it->is_empty() ) {
                return false;
            }
        }
    }
    return true;
}

map_memory::map_memory()
{
    clear_cache();
}

const memorized_terrain_tile &map_memory::get_tile( const tripoint_abs_ms &pos )
{
    const auto p = project_remain<coords::sm>( pos );
    const mm_submap &sm = get_submap( p.quotient_tripoint );
    return sm.tile( p.remainder );
}

bool map_memory::has_memory_for_autodrive( const tripoint_abs_ms &pos )
{
    // HACK: Map memory is not supposed to be used by ingame mechanics.
    //       It's just a graphical overlay, it memorizes tileset tiles and text symbols.
    //       Problem is, many cars' headlights won't cover every ground tile in front of them at night,
    //       and these dark tiles would be considered as possible obstacles.
    //       To work around it, we check for whether map memory has any data associated with the tile
    //       and then assume it's up to date, which works in 99% cases.
    //       Oh, and we don't want to use get_tile() and get_symbol() to avoid looking up the mm_submap twice.
    const auto p = project_remain<coords::sm>( pos );
    shared_ptr_fast<mm_submap> sm = fetch_submap( p.quotient_tripoint );
    return sm->tile( p.remainder ) != mm_submap::default_tile ||
           sm->symbol( p.remainder ) != mm_submap::default_symbol;
}

void map_memory::memorize_tile( const tripoint_abs_ms &pos, const std::string &ter,
                                const int subtile, const int rotation )
{
    const auto p = project_remain<coords::sm>( pos );
    mm_submap &sm = get_submap( p.quotient_tripoint );
    sm.set_tile( p.remainder, memorized_terrain_tile{ ter, subtile, rotation } );
}

int map_memory::get_symbol( const tripoint_abs_ms &pos )
{
    const auto p = project_remain<coords::sm>( pos );
    mm_submap &sm = get_submap( p.quotient_tripoint );
    return sm.symbol( p.remainder );
}

void map_memory::memorize_symbol( const tripoint_abs_ms &pos, const int symbol )
{
    const auto p = project_remain<coords::sm>( pos );
    mm_submap &sm = get_submap( p.quotient_tripoint );
    sm.set_symbol( p.remainder, symbol );
}

void map_memory::memorize_terrain_tile( const tripoint_abs_ms &pos, const std::string &ter,
                                        const int subtile, const int rotation )
{
    const auto p = project_remain<coords::sm>( pos );
    mm_submap &sm = get_submap( p.quotient_tripoint );
    sm.set_terrain_tile( p.remainder, memorized_terrain_tile{ ter, subtile, rotation } );
}

memorized_terrain_tile map_memory::get_terrain_tile( const tripoint_abs_ms &pos )
{
    const auto p = project_remain<coords::sm>( pos );
    mm_submap &sm = get_submap( p.quotient_tripoint );
    return sm.terrain_tile( p.remainder );
}

void map_memory::clear_memorized_overlay( const tripoint_abs_ms &pos )
{
    const auto p = project_remain<coords::sm>( pos );
    mm_submap &sm = get_submap( p.quotient_tripoint );
    sm.set_symbol( p.remainder, mm_submap::default_symbol );
    sm.set_tile( p.remainder, mm_submap::default_tile );
}

void map_memory::clear_memorized_tile( const tripoint_abs_ms &pos )
{
    const auto p = project_remain<coords::sm>( pos );
    mm_submap &sm = get_submap( p.quotient_tripoint );
    sm.set_symbol( p.remainder, mm_submap::default_symbol );
    sm.set_tile( p.remainder, mm_submap::default_tile );
    sm.set_terrain_tile( p.remainder, mm_submap::default_tile );
}

bool map_memory::prepare_region( const tripoint_abs_ms &p1, const tripoint_abs_ms &p2 )
{
    assert( p1.z() == p2.z() );
    assert( p1.x() <= p2.x() && p1.y() <= p2.y() );

    const auto sm_p1 = project_to<coords::sm>( p1 ) + point_south_west;
    const auto sm_p2 = project_to<coords::sm>( p2 ) + point_south_east;

    const auto sm_size = sm_p2.xy() - sm_p1.xy();

    bool z_levels = get_map().has_zlevels();

    if( sm_p1.z() == cache_pos.z() || z_levels ) {
        inclusive_rectangle<point_abs_sm> rect( cache_pos.xy(), cache_pos.xy() + cache_size );
        if( rect.contains( sm_p1.xy() ) && rect.contains( sm_p2.xy() ) ) {
            return false;
        }
    }

    dbg( DL::Info ) << "Preparing memory map for area: pos: " << sm_p1 << " size: " << sm_size;


    int minz = z_levels ? -OVERMAP_DEPTH : p1.z();
    int maxz = z_levels ? OVERMAP_HEIGHT : p1.z();

    cache_pos = sm_p1;
    cache_size = sm_size;

    cached.clear();
    cached.reserve( cache_size.x() * cache_size.y() * ( maxz - minz + 1 ) );

    for( int z = minz; z <= maxz; z++ ) {
        for( int dy = 0; dy < cache_size.y(); dy++ ) {
            for( int dx = 0; dx < cache_size.x(); dx++ ) {
                cached.push_back( fetch_submap( tripoint_abs_sm( cache_pos.xy(), z ) + point_rel_sm( dx, dy ) ) );
            }
        }
    }
    return true;
}

shared_ptr_fast<mm_submap> map_memory::fetch_submap( const tripoint_abs_sm &sm_pos )
{
    shared_ptr_fast<mm_submap> sm = find_submap( sm_pos );
    if( sm ) {
        return sm;
    }
    sm = load_submap( sm_pos );
    if( sm ) {
        return sm;
    }
    return allocate_submap( sm_pos );
}

shared_ptr_fast<mm_submap> map_memory::allocate_submap( const tripoint_abs_sm &sm_pos )
{
    // Since all save/load operations are done on regions of submaps,
    // we need to allocate the whole region at once.
    shared_ptr_fast<mm_submap> ret;
    const auto reg = project_to<coords::mmr>( sm_pos );

    dbg( DL::Info ) << "Allocated mm_region " << reg << " [" << project_to<coords::sm>( reg ) << "]";

    for( size_t y = 0; y < MM_REG_SIZE; y++ ) {
        for( size_t x = 0; x < MM_REG_SIZE; x++ ) {
            const auto pos = project_combine( reg, point_mmr_sm( x, y ) );
            shared_ptr_fast<mm_submap> sm = make_shared_fast<mm_submap>();
            if( pos == sm_pos ) {
                ret = sm;
            }
            submaps.insert( std::make_pair( pos, sm ) );
        }
    }

    return ret;
}

shared_ptr_fast<mm_submap> map_memory::find_submap( const tripoint_abs_sm &sm_pos )
{
    auto sm = submaps.find( sm_pos );
    if( sm == submaps.end() ) {
        return nullptr;
    } else {
        return sm->second;
    }
}

//FIXME: This is to fix old (mid 2022) saves. It can be removed at some point.
static void temp_remove_open_air( const shared_ptr_fast<mm_submap> &sm )
{
    if( sm->is_empty() ) {
        return;
    }
    for( const auto sm_ms : submap_tiles() ) {
        const memorized_terrain_tile &t = sm->tile( sm_ms );

        if( !t.tile.empty() && ( t.tile == "t_open_air" || t.tile == "t_open_air_rooved" ||
                                 t.tile == "t_open_air_rooved_outside" ) ) {
            sm->set_tile( sm_ms, mm_submap::default_tile );
        }
    }
}

shared_ptr_fast<mm_submap> map_memory::load_submap( const tripoint_abs_sm &sm_pos )
{
    if( test_mode ) {
        return nullptr;
    }

    const auto reg = project_to<coords::mmr>( sm_pos );

    mm_region mmr;
    const auto loader = [&]( JsonIn & jsin ) {
        mmr.deserialize( jsin );
    };

    try {
        if( !g->get_active_world()->read_player_mm_omt( reg, loader ) ) {
            // Region not found
            return nullptr;
        }
    } catch( const std::exception &err ) {
        debugmsg( "Failed to load memory map region (%d,%d,%d): %s",
                  reg.x(), reg.y(), reg.z(), err.what() );
        return nullptr;
    }

    dbg( DL::Info ) << "Loaded mm_region " << reg << " [" << project_to<coords::sm>( reg ) << "]";

    shared_ptr_fast<mm_submap> ret;

    for( size_t y = 0; y < MM_REG_SIZE; y++ ) {
        for( size_t x = 0; x < MM_REG_SIZE; x++ ) {
            const auto pos = project_combine( reg, point_mmr_sm( x, y ) );
            shared_ptr_fast<mm_submap> &sm = mmr.submaps[x][y];
            if( pos == sm_pos ) {
                ret = sm;
            }

            temp_remove_open_air( mmr.submaps[x][y] );

            submaps.insert( std::make_pair( pos, sm ) );
        }
    }

    return ret;
}

mm_submap &map_memory::get_submap( const tripoint_abs_sm &sm_pos )
{
    // First, try fetching from cache.
    // If it's not in cache (or cache is absent), go the long way.
    if( cache_pos != tripoint_abs_sm::zero() ) {
        int zoffset = get_map().has_zlevels()
                      ? ( sm_pos.z() + OVERMAP_DEPTH ) * cache_size.y() * cache_size.x()
                      : 0;
        const point_rel_sm idx = ( sm_pos - cache_pos ).xy();
        if( idx.x() > 0 && idx.y() > 0 && idx.x() < cache_size.x() && idx.y() < cache_size.y() ) {
            return *cached[idx.y() * cache_size.x() + idx.x() + zoffset];
        }
    }
    return *fetch_submap( sm_pos );
}

void map_memory::load( const tripoint_abs_ms &pos )
{
    clear_cache();

    const int mm_size = g_mapsize * 2;
    const auto sm = project_to<coords::sm>( pos );
    const auto start = sm - tripoint( mm_size / 2, mm_size / 2, 0 );
    dbg( DL::Info ) << "[LOAD] Loading memory map around " << sm << ". Loading submaps within "
                    << start << "->" << start + tripoint( mm_size, mm_size, 0 );
    for( int dy = 0; dy < mm_size; dy++ ) {
        for( int dx = 0; dx < mm_size; dx++ ) {
            fetch_submap( start + tripoint( dx, dy, 0 ) );
        }
    }
    dbg( DL::Info ) << "[LOAD] Done.";
}

bool map_memory::save( const tripoint_abs_ms &pos )
{
    const auto sm_center = project_to<coords::sm>( pos );

    clear_cache();

    dbg( DL::Info ) << "N submaps before save: " << submaps.size();

    // Since mm_submaps are always allocated in regions,
    // we are certain that each region will be filled.
    std::map<tripoint_abs_mmr, mm_region> regions;
    for( auto &it : submaps ) {
        const auto reg = project_to<coords::mmr>( it.first );
        const auto within_reg = project_remain<coords::mmr>( it.first ).remainder;
        regions[reg].submaps[within_reg.x()][within_reg.y()] = it.second;
    }
    submaps.clear();

    const point mm_hsize_p( g_mapsize, g_mapsize );
    half_open_rectangle<point_abs_sm> rect_keep( sm_center.xy() - mm_hsize_p,
            sm_center.xy() + mm_hsize_p );

    dbg( DL::Info ) << "[SAVE] Saving memory map around " << sm_center << ". Keeping submaps within "
                    << rect_keep.p_min << "->" << rect_keep.p_max;

    bool result = true;

    for( auto &it : regions ) {
        const auto &regp = it.first;
        mm_region &reg = it.second;
        if( !reg.is_empty() ) {
            const auto writer = [&]( std::ostream & fout ) -> void {
                fout << serialize_wrapper( [&]( JsonOut & jsout )
                {
                    reg.serialize( jsout );
                } );
            };

            const bool res = g->get_active_world()->write_player_mm_omt( regp, writer );
            result = result & res;
        }
        const auto regp_sm = project_to<coords::sm>( regp );
        half_open_rectangle<point_abs_sm> rect_reg(
            regp_sm.xy(),
            regp_sm.xy() + point_rel_sm( MM_REG_SIZE, MM_REG_SIZE )
        );
        if( rect_reg.overlaps( rect_keep ) ) {
            dbg( DL::Info ) << "Keeping mm_region " << regp << " [" << regp_sm << "]";
            // Put submaps back
            for( size_t y = 0; y < MM_REG_SIZE; y++ ) {
                for( size_t x = 0; x < MM_REG_SIZE; x++ ) {
                    const auto p = regp_sm + point_rel_sm( x, y );
                    shared_ptr_fast<mm_submap> &sm = reg.submaps[x][y];
                    submaps.insert( std::make_pair( p, sm ) );
                }
            }
        } else {
            dbg( DL::Info ) << "Dropping mm_region " << regp << " [" << regp_sm << "]";
        }
    }

    dbg( DL::Info ) << "[SAVE] Done.";
    dbg( DL::Info ) << "N submaps after save: " << submaps.size();

    return result;
}

void map_memory::clear()
{
    clear_cache();
    submaps.clear();
    dbg( DL::Info ) << "[CLEAR] Done.";
}

void map_memory::clear_cache()
{
    cached.clear();
    cache_pos = tripoint_abs_sm::zero();
    cache_size = point_rel_sm::zero();
}
