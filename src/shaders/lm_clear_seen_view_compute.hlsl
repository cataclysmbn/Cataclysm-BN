// GPU Lighting - Seen View Clear Pass
//
// Clears the resident raw/final seen buffers for one previous player-view square.

cbuffer Constants : register(b0, space2)
{
    int player_x;
    int player_y;
    int cache_x;
    int cache_y;
    int cache_xy;
    int z_count;
    int view_radius;
    int z_start_idx;
    int dispatch_z_count;
    uint3 _pad0;
};

RWStructuredBuffer<float> seen_raw_all : register(u0, space1);
RWStructuredBuffer<float> seen_all     : register(u1, space1);

[numthreads(8, 8, 1)]
void main( uint3 group_id : SV_GroupID, uint3 thread_id : SV_GroupThreadID )
{
    int x = player_x + (int)( group_id.x * 8 + thread_id.x ) - view_radius;
    int y = player_y + (int)( group_id.y * 8 + thread_id.y ) - view_radius;
    int dispatch_z = (int)group_id.z;
    int z = z_start_idx + dispatch_z;

    if( x < 0 || y < 0 || x >= cache_x || y >= cache_y ||
        dispatch_z >= dispatch_z_count || z < 0 || z >= z_count ) {
        return;
    }

    int idx = z * cache_xy + x * cache_y + y;
    seen_raw_all[idx] = 0.0;
    seen_all[idx] = 0.0;
}
