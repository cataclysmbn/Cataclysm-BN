// GPU Lighting — Ambient Initialisation Pass
//
// Initialises lm_all before any per-source ray casting.
// One thread per tile per z-level.
//
// Outdoor tiles receive natural_light[z_idx]; indoor tiles receive
// inside_light.  Sky access is derived solely from physical floor/vehicle-roof
// caches by tracing from the tile upward to OVERMAP_HEIGHT.  When angled
// sunlight shadows are enabled during daytime, the trace follows the current
// sun vector; otherwise it traces straight up.
//
// Vehicle roofs/opaque parts are represented in vehicle_floor_all at z+1 even
// when the visible vehicle surface is on the current z tile.  If that vehicle
// ceiling is the only ceiling and the current tile is itself opaque, light the
// visible surface from the sky.  The same tile will still block propagation in
// the ray passes via transparency_all.
//
// lm_all stores uint — the bit-reinterpretation of positive floats.
// This allows the raytrace pass to use InterlockedMax on the same buffer.
//
// Binding layout (SDL3 GPU / shadercross HLSL conventions):
//   space0  read-only storage buffers  (t registers)
//   space1  read-write storage buffers (u registers)
//   space2  uniform / cbuffer          (b registers)
//
// Thread group: [64, 1, 1]
// Dispatch: (ceil(z_count * cache_xy / 64), 1, 1)

static const float LIGHT_TRANSPARENCY_SOLID = 0.0;

cbuffer Constants : register(b0, space2)
{
    float  inside_light;
    int    cache_x;
    int    cache_y;
    int    cache_xy;        // = cache_x * cache_y (tiles per z-level)
    int    z_count;
    int    overmap_depth;   // z_idx of the surface level (= OVERMAP_DEPTH on CPU)
    uint   angled_sunlight_shadows;
    uint   direct_sunlight;
    float  sun_dx_per_z;
    float  sun_dy_per_z;
    uint   _pad0;
    uint   _pad1;
    // Each float4 element is 16 bytes in both std140 (Vulkan) and HLSL scalar
    // layout, so C++ float[6][4] and HLSL float4[6] are layout-compatible.
    // Access element zi as natural_light[zi / 4][zi % 4].
    // Indices 0..20 = OVERMAP_LAYERS z-levels; indices 21-23 are unused.
    float4 natural_light[6];
};

StructuredBuffer<uint> floor_all         : register(t0, space0);
StructuredBuffer<uint> vehicle_floor_all : register(t1, space0);
StructuredBuffer<float> transparency_all : register(t2, space0);

RWStructuredBuffer<uint> lm_all : register(u0, space1);

int round_nearest_int( float value )
{
    return value >= 0.0 ? (int)floor( value + 0.5 ) : (int)ceil( value - 0.5 );
}

[numthreads(64, 1, 1)]
void main( uint3 dispatch_id : SV_DispatchThreadID )
{
    uint idx = dispatch_id.x;
    if( idx >= (uint)( z_count * cache_xy ) ) {
        return;
    }

    uint z_idx = idx / (uint)cache_xy;
    uint tile  = idx % (uint)cache_xy;

    int x = (int)( tile / (uint)cache_y );
    int y = (int)( tile % (uint)cache_y );

    bool has_sky = true;
    bool blocked_by_visible_vehicle_surface = false;
    bool use_angled_sun = angled_sunlight_shadows != 0u && direct_sunlight != 0u;

    for( int step = 1; (int)z_idx + step < z_count; ++step ) {
        float ray_step = use_angled_sun ? ( (float)step - 0.5 ) : 0.0;
        int sx = x + round_nearest_int( sun_dx_per_z * ray_step );
        int sy = y + round_nearest_int( sun_dy_per_z * ray_step );

        if( sx < 0 || sy < 0 || sx >= cache_x || sy >= cache_y ) {
            break;
        }

        uint above_idx = ( z_idx + (uint)step ) * (uint)cache_xy +
                         (uint)( sx * cache_y + sy );
        bool terrain_ceiling = floor_all[above_idx] != 0u;
        bool vehicle_ceiling = vehicle_floor_all[above_idx] != 0u;
        if( terrain_ceiling || vehicle_ceiling ) {
            if( !terrain_ceiling && vehicle_ceiling && step == 1 &&
                sx == x && sy == y &&
                transparency_all[idx] <= LIGHT_TRANSPARENCY_SOLID ) {
                blocked_by_visible_vehicle_surface = true;
            } else {
                has_sky = false;
            }
            break;
        }
    }

    float ambient = ( has_sky || blocked_by_visible_vehicle_surface )
                    ? natural_light[z_idx / 4][z_idx % 4]
                    : inside_light;
    lm_all[idx] = asuint( ambient );
}
