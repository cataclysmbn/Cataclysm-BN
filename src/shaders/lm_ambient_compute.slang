// GPU Lighting - Ambient Initialisation Pass

[[vk::binding(0, 2)]]
struct AmbientConstants {
    float  inside_light;
    int    cache_x;
    int    cache_y;
    int    cache_xy;
    int    z_count;
    int    overmap_depth;
    uint   angled_sunlight_shadows;
    uint   direct_sunlight;
    float  sun_dx_per_z;
    float  sun_dy_per_z;
    float  solar_shadow_light;
    uint   _pad1;
    float4 natural_light[6];
};

[[vk::binding(0, 2)]]
ConstantBuffer<AmbientConstants> constants : register(b0, space2);

#define inside_light constants.inside_light
#define cache_x constants.cache_x
#define cache_y constants.cache_y
#define cache_xy constants.cache_xy
#define z_count constants.z_count
#define overmap_depth constants.overmap_depth
#define angled_sunlight_shadows constants.angled_sunlight_shadows
#define direct_sunlight constants.direct_sunlight
#define sun_dx_per_z constants.sun_dx_per_z
#define sun_dy_per_z constants.sun_dy_per_z
#define solar_shadow_light constants.solar_shadow_light
#define natural_light constants.natural_light

static const float LIGHT_TRANSPARENCY_SOLID = 0.0;
static const int SUN_NONE   = 0;
static const int SUN_SHADOW = 1;
static const int SUN_DIRECT = 2;
static const uint AMBIENT_THREADS_X = 64u;

[[vk::binding(0, 0)]]
StructuredBuffer<uint> floor_all : register(t0, space0);

[[vk::binding(1, 0)]]
StructuredBuffer<float> transparency_all : register(t1, space0);

[[vk::binding(2, 0)]]
StructuredBuffer<float> source_map_all : register(t2, space0);

[[vk::binding(3, 0)]]
StructuredBuffer<uint> vehicle_floor_all : register(t3, space0);

[[vk::binding(0, 1)]]
RWStructuredBuffer<uint> lm_all : register(u0, space1);

[[vk::binding(1, 1)]]
RWStructuredBuffer<uint> daylight_seed_all : register(u1, space1);

int round_nearest_int( float value )
{
    return value >= 0.0 ? int( floor( value + 0.5 ) ) : int( ceil( value - 0.5 ) );
}

int tile_index( int x, int y, int z )
{
    return z * cache_xy + x * cache_y + y;
}

bool inbounds_xy( int x, int y )
{
    return x >= 0 && y >= 0 && x < cache_x && y < cache_y;
}

int bounded_int( int value, int low, int high )
{
    if( value < low ) {
        return low;
    }
    if( value > high ) {
        return high;
    }
    return value;
}

float larger_float( float lhs, float rhs )
{
    return lhs > rhs ? lhs : rhs;
}

int larger_int( int lhs, int rhs )
{
    return lhs > rhs ? lhs : rhs;
}

int sunlight_state( int x, int y, int z_idx, bool use_angled_sun )
{
    for( int step = 1; z_idx + step < z_count; ++step ) {
        uint above_idx = uint( tile_index( x, y, z_idx + step ) );
        if( floor_all[above_idx] != 0u || vehicle_floor_all[above_idx] != 0u ) {
            return SUN_NONE;
        }
    }

    if( !use_angled_sun ) {
        return SUN_DIRECT;
    }

    for( int step = 1; z_idx + step < z_count; ++step ) {
        float ray_step = -( float( step ) - 0.5 );
        int sx = x + round_nearest_int( sun_dx_per_z * ray_step );
        int sy = y + round_nearest_int( sun_dy_per_z * ray_step );

        if( !inbounds_xy( sx, sy ) ) {
            return SUN_DIRECT;
        }

        uint above_idx = uint( tile_index( sx, sy, z_idx + step ) );
        if( floor_all[above_idx] != 0u ) {
            return SUN_SHADOW;
        }
    }

    int levels_up = z_count - 1 - z_idx;
    float dx_to_sky = -sun_dx_per_z * float( levels_up );
    float dy_to_sky = -sun_dy_per_z * float( levels_up );
    float total = larger_float( larger_float( abs( dx_to_sky ), abs( dy_to_sky ) ),
                                float( levels_up ) );
    int steps = larger_int( int( ceil( total ) ), 1 );

    for( int i = 1; i <= steps; ++i ) {
        float t = float( i ) / float( steps );
        int sx = x + round_nearest_int( dx_to_sky * t );
        int sy = y + round_nearest_int( dy_to_sky * t );
        int sz = z_idx + round_nearest_int( float( levels_up ) * t );

        if( !inbounds_xy( sx, sy ) ) {
            return SUN_DIRECT;
        }
        sz = bounded_int( sz, z_idx, z_count - 1 );
        if( sx == x && sy == y && sz == z_idx ) {
            continue;
        }

        if( transparency_all[tile_index( sx, sy, sz )] <= LIGHT_TRANSPARENCY_SOLID ) {
            return SUN_SHADOW;
        }
    }

    return SUN_DIRECT;
}

void ambient_main_impl( uint3 group_id, uint3 group_thread_id )
{
    uint idx = group_id.x * AMBIENT_THREADS_X + group_thread_id.x;
    if( idx >= uint( z_count * cache_xy ) ) {
        return;
    }

    uint z_idx = idx / uint( cache_xy );
    uint tile  = idx % uint( cache_xy );

    int x = int( tile / uint( cache_y ) );
    int y = int( tile % uint( cache_y ) );

    bool use_angled_sun = angled_sunlight_shadows != 0u && direct_sunlight != 0u;
    int sun_state = sunlight_state( x, y, int( z_idx ), use_angled_sun );

    float daylight = 0.0;
    float ambient = inside_light;
    if( sun_state == SUN_DIRECT ) {
        daylight = natural_light[z_idx / 4][z_idx % 4];
        ambient = daylight;
    } else if( sun_state == SUN_SHADOW ) {
        daylight = solar_shadow_light;
        ambient = daylight;
    }
    float source_light = source_map_all[idx];
    if( source_light > ambient ) {
        ambient = source_light;
    }
    lm_all[idx] = asuint( ambient );
    daylight_seed_all[idx] = asuint( daylight );
}

[numthreads(64, 1, 1)]
void main( uint3 group_id : SV_GroupID, uint3 group_thread_id : SV_GroupThreadID )
{
    ambient_main_impl( group_id, group_thread_id );
}

[numthreads(64, 1, 1)]
void cpu_main( uint3 group_id : SV_GroupID, uint3 group_thread_id : SV_GroupThreadID )
{
    ambient_main_impl( group_id, group_thread_id );
}
