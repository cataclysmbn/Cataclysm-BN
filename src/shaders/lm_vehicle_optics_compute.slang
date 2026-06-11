// GPU Lighting - Vehicle mirror/camera visibility pass

static const float LIGHT_TRANSPARENCY_SOLID    = 0.0;
static const float LIGHT_TRANSPARENCY_OPEN_AIR = 0.038376418216;
static const float VISIBILITY_FULL             = 1.0;

static const uint OPTIC_MIRROR = 0u;
static const uint OPTIC_CAMERA = 1u;
static const uint VEHICLE_OPTICS_THREADS_X = 8u;
static const uint VEHICLE_OPTICS_THREADS_Y = 8u;

struct GpuVehicleOptic
{
    int  x;
    int  y;
    int  z_idx;
    uint kind;
    int  range;
    int  offset_distance;
    uint _pad0;
    uint _pad1;
};

[[vk::binding(0, 2)]]
cbuffer Constants : register(b0, space2)
{
    int   cache_x;
    int   cache_y;
    int   cache_xy;
    int   z_count;
    uint  num_optics;
    int   max_radius;
    uint  trigdist;
    float visible_threshold;
    int   max_view_distance;
    uint  _pad0;
    uint  _pad1;
    uint  _pad2;
};

[[vk::binding(0, 0)]]
StructuredBuffer<float> transparency_all : register(t0, space0);

[[vk::binding(1, 0)]]
StructuredBuffer<float> seen_all : register(t1, space0);

[[vk::binding(2, 0)]]
StructuredBuffer<GpuVehicleOptic> vehicle_optics : register(t2, space0);

[[vk::binding(0, 1)]]
RWStructuredBuffer<uint> camera_all : register(u0, space1);

int tile_index( int x, int y, int z )
{
    return z * cache_xy + x * cache_y + y;
}

int round_nearest_int( float value )
{
    return value >= 0.0 ? int( floor( value + 0.5 ) ) : int( ceil( value - 0.5 ) );
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

int larger_int( int lhs, int rhs )
{
    return lhs > rhs ? lhs : rhs;
}

int nonnegative_int( int value )
{
    return value > 0 ? value : 0;
}

int visibility_distance( int dx, int dy )
{
    if( trigdist == 0u ) {
        return larger_int( dx, dy );
    }
    return int( round( sqrt( float( dx * dx + dy * dy ) ) ) );
}

void max_camera_bits( int idx, uint candidate )
{
#if defined(CATA_SLANG_CPU)
    uint previous = camera_all[idx];
    if( candidate > previous ) {
        camera_all[idx] = candidate;
    }
#else
    InterlockedMax( camera_all[idx], candidate );
#endif
}

float trace_optic_visibility( GpuVehicleOptic optic, int tx, int ty, int dist )
{
    if( dist == 0 && optic.kind == OPTIC_CAMERA ) {
        return VISIBILITY_FULL;
    }

    int sdx = tx - optic.x;
    int sdy = ty - optic.y;
    int steps = larger_int( abs( sdx ), abs( sdy ) );

    float avg_transparency = LIGHT_TRANSPARENCY_OPEN_AIR;
    int transparent_steps = 0;

    for( int i = 1; i <= steps; ++i ) {
        float t = float( i ) / float( steps );
        int ix = bounded_int( optic.x + round_nearest_int( float( sdx ) * t ), 0, cache_x - 1 );
        int iy = bounded_int( optic.y + round_nearest_int( float( sdy ) * t ), 0, cache_y - 1 );
        bool target_step = i == steps;
        float tile_transparency = transparency_all[tile_index( ix, iy, optic.z_idx )];

        if( tile_transparency <= LIGHT_TRANSPARENCY_SOLID ) {
            if( !target_step ) {
                return 0.0;
            }
            break;
        }

        ++transparent_steps;
        avg_transparency =
            ( float( transparent_steps - 1 ) * avg_transparency + tile_transparency ) /
            float( transparent_steps );
    }

    int total_distance = dist + nonnegative_int( optic.offset_distance );
    return exp( -avg_transparency * float( total_distance ) );
}

void vehicle_optics_main_impl( uint3 group_id, uint3 group_thread_id )
{
    if( group_id.x >= num_optics ) {
        return;
    }

    GpuVehicleOptic optic = vehicle_optics[group_id.x];
    if( optic.z_idx < 0 || optic.z_idx >= z_count || optic.range <= 0 ) {
        return;
    }

    if( optic.kind == OPTIC_MIRROR ) {
        int optic_idx = tile_index( optic.x, optic.y, optic.z_idx );
        if( seen_all[optic_idx] < visible_threshold ) {
            return;
        }
    }

    int dx = int( group_id.y * VEHICLE_OPTICS_THREADS_X + group_thread_id.x ) - max_radius;
    int dy = int( group_id.z * VEHICLE_OPTICS_THREADS_Y + group_thread_id.y ) - max_radius;
    int tx = optic.x + dx;
    int ty = optic.y + dy;

    if( tx < 0 || ty < 0 || tx >= cache_x || ty >= cache_y ) {
        return;
    }

    int dist = visibility_distance( abs( dx ), abs( dy ) );
    if( dist > optic.range ) {
        return;
    }
    if( optic.kind == OPTIC_MIRROR &&
        dist + nonnegative_int( optic.offset_distance ) > max_view_distance ) {
        return;
    }

    float visibility = trace_optic_visibility( optic, tx, ty, dist );
    if( visibility <= LIGHT_TRANSPARENCY_SOLID ) {
        return;
    }

    int out_idx = tile_index( tx, ty, optic.z_idx );
    max_camera_bits( out_idx, asuint( visibility ) );
}

[numthreads(8, 8, 1)]
void main( uint3 group_id : SV_GroupID, uint3 group_thread_id : SV_GroupThreadID )
{
    vehicle_optics_main_impl( group_id, group_thread_id );
}

[numthreads(8, 8, 1)]
void cpu_main( uint3 group_id : SV_GroupID, uint3 group_thread_id : SV_GroupThreadID )
{
    vehicle_optics_main_impl( group_id, group_thread_id );
}
