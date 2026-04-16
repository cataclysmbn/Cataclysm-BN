#pragma once
#ifndef CATA_SRC_REALITY_BUBBLE_HELPERS_H
#define CATA_SRC_REALITY_BUBBLE_HELPERS_H

#include "point.h"

namespace reality_bubble
{

inline auto local_square_from_global( const tripoint &global_square,
                                      const tripoint &new_origin_ms ) -> tripoint
{
    return tripoint( global_square.x - new_origin_ms.x,
                     global_square.y - new_origin_ms.y,
                     global_square.z );
}

} // namespace reality_bubble

#endif // CATA_SRC_REALITY_BUBBLE_HELPERS_H
