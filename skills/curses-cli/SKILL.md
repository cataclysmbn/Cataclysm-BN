---
name: curses-cli
description: Automates Cataclysm-BN curses gameplay via CLI for regression checks and reproducible interaction scripts.
allowed-tools: Bash(deno task pr:verify:curses-cli:*)
---

# Curses CLI

Use the CLI wrapper to drive game sessions directly without MCP tool orchestration.

## Quick start

```bash
deno task pr:verify:curses-cli start --state-file /tmp/curses-demo.json --render-webp false
deno task pr:verify:curses-cli inputs-jsonl --state-file /tmp/curses-demo.json
deno task pr:verify:curses-cli send-inputs --state-file /tmp/curses-demo.json --ids-json '["Enter"]'
deno task pr:verify:curses-cli state-dump --state-file /tmp/curses-demo.json --output-format ai
deno task pr:verify:curses-cli snapshot --state-file /tmp/curses-demo.json --lines 120
deno task pr:verify:curses-cli stop --state-file /tmp/curses-demo.json
```

## Core commands

- `start` starts a tmux session and launches the game binary.
- `inputs-jsonl` lists prompt-aware inputs plus action catalog aliases.
- `available-keys-json` reads game-native keybinding context.
- `available-macros-json` reads game-native Lua action-menu macro context.
- `send-inputs` sends one or many actions/keys (use a one-item array for single input).
- `snapshot` and `capture` collect text/screenshot artifacts.
- `wait-text` and `wait-text-gone` synchronize flows on UI text.
- `get-game-state` reads structured state JSON from runtime userdir.
- `state-dump` emits a compact AI-friendly payload (including `available_inputs`).
- `stop` finalizes artifacts and stops the tmux session.

## AI output mode

- AI mode is now the default for supported commands (native YAML with `summary` and `payload` sections).
- In-session interaction commands (`send-inputs`, `send-mouse`, `send-waypoint`, `wait-text`, `wait-text-gone`, `stop`) also include a `screen` block rendered from current pane text.
- Nested structures at depth >= 3 are compacted into flow YAML (example: `prompt_inputs: [{id:'key:!',key:'!',label:'disable_safe_mode'}]`).
- Use `--output-format json` (or `CURSES_CLI_OUTPUT_FORMAT=json`) when raw pretty JSON is required.

## References

- `references/session-management.md`
- `references/macros.md`
- `references/benchmark-loop.md`
