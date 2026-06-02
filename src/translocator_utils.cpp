#include "translocator_utils.h"

namespace translocator
{

auto local_dest( const tripoint_bub_ms &omt_local_dest,
                 const point_bub_ms & ) -> tripoint_bub_ms
{
    return omt_local_dest + point( 60, 60 );
}

} // namespace translocator
