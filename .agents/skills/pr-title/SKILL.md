---
name: pr-title
description: Chooses and validates the best PR title type and optional scope using changed files. Enforces docs-heavy PRs to use docs type and refreshes semantic scopes when new mods are added.
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

## Workflow

1. Gather changed files for the branch against `upstream/main`.
2. Propose a PR title with best type and optional scope.
3. Validate it with:

```sh
deno run -R -N --allow-run=git,deno \
  .agents/skills/pr-title/scripts/pr_title.ts \
  --title "<type>(<scope>): <subject>"
```

4. If validation fails, adjust type/scope and rerun.

## Semantic Scope Refresh

When new mods are added (`data/mods/<MOD_ID>/modinfo.json` as added files), this skill must ensure semantic scopes are refreshed.

- The validator handles this automatically by running `deno task semantic`.
- Commit any resulting `.github/semantic.yml` update together with the PR-title changes.
