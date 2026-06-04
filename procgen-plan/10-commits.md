# Commit Policy

## Status

Do not start implementation until this commit plan is accepted.

This folder is the plan for how implementation commits should be split.

The current docs commit should stay docs-only.

## Rules

- many small commits
- one concern per commit
- tests in the same commit when practical
- no mixed refactor + feature + data + fixes in one commit
- no giant "procgen everything" commit
- docs-only reshaping stays separate from code

## Size rule

Prefer a commit that changes one narrow seam over one that completes a whole phase.

Good commit scope:

- add proc recipe fields
- add schema loader
- add item proc payload save/load
- add builder stub routing

Bad commit scope:

- add entire procgen framework, sandwich builder, spear, tests, and Lua bridge at once

## Planned implementation commit train

The default train should be even smaller than the old phase plan.

1. `feat: add proc ids and core types`
2. `test: cover proc core ids and enums`
3. `feat: add proc recipe fields`
4. `test: cover proc recipe parse`
5. `feat: load proc schemas`
6. `test: cover proc schema parse and terse keys`
7. `feat: add part fact normalization`
8. `test: cover part fact normalization`
9. `feat: route proc recipes to builder stub`
10. `test: cover proc recipe search and routing`
11. `feat: add proc item payload`
12. `test: cover proc payload save and load`
13. `feat: add proc stack key seam`
14. `test: cover proc stack keys`
15. `feat: add builder candidate index`
16. `test: cover builder candidate matching`
17. `feat: add fast preview blob`
18. `test: cover fast preview math`
19. `feat: add proc lua hook bridge`
20. `test: cover lua full-fold bridge`
21. `feat: add proc make bridge`
22. `test: cover lua make normalization`
23. `feat: add sandwich schema and archetype`
24. `test: cover sandwich finalize and inspect`
25. `feat: add proc food consume path`
26. `test: cover proc food nutrition and stacking`
27. `feat: add compact part summaries`
28. `test: cover compact nested save and load`
29. `feat: add compact uncraft for recoverable items`
30. `test: cover spear durability restore`

If a commit becomes wide, split it again.

## Grouping rule

If future work expands into guns/tools/furniture/vparts, create a new commit train for each family instead of extending the food train into one giant branch.

## Commit message rule

Use conventional commits.

Suggested types:

- `feat`
- `fix`
- `refactor`
- `test`
- `docs`

Keep subject lines short and narrow.

## Review rule

Each commit should be reviewable alone.

That means:

- obvious purpose
- passing tests for that slice
- no hidden dependency on a later commit for basic correctness
