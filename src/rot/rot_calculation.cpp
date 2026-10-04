#include "rot/rot_calculation.h"

#include "weather/weather.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>

namespace {

/// Hardcoded lookup table for food rots per hour calculation.
///
/// IRL this tends to double every 10c a few degrees above freezing, but past a certain
/// point the rate decreases until even extremophiles find it too hot. Here we just stop
/// further acceleration at 40C.
///
/// Original formula:
/// @see
/// https://github.com/cataclysmbn/Cataclysm-BN/blob/033901af4b52ad0bfcfd6abfe06bca4e403d44b1/src/item.cpp#L5612-L5640
constexpr auto rot_chart = std::array<int, 44>{
    0,    372,  744,  1118, 1219,  1273,  1388,  1514,  1651,  1800,  1880,  2050,  2235,  2438,
    2658, 2776, 3027, 3301, 3600,  3926,  4100,  4471,  4875,  5317,  5798,  6054,  6602,  7200,
    7852, 8562, 8941, 9751, 10633, 11595, 12645, 13205, 14400, 15703, 17125, 18674, 19501,
};

} // namespace

/// Get the hourly rot for a given temperature from the precomputed table.
/// @see rot_chart
auto get_hourly_rotpoints_at_temp(const units::temperature temp) -> int {
    if (temp < temperatures::freezing) { return 0; }
    if (temp > 40_c) { return 21240; }
    // HACK: due to frequent fahrenheit <-> celsius conversion, 18C is actually 17.777C
    // remove rounding after most of temperatures passed around are in units::temperature
    const auto temp_c = static_cast<float>(units::to_millidegree_celsius(temp)) / 1000;
    return rot_chart[std::round(temp_c)];
}

namespace rot {

shelf_life::shelf_life(time_duration value): value_(value) {}

auto shelf_life::from_duration(time_duration value) -> std::optional<shelf_life> {
    if (value <= 0_turns) { return std::nullopt; }
    return shelf_life(value);
}

auto shelf_life::duration() const -> time_duration { return value_; }

auto shelf_life::relative_rot(time_duration accumulated) const -> double {
    return accumulated / value_;
}

auto shelf_life::from_relative(double fraction) const
    -> std::expected<time_duration, relative_rot_error> {
    if (!std::isfinite(fraction)) { return std::unexpected(relative_rot_error::non_finite); }
    const auto scale = to_turns<double>(value_);
    if (fraction < std::numeric_limits<int>::min() / scale
        || fraction > std::numeric_limits<int>::max() / scale) {
        return std::unexpected(relative_rot_error::out_of_range);
    }
    return time_duration::from_turns(static_cast<int>(scale * fraction));
}

auto shelf_life::minimum_freshness(time_duration accumulated, units::temperature temperature) const
    -> time_duration {
    const auto rate = get_hourly_rotpoints_at_temp(temperature);
    if (rate == 0) { return calendar::INDEFINITELY_LONG_DURATION; }
    const auto remaining = to_turns<int64_t>(value_) - to_turns<int64_t>(accumulated);
    if (remaining <= 0) { return 0_turns; }
    const auto turns = remaining * to_turns<int64_t>(1_hours) / rate;
    return time_duration::from_turns(
        static_cast<int>(std::min(turns, to_turns<int64_t>(calendar::INDEFINITELY_LONG_DURATION))));
}

auto increment(const interval_options& options) -> time_duration {
    // Preserve truncation after field dressing and again after applying the rate.
    // Widen intermediates so extreme explicit inputs cannot overflow time_duration.
    const auto factor = options.field_dressed_corpse ? 0.75 : 1.0;
    const auto adjusted = static_cast<int64_t>(to_turns<double>(options.elapsed) * factor);
    const auto added = static_cast<int64_t>(
        static_cast<double>(adjusted) / to_turns<int>(1_hours)
        * get_hourly_rotpoints_at_temp(options.temperature));
    const auto total = added + to_turns<int64_t>(options.initial_variation);
    return time_duration::from_turns(static_cast<int>(std::clamp<int64_t>(
        total, std::numeric_limits<int>::min(), std::numeric_limits<int>::max())));
}

auto accumulate(time_duration current, time_duration added) -> time_duration {
    const auto total = to_turns<int64_t>(current) + to_turns<int64_t>(added);
    return time_duration::from_turns(static_cast<int>(std::clamp<int64_t>(
        total, std::numeric_limits<int>::min(), std::numeric_limits<int>::max())));
}

} // namespace rot
