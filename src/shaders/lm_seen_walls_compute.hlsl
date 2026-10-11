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

static const float LIGHT_TRANSPARENCY_SOLID    = 0.0;
static const float LIGHT_TRANSPARENCY_OPEN_AIR = 0.038376418216;

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
    int   z_start_idx;
    int   dispatch_z_count;
    uint  trigdist;
};

StructuredBuffer<float> transparency_all : register(t0, space0);
StructuredBuffer<float> seen_src_all     : register(t1, space0);
StructuredBuffer<uint>  vehicle_floor_all : register(t2, space0);
StructuredBuffer<uint>  vehicle_obscured_all : register(t3, space0);

RWStructuredBuffer<float> seen_dst_all : register(u0, space1);

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

bool blocked_by_vehicle_diagonal( int from_x, int from_y, int to_x, int to_y, int z )
{
    int dx = to_x - from_x;
    int dy = to_y - from_y;
    if( abs( dx ) != 1 || abs( dy ) != 1 ) {
        return false;
    }
    if( dx == -1 && dy == -1 ) {
        return ( vehicle_obscured_all[tile_index( from_x, from_y, z )] & 1u ) != 0u;
    }
    if( dx == 1 && dy == -1 ) {
        return ( vehicle_obscured_all[tile_index( from_x, from_y, z )] & 2u ) != 0u;
    }
    if( dx == -1 && dy == 1 ) {
        return ( vehicle_obscured_all[tile_index( to_x, to_y, z )] & 2u ) != 0u;
    }
    return ( vehicle_obscured_all[tile_index( to_x, to_y, z )] & 1u ) != 0u;
}

[numthreads(8, 8, 1)]
void main( uint3 group_id : SV_GroupID, uint3 thread_id : SV_GroupThreadID )
{
    int tx = player_x + (int)( group_id.x * 8 + thread_id.x ) - view_radius;
    int ty = player_y + (int)( group_id.y * 8 + thread_id.y ) - view_radius;
    int tz = z_start_idx + (int)group_id.z;

    if( tx < 0 || ty < 0 || tx >= cache_x || ty >= cache_y ||
        group_id.z >= (uint)dispatch_z_count || tz >= z_count ) {
        return;
    }

    int dist = visibility_distance( abs( tx - player_x ), abs( ty - player_y ),
                                    abs( tz - player_z_idx ) );

    int idx = tile_index( tx, ty, tz );

    if( dist == 0 ) {
        seen_dst_all[idx] = 1.0;
        return;
    }

    if( dist > view_radius ) {
        seen_dst_all[idx] = 0.0;
        return;
    }

    float primary = seen_src_all[idx];

    if( transparency_all[idx] == LIGHT_TRANSPARENCY_OPEN_AIR ) {
        seen_dst_all[idx] = primary;
        return;
    }

    float best = primary;

    for( uint offset_index = 0u; offset_index < 8u; ++offset_index ) {
        int nx = tx;
        int ny = ty;
        if( offset_index == 0u ) {
            ny = ty + 1;
        } else if( offset_index == 1u ) {
            ny = ty - 1;
        } else if( offset_index == 2u ) {
            nx = tx + 1;
        } else if( offset_index == 3u ) {
            nx = tx + 1;
            ny = ty + 1;
        } else if( offset_index == 4u ) {
            nx = tx + 1;
            ny = ty - 1;
        } else if( offset_index == 5u ) {
            nx = tx - 1;
        } else if( offset_index == 6u ) {
            nx = tx - 1;
            ny = ty + 1;
        } else {
            nx = tx - 1;
            ny = ty - 1;
        }
        if( nx < 0 || ny < 0 || nx >= cache_x || ny >= cache_y ) {
            continue;
        }

        int nidx = tile_index( nx, ny, tz );
        if( transparency_all[nidx] <= LIGHT_TRANSPARENCY_SOLID ) {
            continue;
        }
        if( blocked_by_vehicle_diagonal( nx, ny, tx, ty, tz ) ) {
            continue;
        }

        best = max( best, seen_src_all[nidx] );
    }

    if( tz < player_z_idx && tz + 1 < z_count ) {
        int roof_idx = tile_index( tx, ty, tz + 1 );
        if( vehicle_floor_all[roof_idx] != 0u ) {
            best = 0.0;
        }
    }

    seen_dst_all[idx] = best;
}
