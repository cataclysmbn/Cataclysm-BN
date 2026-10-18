# Multi-Z Vehicles: First Playable Milestone

## Status and Purpose

**Status: P1 coordinate/persistence foundations are isolated on `feat/vehicle-3d` and validated on Windows after separation. The playable milestone is not complete.**

This design is based on source inspection at `f4c4a599f123490b3090ebafe4141e20301e2eaf`. The observed gaps below have not all been reproduced in a running build. Recheck the named functions before implementation; file paths and symbols are more useful than fixed line numbers as the code changes.

The milestone is a small, buildable, two-deck vehicle that behaves as **one vehicle**, including an installable stair assembly usable by the avatar, NPCs, and stair-capable monsters. It is not limited to loading a hard-coded double-decker prototype. Geometry and persistence are the first implementation phase, not the entire milestone.

This document selects concrete initial behavior so implementation can proceed without independently inventing rules in each subsystem. These choices are proposals for review, especially the stationary-only stair restriction and the supported, non-transparent stairwell abstraction.

Related documentation:

- [Coordinate systems](../explanation/points_coordinates.md).
- [Game objects](../explanation/game_objects.md).
- [Vehicle construction preview](./vehicle_tiles.md).
- [CMake builds](../guides/building/cmake.md).

The coordinate guide now distinguishes local mount z, nominal yaw transforms, and actual terrain-deformed part positions.

## Branch Scope

The approved local, committed stack is `feat/vehicle-3d` as the foundation, with `fix/vehicle-split-cleanup` based on that foundation. No push is part of this separation. The foundation supports both branches; the dependency is one-way: cleanup depends on the feature foundation, but the 3D feature does not depend on cleanup.

This plan owns permanent local height, nominal XYZ coordinates, ordinary XY reference changes, horizontal wheel support, and the future height-specific construction, physics, stairs, and routing work. All cleanup not directly required by 3D belongs to `fix/vehicle-split-cleanup`, including generic split placement/preservation, identity and connection repair, collision/index/timer cleanup, and physical cargo/active-item/leak corrections. Multi-deck fixtures or an existing `[vehicle_multiz]` test tag do not make those corrections feature work.

P2 retains six-neighbor structural connectivity, split **component discovery**, removal of the four-component cap, and their tests. It does not absorb generic fragment reconstruction. Neither the P1 exit condition nor M1 acceptance requires the separate cleanup branch's generic fragment-preservation contract.

## Implementation Progress

The following foundation changes are retained on `feat/vehicle-3d`:

- Saved `mount_dz`, with missing values defaulting to zero independently of terrain `z_offset`.
- Strict integer-token parsing and a local-height span bound; the new save field does not accept decimal/exponent spellings.
- Preservation of both terrain-offset buffers through part copy/move operations.
- Nominal XYZ transforms with horizontal pivots and zero-z steering displacement.
- XY-only ordinary reference rebasing, including upper-deck origin columns and both yaw anchors.
- Prototype export grouped directly by full local mount, excluding removed parts and preserving nonzero z.
- Per-part world-z bounds guards before indexing vehicle caches. These are defensive guards, not complete vehicle-envelope validation at load/spawn/movement time.
- Horizontal wheel support/balance using the projected XY center of mass and wheel footprint, rather than requiring the center of mass to lie in the wheels' z plane.

Retained test sources and selectors are listed below as an inventory, not as results from the separated branch:

| Source under `tests/`            | Retained tests                                                                                                                                                                                                                                                                                                                                                     |
| -------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------ |
| `vehicle_geometry_test.cpp`      | `vehicle_nominal_geometry_preserves_xyz_at_every_yaw`, `vehicle_shift_parts_rebases_only_xy`, `vehicle_removal_keeps_an_upper_origin_column_without_repeated_rebasing`, `vehicle_export_groups_full_mounts_independently_of_placement`, `vehicle_spatial_caches_reject_out_of_world_decks`                                                                         |
| `vehicle_persistence_test.cpp`   | `vehicle_part_oldsave_defaults_mount_z_without_flattening_terrain`, `vehicle_part_mount_dz_rejects_invalid_numbers`, `vehicle_part_mount_dz_accepts_bounded_integer_tokens`, `vehicle_load_skips_invalid_mount_dz_and_keeps_other_parts`, `vehicle_roundtrip_preserves_signed_mounts_labels_and_zone_keys`, `vehicle_part_copy_move_preserve_both_terrain_buffers` |
| `vehicle_ramp_test.cpp`          | `vehicle_ramp_rebase_preserves_two_deck_part_locations`                                                                                                                                                                                                                                                                                                            |
| `vehicle_wheel_support_test.cpp` | `vehicle_wheel_support_uses_xy_on_nonzero_decks`                                                                                                                                                                                                                                                                                                                   |

The preserved patch's generic split-preservation geometry, persistence, connection, collision, and ramp tests belong to `fix/vehicle-split-cleanup`, as does `vehicle_ramp_cargo_locations_and_processing_use_installed_part`. They are not evidence of implemented feature behavior after separation; future P2 component-discovery tests remain feature-owned.

The extracted foundation builds both the game and tests with MSVC/Ninja. Its focused tests and non-benchmark vehicle regressions pass, including unsplit wheel-support coverage on nonzero decks. Remaining work includes prototype-loader round trips and whole-world passenger reload coverage. Complete placement-envelope validation belongs to the remaining spatial-safety work. P2-P7, including structural deck construction, height-aware physics, stairs, routing, editor controls, and the reference shadow, remain pending.

Verification results belong in each branch's change summary. Existing tests and the future acceptance matrix below are not claims that separated-branch or later milestone cases have passed.

## Milestone Contract

### Required Outcomes

| ID    | Acceptance requirement                                                                                                                                                                                                    |
| ----- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| M1-01 | Ordinary construction can create vertically connected frames and install different components at the same local XY on different decks.                                                                                    |
| M1-02 | One vehicle owns all decks, receives one simulation update, and shares its existing power, fuel, controls, and cargo abstractions.                                                                                        |
| M1-03 | Save/load preserves deck coordinates, cargo, labels, zones, and passengers without flattening the vehicle. Old planar saves still load.                                                                                   |
| M1-04 | Translation, yaw, ramp traversal, and ordinary XY reference-point changes preserve deck geometry and occupant positions. P2 separately validates 3D structural component discovery, not generic fragment reconstruction.  |
| M1-05 | The complete occupied volume participates in collision, external support, world bounds, and relevant map-cache updates. An upper deck cannot support its own vehicle against falling.                                     |
| M1-06 | An installable lower stair part and upper landing provide exact, bidirectional travel between adjacent local decks.                                                                                                       |
| M1-07 | Manual avatar movement, avatar automove, NPC routing/execution, and both monster routing modes can use eligible stairs.                                                                                                   |
| M1-08 | Broken, removed, blocked, moving, unloaded, or otherwise invalid stairs never authorize a stale vertical teleport.                                                                                                        |
| M1-09 | Ordinary floors separate decks for support, sight, and physical attacks. Stair traversal does not turn the boundary into a general physics opening.                                                                       |
| M1-10 | Construction views provide visible deck controls using the stair up/down bindings and a simplified, non-interactive shadow of the immediately lower deck in ASCII and tiles. Rendering does not change the selected part. |
| M1-11 | Existing single-deck vehicles, terrain stairs, vehicle ramps, aircraft controls, and hanging rope ladders retain their behavior.                                                                                          |
| M1-12 | A compact two-deck fixture and a bus-like integration fixture exercise the complete workflow through production APIs.                                                                                                     |

### Initial Decisions

| Topic                 | Milestone decision                                                                                                                     |
| --------------------- | -------------------------------------------------------------------------------------------------------------------------------------- |
| Vehicle identity      | One vehicle object and one owning submap; occupancy can span many submaps and z-levels.                                                |
| Deck representation   | Existing `tripoint_mnt_veh`, with signed local z. No two-deck limit is encoded into the data model.                                    |
| Orientation           | Existing yaw and terrain-following ramp behavior; no pitch or roll.                                                                    |
| Structural attachment | Eligible structural frames connect through all six face-adjacent local mounts. Existing nonstructural attachment restrictions remain.  |
| Stair geometry        | Same local XY, exactly one local z between lower stair and upper landing.                                                              |
| Landing               | An explicit landing part, not an arbitrary aisle found above the stairs.                                                               |
| Stair operation       | Passive, bidirectional, and usable only while the vehicle is stationary and not falling or undergoing vertical movement.               |
| Crossing cost         | Base cost of 100 moves per crossing, charged once. Do not also charge the landing as an ordinary horizontal step.                      |
| Stairwell boundary    | Supported and opaque to cross-deck physical paths for this milestone; only intentional stair movement is permitted.                    |
| Actor restrictions    | No traversal while mounted on a creature, hauling a grabbed object through the stairs, or actively controlling the vehicle.            |
| Route storage         | Keep existing coordinate routes initially. Treat every planned step as a suggestion and validate the live transition before execution. |
| Work reach            | Preserve the existing abstract work-area rules. Deck selection alone does not introduce a new same-deck or scaffolding requirement.    |
| Editor navigation     | Reuse `LEVEL_UP` / `LEVEL_DOWN` and the effective stair bindings; preserve local cursor XY and zoom when changing decks.               |
| Editor reference      | Show a muted footprint of local deck `selected_z - 1` beneath the active deck; it never becomes an editable selection.                 |

Stationary-only applies to **stair crossing**, not to the vehicle: it must drive with occupants on both decks. If it begins moving during an approach, crossing stops safely and routing is reconsidered after it stops. Following a vehicle-relative destination while the vehicle moves is a later feature, not an implicit promise of the existing absolute-coordinate route format.

### Explicit Non-Goals

- Pitch, roll, rollover, suspension, load-bearing strength, or a general rigid-body simulator.
- Offset, diagonal, spiral, retractable, powered, or hatch-equipped stairs.
- Moving between decks while the vehicle is moving.
- An open stairwell transmitting falls, items, projectiles, scent, smoke, sound, or explosions through a new aperture.
- A general replacement for either pathfinder or arbitrary non-monotonic terrain routes. Local vehicle-deck continuity on a ramp is a required, bounded exception described below.
- Arbitrary multi-level vehicles carried on bike racks, or folding multi-level vehicles.
- Full multi-deck boats, aircraft, rail vehicles, or autonomous driving.
- New physical routing of electricity, fuel, or ammunition between decks.
- Layered ASCII vehicle blueprints, world-spawn balancing, or polished double-decker art.

Non-goals are not permission for unsafe fallback behavior. Newly enabled multi-deck configurations must reject unsupported folding/racking, and unsupported automation must not silently assume a flat footprint. Ordinary single-deck uses of those systems must remain available. Any temporary multi-deck restriction needs a specific message, a regression test, and documentation.

## Baseline Architecture and Gaps

This table records the inspected baseline before the foundation patch. Consult the progress section above for changes already made; the remaining rows are not all implemented.

| Area                   | Existing implementation                                                                       | Work needed                                                                                                       |
| ---------------------- | --------------------------------------------------------------------------------------------- | ----------------------------------------------------------------------------------------------------------------- |
| Part geometry          | `src/vehicle/vehicle_part.h`: full local mount, XY `precalc`, separate `z_terrain`.           | Specify and enforce the coordinate contract rather than converting every remaining point type.                    |
| Physical position      | `vehicle::abs_part_location` and `bub_part_location` compose mount z and terrain z.           | Use these for installed parts; audit nominal transform callers.                                                   |
| Nominal transforms     | `vehicle::rotate_to_world`, `mount_to_abs`, and `mount_to_bubble`.                            | Forward rotation currently populates only XY; define pivot z and inverse semantics.                               |
| Prototype input        | `vehicle_prototype::load` accepts explicit part/item `z`.                                     | Use explicit parts for fixtures; palette blueprints remain flat.                                                  |
| Save input/output      | `vehicle_part::serialize` and `deserialize` save mount X/Y and a separate terrain `z_offset`. | Add permanent mount z without reinterpreting old terrain offsets.                                                 |
| Part lifecycle         | `vehicle_part::copy_static_from` copies mount and `precalc`, not `z_terrain`.                 | Define and test copy/move and deliberate placement-state rebuilding.                                              |
| Structural graph       | `can_mount`, `can_unmount`, `is_connected`, `find_and_split_vehicles`.                        | Replace independent planar adjacency assumptions with one structural rule; remove the four-component split limit. |
| Map occupancy          | `map::add_vehicle_to_cache` and mapbuffer footprint indexing use actual part positions.       | Correct occupied-level enumeration, invalidation, bounds, and ownership/occupancy separation.                     |
| Support/collision      | `check_falling_or_floating`, `map::supports_above`, `vehicle_vehicle_collision`.              | Exclude self-support; classify collision from the movement/contact, not origin z.                                 |
| Construction           | `veh_interact` stores a 3D cursor and activities can store cursor z.                          | Add deck actions and filter both previews; ASCII drawing currently projects all decks and can overwrite `cpart`.  |
| Actor vertical actions | `handle_action`, `game::vertical_move`, `npc::move_to`.                                       | Add deliberate vehicle traversal; do not reuse forced movement or NPC's unchecked vertical shortcut.              |
| Physical boundaries    | Terrain floors, vehicle floor caches, and physical-path checks are separate.                  | Align vehicle-deck semantics without changing every caller of `valid_move`.                                       |

Two distinctions must remain visible throughout implementation:

1. Part `location` and drawing `z_order` are slots/priorities **within** a mount. They are not a deck number.
2. A structural connection, a walking connection, and a floor/ceiling boundary are different relationships. A pillar does not imply stairs; breaking stairs does not inherently detach the upper deck.

## Geometry and Persistence

### Coordinate Invariants

For an installed part:

```text
world_xy = vehicle_origin_xy + yaw_transform(local_mount_xy, horizontal_pivot)
world_z  = vehicle_origin_z + local_mount_z + terrain_offset(rotated_xy)
```

- Local mount z is permanent geometry; world origin z is placement; `z_terrain` is placement deformation.
- Steering pivots are horizontal. New pivot values must not introduce an independent vertical subtraction into only some position functions.
- Nominal mount-to-world conversion preserves local z but does not pretend to include an installed part's terrain offset.
- Actual part interactions use physical-position accessors. A generic inverse yaw is not enough to recover a mount on a ramp.
- Hypothetical installation positions obtain the terrain offset for their XY column using the same rule as installed parts. A preview and the eventual installation must agree.
- Parts at the same local XY receive the same terrain offset. Vertically aligned decks therefore remain separated by their local z difference.
- XY-only calculations remain XY-only when that is their meaning: steering, horizontal footprint, and projected wheel support are not automatically 3D volumes.

Do not fix `rotate_to_world` by assigning a z expression before deciding the pivot contract. The current physical-position formula does not subtract pivot z, so a local change there can create two incompatible transforms.

### Ordinary Reference Changes

Use an XY-only local rebase for ordinary reference-point replacement in the milestone. The vertical datum remains separate, and a surviving upper-deck frame need not be forced to local z=0.

Shift local XY mounts, label/zone keys, and both yaw anchors coherently while preserving local z, committed terrain offsets, physical part positions, and the world origin. Recognize an upper-deck part in the local origin column without repeatedly rebasing just because no part occupies local `(0, 0, 0)`. This is not the full-XYZ fragment placement operation owned by `fix/vehicle-split-cleanup`.

### Save Format

Add optional integer `mount_dz` to vehicle-part save data:

```json
{
  "mount_dx": 2,
  "mount_dy": 1,
  "mount_dz": 1,
  "z_offset": -1
}
```

This is an illustrative fragment, not a complete saved part.

- Missing `mount_dz` means zero, preserving old planar saves.
- The new field uses canonical integer tokens. Decimal/exponent spellings are rejected. Its magnitude is bounded by twice the world's vertical span. This is a defensive local-coordinate serialization bound, not complete world-placement validation or a dependency on generic split reconstruction.
- Existing `z_offset` continues to mean terrain displacement. It must never be migrated into a permanent deck coordinate.
- Validate integral coordinates and the resulting world envelope before indexing level arrays. Do not silently clamp invalid decks into another level.
- Explicit prototype `z` and exported prototype `z` must round-trip to the same mount.
- Preserve current readers for old pivot/auxiliary coordinate forms where they already exist. Audit persisted labels and zones separately; a generic tripoint reader need not accept an old two-element array.
- New transient stair links and connector-search snapshots are not serialized. Existing Character/monster coordinate-route serialization need not change; restored steps remain untrusted and must be revalidated before execution. Preserve destination/activity intent when a restored route needs replanning.
- New code reads old saves. Old binaries are not expected to load multi-deck saves correctly; document that downgrade limitation.

## Structural and Spatial Integration

### Structural Connectivity

Define one local structural-neighbor rule used by installation, legal removal, damage splitting, and prototype validation. The initial rule permits eligible structural frames at offsets east, west, north, south, above, and below.

Retain existing same-mount part-slot and dependency checks. Extendable parts, rack attachments, and nonstructural accessories do not acquire arbitrary vertical attachment merely because structural frames do.

Discover all connected components without a hard-coded count. Planned removal that disconnects required structure remains rejected according to existing policy; damage can create separate vehicles. Stair availability is not a structural edge: a broken stair must not be the reason an otherwise connected bus splits.

P2 tests the shared six-neighbor graph, component membership, and removal of the four-component cap. Generic arbitrary-yaw fragment placement and ownership/connection repair remain separate cleanup work, not an additional P2 exit gate.

Forced prototype installation is not an acceptance test for buildability. Tests must also construct a second deck through normal mounting validation.

### Ownership and Indexing

Keep one `unique_ptr<vehicle>` in its owning submap. A separate occupancy view identifies every world tile and level receiving a part contribution.

Do not populate every occupied level's existing `vehicle_list` without separating scheduler membership: `map::vehmove` currently iterates those lists. Verify exactly one turn's movement, fuel/environmental processing, and turret processing per vehicle.

On a geometry or placement change:

1. Record the old occupied cells and affected floor/ceiling boundaries.
2. Compute proposed positions and validate the full world-z envelope and required resident map data.
3. Resolve collisions before committing the movement.
4. Commit ownership and placement once.
5. Refresh local occupancy and the absolute mapbuffer footprint.
6. Invalidate the union of old/new affected levels and relevant adjacent boundary levels.
7. Invalidate routing data before another actor plans or executes a dependent movement.

Rebuilding a level's derived caches must include all vehicles contributing to that level, regardless of their owner z. An independently dirtied upper deck must not disappear merely because the lower owner level was clean.

At a bubble boundary, do not dereference missing submaps or trust an origin-only bounds check. Routing uses the current simulated domain and does not load/generate distant terrain. Vehicle physics must use an explicit residency policy consistently; an unavailable destination blocks/defer movement rather than committing part of it. Retain an owner while its vehicle contributes to an actively simulated footprint, or otherwise establish an equivalent safe lifetime rule.

### Support and Collision

- External support checks exclude all parts of the moving vehicle. Preserve the existing simplified "any valid external support" policy rather than adding tipping physics.
- Horizontal and vertical collision response uses movement/contact direction, not equality of owner z.
- Collision tests include upper-deck walls, overhangs, and crossed floor/ceiling interfaces. Test rotations and ramp transitions, not just endpoint translation on flat ground.
- Vehicle stair flags never imply `NOCOLLIDEBELOW`, `NOCOLLIDEABOVE`, or immunity to another deck's collision.
- Updating center-of-mass height must not accidentally change wheel balance from an XY support test into "COM must be inside the wheels' z plane". Rollover behavior is outside this milestone.
- Unsupported multi-deck watercraft/aircraft/rail/automation cases need explicit boundaries. In particular, counting all occupied voxels for flotation is not a suitable future hull model.

## Floors and Enclosure

A working boardable deck surface provides walking support at its own tile and contributes a ceiling boundary immediately below. Existing same-mount `ROOF` parts continue to represent an overhead boundary above their occupants; they are not automatically moved to the next deck.

For the milestone, use the existing `BOARDABLE` floor convention together with usable structural support. Keep the enclosure algorithm local: expand its effective-overhead-cover query to recognize a working upper deck instead of introducing a general 3D room flood fill.

Establish shared queries for these different purposes rather than one ambiguous boolean:

- Walking support, including usable vehicle decks over terrain `NO_FLOOR`.
- Physical separation between levels for reach, projectiles, and falling.
- Effective overhead cover for shelter and visibility.
- An intentional, currently valid stair crossing.

Relevant consumers include `map::has_floor`, mapbuffer movement checks, `vehicle_vertical_barrier_between`, `physical_clear_path`, vehicle floor/opacity caches, `vehicle::refresh_insides`, and ballistic floor checks. Do not assume changing the rendering floor cache changes all physical behavior.

An intentional stair crossing can bypass the paired landing's ordinary vertical walking barrier. It does not remove that barrier from unrelated physical queries. Other terrain, furniture, roofs, and vehicles remain obstacles.

### Walking on a Ramp

Preserving vertical column spacing is not sufficient for passenger movement. On a ramp, neighboring XY columns can have different `z_terrain` values:

- Adjacent surfaces on the **same local deck** can require both XY and world-z movement.
- Surfaces on **different local decks** can become horizontally adjacent at the same world z.

Introduce a shared movement-specific vehicle-surface step query alongside the existing walking helpers. For a candidate horizontal step, inspect supported destination parts at the candidate world XY and current z or one level above/below. For movement within the same vehicle, retain only destinations on the source's local deck, then apply existing rotated-vehicle adjacency, diagonal obstruction, clearance, and actor movement rules. Do not replace those rules with a naive XY-only or six-neighbor test.

A same-deck slope step is an ordinary supported walk, not a stair crossing. It uses the normal walking cost. A different-local-deck destination requires an explicit stair link even if the world coordinates happen to be horizontally adjacent. Exiting to terrain or another vehicle continues to use the existing boarding/unboarding and physical-clearance rules; this must not become a way to jump a deck boundary within one vehicle.

Physical separation must follow the same local geometry. A solid floor/roof boundary between decks can project onto a side or diagonal boundary between world cells when neighboring columns have different heights. Sight, physical reach, and projectiles must check that boundary even when the segment's endpoints have equal world z. The current equal-z early exit in `physical_floor_between` and z-change-only ballistic floor checks are audit targets, not adequate implementations of this contract.

Share the projected boundary decision with the relevant physical-path and visibility consumers, including CPU/GPU cache representations. Do not mark an entire endpoint tile opaque merely to block one crossing, and do not block a path solely because two parts have different mount z: a real working floor/roof boundary must separate them. Preserve legitimate exterior window sight and paths around missing/destroyed cover. Test local-deck separation independently of the movement-specific surface-step query.

Both planners and avatar/NPC/monster execution must use the same query. Expose valid same-deck height changes as short-lived vehicle-surface edges with their exact endpoints. Keep this edge kind distinct from terrain ramps: do not remove its source from reconstruction or search for a terrain `RAMP` flag beneath an upper deck.

Avatar manual horizontal movement can resolve the height-adjusted destination in the same place that ramp movement adjusts ordinary walking. For automove, a validated vehicle-surface step with XY and z displacement becomes the corresponding horizontal action, which resolves and checks the same destination again. Other mixed-axis steps retain their existing rejection unless independently supported. No general diagonal teleport action is introduced.

M1 requires stopped, mid-ramp tests for walking along each deck, reaching/using stairs, and rejecting same-world-z shortcuts between decks. If a projected step exceeds supported adjacency/one-level height, return no walking edge rather than teleporting. Seated transport and vehicle motion remain separate from the walking query.

## Vehicle Stair Assembly

### Content Contract

The following identifiers are **proposed additions**, not existing flags or parts:

| Property               | Lower stairs                               | Upper landing                              |
| ---------------------- | ------------------------------------------ | ------------------------------------------ |
| Part ID                | `vehicle_stairs`                           | `vehicle_stair_landing`                    |
| Capability flag        | `VEHICLE_STAIRS_UP`                        | `VEHICLE_STAIRS_DOWN`                      |
| Local mount            | `(x, y, z)`                                | `(x, y, z + 1)`                            |
| Slot                   | `center`                                   | `center`                                   |
| Other flags            | `AISLE`, `BOARDABLE`                       | `AISLE`, `BOARDABLE`                       |
| Support                | Usable structural frame at the same mount  | Usable structural frame at the same mount  |
| Direction              | Upward departure and downward arrival      | Downward departure and upward arrival      |
| Operation              | Passive; no power or `enabled` requirement | Passive; no power or `enabled` requirement |
| Suggested ASCII symbol | `<`                                        | `>`                                        |

At this map scale the lower part represents a compact stair flight rising within its column, not a ladder requiring climbing skill. The landing is its explicit upper endpoint. Same-column geometry remains exactly one world level apart under yaw and the existing shared-column ramp model.

Implement the part definitions in a proposed `data/json/vehicleparts/stairs.json`, with flag descriptions in `data/json/vehicleparts/vp_flags.json`. Add/installable component items under `data/json/items/vehicle/` and crafting recipes under the appropriate `data/json/recipes/other/` category. Final mass, durability, material quantities, and crafting balance are content review items, not new movement semantics. Reuse normal metal-part installation/removal/repair requirements and ensure neither endpoint is debug-only.

Use existing aisle/floor graphics as a fallback until dedicated art exists. Names, descriptions, action labels, and failure messages must be translatable. Do not rename the existing `ladder_3` rope part or reinterpret its `LADDER` flag.

### Installation and Removal

- Each endpoint is installed and consumes materials independently through the normal vehicle work activity. Installing one must not silently create the other.
- An unpaired endpoint is allowed as a construction intermediate, but its status clearly says it is not usable yet.
- Do not require the opposite endpoint for installing the first one; that would create a circular construction prerequisite.
- A generic upper aisle does not complete a pair. The `center` slot prevents stacking the landing with a seat/cargo/aisle occupying that slot.
- Reject a lower stair under a `ROOF` or `on_roof` assembly at its own mount. Also reject later installation of that assembly over existing lower stairs.
- An upper-deck roof is allowed above the landing. It is not the boundary between the two endpoints.
- For M1, a broken-but-installed lower roof assembly still blocks the connection until removed. This avoids an inconsistency with current `roof_at_part` behavior.
- Stair and landing endpoints cannot both occupy one center slot. Connecting three decks uses separate stair columns on the intermediate deck; a combined up/down part is deferred.
- Removing either endpoint is allowed when ordinary dependency/removal checks allow it. The remaining endpoint becomes unusable immediately.
- Revalidate activity targets and structural legality before consuming installation materials or removing a part. A delayed activity must not rely on the state when its menu opened.

### Pair Resolution

Resolve a link from installed parts, not from nearby terrain stairs:

1. Find the actual endpoint part at the requested world tile.
2. Read its full local mount and direction capability.
3. Search the exact adjacent local mount on the **same vehicle** for the complementary endpoint.
4. Verify both endpoint parts and supporting frames are available, unbroken, and not carried/racked.
5. Compute both actual world endpoints with `abs_part_location` or `bub_part_location`.
6. Require equal world XY and a world z difference of exactly one.
7. Validate simulated-domain membership, world bounds, stationary state, and physical clearance.

Do not resolve endpoints through the current `mount_to_abs` or `abs_to_mount` without first fixing their contract. Do not use nearest-stair matching, overmap-tile searches, rope length, or any landing from another vehicle.

### Traversability and Occupancy

Keep three results distinguishable:

| Result            | Meaning                                                                          | Caller behavior                                                                         |
| ----------------- | -------------------------------------------------------------------------------- | --------------------------------------------------------------------------------------- |
| No endpoint       | No vehicle stair endpoint exists here in the requested direction.                | Consider ordinary terrain/rope behavior only if that transition is independently valid. |
| Unusable endpoint | Endpoint exists, but pairing, health, motion, geometry, or clearance is invalid. | Stop with a reason; do not fall back to forced movement or a nearby terrain stair.      |
| Valid link        | The physical pair is usable for the actor's capabilities.                        | Apply route avoidance/occupancy policy; recheck immediately before moving.              |

Neither endpoint may be inside impassable terrain/furniture or an unrelated vehicle. An intervening terrain floor blocks the connection even if the vehicle's parts happen to overlap it. The intentional exception applies only to the paired vehicle's own required landing assembly, not arbitrary colliding parts.

Resolve overlap conservatively. One representative returned by `veh_at` does not prove that another vehicle is absent from the footprint. If required, add a narrowly scoped occupancy-conflict query over the existing footprint data rather than treating a valid stair flag as permission to ignore overlaps.

Creature occupancy is transient, not structural topology. A planner can avoid or penalize it according to its existing settings. Execution always checks both creature presence and boardable passenger state. Never call `board_vehicle` to evict an occupant: that function's current occupied-seat recovery is not a traversal policy.

For M1, avatar automove stops at an occupied landing; NPCs wait/replan using their existing movement loop. No cross-deck pushing, swapping, or melee is performed as a side effect of a blocked stair step.

### Physical Boundary Policy

The stairwell is a supported passage abstraction. Its upper landing still supplies support and the ordinary cross-deck sight/attack barrier. Traversal is possible because the stair action explicitly validates the assembly, not because the map becomes open air.

Do not globally exempt stairs in `mapbuffer::valid_move`. That API is also used by falling, explosions, scent, and climbing. Prefer a dedicated stair validator. If a shared lower-level helper is needed, make intentional traversal an explicit purpose with the default physical behavior unchanged; do not overload `flying`, `bash`, or `via_ramp`.

Using, installing, or moving stairs must not modify underlying terrain or furniture. No `GOES_UP`/`GOES_DOWN` terrain is written under a moving bus.

## Shared Traversal Interface

Put the focused connector logic in proposed `src/vehicle/vehicle_stairs.h` and `.cpp`. Keep actor-specific effects in the actor movement code. Do not add a large new framework or a persistent vehicle identity system solely for M1 stairs.

The interface needs three responsibilities; names below are descriptive proposals:

| Responsibility            | Inputs                                                 | Result                                                                     |
| ------------------------- | ------------------------------------------------------ | -------------------------------------------------------------------------- |
| Resolve an incident stair | Map/dimension context, world source, direction         | No endpoint, or a resolved lower/upper pair with availability information. |
| Validate a crossing       | Current source/destination and actor/movement policy   | A usable crossing or a specific failure reason.                            |
| Enumerate routing edges   | Current simulated map and stair-capable route settings | A short-lived set of exact directed world endpoints and transition costs.  |

A resolved pair may contain part indices/references only for the duration of synchronous validation/execution. Part indices change during removal, mounts change during rebasing, and vehicles can unload. Do not retain raw pointers or part indices in a long-lived path.

Use typed coordinates, a named result/options structure, and `std::optional`/`std::expected` where appropriate. More than three parameters should use an options structure, consistent with the repository's C++ conventions. Do not introduce a tuple of loosely related coordinates and status flags.

### Route Lifetime

Existing coordinate route vectors can remain for this milestone:

- A route is not authorization to move. Before any vehicle-related z step, resolve a currently valid link with exactly the actor's current source and the requested destination.
- This intentionally does not promise use of the same physical stair part that existed at planning time. A replacement is acceptable only if it now forms an independently valid crossing between those exact endpoints.
- If the vehicle moved, the actor moved, or the destination changed, reject/cancel/replan rather than teleporting to the saved coordinate.
- If no vehicle link exists, any terrain transition must be independently validated. Never let a stale vehicle step fall into an unconditional NPC vertical-move branch.
- Manual movement from a stopped vehicle's new position can resolve a new link immediately. Automove goals remain world-fixed and may need to be reissued; they do not silently follow the vehicle.
- Rebuild connector snapshots after load/unload or dimension changes. Revalidate restored coordinate paths and replan/cancel invalid steps while preserving existing destination/activity semantics. No new saved connector ID is required.

If future moving-stair travel requires persistent vehicle-relative goals or explicit route-step actions, introduce them as a separately justified extension. Do not preemptively rewrite every coordinate-route consumer for the stationary milestone.

## Actor Execution

### Avatar

Integrate vehicle stairs into the non-forced intentional vertical-action path used by `handle_action` and `game::vertical_move`:

1. Preserve actively piloted aircraft/control behavior. A driver must release controls before using the stairs; do not unboard them as a side effect of `<` or `>`.
2. Apply movement-effect, mounted, and grabbing restrictions.
3. Resolve the vehicle stair in the requested direction before rope or terrain-stair fallback.
4. Validate the exact endpoint, stationarity, support, clearance, and destination occupancy.
5. Commit a normal movement/boarding transition and charge the crossing cost once.
6. Update map/z state, sight, movement events/hooks, fields/traps, and automove expectations through production bookkeeping.

Factor only the necessary shared movement bookkeeping. Do not call `find_or_make_stairs`, relocate to an overmap-matched terrain stair, or implement the feature as `vertical_move(..., true)` followed by a bare `setpos`.

Same-column steps fit the current avatar automove restriction that a z step has no XY displacement. The path must retain both stair endpoints; it is not a ramp step whose source can be removed from the route.

Same-deck vehicle-surface slope steps are the separate mixed-axis case specified above. Their automove direction adaptation must occur before the current generic mixed-axis rejection, and must require fresh surface-step validation.

### NPCs

The acceptance path is `npc::update_path` to repeated `move_to_next` calls, not a test that directly places an NPC upstairs.

Validate a z transition before cross-z occupant interactions and before `npc::move_to` reaches its current teleport-like vertical branch. If a planned vehicle crossing is invalid, wait or clear/replan. A vertical delta alone is never sufficient authorization.

Use the existing movement-state-preserving position operation and field/trap processing. Source and destination boarding state must agree after success, and must both remain unchanged on a rejected crossing. Guard before mutation rather than trying to recover after `board_vehicle` discovers an occupied landing.

Follower seat selection must not merge seats on different decks by XY or let a few unreachable upper seats exclude all reachable lower seats. A follower should be able to reach an avatar or suitable seat on the other deck of a stopped vehicle.

### Monsters

Both monster route generation and movement validation must recognize stairs. Relevant execution gates include `monster::can_reach_to`, vertical-step decisions, and `monster::move_to` in `src/monmove.cpp`.

Require the existing stair capability; do not grant all monsters climbing or flying. Do not use vehicle stairs to permit attacks through the closed floor boundary. Keep route calculation and mutable pathfinding caches on the appropriate main-thread execution path, not parallel decision code.

Apply live physical-boundary validation to **non-stair** vertical movement too. Current open-air flight transitions and `monster::can_reach_to` can rely on terrain `NO_FLOOR` without accounting for a vehicle deck. A flying monster must not bypass an ordinary floor, broken stair, or orphan landing through that alternative path. Terrain stairs, ramps, and flight retain their own explicit exceptions, not the vehicle-stair exception.

Monsters do not use Character seat/passenger bookkeeping. M1 tests their travel on a stationary vehicle only and does not introduce a new monster passenger-transport model.

### Crossing Transaction

All actor adapters must establish the same externally visible postconditions:

- Failure leaves position, source/destination passenger ownership, and vehicle-control state unchanged. Normal waiting/failure turn costs may still apply.
- Success moves to the validated endpoint and charges one base 100-move crossing, with existing actor-specific movement restrictions/effects applied deliberately.
- For a Character, exactly the source boarding record is cleared and the destination record is assigned to that Character.
- The new position's fields/traps/hooks are processed in the same order as the chosen normal movement path. No second, hidden movement charge is applied.
- No partial world mutation is exposed between validation and commit. These operations run synchronously, not as a queued callback holding part pointers.

## Pathfinding

### Actual Consumers

| Consumer                            | Current route path                                                   | Required integration                                                |
| ----------------------------------- | -------------------------------------------------------------------- | ------------------------------------------------------------------- |
| Avatar automove and activity travel | `map::route` in `src/map/legacy_pathfinding.cpp`                     | Explicit vehicle-stair neighbor expansion and executable endpoints. |
| NPC `update_path`                   | `map::route`, regardless of the monster pathfinding option           | Same legacy expansion plus NPC execution validation.                |
| Monster main repathing              | Legacy or `Pathfinding::route` according to `USE_LEGACY_PATHFINDING` | Test both modes.                                                    |
| Other monster routes                | Some still call `map::route` directly                                | Preserve the common legacy route behavior.                          |

Updating only `src/pathfinding.cpp` does not enable NPC or avatar routing.

### Walkable Decks

First make route admissibility agree with actual vehicle walking support. The newer pathfinder currently rejects terrain `NO_FLOOR` for non-fliers even when a vehicle deck occupies the tile. NPC and monster support checks also need the same distinction.

Use a movement-specific live support query for a working deck. Do not globally redefine terrain `has_floor` or make every vehicle part walkable. Treat an underlying ledge trap as covered when the valid deck supports the actor, but preserve unrelated fields/traps and ordinary obstacle rules.

### Legacy A-Star

At each expanded node, consider validated vehicle-stair edges independently of the existing terrain-only `PF_UPDOWN` gate. For each eligible edge:

1. Require stair permission and a destination within the route's loaded bounds.
2. Check exact pairing, physical validity, stationary state, avoidance, and endpoint passability.
3. Add the destination using the current source node's accumulated cost and the crossing cost.
4. Retain the exact source/destination relationship for route reconstruction through ordinary parent coordinates.

Do not use `vertical_move_destination`, which searches for matching terrain stairs rather than the paired vehicle landing. Do not copy a destination-layer score expression when the accumulated score belongs to the source layer.

Legacy flat walking is normally two cost units; use two units for the base 100-move stair crossing. Preserve existing route limits and the convention that legacy routes omit the starting tile.

Also expand validated same-deck vehicle-surface height steps with their normal walking cost. Ordinary same-world-z expansion must reject a shortcut between different local decks of the same vehicle. Neither surface walking nor a missing stair grants permission to ignore a physical floor during another kind of z transition.

### New Pathfinder

Keep terrain transition caches separate from a fresh, route-local vehicle-stair snapshot. Do not append moving vehicle edges permanently to `z_caches` or `cached_closest_z_changes` without invalidation.

The current `get_route_3d` chooses transitions geometrically and gives up if the selected approach is unreachable. M1 must support a second reachable stair when the nearest stair is blocked.

For routes involving vehicle stairs, use a small connector search over the existing 2D path segments:

1. Collect valid vehicle stair/surface edges and relevant existing terrain stairs/ramps within the original route domain. Deduplicate vehicles by identity while collecting them. Do not materialize every open-air flight transition as a graph node.
2. Represent source, goal, and transition endpoints as graph nodes. Evaluate same-level connections lazily with the existing 2D pathfinder, preserving its actor settings.
3. Add directed stair edges with one normalized flat-step cost, scaled by the existing movement coefficient. Vehicle-surface steps use normal walking cost. Keep terrain/ramp costs and reconstruction rules distinct.
4. Search reachable alternatives using segment costs plus transition costs, not only distance to a landing. A blocked approach must not suppress another candidate.
5. Reconstruct coordinate segments with both stair endpoints, removing only duplicate joins. Preserve the newer route convention that includes the start.
6. Apply the original request's search domain, step/cost limits, and stair capability to the complete route, not independently relaxed limits for each segment.

For terrain transitions, M1 may retain the current monotonic progress toward target z. Vehicle-surface edges must be permitted to rise and descend within one local deck, including a same-world-z request whose direct 2D route fails. That bounded vehicle case is required to walk a deformed deck, not a promise of arbitrary terrain z detours. Handle cycles with normal visited/best-cost state. Support multiple stair columns and chained adjacent deck connections.

Retain specialized open-air candidate handling for fliers, but filter its candidates and their execution through live vehicle-boundary checks. A cached terrain-only flight candidate cannot authorize movement through a newly installed floor or landing.

A zero-length approach or departure still requires endpoint validation. An empty segment can mean failure; a trivial segment is not proof that an endpoint is passable. Use deterministic settings in tests and compare actual segment cost rather than blindly counting vector entries.

Bound work by the existing route domain and limits. Enumerate installed stair parts and actual surface discontinuities, not every map tile or every possible deck cell. Do not load submaps to discover a landing. Reuse the existing 2D cost machinery rather than calculating a second inconsistent movement model.

### Search Budget

Spatial bounds and final route length do not bound the cost of trying many unreachable connectors. The current newer pathfinder can retain a full-bubble cost map per destination/settings combination, so a naive all-pairs connector search can multiply both time and memory.

Use a request-wide work budget and bounded scratch storage. Initial engineering limits for measurement are 64 segment-search attempts, eight additional live 2D cost maps, and a total node-expansion budget equivalent to four full single-level search domains. These are internal limits, not saved/gameplay settings; tune them using the fixture and a many-candidate benchmark before merging. Count work across all attempted segments, not just the chosen route.

Reuse/release request-owned scratch maps within that storage cap. Do not retain every speculative destination in the global `d_maps` cache or its allocation pool. Keep chosen segment coordinates/costs only as needed to reconstruct a valid result. Existing unrelated cached maps are not cleared merely to satisfy this request's cap.

On exhaustion, return a fully validated route already found, if any, otherwise no route and normal caller wait/retry behavior. Never return a partial path presented as reaching the requested goal. Record exhaustion through existing debug facilities. Do not materialize per-tile open-air nodes; preserve their specialized search strategy. Add instrumentation-backed tests for bounded segment attempts/map allocation and an unreachable many-connector fixture.

### Dynamic Changes

Vehicle transitions are resolved afresh per route; execution revalidates again. Cached 2D costs and chosen terrain transitions still require appropriate invalidation.

| Event                                                | Required response                                                                                     |
| ---------------------------------------------------- | ----------------------------------------------------------------------------------------------------- |
| Translation, yaw, or ramp displacement               | Update physical footprint; invalidate affected old/new deck routing data before another route query.  |
| Motion starts or stops                               | Route-local stair eligibility reflects current stationary state, even if the position did not change. |
| Endpoint/frame install, remove, break, or repair     | Update availability, walking support, boundaries, and routing immediately.                            |
| Door/obstacle changes                                | Refresh endpoint and approach passability; existing paths are still checked at execution.             |
| Split, fold/rack attempt, destruction, or unregister | Remove obsolete spatial contributions; reject stale steps and prohibited operations.                  |
| Terrain/furniture changes                            | Reevaluate physical clearance and cached 2D/terrain-transition data.                                  |
| Bubble shift, dimension change, or load              | Rebuild/rekey transient data for the new map context; no pointer-based connector survives.            |

`Pathfinding::clear_d_maps` does not itself rebuild all z-transition caches. Audit `mark_dirty_z_cache`, `on_vehicle_moved`, `set_pathfinding_cache_dirty`, and mutation hooks explicitly. End-of-turn clearing alone does not cover two route queries separated by a vehicle mutation in the same turn.

## Construction and Presentation

### Deck Controls and Bindings

Place a compact deck-control strip next to the construction preview, visible in both ASCII and tile modes. It shows the signed **local** deck, higher/lower actions with their current key descriptions, and the lower-reference status. With default bindings it could read:

```text
[< / PgUp] Deck up    Deck +1    [> / PgDn] Deck down
Below: deck +0 (reference only)
```

This is a layout illustration, not literal key text to embed in code. Get displayed hints from the same binding resolution used for dispatch. At narrow terminal widths, abbreviate/wrap the control strip without overlapping the part list or hiding the selected deck. Reserve its space in `allocate_windows`; recompute hit areas on resize and keep the tile clip rectangle below it.

Reuse the existing `LEVEL_UP` and `LEVEL_DOWN` action IDs. `action.cpp` maps gameplay stair movement to these IDs, and `data/raw/keybindings/keybindings.json` defines the global defaults as `<`/Page Up and `>`/Page Down. Register them explicitly: `register_directions()` does not supply deck changes.

Binding behavior must be precise:

- In the normal case, deck navigation follows the player's effective gameplay stair bindings, including a customization local to `DEFAULTMODE`.
- An explicitly chosen `VEH_INTERACT` override may take precedence; do not ship a duplicate editor-only key list that silently stops tracking the gameplay defaults.
- The current input manager falls back from a context to the global default, not from `VEH_INTERACT` through `DEFAULTMODE`. Add narrowly scoped per-action fallback/alias support if needed so dispatch, displayed hints, conflict reporting, and keybinding help agree. Merely registering the same ID is not enough to inherit a gameplay-context override.
- Preserve keyboard-only operation. Make the visible higher/lower controls clickable where the UI backend supports mouse input; a click dispatches the same action as the key, not a separate state mutation.
- Deck actions only browse the editor. They do not move the avatar, use stairs, change the map's active z, consume moves, or require a stair assembly to exist.

There is an existing conflict that implementation must resolve: `DESC_LIST_UP` and `DESC_LIST_DOWN` in `VEH_INTERACT` currently use `<` and `>`. Move their shipped defaults to `,` and `.` after checking the full active context, leaving description scrolling available with displayed hints. Do not rely on registration order to choose which of two actions receives a key.

Preserve customized bindings. Detect a user-specific conflict through the input system and provide a visible resolution path; do not silently overwrite the user's global/gameplay settings. If a conflict remains, clickable controls still work, help reports it, and one physical event must never both scroll descriptions and change decks. Existing install/repair submenus, filters, and text entry retain their focus rules; a deck key is not intercepted while it is being entered as text.

### Navigation State

Keep `vehicle_cursor.z()` as the selected local deck rather than maintaining a second independent deck selection.

1. Enter Examine at the full mount of the interacted part. Reopening after work restores its full local cursor.
2. On a deck action, change z by exactly one while preserving local XY, preview orientation, pan/centering policy, and zoom.
3. Use the normal `move_cursor`/selection refresh path to update `cpart`, `parts_here`, installation candidates, obstruction status, descriptions, and affected list paging before redraw.
4. Invalidate the preview and related panes once the state is coherent. Neither preview backend may write back a new selected part during painting.
5. A failed/out-of-range deck action leaves the complete previous selection unchanged and reports the limit.

Permit browsing every occupied local deck and one empty deck beyond the current lowest/highest occupied mount, subject to world-height constraints. Include empty levels between occupied decks. Compute this range from actual nonremoved mounts; existing mount bounds currently emphasize XY and cannot be assumed to contain correct z extrema. A successfully installed extension expands the range, so constructing another deck does not require selecting a pre-existing upper part.

An empty selected tile has no `cpart`, even if the reference shadow below contains a frame or seat. Installing above it still consults live structural/clearance rules; the shadow neither authorizes installation nor counts as an obstruction. If removal changes the browse range, reconcile selection through the same refresh path, not during drawing.

Show world z at the cursor as secondary information if useful, but label it separately. On a ramp, one local deck need not occupy a single world level. The editor selects local construction geometry, not a world-z slice.

### Lower-Deck Shadow

The reference layer is the immediately lower local deck, `selected_z - 1`, aligned by the same local XY-to-preview transform and zoom as the selected deck. Do not search for the nearest nonempty deck or use world z to choose it. If the lower deck is empty or outside the available range, omit the shadow and show that there is no immediate lower reference.

Build a simplified footprint from nonremoved structural/standalone mounts on that lower deck, one reference cell per local XY. Include damaged-but-present structure for construction orientation; exclude removed parts. Do not render every seat, cargo item, roof stack, label, or part paint color in the reference layer.

- In ASCII, use a low-emphasis neutral marker such as `:` for a reference footprint cell. Its shape as well as its subdued color distinguishes it from active part glyphs.
- In tiles, use a simple muted outline/silhouette per footprint cell, not a second fully detailed vehicle render. Prefer existing primitive/overlay drawing; dedicated tileset art or a new rendering pipeline is not required.
- Draw reference cells only where the selected deck has no displayed part. Draw active parts, active highlights, obstruction indicators, and the cursor afterward with normal prominence.
- Label the reference as lower-deck information. The user must not have to infer the distinction from color alone.
- Reference cells are never selectable, highlighted as active parts, used for mouse hit-testing of parts, or included in active part lists, descriptions, repair targets, and removal targets.
- Keep any rendering tint/clip state local to the preview; do not modify global tileset textures, world visibility, vehicle caches, or the lower parts themselves to create the effect.

The shadow is on when a lower reference exists; M1 does not need a persisted global setting or an additional hotkey to enable it. A future display toggle can be added independently.

Both backends should consume the same active-deck and reference-footprint selection rules, with separate rendering styles. For M1, deriving these small lists per redraw is preferable to another cache with invalidation obligations. If profiling later justifies caching, key it by selected deck and vehicle topology, and invalidate on install/remove/split/rebase.

### Other Presentation Rules

- Mark active lower/upper stair endpoints and report incomplete, blocked, damaged, or moving status. The overview identifies decks for otherwise identical components and can switch the cursor to the selected entry's full mount.
- Preserve current activity cursor-z persistence and reopen on the same local deck after completed work. Revalidate the live target before modifying anything.
- Continue existing work-resource/reach rules explicitly. Do not accidentally add physical same-deck reach constraints that make the first upper frame impossible to build.
- Make world roof cutaway and visibility depend on the viewed/occupied deck, not merely whether the avatar is anywhere in the same vehicle. The construction shadow is independent of world lighting/roof cutaway.
- Limit ordinary multipart-door grouping to its intended mount z. Same-type doors directly above each other must not form one horizontal door run.

## Implementation Slices

Each slice includes its tests and should be reviewable without claiming the entire milestone is complete.

| Slice                    | Deliverable and primary files                                                                                  | Exit condition                                                                                                                                            |
| ------------------------ | -------------------------------------------------------------------------------------------------------------- | --------------------------------------------------------------------------------------------------------------------------------------------------------- |
| P1: geometry/persistence | `vehicle.cpp`, `vehicle_part.cpp`, `savegame_json.cpp`, `vehicle_export.cpp`, coordinate documentation.        | A forced two-deck fixture survives conversion, terrain-buffer copy/move, save/load/export, and ordinary XY rebasing; wheel balance remains horizontal.    |
| P2: structure            | Shared six-neighbor adjacency in installation/removal/connectivity and split component discovery.              | The fixture can be built normally; vertical articulation and more-than-four-component discovery tests pass, without a generic fragment-preservation gate. |
| P3: spatial safety       | `map/map.cpp`, `map/mapbuffer.cpp`, `vehicle_move.cpp`, floor/physical-path consumers.                         | Both decks move/collide/fall correctly, receive one simulation update, and leave no stale cache contributions.                                            |
| P4: stair contract       | New `vehicle_stairs` module, flag/content definitions, normal work validation.                                 | Pair resolution, physical boundaries, costs, occupancy, and stationary eligibility are covered without actor teleport shortcuts.                          |
| P5: traversal/routing    | `handle_action.cpp`, `game.cpp`, `npcmove.cpp`, `monmove.cpp`, both pathfinders.                               | Manual and routed travel works in both directions; stale-path and alternate-stair tests pass.                                                             |
| P6: construction/views   | `veh_interact`, `vehicle_preview`, input binding resolution/defaults, vehicle display and deck-local grouping. | Shared stair bindings, visible deck controls, lower-deck shadow, and real action-to-redraw tests work without cross-deck selection leaks.                 |
| P7: integration          | Test fixtures, bus-like explicit-z prototype, compatibility checks, user-facing documentation.                 | All M1 acceptance cases pass and unsupported combinations are explicitly bounded.                                                                         |

P1 precedes P2 and P3. P4 needs their geometry/support contracts. P5 needs P4. P6 can proceed after P1 but cannot claim normal construction works before P2. P7 depends on all of them.

These are feature dependencies only. `fix/vehicle-split-cleanup` consumes the P1 foundation; it is not a prerequisite for P2-P7 or an extra milestone slice.

P5 also touches `character.cpp` and `avatar_action.cpp` for the mixed-axis vehicle-surface automove/manual-walking adaptation. Its exit tests include stopped mid-ramp walking, not only the same-column stair action.

The exact patch boundaries may be smaller than this table. Avoid a single mass `point`-to-`tripoint` patch or committing a visually successful prototype before persistence and support are safe.

### Additional Source Audit

Before enabling normal multi-deck content, inspect these integration paths even if they are not part of the fixture:

- `vehicle::shift_parts` and `shift_if_needed` for ordinary XY reference changes; `find_and_split_vehicles` for P2's 3D component discovery, not generic fragment reconstruction.
- `vehicle::find_lines_of_parts` for cross-deck door/rack grouping.
- `vehicle_use.cpp` folding and rack metadata for explicit unsupported-shape rejection.
- Height constraints on newly enabled towing attachments; preserve supported lower-deck behavior without making generic towing reconnection cleanup a feature gate.
- `map/lightmap.cpp` cameras/mirrors and roof shadows for accidental XY projection onto another deck.
- `ballistics.cpp`, `vehicle/turret.cpp`, and automatic targeting for whole-vehicle immunity that bypasses an intervening deck.
- `vehicle_autodrive.cpp` for single-level obstacle plans; do not present multi-deck autodrive as supported without clearance-aware planning.
- Mapgen placement paths for upper-deck clearance and loading into otherwise empty upper submaps.

Do not suppress ordinary floors, visible parts, or player interactions to hide a cache/rendering problem. Fix the shared semantics or explicitly reject only the unsupported new operation.

## Test Fixtures

Create a deterministic small fixture with two 3-by-3 decks, enough structural connections to distinguish stair usability from structural support. Use identifiable seats/cargo at the same XY on both decks, one stair pair, lower wheels, and a lower control position. Put real open-air terrain beneath upper-deck tiles so terrain floors cannot accidentally make tests pass.

Use a second variant with two separated stair pairs and a blocked approach to the nearer pair. Use a small bus-like fixture for driving, rotation, ramps, clearance, and passenger integration. These can initially live in the test data rather than normal world spawn tables.

Buildability tests must use ordinary installation validation. Geometry/collision tests may use forced fixture assembly when they are not testing installation, but must state that distinction. Actor tests must use real bound game objects and movement entry points, not mocked stair lookup or direct final-position assignment.

## Test Matrix

| ID | Scenario                                                                             | Required assertion                                                                                                                                                    |
| -- | ------------------------------------------------------------------------------------ | --------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| G1 | Old planar save without `mount_dz`                                                   | All local z values remain zero; existing placement is unchanged.                                                                                                      |
| G2 | Multi-deck save with nonzero world origin and terrain offsets                        | Mount z and terrain z round-trip independently with cargo/labels/zones/passengers.                                                                                    |
| G3 | Part copy/move construction, assignment, vector growth/erase                         | Local mount and both terrain-offset buffers follow their explicit lifecycle policy.                                                                                   |
| G4 | All 24 yaw increments and a nonzero horizontal pivot                                 | Nominal transforms obey their contract; installed deck spacing remains constant.                                                                                      |
| G5 | Ordinary reference-frame removal and XY rebasing on a ramp, without splitting        | Local deck z, both yaw anchors, physical part/passenger positions, and world origin remain coherent; an upper origin column does not trigger repeated rebasing.       |
| G6 | Prototype export/reload                                                              | Explicit local z survives; grouping never re-fetches another deck by flattened XY.                                                                                    |
| G7 | Elevated load supported by ordinary lower-deck wheels                                | Balance uses projected XY center of mass and wheel support; COM height alone does not invalidate the wheels. No rollover model is implied.                            |
| S1 | First upper frame and same-slot parts at different z                                 | Legal vertical installation succeeds; duplicate same-XYZ slots remain rejected.                                                                                       |
| S2 | Removing the only structural connection versus an alternate connection               | Validation follows the shared graph, independently of stair health.                                                                                                   |
| S3 | Six-arm junction destroyed                                                           | All six structural components and their members are discovered; none is silently omitted by a four-component cap. Generic fragment placement is not this test's gate. |
| W1 | Two-deck fixture suspended over air                                                  | Its lower deck cannot count as external support for its upper deck.                                                                                                   |
| W2 | Upper-only wall/ceiling contact, turning, and ramp transition                        | Upper geometry participates in collision and remains inside world limits.                                                                                             |
| W3 | Horizontal collision across differing owner z; vertical collision with equal owner z | Response follows movement/contact direction, not origin comparison.                                                                                                   |
| W4 | Move, remove, or independently dirty only an upper contribution                      | Normal refresh restores correct floors, opacity, occupancy, and path costs without test-only invalidation.                                                            |
| W5 | Owner/footprint crosses a submap or bubble edge                                      | No duplicate simulation, lost overlapping footprint, missing-submap dereference, or stale pointer.                                                                    |
| W6 | Stopped vehicle straddles a ramp                                                     | Manual walking and both planners follow each local deck across world-z changes; same-world-z cross-deck shortcuts are rejected.                                       |
| C1 | Valid pair, orphan endpoint, wrong XY/z, different vehicles                          | Only the exact same-vehicle complementary pair creates edges.                                                                                                         |
| C2 | Passive endpoints with `enabled == false`                                            | Stairs remain usable if otherwise valid.                                                                                                                              |
| C3 | Broken/removed/carried endpoint or support                                           | Crossing fails with no actor/boarding mutation.                                                                                                                       |
| C4 | Roof before stairs and roof after stairs                                             | Both incompatible installation orders are rejected; an upper roof remains allowed.                                                                                    |
| C5 | Terrain floor, furniture, or overlapping vehicle                                     | The stair exception does not bypass unrelated physical obstacles.                                                                                                     |
| C6 | Moving/falling/vertically moving vehicle                                             | No stair crossing; already boarded occupants are still transported normally.                                                                                          |
| R1 | Legacy and new routes between decks in both directions                               | Exact endpoints occur in the result; upper open-air deck approaches are walkable.                                                                                     |
| R2 | Nearest stair unreachable, another reachable                                         | A route through the alternative is found under adequate search limits.                                                                                                |
| R3 | Source/goal exactly on an endpoint                                                   | Trivial approach/departure does not skip validation or duplicate costs.                                                                                               |
| R4 | Stair permission off, avoidance on, constrained search domain                        | Both planners honor capability, avoidance, and total route limits.                                                                                                    |
| R5 | Query, mutate vehicle/part, query again in one turn                                  | No stale dynamic stair or deck-cost result is reused.                                                                                                                 |
| R6 | Many mostly unreachable connectors, including flyer settings                         | Segment work and additional cost-map storage stay bounded; exhaustion never produces a false complete route or per-tile open-air graph.                               |
| R7 | Saved coordinate route restored after topology changes                               | Next-step validation rejects stale crossings while preserving/replanning destination/activity intent.                                                                 |
| A1 | Avatar manual `<`/`>` and real automove                                              | Position, map z, boarding records, effects, and one crossing charge are correct.                                                                                      |
| A2 | NPC `update_path` followed by `move_to_next`                                         | Follower reaches the other deck without unchecked z teleportation.                                                                                                    |
| A3 | Monster traversal with each routing option                                           | Stair-capable monsters plan and execute; incapable monsters do not gain stair access.                                                                                 |
| A4 | Move/destroy/unload stairs after planning                                            | Old route cannot authorize movement to stale endpoints or bypass a floor.                                                                                             |
| A5 | Occupied landing and opposing actors                                                 | No occupant eviction, stacking, cross-floor push/attack, or partial unboarding.                                                                                       |
| A6 | Mounted, grabbing, or controlling actor                                              | Restricted crossing fails without losing controls or invoking unintended aircraft behavior.                                                                           |
| B1 | Solid decks and supported stairwell                                                  | Sight, physical reach, ordinary bullets, and falling do not gain an unintended aperture.                                                                              |
| B2 | Stairs used and vehicle moved                                                        | Underlying terrain/furniture is unchanged.                                                                                                                            |
| B3 | Flyer approaches a floor or broken/orphan landing via ordinary open-air movement     | Planning and execution respect the live boundary, including when it changes after route calculation.                                                                  |
| U1 | Select upper deck, redraw ASCII and tile previews                                    | Cursor, selected part, part list, and repair/install candidates still agree.                                                                                          |
| U2 | Examine upstairs, complete/reopen work, use overview                                 | Full local deck identity persists through the real UI/activity path.                                                                                                  |
| U3 | Matching doors directly above one another                                            | Each deck's ordinary multipart door operates independently.                                                                                                           |
| U4 | Global and `DEFAULTMODE` stair rebinding; explicit editor override                   | Dispatch, control-strip hints, and help use the intended effective bindings; conflicts with description scrolling are visible and never trigger two actions.          |
| U5 | Change decks using keys and clickable controls, including empty extension decks      | Cursor XY, orientation, zoom, and selection refresh remain consistent; avatar/map z and move points do not change.                                                    |
| U6 | Empty active tile above occupied reference tile                                      | Shadow is visible but `cpart` remains absent; no lower part becomes a repair/remove target or selection highlight.                                                    |
| U7 | Active part overlaps shadow; lower deck empty; damaged/removed reference structure   | Active rendering wins; no nearest-deck substitution occurs; shadow membership follows the documented structural rules in both modes.                                  |
| U8 | Resizing/narrow layout, rebasing, installation, and removal during editor use        | Controls/clipping stay correct, local-deck reference alignment persists, and no stale shadow or redraw-induced selection change remains.                              |
| X1 | Existing vehicle/ramp/rail/rope/vision/physical-floor suites                         | Existing supported behavior is preserved.                                                                                                                             |
| X2 | Unsupported multi-deck fold/rack/automation request                                  | Explicit safe refusal rather than geometry loss or planar fallback.                                                                                                   |

Relevant existing test files include `vehicle_test.cpp`, `vehicle_part_test.cpp`, `vehicle_split_test.cpp`, `vehicle_ramp_test.cpp`, `vehicle_rails_test.cpp`, `vehicle_ladder_test.cpp`, `vehicle_interact_test.cpp`, `npc_test.cpp`, `monster_test.cpp`, and `vision_test.cpp` under `tests/`.

Run B1 together with W6: put adjacent actors at equal world z but on different local decks of the ramp-deformed vehicle. Assert that the solid boundary blocks sight, reach, and shots as well as walking; remove the relevant cover and verify that only physically open paths become available. Exercise the applicable CPU/GPU visibility paths without adding test-only opacity.

Add proposed `tests/vehicle_stairs_test.cpp` for connector and actor integration cases. Give new tests a common tag such as `[vehicle_multiz]`, with `[vehicle_stairs]` for the stair subset. Preserve existing test tags. Deterministic tests must clean up global actors, vehicles, route caches, and map state.

UI and cache tests must follow the production action/refresh ordering. Do not manually repair cursor state, mark extra levels dirty, or reset routing caches after the action under test unless production does the same.

## Verification and Completion

The documentation separation does not run formatting, builds, or tests. After code/test extraction, validate each branch independently and record its actual results. For implementation changes, format touched files first, then build both the game and tests. On `feat/vehicle-3d`, the canonical Linux validation is:

```sh
cmake --preset linux-full -DLANGUAGES=none
cmake --build --preset linux-full --target cataclysm-bn-tiles cata_test-tiles
./out/build/linux-full/tests/cata_test-tiles "[vehicle_multiz]" --rng-seed 1
./out/build/linux-full/tests/cata_test-tiles "[vehicle][ramp]" --rng-seed 1
./out/build/linux-full/tests/cata_test-tiles "[vehicle][save]" --rng-seed 1
./out/build/linux-full/tests/cata_test-tiles "verify_copy_from_gets_damage_reduction" --rng-seed 1
```

On the feature branch, `[vehicle_multiz]` selects the retained foundation tests listed above and future height-specific tests as they are added. The same filter on the stacked cleanup branch also selects historical cleanup tests and is not an equivalent feature-only check. `[vehicle][save]` includes the legacy pivot and invalid-part recovery baselines in `vehicle_test.cpp`. G7's focused P1 wheel case and future P2-P7 tests must be added and run before claiming their acceptance requirements.

Run other relevant existing suites as implementation reaches them, not an unfiltered test run in place of branch-specific selection. From the repository root, the Linux game binary for content validation is `./out/build/linux-full/src/cataclysm-bn-tiles`; use `--check-mods` and the repository JSON lint workflow after adding the part/item/recipe/prototype data. Windows developers can use the corresponding configured MSVC game and test targets; this does not replace platform CI coverage.

Manually exercise the bus-like fixture in both construction preview modes: switch decks using the stair keys and visible controls, align an upper frame using the lower-deck shadow, install the upper section, use the stairs, ask a follower to follow, drive onto/off a ramp, stop mid-ramp to test deck continuity, stop and use stairs again, collide with an upper-only obstruction, save/reload, and damage a connection. Check the old location for stale vehicle floors and visibility after moving away. Repeat editor navigation with customized stair bindings and a narrow/resized window.

The milestone is complete only when all required feature outcomes are demonstrated, the documented restrictions are enforced, and the implementation documentation distinguishes delivered behavior from deferred features. Generic fragment preservation and the other cleanup-branch fixes are not implicit completion gates. A forced JSON prototype that displays two floors, a passing coordinate unit test, or a path that cannot actually be executed is not completion.

For a later documentation check, use the file-scoped command below; it has not been run as part of this documentation-only separation:

```sh
deno fmt --check --config docs/deno.jsonc docs/en/dev/design/multi_z_vehicles.md
```

## Follow-Up Decisions

The following do not block the selected M1 behavior, but should remain visible for the next milestone:

- Whether vertical frame attachment should eventually require explicit uprights instead of the initial six-neighbor structural rule.
- Moving stair traversal, vehicle-relative goals, and whether richer route steps/runtime handles are then justified.
- Offset stairs, combined up/down intermediate landings, and independently closable hatches.
- True open-stairwell sight, sound, fluid, projectile, item, and falling propagation.
- General multi-deck boats/aircraft/rails, clearance-aware autodrive, racks, and folding.
- Physical center-of-mass height, rollover, and structural load limits.
- Final component balance, dedicated art, and normal-world double-decker spawning.
