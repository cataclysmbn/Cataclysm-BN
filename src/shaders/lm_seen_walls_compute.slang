// GPU Lighting - Seen-Cache Surface Pass

static const float LIGHT_TRANSPARENCY_SOLID    = 0.0;
static const float LIGHT_TRANSPARENCY_OPEN_AIR = 0.038376418216;

static const uint SEEN_WALLS_THREADS_X = 8u;
static const uint SEEN_WALLS_THREADS_Y = 8u;

[[vk::binding(0, 2)]]
struct SeenWallsConstants {
    int   player_x;
    int   player_y;
    int   player_z_idx;
    int   cache_x;
    int   cache_y;
    int   cache_xy;
    int   z_count;
    int   view_radius;
    float z_scale;
    int   z_start_idx;
    int   dispatch_z_count;
    uint  trigdist;
};

[[vk::binding(0, 2)]]
ConstantBuffer<SeenWallsConstants> constants : register(b0, space2);

#define player_x constants.player_x
#define player_y constants.player_y
#define player_z_idx constants.player_z_idx
#define cache_x constants.cache_x
#define cache_y constants.cache_y
#define cache_xy constants.cache_xy
#define z_count constants.z_count
#define view_radius constants.view_radius
#define z_scale constants.z_scale
#define z_start_idx constants.z_start_idx
#define dispatch_z_count constants.dispatch_z_count
#define trigdist constants.trigdist

[[vk::binding(0, 0)]]
StructuredBuffer<float> transparency_all : register(t0, space0);

[[vk::binding(1, 0)]]
StructuredBuffer<float> seen_src_all : register(t1, space0);

[[vk::binding(2, 0)]]
StructuredBuffer<uint> vehicle_floor_all : register(t2, space0);

[[vk::binding(3, 0)]]
StructuredBuffer<uint> vehicle_obscured_all : register(t3, space0);

[[vk::binding(0, 1)]]
RWStructuredBuffer<float> seen_dst_all : register(u0, space1);

int tile_index( int x, int y, int z )
{
    return z * cache_xy + x * cache_y + y;
}

int visibility_distance( int dx, int dy, int dz )
{
    if( trigdist == 0u ) {
        int best = dx;
        if( dy > best ) {
            best = dy;
        }
        if( dz > best ) {
            best = dz;
        }
        return best;
    }
    return int( round( sqrt( float( dx * dx + dy * dy + dz * dz ) ) ) );
}

bool blocked_by_vehicle_diagonal( int from_x, int from_y, int to_x, int to_y, int z )
{
    int dx = to_x - from_x;
    int dy = to_y - from_y;
    if( abs( dx ) != 1 || abs( dy ) != 1 ) {
        return false;
    }
    if( dx == -1 && dy == -1 ) {
        return ( vehicle_obscured_all[tile_index( from_x, from_y, z )] & 1u ) != 0u;
    }
    if( dx == 1 && dy == -1 ) {
        return ( vehicle_obscured_all[tile_index( from_x, from_y, z )] & 2u ) != 0u;
    }
    if( dx == -1 && dy == 1 ) {
        return ( vehicle_obscured_all[tile_index( to_x, to_y, z )] & 2u ) != 0u;
    }
    return ( vehicle_obscured_all[tile_index( to_x, to_y, z )] & 1u ) != 0u;
}

void seen_walls_main_impl( uint3 group_id, uint3 group_thread_id )
{
    int tx = player_x + int( group_id.x * SEEN_WALLS_THREADS_X + group_thread_id.x ) -
             view_radius;
    int ty = player_y + int( group_id.y * SEEN_WALLS_THREADS_Y + group_thread_id.y ) -
             view_radius;
    int tz = z_start_idx + int( group_id.z );

    if( tx < 0 || ty < 0 || tx >= cache_x || ty >= cache_y ||
        group_id.z >= uint( dispatch_z_count ) || tz >= z_count ) {
        return;
    }

    int dist = visibility_distance( abs( tx - player_x ), abs( ty - player_y ),
                                    abs( tz - player_z_idx ) );

    int idx = tile_index( tx, ty, tz );

    if( dist == 0 ) {
        seen_dst_all[idx] = 1.0;
        return;
    }

    if( dist > view_radius ) {
        seen_dst_all[idx] = 0.0;
        return;
    }

    float primary = seen_src_all[idx];

    if( transparency_all[idx] == LIGHT_TRANSPARENCY_OPEN_AIR ) {
        seen_dst_all[idx] = primary;
        return;
    }

    float best = primary;

    int2 offsets[8] = {
        int2(  0,  1 ),
        int2(  0, -1 ),
        int2(  1,  0 ),
        int2(  1,  1 ),
        int2(  1, -1 ),
        int2( -1,  0 ),
        int2( -1,  1 ),
        int2( -1, -1 )
    };

    [unroll]
    for( int i = 0; i < 8; ++i ) {
        int nx = tx + offsets[i].x;
        int ny = ty + offsets[i].y;
        if( nx < 0 || ny < 0 || nx >= cache_x || ny >= cache_y ) {
            continue;
        }

        int nidx = tile_index( nx, ny, tz );
        if( transparency_all[nidx] <= LIGHT_TRANSPARENCY_SOLID ) {
            continue;
        }
        if( blocked_by_vehicle_diagonal( nx, ny, tx, ty, tz ) ) {
            continue;
        }

        float candidate = seen_src_all[nidx];
        if( candidate > best ) {
            best = candidate;
        }
    }

    if( tz < player_z_idx && tz + 1 < z_count ) {
        int roof_idx = tile_index( tx, ty, tz + 1 );
        if( vehicle_floor_all[roof_idx] != 0u ) {
            best = 0.0;
        }
    }

    seen_dst_all[idx] = best;
}

[numthreads(8, 8, 1)]
void main( uint3 group_id : SV_GroupID, uint3 group_thread_id : SV_GroupThreadID )
{
    seen_walls_main_impl( group_id, group_thread_id );
}

[numthreads(8, 8, 1)]
void cpu_main( uint3 group_id : SV_GroupID, uint3 group_thread_id : SV_GroupThreadID )
{
    seen_walls_main_impl( group_id, group_thread_id );
}
