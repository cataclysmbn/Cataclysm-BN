// GPU implementation of build_transparency_cache / rebuild_transparency_cache.
// One workgroup per submap; [numthreads(12, 12, 1)] maps one thread to one tile.
//   SV_GroupID.x       = submap index into the split submap input buffers
//   SV_GroupThreadID.x = sx (0..11)
//   SV_GroupThreadID.y = sy (0..11)
//
// Binding layout (SDL3 GPU / shadercross HLSL conventions):
//   space0  read-only storage buffers  (t registers)
//   space1  read-write storage buffers (u registers)
//   space2  uniform / cbuffer          (b registers)

static const float LIGHT_TRANSPARENCY_OPEN_AIR = 0.038376418216;
static const float LIGHT_TRANSPARENCY_SOLID    = 0.0;
static const uint  SEEY                        = 12;

// Split so each StructuredBuffer stride stays inside legacy D3D limits.
struct TransparencySubmapIdsIn
{
    uint  ter_ids[144];
    uint  furn_ids[144];
    uint  outside_flags[144];
};

struct TransparencySubmapMetaIn
{
    float field_opacity[144];
    int   cache_offset_x;
    int   cache_offset_y;
    uint  output_offset;
    uint  padding;
};

cbuffer Constants : register(b0, space2)
{
    float sight_penalty; // weather penalty; 1.0 = clear sky
    int   cache_y;       // flat level-cache y-stride (= SEEY * mapsize)
    uint  num_submaps;   // number of entries in the submap input buffers
    uint  output_offset; // float elements from the start of full_transparency_out
};

StructuredBuffer<TransparencySubmapIdsIn>  submap_ids  : register(t0, space0);
StructuredBuffer<TransparencySubmapMetaIn> submap_meta : register(t1, space0);
StructuredBuffer<uint>                     ter_lut     : register(t2, space0);
StructuredBuffer<uint>                     furn_lut    : register(t3, space0);

RWStructuredBuffer<float> compact_transparency_out : register(u0, space1);
RWStructuredBuffer<float> full_transparency_out    : register(u1, space1);

[numthreads(12, 12, 1)]
void main(uint3 group_id : SV_GroupID, uint3 thread_id : SV_GroupThreadID)
{
    if (group_id.x >= num_submaps) {
        return;
    }

    uint sx   = thread_id.x;
    uint sy   = thread_id.y;
    uint tile = sx * SEEY + sy;

    TransparencySubmapIdsIn ids = submap_ids[group_id.x];
    TransparencySubmapMetaIn meta = submap_meta[group_id.x];

    // Mirrors: if( ter.transparent || !furn.transparent ) from rebuild_transparency_cache.
    bool ter_transparent  = ter_lut[ids.ter_ids[tile]]   != 0;
    bool furn_transparent = furn_lut[ids.furn_ids[tile]] != 0;

    float value;
    if (ter_transparent || !furn_transparent) {
        value = LIGHT_TRANSPARENCY_OPEN_AIR;
        if (ids.outside_flags[tile] != 0) {
            value *= sight_penalty;
        }
        value *= meta.field_opacity[tile];
    } else {
        value = LIGHT_TRANSPARENCY_SOLID;
    }

    int cx = meta.cache_offset_x + (int)sx;
    int cy = meta.cache_offset_y + (int)sy;
    compact_transparency_out[group_id.x * 144 + tile] = value;
    full_transparency_out[output_offset + meta.output_offset + cx * cache_y + cy] = value;
}
