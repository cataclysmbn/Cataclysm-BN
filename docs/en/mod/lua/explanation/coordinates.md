# Typed Coordinates in Lua

The game uses a typed coordinate system to make position arithmetic safe and self-documenting.
Every coordinate carries two pieces of metadata: an **origin** and a **scale**. Mixing
incompatible coordinates (e.g. adding an overmap position to a map-square position) is a runtime
error rather than a silent wrong result.

This applies equally in Lua. **Prefer typed coordinates everywhere. Reserve the raw `Tripoint` and
`Point` types for pure offset arithmetic**; cases where you are computing a dimensionless 2D or
3D vector (e.g. a direction or a fixed displacement) and the result will be immediately added to or
subtracted from a typed coordinate.

---

## Origins

The **origin** of a coordinate describes the reference frame it was measured from.

| Origin string | Meaning                                                                       |
| ------------- | ----------------------------------------------------------------------------- |
| `"rel"`       | Dimensionless offset. Can be added to any coordinate at the same scale.       |
| `"abs"`       | Absolute game-world position. The only globally stable origin.                |
| `"bub"`       | Relative to the corner of the current reality bubble (the loaded map region). |
| `"mnt"`       | Local vehicle (mount) space, including rotation. Used with the `veh` scale.   |
| `"sm"`        | Relative to the corner of a specific submap.                                  |
| `"omt"`       | Relative to the corner of a specific overmap terrain tile.                    |
| `"mmr"`       | Relative to the corner of a memory-map region.                                |
| `"seg"`       | Relative to the corner of a segment.                                          |
| `"om"`        | Relative to the corner of a specific overmap.                                 |

The `"rel"` origin is special: it acts as a typed displacement value. Subtracting two matching
absolute coordinates produces a `"rel"` result; adding a `"rel"` coordinate to a non-relative one
produces the same non-relative origin back.

---

## Scales

The **scale** of a coordinate describes the unit size of each step.

| Scale string        | Abbreviation | Size                             |
| ------------------- | ------------ | -------------------------------- |
| `"map_square"`      | `ms`         | 1 game tile                      |
| `"vehicle"`         | `veh`        | Vehicle-local tile               |
| `"submap"`          | `sm`         | 12 × 12 map squares              |
| `"overmap_terrain"` | `omt`        | 2 submaps (24 × 24 map squares)  |
| `"mem_map_region"`  | `mmr`        | `MM_REG_SIZE` submaps            |
| `"segment"`         | `seg`        | `SEG_SIZE` overmap terrain tiles |
| `"overmap"`         | `om`         | `OMAPX` overmap terrain tiles    |

The abbreviated form (`ms`, `sm`, `omt`, etc.) is used in all factory function names and in the
string arguments accepted by the projection functions.

---

## Type names

A typed coordinate's Lua type name is formed by combining origin and scale in PascalCase:

```
Tripoint<Origin><Scale>   →   TripointAbsMs, TripointBubSm, TripointRelOmt, …
Point<Origin><Scale>      →   PointAbsMs, PointRelSm, PointBubMs, …
```

There are 31 valid origin–scale combinations. Attempting to construct an unsupported combination
(e.g. `TripointAbsVeh`) throws a runtime error.

---

## Creating typed coordinates

### Factory functions (functional style)

The `coords` library exposes `coords.tripoint_<origin>_<scale>(x, y, z)` and
`coords.point_<origin>_<scale>(x, y)` for every supported combination:

```lua
local player_pos  = gapi.get_avatar():get_pos_ms()   -- TripointBubMs from the API
local spawn_point = coords.tripoint_abs_ms(100, 200, 0)
local delta       = coords.tripoint_rel_ms(5, 0, 0)   -- typed offset
local omt_pos     = coords.tripoint_abs_omt(12, 8, 0)
```

There is also a generic constructor that takes origin and scale as strings:

```lua
local p = coords.tripoint("abs", "ms", 100, 200, 0)
local q = coords.point("rel", "sm", 3, 3)
```

### Named constructors (OOP style)

Every typed coordinate also has a named constructor table in global scope:

```lua
local p = TripointAbsMs.new(100, 200, 0)      -- from x, y, z
local q = TripointAbsMs.new(raw_tripoint)      -- from raw Tripoint
local r = TripointAbsMs.new(point_coord, z)    -- from matching PointAbsMs + z
local s = TripointAbsMs.new()                  -- zero
```

Both styles produce identical objects. The factory style is generally more concise.

---

## Reading coordinate components

```lua
local p = coords.tripoint_abs_ms(10, 20, 1)

print(p:x(), p:y(), p:z())   -- 10  20  1
print(p:origin())             -- "abs"
print(p:scale())              -- "ms"
print(p:type())               -- "TripointAbsMs"

local xy = p:xy()             -- PointAbsMs(10, 20); drops z, preserves origin/scale
local raw = p:raw()           -- raw Tripoint(10, 20, 1); strips all tags
```

Components can be mutated with `set_x`, `set_y`, `set_z`.

---

## Arithmetic

### Addition

A typed coordinate can be added to:

- A raw `Point` or raw `Tripoint`; the result keeps the original origin and scale
- A `"rel"` typed coordinate **at the same scale**; same result type
- Another `"rel"` typed coordinate plus the operand is non-relative: the non-relative origin wins

```lua
local pos     = coords.tripoint_abs_ms(10, 20, 0)
local offset  = coords.tripoint_rel_ms(3, 0, 0)  -- typed relative offset

local new_pos = pos + offset          -- TripointAbsMs(13, 20, 0)
local also    = pos + Tripoint.new(0, 5, 0)  -- TripointAbsMs(10, 25, 0)  (raw as vector)
```

### Subtraction

Subtracting a `"rel"` coordinate or raw value works like addition in reverse. Subtracting two
matching non-relative coordinates of the same origin and scale produces a `"rel"` result:

```lua
local a   = coords.tripoint_abs_ms(15, 20, 0)
local b   = coords.tripoint_abs_ms(10, 20, 0)
local rel = a - b                              -- TripointRelMs(5, 0, 0)
```

### Multiplication

Only `"rel"` coordinates can be multiplied by an integer scalar:

```lua
local step  = coords.tripoint_rel_ms(1, 0, 0)
local five  = step * 5                          -- TripointRelMs(5, 0, 0)
```

### Equality and ordering

Two typed coordinates are equal only when they share the same origin, scale, and raw values.
The `<` operator orders by `(origin, scale, raw)` lexicographically, making typed coordinates
safe to use as table keys and in sorted containers.

---

## Projection

Projection converts a coordinate from one scale to another **while preserving the origin**.
The game's scale hierarchy runs:

```
ms  <  sm  <  omt  <  mmr  <  seg  <  om
```

Projecting to a coarser scale rounds toward negative infinity (floor division).

```lua
local abs_ms  = coords.tripoint_abs_ms(25, 26, 2)
local abs_omt = abs_ms:to_omt()   -- TripointAbsOmt(1, 1, 2)
local abs_sm  = abs_ms:to_sm()    -- TripointAbsSm(2, 2, 2)
```

Convenience shorthand methods exist for every target scale: `:to_ms()`, `:to_sm()`, `:to_omt()`,
`:to_mmr()`, `:to_seg()`, `:to_om()`. A generic `:to(scale_string)` form is also available.

### project_remain: split into quotient and remainder

When you need to know both _which coarser tile_ a fine coordinate falls in **and** where _within_
that tile it sits, use `project_remain`. It returns two values:

```lua
local abs_ms = coords.tripoint_abs_ms(25, 26, 2)
local quotient, remainder = abs_ms:project_remain_omt()
-- quotient  → TripointAbsOmt(1, 1, 2):   which overmap terrain tile
-- remainder → PointOmtMs(1, 2):          offset within that tile (fine scale, omt origin)
```

Shorthand methods: `:project_remain_sm()`, `:project_remain_omt()`, `:project_remain_mmr()`,
`:project_remain_seg()`, `:project_remain_om()`. Or the generic form:
`:project_remain("omt")`.

The `coords` library also exposes these as free functions:
`coords.project_remain(coord, scale_string)`, `coords.project_remain_omt(coord)`, etc.

### project_combine: reconstruct from quotient and remainder

`project_combine` is the inverse of `project_remain`. Given a coarse coordinate and a fine
offset, it produces the fine-scale absolute coordinate:

```lua
local quotient, remainder = abs_ms:project_remain_omt()
local restored = coords.project_combine(quotient, remainder)
-- restored → TripointAbsMs(25, 26, 2)
```

This is also available as an instance method: `quotient:project_combine(remainder)`.

---

## Distance functions

Distance between two typed coordinates requires them to share the same origin and scale:

```lua
local a = coords.tripoint_abs_ms(10, 10, 0)
local b = coords.tripoint_abs_ms(13, 14, 0)

local rl  = coords.rl_dist(a, b)      -- rectilinear (Manhattan / Chebyshev) distance
local trig = coords.trig_dist(a, b)   -- Euclidean distance
local sq  = coords.square_dist(a, b)  -- square (Chebyshev) distance
```

Instance methods are also provided: `a:rl_dist(b)`, `a:trig_dist(b)`, `a:square_dist(b)`.

---

## Tile enumeration helpers

The `coords` library provides utilities that return arrays of typed point coordinates covering
a standard tile region, useful for iterating over areas:

```lua
local sm_tiles  = coords.submap_tiles()           -- all PointSmMs in one submap
local bub_tiles = coords.tinymap_tiles()          -- all PointBubMs in the tinymap
local omt_tiles = coords.overmap_terrain_tiles()  -- all PointOmtMs in one overmap terrain tile
local om_tiles  = coords.overmap_tiles()          -- all PointOmMs in one overmap
```

---

## When to use raw Point and Tripoint

`Point` and `Tripoint` are untagged 2D/3D integer vectors. Use them only when:

- Computing a pure offset with no inherent game-world meaning (e.g. a direction constant, a
  neighbour delta, a rotation result).
- The value will be immediately added to or subtracted from a typed coordinate rather than stored.

```lua
-- Acceptable: raw Tripoint as a throwaway displacement vector
local neighbour = player_pos + Tripoint.new(1, 0, 0)

-- Preferred: named typed offset when the displacement has a scale context
local step = coords.tripoint_rel_ms(1, 0, 0)
local next  = player_pos + step
```

Do not store positions as raw `Tripoint` or `Point` values when a typed equivalent exists. The
typed form catches mismatched-scale bugs at the point of arithmetic rather than silently
producing wrong map coordinates.

---

## Valid origin–scale combinations

Not every origin is meaningful at every scale. The supported combinations are:

| Origin | Valid scales                          |
| ------ | ------------------------------------- |
| `rel`  | all scales                            |
| `abs`  | `ms`, `sm`, `omt`, `mmr`, `seg`, `om` |
| `bub`  | `ms`, `sm`                            |
| `mnt`  | `veh` only                            |
| `sm`   | `ms` only                             |
| `omt`  | `ms`, `sm`                            |
| `mmr`  | `ms`, `sm`, `omt`                     |
| `seg`  | `ms`, `sm`, `omt`, `mmr`              |
| `om`   | `ms`, `sm`, `omt`, `mmr`, `seg`       |

Constructing an unsupported combination throws a runtime error with a message identifying the
invalid type name.

---

## Quick-reference: coords library API

| Function                                         | Description                                            |
| ------------------------------------------------ | ------------------------------------------------------ |
| `coords.tripoint(origin, scale, x, y, z)`        | Generic typed tripoint constructor                     |
| `coords.point(origin, scale, x, y)`              | Generic typed point constructor                        |
| `coords.tripoint_<o>_<s>(x, y, z)`               | Typed tripoint factory (e.g. `coords.tripoint_abs_ms`) |
| `coords.point_<o>_<s>(x, y)`                     | Typed point factory                                    |
| `coords.project_remain(coord, scale)`            | Split coordinate into quotient + remainder             |
| `coords.project_remain_sm/omt/mmr/seg/om(coord)` | Shorthand project_remain variants                      |
| `coords.project_combine(coarse, fine)`           | Reconstruct fine coordinate from split pair            |
| `coords.rl_dist(a, b)`                           | Rectilinear (Manhattan/Chebyshev) distance             |
| `coords.trig_dist(a, b)`                         | Euclidean distance                                     |
| `coords.square_dist(a, b)`                       | Square (Chebyshev) distance                            |
| `coords.submap_tiles()`                          | Array of all `PointSmMs` offsets within one submap     |
| `coords.tinymap_tiles()`                         | Array of all `PointBubMs` offsets within the tinymap   |
| `coords.overmap_terrain_tiles()`                 | Array of all `PointOmtMs` offsets within one OMT       |
| `coords.overmap_tiles()`                         | Array of all `PointOmMs` offsets within one overmap    |
