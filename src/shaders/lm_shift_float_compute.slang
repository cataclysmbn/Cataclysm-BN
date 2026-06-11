// GPU Lighting - Float Bubble Shift Pass

static const uint SHIFT_FLOAT_THREADS_X = 64u;

struct ShiftFloatConstants {
    int   cache_x;
    int   cache_y;
    int   cache_xy;
    int   z_count;
    int   shift_x_tiles;
    int   shift_y_tiles;
    float fill_value;
    uint  _pad0;
};

[[vk::binding(0, 2)]]
ConstantBuffer<ShiftFloatConstants> constants : register(b0, space2);

[[vk::binding(0, 0)]]
StructuredBuffer<float> src_all : register(t0, space0);

[[vk::binding(0, 1)]]
RWStructuredBuffer<float> dst_all : register(u0, space1);

void shift_float_main_impl( uint3 group_id, uint3 group_thread_id )
{
    uint dst_idx = group_id.x * SHIFT_FLOAT_THREADS_X + group_thread_id.x;
    uint total = uint( constants.z_count * constants.cache_xy );
    if( dst_idx >= total ) {
        return;
    }

    int z = int( dst_idx / uint( constants.cache_xy ) );
    int rem = int( dst_idx - uint( z * constants.cache_xy ) );
    int x = rem / constants.cache_y;
    int y = rem - x * constants.cache_y;

    int source_x = x + constants.shift_x_tiles;
    int source_y = y + constants.shift_y_tiles;

    if( source_x < 0 || source_x >= constants.cache_x || source_y < 0 || source_y >= constants.cache_y ) {
        dst_all[dst_idx] = constants.fill_value;
        return;
    }

    int src_idx = z * constants.cache_xy + source_x * constants.cache_y + source_y;
    dst_all[dst_idx] = src_all[src_idx];
}

[numthreads(64, 1, 1)]
void main( uint3 group_id : SV_GroupID, uint3 group_thread_id : SV_GroupThreadID )
{
    shift_float_main_impl( group_id, group_thread_id );
}

[numthreads(64, 1, 1)]
void cpu_main( uint3 group_id : SV_GroupID, uint3 group_thread_id : SV_GroupThreadID )
{
    shift_float_main_impl( group_id, group_thread_id );
}
