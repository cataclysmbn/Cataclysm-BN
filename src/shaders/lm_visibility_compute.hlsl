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

cbuffer Constants : register(b0, space2)
{
    int   player_x;
    int   player_y;
    int   player_z_idx;
    int   cache_x;
    int   cache_y;
    int   cache_xy;
    int   z_count;
    uint  trigdist;
    int   u_clairvoyance;
    int   u_unimpaired_range;
    int   g_light_level;
    float vision_threshold;
    float visibility_scale_factor;
    float visible_threshold;
    uint  _pad0;
    uint  _pad1;
};

StructuredBuffer<float> transparency_all : register(t0, space0);
StructuredBuffer<uint>  lm_all           : register(t1, space0);
StructuredBuffer<float> seen_all         : register(t2, space0);
StructuredBuffer<uint>  camera_all       : register(t3, space0);
StructuredBuffer<float> source_map_all   : register(t4, space0);

RWStructuredBuffer<uint> visibility_all : register(u0, space1);

int tile_index( int x, int y, int z )
{
    return z * cache_xy + x * cache_y + y;
}

int visibility_distance( int dx, int dy, int dz )
{
    if( trigdist == 0u ) {
        return max( dx, max( dy, dz ) );
    }
    return (int)round( sqrt( (float)( dx * dx + dy * dy + dz * dz ) ) );
}

float scaled_visibility_for_view_distance( float vis )
{
    if( vis <= LIGHT_TRANSPARENCY_SOLID ) {
        return 0.0;
    }
    if( vis >= VISIBILITY_FULL ) {
        return VISIBILITY_FULL;
    }
    if( visibility_scale_factor == 1.0 ) {
        return vis;
    }
    return pow( vis, visibility_scale_factor );
}

bool is_opaque( int x, int y, int z )
{
    int idx = tile_index( x, y, z );
    return transparency_all[idx] <= LIGHT_TRANSPARENCY_SOLID &&
           ( x != player_x || y != player_y );
}

float visible_surface_light( int x, int y, int z )
{
    static const int2 offsets[8] = {
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
        if( nx < 0 || ny < 0 || nx >= cache_x || ny >= cache_y ) {
            continue;
        }
        if( is_opaque( nx, ny, z ) ) {
            continue;
        }

        int nidx = tile_index( nx, ny, z );
        if( seen_all[nidx] == 0.0 && asfloat( camera_all[nidx] ) == 0.0 ) {
            continue;
        }
        best = max( best, asfloat( lm_all[nidx] ) );
    }
    return best;
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
    int z = (int)z_idx;

    int dx = abs( x - player_x );
    int dy = abs( y - player_y );
    int dz = abs( z - player_z_idx );
    int dist = visibility_distance( dx, dy, dz );

    if( dist <= u_clairvoyance ) {
        visibility_all[idx] = LIT_BRIGHT;
        return;
    }

    float vis = max( seen_all[idx], asfloat( camera_all[idx] ) );
    bool obstructed = vis <= visible_threshold;
    float scaled_vis = scaled_visibility_for_view_distance( vis );
    bool opaque = is_opaque( x, y, z );

    float apparent_light;
    if( opaque && scaled_vis > 0.0 ) {
        apparent_light = scaled_vis * visible_surface_light( x, y, z );
    } else {
        apparent_light = scaled_vis * asfloat( lm_all[idx] );
    }

    if( dist > u_unimpaired_range ) {
        if( !obstructed && source_map_all[idx] > 0.0 ) {
            visibility_all[idx] = LIT_BRIGHT_ONLY;
        } else {
            visibility_all[idx] = LIT_DARK;
        }
        return;
    }

    if( obstructed ) {
        if( apparent_light > LIGHT_AMBIENT_LIT ) {
            if( apparent_light > (float)g_light_level ) {
                visibility_all[idx] = LIT_BRIGHT_ONLY;
            } else {
                visibility_all[idx] = LIT_LOW;
            }
        } else if( apparent_light >= vision_threshold ) {
            visibility_all[idx] = LIT_LOW;
        } else {
            visibility_all[idx] = LIT_BLANK;
        }
        return;
    }

    if( apparent_light > LIGHT_SOURCE_BRIGHT || source_map_all[idx] > 0.0 ) {
        visibility_all[idx] = LIT_BRIGHT;
    } else if( apparent_light > LIGHT_AMBIENT_LIT ) {
        visibility_all[idx] = LIT_LIT;
    } else if( apparent_light >= vision_threshold ) {
        visibility_all[idx] = LIT_LOW;
    } else {
        visibility_all[idx] = LIT_BLANK;
    }
}
