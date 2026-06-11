// GPU Lighting - uint buffer fill

static const uint FILL_UINT_THREADS_X = 64u;

struct FillUintConstants
{
    uint total_tiles;
    uint value;
    uint2 _pad;
};

[[vk::binding(0, 2)]]
ConstantBuffer<FillUintConstants> constants : register(b0, space2);

[[vk::binding(0, 1)]]
RWStructuredBuffer<uint> target_values : register(u0, space1);

void fill_uint_main_impl( uint3 group_id, uint3 group_thread_id )
{
    uint idx = group_id.x * FILL_UINT_THREADS_X + group_thread_id.x;
    if( idx >= constants.total_tiles ) {
        return;
    }

    target_values[idx] = constants.value;
}

[numthreads(64, 1, 1)]
void main( uint3 group_id : SV_GroupID, uint3 group_thread_id : SV_GroupThreadID )
{
    fill_uint_main_impl( group_id, group_thread_id );
}

[numthreads(64, 1, 1)]
void cpu_main( uint3 group_id : SV_GroupID, uint3 group_thread_id : SV_GroupThreadID )
{
    fill_uint_main_impl( group_id, group_thread_id );
}
