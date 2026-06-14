// GPU Lighting - uint buffer max-combine

cbuffer Constants : register(b0, space2)
{
    uint total_tiles;
    uint3 _pad;
};

StructuredBuffer<uint> source_values : register(t0, space0);
StructuredBuffer<float> source_map_all : register(t1, space0);
RWStructuredBuffer<uint> target_values : register(u0, space1);

[numthreads(64, 1, 1)]
void main( uint3 dispatch_thread_id : SV_DispatchThreadID )
{
    uint idx = dispatch_thread_id.x;
    if( idx >= total_tiles ) {
        return;
    }

    if( source_map_all[idx] >= 0.0 ) {
        target_values[idx] = max( target_values[idx], source_values[idx] );
    }
}
