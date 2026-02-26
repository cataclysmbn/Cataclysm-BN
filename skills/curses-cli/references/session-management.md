# Session Management

Use a unique state file per run to avoid collisions.

```bash
deno task pr:verify:curses-cli start --state-file /tmp/curses-session.json --render-webp false
deno task pr:verify:curses-cli snapshot --state-file /tmp/curses-session.json --lines 120
deno task pr:verify:curses-cli capture --state-file /tmp/curses-session.json --id session-final --caption "Session final state" --lines 120
deno task pr:verify:curses-cli stop --state-file /tmp/curses-session.json --status passed --required-capture-ids-json '["session-final"]'
```

Notes:

- `start` writes metadata that all other commands use via `--state-file`.
- If a run fails, call `stop --status failed --failure "reason"` to preserve diagnostics.
- Passed status requires cast events plus capture evidence (`--required-capture-ids-json`).
- Clean up stale tmux sessions with `tmux ls` and `tmux kill-session -t <session>`.
