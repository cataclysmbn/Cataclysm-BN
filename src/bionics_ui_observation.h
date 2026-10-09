#pragma once

#include <cstddef>

namespace bionics_ui
{

/// Actual detailed-description production, including work before the first semantic request.
/// Game-thread diagnostics only; these counters are not part of the external contract.
struct description_work_counts {
    std::size_t semantic = 0;
    std::size_t native = 0;
};
auto description_work() -> description_work_counts;
auto reset_description_work() -> void;

} // namespace bionics_ui
