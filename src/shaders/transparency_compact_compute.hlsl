// DXBC-compatible compact transparency pass.
// One workgroup per submap; [numthreads(12, 12, 1)] maps one thread to one tile.
// This shader deliberately avoids large StructuredBuffer strides and a second UAV.

static const float LIGHT_TRANSPARENCY_OPEN_AIR = 0.038376418216;
static const float LIGHT_TRANSPARENCY_SOLID    = 0.0;
static const uint  SEEY                        = 12;
static const uint  SUBMAP_TILES                = 144;

cbuffer Constants : register(b0, space2)
{
    float sight_penalty;
    int   cache_y;
    uint  num_submaps;
    uint  output_offset;
};

StructuredBuffer<uint>  ter_ids       : register(t0, space0);
StructuredBuffer<uint>  furn_ids      : register(t1, space0);
StructuredBuffer<float> field_opacity : register(t2, space0);
StructuredBuffer<uint>  outside_flags : register(t3, space0);

RWStructuredBuffer<float> compact_transparency_out : register(u0, space1);

[numthreads(12, 12, 1)]
void main(uint3 group_id : SV_GroupID, uint3 thread_id : SV_GroupThreadID)
{
    if (group_id.x >= num_submaps) {
        return;
    }

    uint sx = thread_id.x;
    uint sy = thread_id.y;
    uint tile = sx * SEEY + sy;
    uint idx = group_id.x * SUBMAP_TILES + tile;

    bool ter_transparent = ter_ids[idx] != 0;
    bool furn_transparent = furn_ids[idx] != 0;

    float value;
    if (ter_transparent || !furn_transparent) {
        value = LIGHT_TRANSPARENCY_OPEN_AIR;
        if (outside_flags[idx] != 0) {
            value *= sight_penalty;
        }
        value *= field_opacity[idx];
    } else {
        value = LIGHT_TRANSPARENCY_SOLID;
    }

    compact_transparency_out[idx] = value;
}
