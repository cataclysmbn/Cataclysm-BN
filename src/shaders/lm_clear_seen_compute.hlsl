// GPU Lighting - Seen Buffer Clear Pass
//
// Clears the resident raw/final seen buffers before rebuilding player FoV.

cbuffer Constants : register(b0, space2)
{
    uint total_tiles;
    int  cache_xy;
    int  z_start_idx;
    uint _pad2;
};

RWStructuredBuffer<float> seen_raw_all : register(u0, space1);
RWStructuredBuffer<float> seen_all     : register(u1, space1);

[numthreads(64, 1, 1)]
void main( uint3 dispatch_id : SV_DispatchThreadID )
{
    uint idx = dispatch_id.x;
    if( idx >= total_tiles ) {
        return;
    }

    uint dst_idx = (uint)( z_start_idx * cache_xy ) + idx;
    seen_raw_all[dst_idx] = 0.0;
    seen_all[dst_idx] = 0.0;
}
