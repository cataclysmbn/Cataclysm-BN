# Curses CLI Benchmark Protocol

This guide defines a repeatable benchmark loop for Cataclysm-BN `curses-cli` automation and documents known churn points plus mitigation.

## Goal

Run a deterministic scenario that validates real gameplay control quality:

1. start as evacuee,
2. collect baseline tools,
3. travel toward nearest town,
4. enter and sustain a real fight against naturally encountered enemies,
5. archive cast and compact artifacts.

The benchmark is invalid when debug spawning/wish/debug-menu actions are used.
The benchmark is also invalid when the first-day tool bootstrap is skipped.

## Mandatory scenario gates

Before claiming a benchmark pass, complete these gates in order:

1. `evacuee` start is confirmed in-game.
2. First-day tool bootstrap is completed (based on gameplay guides):
   - gather rocks outside,
   - smash locker + bench for base materials,
   - secure `pipe` and a `hammering >= 1` tool path,
   - craft makeshift crowbar.
3. Leave shelter and make hostile contact in natural exploration (no spawned enemies).
4. Confirm real combat engagement in the run (hostile contact + exchanged attacks).

If any gate is missing, stop with `--status failed --stop-reason repro_drift` and describe which gate failed.

## Standard run

```bash
deno task pr:verify:curses-cli start --state-file /tmp/curses-bench.json --render-webp false
deno task pr:verify:curses-cli state-dump --state-file /tmp/curses-bench.json --max-chars 4200 --output-format ai
deno task pr:verify:curses-cli inputs-jsonl --state-file /tmp/curses-bench.json
# execute scripted scenario steps with available_inputs first
# REQUIRED: follow first-day bootstrap before travel/combat
# - gather rocks
# - smash locker + bench
# - craft/carry makeshift crowbar
# REQUIRED: confirm hostile contact and exchanged attacks before marking pass
# if run fails, preserve taxonomy for triage:
# deno task pr:verify:curses-cli stop --state-file /tmp/curses-bench.json --status failed --stop-reason mode_trap --failure "targeting mode did not exit"
deno task pr:verify:curses-cli capture --state-file /tmp/curses-bench.json --id bench-final --caption "Benchmark final state" --lines 120
deno task pr:verify:curses-cli stop --state-file /tmp/curses-bench.json --status passed --required-capture-ids-json '["bench-final"]'
```

The `state-dump` payload now includes `available_inputs` and `stop_reason_candidate`.
Prefer those recommendations first, then fallback to full `inputs-jsonl` when required.
AI output mode is default; use `--output-format json` (or `CURSES_CLI_OUTPUT_FORMAT=json`) when legacy pretty JSON is required.

## Artifact contract

- Session-local root: `/tmp/curses-cli/<run-id>/`
- Cast: `/tmp/curses-cli/<run-id>/casts/*.cast`
- Session manifest and captures: `/tmp/curses-cli/<run-id>/artifacts/`
- Runtime exports:
  - `available_keys.json`
  - `available_macros.json`
  - `ai_state.json`

## Churn control checklist

- Validate current mode before each critical key sequence.
- Guard against nested UI states (`look`, map, debug, targeting, lua console).
- Keep safe-mode policy explicit for the profile.
- Use macro IDs (`macro:<id>`) for robust intent calls where possible.
- Persist stop reason category on failure (`menu_drift`, `mode_trap`, `safe_mode_interrupt`, `repro_drift`) with `stop --stop-reason <category>`.
- Keep `send-inputs` strict prompt validation enabled (default) so keys that are not
  currently offered by prompt context are rejected early.
- Treat missing first-day bootstrap (rock -> locker/bench smash -> crowbar craft) as a hard failure,
  not as optional setup.

## Compact dump (implemented)

Use `state-dump` as the primary context source instead of repeated raw snapshots. Payload fields:

- ASCII pane excerpt (trimmed)
- available inputs (prompt-derived)
- available action keys JSON snapshot
- available macros JSON snapshot
- recent logs and ai_state summary
- run metadata (turn, coords, mode, stop reason)
- `available_inputs` (prompt-safe, directly sendable available actions)

This reduces token load while preserving actionable context.

## Low-priority reproducibility improvements

- Add benchmark profile presets with fixed seed and explicit scenario/profession.
- Add a pre-generated benchmark world/save fixture to avoid early-world variance.
- Add deterministic character template to avoid random traits affecting control difficulty.
- Add fast-start option to skip non-critical intro screens.
