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
deno task pr:verify:curses-cli send-input --state-file /tmp/curses-demo.json --id action:confirm
deno task pr:verify:curses-cli snapshot --state-file /tmp/curses-demo.json --lines 120
deno task pr:verify:curses-cli stop --state-file /tmp/curses-demo.json
```

## Core commands

- `start` starts a tmux session and launches the game binary.
- `inputs-jsonl` lists prompt-aware inputs plus action catalog aliases.
- `available-keys-json` reads game-native keybinding context.
- `available-macros-json` reads game-native Lua action-menu macro context.
- `send-input` and `send-inputs` send one or many actions/keys.
- `snapshot` and `capture` collect text/screenshot artifacts.
- `wait-text` and `wait-text-gone` synchronize flows on UI text.
- `get-game-state` reads structured state JSON from runtime userdir.
- `state-dump` (planned) should emit one compact AI-friendly JSON payload.
- `stop` finalizes artifacts and stops the tmux session.

## References

- `references/session-management.md`
- `references/macros.md`
- `references/benchmark-loop.md`
