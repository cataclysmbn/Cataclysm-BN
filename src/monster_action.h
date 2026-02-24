#pragma once

#include <cstdint>

#include "point.h"

class Creature;

/**
 * Describes the single step a monster intends to take this tick.
 *
 * Produced by monster::decide_action() — a const, read-only planning function
 * that is safe to call from worker threads (Phase 2+).  Consumed by
 * monster::execute_action(), which performs all state mutations.
 *
 * All write operations (move_to, attack_at, bash_at, open_door, die, stumble,
 * special attacks, path trimming, repath, facing, wandf decrement, etc.) live
 * exclusively in execute_action().  decide_action() reads only.
 */

// ---------------------------------------------------------------------------
// Action kinds
// ---------------------------------------------------------------------------
enum class monster_action_kind : uint8_t {
    idle,        // No viable move; consume move_cost moves and optionally stumble.
    die,         // Hallucination death; execute_action calls die(nullptr).
    stumble,     // Monster is stunned; execute_action calls stumble() then zeroes moves.
    special,     // Reserved for Phase 3: signal that a special is pending serialisation.
                 // NOT currently set by decide_action() — specials fire as a fall-through
                 // side effect inside execute_action() to match original move() behaviour.
    move,        // Move to dest tile.
    attack,      // Melee attack creature at dest.
    bash,        // Bash obstacle at dest.
    open_door,   // Open door at dest.
    push,        // Push creature at dest (always must_serialize — displaces another monster).
};

// ---------------------------------------------------------------------------
// Action descriptor
// ---------------------------------------------------------------------------
struct monster_action_t {
    monster_action_kind kind           = monster_action_kind::idle;

    /// Destination tile for move / attack / bash / open_door / push.
    tripoint            dest           = tripoint_zero;

    /// Creature at dest when kind == attack.  Pointer is valid at decide time;
    /// execute_action re-validates before attacking.
    Creature           *target         = nullptr;

    /// Moves to consume for idle actions.  Ignored for all other kinds.
    int                 move_cost      = 0;

    /// Stagger multiplier passed to move_to().  1.0 = no stagger.
    float               stagger_adjust = 1.0f;

    /// Phase 3: route this action through the serial conflict queue rather than
    /// executing it in the parallel phase.  Always true for special and push.
    bool                must_serialize = false;

    /// Tier-0 path is stale; execute_action must call Pathfinding::route()
    /// before stepping.  Tier 1 uses the cached path as-is; Tier 2 never sets
    /// this flag (macro step does not use the path).
    bool                needs_repath   = false;

    /// execute_action should call stumble() after consuming move_cost moves.
    /// Used for the MATT_IGNORE / MATT_FOLLOW-at-range idle action that pairs
    /// a stumble with a partial move deduction (100 moves, not all moves).
    bool                needs_stumble  = false;
};
