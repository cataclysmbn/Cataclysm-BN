---
name: pr-title
description: Chooses and validates the best PR title type and optional scope using changed files and required context. Enforces docs-heavy PRs to use docs type, prevents perf/UI mislabeling, and refreshes semantic scopes when new mods are added.
---

# PR Title

Use this skill when drafting or reviewing pull request titles.

## Goal

- Choose an appropriate Conventional Commit type.
- Choose an optional scope only when it clearly improves precision.
- Prevent misleading types (for example `feat` for docs-only work).

## Rules

1. PR title format must be `<type>: <subject>` or `<type>(<scope>): <subject>`.
2. Type must follow project changelog categories in `docs/en/contribute/changelog_guidelines.md`.
3. If changes are mostly under `docs/` (>80% of changed files), type must be `docs`.
4. Scope is optional. Use it only when specific and accurate (`UI`, `i18n`, `balance`, `port`, `lua`, `mods`, `mods/<MOD_ID>`).
5. Do not use `feat` for documentation-only PRs.
6. Context is mandatory. Provide a short intent summary plus touched areas before final title validation.
7. If context indicates performance optimization, type must be `perf`.
8. If context indicates adding a user interface option, scope must include `UI`.

## Workflow

1. Gather changed files for the branch against `upstream/main`.
2. Write a short context line that includes:
   - user-facing intent (what is improved/fixed)
   - touched areas/files (what was modified)
3. Propose a PR title with best type and optional scope.
4. Validate it with:

```sh
deno run -R -N --allow-run=git,deno \
  .agents/skills/pr-title/scripts/pr_title.ts \
  --title "<type>(<scope>): <subject>" \
  --context "<intent and touched areas>"
```

4. If validation fails, adjust type/scope/context and rerun.

## Context Examples

- Performance-focused: `"improves cache behavior in map processing and reduces frame-time spikes in pathfinding"`
  - Expected type: `perf`
- New UI option: `"adds a new sidebar toggle option in the options UI and updates option rendering"`
  - Expected scope: include `UI`

## Semantic Scope Refresh

When new mods are added (`data/mods/<MOD_ID>/modinfo.json` as added files), this skill must ensure semantic scopes are refreshed.

- The validator handles this automatically by running `deno task semantic`.
- Commit any resulting `.github/semantic.yml` update together with the PR-title changes.
