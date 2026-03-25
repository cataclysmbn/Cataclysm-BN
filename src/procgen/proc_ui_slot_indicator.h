#pragma once

#include <string>
#include <vector>

#include "color.h"
#include "procgen/proc_schema.h"

namespace proc
{

struct slot_indicator_cell {
    char glyph;
    nc_color color;
};

auto slot_indicator_cells( const slot_data &slot, int picked,
                           bool selected ) -> std::vector<slot_indicator_cell>;
auto slot_indicator( const slot_data &slot, int picked ) -> std::string;

} // namespace proc
