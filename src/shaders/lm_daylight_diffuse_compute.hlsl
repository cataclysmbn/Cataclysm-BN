// GPU Lighting - Local Daylight Diffusion Pass
//
// Propagates daylight from physical sun/shadow seed tiles into adjacent covered
// transparent tiles.  This is intentionally separate from point-light raytrace:
// direct sun and angled-sun shadow tiles are immutable seeds, while only tiles
// without a daylight seed gather attenuated daylight from cardinal neighbours.

static const float LIGHT_TRANSPARENCY_SOLID = 0.0;

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

StructuredBuffer<uint>  daylight_seed_all : register(t0, space0);
StructuredBuffer<uint>  daylight_src_all  : register(t1, space0);
StructuredBuffer<float> transparency_all  : register(t2, space0);
StructuredBuffer<float> source_map_all    : register(t3, space0);

RWStructuredBuffer<uint> daylight_dst_all : register(u0, space1);
RWStructuredBuffer<uint> lm_all           : register(u1, space1);

int tile_index( int x, int y, int z )
{
    return z * cache_xy + x * cache_y + y;
}

bool inbounds_xy( int x, int y )
{
    return x >= 0 && y >= 0 && x < cache_x && y < cache_y;
}

[numthreads(64, 1, 1)]
void main( uint3 dispatch_thread_id : SV_DispatchThreadID )
{
    uint idx = dispatch_thread_id.x;
    if( idx >= total_tiles ) {
        return;
    }

    if( source_map_all[idx] < 0.0 ) {
        daylight_dst_all[idx] = 0u;
        return;
    }

    uint seed_bits = daylight_seed_all[idx];
    float seed = asfloat( seed_bits );
    if( seed > 0.0 ) {
        daylight_dst_all[idx] = seed_bits;
        lm_all[idx] = max( lm_all[idx], seed_bits );
        return;
    }

    if( transparency_all[idx] <= LIGHT_TRANSPARENCY_SOLID ) {
        daylight_dst_all[idx] = 0u;
        return;
    }

    uint z_idx = idx / (uint)cache_xy;
    uint tile = idx % (uint)cache_xy;
    int x = (int)( tile / (uint)cache_y );
    int y = (int)( tile % (uint)cache_y );
    int z = (int)z_idx;

    float best = asfloat( daylight_src_all[idx] );
    for( uint offset_index = 0u; offset_index < 4u; ++offset_index ) {
        int nx = x;
        int ny = y;
        if( offset_index == 0u ) {
            ny = y + 1;
        } else if( offset_index == 1u ) {
            ny = y - 1;
        } else if( offset_index == 2u ) {
            nx = x + 1;
        } else {
            nx = x - 1;
        }
        if( !inbounds_xy( nx, ny ) ) {
            continue;
        }

        int nidx = tile_index( nx, ny, z );
        if( source_map_all[nidx] < 0.0 ) {
            continue;
        }
        if( transparency_all[nidx] <= LIGHT_TRANSPARENCY_SOLID ) {
            continue;
        }

        float neighbour = asfloat( daylight_src_all[nidx] );
        if( neighbour <= min_light ) {
            continue;
        }
        best = max( best, neighbour * diffuse_decay );
    }

    if( best < min_light ) {
        daylight_dst_all[idx] = 0u;
        return;
    }

    uint best_bits = asuint( best );
    daylight_dst_all[idx] = best_bits;
    lm_all[idx] = max( lm_all[idx], best_bits );
}
