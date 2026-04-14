#pragma once

/**
 * Free functions for analytically advancing game state by N turns for
 * entities that were outside the reality bubble (or simply not processed)
 * during that period.  These are called from:
 *   - map::loadn()  (submap-level: fields, items, vehicles)
 *   - game::load_npcs()  (per-NPC catchup via Creature::batch_turns)
 *   - map::spawn_monsters_submap() (per-monster catchup)
 *
 * All functions scale O(1) with respect to n and are uncapped.
 * Distribution grid catchup is handled by per-tile timestamps in
 * distribution_grid_tracker::update().
 */

class submap;

/**
 * Analytically advance field intensity/age in @p sm by @p n turns.
 * Uses the half_life formula to compute expected intensity drops rather than
 * simulating each turn individually.
 */
void batch_turns_field( submap &sm, int n );

/**
 * Advance explicit per-turn item countdown counters by @p n turns.
 * Only affects items whose itype has countdown_interval > 0 and are currently
 * active.  Counters clamp at 0; the countdown_action fires on the next
 * normal in-bubble process_items() call (not here, to avoid side-effects
 * in out-of-bubble context).
 */
void batch_turns_items( submap &sm, int n );

/**
 * Convenience wrapper: calls batch_turns_field, batch_turns_items, and
 * vehicle::update_time for every vehicle on the submap.  Intended for use
 * in map::loadn() immediately before actualize().
 */
void run_submap_batch_turns( submap &sm, int n );
