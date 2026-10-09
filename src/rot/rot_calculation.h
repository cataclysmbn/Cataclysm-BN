#pragma once

#include "calendar.h"
#include "units_temperature.h"

#include <expected>
#include <optional>

namespace rot {

enum class relative_rot_error { non_finite, out_of_range };

/// A spoilage denominator that cannot represent a nonperishable item.
class shelf_life {
    time_duration value_;
    explicit shelf_life(time_duration value);

public:
    static auto from_duration(time_duration value) -> std::optional<shelf_life>;
    auto duration() const -> time_duration;
    auto relative_rot(time_duration accumulated) const -> double;
    /// Reject non-finite or out-of-range edits without changing stored state.
    auto from_relative(double fraction) const -> std::expected<time_duration, relative_rot_error>;
    auto minimum_freshness(time_duration accumulated, units::temperature temperature) const
        -> time_duration;
};

struct interval_options {
    time_duration elapsed;
    units::temperature temperature;
    bool field_dressed_corpse = false;
    time_duration initial_variation = 0_turns;
};

/// Integrate one temperature interval without consulting the world, clock, or RNG.
auto increment(const interval_options& options) -> time_duration;
/// Saturating addition keeps explicit edits and long-lived food representable.
auto accumulate(time_duration current, time_duration added) -> time_duration;

} // namespace rot
