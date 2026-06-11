#pragma once

#include "compute_backend.h"

namespace cata_compute::slang_cpu
{

auto init() -> void;
auto shutdown() -> void;
auto status() -> backend_status;

} // namespace cata_compute::slang_cpu
