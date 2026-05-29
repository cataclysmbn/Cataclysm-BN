#pragma once

#include "calendar.h"

namespace activity_time_cadence
{

inline constexpr auto fixed_window() -> time_duration
{
    return 5_minutes;
}

inline constexpr auto weather_refresh() -> time_duration
{
    return fixed_window();
}

inline constexpr auto activity_render_refresh() -> time_duration
{
    return fixed_window();
}

inline constexpr auto cache_refresh() -> time_duration
{
    return fixed_window();
}

} // namespace activity_time_cadence
