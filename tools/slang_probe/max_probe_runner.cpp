#include <array>
#include <cstdint>
#include <iostream>

#include "max_probe.cpp"

auto main() -> int
{
    auto input_values = std::array<uint32_t, 8>{17U, 3U, 99U, 41U, 12U, 74U, 8U, 55U};
    auto output_values = std::array<uint32_t, 1>{0U};

    auto constants = MaxProbeConstants_0{
        .input_count_0 = static_cast<uint32_t>( input_values.size() ),
        .output_index_0 = 0U,
        ._pad0_0 = 0U,
        ._pad1_0 = 0U,
    };

    auto globals = GlobalParams_0{
        .input_values_0 = StructuredBuffer<uint32_t>{
            .data = input_values.data(),
            .count = input_values.size(),
        },
        .output_values_0 = RWStructuredBuffer<uint32_t>{
            .data = output_values.data(),
            .count = output_values.size(),
        },
        .constants_0 = &constants,
    };

    auto varying = ComputeVaryingInput{
        .startGroupID = uint3( 0U, 0U, 0U ),
        .endGroupID = uint3( 1U, 1U, 1U ),
    };

    max_probe_main( &varying, nullptr, &globals );

    if( output_values[0] != 99U ) {
        std::cerr << "Slang max probe FAILED: " << output_values[0] << '\n';
        return 1;
    }

    std::cout << "Slang max probe OK: " << output_values[0] << '\n';
    return 0;
}
