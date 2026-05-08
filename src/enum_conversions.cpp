///
/// A place for enum conversions which don't fit anywhere else
///
#include "enum_conversions.h"

#include "calendar.h"
#include "enums.h"

template<>
std::string io::enum_to_string<Attitude>( Attitude att )
{
    switch( att ) {
        case Attitude::A_HOSTILE:
            return "Hostile";
        case Attitude::A_NEUTRAL:
            return "Neutral";
        case Attitude::A_FRIENDLY:
            return "Friendly";
        case Attitude::A_ANY:
            return "Any";
        case Attitude::NUM_A:
            break;
    }
    debugmsg( "Invalid Attitude" );
    abort();
}

template<>
std::string io::enum_to_string<creature_size>( creature_size data )
{
    switch( data ) {
        case creature_size::tiny:
            return "TINY";
        case creature_size::small:
            return "SMALL";
        case creature_size::medium:
            return "MEDIUM";
        case creature_size::large:
            return "LARGE";
        case creature_size::huge:
            return "HUGE";
        case creature_size::num_creature_size:
            break;
    }
    debugmsg( "Invalid body_size value: %d", static_cast<int>( data ) );
    abort();
}

template<>
std::string io::enum_to_string<moon_phase>( moon_phase data )
{
    switch( data ) {
        case moon_phase::MOON_NEW:
            return "MOON_NEW";
        case moon_phase::MOON_WAXING_CRESCENT:
            return "MOON_WAXING_CRESCENT";
        case moon_phase::MOON_HALF_MOON_WAXING:
            return "MOON_HALF_MOON_WAXING";
        case moon_phase::MOON_WAXING_GIBBOUS:
            return "MOON_WAXING_GIBBOUS";
        case moon_phase::MOON_FULL:
            return "MOON_FULL";
        case moon_phase::MOON_WANING_GIBBOUS:
            return "MOON_WANING_GIBBOUS";
        case moon_phase::MOON_HALF_MOON_WANING:
            return "MOON_HALF_MOON_WANING";
        case moon_phase::MOON_WANING_CRESCENT:
            return "MOON_WANING_CRESCENT";
        case moon_phase::MOON_PHASE_MAX:
            break;
    }
    debugmsg( "Invalid moon_phase %d", static_cast<int>( data ) );
    abort();
}

