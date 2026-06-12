// GPU Lighting - Seen-Cache Surface Pass

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
    uint  vision_block_mask_unused;
    uint  vehicle_obscured_z_mask;
    uint2 _pad;
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
#define vehicle_obscured_z_mask constants.vehicle_obscured_z_mask

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

#include "lm_seen_common.slangh"

float trace_seen_wall_visibility( int tx, int ty, int tz )
{
    int dist = visibility_distance( abs( tx - player_x ), abs( ty - player_y ),
                                    abs( tz - player_z_idx ) );

    int idx = tile_index( tx, ty, tz );

    if( dist == 0 ) {
        return 1.0;
    }

    if( dist > view_radius ) {
        return 0.0;
    }

    float primary = seen_src_all[idx];

    if( transparency_all[idx] == LIGHT_TRANSPARENCY_OPEN_AIR ) {
        return primary;
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

    bool check_vehicle_obscured = vehicle_obscured_z_mask != 0u && vehicle_obscured_level_active( tz );
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
        if( check_vehicle_obscured && blocked_by_vehicle_diagonal( nx, ny, tz, tx, ty, tz ) ) {
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
            return 0.0;
        }
    }

    return best;
}

void write_seen_wall_tile( int tx, int ty, int tz )
{
    int idx = tile_index( tx, ty, tz );
    float best = trace_seen_wall_visibility( tx, ty, tz );
    seen_dst_all[idx] = best;
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

    write_seen_wall_tile( tx, ty, tz );
}

void seen_walls_cpu_main_impl( uint3 group_id )
{
    int tx = player_x + int( group_id.x ) - view_radius;
    int tz = z_start_idx + int( group_id.z );

    if( tx < 0 || tx >= cache_x || group_id.z >= uint( dispatch_z_count ) || tz >= z_count ) {
        return;
    }

    int max_dy = max_visible_y_delta_for_column( abs( tx - player_x ), abs( tz - player_z_idx ) );
    if( max_dy < 0 ) {
        return;
    }

    int y_start = bounded_int( player_y - max_dy, 0, cache_y );
    int y_end = bounded_int( player_y + max_dy + 1, 0, cache_y );
    for( int ty = y_start; ty < y_end; ++ty ) {
        write_seen_wall_tile( tx, ty, tz );
    }
}

[numthreads(8, 8, 1)]
void main( uint3 group_id : SV_GroupID, uint3 group_thread_id : SV_GroupThreadID )
{
    seen_walls_main_impl( group_id, group_thread_id );
}

[numthreads(1, 1, 1)]
void cpu_main( uint3 group_id : SV_GroupID, uint3 group_thread_id : SV_GroupThreadID )
{
    seen_walls_cpu_main_impl( group_id );
}
