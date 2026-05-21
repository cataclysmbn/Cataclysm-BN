// GPU implementation of build_transparency_cache / rebuild_transparency_cache.
// One workgroup per submap; [numthreads(12, 12, 1)] maps one thread to one tile.
//   SV_GroupID.x       = submap index into submap_in[]
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

// Must match transparency_submap_in in gpu_transparency.h exactly.
// Struct stride = 2312 bytes.
struct TransparencySubmapIn
{
    uint  ter_ids[144];
    uint  furn_ids[144];
    float field_opacity[144];
    uint  outside_flags[144];
    int   cache_offset_x;
    int   cache_offset_y;
};

cbuffer Constants : register(b0, space2)
{
    float sight_penalty; // weather penalty; 1.0 = clear sky
    int   cache_y;       // flat level-cache y-stride (= SEEY * mapsize)
    uint  num_submaps;   // number of entries in submap_in
    uint  _pad;          // std140 padding
};

StructuredBuffer<TransparencySubmapIn> submap_in   : register(t0, space0);
StructuredBuffer<uint>                 ter_lut     : register(t1, space0);
StructuredBuffer<uint>                 furn_lut    : register(t2, space0);

RWStructuredBuffer<float> transparency_out : register(u0, space1);

[numthreads(12, 12, 1)]
void main(uint3 group_id : SV_GroupID, uint3 thread_id : SV_GroupThreadID)
{
    if (group_id.x >= num_submaps) {
        return;
    }

    uint sx   = thread_id.x;
    uint sy   = thread_id.y;
    uint tile = sx * SEEY + sy;

    TransparencySubmapIn sm = submap_in[group_id.x];

    // Mirrors: if( ter.transparent || !furn.transparent ) from rebuild_transparency_cache.
    bool ter_transparent  = ter_lut[sm.ter_ids[tile]]   != 0;
    bool furn_transparent = furn_lut[sm.furn_ids[tile]] != 0;

    float value;
    if (ter_transparent || !furn_transparent) {
        value = LIGHT_TRANSPARENCY_OPEN_AIR;
        if (sm.outside_flags[tile] != 0) {
            value *= sight_penalty;
        }
        value *= sm.field_opacity[tile];
    } else {
        value = LIGHT_TRANSPARENCY_SOLID;
    }

    int cx = sm.cache_offset_x + (int)sx;
    int cy = sm.cache_offset_y + (int)sy;
    transparency_out[cx * cache_y + cy] = value;
}
