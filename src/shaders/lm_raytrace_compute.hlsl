// GPU Lighting — Per-Source Ray-Cast Pass
//
// Accumulates per-light-source contributions into lm_all using InterlockedMax.
// One workgroup per light source; threads cover the 2D bounding box of the
// source's effective radius, looping over z-levels internally.
//
// Algorithm per thread:
//   1. Compute 2D offset (dx, dy) from source via group/thread IDs.
//   2. Loop over every z-level in the overmap volume.
//   3. Compute 3D Euclidean distance; skip if > radius.
//   4. Trace a DDA ray from target back toward source, accumulating the
//      running-average transparency (mirrors accumulate_transparency on CPU).
//      Vertical steps check floor_all/vehicle_floor_all to block the ray.
//   5. Compute intensity = luminance / (exp(avg_transparency * distance) * distance^2).
//      The /distance falloff formula is commented below for easy comparison.
//   6. Atomically update lm_all[target] = max(lm_all[target], intensity).
//      Positive-float bit-reinterpretation as uint makes InterlockedMax correct.
//
// Binding layout (SDL3 GPU / shadercross HLSL conventions):
//   space0  read-only storage buffers  (t registers)
//   space1  read-write storage buffers (u registers)
//   space2  uniform / cbuffer          (b registers)
//
// Thread group: [8, 8, 1]
// Dispatch: (num_sources, ceil((2*max_radius+1)/8), ceil((2*max_radius+1)/8))
//   group_id.x = source index
//   group_id.y = x-chunk index (determines dx offset)
//   group_id.z = y-chunk index (determines dy offset)

static const float LIGHT_TRANSPARENCY_SOLID    = 0.0;
static const float LIGHT_TRANSPARENCY_OPEN_AIR = 0.038376418216;
static const float LIGHT_AMBIENT_LOW           = 3.5;

struct GpuLightSource
{
    int   x;
    int   y;
    int   z_idx;
    float luminance;
    float radius;
    uint  _pad0;
    uint  _pad1;
    uint  _pad2;
};

cbuffer Constants : register(b0, space2)
{
    int   cache_x;
    int   cache_y;
    int   cache_xy;     // = cache_x * cache_y
    int   z_count;
    float z_scale;      // Z_LEVEL_SCALE = 1.8
    uint  num_sources;
    int   max_radius;   // bounding-box half-extent for dispatch
    uint  _pad0;
};

StructuredBuffer<float>         transparency_all : register(t0, space0);
StructuredBuffer<uint>          floor_all        : register(t1, space0);
StructuredBuffer<uint>          vehicle_floor_all: register(t2, space0);
StructuredBuffer<GpuLightSource> light_sources   : register(t3, space0);
RWStructuredBuffer<uint>        lm_all           : register(u0, space1);

int round_nearest_int( float value )
{
    return value >= 0.0 ? (int)floor( value + 0.5 ) : (int)ceil( value - 0.5 );
}

[numthreads(8, 8, 1)]
void main( uint3 group_id : SV_GroupID, uint3 thread_id : SV_GroupThreadID )
{
    if( group_id.x >= num_sources ) {
        return;
    }

    GpuLightSource src = light_sources[group_id.x];

    // Compute 2D offset from source centre; may be negative.
    int dx = (int)( group_id.y * 8 + thread_id.x ) - max_radius;
    int dy = (int)( group_id.z * 8 + thread_id.y ) - max_radius;

    int tx = src.x + dx;
    int ty = src.y + dy;

    // Bounds check target 2D position.
    if( tx < 0 || ty < 0 || tx >= cache_x || ty >= cache_y ) {
        return;
    }

    for( int tz = 0; tz < z_count; ++tz ) {
        int dz = tz - src.z_idx;
        float fdx  = (float)dx;
        float fdy  = (float)dy;
        float fdz  = (float)dz * z_scale;
        float dist = sqrt( fdx * fdx + fdy * fdy + fdz * fdz );

        if( dist > src.radius ) {
            continue;
        }

        float intensity;

        if( dist < 0.5 ) {
            // Source tile itself: use luminance directly.
            intensity = src.luminance;
        } else {
            // Trace a DDA ray from target (tx,ty,tz) toward source (sx,sy,sz).
            // sdx/sdy/sdz are the step direction (source - target).
            int sdx = src.x - tx;
            int sdy = src.y - ty;
            int sdz = src.z_idx - tz;

            int steps = max( abs( sdx ), max( abs( sdy ), (int)ceil( (float)abs( sdz ) * z_scale ) ) );
            steps = max( steps, 1 );

            float avg_transparency = LIGHT_TRANSPARENCY_OPEN_AIR;
            bool  blocked          = false;

            // Explicit z-crossing floor check — same half-step boundary rule as
            // lm_seen_compute.  Floors live between z-level centres, not at the
            // source or target centre.
            if( sdz != 0 ) {
                const int sign_z = ( sdz > 0 ) ? 1 : -1;
                const int crossings = abs( sdz );
                for( int k = 0; k < crossings; ++k ) {
                    const float t_z  = ( (float)k + 0.5 ) / (float)crossings;
                    const int   ix_z = clamp( tx + round_nearest_int( (float)sdx * t_z ),
                                              0, cache_x - 1 );
                    const int   iy_z = clamp( ty + round_nearest_int( (float)sdy * t_z ),
                                              0, cache_y - 1 );
                    const int floor_z = sign_z > 0 ? tz + k + 1 : tz - k;
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
                // Interpolate from target toward source.
                float t  = (float)i / (float)steps;
                int   ix = tx + round_nearest_int( (float)sdx * t );
                int   iy = ty + round_nearest_int( (float)sdy * t );
                int   iz = tz + round_nearest_int( (float)sdz * t );

                // Clamp to valid range.
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
                continue;
            }

            // Intensity formula: luminance / (exp(avg * dist) * dist^2)
            // k=2 (inverse square): physically correct, sharper falloff.
            intensity = src.luminance / ( exp( avg_transparency * dist ) * dist * dist );
            // k=1 (inverse distance): softer falloff — swap in if needed.
            // intensity = src.luminance / ( exp( avg_transparency * dist ) * dist );
        }

        // Clamp to avoid spurious overflow when distance is very small.
        intensity = min( intensity, src.luminance );

        if( intensity <= LIGHT_AMBIENT_LOW ) {
            continue;
        }

        // Atomically update lm_all with the maximum contribution.
        // Positive IEEE 754 floats compare correctly as uint32.
        int lm_idx = tz * cache_xy + tx * cache_y + ty;
        InterlockedMax( lm_all[lm_idx], asuint( intensity ) );
    }
}
