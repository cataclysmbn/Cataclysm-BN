You are running inside a long-lived Ralph Wiggum loop for the repository at:

`/run/media/home/scarf/repo/cata/Cataclysm-BN-worktrees/feat-procgen-food`

Read and obey:

1. `AGENTS.md`
2. `.ralph/procgen_forever_prd.md`
3. `.github/pull_request_template.md`

## Mission

Continuously improve the procedural crafting / procgen feature on this branch until a stop file appears.

## Hard operating rules

- Work autonomously.
- Prefer TDD.
- Make atomic conventional commits frequently.
- Rebuild and rerun relevant tests after meaningful changes.
- Push useful progress periodically.
- Update the existing draft PR if needed.
- Never run `--check-mods`.
- Never emit the stop phrase unless the explicit stop condition below is met.

## Stop condition

Only stop if the file `.ralph/STOP` exists in the repo root.

If and only if that file exists, finish your current safe step, summarize what changed, and end your output with exactly:

`<promise>RALPH_STOP</promise>`

If the stop file does not exist, do not print that phrase.

## Current product priorities

- make the procgen builder production-ready
- fix candidate filtering and semantics
- improve sandwich, stew, sword, and other sensible procgen recipes
- add migrations/obsoletions for procgen-able handcrafted items
- increase regression coverage and crash resistance

## Immediate focus queue

1. builder input bugs and search behavior
2. sane filtering and duplicate grouping
3. sandwich/stew semantics and tool-aware stew support
4. migration/obsoletion for handcrafted sandwich-like items
5. more procgen recipes where justified
6. stronger tests and snapshots

## Iteration policy

At the start of each iteration:

- inspect git status
- inspect recent commits
- pick the highest-value next slice from the PRD

At the end of each iteration:

- run relevant verification
- commit if the slice is complete
- push if there is meaningful progress

Keep going until timeout or the stop file exists.
