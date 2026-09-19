#include "magic/spell_targeting.h"

#include "character.h"
#include "creature.h"
#include "game.h"
#include "magic/magic.h"

namespace {

auto spell_target_is_visible(
    const spell& casting, const Character& caster, const tripoint_bub_ms& target) -> bool {
    if (casting.is_valid_target(target_ground) || caster.sees(target)) { return true; }

    const auto* const target_critter = g->critter_at<Creature>(target, true);
    return target_critter != nullptr
        && (caster.sees(*target_critter) || caster.sees_with_infrared(*target_critter)
            || caster.sees_with_specials(*target_critter));
}

} // namespace

auto spell_target_can_be_resolved(
    const spell& casting, const Character& caster, const tripoint_bub_ms& target) -> bool {
    return casting.is_valid_target(caster, target)
        && spell_target_is_visible(casting, caster, target);
}
