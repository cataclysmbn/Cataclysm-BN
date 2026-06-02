#include "translocator_utils.h"

namespace translocator
{

auto local_dest( const tripoint_bub_ms &omt_local_dest,
                 const point_bub_ms &bubble_center ) -> tripoint_bub_ms
{
    return tripoint_bub_ms( omt_local_dest.raw() + bubble_center.raw() );
}

} // namespace translocator
