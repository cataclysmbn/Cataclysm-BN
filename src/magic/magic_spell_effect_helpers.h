#pragma once

#include <functional>
#include <set>

#include "coordinates.h"

class Creature;
class spell;

// spells do not reduce in damage the further away from the epicenter the targets are
// rather they do their full damage in the entire area of effect
std::set<tripoint_abs_ms> calculate_spell_effect_area( const spell &sp,
        const tripoint_abs_ms &target,
        const std::function<std::set<tripoint_abs_ms>( const spell &, const tripoint_abs_ms &, const tripoint_abs_ms &, int, bool )>
        &
        aoe_func, const Creature &caster, bool ignore_walls = false );
