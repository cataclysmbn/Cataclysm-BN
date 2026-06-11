// GPU implementation of build_transparency_cache / rebuild_transparency_cache.

static const float LIGHT_TRANSPARENCY_OPEN_AIR = 0.038376418216;
static const float LIGHT_TRANSPARENCY_SOLID    = 0.0;
static const uint  SEEY                        = 12;

struct TransparencySubmapIn
{
    uint  ter_ids[144];
    uint  furn_ids[144];
    float field_opacity[144];
    uint  outside_flags[144];
    int   cache_offset_x;
    int   cache_offset_y;
    uint  output_offset;
};

[[vk::binding(0, 2)]]
cbuffer Constants : register(b0, space2)
{
    float sight_penalty;
    int   cache_y;
    uint  num_submaps;
    uint  output_offset;
};

[[vk::binding(0, 0)]]
StructuredBuffer<TransparencySubmapIn> submap_in : register(t0, space0);

[[vk::binding(1, 0)]]
StructuredBuffer<uint> ter_lut : register(t1, space0);

[[vk::binding(2, 0)]]
StructuredBuffer<uint> furn_lut : register(t2, space0);

[[vk::binding(0, 1)]]
RWStructuredBuffer<float> compact_transparency_out : register(u0, space1);

[[vk::binding(1, 1)]]
RWStructuredBuffer<float> full_transparency_out : register(u1, space1);

void transparency_main_impl( uint3 group_id, uint3 group_thread_id )
{
    if( group_id.x >= num_submaps ) {
        return;
    }

    uint sx = group_thread_id.x;
    uint sy = group_thread_id.y;
    uint tile = sx * SEEY + sy;

    TransparencySubmapIn sm = submap_in[group_id.x];

    bool ter_transparent = ter_lut[sm.ter_ids[tile]] != 0u;
    bool furn_transparent = furn_lut[sm.furn_ids[tile]] != 0u;

    float value;
    if( ter_transparent || !furn_transparent ) {
        value = LIGHT_TRANSPARENCY_OPEN_AIR;
        if( sm.outside_flags[tile] != 0u ) {
            value *= sight_penalty;
        }
        value *= sm.field_opacity[tile];
    } else {
        value = LIGHT_TRANSPARENCY_SOLID;
    }

    int cx = sm.cache_offset_x + int( sx );
    int cy = sm.cache_offset_y + int( sy );
    compact_transparency_out[group_id.x * 144u + tile] = value;
    full_transparency_out[output_offset + sm.output_offset + uint( cx * cache_y + cy )] = value;
}

[numthreads(12, 12, 1)]
void main( uint3 group_id : SV_GroupID, uint3 group_thread_id : SV_GroupThreadID )
{
    transparency_main_impl( group_id, group_thread_id );
}

[numthreads(12, 12, 1)]
void cpu_main( uint3 group_id : SV_GroupID, uint3 group_thread_id : SV_GroupThreadID )
{
    transparency_main_impl( group_id, group_thread_id );
}
