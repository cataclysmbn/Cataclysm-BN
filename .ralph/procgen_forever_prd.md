# PRD: Procedural Item Creation System - Production Readiness Push

## Context

This branch introduces procedural crafting for items such as sandwiches, stew, and swords. The current feature is still not production ready. The user wants an autonomous improvement loop that continues refining the system with atomic commits until told to stop.

## Product goal

Deliver a production-ready procedural crafting system for Cataclysm: Bright Nights that supports intuitive keyboard-first creation flows, reliable runtime behavior, sane candidate filtering, migration from legacy handcrafted variants where appropriate, and strong automated test coverage.

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

### D. Procgen recipes and migrations

- sandwich should support up to 3 breads
- stew should support tool-gated inputs like cutting and heat source concepts where appropriate
- add more procgen recipes where the design makes sense
- obsolete or migrate handcrafted variants that should now be procgen-backed, especially sandwiches
- dynamically construct names and descriptions where appropriate

### E. Testing and verification

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
- sandwich/stew/sword are reliable demonstrations of the system
- procgen candidate filtering behaves sensibly in both normal play and debug hammerspace
- tests meaningfully cover regressions and runtime bugs
- the branch accumulates a clean series of atomic commits that push the feature toward production readiness

## Explicit next priorities from user feedback

1. candidate list wraparound at top/bottom
2. working search
3. sane debug hammerspace candidate handling
4. prevent using things like `cheese sandwich` as cheese input
5. group duplicates in selected parts and slots with `xN`
6. sandwich max breads = 3
7. sane filtering in general
8. more tests
9. obsoletion and migration for procgen-able items such as sandwiches
10. tool-aware stew input handling
11. broader procgen recipe support where appropriate
12. keep improving autonomously until a stop signal is present
