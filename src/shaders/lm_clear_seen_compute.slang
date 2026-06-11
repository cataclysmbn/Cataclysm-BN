// GPU Lighting - Seen Buffer Clear Pass

static const uint CLEAR_SEEN_THREADS_X = 64u;

struct ClearSeenConstants {
    uint total_tiles;
    int  cache_xy;
    int  z_start_idx;
    uint _pad2;
};

[[vk::binding(0, 2)]]
ConstantBuffer<ClearSeenConstants> constants : register(b0, space2);

[[vk::binding(0, 1)]]
RWStructuredBuffer<float> seen_raw_all : register(u0, space1);

[[vk::binding(1, 1)]]
RWStructuredBuffer<float> seen_all : register(u1, space1);

void clear_seen_main_impl( uint3 group_id, uint3 group_thread_id )
{
    uint idx = group_id.x * CLEAR_SEEN_THREADS_X + group_thread_id.x;
    if( idx >= constants.total_tiles ) {
        return;
    }

    uint dst_idx = uint( constants.z_start_idx * constants.cache_xy ) + idx;
    seen_raw_all[dst_idx] = 0.0;
    seen_all[dst_idx] = 0.0;
}

[numthreads(64, 1, 1)]
void main( uint3 group_id : SV_GroupID, uint3 group_thread_id : SV_GroupThreadID )
{
    clear_seen_main_impl( group_id, group_thread_id );
}

[numthreads(64, 1, 1)]
void cpu_main( uint3 group_id : SV_GroupID, uint3 group_thread_id : SV_GroupThreadID )
{
    clear_seen_main_impl( group_id, group_thread_id );
}
