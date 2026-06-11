// GPU Lighting - Local Daylight Diffusion Pass

static const float LIGHT_TRANSPARENCY_SOLID = 0.0;
static const uint DAYLIGHT_DIFFUSE_THREADS_X = 64u;

[[vk::binding(0, 2)]]
cbuffer Constants : register(b0, space2)
{
    uint  total_tiles;
    int   cache_x;
    int   cache_y;
    int   cache_xy;
    int   z_count;
    float diffuse_decay;
    float min_light;
    uint  _pad;
};

[[vk::binding(0, 0)]]
StructuredBuffer<uint> daylight_seed_all : register(t0, space0);

[[vk::binding(1, 0)]]
StructuredBuffer<uint> daylight_src_all : register(t1, space0);

[[vk::binding(2, 0)]]
StructuredBuffer<float> transparency_all : register(t2, space0);

[[vk::binding(0, 1)]]
RWStructuredBuffer<uint> daylight_dst_all : register(u0, space1);

[[vk::binding(1, 1)]]
RWStructuredBuffer<uint> lm_all : register(u1, space1);

int tile_index( int x, int y, int z )
{
    return z * cache_xy + x * cache_y + y;
}

bool inbounds_xy( int x, int y )
{
    return x >= 0 && y >= 0 && x < cache_x && y < cache_y;
}

void max_lm_bits( uint idx, uint candidate_bits )
{
    uint current_bits = lm_all[idx];
    if( candidate_bits > current_bits ) {
        lm_all[idx] = candidate_bits;
    }
}

void daylight_diffuse_main_impl( uint3 group_id, uint3 group_thread_id )
{
    uint idx = group_id.x * DAYLIGHT_DIFFUSE_THREADS_X + group_thread_id.x;
    if( idx >= total_tiles ) {
        return;
    }

    uint seed_bits = daylight_seed_all[idx];
    float seed = asfloat( seed_bits );
    if( seed > 0.0 ) {
        daylight_dst_all[idx] = seed_bits;
        max_lm_bits( idx, seed_bits );
        return;
    }

    if( transparency_all[idx] <= LIGHT_TRANSPARENCY_SOLID ) {
        daylight_dst_all[idx] = 0u;
        return;
    }

    uint z_idx = idx / uint( cache_xy );
    uint tile = idx % uint( cache_xy );
    int x = int( tile / uint( cache_y ) );
    int y = int( tile % uint( cache_y ) );
    int z = int( z_idx );

    float best = asfloat( daylight_src_all[idx] );
    int2 offsets[4] = {
        int2(  0,  1 ),
        int2(  0, -1 ),
        int2(  1,  0 ),
        int2( -1,  0 )
    };

    [unroll]
    for( int i = 0; i < 4; ++i ) {
        int nx = x + offsets[i].x;
        int ny = y + offsets[i].y;
        if( !inbounds_xy( nx, ny ) ) {
            continue;
        }

        int nidx = tile_index( nx, ny, z );
        if( transparency_all[nidx] <= LIGHT_TRANSPARENCY_SOLID ) {
            continue;
        }

        float neighbour = asfloat( daylight_src_all[nidx] );
        if( neighbour <= min_light ) {
            continue;
        }

        float candidate = neighbour * diffuse_decay;
        if( candidate > best ) {
            best = candidate;
        }
    }

    if( best < min_light ) {
        daylight_dst_all[idx] = 0u;
        return;
    }

    uint best_bits = asuint( best );
    daylight_dst_all[idx] = best_bits;
    max_lm_bits( idx, best_bits );
}

[numthreads(64, 1, 1)]
void main( uint3 group_id : SV_GroupID, uint3 group_thread_id : SV_GroupThreadID )
{
    daylight_diffuse_main_impl( group_id, group_thread_id );
}

[numthreads(64, 1, 1)]
void cpu_main( uint3 group_id : SV_GroupID, uint3 group_thread_id : SV_GroupThreadID )
{
    daylight_diffuse_main_impl( group_id, group_thread_id );
}
