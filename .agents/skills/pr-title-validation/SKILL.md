---
name: pr-title-validation
description: Validate PR titles before creation or retitle.
compatibility: git, gh, conventional-prs
---

# PR Title Validation

Use before `gh pr create`, `gh pr create --web`, `gh pr create --fill`, or PR title edits.

## Sources

- `docs/en/contribute/changelog_guidelines.md`
- `.github/semantic.yml`
- `.github/pull_request_template.md`
- <https://github.com/scarf005/conventional-prs>

## Gate

- Title MUST pass `conventional-prs --input '<title>'`.
- Do not invent scopes.
- Required scopes: ports → `port`; Lua → `lua`; mods → `mods` or `mods/<MOD_ID>`; balance → `balance`; interface → `UI`.

## `feat`/`fix` subject gate

`feat` and `fix` subjects MUST say what matters to players.

- Describe visible gameplay/content/UI behavior, not code mechanics.
- Avoid files, classes, functions, JSON fields, caches, loaders, and refactors when a player-visible effect exists.
- Name the concrete item, monster, map, vehicle, menu, action, mod, platform, crash, or behavior.

Technical wording is fine for non-player-facing `refactor`, `ci`, `build`, `test`, `docs`, `style`, `chore`, and `perf` PRs.

## Workflow

1. Read the diff.
2. Pick type and smallest valid scopes.
3. For `feat`/`fix`, rewrite code wording into player effect.
4. Run `conventional-prs --input '<title>'`.
5. Use `gh pr create --title '<title>'`; do not trust unchecked `--fill`.

## Repairs

- `feat: rebalancing some rifles` → `feat(balance): nerf jam chance of m16 and m4`
- `fix: update vehicle cache invalidation` → `fix: update vehicle visibility after part changes`
- `fix: UI bug` → `fix(UI): prevent item repair menu from hiding refittable items`
- `fix: bundle macOS curses runtime libraries` → `build: bundle macOS curses runtime libraries` (#9288)
- `fix(mods): check default mods by default` → `test(mods): check default mods by default` (#9261)
