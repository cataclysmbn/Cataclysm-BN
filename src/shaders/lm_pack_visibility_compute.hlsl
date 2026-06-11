// GPU Lighting - Visibility Readback Packing Pass
//
// Packs four lit_level enum values into each uint so older DXBC devices can
// read back less data without changing the resident visibility buffer layout.

cbuffer Constants : register(b0, space2)
{
    uint total_words;
    int  cache_xy;
    int  words_per_level;
    int  z_start_idx;
    int  dispatch_z_count;
    int  z_count;
    uint2 _pad;
};

StructuredBuffer<uint> visibility_all : register(t0, space0);
RWStructuredBuffer<uint> visibility_packed : register(u0, space1);

[numthreads(64, 1, 1)]
void main( uint3 dispatch_thread_id : SV_DispatchThreadID )
{
    uint packed_idx = dispatch_thread_id.x;
    if( packed_idx >= total_words ) {
        return;
    }

    uint local_z = packed_idx / (uint)words_per_level;
    uint word_in_level = packed_idx % (uint)words_per_level;
    uint z_idx = (uint)z_start_idx + local_z;
    if( local_z >= (uint)dispatch_z_count || z_idx >= (uint)z_count ) {
        return;
    }

    uint base_tile = word_in_level * 4u;
    uint base_src = z_idx * (uint)cache_xy + base_tile;
    uint packed = 0u;

    [unroll]
    for( uint component = 0u; component < 4u; ++component ) {
        uint tile = base_tile + component;
        if( tile < (uint)cache_xy ) {
            packed |= ( visibility_all[base_src + component] & 0xffu ) << ( component * 8u );
        }
    }

    visibility_packed[z_idx * (uint)words_per_level + word_in_level] = packed;
}
