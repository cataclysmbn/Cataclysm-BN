// GPU Lighting - uint buffer max-combine

static const uint MAX_UINT_THREADS_X = 64u;

[[vk::binding(0, 2)]]
cbuffer Constants : register(b0, space2)
{
    uint total_tiles;
    uint3 _pad;
};

[[vk::binding(0, 0)]]
StructuredBuffer<uint> source_values : register(t0, space0);

[[vk::binding(0, 1)]]
RWStructuredBuffer<uint> target_values : register(u0, space1);

void max_uint_main_impl( uint3 group_id, uint3 group_thread_id )
{
    uint idx = group_id.x * MAX_UINT_THREADS_X + group_thread_id.x;
    if( idx >= total_tiles ) {
        return;
    }

    uint target = target_values[idx];
    uint source = source_values[idx];
    target_values[idx] = target > source ? target : source;
}

[numthreads(64, 1, 1)]
void main( uint3 group_id : SV_GroupID, uint3 group_thread_id : SV_GroupThreadID )
{
    max_uint_main_impl( group_id, group_thread_id );
}

[numthreads(64, 1, 1)]
void cpu_main( uint3 group_id : SV_GroupID, uint3 group_thread_id : SV_GroupThreadID )
{
    max_uint_main_impl( group_id, group_thread_id );
}
