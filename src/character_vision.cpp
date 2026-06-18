#include "character_vision.h"

#include "bionics.h"
#include "character.h"
#include "flag.h"
#include "monster.h"
#include "mtype.h"

static const bionic_id bio_night_vision( "bio_night_vision" );

namespace
{

auto has_mounted_recon_vision( const Character &who ) -> bool
{
    return who.is_mounted() && who.mounted_creature->has_flag( MF_MECH_RECON_VISION );
}

} // namespace

namespace character_vision
{

auto active_night_vision_bonus_level( const Character &who ) -> night_vision_bonus_level
{
    if( who.worn_with_flag( flag_GNVE_EFFECT ) ) {
        return night_vision_bonus_level::enhanced;
    }
    if( who.worn_with_flag( flag_RECON_VISION ) || has_mounted_recon_vision( who ) ||
        who.has_active_bionic( bio_night_vision ) ||
        who.has_effect_with_flag( flag_EFFECT_NIGHT_VISION ) ) {
        return night_vision_bonus_level::standard;
    }
    if( who.worn_with_flag( flag_GNV_EFFECT ) ) {
        return night_vision_bonus_level::standard_goggles;
    }
    return night_vision_bonus_level::none;
}

auto sight_range_bonus( const night_vision_bonus_level level ) -> float
{
    switch( level ) {
        case night_vision_bonus_level::enhanced:
            return 18.0f;
        case night_vision_bonus_level::standard:
        case night_vision_bonus_level::standard_goggles:
            return 10.0f;
        case night_vision_bonus_level::none:
            return 0.0f;
    }
    return 0.0f;
}

} // namespace character_vision
