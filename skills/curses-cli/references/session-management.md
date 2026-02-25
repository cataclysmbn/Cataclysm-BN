# Session Management

Use a unique state file per run to avoid collisions.

```bash
deno task pr:verify:curses-cli start --state-file /tmp/curses-session.json --render-webp false
deno task pr:verify:curses-cli snapshot --state-file /tmp/curses-session.json --lines 120
deno task pr:verify:curses-cli stop --state-file /tmp/curses-session.json --status passed
```

Notes:

- `start` writes metadata that all other commands use via `--state-file`.
- If a run fails, call `stop --status failed --failure "reason"` to preserve diagnostics.
- Clean up stale tmux sessions with `tmux ls` and `tmux kill-session -t <session>`.
