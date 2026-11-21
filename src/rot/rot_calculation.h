#pragma once

#include "calendar.h"
#include "units_temperature.h"

#include <cstdint>
#include <expected>
#include <optional>

namespace rot {

enum class relative_rot_error { non_finite, out_of_range };

/// Shelf life of an item that rots. Always positive.
class shelf_life {
    time_duration value_;
    explicit shelf_life(time_duration value);

public:
    static auto from_duration(time_duration value) -> std::optional<shelf_life>;
    auto duration() const -> time_duration;
    auto relative_rot(time_duration accumulated) const -> double;
    /// Returns an error for NaN, infinity, or a value out of range.
    auto from_relative(double fraction) const -> std::expected<time_duration, relative_rot_error>;
    auto minimum_freshness(
        time_duration accumulated, units::temperature temperature, float rate = 1.0f) const
        -> time_duration;
};

struct interval_options {
    time_duration elapsed;
    units::temperature temperature;
    /// The world's ROT_RATE option.
    float rate = 1.0f;
    bool field_dressed_corpse = false;
    time_duration initial_variation = 0_turns;
};

/// Real time for @p remaining turns of rot to accumulate; INDEFINITELY_LONG_DURATION if never.
auto time_to_rot(int64_t remaining, units::temperature temperature, float rate = 1.0f)
    -> time_duration;
/// Rot gained during one interval at one temperature. Reads no game state.
auto increment(const interval_options& options) -> time_duration;
/// Adds rot, clamped to the range of time_duration.
auto accumulate(time_duration current, time_duration added) -> time_duration;

} // namespace rot
