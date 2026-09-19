#include "activity_type.h"
#include "ammo.h"
#include "bionics.h"
#include "bodypart.h"
#include "catalua_bindings_ids_common.h"
#include "disease.h"
#include "effect.h"
#include "faction.h"
#include "itype.h"
#include "map/field_type.h"
#include "map/mapdata.h"

auto cata::detail::reg_game_ids_primary( sol::state &lua ) -> void
{
    reg_id<ammunition_type, false>( lua );
    reg_id<activity_type, false>( lua );
    reg_id<bionic_data, false>( lua );
    reg_id<body_part_type, true>( lua );
    reg_id<disease_type, false>( lua );
    reg_id<effect_type, false>( lua );
    reg_id<faction, false>( lua );
    reg_id<field_type, true>( lua );
    reg_id<furn_t, true>( lua );
    reg_id<itype, false>( lua );
}
