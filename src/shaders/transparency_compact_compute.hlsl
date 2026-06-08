// DXBC-compatible compact transparency pass.
// One workgroup per submap; [numthreads(12, 12, 1)] maps one thread to one tile.
// This shader deliberately avoids large StructuredBuffer strides, SRVs, and uniform buffers.

static const uint SEEY         = 12;
static const uint SUBMAP_TILES = 144;

RWStructuredBuffer<float> transparency_in         : register(u0, space1);
RWStructuredBuffer<float> compact_transparency_out : register(u1, space1);

[numthreads(12, 12, 1)]
void main(uint3 group_id : SV_GroupID, uint3 thread_id : SV_GroupThreadID)
{
    uint sx = thread_id.x;
    uint sy = thread_id.y;
    uint tile = sx * SEEY + sy;
    uint idx = group_id.x * SUBMAP_TILES + tile;

    compact_transparency_out[idx] = transparency_in[idx];
}
