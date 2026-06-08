// GPU Lighting — Ambient Initialisation Pass
//
// Initialises the frame lm before any per-source ray casting.
// One thread per tile per z-level.
//
// Direct-sun tiles receive natural_light[z_idx]; roofed indoor tiles receive
// inside_light; open-sky tiles in angled sunlight shadow receive
// solar_shadow_light. Sky access is derived from physical terrain floor caches
// by tracing from the tile upward to OVERMAP_HEIGHT. Vehicle roofs block direct
// sunlight into the vehicle interior as vertical cover, but do not cast angled
// sunlight shadows. When angled sunlight shadows are enabled during daytime,
// wall transparency is checked along the ray back toward the sun.
//
// lm_all and daylight_seed_all store uint — the bit-reinterpretation of positive floats.
// This allows the raytrace pass to use InterlockedMax on the same buffer.
//
// Binding layout (SDL3 GPU / shadercross HLSL conventions):
//   space0  read-only storage buffers  (t registers)
//   space1  read-write storage buffers (u registers)
//   space2  uniform / cbuffer          (b registers)
//
// Thread group: [64, 1, 1]
// Dispatch: (ceil(z_count * cache_xy / 64), 1, 1)

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
    float  solar_shadow_light;
    uint   _pad1;
    // Each float4 element is 16 bytes in both std140 (Vulkan) and HLSL scalar
    // layout, so C++ float[6][4] and HLSL float4[6] are layout-compatible.
    // Access element zi as natural_light[zi / 4][zi % 4].
    // Indices 0..20 = OVERMAP_LAYERS z-levels; indices 21-23 are unused.
    float4 natural_light[6];
};

static const float LIGHT_TRANSPARENCY_SOLID = 0.0;
static const int SUN_NONE   = 0;
static const int SUN_SHADOW = 1;
static const int SUN_DIRECT = 2;

StructuredBuffer<uint>  floor_all        : register(t0, space0);
StructuredBuffer<float> transparency_all : register(t1, space0);
StructuredBuffer<float> source_map_all   : register(t2, space0);
StructuredBuffer<uint>  vehicle_floor_all: register(t3, space0);

RWStructuredBuffer<uint> lm_all             : register(u0, space1);
RWStructuredBuffer<uint> daylight_seed_all  : register(u1, space1);

int round_nearest_int( float value )
{
    return value >= 0.0 ? (int)floor( value + 0.5 ) : (int)ceil( value - 0.5 );
}

int tile_index( int x, int y, int z )
{
    return z * cache_xy + x * cache_y + y;
}

bool inbounds_xy( int x, int y )
{
    return x >= 0 && y >= 0 && x < cache_x && y < cache_y;
}

int sunlight_state( int x, int y, int z_idx, bool use_angled_sun )
{
    for( int step = 1; z_idx + step < z_count; ++step ) {
        uint above_idx = (uint)tile_index( x, y, z_idx + step );
        if( floor_all[above_idx] != 0u || vehicle_floor_all[above_idx] != 0u ) {
            return SUN_NONE;
        }
    }

    if( !use_angled_sun ) {
        return SUN_DIRECT;
    }

    for( int step = 1; z_idx + step < z_count; ++step ) {
        float ray_step = -( (float)step - 0.5 );
        int sx = x + round_nearest_int( sun_dx_per_z * ray_step );
        int sy = y + round_nearest_int( sun_dy_per_z * ray_step );

        if( !inbounds_xy( sx, sy ) ) {
            return SUN_DIRECT;
        }

        uint above_idx = (uint)tile_index( sx, sy, z_idx + step );
        if( floor_all[above_idx] != 0u ) {
            return SUN_SHADOW;
        }
    }

    int levels_up = z_count - 1 - z_idx;
    float dx_to_sky = -sun_dx_per_z * (float)levels_up;
    float dy_to_sky = -sun_dy_per_z * (float)levels_up;
    float total = max( max( abs( dx_to_sky ), abs( dy_to_sky ) ), (float)levels_up );
    int steps = max( (int)ceil( total ), 1 );

    for( int i = 1; i <= steps; ++i ) {
        float t = (float)i / (float)steps;
        int sx = x + round_nearest_int( dx_to_sky * t );
        int sy = y + round_nearest_int( dy_to_sky * t );
        int sz = z_idx + round_nearest_int( (float)levels_up * t );

        if( !inbounds_xy( sx, sy ) ) {
            return SUN_DIRECT;
        }
        sz = clamp( sz, z_idx, z_count - 1 );
        if( sx == x && sy == y && sz == z_idx ) {
            continue;
        }

        if( transparency_all[tile_index( sx, sy, sz )] <= LIGHT_TRANSPARENCY_SOLID ) {
            return SUN_SHADOW;
        }
    }

    return SUN_DIRECT;
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

    bool use_angled_sun = angled_sunlight_shadows != 0u && direct_sunlight != 0u;
    int sun_state = sunlight_state( x, y, (int)z_idx, use_angled_sun );

    float daylight = 0.0;
    float ambient = inside_light;
    if( sun_state == SUN_DIRECT ) {
        daylight = natural_light[z_idx / 4][z_idx % 4];
        ambient = daylight;
    } else if( sun_state == SUN_SHADOW ) {
        daylight = solar_shadow_light;
        ambient = daylight;
    }
    ambient = max( ambient, source_map_all[idx] );
    lm_all[idx] = asuint( ambient );
    daylight_seed_all[idx] = asuint( daylight );
}
