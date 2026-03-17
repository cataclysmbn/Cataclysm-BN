# Ramp guide

Ramps are the mapgen-side way to create walkable z-level transitions without using the explicit stair
actions bound to `<` and `>`.

## Flag meanings

- `GOES_UP` and `GOES_DOWN` are stair-style links. They are used by explicit vertical movement and by
  stair search.
- `RAMP_UP` and `RAMP_DOWN` are enter-triggered links. Moving into the flagged tile immediately shifts
  the mover one z-level.
- `RAMP` marks a tile as ramp-like for generic movement helpers, climb checks, and some pathfinding,
  but by itself it is not the same as `RAMP_UP` or `RAMP_DOWN`.
- `RAMP_END` marks the aligned end of a low-stairs style ramp and is used by `avatar_action::ramp_move`
  to make climbing less awkward.

## How the transition is applied

- Entering a `RAMP_UP` tile moves the creature or vehicle to the same `x,y` on `z + 1`.
- Entering a `RAMP_DOWN` tile moves the creature or vehicle to the same `x,y` on `z - 1`.
- The game does not search for a matching tile elsewhere on the map. Ramp transitions are strictly
  same-column (`x,y`) transitions.

Relevant code paths:

- `src/avatar_action.cpp` adjusts player movement when entering `RAMP_UP` / `RAMP_DOWN`.
- `src/monmove.cpp` does the same for monster movement.
- `src/vehicle_move.cpp` applies the same rule to vehicle centers and precalc points.
- `src/pathfinding.cpp` caches ramp transitions as same-column z-level changes.

## Pairing rules

For a normal two-level ramp, the tiles should be paired vertically at the same `x,y`:

- lower z-level high end: `RAMP_UP`
- upper z-level low end: `RAMP_DOWN`

This is how the built-in road ramp terrains work:

- `t_ramp_up_high` on the lower z-level
- `t_ramp_down_low` directly above it on the upper z-level

The opposite direction uses the same pair in reverse.

## Single-tile custom ramps

Single-tile ramps such as corpse slopes need extra care.

- If a tile uses `RAMP_UP`, entering it will try to place the mover onto `z + 1` at the same `x,y`.
- Because of that, a custom upward ramp should only be created where the upper tile is actually open
  for the transition.
- In practice, do not spawn a one-tile upward ramp under a roof or ceiling. For generated slopes like
  `zlope`, the tile above should be passable and should have `NO_FLOOR`.

If you place a `RAMP_UP` tile below an ordinary floor tile, the result is inconsistent with the
intended geometry: the ramp claims it climbs upward, but the destination column is still blocked from
above.

## When to use each approach

- Use `GOES_UP` / `GOES_DOWN` for stairs, ladders, manholes, and other deliberate vertical actions.
- Use `RAMP_UP` / `RAMP_DOWN` for tiles that should change z-level as part of ordinary movement.
- Add `RAMP` when the tile should also behave like a ramp for climb helpers or generic ramp logic.

## Practical checklist

- Pair upward and downward ramp endpoints vertically at the same `x,y` whenever possible.
- Do not assume `RAMP` alone creates an automatic z-level change.
- Do not place a single-tile upward ramp beneath a roof.
- If you need explicit player-only vertical interaction, use stairs or ladders instead of ramps.
