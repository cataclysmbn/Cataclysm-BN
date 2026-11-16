#include "rot/rot_calculation.h"

#include "weather/weather.h"

#include <array>
#include <cmath>

namespace {

/**
 * Hardcoded lookup table for food rots per hour calculation.
 *
 * IRL this tends to double every 10c a few degrees above freezing, but past a certain
 * point the rate decreases until even extremophiles find it too hot. Here we just stop
 * further acceleration at 40C.
 *
 * Original formula:
 * @see
 * https://github.com/cataclysmbn/Cataclysm-BN/blob/033901af4b52ad0bfcfd6abfe06bca4e403d44b1/src/item.cpp#L5612-L5640
 */
constexpr auto rot_chart = std::array<int, 44>{
    0,    372,  744,  1118, 1219,  1273,  1388,  1514,  1651,  1800,  1880,  2050,  2235,  2438,
    2658, 2776, 3027, 3301, 3600,  3926,  4100,  4471,  4875,  5317,  5798,  6054,  6602,  7200,
    7852, 8562, 8941, 9751, 10633, 11595, 12645, 13205, 14400, 15703, 17125, 18674, 19501,
};

} // namespace

/**
 * Get the hourly rot for a given temperature from the precomputed table.
 * @see rot_chart
 */
auto get_hourly_rotpoints_at_temp(const units::temperature temp) -> int {
    /**
     * Precomputed rot lookup table.
     */
    if (temp < temperatures::freezing) { return 0; }
    if (temp > 40_c) { return 21240; }
    // HACK: due to frequent fahrenheit <-> celsius conversion, 18C is actually 17.777C
    // remove rounding after most of temperatures passed around are in `units::temperature`
    const auto temp_c = static_cast<float>(units::to_millidegree_celsius(temp)) / 1000;
    return rot_chart[std::round(temp_c)];
}

namespace rot {

/// Real time for @p remaining_rot to accumulate at @p temp; INDEFINITELY_LONG_DURATION if it never
/// does.
auto time_to_rot(const time_duration remaining_rot, const units::temperature temp)
    -> time_duration {
    if (remaining_rot <= 0_turns) { return 0_turns; }
    const auto rot_per_hour = get_hourly_rotpoints_at_temp(temp);
    if (rot_per_hour <= 0) { return calendar::INDEFINITELY_LONG_DURATION; }
    // Rot is counted in room temperature turns, so scale it by the rot speed at temp.
    const auto turns = to_turns<double>(remaining_rot) * to_turns<double>(1_hours) / rot_per_hour;
    return turns < to_turns<double>(calendar::INDEFINITELY_LONG_DURATION)
             ? time_duration::from_turns(static_cast<int>(turns))
             : calendar::INDEFINITELY_LONG_DURATION;
}

} // namespace rot
