#pragma once

#include <optional>

#include "coordinates.h"
#include "vpart_position.h"

class map;

struct vehicle_grab_target {
    tripoint_bub_ms pos;
    vpart_position vp;
};

auto vehicle_grab_target_at( const map &here,
                             const tripoint_bub_ms &pos ) -> std::optional<vehicle_grab_target>;
