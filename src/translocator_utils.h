#pragma once

#include "coordinates.h"

namespace translocator
{

auto local_dest( const tripoint_bub_ms &omt_local_dest,
                 const point_bub_ms &bubble_center ) -> tripoint_bub_ms;

} // namespace translocator
