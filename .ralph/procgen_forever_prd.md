# PRD: Procedural Item Creation System - Procedural Melee Weapons Focus

## Context

This branch introduces procedural crafting for items such as sandwiches, stew, and swords. The current feature is still not production ready. The user has now shifted the autonomous loop's main focus toward procedural melee weapons while keeping existing food procgen stable.

## Product goal

Deliver a production-ready procedural crafting system for Cataclysm: Bright Nights, with primary emphasis on procedural melee weapons. The system should support intuitive keyboard-first creation flows, reliable runtime behavior, sane candidate filtering, migration from legacy handcrafted variants where appropriate, and strong automated test coverage.

## Current strategic focus

Procedural melee weapons are now the primary focus.

- Food procgen should only receive fixes needed for correctness, migration, or shared-system stability.
- New design and implementation effort should prioritize melee weapon schemas, UI fit, naming, descriptions, stat composition, migrations, and tests.

## Primary user-facing goals

1. Procedural crafting must feel natural and fast from the normal crafting UI.
2. The builder UI must be clear in pure ASCII and remain usable without relying on color.
3. Candidate filtering must be sane and domain-correct.
4. Crafting must never freeze or crash.
5. The system must support real procedural behavior beyond toy examples.

## Scope to improve continuously

### A. Builder UI and interaction

- three-column ASCII layout with strong hierarchy
- shape-based slot indicators using filled, required-empty, and optional-empty cells
- search that actually filters candidates
- navigation that wraps at top and bottom where appropriate
- grouped duplicate display using `xN`
- diff preview for highlighted candidates
- larger, readable modal layout
- clear craft readiness status and hotkey legend

### B. Filtering and semantics

- stop nonsensical matches such as non-food items in sandwich slots
- reject derived finished dishes as raw ingredients where inappropriate
- make sandwich bread semantics broad but sane
- support tag and material filters correctly
- make debug hammerspace candidate generation sane and representative

### C. Crafting correctness

- no segfaults or freezes during selection, craft start, craft completion, save/load, or uncraft
- debug and non-debug paths should share the same core craft completion flow
- highlighted preview and final result should stay consistent

### D. Procgen melee weapons and migrations

- expand procedural melee weapon support beyond a single sword example
- support sensible weapon roles such as blade, head, shaft, guard, grip, pommel, binding, and reinforcement where applicable
- compute bash, cut, stab, to-hit, speed, durability, and weight/volume from material values and part traits where sensible
- support procedural naming and descriptions for melee weapons
- support migration/obsoletion of handcrafted melee items that should become procgen-backed
- add more procgen melee recipes where the design clearly fits
- ensure tool-aware weapon crafting where appropriate

### E. Food and shared recipe work

- sandwich should support up to 3 breads
- stew should support tool-gated inputs like cutting and heat source concepts where appropriate
- obsolete or migrate handcrafted variants that should now be procgen-backed, especially sandwiches
- dynamically construct names and descriptions where appropriate

### F. Testing and verification

- expand unit/regression coverage substantially
- add tests for filtering behavior, search behavior, readiness logic, Lua/runtime fallback, craft completion, and migration behavior
- keep JSON snapshot coverage current where useful

## Non-goals

- do not stop for user confirmation unless absolutely blocked
- do not run `--check-mods`
- do not abandon the branch or PR

## Constraints

- follow repo `AGENTS.md`
- prefer TDD and atomic commits
- use conventional commits
- preserve user worktree state
- keep responses and commits focused

## Acceptance direction

The work is considered moving in the right direction when:

- the builder is stable and intuitive in normal play
- procedural melee weapons are the strongest and most polished demonstrations of the system
- sandwich/stew/sword remain stable demonstrations of shared procgen infrastructure
- procgen candidate filtering behaves sensibly in both normal play and debug hammerspace
- tests meaningfully cover regressions and runtime bugs
- the branch accumulates a clean series of atomic commits that push the feature toward production readiness

## Explicit next priorities from user feedback and focus shift

1. prioritize procedural melee weapons over new food work
2. make melee weapon candidate filtering sane and domain-correct
3. improve melee weapon part schemas, stat composition, naming, and descriptions
4. broaden procgen melee recipes where justified
5. add migrations/obsoletions for procgen-able melee weapons and legacy sandwich items
6. finish builder interaction issues such as wraparound, search, and duplicate grouping
7. keep debug hammerspace behavior sane but secondary to real gameplay behavior
8. support tool-aware inputs where it makes sense, especially stew and weapon crafting
9. write substantially more tests and regression coverage
10. keep improving autonomously until a stop signal is present
