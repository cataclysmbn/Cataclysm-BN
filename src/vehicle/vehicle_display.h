#pragma once

#include "type_id.h"
#include "units.h"

#include <string>

/// A resolved physical part appearance, shared by world drawing, previews and memory.
struct vpart_display {
    vpart_id id = vpart_id::NULL_ID();
    std::string variant;
    units::angle direction = 0_degrees;
    bool is_open = false;
    bool is_broken = false;

    auto get_tileset_id() const -> std::string;
};
