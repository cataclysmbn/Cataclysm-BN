#include "slang_cpu_kernels.h"

#if defined(CATA_SLANG_CPU_GENERATED)
#define cpu_main cata_slang_test_compute_cpu_main
#define _cpu_main cata_slang_test_compute_cpu_main_entry
#define cpu_main_Group cata_slang_test_compute_cpu_main_group
#define cpu_main_Thread cata_slang_test_compute_cpu_main_thread
#include "test_compute.cpp"
#undef cpu_main_Thread
#undef cpu_main_Group
#undef _cpu_main
#undef cpu_main
#endif

namespace cata_compute::slang_cpu::kernels {

auto run_test_compute_probe() -> bool {
#if defined(CATA_SLANG_CPU_GENERATED)
    auto varying = ComputeVaryingInput{
        .startGroupID = uint3(0U, 0U, 0U),
        .endGroupID = uint3(1U, 1U, 1U),
    };
    cata_slang_test_compute_cpu_main(&varying, nullptr, nullptr);
    return true;
#else
    return false;
#endif
}

} // namespace cata_compute::slang_cpu::kernels
