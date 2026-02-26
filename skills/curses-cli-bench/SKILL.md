---
name: curses-cli-bench
description: Runs Cataclysm-BN curses benchmark with strict evidence gates and zero-tolerance reporting.
allowed-tools: Bash(deno task pr:verify:curses-cli:*), Read(/tmp/curses-cli/**), Grep(/tmp/curses-cli/**)
---

# Curses CLI Benchmark (Zero Tolerance)

Use this skill when asked to benchmark gameplay loops. This skill forbids false "pass" claims.

## Non-negotiable rules

- Never mark benchmark `passed` unless every required gate has direct artifact proof.
- Always include the asciinema cast path in results.
- If a gate is missing proof, mark `failed` with `--stop-reason repro_drift`.
- Never infer gameplay progress from assumptions; only trust cast/capture/state evidence.
- Never use blind precomputed input bursts as the primary control strategy.
- Play in observe-act loops: inspect state, send one intent-level action, verify outcome, then continue.

## Required gates

For the evacuee first-day benchmark, all gates must be proven from artifacts:

1. Evacuee start loaded in gameplay.
2. First-day bootstrap executed:
   - gathered rocks,
   - smashed locker and bench,
   - obtained pipe + hammering path,
   - crafted makeshift crowbar.
3. Natural hostile contact reached.
4. Real fight engagement observed (exchanged attacks).

If any gate fails, stop with:

```bash
deno task pr:verify:curses-cli stop \
  --state-file <state-file> \
  --status failed \
  --stop-reason repro_drift \
  --failure "<missing gate + evidence summary>"
```

## Execution protocol

```bash
deno task pr:verify:curses-cli start --state-file /tmp/curses-bench.json --render-webp false
deno task pr:verify:curses-cli send-inputs --state-file /tmp/curses-bench.json --strict-prompt-inputs false --expect-mode any --ids-json '["Down","Down","Enter"]'
deno task pr:verify:curses-cli recover-ui --state-file /tmp/curses-bench.json --expect-mode gameplay --max-steps 20
```

Then run scenario actions and capture evidence after each gate:

- Human-like loop requirements:
  1. `state-dump` before action selection.
  2. send a single intent-level action (or a minimal pair like `open + direction`).
  3. verify via `state-dump`/`recover-ui`/`wait-text`.
  4. only then proceed.

Forbidden pattern (causes drift):

```bash
# BAD: blind replay blob
send-inputs --ids-json '["l","l","l","l","l","j","j","j","s","l","g",","]'
```

Required pattern:

```bash
# GOOD: closed-loop control
deno task pr:verify:curses-cli state-dump --state-file /tmp/curses-bench.json --max-chars 4200
deno task pr:verify:curses-cli send-inputs --state-file /tmp/curses-bench.json --expect-mode gameplay --ids-json '["o","l"]'
deno task pr:verify:curses-cli recover-ui --state-file /tmp/curses-bench.json --expect-mode gameplay
```

```bash
deno task pr:verify:curses-cli capture --state-file /tmp/curses-bench.json --id gate-1 --caption "evacuee loaded" --lines 160
deno task pr:verify:curses-cli capture --state-file /tmp/curses-bench.json --id gate-2 --caption "first-day bootstrap complete" --lines 200
deno task pr:verify:curses-cli capture --state-file /tmp/curses-bench.json --id gate-3 --caption "hostile contact" --lines 200
deno task pr:verify:curses-cli capture --state-file /tmp/curses-bench.json --id gate-4 --caption "fight engagement" --lines 200
```

Only after proving all gates:

```bash
deno task pr:verify:curses-cli stop --state-file /tmp/curses-bench.json --status passed --required-capture-ids-json '["gate-1","gate-2","gate-3","gate-4"]'
```

## Mandatory report format

Every benchmark report must include:

- `status`: passed/failed
- `manifest`: full path
- `cast`: full path (required)
- `cast_log`: full path
- `captures`: full paths
- `gate_evidence`: 1 line per gate with artifact reference
- `postmortem`: blunders + runtime drivers

If `status=passed` but any gate_evidence line is missing, the report is invalid and must be corrected to `failed`.
