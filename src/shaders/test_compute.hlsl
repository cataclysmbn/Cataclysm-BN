// Minimal compute shader for shader toolchain and runtime dispatch validation.
RWStructuredBuffer<uint> probe_out : register(u0, space1);

[numthreads(1, 1, 1)]
void main(uint3 dispatch_id : SV_DispatchThreadID)
{
    probe_out[0] = 0xC0DECAFEu;
}
