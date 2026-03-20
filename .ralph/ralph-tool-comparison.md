# Ralph Tool Comparison

## Candidates reviewed

### 1. `eduardolat/clancy`

- Type: standalone loop orchestrator in Go
- Strengths:
  - works with arbitrary agent CLIs, including `opencode`
  - simple YAML config
  - explicit loop controls: timeout, max steps, stop phrase
  - easiest to run unattended in the background
  - most mature candidate reviewed
- Weaknesses:
  - no built-in task model; prompt must drive the work

### 2. `boxheed/open-ralph`

- Type: Node task runner with provider plugins
- Strengths:
  - propose/execute/verify loop model
  - task-file based workflow
- Weaknesses:
  - provider/task system is heavier than needed here
  - less direct fit for `opencode gpt-5.4 high`
  - wants its own task repository layout

### 3. `Mawla/ralph-wiggum-orchestration`

- Type: orchestration pattern and shell scripts
- Strengths:
  - good docs for multi-agent migration waves
- Weaknesses:
  - not a ready-to-install orchestrator
  - optimized for tmux/worktree migrations, not a direct unattended loop here

### 4. `vava-nessa/ralph-loop-autoswitch`

- Type: multi-CLI failover shell script
- Strengths:
  - quota failover concept
- Weaknesses:
  - less targeted to `opencode`
  - lower maturity and less predictable unattended setup

## Selected tool

`clancy` is the best fit for this repo and request.

## Why this choice

- it directly supports running `opencode` as the worker command
- it is lightweight enough to install and run immediately
- it supports a long-running autonomous loop with guardrails
- it does not force a custom task directory model on this repository
