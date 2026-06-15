// GPU Lighting - uint buffer fill

cbuffer Constants : register(b0, space2)
{
    uint total_tiles;
    uint value;
    uint2 _pad;
};

RWStructuredBuffer<uint> target_values : register(u0, space1);

[numthreads(64, 1, 1)]
void main( uint3 dispatch_thread_id : SV_DispatchThreadID )
{
    uint idx = dispatch_thread_id.x;
    if( idx >= total_tiles ) {
        return;
    }

    target_values[idx] = value;
}
