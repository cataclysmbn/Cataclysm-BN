#pragma once

#include "calendar.h"
#include "type_id.h"

enum class water_source_poison {
    none,
    sewage,
    natural,
};

/// Resource availability, not a sampled drink. Poison is sampled only when taking water.
struct water_source {
    itype_id type;
    time_point birthday = calendar::start_of_cataclysm;
    water_source_poison poison = water_source_poison::none;
};
