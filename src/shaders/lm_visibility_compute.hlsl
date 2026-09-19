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
    float detail_range;
    int   z_start_idx;
    int   dispatch_z_count;
    int   _pad0;
    int   _pad1;
    int   _pad2;
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
    float best = 0.0;
    for( uint offset_index = 0u; offset_index < 8u; ++offset_index ) {
        int nx = x;
        int ny = y;
        if( offset_index == 0u ) {
            ny = y + 1;
        } else if( offset_index == 1u ) {
            ny = y - 1;
        } else if( offset_index == 2u ) {
            nx = x + 1;
        } else if( offset_index == 3u ) {
            nx = x + 1;
            ny = y + 1;
        } else if( offset_index == 4u ) {
            nx = x + 1;
            ny = y - 1;
        } else if( offset_index == 5u ) {
            nx = x - 1;
        } else if( offset_index == 6u ) {
            nx = x - 1;
            ny = y + 1;
        } else {
            nx = x - 1;
            ny = y - 1;
        }
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

uint classify_light(
    int dist, bool obstructed, float apparent_light, bool source_light, bool camera_visible )
{
    if( dist > u_unimpaired_range ) {
        if( !obstructed && source_light ) {
            return LIT_BRIGHT_ONLY;
        }
        return LIT_DARK;
    }
    if( (float)dist > detail_range && !camera_visible ) {
        if( !obstructed && source_light ) {
            return LIT_BRIGHT_ONLY;
        }
        return LIT_BLANK;
    }

    if( obstructed ) {
        if( apparent_light > LIGHT_AMBIENT_LIT ) {
            if( apparent_light > (float)g_light_level ) {
                return LIT_BRIGHT_ONLY;
            }
            return LIT_LOW;
        }
        if( apparent_light >= vision_threshold ) {
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
    if( apparent_light >= vision_threshold ) {
        return LIT_LOW;
    }
    return LIT_BLANK;
}

[numthreads(64, 1, 1)]
void main( uint3 dispatch_id : SV_DispatchThreadID )
{
    uint local_idx = dispatch_id.x;
    if( local_idx >= (uint)( dispatch_z_count * cache_xy ) ) {
        return;
    }

    uint z_idx = (uint)z_start_idx + local_idx / (uint)cache_xy;
    if( z_idx >= (uint)z_count ) {
        return;
    }
    uint tile = local_idx % (uint)cache_xy;
    uint idx = z_idx * (uint)cache_xy + tile;

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

    float camera_vis = asfloat( camera_all[idx] );
    float vis = max( seen_all[idx], camera_vis );
    bool obstructed = vis <= visible_threshold;
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
