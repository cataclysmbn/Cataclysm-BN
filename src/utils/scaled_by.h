#pragma once

#include <cstdint>

namespace utils {

struct scaled_by_options {
    int total;
    int cur;
    int max;
};

/// Returns `total * cur / max`, multiplied in 64 bits so `total * cur` cannot overflow `int`.
inline auto scaled_by(const scaled_by_options& opts) -> int {
    return static_cast<int>(static_cast<int64_t>(opts.total) * opts.cur / opts.max);
}

} // namespace utils
