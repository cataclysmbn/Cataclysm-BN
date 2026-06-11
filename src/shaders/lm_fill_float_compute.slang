// GPU Lighting - float buffer fill

static const uint FILL_FLOAT_THREADS_X = 64u;

struct FillFloatConstants {
    uint  total_tiles;
    int   cache_xy;
    int   z_start_idx;
    float value;
};

[[vk::binding(0, 2)]]
ConstantBuffer<FillFloatConstants> constants : register(b0, space2);

[[vk::binding(0, 1)]]
RWStructuredBuffer<float> target_values : register(u0, space1);

void fill_float_main_impl( uint3 group_id, uint3 group_thread_id )
{
    uint idx = group_id.x * FILL_FLOAT_THREADS_X + group_thread_id.x;
    if( idx >= constants.total_tiles ) {
        return;
    }

    uint dst_idx = uint( constants.z_start_idx * constants.cache_xy ) + idx;
    target_values[dst_idx] = constants.value;
}

[numthreads(64, 1, 1)]
void main( uint3 group_id : SV_GroupID, uint3 group_thread_id : SV_GroupThreadID )
{
    fill_float_main_impl( group_id, group_thread_id );
}

[numthreads(64, 1, 1)]
void cpu_main( uint3 group_id : SV_GroupID, uint3 group_thread_id : SV_GroupThreadID )
{
    fill_float_main_impl( group_id, group_thread_id );
}
