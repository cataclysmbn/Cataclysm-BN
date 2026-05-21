// GPU Lighting — Seen-Cache Ray-Cast Pass
//
// Computes seen_cache for the player's point of view.
// One thread per target tile (x, y); z-level determined by group_id.z.
//
// Algorithm per thread:
//   1. Map group/thread IDs to a target tile (tx, ty, tz).
//   2. Compute 3D distance from player; skip if beyond view_radius.
//   3. Trace a DDA ray from player toward target, accumulating a running-
//      average transparency (mirrors accumulate_transparency on CPU).
//      Vertical steps check floor_all/vehicle_floor_all to block the ray.
//   4. seen_all[target] = exp(-avg_transparency * distance).
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
    uint  _pad0;
    uint  _pad1;
    uint  _pad2;
};

StructuredBuffer<float> transparency_all  : register(t0, space0);
StructuredBuffer<uint>  floor_all         : register(t1, space0);
StructuredBuffer<uint>  vehicle_floor_all : register(t2, space0);

RWStructuredBuffer<float> seen_all : register(u0, space1);

int round_nearest_int( float value )
{
    return value >= 0.0 ? (int)floor( value + 0.5 ) : (int)ceil( value - 0.5 );
}

[numthreads(8, 8, 1)]
void main( uint3 group_id : SV_GroupID, uint3 thread_id : SV_GroupThreadID )
{
    // Map to target tile.
    int tx = player_x + (int)( group_id.x * 8 + thread_id.x ) - view_radius;
    int ty = player_y + (int)( group_id.y * 8 + thread_id.y ) - view_radius;
    int tz = (int)group_id.z;

    if( tx < 0 || ty < 0 || tx >= cache_x || ty >= cache_y || tz >= z_count ) {
        return;
    }

    // 3D distance from player.
    float fdx  = (float)( tx - player_x );
    float fdy  = (float)( ty - player_y );
    float fdz  = (float)( tz - player_z_idx ) * z_scale;
    float dist = sqrt( fdx * fdx + fdy * fdy + fdz * fdz );

    int seen_idx = tz * cache_xy + tx * cache_y + ty;

    if( dist < 0.5 ) {
        // Player's own tile: always fully visible.
        seen_all[seen_idx] = 1.0;
        return;
    }

    if( dist > (float)view_radius ) {
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
            if( sign_z < 0 && k == 0 && ix_z == player_x && iy_z == player_y ) {
                continue;
            }
            const int floor_z = sign_z > 0 ? player_z_idx + k + 1 : player_z_idx - k;
            if( floor_z >= 0 && floor_z < z_count ) {
                const int fl_idx = floor_z * cache_xy + ix_z * cache_y + iy_z;
                if( floor_all[fl_idx] != 0u || vehicle_floor_all[fl_idx] != 0u ) {
                    blocked = true;
                    break;
                }
            }
        }
    }

    for( int i = 1; i < steps; ++i ) {
        float t  = (float)i / (float)steps;
        int   ix = player_x + round_nearest_int( (float)sdx * t );
        int   iy = player_y + round_nearest_int( (float)sdy * t );
        int   iz = player_z_idx + round_nearest_int( (float)sdz * t );

        ix = clamp( ix, 0, cache_x - 1 );
        iy = clamp( iy, 0, cache_y - 1 );
        iz = clamp( iz, 0, z_count - 1 );

        float t_val = transparency_all[iz * cache_xy + ix * cache_y + iy];
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

    // Visibility decays exponentially with distance through the average
    // transparency, matching the CPU k_sight_model.  Values remain in [0, 1]
    // so apparent_light_helper's pow(vis, scale_factor) * lm is correct.
    seen_all[seen_idx] = exp( -avg_transparency * dist );
}
