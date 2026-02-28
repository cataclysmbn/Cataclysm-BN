#include "map_memory.h"

#include "coordinate_conversions.h"
#include "cuboid_rectangle.h"
#include "debug.h"
#include "filesystem.h"
#include "fstream_utils.h"
#include "game.h"
#include "line.h"
#include "translations.h"
#include "map.h"
#include "world.h"
const memorized_terrain_tile mm_submap::default_tile { "", 0, 0 };
const int mm_submap::default_symbol = 0;

#define MM_SIZE (MAPSIZE * 2)

#define dbg(x) DebugLog((x),DC::MapMem)

/**
 * Helper class for converting global sm coord into
 * global mm_region coord + sm coord within the region.
 */
struct reg_coord_pair {
    tripoint reg;
    point sm_loc;

    reg_coord_pair( const tripoint &p ) : sm_loc( p.xy() ) {
        reg = tripoint( sm_to_mmr_remain( sm_loc.x, sm_loc.y ), p.z );
    }
};

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

map_memory::coord_pair::coord_pair( const tripoint &p ) : loc( p.xy() )
{
    sm = tripoint( ms_to_sm_remain( loc.x, loc.y ), p.z );
}

map_memory::map_memory()
{
    clear_cache();
}

const memorized_terrain_tile &map_memory::get_tile( const tripoint &pos )
{
    coord_pair p( pos );
    const mm_submap &sm = get_submap( p.sm );
    return sm.tile( p.loc );
}

bool map_memory::has_memory_for_autodrive( const tripoint &pos )
{
    // HACK: Map memory is not supposed to be used by ingame mechanics.
    //       It's just a graphical overlay, it memorizes tileset tiles and text symbols.
    //       Problem is, many cars' headlights won't cover every ground tile in front of them at night,
    //       and these dark tiles would be considered as possible obstacles.
    //       To work around it, we check for whether map memory has any data associated with the tile
    //       and then assume it's up to date, which works in 99% cases.
    //       Oh, and we don't want to use get_tile() and get_symbol() to avoid looking up the mm_submap twice.
    coord_pair p( pos );
    shared_ptr_fast<mm_submap> sm = fetch_submap( p.sm );
    return sm->tile( p.loc ) != mm_submap::default_tile ||
           sm->symbol( p.loc ) != mm_submap::default_symbol;
}

void map_memory::memorize_tile( const tripoint &pos, const std::string &ter,
                                const int subtile, const int rotation )
{
    coord_pair p( pos );
    mm_submap &sm = get_submap( p.sm );
    sm.set_tile( p.loc, memorized_terrain_tile{ ter, subtile, rotation } );
}

int map_memory::get_symbol( const tripoint &pos )
{
    coord_pair p( pos );
    const mm_submap &sm = get_submap( p.sm );
    return sm.symbol( p.loc );
}

void map_memory::memorize_symbol( const tripoint &pos, const int symbol )
{
    coord_pair p( pos );
    mm_submap &sm = get_submap( p.sm );
    sm.set_symbol( p.loc, symbol );
}

void map_memory::clear_memorized_tile( const tripoint &pos )
{
    coord_pair p( pos );
    mm_submap &sm = get_submap( p.sm );
    sm.set_symbol( p.loc, mm_submap::default_symbol );
    sm.set_tile( p.loc, mm_submap::default_tile );
}

bool map_memory::prepare_region( const tripoint &p1, const tripoint &p2 )
{
    assert( p1.z == p2.z );
    assert( p1.x <= p2.x && p1.y <= p2.y );

    tripoint sm_p1 = coord_pair( p1 ).sm - point_south_east;
    tripoint sm_p2 = coord_pair( p2 ).sm + point_south_east;

    tripoint sm_pos = sm_p1;
    point sm_size = sm_p2.xy() - sm_p1.xy();

    bool z_levels = get_map().has_zlevels();

    if( sm_pos.z == cache_pos.z || z_levels ) {
        inclusive_rectangle<point> rect( cache_pos.xy(), cache_pos.xy() + cache_size );
        if( rect.contains( sm_p1.xy() ) && rect.contains( sm_p2.xy() ) ) {
            return false;
        }
    }

    dbg( DL::Info ) << "Preparing memory map for area: pos: " << sm_pos << " size: " << sm_size;


    int minz = z_levels ? -OVERMAP_DEPTH : p1.z;
    int maxz = z_levels ? OVERMAP_HEIGHT : p1.z;

    cache_pos = sm_pos;
    cache_size = sm_size;

    cached.clear();
    cached.reserve( cache_size.x * cache_size.y * ( maxz - minz + 1 ) );

    for( int z = minz; z <= maxz; z++ ) {
        for( int dy = 0; dy < cache_size.y; dy++ ) {
            for( int dx = 0; dx < cache_size.x; dx++ ) {
                cached.push_back( fetch_submap( tripoint( cache_pos.xy(), z ) + point( dx, dy ) ) );
            }
        }
    }
    return true;
}

shared_ptr_fast<mm_submap> map_memory::fetch_submap( const tripoint &sm_pos )
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

shared_ptr_fast<mm_submap> map_memory::allocate_submap( const tripoint &sm_pos )
{
    // Since all save/load operations are done on regions of submaps,
    // we need to allocate the whole region at once.
    shared_ptr_fast<mm_submap> ret;
    tripoint reg = reg_coord_pair( sm_pos ).reg;

    dbg( DL::Info ) << "Allocated mm_region " << reg << " [" << mmr_to_sm_copy( reg ) << "]";

    for( size_t y = 0; y < MM_REG_SIZE; y++ ) {
        for( size_t x = 0; x < MM_REG_SIZE; x++ ) {
            tripoint pos = mmr_to_sm_copy( reg ) + tripoint( x, y, 0 );
            shared_ptr_fast<mm_submap> sm = make_shared_fast<mm_submap>();
            if( pos == sm_pos ) {
                ret = sm;
            }
            submaps.insert( std::make_pair( pos, sm ) );
        }
    }

    return ret;
}

shared_ptr_fast<mm_submap> map_memory::find_submap( const tripoint &sm_pos )
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
    for( int x = 0; x < SEEX; x++ ) {
        for( int y = 0; y < SEEY; y++ ) {
            const memorized_terrain_tile &t = sm->tile( {x, y} );

            if( !t.tile.empty() && ( t.tile == "t_open_air" || t.tile == "t_open_air_rooved" ||
                                     t.tile == "t_open_air_rooved_outside" ) ) {
                sm->set_tile( {x, y}, mm_submap::default_tile );
            }
        }
    }

}

shared_ptr_fast<mm_submap> map_memory::load_submap( const tripoint &sm_pos )
{
    if( test_mode ) {
        return nullptr;
    }

    reg_coord_pair p( sm_pos );

    mm_region mmr;
    const auto loader = [&]( JsonIn & jsin ) {
        mmr.deserialize( jsin );
    };

    try {
        if( !g->get_active_world()->read_player_mm_quad( p.reg, loader ) ) {
            // Region not found
            return nullptr;
        }
    } catch( const std::exception &err ) {
        debugmsg( "Failed to load memory map region (%d,%d,%d): %s",
                  p.reg.x, p.reg.y, p.reg.z, err.what() );
        return nullptr;
    }

    dbg( DL::Info ) << "Loaded mm_region " << p.reg << " [" << mmr_to_sm_copy( p.reg ) << "]";

    shared_ptr_fast<mm_submap> ret;

    for( size_t y = 0; y < MM_REG_SIZE; y++ ) {
        for( size_t x = 0; x < MM_REG_SIZE; x++ ) {
            tripoint pos = mmr_to_sm_copy( p.reg ) + tripoint( x, y, 0 );
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

mm_submap &map_memory::get_submap( const tripoint &sm_pos )
{
    // First, try fetching from cache.
    // If it's not in cache (or cache is absent), go the long way.
    if( cache_pos != tripoint_min ) {
        int zoffset = get_map().has_zlevels()
                      ? ( sm_pos.z + OVERMAP_DEPTH ) * cache_size.y * cache_size.x
                      : 0;
        const point idx = ( sm_pos - cache_pos ).xy();
        if( idx.x > 0 && idx.y > 0 && idx.x < cache_size.x && idx.y < cache_size.y ) {
            return *cached[idx.y * cache_size.x + idx.x + zoffset];
        }
    }
    return *fetch_submap( sm_pos );
}

void map_memory::load( const tripoint &pos )
{
    clear_cache();

    coord_pair p( pos );
    tripoint start = p.sm - tripoint( MM_SIZE / 2, MM_SIZE / 2, 0 );
    dbg( DL::Info ) << "[LOAD] Loading memory map around " << p.sm << ". Loading submaps within "
                    << start << "->" << start + tripoint( MM_SIZE, MM_SIZE, 0 );
    for( int dy = 0; dy < MM_SIZE; dy++ ) {
        for( int dx = 0; dx < MM_SIZE; dx++ ) {
            fetch_submap( start + tripoint( dx, dy, 0 ) );
        }
    }
    dbg( DL::Info ) << "[LOAD] Done.";
}

bool map_memory::save( const tripoint &pos )
{
    tripoint sm_center = coord_pair( pos ).sm;

    clear_cache();

    dbg( DL::Info ) << "N submaps before save: " << submaps.size();

    // Since mm_submaps are always allocated in regions,
    // we are certain that each region will be filled.
    std::map<tripoint, mm_region> regions;
    for( auto &it : submaps ) {
        const reg_coord_pair p( it.first );
        regions[p.reg].submaps[p.sm_loc.x][p.sm_loc.y] = it.second;
    }
    submaps.clear();

    constexpr point MM_HSIZE_P = point( MM_SIZE / 2, MM_SIZE / 2 );
    half_open_rectangle<point> rect_keep( sm_center.xy() - MM_HSIZE_P, sm_center.xy() + MM_HSIZE_P );

    dbg( DL::Info ) << "[SAVE] Saving memory map around " << sm_center << ". Keeping submaps within "
                    << rect_keep.p_min << "->" << rect_keep.p_max;

    bool result = true;

    for( auto &it : regions ) {
        const tripoint &regp = it.first;
        mm_region &reg = it.second;
        if( !reg.is_empty() ) {
            const auto writer = [&]( std::ostream & fout ) -> void {
                fout << serialize_wrapper( [&]( JsonOut & jsout )
                {
                    reg.serialize( jsout );
                } );
            };

            const bool res = g->get_active_world()->write_player_mm_quad( regp, writer );
            result = result & res;
        }
        tripoint regp_sm = mmr_to_sm_copy( regp );
        half_open_rectangle<point> rect_reg(
            regp_sm.xy(),
            regp_sm.xy() + point( MM_REG_SIZE, MM_REG_SIZE )
        );
        if( rect_reg.overlaps( rect_keep ) ) {
            dbg( DL::Info ) << "Keeping mm_region " << regp << " [" << regp_sm << "]";
            // Put submaps back
            for( size_t y = 0; y < MM_REG_SIZE; y++ ) {
                for( size_t x = 0; x < MM_REG_SIZE; x++ ) {
                    tripoint p = regp_sm + tripoint( x, y, 0 );
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
    cache_pos = tripoint_min;
    cache_size = point_zero;
}
