// GPU Lighting - Render-Only Colored Light Pass
//
// Emits the strongest colored light source per tile into a packed uint buffer:
// high byte = rank, low bytes = RGB888.  Gameplay light remains scalar.

static const float LIGHT_TRANSPARENCY_SOLID = 0.0;
static const float LIGHT_TRANSPARENCY_OPEN_AIR = 0.038376418216;
static const float LIGHT_AMBIENT_LOW = 3.5;
static const float DEFAULT_TARGET_Z_FRAC = 0.5;
static const float VEHICLE_ROOF_SURFACE_Z_FRAC = 0.70;
static const float VEHICLE_ROOF_EPSILON = 0.001;
static const uint  LIGHT_SOURCE_DIRECTIONAL = 1u;
static const uint  LIGHT_SOURCE_EXTERNAL_VEHICLE = 2u;

struct GpuColoredLightSource
{
    int   x;
    int   y;
    int   z_idx;
    uint  flags;
    float luminance;
    float radius;
    float dir_x;
    float dir_y;
    float cone_cos;
    float z_frac;
    uint  color_rgb;
    uint  _pad;
};

cbuffer Constants : register(b0, space2)
{
    int   cache_x;
    int   cache_y;
    int   cache_xy;
    int   z_count;
    float z_scale;
    uint  num_sources;
    int   max_radius;
    uint  source_offset;
};

StructuredBuffer<float>                 transparency_all : register(t0, space0);
StructuredBuffer<uint>                  floor_all        : register(t1, space0);
StructuredBuffer<uint>                  vehicle_floor_all: register(t2, space0);
StructuredBuffer<GpuColoredLightSource> light_sources    : register(t3, space0);
RWStructuredBuffer<uint>                color_all        : register(u0, space1);

int tile_index( int x, int y, int z )
{
    return z * cache_xy + x * cache_y + y;
}

int round_nearest_int( float value )
{
    return value >= 0.0 ? (int)floor( value + 0.5 ) : (int)ceil( value - 0.5 );
}

bool has_vehicle_surface( int x, int y, int z )
{
    int roof_z = z + 1;
    if( roof_z < 0 || roof_z >= z_count ) {
        return false;
    }
    return vehicle_floor_all[tile_index( x, y, roof_z )] != 0u;
}

bool target_vehicle_surface_blocks_source(
    GpuColoredLightSource src, int tx, int ty, int tz, float target_frac )
{
    if( !has_vehicle_surface( tx, ty, tz ) ) {
        return false;
    }

    float roof_plane = (float)tz + VEHICLE_ROOF_SURFACE_Z_FRAC;
    float source_h = (float)src.z_idx + src.z_frac;
    float target_h = (float)tz + target_frac;

    return source_h > roof_plane + VEHICLE_ROOF_EPSILON
           && target_h < roof_plane - VEHICLE_ROOF_EPSILON;
}

float open_target_z_frac( GpuColoredLightSource src )
{
    return src.z_frac > VEHICLE_ROOF_SURFACE_Z_FRAC + VEHICLE_ROOF_EPSILON ?
           VEHICLE_ROOF_SURFACE_Z_FRAC : DEFAULT_TARGET_Z_FRAC;
}

bool ray_sample_is_above_vehicle_surface( int x, int y, int z, float ray_h )
{
    if( !has_vehicle_surface( x, y, z ) ) {
        return false;
    }
    float roof_plane = (float)z + VEHICLE_ROOF_SURFACE_Z_FRAC;
    return ray_h >= roof_plane - VEHICLE_ROOF_EPSILON;
}

float trace_intensity(
    GpuColoredLightSource src, int tx, int ty, int tz, int dx, int dy, float target_frac )
{
    float fdx = (float)dx;
    float fdy = (float)dy;
    float fdz = ( (float)tz + target_frac - ( (float)src.z_idx + src.z_frac ) ) * z_scale;
    float dist = sqrt( fdx * fdx + fdy * fdy + fdz * fdz );

    if( dist > src.radius ) {
        return 0.0;
    }

    if( dist < 0.5 ) {
        return src.luminance;
    }

    int sdx = src.x - tx;
    int sdy = src.y - ty;
    int sdz = src.z_idx - tz;
    if( target_vehicle_surface_blocks_source( src, tx, ty, tz, target_frac ) ) {
        return 0.0;
    }

    int steps = max( abs( sdx ), max( abs( sdy ), (int)ceil( abs( fdz ) ) ) );
    steps = max( steps, 1 );

    float avg_transparency = LIGHT_TRANSPARENCY_OPEN_AIR;

    if( sdz != 0 ) {
        int sign_z = sdz > 0 ? 1 : -1;
        int crossings = abs( sdz );
        for( int k = 0; k < crossings; ++k ) {
            float t_z = ( (float)k + 0.5 ) / (float)crossings;
            int ix_z = clamp( tx + round_nearest_int( (float)sdx * t_z ), 0, cache_x - 1 );
            int iy_z = clamp( ty + round_nearest_int( (float)sdy * t_z ), 0, cache_y - 1 );
            int floor_z = sign_z > 0 ? tz + k + 1 : tz - k;
            if( floor_z >= 0 && floor_z < z_count ) {
                int fl_idx = tile_index( ix_z, iy_z, floor_z );
                if( floor_all[fl_idx] != 0u ) {
                    return 0.0;
                }
                if( vehicle_floor_all[fl_idx] != 0u &&
                    ( src.flags & LIGHT_SOURCE_EXTERNAL_VEHICLE ) == 0u ) {
                    return 0.0;
                }
            }
        }
    }

    for( int i = 1; i < steps; ++i ) {
        float t = (float)i / (float)steps;
        int ix = clamp( tx + round_nearest_int( (float)sdx * t ), 0, cache_x - 1 );
        int iy = clamp( ty + round_nearest_int( (float)sdy * t ), 0, cache_y - 1 );
        int iz = clamp( tz + round_nearest_int( (float)sdz * t ), 0, z_count - 1 );

        float t_val = transparency_all[tile_index( ix, iy, iz )];
        if( t_val <= LIGHT_TRANSPARENCY_SOLID ) {
            float source_h = (float)src.z_idx + src.z_frac;
            float target_h = (float)tz + target_frac;
            float ray_h = target_h + ( source_h - target_h ) * t;
            if( ray_sample_is_above_vehicle_surface( ix, iy, iz, ray_h ) ) {
                continue;
            }
            return 0.0;
        }

        avg_transparency = ( (float)( i - 1 ) * avg_transparency + t_val ) / (float)i;
    }

    return min( src.luminance, src.luminance / ( exp( avg_transparency * dist ) * dist ) );
}

uint packed_color_value( float intensity, uint color_rgb )
{
    uint rank = (uint)clamp( round( intensity * 2.0 ), 1.0, 255.0 );
    return ( rank << 24 ) | ( color_rgb & 0x00ffffffu );
}

[numthreads(8, 8, 1)]
void main( uint3 group_id : SV_GroupID, uint3 thread_id : SV_GroupThreadID )
{
    if( group_id.x >= num_sources ) {
        return;
    }

    GpuColoredLightSource src = light_sources[source_offset + group_id.x];

    int dx = (int)( group_id.y * 8 + thread_id.x ) - max_radius;
    int dy = (int)( group_id.z * 8 + thread_id.y ) - max_radius;
    int tx = src.x + dx;
    int ty = src.y + dy;

    if( tx < 0 || ty < 0 || tx >= cache_x || ty >= cache_y ) {
        return;
    }

    float fdx = (float)dx;
    float fdy = (float)dy;
    float dist_xy_sq = fdx * fdx + fdy * fdy;
    if( ( src.flags & LIGHT_SOURCE_DIRECTIONAL ) != 0u ) {
        if( dist_xy_sq <= 0.0001 ) {
            return;
        }
        float cone_dot = ( fdx * src.dir_x + fdy * src.dir_y ) * rsqrt( dist_xy_sq );
        if( cone_dot < src.cone_cos ) {
            return;
        }
    }

    int z_span = (int)ceil( src.radius / z_scale );
    int z_min = max( 0, src.z_idx - z_span );
    int z_max = min( z_count - 1, src.z_idx + z_span );

    for( int tz = z_min; tz <= z_max; ++tz ) {
        int idx = tile_index( tx, ty, tz );
        bool vehicle_surface = has_vehicle_surface( tx, ty, tz );
        float target_frac =
            vehicle_surface ? VEHICLE_ROOF_SURFACE_Z_FRAC : open_target_z_frac( src );
        float intensity = trace_intensity( src, tx, ty, tz, dx, dy, target_frac );
        if( intensity > LIGHT_AMBIENT_LOW ) {
            InterlockedMax( color_all[idx], packed_color_value( intensity, src.color_rgb ) );
        }
    }
}
