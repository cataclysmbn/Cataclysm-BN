#include "artifact.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <iosfwd>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <vector>

#include "assign.h"
#include "bodypart.h"
#include "cata_utility.h"
#include "color.h"
#include "damage.h"
#include "debug.h"
#include "fstream_utils.h"
#include "item_factory.h"
#include "iuse.h"
#include "json.h"
#include "rng.h"
#include "string_formatter.h"
#include "translations.h"
#include "type_id.h"
#include "units.h"
#include "value_ptr.h"
#include "world.h"

namespace io
{
#define PAIR(x) case x: return #x;
template<>
std::string enum_to_string<art_effect_passive>( art_effect_passive data )
{
    switch( data ) {
        // *INDENT-OFF*
        PAIR( AEP_NULL )
        PAIR( AEP_STR_UP )
        PAIR( AEP_DEX_UP )
        PAIR( AEP_PER_UP )
        PAIR( AEP_INT_UP )
        PAIR( AEP_ALL_UP )
        PAIR( AEP_SPEED_UP )
        PAIR( AEP_PBLUE )
        PAIR( AEP_SNAKES )
        PAIR( AEP_INVISIBLE )
        PAIR( AEP_CLAIRVOYANCE )
        PAIR( AEP_CLAIRVOYANCE_PLUS )
        PAIR( AEP_SUPER_CLAIRVOYANCE )
        PAIR( AEP_STEALTH )
        PAIR( AEP_EXTINGUISH )
        PAIR( AEP_GLOW )
        PAIR( AEP_PSYSHIELD )
        PAIR( AEP_RESIST_ELECTRICITY )
        PAIR( AEP_CARRY_MORE )
        PAIR( AEP_SAP_LIFE )
        PAIR( AEP_FUN )
        PAIR( AEP_SPLIT )
        PAIR( AEP_HUNGER )
        PAIR( AEP_THIRST )
        PAIR( AEP_SMOKE )
        PAIR( AEP_EVIL )
        PAIR( AEP_SCHIZO )
        PAIR( AEP_RADIOACTIVE )
        PAIR( AEP_MUTAGENIC )
        PAIR( AEP_ATTENTION )
        PAIR( AEP_STR_DOWN )
        PAIR( AEP_DEX_DOWN )
        PAIR( AEP_PER_DOWN )
        PAIR( AEP_INT_DOWN )
        PAIR( AEP_ALL_DOWN )
        PAIR( AEP_SPEED_DOWN )
        PAIR( AEP_FORCE_TELEPORT )
        PAIR( AEP_MOVEMENT_NOISE )
        PAIR( AEP_BAD_WEATHER )
        PAIR( AEP_SICK )
        // *INDENT-ON*
        case NUM_AEPS:
            break;
    }
    debugmsg( "Invalid AEP" );
    abort();
}
template<>
std::string enum_to_string<art_effect_active>( art_effect_active data )
{
    switch( data ) {
        // *INDENT-OFF*
        PAIR( AEA_NULL )
        PAIR( AEA_STORM )
        PAIR( AEA_FIREBALL )
        PAIR( AEA_ADRENALINE )
        PAIR( AEA_MAP )
        PAIR( AEA_BLOOD )
        PAIR( AEA_FATIGUE )
        PAIR( AEA_ACIDBALL )
        PAIR( AEA_PULSE )
        PAIR( AEA_HEAL )
        PAIR( AEA_CONFUSED )
        PAIR( AEA_ENTRANCE )
        PAIR( AEA_BUGS )
        PAIR( AEA_TELEPORT )
        PAIR( AEA_LIGHT )
        PAIR( AEA_GROWTH )
        PAIR( AEA_HURTALL )
        PAIR( AEA_FUN )
        PAIR( AEA_SPLIT )
        PAIR( AEA_RADIATION )
        PAIR( AEA_PAIN )
        PAIR( AEA_MUTATE )
        PAIR( AEA_PARALYZE )
        PAIR( AEA_FIRESTORM )
        PAIR( AEA_ATTENTION )
        PAIR( AEA_TELEGLOW )
        PAIR( AEA_NOISE )
        PAIR( AEA_SCREAM )
        PAIR( AEA_DIM )
        PAIR( AEA_FLASH )
        PAIR( AEA_VOMIT )
        PAIR( AEA_SHADOWS )
        PAIR( AEA_STAMINA_EMPTY )
        // *INDENT-ON*
        case NUM_AEAS:
            break;
    }
    debugmsg( "Invalid AEA" );
    abort();
}

template<>
std::string enum_to_string<art_charge>( art_charge data )
{
    switch( data ) {
        // *INDENT-OFF*
        PAIR( ARTC_NULL )
        PAIR( ARTC_TIME )
        PAIR( ARTC_SOLAR )
        PAIR( ARTC_PAIN )
        PAIR( ARTC_HP )
        PAIR( ARTC_FATIGUE )
        PAIR( ARTC_PORTAL )
        // *INDENT-ON*
        case NUM_ARTCS:
            break;
    }
    debugmsg( "Invalid ARTC" );
    abort();
}

template<>
std::string enum_to_string<art_charge_req>( art_charge_req data )
{
    switch( data ) {
        // *INDENT-OFF*
        PAIR( ACR_NULL )
        PAIR( ACR_EQUIP )
        PAIR( ACR_SKIN )
        PAIR( ACR_SLEEP )
        PAIR( ACR_RAD )
        PAIR( ACR_WET )
        PAIR( ACR_SKY )
        // *INDENT-ON*
        case NUM_ACRS:
            break;
    }
    debugmsg( "Invalid ACR" );
    abort();
}

template<>
std::string enum_to_string<artifact_natural_property>( artifact_natural_property data )
{
    switch( data ) {
        // *INDENT-OFF*
        PAIR( ARTPROP_NULL )
        PAIR( ARTPROP_WRIGGLING )
        PAIR( ARTPROP_GLOWING )
        PAIR( ARTPROP_HUMMING )
        PAIR( ARTPROP_MOVING )
        PAIR( ARTPROP_WHISPERING )
        PAIR( ARTPROP_BREATHING )
        PAIR( ARTPROP_DEAD )
        PAIR( ARTPROP_ITCHY )
        PAIR( ARTPROP_GLITTERING )
        PAIR( ARTPROP_ELECTRIC )
        PAIR( ARTPROP_SLIMY )
        PAIR( ARTPROP_ENGRAVED )
        PAIR( ARTPROP_CRACKLING )
        PAIR( ARTPROP_WARM )
        PAIR( ARTPROP_RATTLING )
        PAIR( ARTPROP_SCALED )
        PAIR( ARTPROP_FRACTAL )
        // *INDENT-ON*
        case ARTPROP_MAX:
            break;
    }
    debugmsg( "Invalid ARTPROP" );
    abort();
}
#undef PAIR

} // namespace io
