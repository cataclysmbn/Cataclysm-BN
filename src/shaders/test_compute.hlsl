// Minimal compute shader for shader toolchain and runtime dispatch validation.
RWStructuredBuffer<uint> probe_in  : register(u0, space1);
RWStructuredBuffer<uint> probe_out : register(u1, space1);

[numthreads(12, 12, 1)]
void main(uint3 dispatch_id : SV_DispatchThreadID)
{
    uint idx = dispatch_id.x * 12u + dispatch_id.y;
    probe_out[idx] = probe_in[idx] ^ 0xC0DECAFEu;
}
