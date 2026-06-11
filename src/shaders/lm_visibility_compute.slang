// GPU Lighting - Final Visibility Classification Pass
//
// Converts lm/seen/camera/source data into lit_level enum values.  This mirrors
// map::apparent_light_at so render-facing code can continue reading
// level_cache::visibility_cache unchanged.

static const float LIGHT_TRANSPARENCY_SOLID = 0.0;
static const float VISIBILITY_FULL          = 1.0;
static const float LIGHT_SOURCE_BRIGHT      = 10.0;
static const float LIGHT_AMBIENT_LIT        = 10.0;

static const uint LIT_DARK        = 0u;
static const uint LIT_LOW         = 1u;
static const uint LIT_BRIGHT_ONLY = 2u;
static const uint LIT_LIT         = 3u;
static const uint LIT_BRIGHT      = 4u;
static const uint LIT_BLANK       = 6u;
static const uint VISIBILITY_THREADS_X = 64u;

struct VisibilityConstants {
    int player_x;
    int player_y;
    int player_z_idx;
    int cache_x;
    int cache_y;
    int cache_xy;
    int z_count;
    uint trigdist;
    int u_clairvoyance;
    int u_unimpaired_range;
    int g_light_level;
    float vision_threshold;
    float visibility_scale_factor;
    float visible_threshold;
    float detail_range;
    int z_start_idx;
    int dispatch_z_count;
    uint _pad0;
    uint _pad1;
    uint _pad2;
};

[[vk::binding(0, 2)]]
ConstantBuffer<VisibilityConstants> constants : register(b0, space2);

[[vk::binding(0, 0)]]
StructuredBuffer<float> transparency_all : register(t0, space0);

[[vk::binding(1, 0)]]
StructuredBuffer<uint> lm_all : register(t1, space0);

[[vk::binding(2, 0)]]
StructuredBuffer<float> seen_all : register(t2, space0);

[[vk::binding(3, 0)]]
StructuredBuffer<uint> camera_all : register(t3, space0);

[[vk::binding(4, 0)]]
StructuredBuffer<float> source_map_all : register(t4, space0);

[[vk::binding(0, 1)]]
RWStructuredBuffer<uint> visibility_all : register(u0, space1);

int tile_index( int x, int y, int z )
{
    return z * constants.cache_xy + x * constants.cache_y + y;
}

int visibility_distance( int dx, int dy, int dz )
{
    if( constants.trigdist == 0u ) {
        int best = dx;
        if( dy > best ) {
            best = dy;
        }
        if( dz > best ) {
            best = dz;
        }
        return best;
    }
    return int( round( sqrt( float( dx * dx + dy * dy + dz * dz ) ) ) );
}

float scaled_visibility_for_view_distance( float vis )
{
    if( vis <= LIGHT_TRANSPARENCY_SOLID ) {
        return 0.0;
    }
    if( vis >= VISIBILITY_FULL ) {
        return VISIBILITY_FULL;
    }
    if( constants.visibility_scale_factor == 1.0 ) {
        return vis;
    }
    return pow( vis, constants.visibility_scale_factor );
}

bool is_opaque( int x, int y, int z )
{
    int idx = tile_index( x, y, z );
    return transparency_all[idx] <= LIGHT_TRANSPARENCY_SOLID &&
           ( x != constants.player_x || y != constants.player_y );
}

float visible_surface_light( int x, int y, int z )
{
    int2 offsets[8] = {
        int2(  0,  1 ),
        int2(  0, -1 ),
        int2(  1,  0 ),
        int2(  1,  1 ),
        int2(  1, -1 ),
        int2( -1,  0 ),
        int2( -1,  1 ),
        int2( -1, -1 )
    };

    float best = 0.0;
    [unroll]
    for( int i = 0; i < 8; ++i ) {
        int nx = x + offsets[i].x;
        int ny = y + offsets[i].y;
        if( nx < 0 || ny < 0 || nx >= constants.cache_x || ny >= constants.cache_y ) {
            continue;
        }
        if( is_opaque( nx, ny, z ) ) {
            continue;
        }

        int nidx = tile_index( nx, ny, z );
        if( seen_all[nidx] == 0.0 && asfloat( camera_all[nidx] ) == 0.0 ) {
            continue;
        }
        float candidate = asfloat( lm_all[nidx] );
        if( candidate > best ) {
            best = candidate;
        }
    }
    return best;
}

uint classify_light(
    int dist, bool obstructed, float apparent_light, bool source_light, bool camera_visible )
{
    if( dist > constants.u_unimpaired_range ) {
        if( !obstructed && source_light ) {
            return LIT_BRIGHT_ONLY;
        }
        return LIT_DARK;
    }
    if( float( dist ) > constants.detail_range && !camera_visible ) {
        if( !obstructed && source_light ) {
            return LIT_BRIGHT_ONLY;
        }
        return LIT_BLANK;
    }

    if( obstructed ) {
        if( apparent_light > LIGHT_AMBIENT_LIT ) {
            if( apparent_light > float( constants.g_light_level ) ) {
                return LIT_BRIGHT_ONLY;
            }
            return LIT_LOW;
        }
        if( apparent_light >= constants.vision_threshold ) {
            return LIT_LOW;
        }
        return LIT_BLANK;
    }

    if( apparent_light > LIGHT_SOURCE_BRIGHT || source_light ) {
        return LIT_BRIGHT;
    }
    if( apparent_light > LIGHT_AMBIENT_LIT ) {
        return LIT_LIT;
    }
    if( apparent_light >= constants.vision_threshold ) {
        return LIT_LOW;
    }
    return LIT_BLANK;
}

void visibility_main_impl( uint3 group_id, uint3 group_thread_id )
{
    uint local_idx = group_id.x * VISIBILITY_THREADS_X + group_thread_id.x;
    if( local_idx >= uint( constants.dispatch_z_count * constants.cache_xy ) ) {
        return;
    }

    uint z_idx = uint( constants.z_start_idx ) + local_idx / uint( constants.cache_xy );
    if( z_idx >= uint( constants.z_count ) ) {
        return;
    }
    uint tile = local_idx % uint( constants.cache_xy );
    uint idx = z_idx * uint( constants.cache_xy ) + tile;

    int x = int( tile / uint( constants.cache_y ) );
    int y = int( tile % uint( constants.cache_y ) );
    int z = int( z_idx );

    int dx = abs( x - constants.player_x );
    int dy = abs( y - constants.player_y );
    int dz = abs( z - constants.player_z_idx );
    int dist = visibility_distance( dx, dy, dz );

    if( dist <= constants.u_clairvoyance ) {
        visibility_all[idx] = LIT_BRIGHT;
        return;
    }

    float camera_vis = asfloat( camera_all[idx] );
    float seen_vis = seen_all[idx];
    float vis = seen_vis > camera_vis ? seen_vis : camera_vis;
    bool obstructed = vis <= constants.visible_threshold;
    float scaled_vis = scaled_visibility_for_view_distance( vis );
    bool opaque = is_opaque( x, y, z );

    float apparent_light;
    if( opaque && scaled_vis > 0.0 ) {
        apparent_light = scaled_vis * visible_surface_light( x, y, z );
    } else {
        apparent_light = scaled_vis * asfloat( lm_all[idx] );
    }

    visibility_all[idx] =
        classify_light( dist, obstructed, apparent_light, source_map_all[idx] > 0.0,
                        camera_vis > LIGHT_TRANSPARENCY_SOLID );
}

[numthreads(64, 1, 1)]
void main( uint3 group_id : SV_GroupID, uint3 group_thread_id : SV_GroupThreadID )
{
    visibility_main_impl( group_id, group_thread_id );
}

[numthreads(64, 1, 1)]
void cpu_main( uint3 group_id : SV_GroupID, uint3 group_thread_id : SV_GroupThreadID )
{
    visibility_main_impl( group_id, group_thread_id );
}
