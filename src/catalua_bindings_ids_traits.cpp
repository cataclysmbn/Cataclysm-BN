#include "catalua_bindings_ids_common.h"

#include "flag.h"
#include "flag_trait.h"
#include "martialarts.h"
#include "material.h"
#include "mission.h"
#include "monfaction.h"
#include "morale_types.h"
#include "mtype.h"

auto cata::detail::reg_game_ids_traits( sol::state &lua ) -> void
{
    reg_id<json_flag, false>( lua );
    reg_id<json_trait_flag, false>( lua );
    reg_id<ma_buff, false>( lua );
    reg_id<mission_type, false>( lua );
    reg_id<ma_technique, false>( lua );
    reg_id<martialart, false>( lua );
    reg_id<material_type, false>( lua );
    reg_id<monfaction, true>( lua );
    reg_id<morale_type_data, false>( lua );
    reg_id<mtype, false>( lua );
}
