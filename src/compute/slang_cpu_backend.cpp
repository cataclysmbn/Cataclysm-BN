#include "slang_cpu_backend.h"

namespace cata_compute::slang_cpu
{

namespace
{

auto s_initialized = false;

} // namespace

auto init() -> void
{
    s_initialized = true;
}

auto shutdown() -> void
{
    s_initialized = false;
}

auto status() -> backend_status
{
    return {
        .kind = backend_kind::slang_cpu,
        .available = false,
        .supports_lighting = false,
        .supports_visibility = false,
        .supports_transparency = false,
        .supports_sight_pairs = false,
        .name = "slang_cpu",
        .detail = s_initialized ? "Slang CPU backend has no generated kernels"
                  : "Slang CPU backend is not initialized",
    };
}

} // namespace cata_compute::slang_cpu
