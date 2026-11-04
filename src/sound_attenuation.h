#pragma once

#include "coordinates.h"

struct sound_attenuation_options {
    tripoint_bub_ms source;
    tripoint_bub_ms listener;
    bool line_of_sight = false;
    bool for_horde_signal = false;
};

auto vol_z_adjust( const sound_attenuation_options &options ) -> short;
