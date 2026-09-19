---
name: test-quality
description: Write or review Cataclysm-BN tests that avoid flakes, use deterministic fixtures, and clean up global state.
---

# Test Quality

Use when adding or changing automated tests.

## Gates

- Test one behavior contract, not incidental implementation details.
- Use deterministic fixtures from `data/mods/TEST_DATA` for data with random, balance, or evolving behavior.
- Avoid assertions that rely on probability, wall-clock time, locale, file order, or current global options unless the test pins them.
- Reproduce reported flakes with the failing `--rng-seed` before accepting the fix.
- When CI test jobs fail, inspect the job log for the exact test command, filters, shard split, user-dir, order-sensitive included tests, `--rng-seed`, and backend flags. Run that same shard command on the final rebased HEAD before declaring the fix validated; do not substitute a narrower tag filter or a different full-test command.
- Before pushing test fixes that touch global state, NPCs, maps, overmaps, player position, RNG, options, or fixtures, run both the relevant focused test and the CI-equivalent shard/full command that can expose state bleed from earlier tests.
- Keep Lua/C++ integration tests on real bound objects when the binding behavior is under test.

## Cleanup

- Restore global state with `restore_on_out_of_scope` for calendar, options, globals, and singleton state.
- Use `clear_map()` and explicit player placement when map contents matter.
- Do not leave creatures, items, hooks, worlds, temp files, or option changes that can affect later tests.
- Prefer new `TEST_DATA` fixtures over mutating base-game data in the test body.
