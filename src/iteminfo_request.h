#pragma once

#include "enums.h"
#include "iteminfo_query.h"

/// Inspection retains intentional snippet-read effects; observation only describes the item.
enum class iteminfo_mode {
    inspection,
    observation,
};

struct iteminfo_request {
    const iteminfo_query &parts = iteminfo_query::all;
    int batch = 1;
    temperature_flag temperature = temperature_flag::TEMP_NORMAL;
    iteminfo_mode mode = iteminfo_mode::inspection;

    auto observing() const -> bool { return mode == iteminfo_mode::observation; }
};
