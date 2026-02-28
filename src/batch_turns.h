#pragma once

/**
 * Phase 6 — Batched Turn Interface
 *
 * Free functions for analytically advancing game state by N turns for
 * entities that were outside the reality bubble (or simply not processed)
 * during that period.  These are called from:
 *   - map::loadn()  (submap-level: fields, items, vehicles)
 *   - game::load_npcs()  (per-NPC catchup via Creature::batch_turns)
 *   - map::spawn_monsters_submap() (per-monster catchup)
 *
 * All functions clamp n to a safe maximum to prevent O(saved-time) blowups
 * on fresh saves loaded after long real-world absence.
 */

#include <cstdint>

class distribution_grid;
class submap;
class vehicle;

// ---------------------------------------------------------------------------
// Catchup caps — keep these conservative; they limit worst-case load time.
// ---------------------------------------------------------------------------
inline constexpr int MAX_CATCHUP_FIELDS  = 1000;
inline constexpr int MAX_CATCHUP_ITEMS   = 1000;
inline constexpr int MAX_CATCHUP_VEHICLE = 100;
inline constexpr int MAX_CATCHUP_MONSTER = 10000;
inline constexpr int MAX_CATCHUP_NPC    = 10000;
inline constexpr int MAX_CATCHUP_GRID   = 500;

// ---------------------------------------------------------------------------
// Submap-level batch functions
// ---------------------------------------------------------------------------

/**
 * Analytically advance field intensity/age in @p sm by @p n turns.
 * Uses the half_life formula to compute expected intensity drops rather than
 * simulating each turn individually.  Clamped at MAX_CATCHUP_FIELDS.
 */
void batch_turns_field( submap &sm, int n );

/**
 * Advance explicit per-turn item countdown counters by @p n turns.
 * Only affects items whose itype has countdown_interval > 0 and are currently
 * active.  Counters clamp at 0; the countdown_action fires on the next
 * normal in-bubble process_items() call (not here, to avoid side-effects
 * in out-of-bubble context).  Clamped at MAX_CATCHUP_FIELDS.
 */
void batch_turns_items( submap &sm, int n );

/**
 * Apply the net electrical balance of @p veh for @p n turns.
 * Uses net_battery_charge_rate_w() and a single charge/discharge call,
 * rather than simulating each turn individually.  Clamped at MAX_CATCHUP_VEHICLE.
 */
void batch_turns_vehicle( vehicle &veh, int n );

/**
 * Apply the net power balance of @p grid for @p n turns.
 * Calls distribution_grid::apply_net_power(net * n).
 * Clamped at MAX_CATCHUP_GRID.
 */
void batch_turns_distribution_grid( distribution_grid &grid, int n );

/**
 * Convenience wrapper: calls batch_turns_field, batch_turns_items, and
 * batch_turns_vehicle for every vehicle on the submap.  Intended for use
 * in map::loadn() immediately before actualize().
 */
void run_submap_batch_turns( submap &sm, int n );
