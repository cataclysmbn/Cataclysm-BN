struct MaxProbeConstants
{
    uint input_count;
    uint output_index;
    uint _pad0;
    uint _pad1;
};

[[vk::binding(0, 0)]]
StructuredBuffer<uint> input_values : register(t0, space0);

[[vk::binding(0, 1)]]
RWStructuredBuffer<uint> output_values : register(u0, space1);

[[vk::binding(0, 2)]]
ConstantBuffer<MaxProbeConstants> constants : register(b0, space2);

void cataMaxStore(uint idx, uint value)
{
#if defined(CATA_SLANG_CPU)
    uint previous = output_values[idx];
    if(value > previous)
    {
        output_values[idx] = value;
    }
#else
    InterlockedMax(output_values[idx], value);
#endif
}

[numthreads(64, 1, 1)]
void max_probe_main(uint3 dispatch_id : SV_DispatchThreadID)
{
    uint idx = dispatch_id.x;
    if(idx >= constants.input_count)
    {
        return;
    }

    cataMaxStore(constants.output_index, input_values[idx]);
}
