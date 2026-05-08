# Points, tripoints, and coordinate systems

This document describes the coordinate system in CBN, as well as covers the patterns
contributors should use when touching coordinate-related code during the ongoing
`tripoint` → typed coordinate migration. The goal is to eliminate raw `tripoint`/`point` from
game-logic code, consolidate the legacy conversion functions, and make coordinate intent
explicit at compile time.

## Axes

The game is three-dimensional, with the axes oriented as follows:

- The **x-axis** goes from left to right across the display (in non-isometric views).
- The **y-axis** goes from top to bottom of the display.
- The **z-axis** is vertical, with negative z pointing underground and positive z pointing to the
  sky.

## Coordinate systems

CBN uses a variety of coordinate systems for different purposes. These differ by scale and origin.

The most precise coordinates are **map square** (ms) coordinates. These refer to the tiles you see
normally when playing the game.

Three origins for map square coordinates are common:

- **Absolute** coordinates, sometimes called global, which are a global system for the whole game,
  relative to a fixed origin.
- **Bubble** coordinates, which are relative to the corner of the current "reality bubble", or `map`
  roughly centered on the avatar. In local map square coordinates, `x` and `y` values will both fall
  in the range `[0, MAPSIZE_X)`.
- **Vehicle** coordinates, which are relative to the pivot point of the current vehicle, and
  rotate with it. This origin is special because it requires that you use the `mount_to_*` and
  `*_to_mount` functions for them to work correctly, as all other coordinate spaces do not require
  you to account for rotation like vehicles do.

There is a **vehicle** scale (veh), however this is only used for the mount functions and is currently
the same as map square coordinates. It only serves to make it harder to make mistakes with vehicle
coordinates.

The next scale is **submap** (sm) coordinates. One submap is 12x12 (`SEEX`x`SEEY`) map squares.
Submaps are the scale at which chunks of the map are loaded or saved as they enter or leave the
reality bubble.

Next comes **overmap terrain** (omt) coordinates. One overmap terrain is 2x2 submaps. Overmap
terrains correspond to a single tile on the map view in-game, and are the scale of chunk of mapgen.

Then there's **memory map region** (mmr) coordinates. These are currently only used in saving/loading
submaps and you are unlikely to encounter them.

**segment** (seg) coordinates are used in a similar fashion to memory map regions, just a larger scale.

Largest are **overmap** (om) coordinates. One overmap is 180x180 (`OMAPX`x`OMAPY`) overmap terrains.
Large-scale mapgen (e.g. city layout) happens one overmap at a time.

As well as absolute, bubble and vehicle coordinates, sometimes we need to use coordinates relative so
some larger scale. For example, when performing mapgen for a single overmap, we want to work with
coordinates within that overmap. This will be an overmap terrain-scale point relative to the corner
of its containing overmap, and so typically take `x` and `y` values in the range [0,180).

## Vertical coordinates

Although `x` and `y` coordinates work at all these various scales, `z` coordinates are consistent
across all contexts. They lie in the range [-`OVERMAP_DEPTH`,`OVERMAP_HEIGHT`].

## Vehicle coordinates

Each vehicle has its own origin point, which will be at a particular part of the vehicle (e.g. it
might be at the driver's seat). The origin can move if the vehicle is damaged and all the vehicle
parts at that location are destroyed.

Vehicles use two systems of coordinates relative to their origin:

- **vehicle / mount** coordinates provide a location for vehicle parts that does not change as the
  vehicle moves. It is the map square of that part, relative to the vehicle origin, when the vehicle
  is facing due east.

- **map square** is the map square, relative to the origin, but accounting for the vehicle's current
  facing.

Vehicle facing is implemented via a combination of rotations (by quarter turns) and shearing to
interpolate between quarter turns. The logic to convert between vehicle mount and map square
coordinates is complicated and handled by the `vehicle::abs_to_mount()` and
`vehicle::mount_to_bubble()` families of functions.

Currently, vehicle mount coordinates do not have a z-level component, but vehicle map square
coordinates do. The z coordinate is relative to the vehicle origin. This is likely to change, as
we migrate to typed tripoints.

## Point types

To work with these coordinate systems we have a variety of types. These are defined in
`coordinates.h`. For example, we have `point_abs_ms` for absolute map-square coordinates. The three
parts of the type name are _dimension_ `_` _origin_ `_` _scale_.

- **dimension** is either `point` for two-dimensional or `tripoint` for three-dimensional.
- **origin** specifies what the value is relative to, and can be:
  - `rel` means relative to some arbitrary point. This is the result of subtracting two points with
    a common origin. It would be used for example to represent the offset between the avatar and a
    monster they are shooting at.
  - `abs` means global absolute coordinates.
  - `bub` means relative to the corner of the reality bubble.
  - `mnt` means relative (and rotated to) the reference point of a vehicle.
  - `sm` means relative to a corner of a submap.
  - `omt` means relative to a corner of an overmap terrain.
  - `om` means relative to a corner of an overmap.
- **scale** means the scale as discussed above.
  - `ms` for map square.
  - `veh` for vehicle mount coordinates (only relevant for the `mnt` origin).
  - `sm` for submap.
  - `omt` for overmap terrain.
  - `seg` for segment.
  - `om` for overmap.

## Raw point types

As well as these types with origin and scale encoded into the type, there are simple raw point types
called just `point` and `tripoint`. These can be used when no particular game scale is intended.

At time of writing we are still in the process of transitioning the codebase away from using these
raw point types everywhere, so you are likely to see legacy code using them in places where the more
type-safe points are more appropriate. Code should prefer to use the types which include their coordinate
system where feasible.

The rule of thumb:
If a tripoint or point represents a position in the world (or derivative reference frame)
then it should be typed.

## Converting between point types

### Changing scale

To change the scale of a point without changing its origin, use `project_to`. For example:

```cpp
point_abs_ms pos_ms = get_avatar()->global_square_location().xy();
point_abs_omt pos_omt = project_to<coords::omt>( pos_ms );
assert( pos_omt == get_avatar()->global_omt_location().xy() );
```

The same function `project_to` can be used for scaling up or down. When converting to a coarser
coordinate system precision is of course lost. If you care about the remainder then you must instead
use `project_remain`.

`project_remain` allows you to convert to a coarser coordinate system and also capture the remainder
relative to that coarser point. It returns a helper struct intended to be used with
[`std::tie`](https://en.cppreference.com/w/cpp/utility/tuple/tie) to capture the two parts of the
result. For example, suppose you want to know which overmap the avatar is in, and which overmap
terrain they are in within that overmap.

```cpp
point_abs_omt abs_pos = get_avatar()->global_omt_location().xy();
point_abs_om overmap;
point_om_omt omt_within_overmap;
std::tie( overmap, omt_within_overmap ) = project_remain<coords::om>( abs_pos );
```

That makes sense for two-dimensional `point` types, but how does it handle `tripoint`? Recall that
the z-coordinates do not scale along with the horizontal dimensions, so `z` values are unchanged by
`project_to` and `project_remain`. However, for `project_remain` we don't want to duplicate the
z-coordinate in both parts of the result, so you must choose exactly one to be a `tripoint`. In the
example above, z-coodinates do not have much meaning at the overmap scale, so you probably want the
z-coordinate in `omt_within_overmap`. That can be done as follows:

```cpp
tripoint_abs_omt abs_pos = get_avatar()->global_omt_location();
point_abs_om overmap;
tripoint_om_omt omt_within_overmap;
std::tie( overmap, omt_within_overmap ) = project_remain<coords::om>( abs_pos );
```

The last available operation for rescaling points is `project_combine`. This performs the opposite
operation from `project_remain`. Given two points where the origin of the second matches the scale
of the first, you can combine them into a single value. As you might expect from the above
discussion, one of these two can be a `tripoint`, but not both.

```cpp
tripoint_abs_omt abs_pos = get_avatar()->global_omt_location();
point_abs_om overmap;
tripoint_om_omt omt_within_overmap;
std::tie( overmap, omt_within_overmap ) = project_remain<coords::om>( abs_pos );
tripoint_abs_omt abs_pos_again = project_combine( overmap, omt_within_overmap );
assert( abs_pos == abs_pos_again );
```

The functions in `coordinate_conversions.h` (e.g. `ms_to_sm_copy`, `sm_to_omt_remain`, `omt_to_sm`)
are **legacy**. Replace them with the three template functions from `coordinates.h`. These only
require you to state the _destination_ scale; the source is baked into the input type.

### Changing origin

`project_remain` and `project_combine` facilitate some changes of origin, but only those origins
specifically related to rescaling. To convert to or from bubble or vehicle coordinates requires a
specific `map` or `vehicle` object.

### Bubble Conversions - Use `abs_to_bub` / `bub_to_abs`

Never compute bubble coordinates by hand. Use the map's conversion functions:

```cpp
// Bad - manual arithmetic, easy to get wrong
tripoint local = p - tripoint( abs_sub.x() * SEEX, abs_sub.y() * SEEY, 0 );

// Good
tripoint_bub_ms bub = here.abs_to_bub( abs_ms_pos );
tripoint_abs_ms abs = here.bub_to_abs( bub_ms_pos );
```

Free-function overloads (no `get_map().` needed at call sites) exist for all common typed variants:

```cpp
abs_to_bub( tripoint_abs_ms )  →  tripoint_bub_ms
bub_to_abs( tripoint_bub_ms )  →  tripoint_abs_ms
abs_to_bub( tripoint_abs_sm )  →  tripoint_bub_sm
bub_to_abs( tripoint_bub_sm )  →  tripoint_abs_sm
// …and their 2D point_ variants
```

Avoid using these in performance critical locations, since the functions themselves have to fetch
the map object. This is most pertinent to lighting, shadows, and other cache calculations. These
make up most of the valid uses for bubble space, so think before you use them. Prefer keeping a
local variable for the map before large iterations if possible.

**Bubble-space z is always the absolute z-level.**

"Bubble space" means coordinates measured from the top-left corner of the reality bubble, exactly
as `tripoint_sm_ms` means "map squares from the top-left of this submap." The x and y components
are bubble-relative, but z is absolute because the bubble never shifts vertically
(`vertical_shift` only updates `abs_sub.z()`; it does not reorganize the grid). Therefore:

```
bub.z() == abs.z()    // always true
```

This is enforced by `abs_to_bub` and `bub_to_abs`, which transform XY only. Do not add or subtract
`abs_sub.z()` from z manually - it is always a no-op and will introduce bugs.

## Point operations

We provide standard arithmetic operations as overloaded operators, but limit them to prevent bugs.
For example, most point types cannot be multiplied by a constant, but ones with the `rel` origin can
(it makes sense to say "half as far in the same direction").

Similarly, you can't generally add two points together, but you can when one of them has the `rel`
origin, or if one of them is a raw point type.

For computing distances a variety of functions are available, depending on your requirements:
`square_dist`, `trig_dist`, `rl_dist`, `manhattan_dist`. Other related utility functions include
`direction_from` and `line_to`.

To iterate over nearby points of the same type you can use `closest_points_first`.

### Deprecated → preferred reference

| Deprecated                            | Preferred                                 |
| ------------------------------------- | ----------------------------------------- |
| `here.getabs( tripoint )`             | `here.bub_to_abs( tripoint_bub_ms( p ) )` |
| `here.getlocal( tripoint )`           | `here.abs_to_bub( tripoint_abs_ms( p ) )` |
| `ms_to_sm_copy( p )`                  | `project_to<coords::sm>( p )`             |
| `sm_to_ms_copy( p )`                  | `project_to<coords::ms>( p )`             |
| `sm_to_omt_copy` + `sm_to_omt_remain` | `project_remain<coords::omt>( p )`        |
| `omt_to_ms_copy( omt ) + offset`      | `project_combine( omt, offset )`          |

## Available Template Helpers

All of the following are implemented and available for use:

| Helper                            | Location                 | Purpose                                             |
| --------------------------------- | ------------------------ | --------------------------------------------------- |
| `project_to<S>(p)`                | `coordinates.h`          | Scale conversion, preserves origin                  |
| `project_remain<S>(p)`            | `coordinates.h`          | Quotient + remainder decomposition                  |
| `project_combine(coarse, fine)`   | `coordinates.h`          | Recombine quotient + remainder                      |
| `abs_to_bub(p)` / `bub_to_abs(p)` | `map.h` (free functions) | Bubble ↔ absolute                                   |
| `p.reinterpret_as<T>()`           | `coordinates.h`          | Explicit type-pun during migration scaffolding only |
| `IsCoordPoint<T>` concept         | `coordinates.h`          | Constrains templates to typed coordinates           |
| `rl_dist(a, b)` typed overload    | `line.h`                 | Accepts any same-type `coord_point` pair            |
| `ch.bub_pos()` / `ch.abs_pos()`   | `creature.h`             | Typed creature position accessors                   |

`reinterpret_as<T>()` is a migration scaffold: it makes unsafe origin-punning explicit and grep-able.
A call site that still uses it is not fully migrated.

## Absolute vs Bubble Space

Prefer **absolute space** for game logic when the conversion is straightforward. Bubble space is appropriate for:

- Rendering and display code
- Functions that are inherently tied to the reality bubble e.g. `map::shift`
- Code where converting is non-trivial and the change is out of scope

Legacy code leaned on the reality bubble as a convenient local frame. When you encounter such code and the fix is a simple swap, make it. If it requires broader surgery, move on.
