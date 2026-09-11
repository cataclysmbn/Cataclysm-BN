#include "catalua_bindings_ids_common.h"

#include "enchantments/enchantment_flag.h"
#include "enchantments/enchantment_value.h"
#include "emit.h"
#include "fault.h"
#include "martialarts.h"
#include "mod_manager.h"
#include "mongroup.h"
#include "requirements.h"
#include "type_id.h"
#include "vitamin.h"

auto cata::detail::reg_game_ids_misc( sol::state &lua ) -> void
{
    reg_id<MOD_INFORMATION, false>( lua );
    reg_id<MonsterGroup, false>( lua );
    reg_id<weapon_category, false>( lua );
    reg_id<emit, false>( lua );
    reg_id<fault, false>( lua );
    reg_id<quality, false>( lua );
    reg_id<vitamin, false>( lua );
    reg_id<enchantment_value, false>( lua );
    reg_id<enchantment_flag, false>( lua );
}
