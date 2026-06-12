// GPU Lighting - Seen-Cache Ray-Cast Pass

static const uint SEEN_THREADS_X = 8u;
static const uint SEEN_THREADS_Y = 8u;

[[vk::binding(0, 2)]]
struct SeenConstants {
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
    uint  vision_block_mask;
    uint  vehicle_obscured_z_mask;
    uint2 _pad;
};

[[vk::binding(0, 2)]]
ConstantBuffer<SeenConstants> constants : register(b0, space2);

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
#define vision_block_mask constants.vision_block_mask
#define vehicle_obscured_z_mask constants.vehicle_obscured_z_mask

[[vk::binding(0, 0)]]
StructuredBuffer<float> transparency_all : register(t0, space0);

[[vk::binding(1, 0)]]
StructuredBuffer<uint> floor_all : register(t1, space0);

[[vk::binding(2, 0)]]
StructuredBuffer<uint> vehicle_floor_all : register(t2, space0);

[[vk::binding(3, 0)]]
StructuredBuffer<uint> vehicle_obscured_all : register(t3, space0);

[[vk::binding(0, 1)]]
RWStructuredBuffer<float> seen_all : register(u0, space1);

#define CATA_LM_SEEN_HAS_PLAYER_VISION_BLOCK 1
#include "lm_seen_common.slangh"

float trace_seen_visibility( int tx, int ty, int tz )
{
    int dist_i = visibility_distance( abs( tx - player_x ), abs( ty - player_y ),
                                      abs( tz - player_z_idx ) );
    float dist = float( dist_i );

    if( dist_i == 0 ) {
        return 1.0;
    }

    if( dist_i > view_radius ) {
        return 0.0;
    }

    int sdx = tx - player_x;
    int sdy = ty - player_y;
    int sdz = tz - player_z_idx;

    int scaled_z_steps = int( ceil( float( abs( sdz ) ) * z_scale ) );
    int steps = largest_int_3( abs( sdx ), abs( sdy ), scaled_z_steps );
    if( steps < 1 ) {
        steps = 1;
    }

    float transparency_sum = 0.0;
    int transparency_steps = 0;
    bool blocked = false;
    bool check_vehicle_obscured = vehicle_obscured_z_mask != 0u;

    if( sdz != 0 ) {
        int sign_z = sdz > 0 ? 1 : -1;
        int crossings = abs( sdz );
        for( int k = 0; k < crossings; ++k ) {
            float t_z = ( float( k ) + 0.5 ) / float( crossings );
            int ix_z = bounded_int( player_x + round_nearest_int( float( sdx ) * t_z ),
                                    0, cache_x - 1 );
            int iy_z = bounded_int( player_y + round_nearest_int( float( sdy ) * t_z ),
                                    0, cache_y - 1 );
            if( sign_z < 0 && k == 0 && ix_z == player_x && iy_z == player_y ) {
                continue;
            }
            int floor_z = sign_z > 0 ? player_z_idx + k + 1 : player_z_idx - k;
            if( floor_z >= 0 && floor_z < z_count ) {
                int fl_idx = floor_z * cache_xy + ix_z * cache_y + iy_z;
                if( floor_all[fl_idx] != 0u ) {
                    blocked = true;
                    break;
                }
            }
        }
    }

    int prev_ix = player_x;
    int prev_iy = player_y;
    int prev_iz = player_z_idx;
    float inv_steps = 1.0 / float( steps );
    for( int i = 1; i < steps; ++i ) {
        float t = float( i ) * inv_steps;
        int ix = player_x + round_nearest_int( float( sdx ) * t );
        int iy = player_y + round_nearest_int( float( sdy ) * t );
        int iz = player_z_idx + round_nearest_int( float( sdz ) * t );

        ix = bounded_int( ix, 0, cache_x - 1 );
        iy = bounded_int( iy, 0, cache_y - 1 );
        iz = bounded_int( iz, 0, z_count - 1 );

        if( vision_block_mask != 0u && blocked_by_player_vision_adjustment( ix, iy, iz ) ) {
            blocked = true;
            break;
        }
        if( check_vehicle_obscured && vehicle_obscured_level_active( iz ) ) {
            if( blocked_by_vehicle_diagonal( prev_ix, prev_iy, prev_iz, ix, iy, iz ) ) {
                blocked = true;
                break;
            }
        }

        float t_val = transparency_all[tile_index( ix, iy, iz )];
        if( t_val <= LIGHT_TRANSPARENCY_SOLID ) {
            blocked = true;
            break;
        }

        transparency_sum += t_val;
        ++transparency_steps;
        prev_ix = ix;
        prev_iy = iy;
        prev_iz = iz;
    }

    if( blocked ) {
        return 0.0;
    }

    if( tz < player_z_idx && tz + 1 < z_count ) {
        int roof_idx = tile_index( tx, ty, tz + 1 );
        if( vehicle_floor_all[roof_idx] != 0u ) {
            return 0.0;
        }
    }

    float avg_transparency =
        transparency_steps > 0 ? transparency_sum / float( transparency_steps ) :
        LIGHT_TRANSPARENCY_OPEN_AIR;
    return exp( -avg_transparency * dist );
}

void write_seen_tile( int tx, int ty, int tz )
{
    seen_all[tile_index( tx, ty, tz )] = trace_seen_visibility( tx, ty, tz );
}

void seen_main_impl( uint3 group_id, uint3 group_thread_id )
{
    int tx = player_x + int( group_id.x * SEEN_THREADS_X + group_thread_id.x ) - view_radius;
    int ty = player_y + int( group_id.y * SEEN_THREADS_Y + group_thread_id.y ) - view_radius;
    int tz = z_start_idx + int( group_id.z );

    if( tx < 0 || ty < 0 || tx >= cache_x || ty >= cache_y ||
        group_id.z >= uint( dispatch_z_count ) || tz >= z_count ) {
        return;
    }

    write_seen_tile( tx, ty, tz );
}

void seen_cpu_main_impl( uint3 group_id )
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
        write_seen_tile( tx, ty, tz );
    }
}

[numthreads(8, 8, 1)]
void main( uint3 group_id : SV_GroupID, uint3 group_thread_id : SV_GroupThreadID )
{
    seen_main_impl( group_id, group_thread_id );
}

[numthreads(1, 1, 1)]
void cpu_main( uint3 group_id : SV_GroupID, uint3 group_thread_id : SV_GroupThreadID )
{
    seen_cpu_main_impl( group_id );
}
