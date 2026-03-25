#include "procgen/proc_types.h"

#include <cstdlib>
#include <string>

#include "debug.h"
#include "enum_conversions.h"

namespace io
{

template<>
std::string enum_to_string<proc::hist>( proc::hist data )
{
    switch( data ) {
        // *INDENT-OFF*
        case proc::hist::none: return "none";
        case proc::hist::compact: return "compact";
        case proc::hist::full: return "full";
        // *INDENT-ON*
        case proc::hist::num_hist:
            break;
    }

    debugmsg( "Invalid proc::hist" );
    std::abort();
}

} // namespace io
