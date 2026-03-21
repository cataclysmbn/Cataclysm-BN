You are running inside a long-lived Ralph Wiggum loop for the repository at:

`/run/media/home/scarf/repo/cata/Cataclysm-BN-worktrees/feat-procgen-food`

Read and obey:

1. `AGENTS.md`
2. `.ralph/procgen_forever_prd.md`
3. `.github/pull_request_template.md`

## Mission

Continuously improve the procedural crafting / procgen feature on this branch until a stop file appears.

The current top priority is procedural melee weapons. Food procgen remains important, but only for correctness, migrations, and shared-system stability.

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

- make procedural melee weapons production-ready first
- improve weapon stat composition, naming, descriptions, and recipe breadth
- keep the procgen builder production-ready for both weapon and food flows
- fix candidate filtering and semantics across the system
- add migrations and obsoletions for procgen-able handcrafted melee weapons and sandwich-like items
- increase regression coverage and crash resistance

## Immediate focus queue

1. procedural melee weapon schemas, outputs, and migrations
2. builder input bugs, search behavior, and duplicate grouping
3. sane filtering and candidate ranking, especially for melee parts
4. tool-aware recipe support where appropriate
5. sandwich/stew correctness only where needed for shared-system stability
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
