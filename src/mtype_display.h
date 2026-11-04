#pragma once

#include "color.h"

#include <string>
#include <vector>

struct mtype;

/// One line of what a creature entry of the faction screen shows about a monster type.
struct mtype_display_line {
    std::string text;
    nc_color color;
    int indent = 0;
};

/// The name, difficulty, origin, size, species, senses, abilities and description of a monster type, with the
/// origin and species lines folded to `width`. A blank line comes before the description.
auto mtype_display_lines( const mtype &type, int width ) -> std::vector<mtype_display_line>;
