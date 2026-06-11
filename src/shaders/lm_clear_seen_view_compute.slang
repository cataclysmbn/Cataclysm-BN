// GPU Lighting - Seen View Clear Pass

static const uint CLEAR_SEEN_VIEW_THREADS_X = 8u;
static const uint CLEAR_SEEN_VIEW_THREADS_Y = 8u;

struct ClearSeenViewConstants {
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

[[vk::binding(0, 2)]]
ConstantBuffer<ClearSeenViewConstants> constants : register(b0, space2);

[[vk::binding(0, 1)]]
RWStructuredBuffer<float> seen_raw_all : register(u0, space1);

[[vk::binding(1, 1)]]
RWStructuredBuffer<float> seen_all : register(u1, space1);

void clear_seen_view_main_impl( uint3 group_id, uint3 group_thread_id )
{
    int x = constants.player_x + int( group_id.x * CLEAR_SEEN_VIEW_THREADS_X + group_thread_id.x ) -
            constants.view_radius;
    int y = constants.player_y + int( group_id.y * CLEAR_SEEN_VIEW_THREADS_Y + group_thread_id.y ) -
            constants.view_radius;
    int dispatch_z = int( group_id.z );
    int z = constants.z_start_idx + dispatch_z;

    if( x < 0 || y < 0 || x >= constants.cache_x || y >= constants.cache_y ||
        dispatch_z >= constants.dispatch_z_count || z < 0 || z >= constants.z_count ) {
        return;
    }

    int idx = z * constants.cache_xy + x * constants.cache_y + y;
    seen_raw_all[idx] = 0.0;
    seen_all[idx] = 0.0;
}

[numthreads(8, 8, 1)]
void main( uint3 group_id : SV_GroupID, uint3 group_thread_id : SV_GroupThreadID )
{
    clear_seen_view_main_impl( group_id, group_thread_id );
}

[numthreads(8, 8, 1)]
void cpu_main( uint3 group_id : SV_GroupID, uint3 group_thread_id : SV_GroupThreadID )
{
    clear_seen_view_main_impl( group_id, group_thread_id );
}
