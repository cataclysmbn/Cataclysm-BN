// GPU Lighting - Seen-Cache Surface Pass
//
// The primary seen-cache pass traces one center-to-center ray per target tile.
// That misses visible surface tiles at glancing angles because the ray to a
// distant wall/window/vehicle part crosses nearer blockers first.
//
// This pass runs after lm_seen_compute.  It lets a target inherit visibility
// from any raw-visible adjacent transparent tile.  The source buffer is the raw
// center-ray result, not this pass's output, so this closes one-tile surface
// notches without cascading visibility through rooms.

static const float LIGHT_TRANSPARENCY_SOLID = 0.0;

cbuffer Constants : register(b0, space2)
{
    int   player_x;
    int   player_y;
    int   player_z_idx;
    int   cache_x;
    int   cache_y;
    int   cache_xy;
    int   z_count;
    int   view_radius;
    float z_scale;
    uint  _pad0;
    uint  _pad1;
    uint  _pad2;
};

StructuredBuffer<float> transparency_all : register(t0, space0);
StructuredBuffer<float> seen_src_all     : register(t1, space0);

RWStructuredBuffer<float> seen_dst_all : register(u0, space1);

int tile_index( int x, int y, int z )
{
    return z * cache_xy + x * cache_y + y;
}

[numthreads(8, 8, 1)]
void main( uint3 group_id : SV_GroupID, uint3 thread_id : SV_GroupThreadID )
{
    int tx = player_x + (int)( group_id.x * 8 + thread_id.x ) - view_radius;
    int ty = player_y + (int)( group_id.y * 8 + thread_id.y ) - view_radius;
    int tz = (int)group_id.z;

    if( tx < 0 || ty < 0 || tx >= cache_x || ty >= cache_y || tz >= z_count ) {
        return;
    }

    float fdx  = (float)( tx - player_x );
    float fdy  = (float)( ty - player_y );
    float fdz  = (float)( tz - player_z_idx ) * z_scale;
    float dist = sqrt( fdx * fdx + fdy * fdy + fdz * fdz );

    int idx = tile_index( tx, ty, tz );

    if( dist < 0.5 ) {
        seen_dst_all[idx] = 1.0;
        return;
    }

    if( dist > (float)view_radius ) {
        seen_dst_all[idx] = 0.0;
        return;
    }

    float primary = seen_src_all[idx];

    float best = primary;

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

    [unroll]
    for( int i = 0; i < 8; ++i ) {
        int nx = tx + offsets[i].x;
        int ny = ty + offsets[i].y;
        if( nx < 0 || ny < 0 || nx >= cache_x || ny >= cache_y ) {
            continue;
        }

        int nidx = tile_index( nx, ny, tz );
        if( transparency_all[nidx] <= LIGHT_TRANSPARENCY_SOLID ) {
            continue;
        }

        best = max( best, seen_src_all[nidx] );
    }

    seen_dst_all[idx] = best;
}
