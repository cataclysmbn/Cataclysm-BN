#pragma once

#include <optional>

#include "coordinates.h"
#include "vpart_position.h"

class mapbuffer;

struct vehicle_grab_target {
    tripoint_abs_ms pos;
    vpart_position vp;
};

auto vehicle_grab_target_at( mapbuffer &here,
                             const tripoint_abs_ms &pos ) -> std::optional<vehicle_grab_target>;
