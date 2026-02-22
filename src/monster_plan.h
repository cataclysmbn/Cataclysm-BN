#pragma once

#include <vector>

#include "point.h"
#include "type_id.h"

/**
 * Captures the complete output of monster::compute_plan().
 *
 * compute_plan() is designed to be a pure function with respect to the
 * monster's own state: it reads *this but does not write it.  All intended
 * mutations are collected here and applied later by monster::apply_plan() on
 * the main thread.  This split is the prerequisite for running planning in
 * parallel across all monsters.
 *
 * Fields use final values, not deltas, because plan() logic reads its own
 * writes mid-function (e.g. anger is incremented then tested in the same
 * pass).  Shadowing with locals is simpler than tracking deltas.
 */
struct monster_plan_t {
    // Movement goal — result of set_dest() / unset_dest() calls during planning.
    // Initialized from the monster's current goal so a no-decision pass is
    // a no-op in apply_plan.
    tripoint goal;

    // Wander state.  Only written when swarm dispersal logic fires;
    // apply_plan only updates wander_pos/wandf when this flag is set.
    bool wander_updated = false;
    tripoint wander_pos;
    int wandf = 0;

    // Per-monster stat final values.
    int anger   = 0;
    int morale  = 0;
    int friendly = 0;

    // Faction anger entries accumulated during planning.
    // Applied via add_faction_anger() in apply_plan().
    struct faction_anger_entry {
        mfaction_id faction;
        int amount = 0;
    };
    std::vector<faction_anger_entry> faction_angers;

    // Effects to remove.  Applied via remove_effect() in apply_plan().
    std::vector<efftype_id> effects_to_remove;

    // Character aggro triggers that fired during planning.
    // Each entry is the reason string (a string literal) from the call site.
    // apply_plan() calls trigger_character_aggro(reason) for each.
    std::vector<const char *> aggro_triggers;
};
