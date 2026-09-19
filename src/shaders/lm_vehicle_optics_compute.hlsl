// GPU Lighting - Vehicle mirror/camera visibility pass
//
// Produces resident camera_all visibility from compact vehicle optic metadata.
// Mirrors are gated by the already-computed player seen cache at the mirror
// tile; cameras are gated on the CPU-packed active camera-control state.

static const float LIGHT_TRANSPARENCY_SOLID    = 0.0;
static const float LIGHT_TRANSPARENCY_OPEN_AIR = 0.038376418216;
static const float VISIBILITY_FULL             = 1.0;

static const uint OPTIC_MIRROR = 0u;
static const uint OPTIC_CAMERA = 1u;

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

StructuredBuffer<float> transparency_all : register(t0, space0);
StructuredBuffer<float> seen_all         : register(t1, space0);
StructuredBuffer<GpuVehicleOptic> vehicle_optics : register(t2, space0);

RWStructuredBuffer<uint> camera_all : register(u0, space1);

int tile_index( int x, int y, int z )
{
    return z * cache_xy + x * cache_y + y;
}

int round_nearest_int( float value )
{
    return value >= 0.0 ? (int)floor( value + 0.5 ) : (int)ceil( value - 0.5 );
}

int visibility_distance( int dx, int dy )
{
    if( trigdist == 0u ) {
        return max( dx, dy );
    }
    return (int)round( sqrt( (float)( dx * dx + dy * dy ) ) );
}

float trace_optic_visibility( GpuVehicleOptic optic, int tx, int ty, int dist )
{
    if( dist == 0 && optic.kind == OPTIC_CAMERA ) {
        return VISIBILITY_FULL;
    }

    const int sdx = tx - optic.x;
    const int sdy = ty - optic.y;
    const int steps = max( abs( sdx ), abs( sdy ) );

    float avg_transparency = LIGHT_TRANSPARENCY_OPEN_AIR;
    int transparent_steps = 0;

    for( int i = 1; i <= steps; ++i ) {
        const float t = (float)i / (float)steps;
        const int ix = clamp( optic.x + round_nearest_int( (float)sdx * t ), 0, cache_x - 1 );
        const int iy = clamp( optic.y + round_nearest_int( (float)sdy * t ), 0, cache_y - 1 );
        const bool target_step = ( i == steps );
        const float tile_transparency = transparency_all[tile_index( ix, iy, optic.z_idx )];

        if( tile_transparency <= LIGHT_TRANSPARENCY_SOLID ) {
            if( !target_step ) {
                return 0.0;
            }
            break;
        }

        ++transparent_steps;
        avg_transparency =
            ( (float)( transparent_steps - 1 ) * avg_transparency + tile_transparency ) /
            (float)transparent_steps;
    }

    const int total_distance = dist + max( optic.offset_distance, 0 );
    return exp( -avg_transparency * (float)total_distance );
}

[numthreads(8, 8, 1)]
void main( uint3 group_id : SV_GroupID, uint3 thread_id : SV_GroupThreadID )
{
    if( group_id.x >= num_optics ) {
        return;
    }

    GpuVehicleOptic optic = vehicle_optics[group_id.x];
    if( optic.z_idx < 0 || optic.z_idx >= z_count || optic.range <= 0 ) {
        return;
    }

    if( optic.kind == OPTIC_MIRROR ) {
        const int optic_idx = tile_index( optic.x, optic.y, optic.z_idx );
        if( seen_all[optic_idx] < visible_threshold ) {
            return;
        }
    }

    const int dx = (int)( group_id.y * 8 + thread_id.x ) - max_radius;
    const int dy = (int)( group_id.z * 8 + thread_id.y ) - max_radius;
    const int tx = optic.x + dx;
    const int ty = optic.y + dy;

    if( tx < 0 || ty < 0 || tx >= cache_x || ty >= cache_y ) {
        return;
    }

    const int dist = visibility_distance( abs( dx ), abs( dy ) );
    if( dist > optic.range ) {
        return;
    }
    if( optic.kind == OPTIC_MIRROR && dist + max( optic.offset_distance, 0 ) > max_view_distance ) {
        return;
    }

    const float visibility = trace_optic_visibility( optic, tx, ty, dist );
    if( visibility <= LIGHT_TRANSPARENCY_SOLID ) {
        return;
    }

    const int out_idx = tile_index( tx, ty, optic.z_idx );
    InterlockedMax( camera_all[out_idx], asuint( visibility ) );
}
