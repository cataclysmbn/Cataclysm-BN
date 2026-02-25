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
- Standard artifact roots under `/tmp/curses-cli`.

## Operator command skeleton

```bash
deno task pr:verify:curses-cli start --state-file /tmp/curses-bench.json --render-webp false
deno task pr:verify:curses-cli inputs-jsonl --state-file /tmp/curses-bench.json
# ... scripted control loop ...
deno task pr:verify:curses-cli capture --state-file /tmp/curses-bench.json --id bench-final --caption "Final benchmark state" --lines 120
deno task pr:verify:curses-cli stop --state-file /tmp/curses-bench.json --status passed
```

## Failure taxonomy

- `menu_drift`: expected gameplay mode but nested UI/menu mode detected.
- `mode_trap`: targeting/look/map/debug mode not exited within guard timeout.
- `safe_mode_interrupt`: hostile detected and run stalled due to safe mode.
- `repro_drift`: unexpected profile/trait/seed-dependent divergence.

## Compact state-dump direction (planned)

Add a single compact dump command that emits:

- current ASCII pane excerpt
- prompt-derived available inputs
- engine action keys JSON snapshot
- macro JSON snapshot
- recent game logs / ai_state
- run metadata and stop reason class

Use this as primary AI context; keep full captures only for divergence points.
