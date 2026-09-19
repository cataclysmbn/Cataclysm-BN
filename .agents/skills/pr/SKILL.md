---
name: pr
description: Create or update Cataclysm-BN PRs with validated titles, terse bodies, related links, and reviewer-useful evidence.
compatibility: git, gh, conventional-prs
---

# PR

Use before `gh pr create`, `gh pr edit`, PR body edits, or PR title edits.

## Sources

- `docs/en/contribute/changelog_guidelines.md`
- `.github/semantic.yml`
- `.github/pull_request_template.md`
- <https://github.com/scarf005/conventional-prs>

## Title gate

- Must pass `conventional-prs --input '<title>'`.
- Use the smallest valid scope; do not invent scopes.
- Required scopes: ports → `port`; Lua → `lua`; mods → `mods` or `mods/<MOD_ID>`; balance → `balance`; interface → `UI`.
- For `feat`/`fix`, describe the visible player/reviewer impact, not implementation details.

## Body gate

- Read the diff, PR template, and existing PR body before writing or editing.
- Search related issues/PRs. Also link known source PRs, commits, reports, or regressions even if search misses them.
- Keep the body terse. Include only reviewer-useful context.
- Purpose: state the bug/need and the relevant evidence. Paste the key log/screenshot/result snippet when it proves the issue.
- Solution: concise bullets for what changed.
- Testing: do not dump CI-equivalent command lists. State target/manual validations and results; include commands only when local-only, non-obvious, or useful for reproduction.
- Preserve human content, artifacts, screenshots, checklist state, and generated artifact blocks when editing.
- Remove irrelevant optional checklist items; keep applicable required/template items concrete.

## Workflow

1. Inspect the diff and current branch status.
2. Read the PR template and current PR body if editing.
3. Run targeted `gh search issues` / `gh search prs`; inspect known source PRs from context or `git log`.
4. Draft the title and run `conventional-prs --input '<title>'`.
5. Draft or patch the body using the body gate.
6. Use explicit title/body arguments or `--body-file`; do not rely on unchecked `--fill`.

## Repairs

- `feat: rebalancing some rifles` → `feat(balance): nerf jam chance of m16 and m4`
- `fix: update vehicle cache invalidation` → `fix: update vehicle visibility after part changes`
- `fix: UI bug` → `fix(UI): prevent item repair menu from hiding refittable items`
- Testing command dump → `Added regression coverage for X; reproduced Y before the fix; confirmed Z after the fix.`
- Missing proof → include the one log line, screenshot, or before/after table that explains why the change exists.
