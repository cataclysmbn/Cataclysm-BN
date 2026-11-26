#pragma once

#include "calendar.h"
#include "cata_utility.h"
#include "state_helpers.h"
#include "weather/weather.h"

/// Clean world @p since the start of the cataclysm at 18 C; restores time and weather on exit.
struct rot_test_state {
    restore_on_out_of_scope<time_point> time{calendar::turn};
    restore_on_out_of_scope<units::temperature> temperature{get_weather().temperature};

    explicit rot_test_state(time_duration since = 91_days) {
        clear_all_state();
        calendar::turn = calendar::start_of_cataclysm + since;
        get_weather().temperature = 18_c;
        get_weather().clear_temp_cache();
    }
    ~rot_test_state() {
        clear_all_state();
        get_weather().clear_temp_cache();
    }
};
