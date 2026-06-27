#pragma once

#include "coordinates.h"

class Character;
class spell;

/// Returns whether a selected spell target still satisfies casting-time target checks.
auto spell_target_can_be_resolved( const spell &casting, const Character &caster,
                                   const tripoint_bub_ms &target ) -> bool;
