# Benchmark Loop

This runbook defines a durable `curses-cli` benchmark loop that survives context compaction and keeps results comparable across sessions.

## Benchmark target

`town_fight_v1` success criteria:

1. Start as evacuee in reproducible profile.
2. Reach nearest town using map-assisted travel flow.
3. Engage at least one naturally encountered hostile in melee/ranged combat.
4. Finish with cast + compact artifacts; no debug spawning/actions.

## Required invariants

- Fixed benchmark profile (`scenario`, `profession`, seed policy, safe mode policy).
- Deterministic script path and timeout budget.
- No debug menu usage (`F12`, debug spawn, wish menu).
- Session-local roots under `/tmp/curses-cli/<run-id>/` (casts/artifacts/userdir stay together).

## Operator command skeleton

```bash
deno task pr:verify:curses-cli start --state-file /tmp/curses-bench.json --render-webp false
deno task pr:verify:curses-cli state-dump --state-file /tmp/curses-bench.json --max-chars 4200 --output-format ai
deno task pr:verify:curses-cli inputs-jsonl --state-file /tmp/curses-bench.json
# ... closed-loop control (observe -> single action -> verify) ...
deno task pr:verify:curses-cli capture --state-file /tmp/curses-bench.json --id bench-final --caption "Final benchmark state" --lines 120
deno task pr:verify:curses-cli stop --state-file /tmp/curses-bench.json --status passed --required-capture-ids-json '["bench-final"]'
# failure example:
# deno task pr:verify:curses-cli stop --state-file /tmp/curses-bench.json --status failed --stop-reason mode_trap --failure "targeting mode did not exit"
```

Prefer `state-dump.available_inputs` as first-choice actions before wider prompt scans.
`send-inputs` rejects unavailable prompt keys by default to reduce mode drift.
Avoid long blind `ids-json` blobs; use short intent-level actions and verify each transition.
AI output mode is default; use `--output-format json` only when full pretty JSON compatibility is required.

## Failure taxonomy

- `menu_drift`: expected gameplay mode but nested UI/menu mode detected.
- `mode_trap`: targeting/look/map/debug mode not exited within guard timeout.
- `safe_mode_interrupt`: hostile detected and run stalled due to safe mode.
- `repro_drift`: unexpected profile/trait/seed-dependent divergence.

## Compact state-dump (implemented)

`state-dump` emits:

- current ASCII pane excerpt
- prompt-derived available inputs
- engine action keys JSON snapshot
- macro JSON snapshot
- recent game logs / ai_state
- run metadata and stop reason class
- recommended prompt-safe input IDs

Use this as primary AI context; keep full captures only for divergence points.
