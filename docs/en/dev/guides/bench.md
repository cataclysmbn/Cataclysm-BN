# Curses CLI Benchmark Protocol

This guide defines a repeatable benchmark loop for Cataclysm-BN `curses-cli` automation and documents known churn points plus mitigation.

## Goal

Run a deterministic scenario that validates real gameplay control quality:

1. start as evacuee,
2. collect baseline tools,
3. travel toward nearest town,
4. fight naturally encountered enemies,
5. archive cast and compact artifacts.

The benchmark is invalid when debug spawning/wish/debug-menu actions are used.

## Standard run

```bash
deno task pr:verify:curses-cli start --state-file /tmp/curses-bench.json --render-webp false
deno task pr:verify:curses-cli inputs-jsonl --state-file /tmp/curses-bench.json
# execute scripted scenario steps
deno task pr:verify:curses-cli capture --state-file /tmp/curses-bench.json --id bench-final --caption "Benchmark final state" --lines 120
deno task pr:verify:curses-cli stop --state-file /tmp/curses-bench.json --status passed
```

## Artifact contract

- Cast: `/tmp/curses-cli/casts/*.cast`
- Session manifest and captures: `/tmp/curses-cli/runs/live-*/`
- Runtime exports:
  - `available_keys.json`
  - `available_macros.json`
  - `ai_state.json`

## Churn control checklist

- Validate current mode before each critical key sequence.
- Guard against nested UI states (`look`, map, debug, targeting, lua console).
- Keep safe-mode policy explicit for the profile.
- Use macro IDs (`macro:<id>`) for robust intent calls where possible.
- Persist stop reason category on failure (`menu_drift`, `mode_trap`, `safe_mode_interrupt`, `repro_drift`).

## Planned compact dump

A compact dump should be preferred over raw repeated full snapshots for AI loops. Target payload:

- ASCII pane excerpt (trimmed)
- available inputs (prompt-derived)
- available action keys JSON snapshot
- available macros JSON snapshot
- recent logs and ai_state summary
- run metadata (turn, coords, mode, stop reason)

This reduces token load while preserving actionable context.

## Low-priority reproducibility improvements

- Add benchmark profile presets with fixed seed and explicit scenario/profession.
- Add a pre-generated benchmark world/save fixture to avoid early-world variance.
- Add deterministic character template to avoid random traits affecting control difficulty.
- Add fast-start option to skip non-critical intro screens.
