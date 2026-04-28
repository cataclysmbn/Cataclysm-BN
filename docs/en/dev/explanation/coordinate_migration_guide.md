# Coordinate Type Migration Guide

This guide covers the patterns contributors should use when touching coordinate-related code during the ongoing `tripoint` → typed coordinate migration. The goal is to eliminate raw `tripoint`/`point` from game-logic code, consolidate the legacy conversion functions, and make coordinate intent explicit at compile time.

---

## The Type System at a Glance

Every typed coordinate is a `coord_point<Point, Origin, Scale>` specialisation. The three axes of the type encode what you know statically:

| Axis | Examples | Meaning |
|---|---|---|
| **Point** | `point`, `tripoint` | 2D or 3D |
| **Origin** | `origin::abs`, `origin::bubble`, `origin::relative` | What the coordinates are measured from |
| **Scale** | `coords::ms`, `coords::sm`, `coords::omt`, `coords::om` | Unit size |

Common named aliases you'll use most:

```
tripoint_abs_ms   - absolute world position, map-square scale
tripoint_abs_sm   - absolute world position, submap scale
tripoint_bub_ms   - position relative to reality bubble origin, map-square scale
tripoint_bub_sm   - position relative to reality bubble origin, submap scale
tripoint_rel_ms   - a displacement / offset (no fixed origin)
point_abs_sm      - 2D variants follow the same pattern
```

---

## Z-Level Convention

**Bubble-space z is always the absolute z-level.**

"Bubble space" means coordinates measured from the top-left corner of the reality bubble, exactly as `tripoint_sm_ms` means "map squares from the top-left of this submap." The x and y components are bubble-relative, but z is absolute because the bubble never shifts vertically (`vertical_shift` only updates `abs_sub.z()`; it does not reorganise the grid). Therefore:

```
bub.z() == abs.z()    // always true
```

This is enforced by `abs_to_bub` and `bub_to_abs`, which transform XY only. Do not add or subtract `abs_sub.z()` from z manually - it is always a no-op and will introduce bugs.

---

## Scale Conversions - Replace `coordinate_conversions.h`

The functions in `coordinate_conversions.h` (e.g. `ms_to_sm_copy`, `sm_to_omt_remain`, `omt_to_sm`) are **legacy**. Replace them with the three template functions from `coordinates.h`. These only require you to state the *destination* scale; the source is inferred.

### `project_to<DestScale>` - convert between scales

```cpp
// Before
tripoint sm = ms_to_sm_copy( ms_pos );

// After - origin is preserved, only scale changes
auto sm_pos = project_to<coords::sm>( abs_ms_pos );  // tripoint_abs_sm
```

### `project_remain<DestScale>` - fine → coarse, saving the remainder

Going from a fine scale to a coarser one loses granularity. `project_remain` captures both the coarse result *and* the intra-tile offset that would otherwise be discarded.

```cpp
// Before
point omt   = sm_to_omt_copy( sm_pos );
point intra = sm_to_omt_remain( sm_pos );  // confusing: modifies in-place

// After
auto [omt, intra] = project_remain<coords::omt>( abs_sm_pos );
// omt  : point_abs_omt
// intra: point_omt_ms  (origin reflects the coarse tile's scale)
```

### `project_combine` - coarse + fine → single fine coordinate

Going from a coarse scale to a finer one requires supplying the missing granularity. `project_combine` takes a coarse tile and an intra-tile offset and produces a single fine-scale coordinate - the logical counterpart to `project_remain`.

```cpp
// Before
tripoint ms = omt_to_ms_copy( omt ) + intra_offset;

// After
auto ms_pos = project_combine( abs_omt, omt_ms_offset );  // tripoint_abs_ms
```

**All three templates preserve the origin tag and transform XY only** (z passes through), matching the behavior of `project_to` with `multiply_xy` / `divide_xy_round_to_minus_infinity`.

---

## Bubble Conversions - Use `abs_to_bub` / `bub_to_abs`

Never compute bubble coordinates by hand. Use the map's conversion functions:

```cpp
// Bad - manual arithmetic, easy to get wrong
tripoint local = p - tripoint( abs_sub.x() * SEEX, abs_sub.y() * SEEY, 0 );

// Good
tripoint_bub_ms bub = abs_to_bub( abs_ms_pos );
tripoint_abs_ms abs = bub_to_abs( bub_ms_pos );
```

Free-function overloads (no `get_map().` needed at call sites) exist for all common typed variants:

```cpp
abs_to_bub( tripoint_abs_ms )  →  tripoint_bub_ms
bub_to_abs( tripoint_bub_ms )  →  tripoint_abs_ms
abs_to_bub( tripoint_abs_sm )  →  tripoint_bub_sm
bub_to_abs( tripoint_bub_sm )  →  tripoint_abs_sm
// …and their 2D point_ variants
```

### Deprecated → preferred reference

| Deprecated | Preferred |
|---|---|
| `here.getabs( tripoint )` | `bub_to_abs( tripoint_bub_ms( p ) )` |
| `here.getlocal( tripoint )` | `abs_to_bub( tripoint_abs_ms( p ) )` |
| `ms_to_sm_copy( p )` | `project_to<coords::sm>( p )` |
| `sm_to_ms_copy( p )` | `project_to<coords::ms>( p )` |
| `sm_to_omt_copy` + `sm_to_omt_remain` | `project_remain<coords::omt>( p )` |
| `omt_to_ms_copy( omt ) + offset` | `project_combine( omt, offset )` |
| `ch.pos()` wrapped in `tripoint_bub_ms(…)` | `ch.bub_pos()` |
| `here.getabs( ch.pos() )` | `ch.abs_pos()` |

---

## Raw `tripoint` → Typed Coordinate Patterns

When migrating a function, establish what the coordinate represents and apply the correct type. A brief reference:

| Situation | Type |
|---|---|
| Absolute world position (overmap, save data) | `tripoint_abs_ms` |
| Absolute world position, in submap scale | `tripoint_abs_sm` |
| Position inside the reality bubble (map queries) | `tripoint_bub_ms` |
| Displacement / offset / direction | `tripoint_rel_ms` |
| Untyped API requires raw value | `typed_pos.raw()` |

There are more, but these are the most commonly used.

---

## Available Template Helpers

All of the following are implemented and available for use:

| Helper | Location | Purpose |
|---|---|---|
| `project_to<S>(p)` | `coordinates.h` | Scale conversion, preserves origin |
| `project_remain<S>(p)` | `coordinates.h` | Quotient + remainder decomposition |
| `project_combine(coarse, fine)` | `coordinates.h` | Recombine quotient + remainder |
| `abs_to_bub(p)` / `bub_to_abs(p)` | `map.h` (free functions) | Bubble ↔ absolute |
| `p.reinterpret_as<T>()` | `coordinates.h` | Explicit type-pun during migration scaffolding only |
| `IsCoordPoint<T>` concept | `coordinates.h` | Constrains templates to typed coordinates |
| `rl_dist(a, b)` typed overload | `line.h` | Accepts any same-type `coord_point` pair |
| `ch.bub_pos()` / `ch.abs_pos()` | `creature.h` | Typed creature position accessors |

`reinterpret_as<T>()` is a migration scaffold: it makes unsafe origin-punning explicit and grep-able. A call site that still uses it is not fully migrated.

---

## Absolute vs Bubble Space

Prefer **absolute space** for game logic when the conversion is straightforward. Bubble space is appropriate for:
- Rendering and display code
- Functions that are inherently tied to the reality bubble e.g. `map::shift`
- Code where converting is non-trivial and the change is out of scope

Legacy code leaned on the reality bubble as a convenient local frame. When you encounter such code and the fix is a simple swap, make it. If it requires broader surgery, move on.
