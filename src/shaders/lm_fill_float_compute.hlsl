// GPU Lighting - float buffer fill

cbuffer Constants : register(b0, space2)
{
    uint  total_tiles;
    int   cache_xy;
    int   z_start_idx;
    float value;
};

RWStructuredBuffer<float> target_values : register(u0, space1);

[numthreads(64, 1, 1)]
void main( uint3 dispatch_thread_id : SV_DispatchThreadID )
{
    uint idx = dispatch_thread_id.x;
    if( idx >= total_tiles ) {
        return;
    }

    uint dst_idx = (uint)( z_start_idx * cache_xy ) + idx;
    target_values[dst_idx] = value;
}
