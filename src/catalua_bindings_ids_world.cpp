#include "catalua_bindings_ids_common.h"

#include "ammo_effect.h"
#include "magic.h"
#include "mapdata.h"
#include "monstergenerator.h"
#include "mutation.h"
#include "omdata.h"
#include "recipe.h"
#include "skill.h"
#include "trap.h"

auto cata::detail::reg_game_ids_world( sol::state &lua ) -> void
{
    reg_id<mutation_branch, false>( lua );
    reg_id<mutation_category_trait, false>( lua );
    reg_id<oter_t, true>( lua );
    reg_id<recipe, false>( lua );
    reg_id<Skill, false>( lua );
    reg_id<species_type, false>( lua );
    reg_id<spell_type, false>( lua );
    reg_id<ter_t, true>( lua );
    reg_id<trap, true>( lua );
    reg_id<ammo_effect, true>( lua );
}
