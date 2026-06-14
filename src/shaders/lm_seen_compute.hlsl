// GPU Lighting — Seen-Cache Ray-Cast Pass
//
// Computes seen_cache for the player's point of view.
// One thread per target tile (x, y); z-level determined by group_id.z.
//
// Algorithm per thread:
//   1. Map group/thread IDs to a target tile (tx, ty, tz).
//   2. Compute gameplay view distance from player; skip if beyond view_radius.
//   3. Trace a DDA ray from player toward target, accumulating a running-
//      average transparency (mirrors accumulate_transparency on CPU).
//      Vertical steps check floor_all to block the ray.  Vehicle roofs are
//      applied as a fixed vertical stamp directly beneath the roof.
//   4. seen_all[target] = exp(-avg_transparency * gameplay_distance).
//      This matches the CPU k_sight_model decay: values in [0, 1] that
//      apparent_light_helper then multiplies by lm to get apparent brightness.
//      Player's own tile is always VISIBILITY_FULL = 1.0.
//
// Binding layout (SDL3 GPU / shadercross HLSL conventions):
//   space0  read-only storage buffers  (t registers)
//   space1  read-write storage buffers (u registers)
//   space2  uniform / cbuffer          (b registers)
//
// Thread group: [8, 8, 1]
// Dispatch: (ceil((2*view_radius+1)/8), ceil((2*view_radius+1)/8), z_count)
//   group_id.x/y = 2D chunk index
//   group_id.z   = target z_idx  (0..z_count-1)

static const float LIGHT_TRANSPARENCY_SOLID    = 0.0;
static const float LIGHT_TRANSPARENCY_OPEN_AIR = 0.038376418216;

cbuffer Constants : register(b0, space2)
{
    int   player_x;
    int   player_y;
    int   player_z_idx;
    int   cache_x;
    int   cache_y;
    int   cache_xy;     // = cache_x * cache_y
    int   z_count;
    int   view_radius;
    float z_scale;
    int   z_start_idx;
    int   dispatch_z_count;
    uint  trigdist;
    uint  vision_block_mask;
    uint3 _pad;
};

StructuredBuffer<float> transparency_all  : register(t0, space0);
StructuredBuffer<uint>  floor_all         : register(t1, space0);
StructuredBuffer<uint>  vehicle_floor_all : register(t2, space0);
StructuredBuffer<uint>  vehicle_obscured_all : register(t3, space0);

RWStructuredBuffer<float> seen_all : register(u0, space1);

int round_nearest_int( float value )
{
    return value >= 0.0 ? (int)floor( value + 0.5 ) : (int)ceil( value - 0.5 );
}

int visibility_distance( int dx, int dy, int dz )
{
    if( trigdist == 0u ) {
        return max( dx, max( dy, dz ) );
    }
    return (int)round( sqrt( (float)( dx * dx + dy * dy + dz * dz ) ) );
}

uint adjacent_block_bit( int rx, int ry )
{
    if( rx ==  0 && ry == -1 ) { return 1u << 0; }
    if( rx ==  1 && ry == -1 ) { return 1u << 1; }
    if( rx ==  1 && ry ==  0 ) { return 1u << 2; }
    if( rx ==  1 && ry ==  1 ) { return 1u << 3; }
    if( rx ==  0 && ry ==  1 ) { return 1u << 4; }
    if( rx == -1 && ry ==  1 ) { return 1u << 5; }
    if( rx == -1 && ry ==  0 ) { return 1u << 6; }
    if( rx == -1 && ry == -1 ) { return 1u << 7; }
    return 0u;
}

bool blocked_by_player_vision_adjustment( int x, int y, int z )
{
    if( vision_block_mask == 0u || z != player_z_idx ) {
        return false;
    }
    uint bit = adjacent_block_bit( x - player_x, y - player_y );
    return bit != 0u && ( vision_block_mask & bit ) != 0u;
}

int tile_index( int x, int y, int z )
{
    return z * cache_xy + x * cache_y + y;
}

bool blocked_by_vehicle_diagonal( int from_x, int from_y, int from_z, int to_x, int to_y, int to_z )
{
    int z = to_z;
    int dx = to_x - from_x;
    int dy = to_y - from_y;
    if( from_z != to_z ) {
        dx = to_x - from_x;
        dy = to_y - from_y;
    }
    if( z < 0 || z >= z_count || abs( dx ) != 1 || abs( dy ) != 1 ) {
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

[numthreads(8, 8, 1)]
void main( uint3 group_id : SV_GroupID, uint3 thread_id : SV_GroupThreadID )
{
    // Map to target tile.
    int tx = player_x + (int)( group_id.x * 8 + thread_id.x ) - view_radius;
    int ty = player_y + (int)( group_id.y * 8 + thread_id.y ) - view_radius;
    int tz = z_start_idx + (int)group_id.z;

    if( tx < 0 || ty < 0 || tx >= cache_x || ty >= cache_y ||
        group_id.z >= (uint)dispatch_z_count || tz >= z_count ) {
        return;
    }

    // Gameplay distance from player.  This must match final visibility so
    // seen-cache decay and lit-level classification agree on range.
    int dist_i = visibility_distance( abs( tx - player_x ), abs( ty - player_y ),
                                      abs( tz - player_z_idx ) );
    float dist = (float)dist_i;

    int seen_idx = tz * cache_xy + tx * cache_y + ty;

    if( dist_i == 0 ) {
        // Player's own tile: always fully visible.
        seen_all[seen_idx] = 1.0;
        return;
    }

    if( dist_i > view_radius ) {
        seen_all[seen_idx] = 0.0;
        return;
    }

    // Ray from player toward target; accumulate running-average transparency.
    int sdx = tx - player_x;
    int sdy = ty - player_y;
    int sdz = tz - player_z_idx;

    int steps = max( abs( sdx ), max( abs( sdy ), (int)ceil( (float)abs( sdz ) * z_scale ) ) );
    steps = max( steps, 1 );

    // Initialise to open-air so adjacent tiles (steps == 1, loop body never
    // executes) decay correctly with exp(-OPEN_AIR * dist).
    float avg_transparency = LIGHT_TRANSPARENCY_OPEN_AIR;
    bool  blocked          = false;

    // Explicit z-crossing floor check.
    // A floor is the boundary between two z-level centres, so crossing one
    // z-level must be tested at t=0.5, not at the target z centre.  Testing at
    // the target centre lets oblique rays step past the actual floor tile.
    if( sdz != 0 ) {
        const int sign_z = ( sdz > 0 ) ? 1 : -1;
        const int crossings = abs( sdz );
        for( int k = 0; k < crossings; ++k ) {
            const float t_z  = ( (float)k + 0.5 ) / (float)crossings;
            const int   ix_z = clamp( player_x + round_nearest_int( (float)sdx * t_z ),
                                      0, cache_x - 1 );
            const int   iy_z = clamp( player_y + round_nearest_int( (float)sdy * t_z ),
                                      0, cache_y - 1 );
            const int floor_z = sign_z > 0 ? player_z_idx + k + 1 : player_z_idx - k;
            if( floor_z >= 0 && floor_z < z_count ) {
                const int fl_idx = floor_z * cache_xy + ix_z * cache_y + iy_z;
                if( floor_all[fl_idx] != 0u ) {
                    blocked = true;
                    break;
                }
            }
        }
    }

    for( int i = 1; i < steps; ++i ) {
        int prev_ix = player_x + round_nearest_int( (float)sdx * ( (float)( i - 1 ) / (float)steps ) );
        int prev_iy = player_y + round_nearest_int( (float)sdy * ( (float)( i - 1 ) / (float)steps ) );
        int prev_iz = player_z_idx + round_nearest_int( (float)sdz * ( (float)( i - 1 ) / (float)steps ) );
        float t  = (float)i / (float)steps;
        int   ix = player_x + round_nearest_int( (float)sdx * t );
        int   iy = player_y + round_nearest_int( (float)sdy * t );
        int   iz = player_z_idx + round_nearest_int( (float)sdz * t );

        prev_ix = clamp( prev_ix, 0, cache_x - 1 );
        prev_iy = clamp( prev_iy, 0, cache_y - 1 );
        prev_iz = clamp( prev_iz, 0, z_count - 1 );
        ix = clamp( ix, 0, cache_x - 1 );
        iy = clamp( iy, 0, cache_y - 1 );
        iz = clamp( iz, 0, z_count - 1 );

        if( blocked_by_player_vision_adjustment( ix, iy, iz ) ) {
            blocked = true;
            break;
        }
        if( blocked_by_vehicle_diagonal( prev_ix, prev_iy, prev_iz, ix, iy, iz ) ) {
            blocked = true;
            break;
        }

        float t_val = transparency_all[tile_index( ix, iy, iz )];
        if( t_val <= LIGHT_TRANSPARENCY_SOLID ) {
            blocked = true;
            break;
        }

        // Running-average transparency (mirrors accumulate_transparency on CPU).
        avg_transparency = ( (float)( i - 1 ) * avg_transparency + t_val ) / (float)i;
    }

    if( blocked ) {
        seen_all[seen_idx] = 0.0;
        return;
    }

    if( tz < player_z_idx && tz + 1 < z_count ) {
        int roof_idx = tile_index( tx, ty, tz + 1 );
        if( vehicle_floor_all[roof_idx] != 0u ) {
            seen_all[seen_idx] = 0.0;
            return;
        }
    }

    // Visibility decays exponentially with distance through the average
    // transparency, matching the CPU k_sight_model.  Values remain in [0, 1]
    // so apparent_light_helper's pow(vis, scale_factor) * lm is correct.
    seen_all[seen_idx] = exp( -avg_transparency * dist );
}
