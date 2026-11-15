#pragma once

namespace calendar {

/// Counter that `calendar::invalidate_time_config_cache()` bumps when an option value may change.
/// Caches of option values compare it to decide when to read the options again.
auto option_generation() -> int;

} // namespace calendar
