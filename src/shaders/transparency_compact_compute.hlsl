// DXBC-compatible compact transparency pass.
// One workgroup per submap; [numthreads(12, 12, 1)] maps one thread to one tile.
// This shader deliberately avoids large StructuredBuffer strides, multiple SRVs, and a second UAV.

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

struct CompactTransparencyIn
{
    uint  ter_transparent;
    uint  furn_transparent;
    float field_opacity;
    uint  outside_flag;
};

StructuredBuffer<CompactTransparencyIn> transparency_in : register(t0, space0);

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

    CompactTransparencyIn input = transparency_in[idx];
    bool ter_transparent = input.ter_transparent != 0;
    bool furn_transparent = input.furn_transparent != 0;

    float value;
    if (ter_transparent || !furn_transparent) {
        value = LIGHT_TRANSPARENCY_OPEN_AIR;
        if (input.outside_flag != 0) {
            value *= sight_penalty;
        }
        value *= input.field_opacity;
    } else {
        value = LIGHT_TRANSPARENCY_SOLID;
    }

    compact_transparency_out[idx] = value;
}
